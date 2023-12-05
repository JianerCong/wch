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
using namespace pure;

#define TRY_REAL_SSL

#if defined (TRY_REAL_SSL) && defined (WITH_PROTOBUF)
/*
  🐢 : If we have pb serialization, we are ready to use our openssl-based
  IMsgManageable.
 */

struct BftAndColleague{
  mock::Executable * e;
  SslMsgMgr * s;
  mock::AsyncEndpointNetworkNode * n;

  shared_ptr<RbftConsensus> nd;
  string my_endpoint;
  /*
    s in fact is our endpoint, so it's usually formed by:

    string endpoint = ::pure::SignedData::serialize_3_strs("<my-pk-pem>","localhost:1234","<my-cert>");

    Therefore, we can get what we need from this endpoint:
   */
  BftAndColleague(string  s,
                  vector<string> all_endpoints,
                  /*pass by value (important)*/
                  string ca_pk_pem = ""
                  ): my_endpoint(s){

    auto r = SignedData::parse_3_strs(s);
    BOOST_ASSERT(r);
    auto [my_sk_pem, my_addr_port_str, mmy_cert] = r.value();

    this->e = new mock::Executable(s);
    this->n = new mock::AsyncEndpointNetworkNode(s);
    this->s = new SslMsgMgr(my_sk_pem, my_addr_port_str, mmy_cert, ca_pk_pem);

    this->nd = RbftConsensus::create(
                                     dynamic_cast<IAsyncEndpointBasedNetworkable *>(this->n),
                                     dynamic_cast<IForConsensusExecutable*>(this->e),
                                     dynamic_cast<IMsgManageable *>(this->s),
                                     all_endpoints
                                     );
  }

  ~BftAndColleague(){
    BOOST_LOG_TRIVIAL(debug) <<  "Closing BftAndColleague: " S_MAGENTA << this->my_endpoint.substr(0,4) << S_NOR;
    this->nd.reset();           // release the shared_ptr and close the BFT (calls the d'tor)

    // Now they can be safely freed
    delete this->e;
    delete this->s;
    delete this->n;
  }
}

  struct NodeFactory {
    unordered_map<string,shared_ptr<BftAndColleague>> nodes;
    vector<string> all_endpoints;
    int N;
    // the CA's key
    UniquePtr<EVP_PKEY> ca;

    NodeFactory(int n = 2): N(n){
      BOOST_LOG_TRIVIAL(debug) << "🌱 \tInitial cluster size: " S_CYAN << n << S_NOR ;
      for (int i = 0;i<n;i++){
        this->all_endpoints.push_back((format("N%d") % i).str());
      }

      for (const string & s : this->all_endpoints){
        BOOST_LOG_TRIVIAL(debug) << "\tStarting node" S_CYAN << s << S_NOR;
        this->nodes[s] = make_shared<BftAndColleague>(s,this->all_endpoints);
      }

      // generate the CA
      ca = UniquePtr<EVP_PKEY>(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"));
    }

    /**
     * @brief make a new node and add it to the cluster
     *
     * 🦜 : What do we need to make a new node?
     *
     * 🐢 : We need to
     *    1. generate a new key pair
     *    2. generate a new certificate (sign the pk pem of new pair using ca's sk).
     *    3. generate a new endpoint string (using the new pk pem and the new cert)
     *    4. make a new BftAndColleague
     * and that's it ?
     *
     * 🦜 : Okay, can the sk of this node be safely thrown away?
     *
     * 🐢 : I think so, after we passed the sk_pem to the SslMsgMgr, we don't need
     * it anymore.
     */
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
}

#else
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
  string my_endpoint;
  BftAndColleague(string  s,
                  vector<string> all_endpoints
                  /*pass by value (important)*/
                  ): my_endpoint(s){

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
    BOOST_LOG_TRIVIAL(debug) <<  "Closing BftAndColleague: " S_MAGENTA << this->my_endpoint << S_NOR;
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
      this->all_endpoints.push_back((format("N%d") % i).str());
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

    // 🦜 Update the current all_endpoints from a whatever node.
    {
      std::unique_lock l(this->nodes.begin()->second->nd->all_endpoints.lock);
      this->all_endpoints = this->nodes.begin()->second->nd->all_endpoints.o;
    }

    // make the node
    this->nodes[s] = make_shared<BftAndColleague>(s, this->all_endpoints);
  }
};
#endif

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
// eval: (setq flycheck-gcc-include-path (list ".." "../.."))
// End:
