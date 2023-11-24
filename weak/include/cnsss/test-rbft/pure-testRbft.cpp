#define BFT_STATE_DEBUG
#include "pure-rbft.hpp"
#include <iostream>

using std::unordered_map;
using std::unique_ptr;
using std::make_unique;
using std::cout;
using std::cin;

#include <vector>
using std::vector;


// #include <map>
// using std::map;

using namespace pure;

struct BftAndColleague{
  // unique_ptr<mock::Executable> e;
  // unique_ptr<mock::Signer> s;
  // unique_ptr<mock::AsyncEndpointNetworkNode> n;

  /*

    🦜 : We kinda need to manage them manually because cnsss must be closed
    before these. (especially the network... cnsss will call net->clear() in its
    dtor. and you don't want segmentation fault)
   */

  mock::Executable * e;
  NaiveMsgMgr * s;
  mock::AsyncEndpointNetworkNode * n;

  shared_ptr<RbftConsensus> nd;
  string id;
  BftAndColleague(string  s,
                  vector<string> all_endpoints
                  /*pass by value (important)*/
                  ): id(s){
    this->e = new mock::Executable(s);
    this->s = new NaiveMsgMgr(s);
    this->n = new mock::AsyncEndpointNetworkNode(s);


    this->nd = RbftConsensus::create(
                                     dynamic_cast<IAsyncEndpointBasedNetworkable *>(this->n),
                                     dynamic_cast<IForConsensusExecutable*>(this->e),
                                     dynamic_cast<IMsgManageable *>(this->s),
                                     all_endpoints
                                     );
  }

  ~BftAndColleague(){
    BOOST_LOG_TRIVIAL(debug) <<  "Closing BftAndColleague: " S_MAGENTA << this->id << S_NOR;
    this->nd.reset();           // release the shared_ptr and close the BFT (calls the d'tor)

    // Now they can be safely freed
    delete this->e;
    delete this->s;
    delete this->n;
}
};

struct NodeFactory{
  unordered_map<string,shared_ptr<BftAndColleague>> nodes;
  vector<string> all_endpoints;
  int N;
  NodeFactory(int n = 2): N(n){
    BOOST_LOG_TRIVIAL(debug) << "🌱 \tInitial cluster size: " S_CYAN << n << S_NOR ;
    for (int i = 0;i<n;i++){
      this->all_endpoints.push_back(
                                 (format("N%d") % i).str()
                                 );
    }

    for (const string & s : this->all_endpoints){
      BOOST_LOG_TRIVIAL(debug) << "\tStarting node" S_CYAN << s << S_NOR;
      this->nodes[s] = make_shared<BftAndColleague>(s,this->all_endpoints);
    }
  }

  void kick(string s){
    if (not this->nodes.contains(s)){
      BOOST_LOG_TRIVIAL(debug) << "Trying to kick " S_RED + s + S_NOR ", but it doesn't exists. ";
      return ;
    }

    BOOST_LOG_TRIVIAL(debug) << "🚮️ kick " S_RED + s + S_NOR;

    // this->nodes[s]->close();
    this->nodes.erase(s);
  }

  void make_node(){
    string s = (format("N%d") % this->N).str();
    this->N++;

    BOOST_LOG_TRIVIAL(debug) <<  "\tAdding node " S_GREEN + s + S_NOR;

    // 🦜 Update the current all_endpoints
    {
      std::unique_lock l(this->nodes.begin()->second->nd->all_endpoints.lock);
      this->all_endpoints = this->nodes.begin()->second->nd->all_endpoints.o;
    }

    // make the node
    this->nodes[s] = make_shared<BftAndColleague>(s, this->all_endpoints);
  }
};

void start_cluster(int n){
  unique_ptr<mock::AsyncEndpointNetworkNode> nClient =
    make_unique<mock::AsyncEndpointNetworkNode>("ClientAAA");

  NodeFactory fac{n};


  while (true){
    string reply;
    cout << "Enter: ";
    cin >> reply;
    if (reply == "stop" or reply == "s")
      break;

    if (reply == "append" or reply == "a"){
      cout << S_MAGENTA << "Appending node\n" << S_NOR;
      fac.make_node();
      while (cin.get() != '\n') continue;
      continue;
    }

    if (reply == "kick"){
      string nodeToKick;
      cin >> nodeToKick;
      cout << S_MAGENTA << "Kicking node " << nodeToKick
           << "\n" << S_NOR;

      fac.kick(nodeToKick);

      // eat line
      while (cin.get() != '\n') continue;
      continue;
    }

    string cmd;
    cin >> cmd;
    while (cin.get() != '\n') continue;
    cout << S_MAGENTA "Sending " + cmd +
      " To " + reply +  S_NOR << "\n";
    nClient->send(reply,"/pleaseExecuteThis",cmd);
  }

  BOOST_LOG_TRIVIAL(debug) << "Tester closed";

  fac.nodes.clear();            // remove all (🦜 : calls the dtor respectively).
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
// eval: (setq flycheck-gcc-include-path (list ".."))
// End:
