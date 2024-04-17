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
#include "../net/pure-netAsstn.hpp"
/*
  🦜 : Because *bft relies on digital signature, so we need to import
  IMsgManageable
 */

#include <set>
#include <thread>

#include "./.generated_pb/pure-rbft.pb.h"

#include <mutex>
namespace pure{
  using boost::format;
  using std::move;
  using std::vector;

  using std::make_shared;
  using std::shared_ptr;

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


  template<typename T>
  // 🦜 : Bind the object with a lock.
  struct LockedObject{
    T o;
    mutable std::mutex lock;
  };

  /**
   * @brief Msg sent to the next primary when a node is ready to do view-change.
   *
   * This one is supposed to be signed before being sent. So it doesn't need to
   * contain the `from` field.
   */
  class LaidDownMsg:
    virtual public IJsonizable,
    virtual public ISerializableInPb<hiPb::LaidDownMsg>,
    virtual public ISerializable
  {
  public:
    LaidDownMsg() = default;
    LaidDownMsg(string mmsg,int eepoch,string sstate):
      msg(mmsg), epoch(eepoch), state(sstate) {};
    string msg;
    int epoch;
    string state;

    json::value toJson() const noexcept override {
      return json::value_from(*this);
    }

    bool fromJson(const json::value &v) noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("Forming LaidDownMsg from Json");
      try {
        this->msg = value_to<string>(v.at("msg"));
        this->epoch = value_to<int>(v.at("epoch"));
        this->state = value_to<string>(v.at("state"));
      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json:" S_RED " %s" S_NOR) % e.what();
        return false;
      }

      BOOST_LOG_TRIVIAL(trace) << "LaidDown msg formed.";
      return true;
    }

    ADD_TO_FROM_STR_WITH_JSON_OR_PB // defines the to/fromString() methods

    /*
      🦜 : If we have protobuf, we can use that to serialize the object.
    */
    void fromPb(const hiPb::LaidDownMsg & pb) override {
      this->msg = pb.msg();
      this->epoch = pb.epoch();
      this->state = pb.state();
    }

    hiPb::LaidDownMsg toPb() const override {
      hiPb::LaidDownMsg o;
      o.set_msg(this->msg);
      o.set_epoch(this->epoch);
      o.set_state(this->state);
      return o;
    }

  }; // class LaidDownMsg

  // json functions for LaidDownMsg
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, LaidDownMsg const& c ){
    jv = {
      {"msg", c.msg},
      {"epoch", c.epoch},
      {"state", c.state}
    };
  }

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  ADD_FROM_JSON_TAG_INVOKE(LaidDownMsg);

  /**
   * @brief The new_view_certificate msg
   *
   * 🦜: This is the msg that a new primary needs to boardcast to the group. It
   * need to collect enough valid 'LaidDownMsg' in order to form the
   * `new_view_certificate`.
   */
  class NewViewCertificate:
    virtual public IJsonizable,
    virtual public ISerializableInPb<hiPb::NewViewCertificate>,
    virtual public ISerializable
  {
  public:
    string msg;
    int epoch;
    vector<string> new_view_certificate;
    vector<string> sig_of_nodes_to_be_added;
    vector<string> cmds;

    NewViewCertificate() = default;

    /**
     * @brief Create a new_view_certificate
     *
     * @param mmsg : The msg to be boardcast. Just for debugging purpose. Can be
     * arbitrary.
     *
     * @param eepoch : The new epoch. This is also unnecessary because the
     * agreed epoch should be already the `new_view_certificate`.
     *
     * @param nnew_view_certificate : The signed LaidDownMsg that primary collected.
     *
     * @param ssig_of_nodes_to_be_added : The signature of the new nodes to be
     * added in the turn.
     *
     * @param ccmds : The command history so far. These should only be sent to
     * the new nodes.
     */
    NewViewCertificate(string mmsg,int eepoch,vector<string> nnew_view_certificate,
                       vector<string> ssig_of_nodes_to_be_added,
                       vector<string> ccmds = {} // optional
                       ):
      msg(mmsg),
      epoch(eepoch),
      new_view_certificate(nnew_view_certificate),
      sig_of_nodes_to_be_added(ssig_of_nodes_to_be_added),
      cmds(ccmds)
    {}

    json::value toJson() const noexcept override {
      return json::value_from(*this);
    };
    bool fromJson(const json::value &v) noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("Forming NewViewCertificate from Json");

      try {

        this->msg = value_to<string>(v.at("msg"));
        this->epoch = value_to<int>(v.at("epoch"));
        this->new_view_certificate = value_to<vector<string>>(v.at("new_view_certificate"));
        this->sig_of_nodes_to_be_added = value_to<vector<string>>(v.at("sig_of_nodes_to_be_added"));
        this->cmds = value_to<vector<string>>(v.at("cmds"));

      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json:" S_RED " %s" S_NOR) % e.what();
        return false;
      }
      return true;
    }

    ADD_TO_FROM_STR_WITH_JSON_OR_PB // defines the to/fromString() methods

    /*
      🦜 : If we have protobuf, we can use that to serialize the object.
    */

    void fromPb(const hiPb::NewViewCertificate & pb) override {
      this->msg = pb.msg();
      this->epoch = pb.epoch();

      // copy
      this->new_view_certificate = vector<string>(pb.new_view_certificate().begin(),
                                                  pb.new_view_certificate().end());
      this->sig_of_nodes_to_be_added = vector<string>(pb.sig_of_nodes_to_be_added().begin(),
                                                      pb.sig_of_nodes_to_be_added().end());
      this->cmds = vector<string>(pb.cmds().begin(),
                                  pb.cmds().end());
    }

    hiPb::NewViewCertificate toPb() const override {
      hiPb::NewViewCertificate o;
      o.set_msg(this->msg);
      o.set_epoch(this->epoch);

      // copy
      for (auto s : this->new_view_certificate)
        o.add_new_view_certificate(s);

      for (auto s : this->sig_of_nodes_to_be_added)
        o.add_sig_of_nodes_to_be_added(s);

      for (auto s : this->cmds)
        o.add_cmds(s);

      return o;
    }

  };                          // class New_view_certificate

  // json functions for NewViewCertificate
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, NewViewCertificate const& c ){
    jv = {
      {"msg", c.msg},
      {"epoch", c.epoch},
    };

    jv.as_object()["new_view_certificate"] = json::value_from(c.new_view_certificate);
    jv.as_object()["sig_of_nodes_to_be_added"] = json::value_from(c.sig_of_nodes_to_be_added);
    jv.as_object()["cmds"] = json::value_from(c.cmds);
  }

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  ADD_FROM_JSON_TAG_INVOKE(NewViewCertificate);



  class RbftConsensus:
    public virtual ICnsssPrimaryBased,
    public std::enable_shared_from_this<RbftConsensus>
    // define shared_from_this()
  {
private:
  RbftConsensus(IAsyncEndpointBasedNetworkable * const n,
                IForConsensusExecutable * const e,
                IMsgManageable * const s,
                vector<string> all_endpoints):
    exe(e), sig(s), net(n)
    {
    // no need to lock here.
      this->all_endpoints.o = all_endpoints;


    // 🦜 : We can't use set for all_endpoints. Because new nodes must be
    // added to the end.

      if (not ::pure::contains(all_endpoints,this->net->listened_endpoint()))
      this->start_listening_as_newcomer();
    else{
      if (this->primary() == this->net->listened_endpoint())
        this->start_listening_as_primary();
      else
        this->start_listening_as_sub();

      // start the timer
      this->timer = std::thread(std::bind(&RbftConsensus::start_faulty_timer, this));
      // here the ctor ends
    }
  }
public:

    // 🐢 No public constructor, only a factory function, so there's no way to have
    // shared_from_this() return nullptr. (🦜 this is recommanded code from c++ official website)
    [[nodiscard]] static shared_ptr<RbftConsensus> create(IAsyncEndpointBasedNetworkable * const n,
                                                                 IForConsensusExecutable * const e,
                                                                 IMsgManageable * const s,
                                                          vector<string> all_endpoints){
      // Not using std::make_shared<B> because the c'tor is private.
      return shared_ptr<RbftConsensus>(new RbftConsensus(n,e,s,all_endpoints));
    }

    std::thread timer;

    std::atomic_flag closed = ATOMIC_FLAG_INIT;
    std::atomic_flag view_change_state = ATOMIC_FLAG_INIT;
    std::atomic_int epoch = 0;
    std::atomic_int patience = 0;

    /**
     * @brief The role of the node. Can be one of :
     *
     *  + 0 : Newcomer
     *  + 1 : Sub
     *  + 2 : Primary
     *
     *  🦜 : Why bother?
     *
     *  🐢 : Because we kinda feel like it's somewhat too wasteful to clear()
     *  and re-listen() the same things when a Sub node just needs to stay Sub
     *  in a new epoch.
     *
     *  🦜 : Make sense... That can save a lot of console output too.
     */
    std::atomic_char role = static_cast<char>(Role::Newcomer);
    enum class Role: char {Newcomer=0,Sub=1,Primary=2};

    LockedObject<
      unordered_map<int,
                    shared_ptr<
                      unordered_map<string,
                                    shared_ptr<
                                      vector<string>
                                      >
                                    >
                      >
                    >
      > laid_down_history;

    LockedObject<std::set<string>> sig_of_nodes_to_be_added;

    /*
      🦜 : This list of commands to be confirmed. This is kinda like a
      counter. When a command has reached more than 2f + 1 count (confirmed
      message) then the command can be savely executed.

      The meaning of to_be_confirmed_commands is kinda like : <cmd> : {set of who received commands}

      So when two sub-nodes call each other, they will exchange what
      they've got in their hand. As a result, eventually the node will
      still execute what primary sent to most nodes. (🦜: This eventually makes each nodes "selfless").
    */
    LockedObject<unordered_map<string,
                               shared_ptr<std::set<string>>
                               >> to_be_confirmed_commands;

    /*
      When a node has collected enough `comfirm` for a command, it will
      boardcast the `commit` for a command.

      When a node has collected enough `commit` for a command, it executes it
      no matter what. 
    */
    LockedObject<unordered_map<string,
                               shared_ptr<std::set<string>>
                               >> to_be_committed_commands;

    LockedObject<std::set<string>> received_commands;
    LockedObject<vector<string>> command_history;
    /*
     * 🐢 : Make the ctor private, see pure-ListenToOneConsensus.hpp for reason.
     */


    IAsyncEndpointBasedNetworkable * const net;
    IForConsensusExecutable * const exe;
    IMsgManageable * const sig;

    LockedObject<vector<string>> all_endpoints;

    void clear_memory(){
      this->view_change_state.clear(); // set to false
      {std::unique_lock l(this->received_commands.lock);
        this->received_commands.o.clear();
      }
}

    void clear_and_listen_common_things(){
      this->clear_memory();
      this->net->clear();
      this->net->listen("/ILaidDown", bind(&RbftConsensus::handle_laid_down,this,_1,_2));
      this->net->listen("/IamThePrimary", bind(&RbftConsensus::handle_new_primary,this,_1,_2));
      this->net->listen("/pleaseAddMeNoBoardcast",
                        bind(&RbftConsensus::handle_add_new_node_no_boardcast,this,_1,_2));
    }

    /**
     * @brief Endpoint said it's the primary, if the data contains the required
     things, then we follow it.

     Here we should also handle adding-new nodes.

     @param data: Serialized NewViewCertificate
    */
    void  handle_new_primary(string endpoint, string data){
      NewViewCertificate o;
      if (not o.fromString(data)){
        this->say(format("❌️Error parsing NewViewCertificate " S_RED "%s" S_NOR) %
                  pure::get_data_for_log(data)
                  );
        return ;
      }
      vector<string> newcomers = this->get_newcommers(o.sig_of_nodes_to_be_added);
      {
        std::unique_lock l(this->all_endpoints.lock);
        for (auto newcomer : newcomers){
          if (::pure::contains(this->all_endpoints.o,newcomer)){
            this->say("This primary has old newcomer, ignoring it");
            return;
          }
        }
      } // unlocks here

      if (this->epoch.load() > o.epoch){
        this->say(format("Ignoring older msg in epoch=" S_CYAN "%d" S_NOR) % o.epoch);
        return;
      }

      if (this->check_cert(o.new_view_certificate,o.epoch,false)){
        /*
          🦜 : Now we are ready to start a new view. We do the following:

          1. update the epoch
          2. reset the timer
          3. clear the laid_down_history
          4. clear the sig_of_nodes_to_be_added
          5. add the newcomers as per primary.
          6. start as sub

          🦜 : Every time new nodes are added in the cluster, the epoch will be
          recalculated .
        */

        // 1.
        int e = o.epoch;
        this->epoch.store(this->epoch_considering_newcomers(e,newcomers.size()));
        this->say(format("🐸 changed to %d") % this->epoch.load());

        // 2.
        this->comfort();

        // 3.
        {
          std::unique_lock l(this->laid_down_history.lock);
          this->laid_down_history.o.clear();
        } // unlocks here

        {
          std::unique_lock l(this->sig_of_nodes_to_be_added.lock);
          this->sig_of_nodes_to_be_added.o.clear();
        } // unlocks

        {
          std::unique_lock l(this->all_endpoints.lock);
          // all_endpoints += newcomers

          /*
            🦜 : I don't think there will be a lot of newcomers in each round,
            so I'm just gonna stick to push_back.
          */
          for (auto newcomer : newcomers){
            this->all_endpoints.o.push_back(newcomer);
          }
        } // unlocks


        this->start_listening_as_sub();
      }else{
        // check cert failed
        this->say(format("❌️ Invalid certificate: " S_RED " %s " S_NOR " from " S_CYAN " %s " S_NOR ", Do nothing.")
                  % o % endpoint);
      }
    } // handle_new_primary

    /**
     * @brief Check wether the view-change certificate is valid.

     🦜 : The view-change certificate is valid if

     1. All msgs in it are properly signed.

     2. The state-hash and epoch of them are all the same. In particular,
     the epoch of them should all be equal to `e`. And the state(-hash)
     should all be equal to our hash

     3. And they must have been sent from different nodes.

     4. And that's it...?

     🐢 : Yeah, and it is also possible that we are different from the
     world, in that case... We stop

    */
    bool check_cert(vector<string> l, int e, bool for_newcomer = true){
      std::set<string> valid_host;

      try{
        BOOST_ASSERT_MSG(l.size() != 0, "What ? Checking empty certificate ? There's no such thing.");
        /*
          🦜 : What are the elements in l?

          🐢 : LaidDownMsg

          🦜 : What's in it?

          🐢 : 'epoch' and 'state', and also, because it's signed, so the `from`
          is there too. Note that the msg itself doesn't need to contain the
          `from` field, because it's supposed to be signed.

        */
        string s = l[0];
        optional<tuple<string,string>> sr = this->sig->tear_msg_open(s);

        BOOST_ASSERT_MSG(sr,
                         (format("Even the first signature = %s is wrong") % s).str().c_str()
                         );

        // 🦜 : Boost assert can only accept cstring.. so we kinda have to make an extra c_str()
        LaidDownMsg msg0;
        BOOST_ASSERT_MSG(msg0.fromString(std::get<1>(sr.value())),
                         "Failed to form LaidDownMsg from string");

        this->say("Start inserting into valid_host");
        valid_host.insert(std::get<0>(sr.value()));

        if (not for_newcomer)
          BOOST_ASSERT_MSG(
                           msg0.state == this->get_state(),
                           (format("State mismatch: %s != %s") % msg0.state % this->get_state()).str().c_str()
                           );

        for (auto it = l.begin()+1;it < l.end();it++){

          auto o = this->sig->tear_msg_open(*it);
          BOOST_ASSERT(o);
          LaidDownMsg msg;
          BOOST_ASSERT(msg.fromString(std::get<1>(o.value())));
          valid_host.insert(std::get<0>(o.value()));

          if (for_newcomer){
            BOOST_ASSERT(msg.state == msg0.state);
          }else{
            BOOST_ASSERT(msg.state == this->get_state());
          }

          /*
            🐢  : When a newcomer receives a new-view msgificate, it
            also need to check it. But it won't check the state.
          */
          BOOST_ASSERT(msg.epoch == e);

        }

        /*
          🦜 : How many laiddown do we need ?

          🐢 : Every correct node sends this, so there should be N - f
        */

        size_t x = this->N() - this->f();
        BOOST_ASSERT_MSG(valid_host.size() >= x,
                         (
                          format("Requires %d LaidDownMsg%s, but got %d")
                          % x % pluralizeOn(x) % valid_host.size()
                          ).str().c_str()
                         );
      }
      catch(my_assertion_error & e){
        this->say(format(S_RED "❌️ Assertion error: %s" S_NOR) % e.what());
        return false;
      }

      // this->say(format("Received ok view-change-vertificate"))
      return true;
    } // check_cert

    void start_listening_as_primary(){

      /*
        🦜 : Okay, I feel like the following is not very likely. Let's ignore it.

        🐢 : Yeah... But becoming Sub again. I think it's common enough to do something
       */

      // if (this->role.load() == static_cast<char>(Role::Primary)){
      //   this->say("\t 🌱: Becoming primary again (should be in one-node cluster)");
      //   this->clear_memory();
      //   return ;
      // }

      this->say("\t 🌱: Start listening as primary");
      this->clear_and_listen_common_things();
      this->net->listen("/pleaseExecuteThis",
                        bind(&RbftConsensus::handle_execute_for_primary1,this,_1,_2)
                        );

      this->role.store(static_cast<char>(Role::Primary));
    }

    void start_listening_as_sub(){

      if (this->role.load() == static_cast<char>(Role::Sub)){
        this->say("\t 📗️: Becoming sub again.");
        this->clear_memory();
        return;
      }

      this->say("\t 🌱: Start listening as sub");
      this->clear_and_listen_common_things();
      this->net->listen("/pleaseExecuteThis",
                        bind(&RbftConsensus::handle_execute_for_sub1,this,_1,_2)
                        );

      this->net->listen("/pleaseConfirmThis",
                        bind(&RbftConsensus::handle_confirm_for_sub,this,_1,_2)
                        );

      this->net->listen("/pleaseCommitThis",
                        bind(&RbftConsensus::handle_commit_for_sub,this,_1,_2)
                        );

      this->role.store(static_cast<char>(Role::Sub));
    }

    /**
     * @brief The current primary
     */
    string primary() const {
      // 🦜 : We don't lock it here. Otherwise it will lock everything somebody say...

      return this->all_endpoints.o[this->epoch.load() % this->all_endpoints.o.size()];
    }

    /**
     * @brief Make a `state string` according to `self.command_history`.
     *
     * Get the (hash of) state, which should only be changed by the cmd executed
     so far.

     🦜 : Technically we should hash the commands history so far, but for
     now...

     🐢 : Let's just use:
    */
    static string cmds_to_state(vector<string> /*cmds*/){
      return "<mocked-state>";
      // return boost::algorithm::join(cmds,":");
    }

    string get_state(){
      std::unique_lock l(this->command_history.lock);
      return RbftConsensus::cmds_to_state(this->command_history.o);
    }
    string get_signed_state(){
      return this->sig->prepare_msg(this->get_state());
    }

    /**
     * @brief The execute interface exposed to the outside world.
     */
    optional<string> handle_execute_for_sub(string endpoint, string data) override{
      handle_execute_for_sub1(endpoint, data);
      return "";
    }

    void handle_execute_for_sub1(string endpoint, string data){
      /*
        🐢 : All things starts with handle_execute_for_sub/primary(), which
        is like the gateway of the node. So if we disable this, then...
      */

      if (this->view_change_state.test()){
        BOOST_LOG_TRIVIAL(error) << format("❌️ Dear %s, we are currently selecting new primary "
                                           "for epoch %d. Please try again later.") % endpoint % this->epoch.load();
        return;
      }
      if (endpoint == this->primary()){
        /*
          🦜 : Boardcast to other subs

          🦜 : take out the endpoints, because self.primary() will also lock
          'all_endpoints'
        */

        this->boardcast_to_other_subs("/pleaseConfirmThis",data);

        this->say(S_BLUE "\t\tConfirmming cmd to myself" S_NOR);
        this->add_to_to_be_confirmed_commands(this->net->listened_endpoint(),data);
        // done
      }else{
        // forward, it's from the client
        this->net->send(this->primary(),"/pleaseExecuteThis",data);
        // done
      }
    }

    /**
     * @brief Remember that endpoint `received` data.

     * 🦜 : Is this the only method that touch `self.to_be_confirmed_commands` ?

     * 🐢 : Looks so. When `to_be_confirmed_commands` have collected enough. Add
     that to the `to_be_confirmed_commands` (this might boardcast
     '/pleaseCommitThis'.)

    */
    void  add_to_to_be_confirmed_commands(string endpoint, string data){
      std::unique_lock l(this->to_be_confirmed_commands.lock);

      if (not this->to_be_confirmed_commands.o.contains(data)){

        this->say(format("Adding " S_MAGENTA "%s " S_NOR " from  " S_MAGENTA "%s" S_NOR)
                  % pure::get_data_for_log(data)
                  % endpoint);
        this->to_be_confirmed_commands.o[data] = make_shared<std::set<string>>();
      }

      // 🦜 : reference the one (this is not something that we can do in OOthon)
      shared_ptr<std::set<string>> s = this->to_be_confirmed_commands.o[data];
      s->insert(endpoint);

      /*

        🦜 : How many we should collect?

        🐢 : I think it should be N - 1 - f, 1 corresponds to the
        primary. For example, when N = 2, there should be one confirm
        (the one N1 put it after received the command). When N = 3,
        there should be two, (needs an extra one from N2).

      */

      long unsigned int x = this->N() - 1 - this->f();

      if (s->size() >= x){
        this->say(format("⚙️ command " S_CYAN "%s " S_NOR
                         " confirmed by " S_CYAN "%d " S_NOR " node%s, boardcasting commit")
                  % pure::get_data_for_log(data) % s->size() % pluralizeOn(s->size())
                  );

        this->boardcast_to_other_subs("/pleaseCommitThis", data);
        this->say(S_GREEN "\t\tCommitting cmd to myself" S_NOR);
        this->add_to_to_be_committed_commands(this->net->listened_endpoint(),data);

        // remove the entry at all
        this->to_be_confirmed_commands.o.erase(data);
      }else{
        this->say(format("⚙️ command " S_CYAN "%s " S_NOR
                         " confirmed by " S_CYAN "%d " S_NOR " node%s, not yet.")
                  % pure::get_data_for_log(data) % s->size() % pluralizeOn(s->size())
                  );
      }
    }

    void boardcast_to_other_subs(string target, string data){
      vector<string> all_endpoints;
      {
        std::unique_lock l(this->all_endpoints.lock);
        all_endpoints = this->all_endpoints.o;
      } // unlocks


      for (const string & sub : all_endpoints){
        if (sub != this->net->listened_endpoint()
            and
            sub != this->primary()
            ){
          this->net->send(sub,target,data);
        }
      }
    }

    void finally_execute_it(string data){
      this->exe->execute(data);
      std::unique_lock l(this->command_history.lock);
      this->command_history.o.push_back(data);
    }



    /**
     * @brief Remember that endpoint `received` data.

     *         When `to_be_committed_commands` have collected enough. It will finally
     *         execute.

     *         🦜 : Most of the code here is copied from add_to_to_be_confirmed_commands
     */
    void  add_to_to_be_committed_commands(string endpoint, string data){
      std::unique_lock l(this->to_be_committed_commands.lock);

      string data_for_log = pure::get_data_for_log(data);
      if (not this->to_be_committed_commands.o.contains(data)){
        // this->say(format("Adding " S_MAGENTA "%s " S_NOR " from  " S_MAGENTA "%s" S_NOR)
        //           % data % endpoint);

        this->say(format("Adding " S_MAGENTA "%s " S_NOR " from  " S_MAGENTA "%s" S_NOR)
                  % data_for_log % endpoint);
        this->to_be_committed_commands.o[data] = make_shared<std::set<string>>();
      }

      // 🦜 : reference the one (this is not something that we can do in OOthon)
      shared_ptr<std::set<string>> s = this->to_be_committed_commands.o[data];
      s->insert(endpoint);

      /*

        🦜 : How many we should collect?

        🐢 : I think it should be N - 1 - f, 1 corresponds to the
        primary. For example, when N = 2, there should be one confirm
        (the one N1 put it after received the command). When N = 3,
        there should be two, (needs an extra one from N2).

      */

      std::size_t x = this->N() - 1 - this->f();
      if (s->size() >= x){
        this->say(format("⚙️ command " S_CYAN "%s " S_NOR
                         " committed by " S_CYAN "%d " S_NOR " node%s, execute it and clear 🚮️")
                  % data_for_log % s->size() % pluralizeOn(s->size())
                  );
        this->finally_execute_it(data);
        // remove the entry at all
        this->to_be_committed_commands.o.erase(data);
      }else{
        this->say(format("⚙️ command " S_CYAN "%s " S_NOR
                         " committed by " S_CYAN "%d " S_NOR " node%s, not yet.")
                  % data_for_log % s->size() % pluralizeOn(s->size())
                  );
      }
    }

    void  handle_commit_for_sub(string endpoint, string data){
      if (this->view_change_state.test()){
        BOOST_LOG_TRIVIAL(error) << format("❌️ Dear %s, we are currently selecting new primary "
                                           "for epoch %d. Please try again later.") % endpoint % this->epoch.load();
        return;
      }
      this->add_to_to_be_committed_commands(endpoint, data);
    }

    void  handle_confirm_for_sub(string endpoint, string data){
      if (this->view_change_state.test()){
        BOOST_LOG_TRIVIAL(error) << format("❌️ Dear %s, we are currently selecting new primary "
                                           "for epoch %d. Please try again later.") % endpoint % this->epoch.load();
        return;
      }
      this->add_to_to_be_confirmed_commands(endpoint, data);
    }

    std::size_t N(){
      std::unique_lock l(this->all_endpoints.lock);
      return this->all_endpoints.o.size();
    }
    /**
     * @brief Get the number of random nodes the system can tolerate
     */
    std::size_t f(){
      std::unique_lock l(this->all_endpoints.lock);
      long unsigned int N = this->all_endpoints.o.size();
      return (N - 1) / 3;
      // BOOST_CHECK_EQUAL(1/3,0);
      // BOOST_CHECK_EQUAL(2/3,0);
      // BOOST_CHECK_EQUAL(3/3,1);
      // BOOST_CHECK_EQUAL(4/3,1);

      // 🦜 : C by default, does truncated division.
    }

    /**
     * @brief Start the faulty timer.

     * Life always has up-and-downs, but time moves toward Qian.

     *         🦜: The faulty timer is just a clock that keeps calling
     *         trigger_view_change(). It will not be reset by anybody except a
     *         new-primary with a new-view-certificate.
     */
    void start_faulty_timer(){
      while (not this->closed.test()){
        this->comfort();

        // 🦜 : Here we used atomic, so we don't need to lock it.
        while (this->patience.load() > 0){
          std::this_thread::sleep_for(std::chrono::seconds(3));
          // this->say("closed?");
          if (this->closed.test()){
            this->say("\t 👋 Timer closed");
            return;
          }
          // this->say("down?");
          this->patience--;
          this->say(format(" patience >> " S_MAGENTA "%d " S_NOR ", 🐢: Pr : " S_CYAN "%s" S_NOR)
                    % this->patience.load() %
                    ICnsssPrimaryBased::make_endpoint_human_readable(this->primary())
                    );
        }

        // here the patience is run off
        this->trigger_view_change();
        // 🦜 : We trigger the view-change and reset the patience.
      }

      this->say("\t 👋 Timer closed");
    }

    void comfort(){
      this->patience.store(8);
      this->say(format("❄ patience set to %d") % this->patience.load());
    }

    void trigger_view_change(){
      /*
        When a node triggers the view-change, it borad cast something like
        'I laid down with this state: {...} for view {0}'

        And others will listens to it.
      */

      this->epoch++;
      this->say("ViewChange triggered");

      this->view_change_state.test_and_set(); // set to true

      string next_primary = this->primary();

      LaidDownMsg msg{
        "Dear " + next_primary + ", I laid down for U.",
        this->epoch.load(),
        this->get_state()
      };

      string data = this->sig->prepare_msg(msg.toString());

      // Send to next_primary my state
      if (next_primary == this->net->listened_endpoint()){
        this->say(S_MAGENTA "Send laiddown for myself" S_NOR);
        this->remember_this_laiddown_and_be_primary_maybe(msg,std::move(data));
      }else{
        this->net->send(next_primary,"/ILaidDown",data);
      }
    }

    /**
     * @brief Response to a `laid-down` msg.

     *  🐢 : The goal is to see wether the `next primary` is OK. We need to
     *  receive 2f msgs (plus this node itself, then it should have 2f + 1 state) to check
     *  that.

     *  In particular, the state of `next primary` must be known.

     *  🦜 : What if we already got 2f msgs, but didn't got the one from `next
     * primary`?

     *  🐢 : Let's ask it explicitly?

     *  🦜 : Yeah, and if it doesn't reply. We do a new view-change.
     *
     *  🦜 : Wait... I changed my mind. Let's just ignore it if it's down and
     * wait for the next view change.
     *
     * @param endpoint The node that sends the laiddown msg. This node
     * (probably) wants us to be the primary.
     *
     * @param data The signed LaidDownMsg
     */
    void  handle_laid_down(string endpoint, string data){
      try {
        this->say(
                  (format("Handling laid_down from %s") %
                   ICnsssPrimaryBased::make_endpoint_human_readable(endpoint)
                   ).str()
                  );

        auto o = this->sig->tear_msg_open(data);
        BOOST_VERIFY(o);

        LaidDownMsg msg;
        BOOST_VERIFY(msg.fromString(std::get<1>(o.value())));

        if (msg.epoch < this->epoch.load()){
          this->say(format("🚮️ Ignoring lower epoch laid-down msg: epoch=%d")
                    % msg.epoch);
          return ;
        }

        /*
          🦜 : I feel like we should only use BOOST_VERIFY if it's something
          important.... Bucause they will be painted in red.
        */

        this->say("Getting next primary");
        string next_primary;
        {
          std::unique_lock l(this->all_endpoints.lock);
          next_primary = this->all_endpoints.o[
                                                 msg.epoch %
                                                 this->all_endpoints.o.size()
                                                 ];

        } // unlocked here
        this->say("Got next primary = " +
                  ICnsssPrimaryBased::make_endpoint_human_readable(next_primary)
                  );

        if (next_primary != this->net->listened_endpoint()){
          this->say(format("🚮️ This view-change is for " S_CYAN "%s " S_NOR ",non of my bussinesses.")
                    % next_primary);
          return;
        }

        this->remember_this_laiddown_and_be_primary_maybe(msg,data);
      }catch (my_assertion_error & e){
        this->say(format("❌️ Assertion error:" S_RED "\n\t%s" S_NOR) % e.what());
        // do nothing
        return;
      }
    }

    /**
     * @brief Add the laid-down msg `o` for this node into laid_down_history and
     * if enough laid-down msg is collected, then be the primary.

     * @param data `data` should be the signed form of `o`:
     *         data = this->sig->prepare_msg(o.fromString())
     *
     * @param o the newly added LaidDownMsg
     */
    void remember_this_laiddown_and_be_primary_maybe(const LaidDownMsg & o, const string data){

      // this->say("Start remembering");
      // string state = o.state;
      // int epoch = o.epoch;
      shared_ptr<vector<string>> to_be_added_list;

      // this->say("1.");
      {
        std::unique_lock l(this->laid_down_history.lock);
        // 🦜 : 1. Does this epoch already has something ? If not, create a new dict{}
        if (not this->laid_down_history.o.contains(o.epoch)){

          this->say(format("\tAdding new record in laid_down_history for epoch " S_GREEN "%d " S_NOR)
                    % o.epoch);
          this->laid_down_history.o.insert({
              o.epoch,
              make_shared<
              unordered_map<string,shared_ptr<vector<string>>>
              //            state : votes for state
              >()});
        }

        // this->say("2.");
        // 🦜 : 2. Does this epoch already got state like this ? If not, create a new list[]
        if (not this->laid_down_history.o.at(o.epoch)->contains(o.state)){
          this->say(format("\tAdding new record in laid_down_history for " S_GREEN "epoch=%d,state=%s" S_NOR)
                    % o.epoch % o.state);
          this->laid_down_history.o.at(o.epoch)->insert({
              o.state,
              make_shared<vector<string>>()
            });

        }

        // 🦜 Now take the list
        to_be_added_list = this->laid_down_history.o.at(o.epoch)->at(o.state);
      } // unlocks here

      // this->say("3.");
      // 🦜 : This is the list that data (signed msg) will be added in.
      if (::pure::contains(to_be_added_list, data)){
        this->say(format("🚮️ Ignoring duplicated msg:" S_CYAN "%s" S_NOR)% pure::get_data_for_log(data));
        return;
      }
      to_be_added_list->push_back(data);

      /*

        🦜 : How many laiddown msg does one need to collect in order to be the new primary?

        🐢 : Every correct node will append to it even the next primary itself.
        so it's N - f.

      */

      // this->say("4.");
      int x = this->N() - this->f();
      if (to_be_added_list->size() >= x){
        this->say(format("Collected enough " S_GREEN "%d " S_NOR " >= " S_CYAN "%d" S_NOR " LaidDownMsg%s")
                  % to_be_added_list->size() % x % pluralizeOn(to_be_added_list->size())
                  );
        this->try_to_be_primary(o.epoch,o.state,to_be_added_list);
      }else{
        this->say(format("Collected " S_MAGENTA "%d " S_NOR " <= " S_CYAN "%d" S_NOR " LaidDownMsg%s"
                         S_MAGENTA " not yet the time" S_NOR)
                  % to_be_added_list->size() % x % pluralizeOn(to_be_added_list->size())
                  );
        /*
          🐢 No need to return the to_be_added_list, because we are using shared_ptr<>
        */
      }

      /*
        🦜 : It is my bussinesses and the epoch is right for me. I am just
        ganna get majority of same-state, and if I am not one of them... I will
        sync to one of them?

        🐢 : For now, let's just skip that step..We do that later.

      */
    }

    /**
     * @brief Verify and get the newcommers collected.
     */
    vector<string> get_newcommers(const vector<string> & sigs) const {
      std::set<string> o;
      for (const string & sig : sigs ){
        auto r = this->sig->tear_msg_open(sig);
        if (not r)
          continue;

        // else
        string n = std::get<0>(r.value());
        if (o.insert(n).second) // 🦜 added
          this->say(format(" Verified node : " S_GREEN "%s" S_NOR) % n);
      }
      vector<string> r(o.begin(),o.end()); // copy
      return r;
    }

    void try_to_be_primary(int e, string state, shared_ptr<vector<string>> my_list){
      this->say(
                format("This's my time for epoch " S_GREEN "%d" S_NOR ", my state is\n\t"
                       S_CYAN "%s\n\t" S_NOR
                       "and the majority has state\n\t"
                       S_CYAN "%s\n\t" S_NOR
                       ) % e % this->get_state() % state
                );

      {
        // Should be my-view
        std::unique_lock l(this->all_endpoints.lock);
        // 🐢 : If U call self.N() here, dead lock happens.
        BOOST_ASSERT(
                     this->all_endpoints.o[e % this->all_endpoints.o.size()]
                     ==
                     this->net->listened_endpoint()
                     );

      }     // unlock

      BOOST_ASSERT_MSG(this->get_state() == state,
                       "I am different from other correct nodes. "
                       "The cluster might contained too many random nodes."
                       );

      /*

        🐢 : Add those new nodes, and notify them that they are in. The things
        that needs to be sent to the newcomers are different from those sent to
        other subs. In particular, it needs to send 'cmds' to those newcomers
        too, in addition to what's sent to everybody.

      */

      unique_ptr<vector<string>> sig_for_newcomers;
      // pop the newcomers
      {
        std::unique_lock l(this->sig_of_nodes_to_be_added.lock);
        // set to list
        sig_for_newcomers = make_unique<vector<string>>(this->sig_of_nodes_to_be_added.o.begin(),
                                                        this->sig_of_nodes_to_be_added.o.end()
                                                        );
        this->sig_of_nodes_to_be_added.o.clear();
      } // unlocks
      vector<string> newcomers = this->get_newcommers(*sig_for_newcomers);

      if (newcomers.empty()){
        /*
          🐢 : If there's no newcomer, then the life is much easier:
          We just need to boardcast the new-view-certificate.
        */

        NewViewCertificate m{
          "Hi, I am the primary now" /*msg*/ ,
          e /*epoch*/ ,
          *my_list /*new_view_certificate*/
          ,
          {} /*sig_of_nodes_to_be_added*/
        };

        this->epoch.store(e);
        this->boardcast_to_others("/IamThePrimary",m.toString());
      }else{
        /*
          🐢 : But if there're newcomers, then we have to do the
          following:

          1. Recalculate the 'to-be-updated' epoch such that this node is the
          new primary. In details, the new epoch should be

          self.epoch =  e + (e\\N) * len(newcomers)            (1)

          However, the current epoch (before calculation) should still be
          saved and passed over the network because the msgs in
          `new-viwe-certificate` will be verified against this epoch (🐢 : No
          worries, the subs will calculate the `to-be-updated epoch`
          themselves.)

          2. Boardcast to existing nodes the `self.sig_of_nodes_to_be_added`.
          The other nodes should be able to derive the `newcomers` from it.
          Inside this msg, the `sig_of_nodes_to_be_added` should contains the
          new nodes to be added, and the `epoch` should be the *old one* we just
          used. (🦜 : This epoch should be same as the one that appears in
          the new-view-certificate.)

          When sub nodes received this,
          it will first see that the
          `sig_of_nodes_to_be_added` (and the newcomers derived from it) is
          not empty. Next, it will verify the `new-view-certificate`
          according to the epoch passed.

          But then, it will set its epoch according to eqn (1).

          Just like the primary. (🦜 : So when passing things aroung over the
          network, they all talk about the 'low' epoch, but in fact they will
          update their epoch to equiation (1), 🐢 : Yes)

          3. Notify the newcomers. This is basically the same process as for
          the subs. It's just that 'cmds' will also be sent inside the msg.
        */

        // 1.
        this->epoch.store(this->epoch_considering_newcomers(e,newcomers.size()));

        // 2.

        NewViewCertificate m{
          "Hi, I am the primary now" /*msg*/ ,
          e /*epoch*/ ,
          *my_list /*new_view_certificate*/ ,
          *sig_for_newcomers /*sig_of_nodes_to_be_added*/
        };
        this->boardcast_to_others("/IamThePrimary",m.toString());

        // 3.
        {
          std::unique_lock l(this->all_endpoints.lock);
          for (const string & newcomer : newcomers)
            this->all_endpoints.o.push_back(newcomer);
        }

        {
          std::unique_lock l(this->command_history.lock);
          // 🦜 Append cmds in the new-view cert
          m.cmds = this->command_history.o;
        }

        for (const string & newcomer : newcomers)
          this->net->send(newcomer,"/IamThePrimaryForNewcomer",m.toString());

        // 🦜 : In fact, the above can all be called asynchronously.

      } // case when there're newcomers

      // 🐢 be the primary

      this->say(format("\t🌐️ Epoch set to " S_MAGENTA "%d" S_NOR)
                % this->epoch.load());

      this->start_listening_as_primary();

    } // try_to_be_primary

    /**
     * @brief The execute interface exposed to the outside world. (we need to
     * override it.)
     */
    optional<string> handle_execute_for_primary(string endpoint, string data) override{
      handle_execute_for_primary1(endpoint,data);
      return "";
    }

    void handle_execute_for_primary1(string endpoint, string data){
      if (this->view_change_state.test()){
        BOOST_LOG_TRIVIAL(error) << format("❌️ Dear %s, we are currently selecting new primary "
                                           "for epoch %d. Please try again later.") % endpoint % this->epoch.load();
        return;
      }

      {
        std::unique_lock l(this->received_commands.lock);
        if (this->received_commands.o.contains(data)){
          this->say("\tIgnoring duplicated cmd " S_MAGENTA + data + S_NOR);
          return;
        }
      } // unlocks here

      this->finally_execute_it(data);

      /*
        🦜 : Can primary just execute it?

        🐢 : Yeah, it can. Then it just assume that the group has 2f + 1 correct
        nodes, and that should be fine. It doesn't bother checking
      */

      this->boardcast_to_others("/pleaseExecuteThis",data);
    }

    /**
     * @brief Check whether I am primary
     */
    bool is_primary() const noexcept override{
      if (this->view_change_state.test())
        return false;

      return this->primary() == this->net->listened_endpoint();
    }

    template<typename T>
    void say(T s) const{
      string my_id = "NewCommer";

      {
        std::unique_lock l(this->all_endpoints.lock);
        auto v = &(this->all_endpoints.o);
        if (::pure::contains(this->all_endpoints.o,this->net->listened_endpoint()))
          my_id = lexical_cast<string>(
                                       std::distance(v->begin(),
                                                     ranges::find(*v,this->net->listened_endpoint()))
                                       );
      } // unlocks here
#if defined (WITH_PROTOBUF)
      my_id += (": " +  ICnsssPrimaryBased::make_endpoint_human_readable(this->net->listened_endpoint()));
#endif

      // 🦜 : Don't show this in actual run ⚠️
      string cmds;

#if defined (BFT_STATE_DEBUG)
      {
        std::unique_lock l(this->command_history.lock);
        cmds = S_GREEN "[" +
          boost::algorithm::join(this->command_history.o,",") +
          "]" S_NOR;
      }
#endif


      BOOST_LOG_TRIVIAL(debug)
        << S_CYAN "[" + my_id + "]" S_NOR " "
        << cmds
        << " " S_BLUE "<" << this->epoch.load() << ">: " S_NOR
        << s;
    }

    /**
     * @brief Board cast target with data to all other nodes in
     `self.all_endpoints.
    */
    void boardcast_to_others(string_view target, string_view data){
      std::unique_lock l(this->all_endpoints.lock);
      for (const string & node : this->all_endpoints.o)
        if (node != this->net->listened_endpoint())
          this->net->send(node, string(target), string(data));
    }

    /**
     * @brief Add a new node.
     *
     * this just remembers the add-new msg, which should have been signed by
     putting it into the self.sig_of_nodes_to_be_added.
    */
    void  handle_add_new_node_no_boardcast(string endpoint, string data){
      /*
        🐢 : Using a set is ok, in fact only the next-primary needs to
        remember it, and it will boardcast it when becoming the new primary. At
        that time, it turns the set into a list.

        🦜 : Do we need to check the result ?

        🐢 : Technically, we do. And the majority should answer 'OK'. But...

        🦜 : Yeah..
      */

      std::unique_lock l(this->sig_of_nodes_to_be_added.lock);
      this->sig_of_nodes_to_be_added.o.insert(data);
    }

    void start_listening_as_newcomer(){
      /*
        🦜 : Here we used `self.boardcast_to_others()`, but because this
        node is not in `self.all_endpoints` yet, so it will in fact boardcast
        to the whole group.
      */
      this->boardcast_to_others("/pleaseAddMeNoBoardcast", this->sig->prepare_msg("Hi, please let me in."));
      this->net->listen("/IamThePrimaryForNewcomer",
                        bind(&RbftConsensus::handle_new_primary_for_newcomer,this,_1,_2)
                        );
    }
    void  handle_new_primary_for_newcomer(string endpoint, string data){
      /*
        🦜 : The data for handle_new_primary_for_newcomer should be same as the
        one received by handle_new_primary(), but with the additional field :
        'cmds', which contains the cmds that the new node should execute.

        So it should be parsed into something like

        d = {
        'epoch' : 2,
        'sig_of_nodes_to_be_added' : ['sig1','sig2'],
        'cmds' : ['c1','c2'],
        'new-view-certificate' : ['sig1','sig2'],
        'msg' : 'Hi, I am the primary now'
        }
      */

      NewViewCertificate d;
      if (not d.fromString(data)){
        this->say("❌️ Failed to parse data into NewViewCertificate:\n" S_RED + data + S_NOR ", ignoring it.");
        return;
      }

      this->say(format("Checking new cert with \n"
                       // "\t" S_MAGENTA "all_endpoints = %s" S_NOR
                       "\t " S_MAGENTA "cert = %s, epoch = %d" S_NOR
                       ) %
                boost::algorithm::join(d.new_view_certificate,",")
                % d.epoch);

      if (not this->check_cert(d.new_view_certificate,d.epoch,true /*for_newcomer*/)){
        this->say(
                  ("Invalid certificate: " S_RED) +
                  data +
                  (S_NOR "from " S_CYAN) +
                  endpoint +
                  S_NOR ". Do nothing."
                  );
        return ;
      }

      vector<string> newcomers = this->get_newcommers(d.sig_of_nodes_to_be_added);

      /*
        `self.epoch_considering_newcomers` will access
        `self.all_endpoints`, so make sure to calculate the `epoch` before
        calling:

        self.all_endpoints += newcomers

        This is important, otherwise you get wrong epoch.
      */

      this->epoch.store(this->epoch_considering_newcomers(d.epoch,
                                                          newcomers.size()));
      this->say(format("\t 🌐️ Epoch set to " S_MAGENTA "%d" S_NOR)
                % this->epoch.load());

      {
        std::unique_lock l(this->all_endpoints.lock);

        /*
          🦜 : I don't think there will be a lot of newcomers in each round,
          so I'm just gonna stick to push_back.
        */
        for (auto newcomer : newcomers){
          this->all_endpoints.o.push_back(newcomer);
        }

        BOOST_ASSERT_MSG(::pure::contains(this->all_endpoints.o,this->net->listened_endpoint()),
                         "What? I am not added by the new primary?");
      } // unlocks

      for (string & cmd : d.cmds){
        this->exe->execute(cmd);
      }
      // no need to lock here
      this->command_history.o = d.cmds;

      // start the timer

      this->timer = std::thread(std::bind(&RbftConsensus::start_faulty_timer, this));

      this->start_listening_as_sub();

    }

    /**
     * @brief The equivalent epoch number when there're newcomers.
     *
     * 🦜 : Note that when there's no newcomers, then it's just e.
     */
    int epoch_considering_newcomers(int e, int num_newcomers){
      return e + (e / this->N()) * num_newcomers;
    }

    virtual ~RbftConsensus(){
      BOOST_LOG_TRIVIAL(debug) <<  "👋 BFT closed";
      this->net->clear();          // 🐢 : Make sure to close the cnsss before net
      this->closed.test_and_set(); // closed = true
      this->timer.join();          // wait for the timer
    }
  };                            // class RbftConsensus

  /*
    --------------------------------------------------

    🦜 :Below we try to use the PBFT.
   */



} // namespace pure
