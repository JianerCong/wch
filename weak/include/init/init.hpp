#pragma once
/**
 * @brief Initializer of the system.
 *
 * 🐢 : Here we start the system.
 *
 * 🦜 : So what's the process?
 *
 * 🐢 : The process is as folloing:
 *
 *   0. Read the options. The among many options, important ones are:
 *
 *        + string consensus_name: The consensus name to use.
 *
 *        + <consensus_related_settings>: Then we have different options for
 *        different consensus, and those options are grouped together.
 *
 *        + int port: The port to listen
 *
 *        + string data dir: The folder in which all the chain related data will
 *        be stored. Backing up this folder should back up the state of the
 *        nodes. And if the data dir is not empty, we will assume that this
 *        folder contains the data the chain need. (🦜 : Oh, so usually we need
 *        to set the data-dir to an empty folder right? 🐢 : Yes)
 *
 *        + and... That's it.
 *
 *  🦜 : Okay, so the important options are "3 + 1" = FC2P =
 *  <folder(data_dir)><consensus_name><consensus_spec><port>.
 *
 *  🐢 : these are at least, I think.
 *
 *  // Prepare system --------------------------------------------------
 *
 *  🦜 : Okay, what's next?
 *
 *  🐢 : Next we let the crew in. In particular:
 *
 *  1. We start the rpc server.
 *
 * Here we first start a `::pure::MultiStackHttpServer`, this is a container of
 * Http-servers. Next we init and add `WeakAsyncTcpHttpServer` to it.
 * Optionally, we add `::pure::WeakAsyncUnixHttpServer` if we have the option.
 *
 *  Other part will
 *  register/unregister listeners on it. (🦜 : Which part? 🐢 : I think for now
 *  it's just the Rpc and the httpAsstn, which is for cnsss (P2P). 🦜: Okay...)
 *
 *  We used to use WeakHttpServer, but now we have migrated to use
 *  WeakAsyncHttpServer. This will let boost::asio to manage threads for our
 *  handlers. Hopefully this will make it less error-prone when we turn the
 *  server down.
 *
 *  2. We start the WorldStorage(data_dir) instance, which will open (create if
 *  not present) rocksDB in two folders `<data_dir>/chainDB` and
 *  `<data_dir>/stateDB`.
 *
 *  3. Next, we will try to read the "other/blk_number" key on chainDB. If it's
 *  not there (which should be the case if <data_dir> itself is empty.), we
 *  assume that the chain is empty. Otherwise, we have some other things to do.
 *
 *  (🦜 : "other things" means that we need to some info about the chain to
 *  initialize some modules right? 🐢: Yes.) In particular, we need
 *
 *    3.1. the hashes of all Txs that is on the chain so far. (🦜: How do we get
 *    that? do we need to store all these into a key on the stateDB? 🐢: No, we
 *    do it the hard way. We count that by ourselves.) Remember that for each Tx
 *    on the chain, we store a (k="/tx/<tx.hash>",v=<TxReceipt>) right?
 *
 *    So we can just get all key in the chainDB that starts with "/tx/", then we
 *    can extract all the <tx.hash>.
 *
 *      vector<string> v = wrld.getKeysInChainDBStartWith("/tx/");
 *      vector<string> hashes;
 *      hashes.reserve(v.size());
 *
 *      for (const string k & : v){
 *         hashes.push_back(extract_hash_from_key(k))l
 *      }
 *
 *    3.2. The latest_blk_num, which should be chainDB["/other/blk_number"]
 *    . this (+ 1) is passed to sealer. Also, the `previous_hash`, which should be
 *    the hash of the Blk located at chainDB["/blk/<blk_number>"].
 *
 * 4. Next we start consensus. This has two step:
 *      + start a pure-consensus: this has all the dirty logic of consensus.
 *
 *      + start a CnsssBlkChainAsstn: this is like an agent that manage "some"
 *      of the communication between consensus and other modules (mainly RPC and
 *      sealer). This doesn't need anything else apart from a pure-consensus.
 *
 *    To start a pure-consensus, we need two things: an `exe`
 *    (::pure::IForConsensusExecutable) and an `network`
 *    (::pure::IEndpointBasedNetworkable or ::pure::IAsyncEndpointBasedNetworkable).
 *
 *    Optionally, we also need a signer for those cnsss that needs digital
 *    signature.
 *
 *    4.1 `exe` is kinda like the "computer" of pure-consensus, which should be
 *    complex.(🐢 : Here is also a place where we can mock a good amount of
 *    code.) In real run, we should start an
 *    `ExecutorForCnsss`(cnsss/exeForCnsss.hpp) for Cnsss here. This will
 *    require two things
 *
 *       4.1.1 an `IBlkExecutable`: which can be used to execute and commit Blk.
 *       Serious instance is the `BlkExecutor` (execManager.hpp). This will need 2 + 1 things:
 *
 *          4.1.1.1 `IWorldChainStateSettable` and `IAcnGettable`, these are the
 *          storage related interface that the `WorldStorage` can provide.
 *
 *          4.1.1.2 `ITxExecutable`, the class that can execute one Tx and
 *          returns the state changes and result. Serious implementation is
 *          `Div2Executor` (div2Executor.hpp). This instance don't need anything
 *          to initialize. ([2024-1-3] 🦜 : This use to be EvmExecutor, but now
 *          it's Div2Executor, it can switch between VMs.)
 *
 *          <2024-04-02 Tue> 🦜 : Now we introduce SeriousDiv2Executor, which
 *          can verify the Tx. We also added `--tx-mode-serious` option which is
 *          stored in the `o.tx_mode_serious` variable. It can be `debug`,
 *          `public` or `ca@/path/to/ca_pk_pem`. 🦜 : So here we need a function
 *          `[bool serious, string ca_pk_pem=""]
 *          figure_out_tx_mode(o.tx_mode_serious)`. Let's write it here...
 *
 *       4.1.2 an `IPoolSettable`: this is where exe can throw `Tx` in. Serious
 *       implementation is `Mempool` (cnsss/mempool.hpp).
 *
 *    4.2 `net` is the network that cnsss use to communicate with each other (p2p).
 *
 *    Here we have two types of network component (we called them `network
 *    assistant`).
 *
 *    4.2.1. `IEndpointBasedNetworkable`. This is for cnsss that need response for
 *    each request, such as Solo and Raft. In this case, we currenly have
 *    http-based network. Serious implementation is `IPBasedHttpNetAsstn`
 *    (net/pure-httpNetAsstn.hpp).
 *
 *    4.2.2. `IAsyncEndpointBasedNetworkable`. This is for cnssses that don't need
 *    response for each request, such as *BFT. In this case, we have
 *    `IPBasedUdpNetAsstn` (net/pure-udpNetAsstn.hpp).
 *
 *
 *    `IPBasedHttpNetAsstn` requires a pointer-to-WeakHttpServer (started in
 *    step 1) and a message manager.(🦜 : What is this? 🐢 : This will "sign"
 *    the datagram that you send, this lets the receiver know "Who R U".)
 *    There's `NaiveMsgMgr` in the same file (net/pure-netAsstn.hpp), this takes
 *    care of some msg wrangling before and after sending and receiving over the
 *    network.
 *
 *    🦜 : So message manager will sign the data before sending it? 🐢 : Yes.
 *
 *    🦜 : So I guess there's a serious implementation using openssl right?
 *
 *    🐢 : Yeah, and it's called `SslMsgMgr` (net/pure-netAsstn.hpp). Let's talk
 *    about that.
 *
 *       4.2.2.1 If the option `--without-crypto=no` is given (default is `yes`),
 *       then we will try to use a `SslMsgMgr`. This will consume all the
 *       options that starts with `--crypto`. In particular it contains:
 *
 *          + crypto.ca_public_key_pem_file: The path to the CA public key file.
 *
 *          + crypto.node_secret_key_pem_file: The path to the node's secret key
 *          file.
 *
 *          + crypto.node_cert_file: The path to the node's certificate
 *          file. (Which is CA's signature on the node's public key. Should be binary.)
 *
 *          + crypto.peer_json_file_or_string: The path to the <peer-json> file,
 *          or the json string itself. This json tells us the public key and
 *          signature of each peer. @see PeerCryptoInfo.
 *
 *       These options are only read when `--without-crypto=no` is given.
 *
 *       🦜 : Okay. But do we need to do some preparation before init an
 *       instance of `SslMsgMgr`?
 *
 *       🐢 : Yes. But we don't need to touch any crypto stuff here. We just
 *       need to read the content of those files.
 *
 *       string my_sk_pem = read_file(o.crypto.node_secret_key_pem_file); // plain text
 *       string ca_pk_pem = read_file(o.crypto.ca_public_key_pem_file); // plain text
 *       string my_cert = read_file(o.crypto.node_cert_file); // binary
 *
 *       auto mgr = SslMsgMgr(my_sk_pem, my_addr_port_str, my_cert,ca_pk_pem);
 *
 *       ... And.. that's it. The SslMsgMgr is up.
 *
 *    `IPBasedUdpNetAsstn` also requires a message manager, but more
 *    importantly, it needs a `port` to listen on UDP.
 *
 *    One thing to note that NaiveMsgMgr also need an `id` to start. This `id`
 *    need to be constructed seriously and it will be returned by the
 *    cnsss->listened_endpoint(). (🦜 : Okay, so what's the format of this
 *    string ? 🐢 : This is usually something like
 *
 *    ::pure::SignedData::serialize_3_strs("<pk>","10.0.0.2:7777","<crt>");
 *    ).
 *
 *    🐢 : Note that, if we are doing serious crypto and using Solo as our
 *    cnsss, then we would need to know the primary's public key and its crt:
 *
 *       string Solo_node_to_connect_pk_pem = read_file(o.crypto.Solo_node_to_connect_pk_pem_file); // plain text
 *       string Solo_node_to_connect_cert = read_file(o.crypto.Solo_node_to_connect_cert_file); // binary
 *       string endpoint_node_to_connect = ::pure::SignedData::serialize_3_strs(Solo_node_to_connect_pk_pem,
 *                                                                       o.Solo_node_to_connect,
 *                                                                       o.Solo_node_to_connect_cert)
 *
 *    4.2e `net` and `exe` are essential to (almost) all cnsss. In addition to
 *    that, some cnsss requires the magic of digital-signature (most commonly
 *    *BFT). If that's the case, we will reuse our msg_mgr. (🦜 : That's green.
 *    Smaller footprint, good.)
 *
 *    4.3 Now, we have our `net` and `exe` so we can start our cnsss. Different
 *    cnsss have different initial conditions. We document them here one by one.
 *
 *       4.3.1 Solo. This cnsss needs an `IEndpointBasedNetworkable`,
 *       `IForConsensusExecutable` and an optional argument `node_to_connect`.
 *       `node_to_connect` should be an endpoint, if this one is not given, then
 *       the node is considered to be the primary node. Otherwise, the node is
 *       considered to be a sub that will ask `node_to_connect`.
 *
 *       4.3.2 Rbft. This cnsss needs an `IAsyncEndpointBasedNetworkable`,
 *       `IForConsensusExecutable` and a list of endpoints. If this endpoint is
 *       in that list, then this node is considered to be an `initial member` of
 *       the cluster. All `initial member` must be started (almost) at the same
 *       time, and all of these `initial member` should have been initialized
 *       with the same list of endpoints.
 *
 *       Otherwise, (and usually we wanna do that for subs) if this endpoint is
 *       not in that endpoint list, then this node is considered to be a
 *       `newcomer` and it will try to ask the existing cluster to get in. If
 *       everything goes well, this node will join the group in the next epoch.
 *
 *
 *  5. Start the Sealer (cnsss/sealer.hpp). This will need the 1 + 1 + 2 things
 *    5.1 `IForSealerBlkPostable`, the interface exposed by the CnsssBlkChainAsstn.
 *
 *    5.2 `IForSealerTxHashesGettable`, the interface exposed by the pool.
 *
 *    5.3 the next_blk_number `n` (= latest_blk_num + 1) and the `previous_hash`
 *    obtained before. (🐢: previous_hash will be used to set the parent hash of
 *    the new Blk).
 *
 *    (🐢 : But, if we have chosen to use light-exe, or we have the option
 *    `--without_sealer= yes`, then this step can be skipped.)
 *
 *  6. Finally we start the Rpc (rpc.hpp), which requires four things:
 *
 *    6.1 `IForRpcNetworkable`: the network that rpc will use, this seperate RPC
 *    from HTTP. Serious implementation is the HttpRpcAsstn defined in the same file (rpc.hpp).
 *
 *    6.2 `IForRpcTxsAddable` : where RPC can drop vector<Tx> into. should be exposed by CnsssBlkChainAsstn.
 *
 *    6.3 `IChainDBGettable` : The chainDB that Rpc will ask data for.
 *
 *    6.4  `IForRpc` : The interface exposed by pool to show its status.
 */

#include "init/options.hpp"
// #include "net/pure-weakHttpServer.hpp"
#include "net/pure-weakAsyncHttpServer.hpp" // 🦜 : We migrated from the above to this [2023.08.22]
#include "storageManager.hpp"
#include "rpc.hpp"

// cnsss
#include "cnsss/pure-listenToOne.hpp"
#include "cnsss/pure-rbft.hpp"

#include "net/pure-httpNetAsstn.hpp"
#include "net/pure-udpNetAsstn.hpp"

#include "div2Executor.hpp"
#include "txVerifier.hpp"       // <2024-04-07 Sun> 🦜 : We need this to respond to the `--tx-mode-serious` option.

#include "execManager.hpp"
#include "cnsss/mempool.hpp"
#include "cnsss/exeForCnsss.hpp"

#include "cnsss/sealer.hpp"
#include "cnsss/cnsssBlkChainAsstn.hpp"


#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>

#include "toolbox.hpp"
namespace weak{





  struct PeerCryptoInfo {
    string addr_port;
    string pk_pem;
    string cert;

    string endpoint() const{
      return ::pure::SignedData::serialize_3_strs(this->pk_pem,this->addr_port,this->cert);
    }

    /**
     * @brief Parse the json file or string to get the peer crypto info.
     *
     * @param json_file_or_string The json file or string. If the string starts
     * with a '@', for example, "@hi.json" then it's a file path. Otherwise, it's the json string.
     *
     * @param required_keys The required keys in the json. If not all of them
     * are in the json, we will throw.
     *
     * The json should have the following format:

     * {
     *   "<addr:port>" : {"pk_pem_file" : "<path to pk pem file>", "cert_file" : "<path to cert pem file>"},
     *   "<addr:port2>" : {"pk_pem_file" : "<path to pk pem file>", "cert_file" : "<path to cert pem file>"},
     *   ...
     * }
     *
     * For example:
     *
     * {
     *   "localhost:7777" : {"pk_pem_file" : "./N0-pk.pem", "cert_file" : "./N0-cert.sig"},
     *   "localhost:7778" : {"pk_pem_file" : "./N1-pk.pem", "cert_file" : "./N1-cert.sig"},
     *   "localhost:7779" : {"pk_pem_file" : "./N2-pk.pem", "cert_file" : "./N2-cert.sig"}
     * }
     */
    static unordered_map<string,PeerCryptoInfo> parse_peer_json(string json_file_or_string,
                                                                vector<string> required_keys = {}
                                                                ){
      unordered_map<string,PeerCryptoInfo> o;

      if (json_file_or_string.size() > 0 and json_file_or_string[0] == '@'){
        // this is a file path
        BOOST_LOG_TRIVIAL(debug) << "\tParsing json file " S_CYAN << json_file_or_string.substr(1) << S_NOR " to get peer crypto info";
        string s = read_file(json_file_or_string.substr(1));
        o = parse_peer_json(s);
      }else{
        BOOST_LOG_TRIVIAL(debug) << "\tParsing json string";
        // this is a json string
        string s = json_file_or_string;
        json::error_code ec;
        json::value jv = json::parse(s, ec);
        BOOST_ASSERT_MSG(not ec,
                         (format("Error parsing json %s") % s).str().c_str()
                         );
        for (const auto & [k,v] : jv.as_object()){
          string addr_port = k;
          string pk_pem_file = string(v.at("pk_pem_file").as_string());
          string cert_file = string(v.at("cert_file").as_string());
          // 🦜 : It looks like we need to explicitly convert from json::string to std::string
          string pk_pem = read_file(pk_pem_file);
          string cert = read_file(cert_file);

          o[addr_port] = PeerCryptoInfo{addr_port,pk_pem,cert};
        }
      }

      // check if all required keys are in the json
      for (const string & k : required_keys){
        if (not o.contains(k)){
          BOOST_THROW_EXCEPTION(std::runtime_error((format("Required key %s is not in the json") % k).str()));
        }
      }
      return o;
    }
  };

  inline string turn_addr_port_into_raw_endpoint(string addr_port){
    return ::pure::SignedData::serialize_3_strs("<mock-pk>",addr_port,"");
  }


  namespace ranges = std::ranges;
  template<class T>
  bool is_unique(vector<T> v){
    ranges::sort(v);
    auto ret = ranges::unique(v);
    // 🦜 : If the input is unique, then the return value of unique is the end of v
    return ret.begin() == v.end();
  }

  /**
   * @brief Combine the <pk> with each of the host in v.
   *
   * @param v : A list of <address:port> such as {"10.0.0.1:80","10.0.0.1:81"}
   *
   * 🦜 : This list should be unique. If not, we std::exit(EXIT_FAILURE);
   *
   * @return the combined list. (Sorted)
   */
  vector<string> prepare_endpoint_list(const vector<string> & v){

    if (not is_unique(v)){
      BOOST_THROW_EXCEPTION(std::runtime_error(
                                               S_RED "❌️ The endpoint list is not unique: " +
                                               boost::algorithm::join(v,",") +
                                               S_NOR
                                               )
                            );
    }

    vector<string> o;
    o.reserve(v.size());
    for (const string & addr_port : v){
      o.push_back(turn_addr_port_into_raw_endpoint(addr_port));
    }
    return o;
  }

  void sleep_for(int i=1){
    std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
  }
  using std::unordered_set;
  /**
   * @brief extract <hash> from "/tx/<hash in hex>"
   *
   * @param k The key string
   *
   * @return the parsed hash. std::exit(EXIT_FAILURE) on error.
   */
  hash256 get_hash_from_key(const string & k){
    if (k.size() != 4 + 32 * 2)
      BOOST_THROW_EXCEPTION(std::runtime_error((format("The length of hash key should be 4 + 32*2 = 68, but %s has %d") % k % k.size()).str()));

    string hs = k.substr(4);
    auto r = evmc::from_hex<hash256>(hs);
    if (not r)
      BOOST_THROW_EXCEPTION(std::runtime_error((format("Error parsing hs=%s as hash") % hs).str()));

    return r.value();
  };

  /**
   * @brief get the required things from chainDB
   *
   * @param ns The blkNumber to be parsed
   * 
   * @return the latest blk number, latest blk hash, all tx hashes on the chain.
   *
   * 🦜 : Why do we need to pass the `IChainDBGettable2` ?
   *
   * 🐢 : Because we need not only `get()`, but also `getPrefix()` in order to
   * get the tx hashes.
   */
  tuple<
    unique_ptr<int>,
    unique_ptr<hash256>,
    unique_ptr<unordered_set<hash256>>> get_required_things_from_ChainDB(IChainDBGettable2 * w,const string & ns){
    // parse the blk number --------------------------------------------------

    // 🦜 : We borrow a static helper
    optional<int> rN;
    std::tie(rN, std::ignore) = Rpc::parse_positive_int(ns);
    if (not rN){
      BOOST_LOG_TRIVIAL(error) << format("❌️ Data Corruption Error : '/other/blk_number' on chain =" S_RED "%s" S_NOR" is invalid") % ns;
      BOOST_THROW_EXCEPTION(std::runtime_error("Error parsing blk_number"));
    }
    // Get the Blk
    // 🦜 : We borrow another static helper
    auto [rB,msg] = Rpc::get_Blk_from_chain(dynamic_cast<IChainDBGettable*>(w),rN.value());
    if (not rB){
      BOOST_LOG_TRIVIAL(error) << S_RED + msg + S_NOR;
      BOOST_THROW_EXCEPTION(std::runtime_error("Error getting Blk on chain"));
    }
    // 🐢 : Now we got the latest Blk. But we only need the hash, so:
    unique_ptr<int> n = make_unique<int>(rN.value());
    unique_ptr<hash256> h =  make_unique<hash256>(rB.value().hash());

    /*
      🦜 : Now what's left ?

      🐢 : Now we need to get the tx-hashes. Here we use the get-prefix key
      helper.
    */
    vector<string> v = w->getKeysStartWith("/tx/");
    auto vh = make_unique<unordered_set<hash256>>();
    vh->reserve(v.size());
    // It's time to loop, it's time.
    for (const string & k : v){
      vh->insert(get_hash_from_key(k));
    };
    return make_tuple(move(n),move(h),move(vh));
  }


  /**
   * @brief Figure out the tx mode from the string.
   *
   * @param s The string to be parsed. It can be `debug`, `public`, `ca@/path/to/ca.pem`
   *
   * @return the pair [serious, ca_pk_pem]
   */
  tuple<bool,string> figure_out_tx_mode(const string & s){
    if (s == "debug"){
      return make_tuple(false,"");
    }else if (s == "public"){
      return make_tuple(true,"");
    }else if (s.starts_with("ca@")){
      // 🦜 : We need to remove the "ca@" prefix
      return make_tuple(true,read_file(s.substr(3)));
    }else{
      BOOST_THROW_EXCEPTION(std::runtime_error((format("Error parsing tx mode %s, it should be one of 'debug', 'public', 'ca@/path/to/ca.pem'") % s).str()));
    }
  }


  struct LightExeAndPartners{

    unique_ptr<Div2Executor> tx_exe; // <! [2024-01-03] change to Div2Executor
    unique_ptr<BlkExecutor> blk_exe;
    unique_ptr<LightExecutorForCnsss> exe;

    LightExeAndPartners(IWorldChainStateSettable* w1,
                        IAcnGettable * w2,
                        ITxVerifiable * txf,
                        uint64_t next_blk_number=0,
                        hash256 previous_hash = {},
                        int optimization_level = 2,
                        IPoolSettable * p = nullptr,
                        IForSealerTxHashesGettable * m = nullptr
                        ){
      this->tx_exe = make_unique<Div2Executor>();
      this->blk_exe = make_unique<BlkExecutor>(w1, dynamic_cast<ITxExecutable*>(this->tx_exe.get()),w2,
                                                txf);

      // 🦜 : ^^ above copied from ExeAndPartners
      this->exe = make_unique<LightExecutorForCnsss>(
                                                     dynamic_cast<IBlkExecutable*>(&(*this->blk_exe)),
                                                     p,m,
                                                     txf, // 🦜 : Also one for you
                                                     optimization_level,
                                                     next_blk_number,
                                                     previous_hash);
    }
  };

  struct ExeAndPartners{
    unique_ptr<Div2Executor> tx_exe;
    unique_ptr<BlkExecutor> blk_exe;
    unique_ptr<ExecutorForCnsss> exe;

    ExeAndPartners(
                   IWorldChainStateSettable* w1,
                   IAcnGettable * w2,
                   IPoolSettable * p,
                   ITxVerifiable * txf
                   ){
      /*
        🦜 : I just realize that Div2Executor is stateless...

        🐢 : It is, but we have to make it a class so that it can "implement" a
        method. A namespace can't.
      */
      this->tx_exe = make_unique<Div2Executor>();

      this->blk_exe = make_unique<BlkExecutor>(w1, dynamic_cast<ITxExecutable*>(this->tx_exe.get()),
                                               w2, txf);

      this->exe = make_unique<ExecutorForCnsss>(dynamic_cast<IBlkExecutable*>(&(*this->blk_exe)), p);
    }
  };

  void init(int argc, char* argv[]){

    // 🦜 : <2024-04-03 Wed> If argv[1] is `toolbox`, we let the toolbox in
    if (argc > 1 and string(argv[1]) == "toolbox"){
      ::weak::Toolbox::run(argc,argv);
      return;
    }

    Options o{argc,argv};
    BOOST_LOG_TRIVIAL(debug) << format("⚙️ Chain started: cnsss=" S_CYAN "%s" S_NOR ",port=" S_CYAN "%d" S_NOR)
      % o.consensus_name % o.port;

    {
      struct {
        unique_ptr<::pure::WeakAsyncTcpHttpServer> s_tcp;
#if defined(__unix__)
        unique_ptr<::pure::WeakAsyncUnixHttpServer> s_unix;
#endif
        unique_ptr<::pure::MultiStackHttpServer> stk;
        ::pure::IHttpServable* iHttpServable;
      } srv;

      srv.stk = make_unique<::pure::MultiStackHttpServer>();

      // 🦜 : add tcp
      srv.s_tcp = make_unique<::pure::WeakAsyncTcpHttpServer>(boost::numeric_cast<uint16_t>(o.port));
      srv.stk->srvs.push_back(dynamic_cast<::pure::IHttpServable *>(srv.s_tcp.get()));
      // ::pure::WeakAsyncHttpServer srv{boost::numeric_cast<uint16_t>(o.port)}; // throw bad_cast

#if defined(__unix__)           // 🦜 : Add unix socket if we are on unix
      if (o.unix_socket != ""){
        srv.s_unix = make_unique<::pure::WeakAsyncUnixHttpServer>(o.unix_socket);
        srv.stk->srvs.push_back(dynamic_cast<::pure::IHttpServable *>(srv.s_unix.get()));
      }
#endif

      // make the pointer
      srv.iHttpServable = dynamic_cast<::pure::IHttpServable*>(srv.stk.get());

      // std::this_thread::sleep_for(std::chrono::seconds(1)); // wait until its up
      /* 🦜 : I doubt that.^^^ */
      {
        // 2.

        struct {
          unique_ptr<InRamWorldStorage> ram;
          unique_ptr<WorldStorage> db;

          IChainDBGettable* iChainDBGettable;
          IChainDBGettable2* iChainDBGettable2;
          IWorldChainStateSettable* iWorldChainStateSettable;
          IAcnGettable* iAcnGettable;
        } w;

        if (o.data_dir == ""){
          BOOST_LOG_TRIVIAL(info) << format("Starting in " S_CYAN "RAM mode" S_NOR ". Nothing will be written to disk");
          w.ram = make_unique<InRamWorldStorage>();

          // prepare the interfaces
          w.iChainDBGettable2 = dynamic_cast<IChainDBGettable2*>(&(*w.ram));
          w.iChainDBGettable = dynamic_cast<IChainDBGettable*>(&(*w.ram));
          w.iWorldChainStateSettable = dynamic_cast<IWorldChainStateSettable*>(&(*w.ram));
          w.iAcnGettable = dynamic_cast<IAcnGettable*>(&(*w.ram));
        }else{
          BOOST_LOG_TRIVIAL(info) << format("Starting rocksDB at data-dir = " S_CYAN "%s" S_NOR ) % o.data_dir;
          w.db = make_unique<WorldStorage>(o.data_dir);

          // prepare the interfaces
          w.iChainDBGettable2 = dynamic_cast<IChainDBGettable2*>(&(*w.db));
          w.iChainDBGettable = dynamic_cast<IChainDBGettable*>(&(*w.db));
          w.iWorldChainStateSettable = dynamic_cast<IWorldChainStateSettable*>(&(*w.db));
          w.iAcnGettable = dynamic_cast<IAcnGettable*>(&(*w.db));
          /*implicitly calls filesystem::path(string)*/
        };


        // 3.
        BOOST_LOG_TRIVIAL(info) << format("⚙️ try " S_CYAN "Restoring" S_NOR);
        optional<string> rN =  w.iChainDBGettable->getFromChainDB("/other/blk_number");
        const bool fresh_start = not bool(rN);
        unique_ptr<int> latest_blk_num;
        unique_ptr<hash256> latest_blk_hash;

        // 🦜 : You need to initialize txhs here. Otherwise you will pass an
        // empty unique_ptr to pool, which will let the core dump when adding
        // Txs to the pool.
        unique_ptr<unordered_set<hash256>> txhs = make_unique<unordered_set<hash256>>() ;

        if (fresh_start){
          BOOST_LOG_TRIVIAL(info) << format("Chain empty");
        }else{
          BOOST_LOG_TRIVIAL(info) << format("Opening existing chain.");
          /*
            🦜: This one throw

            🐢 : Let it throw
          */
          std::tie(latest_blk_num,latest_blk_hash,txhs) = get_required_things_from_ChainDB(w.iChainDBGettable2,rN.value());
        }

        // 4. Start the cnsss
        using ::pure::IPBasedHttpNetAsstn;
        using ::pure::IPBasedUdpNetAsstn;
        using ::pure::ICnsssPrimaryBased;
        BOOST_LOG_TRIVIAL(info) << format("⚙️ starting " S_CYAN "cnsss" S_NOR);
        // 4.1 exe

        // 4.1.2
        // ::pure::ICnsssPrimaryBased cnsss;
        Mempool pool{move(txhs),o.txs_per_blk};

        // 4.1.1.2
        struct {
          unique_ptr<ExeAndPartners> normal;
          unique_ptr<LightExeAndPartners> light;
          unique_ptr<::pure::mock::Executable> mock;
          ::pure::IForConsensusExecutable* iForConsensusExecutable;

          unique_ptr<TxVerifier> tx_verifier;
          ITxVerifiable * iTxVerifiable = nullptr;
        } exe;

        if (o.mock_exe == "yes"){
          /*
            🦜 : Why do we need to open a pool even if we are mocking the exe?

            🐢 : Because Rpc and sealer will need it. Although without a serious
            `exe`, nothing will be thrown into the pool.

            🦜 : Oh, so if the `exe` is mocked, `sealer` should be mocked as well
            right? because otherwise it will just keep checking the empty pool.

            🐢 : Yeah.
          */
          BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting " S_MAGENTA "mocked `exe`" S_NOR " for cnsss");
          exe.mock = make_unique<::pure::mock::Executable>(IPBasedHttpNetAsstn::combine_addr_port(o.my_address,o.port));

          exe.iForConsensusExecutable = dynamic_cast<::pure::IForConsensusExecutable*>(&(*exe.mock));
        }else{
          // <2024-04-02 Tue> 🦜 prepare the tx-mode config
          auto [serious,ca_pk_pem] = figure_out_tx_mode(o.tx_mode_serious);
          if (serious){
            BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting " S_CYAN "`serious exe`" S_NOR " for cnsss");
            exe.tx_verifier = make_unique<TxVerifier>(ca_pk_pem);
            exe.iTxVerifiable = dynamic_cast<ITxVerifiable*>(&(*exe.tx_verifier));
          }

          if (o.light_exe == "yes"){
            if (fresh_start){
              BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting [fresh-start] " S_CYAN "`light exe`" S_NOR " for cnsss");
              exe.light = make_unique<LightExeAndPartners>(w.iWorldChainStateSettable,
                                                           w.iAcnGettable,
                                                           exe.iTxVerifiable);
            }else{
              BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting [persisted] " S_CYAN "`light exe`" S_NOR
                                                " for cnsss, current chain size: " S_CYAN " %d " S_NOR) % (*latest_blk_hash);

              exe.light = make_unique<LightExeAndPartners>(w.iWorldChainStateSettable,
                                                           w.iAcnGettable,
                                                            exe.iTxVerifiable,
                                                           boost::numeric_cast<uint64_t>(*latest_blk_num) + 1,
                                                           *latest_blk_hash
                                                           );
            }
            exe.iForConsensusExecutable = dynamic_cast<::pure::IForConsensusExecutable*>(&(*(exe.light->exe)));

          }else{
            BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting " S_CYAN "`normal exe`" S_NOR " for cnsss");
            exe.normal = make_unique<ExeAndPartners>(w.iWorldChainStateSettable,
                                                     w.iAcnGettable,
                                                     dynamic_cast<IPoolSettable*>(&pool),
                                                      exe.iTxVerifiable);

            exe.iForConsensusExecutable = dynamic_cast<::pure::IForConsensusExecutable*>(&(*(exe.normal->exe)));
          }
        }

        // 4.2 net
        {
          string my_addr_port = IPBasedHttpNetAsstn::combine_addr_port(o.my_address,o.port);
          BOOST_LOG_TRIVIAL(info) << format("\t⚙️ using p2p addr_port: " S_CYAN "%s" S_NOR) % my_addr_port;


          struct {
            shared_ptr<::pure::NaiveMsgMgr> naive;
            shared_ptr<::pure::SslMsgMgr> ssl;
            ::pure::IMsgManageable * iMsgManageable;
          } msg_mgr;

          if (o.without_crypto == "yes"){
            string endpoint_for_cnsss = turn_addr_port_into_raw_endpoint(my_addr_port);
            msg_mgr.naive = make_shared<::pure::NaiveMsgMgr>(endpoint_for_cnsss);
            // the default mgr
            msg_mgr.iMsgManageable = dynamic_cast<::pure::IMsgManageable*>(&(*msg_mgr.naive));
          }else{
            BOOST_LOG_TRIVIAL(info) <<  format("⚙️ Using serious crypto");

            string my_sk_pem = read_file(o.crypto.node_secret_key_pem_file); // plain text
            string ca_pk_pem = read_file(o.crypto.ca_public_key_pem_file); // plain text
            string my_cert = read_file(o.crypto.node_cert_file); // binary

            msg_mgr.ssl = make_shared<::pure::SslMsgMgr>(my_sk_pem, my_addr_port, my_cert,ca_pk_pem);
            msg_mgr.iMsgManageable = dynamic_cast<::pure::IMsgManageable*>(&(*msg_mgr.ssl));
          }

          struct {
            unique_ptr<IPBasedHttpNetAsstn> http;
            unique_ptr<IPBasedUdpNetAsstn> udp;

            ::pure::IEndpointBasedNetworkable* iEndpointBasedNetworkable;
            ::pure::IAsyncEndpointBasedNetworkable* iAsyncEndpointBasedNetworkable;
          } net;

          if (o.consensus_name == "Solo" or o.consensus_name == "Solo-static"){
            BOOST_LOG_TRIVIAL(debug) <<  "\t 🌐️ Using " S_CYAN "http-based " S_NOR " p2p";
            net.http = make_unique<IPBasedHttpNetAsstn>(srv.iHttpServable,
                                                        msg_mgr.iMsgManageable);
            net.iEndpointBasedNetworkable = dynamic_cast<::pure::IEndpointBasedNetworkable*>(&(*net.http));
          }else if (o.consensus_name == "Rbft"){
            BOOST_LOG_TRIVIAL(debug) <<  "\t 🌐️ Using " S_CYAN "udp-based " S_NOR " p2p";
            net.udp = make_unique<IPBasedUdpNetAsstn>(boost::numeric_cast<uint16_t>(o.port),
                                                      msg_mgr.iMsgManageable
                                                      ); // throw bad_cast
            net.iAsyncEndpointBasedNetworkable = dynamic_cast<::pure::IAsyncEndpointBasedNetworkable*>(&(*net.udp));
          }else{
            BOOST_LOG_TRIVIAL(error) << format("❌️ Unknown consensus method: " S_RED "%s" S_NOR) % o.consensus_name;
            std::exit(EXIT_FAILURE);
          }

            // 4.3 cnsss
          {
            struct {
              ICnsssPrimaryBased * iCnsssPrimaryBased;

              shared_ptr<::pure::ListenToOneConsensus> listenToOne;
              shared_ptr<::pure::RbftConsensus> rbft;
            } cnsss;

            unordered_map<string,PeerCryptoInfo> peers; // <! the peers crypto info, empty if --without_crypto=yes (default)

            if (o.consensus_name == "Solo" or o.consensus_name == "Solo-static"){
              string endpoint_node_to_connect;

              if (not o.Solo_node_to_connect.empty()){
                BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting as Solo sub");

                if (o.without_crypto == "yes"){
                  endpoint_node_to_connect = turn_addr_port_into_raw_endpoint(o.Solo_node_to_connect);
                  // 🦜 : It's important to make this <mock-pk> same as the one registered to msg_msg for primary node.
                }else{
                  BOOST_LOG_TRIVIAL(debug) << "\t⚙️ Parsing peer json";
                  /*
                    🦜 : If we are using serious crypto, we need primary's cryptoInfo
                  */
                  peers = PeerCryptoInfo::parse_peer_json(o.crypto.peer_json_file_or_string,
                                                          {o.Solo_node_to_connect} /* required peer addr_port*/
                                                          );
                  BOOST_LOG_TRIVIAL(debug) << "\tTrying to get endpoint for " S_CYAN << o.Solo_node_to_connect << S_NOR  " from peer json";
                  endpoint_node_to_connect = peers.at(o.Solo_node_to_connect).endpoint();
                }

              }else{
                BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting as Solo primary");
              }

              bool remember = (o.consensus_name == "Solo");
              if (not remember){
                BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Solo started in " S_CYAN "static mode (command_history won't be remembered)" S_NOR);
              }
              cnsss.listenToOne = ::pure::ListenToOneConsensus::create(net.iEndpointBasedNetworkable
                                                               ,exe.iForConsensusExecutable,
                                                                       endpoint_node_to_connect, remember);

              cnsss.iCnsssPrimaryBased = dynamic_cast<ICnsssPrimaryBased*>(&(*cnsss.listenToOne));
            }else if (o.consensus_name == "Rbft"){
              BOOST_ASSERT_MSG(not o.Bft_node_list.empty(),
                               "❌️ Empty cluster endpoint list for RBFT");

              vector<string> all_endpoints;
              if (o.without_crypto == "yes"){
                // 1. translate the node-addr to endpoint format.
                all_endpoints = prepare_endpoint_list(o.Bft_node_list);
                /*
                  🦜 : Instead of the following:

                  ranges::transform(o.Bft_node_list,
                  std::back_inserter(all_endpoints),
                  [](string c) -> string {
                  return ::pure::SignedData::serialize_3_strs("<mock-pk>",c,"");
                  });

                  I'm gonna do a dump loop ourselves.
                */
              }else{

                // 0. make sure that the node list is unique
                BOOST_ASSERT_MSG(is_unique(o.Bft_node_list),
                                 (
                                  "❌️ The endpoint list is not unique: " +
                                  boost::algorithm::join(o.Bft_node_list,",")
                                  ).c_str()
                                 );

                // 1. parse the peers json file
                peers = PeerCryptoInfo::parse_peer_json(o.crypto.peer_json_file_or_string,
                                                        o.Bft_node_list /* required peer addr_port*/
                                                        );

                // 2. translate the node-addr to endpoint format.
                for (const string & c : o.Bft_node_list){
                  all_endpoints.push_back(peers.at(c).endpoint());
                }

              }

              // 2. start the cnsss
              cnsss.rbft = ::pure::RbftConsensus::create(net.iAsyncEndpointBasedNetworkable,
                                                         exe.iForConsensusExecutable,
                                                         msg_mgr.iMsgManageable,
                                                         all_endpoints);

              cnsss.iCnsssPrimaryBased = dynamic_cast<ICnsssPrimaryBased*>(&(*cnsss.rbft));
            }else{
              BOOST_LOG_TRIVIAL(error) << format("❌️ Unknown consensus method: " S_RED "%s" S_NOR) % o.consensus_name;
              std::exit(EXIT_FAILURE);
            }

            CnsssBlkChainAsstn cnsss_asstn{cnsss.iCnsssPrimaryBased};

            // 5.
            {

              unique_ptr<Sealer> sl;
              if (o.without_sealer == "yes"){
                BOOST_LOG_TRIVIAL(info) << format("⚙️ Starting without" S_CYAN " sealer" S_NOR);
              }else{
                // The sealer --------------------------------------------------
                BOOST_LOG_TRIVIAL(info) << format("⚙️ Starting " S_CYAN "sealer" S_NOR);
                if (fresh_start){
                  sl = make_unique<Sealer>(dynamic_cast<IForSealerBlkPostable*>(&cnsss_asstn),
                                           dynamic_cast<IForSealerTxHashesGettable*>(&pool));
                }else{
                  sl = make_unique<Sealer>(dynamic_cast<IForSealerBlkPostable*>(&cnsss_asstn),
                                           dynamic_cast<IForSealerTxHashesGettable*>(&pool),
                                           boost::numeric_cast<uint64_t>(*latest_blk_num) + 1,
                                           *latest_blk_hash);
                }
              }

              {
                // 6.
                BOOST_LOG_TRIVIAL(info) << format("⚙️ Starting " S_CYAN "Rpc" S_NOR);
                HttpRpcAsstn hra{srv.iHttpServable};
                Rpc rpc{
                  dynamic_cast<IForRpcNetworkable*>(&hra),
                  dynamic_cast<IForRpcTxsAddable*>(&cnsss_asstn),
                  w.iChainDBGettable,
                  dynamic_cast<IForRpc*>(&pool)
                };

                namespace trivial = boost::log::trivial;
                if (o.verbose == "no"){
                  BOOST_LOG_TRIVIAL(info) << format( S_CYAN "🌐️ chain started in silence mode [any key to quit]" S_NOR);
                  // The global singleton core
                  boost::log::core::get()->set_filter
                    (
                     // A Boost.Phoenix lambda
                     trivial::severity >= trivial::error
                     // LHS: placeholder var; RHS: value of type severity_level
                     );
                }

                /*
                  🦜 : Now we just need to wait
                */
                BOOST_LOG_TRIVIAL(info) << format( S_CYAN "🌐️ chain started[any key to quit]" S_NOR);
                std::cin.get();

                // 🦜 : unset the verbosity to see things turned down.
                if (o.verbose == "no"){
                  // The global singleton core
                  boost::log::core::get()->set_filter
                    (
                     // A Boost.Phoenix lambda
                     trivial::severity >= trivial::trace
                     // LHS: placeholder var; RHS: value of type severity_level
                     );
                }

              } // rpc closed
            } // sealer closed
          } // cnsss cleared

        } // net for cnsss closed
        sleep_for(1);           // 🦜 : Give it some time to close the connections.
      } // wrld closed
    } // server closed
    BOOST_LOG_TRIVIAL(info) << format("👋 Chain closed");
  };                            // init()
} // namespace weak
