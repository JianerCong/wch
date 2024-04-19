#define BFT_STATE_DEBUG
#include "pure-raft.hpp"
#include <iostream>

using std::unordered_map;
using std::unique_ptr;
using std::make_unique;
using std::cout;
using std::cin;

#include <vector>
using std::vector;
using namespace pure;

// Called when BOOST_ASSERT failed
void boost::assertion_failed(char const * expr, char const * function, char const * file, long line){
  std::string s = (format("❌️\n\tassertion %s has failed. (func=%s,file=%s,line=%ld)")
                   % expr % function % file % line).str();
  BOOST_THROW_EXCEPTION(my_assertion_error(s));
} // 🦜 : It seems like boost uniform_int_distribution requires us to define ^ 

template<typename T>
struct RaftAndColleague {
  mock::Executable * e;
  mock::AsyncEndpointNetworkNode * n;

  shared_ptr<T> raft;
  string my_endpoint;

  RaftAndColleague(int my_id, vector<string> endpoints /*pass by value*/){
    e = new mock::Executable(endpoints[my_id]);
    n = new mock::AsyncEndpointNetworkNode(endpoints[my_id]);

    endpoints.erase(endpoints.begin() + my_id); // the others
    raft = T::create(
                                     dynamic_cast<IAsyncEndpointBasedNetworkable *>(this->n),
                                     dynamic_cast<IForConsensusExecutable*>(this->e),
                                     endpoints);
  }

  ~RaftAndColleague(){
    BOOST_LOG_TRIVIAL(debug) <<  "👋 Closing RaftAndColleague: " S_MAGENTA << my_endpoint;
    this->raft->stop();
    // this->raft.reset();         // release the shared_ptr
    BOOST_LOG_TRIVIAL(debug) <<  "👋 Closing exe";
    delete this->e;
    BOOST_LOG_TRIVIAL(debug) <<  "👋 Closing net";
    delete this->n;
    BOOST_LOG_TRIVIAL(debug) <<  "Finished net";
  }
};

struct Stage {
  // using T = RaftAndColleague<RaftConsensusBase>;
  using T = RaftAndColleague<RaftConsensus>;

  unordered_map<string, shared_ptr<T>> nodes;
  vector<string> endpoints;
  /**
   * @brief Initialize a stage with N nodes.
   *
   * 🐢 : So I think simply creating N RaftAndColleague objects should be enough...
   */
  Stage(int n=2){
    // 1. prepare the endpoints
    for (int i = 0; i < n; i++){
      string endpoint = "N" + std::to_string(i);
      endpoints.push_back(endpoint);
    }

    // 2. create the RaftAndColleague objects
    for (int i = 0; i < n; i++){
      string my_endpoint = endpoints[i];
      nodes[my_endpoint] = make_shared<T>(i, endpoints);// Called when BOOST_ASSERT failed

    }
  }

  void kick(int i){
    // 0.
    string ep = "N" + std::to_string(i);
    BOOST_LOG_TRIVIAL(debug) <<  "👟 Kicking " S_MAGENTA << ep << S_NOR;
    if (not nodes.contains(ep)){
      BOOST_LOG_TRIVIAL(debug) <<  "🚫 No such node" S_MAGENTA << ep;
      return;
    }

    // 1. stop the node
    this->nodes.erase(ep);
    // nodes[ep].stop();
  }
};

void start_cluster(int n){
  Stage stage(n);
  unique_ptr<mock::AsyncEndpointNetworkNode> nClient =
    make_unique<mock::AsyncEndpointNetworkNode>("ClientAAA");

  while (true){
    string reply;
    cin >> reply;
    if (reply == "q" or reply == "quit"){
      break;
    }
    if (reply == "k" or reply == "kick"){
      int i;
      cin >> i;
      if (i < 0){
        break;
      }
      stage.kick(i);
      BOOST_LOG_TRIVIAL(debug) <<  "👟 Kicked N" S_MAGENTA << i << S_NOR;
      continue;
    }
    // read command
    string cmd;
    cin >> cmd;
    BOOST_LOG_TRIVIAL(debug) << "🦜 : Sending command " + cmd + " to " + reply;
    nClient->send(reply, "/pleaseExecute", cmd);
  }

  // remove all
  BOOST_LOG_TRIVIAL(debug) <<  "👋 Closing all nodes";
  stage.nodes.clear();
}


#define ARGV_SHIFT()  { i_argc--; i_argv++; }
int main(int i_argc, const char *i_argv[]){
  ARGV_SHIFT(); // Skip executable name

  int n = 2;
  if (i_argc > 0){
    n = lexical_cast<int>(i_argv[0]);
  }

  start_cluster(n);
  return 0;
}

//  N1 a
//

// Local Variables:
// eval: (setq flycheck-gcc-include-path (list ".." "../.."))
// End:
