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
#define WITH_PROTOBUF

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
  BftAndColleague(string  mmy_endpoint,
                  vector<string> all_endpoints,
                  /*pass by value (important)*/
                  string my_sk_pem,
                  string ca_pk_pem = ""
                  ): my_endpoint(mmy_endpoint){

    auto r = SignedData::parse_3_strs(mmy_endpoint);
    BOOST_ASSERT(r);
    auto [my_pk_pem, my_addr_port_str, mmy_cert] = r.value();

    BOOST_LOG_TRIVIAL(debug) <<  "Got my_pk_pem: " S_MAGENTA << my_pk_pem << S_NOR;
    BOOST_LOG_TRIVIAL(debug) <<  "Got my_addr_port_str: " S_MAGENTA << my_addr_port_str << S_NOR;

    this->e = new mock::Executable(mmy_endpoint);
    this->n = new mock::AsyncEndpointNetworkNode(mmy_endpoint);
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

};

struct NodeFactory {
  unordered_map<string,shared_ptr<BftAndColleague>> nodes;
  vector<string> all_endpoints;
  int N;
  // the CA's key
  const UniquePtr<EVP_PKEY> ca_sk;
  string ca_pk_pem() {
    return SslMsgMgr::dump_key_to_pem(this->ca_sk.get(), false /*is_secret*/);
  }
  /**
   * @brief Initiaze the cluster with n nodes
   * @param n the number of nodes
   *
   * 🦜 : If there's a function called `make_node()`, then why don't we just
   * call that function `n` times?
   *
   * 🐢 : I think because it's kinda necessary to have a `initial cluster`
   * first, that is, some nodes be started at the same time, and they all know
   * each other's endpoints.
   *
   * 🦜 : Okay, so what do we need to do in this ctor?
   *
   * 🐢 : I think we need to
   *   1. init the ca key pair.
   *   2. init the endpoints of n nodes, this includes
   *      2.1 generate the key pair for each node
   *      2.2 generate the cert for each node (sign the pk pem of new pair using ca's sk).
   *      2.3 generate the endpoint string for each node (using the new pk pem and the new cert)
   */
  NodeFactory(int n = 2): N(n),
                          // 1. generate the CA
                          ca_sk(UniquePtr<EVP_PKEY>(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519")))
  {

    BOOST_LOG_TRIVIAL(debug) << "🌱 \tInitial cluster size: " S_CYAN << n << S_NOR ;
    unordered_map<string,string> ep_to_sk_pem;
    // first, prepare the endpoints (this is usually filled in a config file)
    for (int i = 0;i<n;i++){
      // this->all_endpoints.push_back((format("N%d") % i).str());
      auto [sk_pem,ep] = this->prepare_node();
      this->all_endpoints.push_back(ep);
      ep_to_sk_pem[ep] = sk_pem;
    }

    for (string ep : this->all_endpoints){
      BOOST_LOG_TRIVIAL(debug) << "\tStarting node" S_CYAN << ep.substr(0,4) << S_NOR;
      this->nodes[ep] = make_shared<BftAndColleague>(ep,
                                                     this->all_endpoints,
                                                     ep_to_sk_pem[ep],
                                                     this->ca_pk_pem()
                                                     );

      /*
        🦜 : Let's try encryp something here
        --------------------------------------------------
      */
      string msg = "aaa";
      string data = this->nodes.at(ep)->nd->sig->prepare_msg(std::move(msg));
      // try decrypt using the first nodes
      auto r = this->nodes.begin()->second->nd->sig->tear_msg_open(data);
      if (r){
        BOOST_LOG_TRIVIAL(debug) << "✅️ tear_msg_open";
      }else{
        BOOST_LOG_TRIVIAL(error) << "❌️ failed tear_msg_open";
      }
      // --------------------------------------------------

    }

  }

  /**
   * @brief Prepare the key and endpoint for a new node
   * @return a tuple of <sk_pem, endpoint>
   *
   * 🐢: In this method, we:
   *
   *    1. generate a new key pair
   *    2. generate a new certificate (sign the pk pem of new pair using ca's sk).
   *    3. generate a new endpoint string (using the new pk pem and the new cert)
   * 
   */
  tuple<string,string> prepare_node(){
    // 1.
    UniquePtr<EVP_PKEY> sk(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"));

    // 2.
    string pk_pem = SslMsgMgr::dump_key_to_pem(sk.get(), false /*is_secret*/);
    string sk_pem = SslMsgMgr::dump_key_to_pem(sk.get(), true /*is_secret*/);
    string cert = SslMsgMgr::do_sign(ca_sk.get(),pk_pem);

    // 3.
    /*
      🦜 : I think a mocked <addr:port> here should be fine, be each endpoint
      only needs to be unique, and the random <pk_pem> should be enough.
    */
    string ep = ::pure::SignedData::serialize_3_strs(pk_pem,"<mk>",cert);

    return make_tuple(sk_pem,ep);
  }

  /**
   * @brief make a new node and add it to the cluster
   *
   * 🦜 : What do we need to make a new node?
   *
   * 🐢 : We need to
   *    1. call prepare_node() to get the sk_pem and the endpoint
   *    2. make a new BftAndColleague
   * and that's it ?
   */
  void make_node(){
    this->N++;
    BOOST_LOG_TRIVIAL(debug) <<  "\tAdding node no." S_GREEN <<  this->N << S_NOR;

    // 1.
    auto [sk_pem,ep] = this->prepare_node();
    // 2.

    // 🦜 Update the current all_endpoints
    this->update_all_endpoints();
    // make the node
    this->nodes[ep] = make_shared<BftAndColleague>(ep, this->all_endpoints,
                                                   sk_pem, this->ca_pk_pem());
  }

  void update_all_endpoints(){
    std::unique_lock l(this->nodes.begin()->second->nd->all_endpoints.lock);
    this->all_endpoints = this->nodes.begin()->second->nd->all_endpoints.o;
  }
  /**
   * @brief kick the ith node
   * @param i the index of the node to be kicked
   */
  void kick(int i){
    if (
        static_cast<size_t>(i) >= this->all_endpoints.size()){
      BOOST_LOG_TRIVIAL(debug) << "Trying to kick " S_RED + lexical_cast<string>(i) + S_NOR ", but it doesn't exists. ";
      return ;
    }

    string ep = this->all_endpoints[i];
    this->all_endpoints.erase(this->all_endpoints.begin() + i);

    this->nodes.erase(ep);
    BOOST_LOG_TRIVIAL(info) <<  "🚮️ kick " S_RED +
      ICnsssPrimaryBased::make_endpoint_human_readable(ep)
      + S_NOR;
  }
};

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

      #if defined (TRY_REAL_SSL) && defined (WITH_PROTOBUF)
      int nodeToKick;
      #else
      string nodeToKick;
      #endif

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
    #if defined (TRY_REAL_SSL) && defined (WITH_PROTOBUF)
    int i = lexical_cast<int>(reply);
    fac.update_all_endpoints();
    string ep = fac.all_endpoints[i];

    cout << S_MAGENTA "Sending " + cmd +
      " To " +
      ICnsssPrimaryBased::make_endpoint_human_readable(ep)
      +  S_NOR << "\n";
    nClient->send(ep,"/pleaseExecuteThis",cmd);
    #else
    cout << S_MAGENTA "Sending " + cmd +
      " To " +
      reply
      +  S_NOR << "\n";
    nClient->send(reply,"/pleaseExecuteThis",cmd);
    #endif
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
