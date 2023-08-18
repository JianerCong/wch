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
#include <thread>


#include <mutex>
namespace pure{
  using namespace std::placeholders;  // for _1, _2, _3...
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

  class ISignable{
  public:
    virtual string sign(string_view s) noexcept=0;

    virtual bool verify(string_view msg) noexcept=0;

    /**
     * @brief Extract the data in msg.
     *
     * @param msg The signed data.
     *
     * 🦜 : It is assumed that the `msg` has been verified using verify().
     * Otherwise, this and get_from() will return rabbish.
     */
    virtual string_view get_data(string_view msg) noexcept=0;
    virtual string_view get_from(string_view msg) noexcept=0;
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


  /**
   * @brief Msg sent to the next primary when a node is ready to do view-change.
   *
   * This one is supposed to be signed before being sent. So it doesn't need to
   * contain the `from` field.
   */
  class LaidDownMsg:
    virtual public IJsonizable,
    virtual public ISerializable
  {
  public:
    LaidDownMsg() = default;
    LaidDownMsg(string mmsg,int eepoch,string sstate):
      msg(mmsg), epoch(eepoch), state(sstate) {};
    string msg;
    int epoch;
    string state;

    string toString() const noexcept override {
      return IJsonizable::toJsonString();
    }

    bool fromString(string_view s) noexcept override{
      BOOST_LOG_TRIVIAL(debug) <<  "Forming NewViewCertificate from string.";
      return IJsonizable::fromJsonString(s);
    }

    json::value toJson() const noexcept override {
      return json::value_from(*this);
    }

    bool fromJson(const json::value &v) noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("Forming NewViewCertificate from Json");
      try {
        this->msg = value_to<string>(v.at("msg"));
        this->epoch = value_to<int>(v.at("epoch"));
        this->state = value_to<string>(v.at("state"));
      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json:" S_RED " %s" S_NOR) % e.what();
        return false;
      }
      return true;
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
    NewViewCertificate(string mmsg,int eepoch,vector<string> nnew_view_certificate={},
                       vector<string> ssig_of_nodes_to_be_added={},
                       vector<string> ccmds = {}
                       ):
      msg(mmsg),
      epoch(eepoch),
      new_view_certificate(nnew_view_certificate),
      sig_of_nodes_to_be_added(ssig_of_nodes_to_be_added),
      cmds(ccmds)
    {}

    string toString() const noexcept override {
      return IJsonizable::toJsonString();
    }

    bool fromString(string_view s) noexcept override{
      BOOST_LOG_TRIVIAL(debug) <<  "Forming NewViewCertificate from string.";
      return IJsonizable::fromJsonString(s);
    }

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
    std::atomic_flag closed = ATOMIC_FLAG_INIT;
    std::atomic_flag view_change_state = ATOMIC_FLAG_INIT;
    std::atomic_int epoch = 0;
    std::atomic_int patience = 0;

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

    LockedObject<unordered_map<string,
                               shared_ptr<std::set<string>>
                               >> to_be_confirmed_commands;
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
      this->all_endpoints.o = vector<string>(all_endpoints.begin(),
                                             all_endpoints.end());
      // 🦜 : We can't use set for all_endpoints. Because new nodes must be
      // added to the end.

      if (not all_endpoints.contains(this->net->listened_endpoint()))
        this->start_listening_as_newcomer();
      else{
        if (this->primary() == this->net->listened_endpoint())
          this->start_listening_as_primary();
        else
          this->start_listening_as_sub();

        // start the timer
        std::thread(std::bind(&RbftConsensus::start_faulty_timer, this)).detach();
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

    /**
     * @brief Endpoint said it's the primary, if the data contains the required
     things, then we follow it.

     Here we should also handle adding-new nodes.

     @param data: Serialized NewViewCertificate
    */
    void  handle_new_primary(string endpoint, string data){
      NewViewCertificate o;
      if (not o.fromString(data)){
        this->say(format("❌️Error parsing NewViewCertificate " S_RED "%s" S_NOR) % data);
        return ;
      }
      vector<string> newcomers = this->get_newcommers(o.sig_of_nodes_to_be_added);
      {
        std::unique_lock l(this->all_endpoints.lock);
        for (auto newcomer : newcomers){
          if (contains(this->all_endpoints.o,newcomer)){
            this->say("This primary has old newcomer, ignoring it");
            return;
          }
        }
      } // unlocks here

      if (this->epoch.load() > o.epoch){
        this->say(format("Ignoring older msg in epoch=" S_CYAN "%d" S_NOR) % o.epoch);
        return;
      }

      if (this->check_cert(o.new_view_certificate,o.epoch)){
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
        BOOST_ASSERT_MSG(this->sig->verify(s),
                         (format("Even the first signature = %s is wrong") % s).str().c_str()
                         );

        // 🦜 : Boost assert can only accept cstring.. so we kinda have to make an extra c_str()
        LaidDownMsg msg0;
        BOOST_ASSERT_MSG(msg0.fromString(this->sig->get_data(s)),
                         "Failed to form LaidDownMsg from string");

        valid_host.insert(string(this->sig->get_from(s)));

        if (not for_newcomer)
          BOOST_ASSERT_MSG(
                           msg0.state == this->get_state(),
                           (format("State mismatch: %s != %s") % msg0.state % this->get_state()).str().c_str()
                           );

        for (auto it = l.begin()+1;it < l.end();it++){

          BOOST_ASSERT(this->sig->verify(*it));
          LaidDownMsg msg;
          BOOST_ASSERT(msg.fromString(this->sig->get_data(*it)));
          valid_host.insert(string(this->sig->get_from(*it)));

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

        int x = this->N() - this->f();
        BOOST_ASSERT_MSG(valid_host.size() < x,
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

      // this->say(format("Recieved ok view-change-vertificate"))
      return true;
    } // check_cert

    void start_listening_as_primary(){
      this->say("\t 🌱: Start listening as primary");
      this->clear_and_listen_common_things();
      this->net->listen("/pleaseExecuteThis",
                        bind(&RbftConsensus::handle_execute_for_primary,this,_1,_2)
                        );
    }

    void start_listening_as_sub(){
      this->say("\t 🌱: Start listening as sub");
      this->clear_and_listen_common_things();
      this->net->listen("/pleaseExecuteThis",
                        bind(&RbftConsensus::handle_execute_for_sub,this,_1,_2)
                        );

      this->net->listen("/pleaseConfirmThis",
                        bind(&RbftConsensus::handle_confirm_for_sub,this,_1,_2)
                        );
    }

    /**
     * @brief The current primary
     */
    string primary(){
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
    static string cmds_to_state(vector<string> cmds){
      return boost::algorithm::join(cmds,":");
    }

    string get_state(){
      std::unique_lock l(this->command_history.lock);
      return RbftConsensus::cmds_to_state(this->command_history.o);
    }
    string get_signed_state(){
      return this->sig->sign(this->get_state());
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

        vector<string> all_endpoints;
        {
          std::unique_lock l(this->all_endpoints.lock);
          all_endpoints = this->all_endpoints.o;
} // unlocks

        this->say("\t\tBoardcasting cmd confirm");

        for (const string & sub : all_endpoints){
          if (sub != this->net->listened_endpoint()
              and
              sub != this->primary()
              ){
            this->net->send(sub,"/pleaseConfirmThis",data);
          }
        }

        this->say("\t\tAdding cmd to myself");
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

     *         🦜 : Is this the only method that touch `self.to_be_confirmed_commands` ?

     *         🐢 : Looks so.
     *      */
    void  add_to_to_be_confirmed_commands(string endpoint, string data){
      std::unique_lock l(this->to_be_confirmed_commands.lock);

      if (not this->to_be_confirmed_commands.o.contains(data)){
        this->say(format("Adding " S_MAGENTA "%s " S_NOR " from  " S_MAGENTA "%s" S_NOR)
                  % data % endpoint);
        this->to_be_confirmed_commands.o[data] = make_shared<std::set<string>>();
      }

      // 🦜 : reference the one (this is not something that we can do in OOthon)
      shared_ptr<std::set<string>> s = this->to_be_confirmed_commands.o[data];
      s->insert(endpoint);

      /*

        🦜 : How many we should collect?

        🐢 : I think it should be N - 1 - f, 1 corresponds to the
        primary. For example, when N = 2, there should be one confirm
        (the one N1 put it after recieved the command). When N = 3,
        there should be two, (needs an extra one from N2).

       */

      int x = this->N() - 1 - this->f();
      if (s->size() >= x){
        this->say(format("⚙️ command " S_CYAN "%s " S_NOR
                         " confirmed by " S_CYAN "%d " S_NOR " node%s, executing it and clear")
                  % data % s->size() % pluralizeOn(s->size())
                  );

        this->exe->execute(data);
        {
          std::unique_lock l(this->command_history.lock);
          this->command_history.o.push_back(data);
        }

        // remove the entry at all
        this->to_be_confirmed_commands.o.erase(data);
      }else{
        this->say(format("⚙️ command " S_CYAN "%s " S_NOR
                         " confirmed by " S_CYAN "%d " S_NOR " node%s, not yet.")
                  % data % s->size() % pluralizeOn(s->size())
                  );
      }
}

    void  handle_confirm_for_sub(string endpoint, string data){
      if (this->view_change_state.test()){
        BOOST_LOG_TRIVIAL(error) << format("❌️ Dear %s, we are currently selecting new primary "
                                           "for epoch %d. Please try again later.") % endpoint % this->epoch.load();
        return;
      }
      this->add_to_to_be_confirmed_commands(endpoint, data);
    }

    int N(){
      std::unique_lock l(this->all_endpoints.lock);
      return this->all_endpoints.o.size();
    }
    /**
     * @brief Get the number of random nodes the system can tolerate
     */
    int f(){
      std::unique_lock l(this->all_endpoints.lock);
      int N = this->all_endpoints.o.size();
      int ff = (N - 1) / 3;     // trancated

      // BOOST_CHECK_EQUAL(1/3,0);
      // BOOST_CHECK_EQUAL(2/3,0);
      // BOOST_CHECK_EQUAL(3/3,1);
      // BOOST_CHECK_EQUAL(4/3,1);

      // 🦜 : C by default, does truncated division.
      return ff;
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
          if (this->closed.test()){
            this->say("\t 👋 Timer closed");
            return;
          }
          this->patience--;
          this->say(format(" patience >> " S_MAGENTA "%d " S_NOR ", 🐢: Pr : " S_CYAN "%s" S_NOR)
                    % this->patience.load() % this->primary());
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

      string data = this->sig->sign(msg.toString());

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
        BOOST_VERIFY(this->sig->verify(data));
        LaidDownMsg msg;
        BOOST_VERIFY(msg.fromString(this->sig->get_data(data)));

        if (msg.epoch < this->epoch.load()){
          this->say(format("🚮️ Ignoring lower epoch laid-down msg: epoch=%d")
                    % msg.epoch);
          return ;
        }
        /*
          🦜 : I feel like we should only use BOOST_VERIFY if it's something
          important.... Bucause they will be painted in red.
        */

        const string * next_primary;
        {
          std::unique_lock l(this->all_endpoints.lock);
          next_primary = &(
                           this->all_endpoints.o[
                                                 msg.epoch %
                                                 this->all_endpoints.o.size()
                                                 ]
                           );
        } // unlocked here
        if (*next_primary != this->net->listened_endpoint()){
          this->say(format("🚮️ This view-change is for " S_CYAN "%d " S_NOR ",non of my bussinesses.")
                    % *next_primary);
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
     *         data = this->sig->sign(o.fromString())
     *
     * @param o the newly added LaidDownMsg
     */
    void remember_this_laiddown_and_be_primary_maybe(const LaidDownMsg & o, const string data){
      // string state = o.state;
      // int epoch = o.epoch;
      shared_ptr<vector<string>> to_be_added_list;

      {
        std::unique_lock l(this->laid_down_history.lock);
        // 🦜 : 1. Does this epoch already has something ? If not, create a new dict{}
        if (not this->laid_down_history.o.contains(o.epoch)){
  
          this->say(format("\tAdding new record in laid_down_history for epoch " S_GREEN "%d " S_NOR)
                    % o.epoch);
          this->laid_down_history.o.insert({
              epoch,
              make_shared<
              unordered_map<string,shared_ptr<vector<string>>>
              //            state : votes for state
              >()});
        }

        // 🦜 : 2. Does this epoch already got state like this ? If not, create a new list[]
        if (not this->laid_down_history.o.at(o.epoch)->contains(o.state)){
          this->say(format("t\tAdding new record in laid_down_history for " S_GREEN "epoch=%d,state=%s" S_NOR)
                    % o.epoch % o.state);
          this->laid_down_history.o.at(o.epoch)->insert({
              o.state,
              make_shared<vector<string>>()
            });

          // 🦜 Now take the list
          to_be_added_list = this->laid_down_history.o.at(o.epoch)->at(o.state);
        }
      } // unlocks here

      // 🦜 : This is the list that data (signed msg) will be added in.
      if (contains(*to_be_added_list, data)){
        this->say(format("🚮️ Ignoring duplicated msg:" S_CYAN "%s" S_NOR)% data);
        return;
      }
      to_be_added_list->push_back(data);

      /*

        🦜 : How many laiddown msg does one need to collect in order to be the new primary?

        🐢 : Every correct node will append to it even the next primary itself.
        so it's N - f.

       */

      int x = this->N() - this->f();
      if (to_be_added_list->size() >= x){
        this->say(format("Collected enough " S_GREEN "%d " S_NOR " >= " S_CYAN "%d" S_NOR " LaidDownMsg%s")
                  % to_be_added_list->size() % x % pluralizeOn(to_be_added_list->size())
                  );
        this->try_to_be_primary(o.epoch,o.state,to_be_added_list);
      }else{
        this->say(format("Collected enough " S_MAGENTA "%d " S_NOR " <= " S_CYAN "%d" S_NOR " LaidDownMsg%s"
                         S_MAGENTA "not yet the time" S_NOR)
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
    vector<string> get_newcommers(vector<string> sigs){
      std::set<string> o;
      for (const string & sig : sigs ){
        if (not this->sig->verify(sig))
          continue;

        // else
        string_view n = this->sig->get_from(sig);
        if (o.insert(string(n)).second) // 🦜 added
          this->say(format(" Verified node " S_GREEN "%s" S_NOR) % n);
      }
      vector<string> r(o.begin(),o.end()); // copy
      return r;
    }

    void try_to_be_primary(int e, string state, shared_ptr<vector<string>> my_list){}
    optional<string> handle_execute_for_primary(string endpoint, string data) override{
      return "";
    }
    bool is_primary(){
      return false;
    }

    template<typename T>
    void say(T s){
      BOOST_LOG_TRIVIAL(debug) << s;
    }

    void boardcast_to_others(string_view target, string_view data){}
    void  handle_add_new_node_no_boardcast(string endpoint, string data){}

    void start_listening_as_newcomer(){}
    void  handle_new_primary_for_newcomer(string endpoint, string data){}
    int epoch_considering_newcomers(int e, int num_newcomers){
      return 0;
    }
    void close(){}
  };

} // namespace pure
