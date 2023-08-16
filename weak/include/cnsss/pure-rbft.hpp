/**
 * @file pure-rbft.hpp
 *
 * @brief The (probably) 'robust' version of PBFT.
 *
 * 🐢 : RBFT is essentialy PBFT but with some additional features. The important ones
 * are:
 *
 *    1. The primary will filter out invalid Txs and prevent them from being
 *    boardcast to the cluster.
 *
 *    2. Nodes can be added dynamically to the cluster, which means that
 *    there're mechanisms that help new node to get synced.
 *
 */
#pragma once

#include <unordered_map>

#include <boost/algorithm/string.hpp> // for join()
#include <mutex>
#include <memory>

#include <boost/json.hpp>
#include <vector>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include "pure-forCnsss.hpp"
#include <set>


#include <mutex>
namespace pure{
  using namespace std::placeholders;  // for _1, _2, _3...
  using boost::format;
  using std::move;
  using std::vector;


  using std::unique_ptr;

  using std::unordered_map;
  namespace json = boost::json;
  using namespace std::placeholders;  // for _1, _2, _3...
  using std::bind;
  using std::shared_ptr;

  namespace json = boost::json;
  using json::value_to;

  /*
    🐢 :pbft needs something more than a normal static consensus in that, it
    needs digital signature. In particular, sometimes, it needs to store a
    particular msg sent by other nodes. For example, N1 needs to show N2 that
    'Look, N3 said <...>'. In this case, N1 needs to store the msg containing:

    <pk of N3><signature of N3 for `data`><`data`>

    In particular, this is needed during view-change, when nodes are asking each
    other `hey, what's your state.`

    🦜 : Ok, so what do we need ?

    🐢 : A signer.
  */

  class ISignable{
  public:
    virtual string sign(string_view s) noexcept=0;

    virtual bool verify(string_view msg) noexcept=0;
    virtual optional<string_view> get_data(string_view msg) noexcept=0;
    virtual optional<string_view> get_from(string_view msg) noexcept=0;
  };

  class IAsyncEndpointBasedNetworkable: public virtual IForCnsssNetworkable{
  public:
    virtual void listen(string target,function<void(string,string)> f) noexcept=0;
    virtual void send(string endpoint, string target,string data) noexcept=0;
  };

  template<typename T>
  // 🦜 : Bind the object with a lock.
  struct LockedObject{
    T o;
    mutable std::mutex lock;
  };


  class NewViewCertificate:
    virtual public IJsonizable,
    virtual public ISerializable
  {
  public:
    string msg;
    int epoch;
    vector<string> new_view_certificate;
    vector<string> sig_of_nodes_to_be_added;

    NewViewCertificate(string mmsg,int eepoch,vector<string> nnew_view_certificate,
                       vector<string> ssig_of_nodes_to_be_added):
      msg(mmsg),
      epoch(eepoch),
      new_view_certificate(nnew_view_certificate),
      sig_of_nodes_to_be_added(ssig_of_nodes_to_be_added){}

    string toString() const noexcept override {
      return IJsonizable::toJsonString();
    };

    bool fromString(string_view s) noexcept override{
      BOOST_LOG_TRIVIAL(debug) <<  "Forming NewViewCertificate from string.";
      return IJsonizable::fromJsonString(s);
    };

    bool fromJson(const json::value &v) noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("Forming NewViewCertificate from Json");

      try {
        this->msg = value_to<string>(v.at("msg"));
        this->epoch = value_to<int>(v.at("epoch"));
        this->new_view_certificate = value_to<vector<string>>(v.at("new_view_certificate"));
        this->sig_of_nodes_to_be_added = value_to<vector<string>>(v.at("sig_of_nodes_to_be_added"));

      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }
      return true;
    }
  };                          // class New_view_certificate

  class RbftConsensus:
    public virtual ICnsssPrimaryBased,
    public std::enable_shared_from_this<RbftConsensus>
    // define shared_from_this()
  {
  private:
    std::atomic_flag closed = ATOMIC_FLAG_INIT;
    std::atomic_flag view_change_state = ATOMIC_FLAG_INIT;
    std::atomic_int epoch = 0;

    LockedObject<
      unordered_map<int,unordered_map<string,vector<string>>>
      > laid_down_history;

    LockedObject<std::set<string>> sig_of_nodes_to_be_added;

    LockedObject<unordered_map<string,std::set<string>>> to_be_confirmed_commands;
    LockedObject<std::set<string>> recieved_commands;
    LockedObject<vector<string>> command_history;
    /*
     * 🐢 : Make the ctor private, see pure-ListenToOneConsensus.hpp for reason.
     */


    IAsyncEndpointBasedNetworkable * const net;
    IForConsensusExecutable * const exe;
    ISignable * const sig;

    LockedObject<vector<string>> all_endpoints;

    RbftConsensus(IAsyncEndpointBasedNetworkable * const n,
                  IForConsensusExecutable * const e,
                  ISignable * const s,
                  std::set<string> all_endpoints):
      exe(e), sig(s), net(n){

      // no need to lock here.
      this->all_endpoints.o = vector<string>(all_endpoints);

      if (not all_endpoints.contains(this->net->listened_endpoint()))
        this->start_listening_as_newcomer();
      else{
        if (this->primary() == this->net->listened_endpoint())
          this->start_listening_as_primary();
        else
          this->start_listening_as_sub();

        // start the timer
        std::thread{std::bind(&RbftConsensus::start_faulty_timer, this)}.detach();
        // here the ctor ends
      }
    }

    void clear_and_listen_common_things(){
      this->view_change_state.clear(); // set to false
      this->net->clear();
      this->net->listen("/ILaidDown", bind(&RbftConsensus::handle_laid_down,this,_1,_2));
      this->net->listen("/IamThePrimary", bind(&RbftConsensus::handle_new_primary,this,_1,_2));
      this->net->listen("/pleaseAddMeNoBoardcast",
                        bind(&RbftConsensus::handle_add_new_node_no_boardcast,this,_1,_2));
    }

    void  handle_new_primary(string endpoint, string data){

    }

  };

} // namespace pure