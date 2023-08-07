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

// colors
#define S_RED     "\x1b[31m"
#define S_GREEN   "\x1b[32m"
#define S_YELLOW  "\x1b[33m"
#define S_BLUE    "\x1b[34m"
#define S_MAGENTA "\x1b[35m"
#define S_CYAN    "\x1b[36m"
#define S_NOR "\x1b[0m"

namespace pure{
    using boost::format;
  using std::move;
  using std::vector;


  using std::unique_ptr;

  using std::unordered_map;
  namespace json = boost::json;
  using namespace std::placeholders;  // for _1, _2, _3...
  using std::bind;
  using std::shared_ptr;

  class ListenToOneConsensus:
    public virtual ICnsssPrimaryBased,
    public std::enable_shared_from_this<ListenToOneConsensus> // define shared_from_this()
  {
  private:
    /*
     * 🐢 : Make the ctor private, only use factory function.
     *
     * 🦜 : Why bother using the shared_ptr magic? This is same as "boxing the
     * object" write?
     *
     * 🐢 : This is becasue in pure-testListenToOneConsensus.cpp, we initialzed
     * multiple ListenToOneConsensus and store them in a container. But problem
     * is that the event handler is registered during the construction of the
     * cnsss object, with respect to the `this->` pointer at that moment. But
     * the address of `this` is changed after the object (on the stack) is
     * copied into the container. As a result, the event handler cannot be
     * called.
     *
     * The solution is to send the cnsss object to heap, and only manage the
     * shared_ptr (C#-style). This ensures that the location of `this` pointer
     * will not change during the lifetime of the instance (construction, copy
     * to container,destruction).
     */
    ListenToOneConsensus(IEndpointBasedNetworkable *  const n,
                         IForConsensusExecutable *  const e,
                         const string & nodeToConnect = ""
                         ): exe(e), net(n),
                            primary_is_me(nodeToConnect == ""),
                            primary(nodeToConnect){
      if (not primary_is_me){
        this->ask_primary_for_entry();
        this->start_listening_as_sub();
        this->say("started listening as sub 🐸");
      }else{
        this->say("started as primary🐸");
        this->start_listening_as_primary();
      }
    };
  public:
    // use shared_from_this()

    // No public constructor, only a factory function, so there's no way to have
    // shared_from_this() return nullptr.
    [[nodiscard]] static shared_ptr<ListenToOneConsensus> create(IEndpointBasedNetworkable *  const n,
                                                                 IForConsensusExecutable *  const e,
                                                                 const string & nodeToConnect = ""){
      // Not using std::make_shared<B> because the c'tor is private.
      return shared_ptr<ListenToOneConsensus>(new ListenToOneConsensus(n,e,nodeToConnect));
    }
    IForConsensusExecutable * const exe;
    IEndpointBasedNetworkable * const net;

    // 🦜 : In listenToOne, primary is fixed.
    const bool primary_is_me;
    const string primary;

    /**
     * @brief The method required by interface. Check primary.
     */
    bool is_primary() const noexcept override{
      return this->primary_is_me;
    }

    void say(string s){
      BOOST_LOG_TRIVIAL(debug) << format("[%s]: %s")
        % this->net->listened_endpoint() % s;
    };


    vector<string> known_subs;
    vector<string> command_history;

    void start_listening_as_primary(){
      this->net->listen("/pleaseAddMe",
                        bind(&ListenToOneConsensus::handle_add_new_node,this,_1,_2));

      this->net->listen("/pleaseExecuteThis",
                        bind(&ListenToOneConsensus::handle_execute_for_primary,this,_1,_2));
    };

    optional<string> handle_add_new_node(string endpoint, string data){
      // BOOST_LOG_TRIVIAL(trace) << format("🐸 Adding new node");
      this->known_subs.push_back(string(endpoint));
      json::object o;
      // BOOST_LOG_TRIVIAL(trace) << format("🐸 Adding msg");
      string msg = (format("Dear %s, you're in. %s") % endpoint % this->net->listened_endpoint()).str();
      // BOOST_LOG_TRIVIAL(trace) << format("msg = %s") % msg;
      o.emplace("msg",msg);

      // BOOST_LOG_TRIVIAL(trace) << format("🐸 Adding cmds");
      o.emplace("command_history",
                json::value_from(this->command_history));
      // boost::json knows about vector
      return json::serialize(o);                // produces "[1,2,3]"
    };

    optional<string> handle_execute_for_primary(string endpoint,
                                                string data) override{
      this->command_history.push_back(string(data));
      this->exe->execute(data); // This may modify the `data`

      vector<string> & l = this->known_subs; // reference
      for (auto it = l.begin(); it != l.end();){
        optional<string> r = this->net->send(*it,"/pleaseExecuteThis",data);
        if (not r){
          this->say((format("❌️ Node-%s is down, kick it off the group") % *it).str());
          l.erase(it);
        }else{
          it++;
        }
      }

      using boost::algorithm::join;
      string s_members = join(this->known_subs,",");
      return (format("Dear Client \n"
                     "\tyour request has been carried out by our group.\n"
                     "\tMembers: %s\n"
                     "\t\tSincerely\n"
                     "\t\t%s,The primary\n"
                     ) % s_members % this->net->listened_endpoint()
              ).str();
    };

    void ask_primary_for_entry(){
      string msg = (format("Hi primary %s\n"
                           "\tplease add me in the group\n"
                           "\tRegards %s"
                           ) % this->primary
                    % this->net->listened_endpoint()).str();
      optional<string> r = this->net->send(this->primary,
                                           "/pleaseAddMe",
                                           msg);

      if (not r)
        throw std::runtime_error("Failed to join the group");

      json::error_code ec;
      json::value jv = json::parse(r.value(), ec);
      if (ec)
        throw std::runtime_error("Wrong answer from the primary.");

      vector<string> cmds = json::value_to<vector<string>>(jv.as_object().at("command_history"));

      for (string & cmd : cmds){
        this->exe->execute(cmd);
      }
    };

    void start_listening_as_sub(){
      this->net->listen("/pleaseExecuteThis",
                        bind(&ListenToOneConsensus::handle_execute_for_sub,this,_1,_2)
                        );
    };

    optional<string> handle_execute_for_sub(string endpoint, string data) override{
      // BOOST_LOG_TRIVIAL(trace) << format("🐸 handling exe for sub, data = %s, endpoint = %s")
      //   % data % endpoint;

      // this->say((format("Handling execution")).str());
      if (endpoint == this->primary){
        this->say(
                  // (format("Execution from boss")).str()
                  "\t" S_MAGENTA "Execution " S_NOR " from boss 🌕"
                  );

        this->exe->execute(data);
        return (format("Dear boss %s\n"
                       "\tMission[%s] is accomplished\n"
                       "\tRegards\t%s"
                       ) % this->primary % data % this->net->listened_endpoint()).str();
      }

      this->say(
                // (format("Execution from others")).str()
                "\t" S_MAGENTA "Execution " S_NOR " from others 🌘"
                );
      // forward
      auto r = this->net->send(this->primary,
                               "/pleaseExecuteThis", data);
      if (not r)
        BOOST_THROW_EXCEPTION(std::runtime_error(format("Failed to forward msg to primary").str()));
      return r;
    };
  };
}


namespace mock{
  using namespace pure;
  class Executable: public virtual IForConsensusExecutable{
  public:
    string id;
    Executable(string i):id(i){};
    string execute(string & cmd) noexcept override{
      BOOST_LOG_TRIVIAL(debug) << format(S_RED "🦜 [%s] Exec: %s" S_NOR)
        % this->id % cmd;
      return "OK";
    };
  };

  unordered_map<string,
                function<optional<string>(string,string)>
                > network_hub;
  std::mutex lock_for_network_hub;

  class EndpointNetworkNode: public virtual IEndpointBasedNetworkable{
  public:
    string endpoint;
    EndpointNetworkNode(string e): endpoint(e){};

    string listened_endpoint() noexcept override{
      return this->endpoint;
    };

    void listen(string target,
                function<optional<string>(string,string)> handler
                ) noexcept override{
      string k = (format("%s-%s") % this->endpoint % target).str();
      BOOST_LOG_TRIVIAL(debug) << format("Adding handler: %s") % k;
      network_hub[k] = handler;
    };

    void clear() noexcept override{
      std::unique_lock l(lock_for_network_hub);

      // erase all odd numbers from c
      for (auto it = network_hub.begin(); it != network_hub.end();){
        if (it->first.starts_with(this->endpoint + "-")){
          BOOST_LOG_TRIVIAL(debug) << format("Removing handler: %s") % it->first;
          it = network_hub.erase(it);
        }else{
          ++it;
        }
      }
    }; // unlock here


    optional<string> send(string  endpoint,
                          string target,
                          string data) noexcept override{
      string k = (format("%s-%s") % endpoint % target).str();
      BOOST_LOG_TRIVIAL(debug) << format(S_CYAN " Calling handler: %s " S_NOR
                                         "with data\n"
                                         S_CYAN "%s" S_NOR
                                         ) % k % data;
      // write send()
      optional<string> r;
      if (network_hub.contains(k)){
        BOOST_LOG_TRIVIAL(debug) << format("Handler found");
        r = network_hub.at(k)(this->endpoint,data);
      }else{
        BOOST_LOG_TRIVIAL(debug) << format("Handler %s not found" ) % k;
        r = {};
      }
      if (r)
        BOOST_LOG_TRIVIAL(debug) << format("Got result: " S_GREEN "%s" S_NOR) % r.value();
      else
        BOOST_LOG_TRIVIAL(debug) << format("Got result: " S_GREEN "None" S_NOR);

      return r;
    };
  };

}
