#pragma once

#include "init/options.hpp"
// #include "net/pure-weakHttpServer.hpp"
#include "net/pure-weakAsyncHttpServer.hpp" // 🦜 : We migrated from the above to this [2023.08.22]
#include "storageManager.hpp"
#include "rpc.hpp"

// cnsss
#include "cnsss/pure-listenToOne.hpp"
#include "cnsss/pure-rbft.hpp"
#include "cnsss/pure-raft.hpp"

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
    if (s.empty() or s == "debug"){
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
                        IForLightExeTxWashable * p,
                        uint64_t next_blk_number=0,
                        hash256 previous_hash = {},
                        int optimization_level = 2
                        ){
      this->tx_exe = make_unique<Div2Executor>();
      this->blk_exe = make_unique<BlkExecutor>(w1, dynamic_cast<ITxExecutable*>(this->tx_exe.get()),w2, txf);

      // 🦜 : ^^ above copied from ExeAndPartners
      this->exe = make_unique<LightExecutorForCnsss>(
                                                     dynamic_cast<IBlkExecutable*>(&(*this->blk_exe)),
                                                     p,
                                                     optimization_level,
                                                     next_blk_number,
                                                     previous_hash);
    }
  };

  struct ExeAndPartners{
    unique_ptr<Div2Executor> tx_exe;
    unique_ptr<BlkExecutor> blk_exe;
    unique_ptr<ExecutorForCnsss> exe;

    ExeAndPartners(IWorldChainStateSettable* w1,
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

      this->blk_exe = make_unique<BlkExecutor>(w1, dynamic_cast<ITxExecutable*>(this->tx_exe.get()),w2, txf);

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
      // 0. Prepare the tx-verifier
      struct {
        unique_ptr<TxVerifier> tx_verifier;
        ITxVerifiable * iTxVerifiable = nullptr;
      } txf;

      // <2024-04-02 Tue> 🦜 prepare the tx-mode config
      auto [serious,ca_pk_pem] = figure_out_tx_mode(o.tx_mode_serious);
      if (serious){
        BOOST_LOG_TRIVIAL(info) << "\t⚙️ Starting " S_CYAN "`txVerifyer`" S_NOR;
        txf.tx_verifier = make_unique<TxVerifier>(ca_pk_pem);
        txf.iTxVerifiable = dynamic_cast<ITxVerifiable*>(txf.tx_verifier.get());
      }

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
            if (o.light_exe == "yes"){
              if (fresh_start){
                BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting [fresh-start] " S_CYAN "`light exe`" S_NOR " for cnsss");
                exe.light = make_unique<LightExeAndPartners>(w.iWorldChainStateSettable,
                                                             w.iAcnGettable,
                                                             txf.iTxVerifiable,
                                                             dynamic_cast<IForLightExeTxWashable*>(&pool)
                                                             );
              }else{
                BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting [persisted] " S_CYAN "`light exe`" S_NOR
                                                  " for cnsss, current chain size: " S_CYAN " %d " S_NOR) % (*latest_blk_hash);
                exe.light = make_unique<LightExeAndPartners>(w.iWorldChainStateSettable,
                                                             w.iAcnGettable,
                                                             txf.iTxVerifiable,
                                                             dynamic_cast<IForLightExeTxWashable*>(&pool),
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
                                                       txf.iTxVerifiable);

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
            }else if (o.consensus_name == "Rbft" or o.consensus_name == "Raft"){
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
                shared_ptr<::pure::RaftConsensus> raft;
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
              }else if (o.consensus_name == "Raft"){ // <2024-04-19 Fri> 🦜 : raft
                vector<string> others_endpoints = prepare_endpoint_list(o.Raft_node_list);
                cnsss.raft = ::pure::RaftConsensus::create(net.iAsyncEndpointBasedNetworkable,
                                                           exe.iForConsensusExecutable,
                                                           others_endpoints);
                cnsss.iCnsssPrimaryBased = dynamic_cast<ICnsssPrimaryBased*>(cnsss.raft.get());
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
                    dynamic_cast<IForRpc*>(&pool),
                    txf.iTxVerifiable
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
    } // txVerifier closed
    BOOST_LOG_TRIVIAL(info) << format("👋 Chain closed");
  };                            // init()
} // namespace weak
