/**
 * @file pure-raft.hpp
 * @author Jianer Cong
 * @brief The raft consensus
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
// #include <boost/random/mersenne_twister.hpp>
// #include <boost/random/uniform_int_distribution.hpp>

namespace pure {
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

  class RaftConsensusBase :
    // public virtual ICnsssPrimaryBased, // 🦜 : Nope, to be implemented by the subclass
    public std::enable_shared_from_this<RaftConsensusBase> {
  protected:

    IAsyncEndpointBasedNetworkable * const net;
    IForConsensusExecutable * const exe;
    vector<string> const others;
    std::atomic_flag done = ATOMIC_FLAG_INIT;

    std::atomic_int patience = 0;
    std::atomic_int term = 0;
    std::atomic_int voted_term = 0;
    std::atomic_int my_votes = 0;
    string primary = "";
    std::thread timer;

    RaftConsensusBase(IAsyncEndpointBasedNetworkable * const n,
                      IForConsensusExecutable * const e,
                      vector<string> o):
      net(n), exe(e), others(o) {
      BOOST_LOG_TRIVIAL(debug) <<  "🌱 " + this->net->listened_endpoint() + " started";
      /*
        self.net.listen('/pleaseVoteMe',self.handle_pleaseVoteMe)
        self.net.listen('/voteForYou',self.handle_voteForYou)
        self.net.listen('/heartbeat',self.handle_heartbeat)
        self.net.listen('/iAmThePrimary',self.handle_iAmThePrimary)
       */
      this->net->listen("/pleaseVoteMe", bind(&RaftConsensusBase::handle_pleaseVoteMe, this, _1,_2));
      this->net->listen("/voteForYou", bind(&RaftConsensusBase::handle_voteForYou, this, _1,_2));
      this->net->listen("/heartbeat", bind(&RaftConsensusBase::handle_heartbeat, this, _1,_2));
      this->net->listen("/iAmThePrimary", bind(&RaftConsensusBase::handle_iAmThePrimary, this, _1,_2));
      this->start();
    }
  public:
    [[nodiscard]] static shared_ptr<RaftConsensusBase> create(
      IAsyncEndpointBasedNetworkable * const n,
      IForConsensusExecutable * const e,
      vector<string> o){
      // return make_shared<RaftConsensusBase>(new RaftConsensusBase(n,e,o));
      // Not using std::make_shared<B> because the c'tor is private.
      return shared_ptr<RaftConsensusBase>(new RaftConsensusBase(n,e,o));
    }

    void start() {
      // this->start_internal_timer();
      this->timer = std::thread([this](){this->start_internal_timer();});
    }

    void stop(){
      this->net->clear();
      this->done.test_and_set();
      // wait for the timer to finish
      // BOOST_LOG_TRIVIAL(debug) <<  "Before joining the timer";
      this->timer.join();
      // BOOST_LOG_TRIVIAL(debug) <<  "👋 " + this->net->listened_endpoint() + "stopped";
    }

    void start_internal_timer() {
      this->comfort();
      while (not this->done.test()){
        if (this->primary != this->net->listened_endpoint()) {
          // tick down the patience
          if (this->patience.load() == 0){
            this->complain();
            this->comfort();
          }else{
            this->patience--;
          }
          this->say((format("patience = %d") % this->patience.load()
                     ).str());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      }
      this->say("👋 timer bye");
    }

    void complain(){
      // this->say("📗️ patience is 0");
      // 1. start a new term and vote for myself
      this->term++;
      this->my_votes = 1;
      this->voted_term = this->term.load();

      // 2. boardcast '/pleaseVoteMe'
      for (auto other = this->others.cbegin(); other != this->others.cend(); other++){
        this->net->send(*other, "/pleaseVoteMe", std::to_string(this->term));
      }
    }

    void handle_pleaseVoteMe(string from, string msg){
      this->say((format("🗳️ Got vote request from %s, my term: %d, voted term: %d")
                 %
                 ICnsssPrimaryBased::make_endpoint_human_readable(from)
                  % this->term.load() % this->voted_term.load()).str());
      int term = std::stoi(msg);
      if (term > this->term.load()
          and term > this->voted_term.load()){
        // vote for him/her
        this->voted_term.store(term);
        this->net->send(from, "/voteForYou", std::to_string(term));
      }
      // else do nothing
    }

    void handle_voteForYou(string from, string msg){
      int term = std::stoi(msg);
      this->say((format("🗳️ Got vote from %s, my term: %d, now I have %d votes")
                 % ICnsssPrimaryBased::make_endpoint_human_readable(from)
                 % this->term.load() % this->my_votes.load()).str());

      if (term == this->term.load()){
        this->my_votes++;
        if (this->my_votes.load() > this->others.size() / 2){
          this->say((format("🎉 I am the primary now, term = %d") % this->term.load()).str());
          // boardcast '/iAmThePrimary'
          for (auto other = this->others.cbegin(); other != this->others.cend(); other++){
            this->net->send(*other, "/iAmThePrimary", std::to_string(this->term.load()));
          }
          this->primary = this->net->listened_endpoint();
          this->start_being_primary();
        }
      }
      // else do nothing
    }

    void start_being_primary(){
      this->my_votes = 0;
      while (not this->done.test() and (this->primary == this->net->listened_endpoint())){
        // heartbeat
        for (auto other = this->others.cbegin(); other != this->others.cend(); other++){
          this->net->send(*other, "/heartbeat", "");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        // 🦜 : It's important to put this line after the for-loop, cuz there's
        // a chance that `this` is gone during the sleep.
      }
    }

    void handle_heartbeat(string from, string msg){
      // only respond to the primary
      if (this->primary == from){
        this->comfort(from);
      }
    }

    void handle_iAmThePrimary(string from, string msg){
      int term = std::stoi(msg);
      if (term >= this->term.load()){
        this->term.store(term);
        this->primary = from;
        this->say((format("🎉 Welcome the new primary: %s, term = %d")
                   %
                   ICnsssPrimaryBased::make_endpoint_human_readable(from)
                   % term).str());
        this->comfort();
      }
      // else do nothing
    }

    void comfort(string by = "myself"){
      // set to randome 5:1:10
      this->patience = 5 + (std::rand() % 6);
      // this->say((format("patience set to %d by %s") % this->patience.load() % by).str());
    }

    virtual string prompt(){
      return (
              format(S_CYAN "[%s]" S_NOR "[Pm=%s]" S_GREEN "[t=%d]: " S_NOR)
              %
              ICnsssPrimaryBased::make_endpoint_human_readable(this->net->listened_endpoint())
              %
              ICnsssPrimaryBased::make_endpoint_human_readable(this->primary)
              % this->term.load()
              ).str();
    }

    void say(string msg){
      BOOST_LOG_TRIVIAL(debug) << this->prompt() << msg;
    }
  }; // class RaftConsensusBase

class RaftConsensus :
  public RaftConsensusBase,
  public virtual ICnsssPrimaryBased,
  public std::enable_shared_from_this<RaftConsensus> {
private:
  RaftConsensus(IAsyncEndpointBasedNetworkable * const n,
                IForConsensusExecutable * const e,
                vector<string> o):
    RaftConsensusBase(n,e,o) {
    this->net->listen("/pleaseExecute", bind(&RaftConsensus::handle_pleaseExecute, this, _1,_2));
    // ⚠️ Caveat, in RaftConsensusBase(...), the overriden methods won't be called...
  }
public:
  vector<string> cmds;
  [[nodiscard]] static shared_ptr<RaftConsensus> create(IAsyncEndpointBasedNetworkable * const n,
                                                            IForConsensusExecutable * const e,
                                                            vector<string> o){
    // Not using std::make_shared<B> because the c'tor is private.
    return shared_ptr<RaftConsensus>(new RaftConsensus(n,e,o));
  }

  bool is_primary() const noexcept override {
    return this->primary == this->net->listened_endpoint();
  }

  optional<string> handle_execute_for_primary(string endpoint,
                                              string data) override {
    // 1. execute the command
    this->exe->execute(data);
    this->cmds.push_back(data);

      // 2. boardcast the command
    for (auto other = this->others.cbegin(); other != this->others.cend(); other++){
      this->net->send(*other, "/pleaseExecute", data);
    }
    return "Done";
  }
  optional<string> handle_execute_for_sub(string endpoint,
                                          string data){
    // 1. if endpoint is not the primary, forward the command to the primary
    if (endpoint != this->primary){
      this->net->send(this->primary, "/pleaseExecute", data);
      return "forwarded to primary";
    }else{
      // 2. execute the command
      this->exe->execute(data);
      this->cmds.push_back(data);
      return "Done";
    }
  }

  void handle_pleaseExecute(string from, string msg){
    if (this->is_primary()){
      this->handle_execute_for_primary(from, msg);
    }else{
      this->handle_execute_for_sub(from, msg);
    }
  }
  string prompt() override {
    return (
            format(S_CYAN "[%s]" S_NOR "[Pm=%s]" S_GREEN "[t=%d][%s]: " S_NOR)
            % ICnsssPrimaryBased::make_endpoint_human_readable(this->net->listened_endpoint())
            % ICnsssPrimaryBased::make_endpoint_human_readable(this->primary)
            % this->term.load()
            % boost::algorithm::join(this->cmds,":")
            ).str();
  }

};                              // class RaftConsensus
}   // namespace pure
