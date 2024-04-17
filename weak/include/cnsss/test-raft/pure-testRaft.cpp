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

struct RaftAndColleague {
  mock::Executable * e;
  mock::AsyncEndpointNetworkNode * n;

  shared_ptr<RaftConsensusBase> raft;
  string my_endpoint;
  RaftAndColleague(int my_id, vector<string> endpoints /*pass by value*/){
    e = new mock::Executable(endpoints[my_id]);
    n = new mock::AsyncEndpointNetworkNode(endpoints[my_id]);

    endpoints.erase(endpoints.begin() + my_id); // the others
    raft = RaftConsensusBase::create(
                                     dynamic_cast<IAsyncEndpointBasedNetworkable *>(this->n),
                                     dynamic_cast<IForConsensusExecutable*>(this->e),
                                     endpoints);
  }
  ~RaftAndColleague(){
    BOOST_LOG_TRIVIAL(debug) <<  "👋 Closing RaftAndColleague: " S_MAGENTA << my_endpoint;
    this->raft->stop();
    delete this->e;
    delete this->n;
  }
};

struct Stage {
  unordered_map<string, shared_ptr<RaftAndColleague>> nodes;
  vector<string> endpoints;
  int N;
  /**
   * @brief Initialize a stage with N nodes.
   *
   * 🐢 : So I think simply creating N RaftAndColleague objects should be enough...
   */
  Stage(int n=2): N(n){
    // 1. prepare the endpoints
    for (int i = 0; i < N; i++){
      string endpoint = "N" + std::to_string(i);
      endpoints.push_back(endpoint);
    }

    // 2. create the RaftAndColleague objects
    for (int i = 0; i < N; i++){
      string my_endpoint = endpoints[i];
      nodes[my_endpoint] = make_shared<RaftAndColleague>(i, endpoints);
    }
  }

  void kick(int i){
    // 0.
    string ep = "N" + std::to_string(i);
    BOOST_LOG_TRIVIAL(debug) <<  "👟 Kicking " S_MAGENTA << ep;
    if (not nodes.contains(ep)){
      BOOST_LOG_TRIVIAL(debug) <<  "🚫 No such node" S_MAGENTA << ep;
      return;
    }

    // 1. stop the node
    this->nodes.erase(ep);
  }
};

void start_cluster(int n){
  Stage stage(n);
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
    }
  }
  // remove all
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
