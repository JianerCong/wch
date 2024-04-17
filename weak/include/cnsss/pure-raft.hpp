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
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

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
    public virtual ICnsssPrimaryBased,
    public std::enable_shared_from_this<RaftConsensusBase> {
  private:

    IAsyncEndpointBasedNetworkable * const net;
    IForConsensusExecutable * const exe;
    vector<string> const others;
    std::atomic_flag done = ATOMIC_FLAG_INIT;

    std::atomic_int patience = 0;
    std::atomic_int term = 0;
    std::atomic_int voted_term = 0;
    std::atomic_int my_votes = 0;
    string primary = "";

    RaftConsensusBase(IAsyncEndpointBasedNetworkable * const n,
                      IForConsensusExecutable * const e,
                      vector<string> o):
      net(n), exe(e), others(o) {
      BOOST_LOG_TRIVIAL(debug) <<  "🌱" + this->net->listened_endpoint() + "started";
      this->start();

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
    }
  public:
    [[nodiscard]] static shared_ptr<RaftConsensusBase> create(
      IAsyncEndpointBasedNetworkable * const n,
      IForConsensusExecutable * const e,
      vector<string> o){
      return make_shared<RaftConsensusBase>(n, e, o);
    }

    void start() {
      this->start_internal_timer();
    }

    void stop(){
      this->done.test_and_set();
      this->net->clear();
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
      this->say("📗️ patience is 0");
      // 1. start a new term and vote for myself
      this->term++;
      this->my_votes = 1;
      this->voted_term = this->term;

      // 2. boardcast '/pleaseVoteMe'
      for (auto other : this->others.cbegin(), this->others.cend()){
        this->net->send(*other, "/pleaseVoteMe", std::to_string(this->term));
      }
    }

    void handle_pleaseVoteMe(string from, string msg){
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
                 % from % this->term.load() % this->my_votes.load()).str());

      if (term == this->term.load()){
        this->my_votes++;
        if (this->my_votes.load() > this->others.size() / 2){
          this->say((format("🎉 I am the primary now, term = %d") % this->term.load()).str());
          // boardcast '/iAmThePrimary'
          for (auto other : this->others.cbegin(), this->others.cend()){
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
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        // heartbeat
        for (auto other : this->others.cbegin(), this->others.cend()){
          this->net->send(*other, "/heartbeat", "");
        }
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
                   % from % term).str());
        this->comfort();
      }
      // else do nothing
    }

    boost::random::mt19937 gen;
    void comfort(string by = "myself"){
      // set to randome 5:1:10
      boost::random::uniform_int_distribution<> dist(5,10);
      this->patience = dist(gen);
      this->say((format("patience set to %d by %s") % this->patience.load() % by).str());
    }

    string prompt(){
      return (
              format(S_CYAN "[%s]" S_NOR "[Pm=%s]" S_GREEN "[t=%d]: " S_NOR)
              % this->net->listened_endpoint()
              % this->primary
              % this->term.load()
              ).str();
    }

    void say(string msg){
      BOOST_LOG_TRIVIAL(debug) << this->prompt() << msg;
    }

  }; // class RaftConsensusBase
}   // namespace pure
