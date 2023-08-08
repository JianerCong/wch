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
 *   1. Read the options. The among many options, important ones are:
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
 *  1. We start the ::pure::WeakHttpServer from `pure-weakHttpServer.hpp`. This
 *  should be started first, and stopped last. Other part will
 *  register/unregister listeners on it. (🦜 : Which part? 🐢 : I think for now
 *  it's just the Rpc and the httpAsstn, which is for cnsss (P2P). 🦜: Okay...)
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
 *  initialize some module right? 🐢: Yes.) In particular, we need
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
 *      sealer). This doesn't need anything else from apart from a
 *      pure-consensus.
 *
 *    To start a pure-consensus, we need two things: an `exe`
 *    (::pure::IForConsensusExecutable) and an `network`
 *    (::pure::IEndpointBasedNetworkable).
 *
 *    4.1 `exe` is kinda like the "computer" of pure-consensus, which should be
 *    complex.(🐢 : Here is also where we can mock a good amount of code.) In
 *    real run, we should start an `ExecutorForCnsss`(cnsss/exeForCnsss.hpp) for Cnsss
 *    here. This will require two things
 *    
 *       4.1.1 an `IBlkExecutable`: which can be used to execute and commit Blk.
 *       Serious instance is the `BlkExecutor` (execManager.hpp). This will need 2 + 1 things:
 *       
 *          4.1.1.1 `IWorldChainStateSettable` and `IAcnGettable`, these are the
 *          storage related interface that the `WorldStorage` can provide.
 *
 *          4.1.1.2 `ITxExecutable`, the class that can execute one Tx and
 *          returns the state changes and result. Serious implementation is
 *          `EvmExecutor` (evmExecutor.hpp). This instance don't need anything
 *          to initialize.
 *
 *       4.1.2 an `IPoolSettable`: this is where exe can throw `Tx` in. Serious
 *       implementation is Mempool (cnsss/mempool.hpp).
 *
 *    4.2 `net` is the network that cnsss use to communicate with each other.
 *    Serious implementation is `IpBasedHttpNetAsstn`
 *    (net/pure-httpNetAsstn.hpp). This requires a pointer-to-WeakHttpServer
 *    (started in step 1) and a message manager.(🦜 : What is this? 🐢 : This
 *    will "sign" the datagram that you send, this lets the receiver know "Who R
 *    U".) There's `NaiveMsgMgr` in the same file (net/pure-httpNetAsstn.hpp),
 *    this takes care of some msg wrangling before and after sending and
 *    receiving over the network.
 *
 *    One thing to note that NaiveMsgMgr also need an `id` to start. This `id`
 *    need to be constructed seriously and it will be returned by the
 *    cnsss->listened_endpoint(). (🦜 : Okay, so what's the format of this
 *    string ?)
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
#include "net/pure-weakHttpServer.hpp"
#include "storageManager.hpp"
#include "rpc.hpp"

#include "cnsss/pure-listenToOne.hpp"
#include "net/pure-httpNetAsstn.hpp"

#include "evmExecutor.hpp"
#include "execManager.hpp"
#include "cnsss/mempool.hpp"
#include "cnsss/exeForCnsss.hpp"

#include "cnsss/sealer.hpp"
#include "cnsss/cnsssBlkChainAsstn.hpp"


#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>

// #define WEAK_WITH_SEALER
#undef WEAK_WITH_SEALER
#define WEAK_IN_RAM
// #undef WEAK_IN_RAM
#define WEAK_MOCK_EXE
// #undef WEAK_MOCK_EXE



namespace weak{

  void sleep_for(int i=1){
    std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
  }
  using ::pure::IPBasedHttpNetAsstn;
  using ::pure::ICnsssPrimaryBased;
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
    unique_ptr<hash256> h =  make_unique<hash256>(rB.value().hash);

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

  struct LightExeAndPartners{
    unique_ptr<EvmExecutor> tx_exe;
    unique_ptr<BlkExecutor> blk_exe;
    unique_ptr<LightExecutorForCnsss> exe;

    LightExeAndPartners(
                        IWorldChainStateSettable* w1,
                        IAcnGettable * w2,
                        uint64_t next_blk_number=0,
                        hash256 previous_hash = {},
                        int optimization_level = 2,
                        IPoolSettable * p = nullptr,
                        IForSealerTxHashesGettable * m = nullptr){
      this->tx_exe = make_unique<EvmExecutor>();
      this->blk_exe = make_unique<BlkExecutor>(
                                               w1,
                                               dynamic_cast<ITxExecutable*>(&(*this->tx_exe)),
                                               w2
                                               );

      // 🦜 : ^^ above copied from ExeAndPartners
      this->exe = make_unique<LightExecutorForCnsss>(
                                                     dynamic_cast<IBlkExecutable*>(&(*this->blk_exe)),
                                                     p,m,
                                                     optimization_level,
                                                     next_blk_number,
                                                     previous_hash);
    }
  };

  struct ExeAndPartners{
    unique_ptr<EvmExecutor> tx_exe;
    unique_ptr<BlkExecutor> blk_exe;
    unique_ptr<ExecutorForCnsss> exe;

    ExeAndPartners(
                   IWorldChainStateSettable* w1,
                   IAcnGettable * w2,
                   IPoolSettable * p
                   ){
      /*
        🦜 : I just realize that EvmExecutor is stateless...

        🐢 : It is, but we have to make it a class so that it can "implement" a
        method. A namespace can't.
      */
      this->tx_exe = make_unique<EvmExecutor>();
      this->blk_exe = make_unique<BlkExecutor>(
                                               w1,
                                               dynamic_cast<ITxExecutable*>(&(*this->tx_exe)),
                                               w2
                                               );
      this->exe = make_unique<ExecutorForCnsss>(
                                                dynamic_cast<IBlkExecutable*>(&(*this->blk_exe)),
                                                p
                                                );
    }
  };

  void init(int argc, char* argv[]){
    Options o{argc,argv};
    BOOST_LOG_TRIVIAL(debug) << format("⚙️ Chain started: cnsss=" S_CYAN "%s" S_NOR ",port=" S_CYAN "%d" S_NOR)
      % o.consensus_name % o.port;

    {
      ::pure::WeakHttpServer srv{boost::numeric_cast<uint16_t>(o.port)}; // throw bad_cast
      std::this_thread::sleep_for(std::chrono::seconds(1)); // wait until its up
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
        BOOST_LOG_TRIVIAL(info) << format("⚙️ starting " S_CYAN "cnsss" S_NOR);
        // 4.1 exe

        // 4.1.2
        // ::pure::ICnsssPrimaryBased cnsss;
        Mempool pool{move(txhs),o.txs_per_blk};

        // 4.1.1.2
        struct {
          unique_ptr<ExeAndPartners> normal;
          unique_ptr<LightExeAndPartners> light;
          unique_ptr<mock::Executable> mock;

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
          exe.mock = make_unique<mock::Executable>(IPBasedHttpNetAsstn::combine_addr_port(o.my_endpoint,o.port));

          exe.iForConsensusExecutable = dynamic_cast<::pure::IForConsensusExecutable*>(&(*exe.mock));
        }else{
          if (o.light_exe == "yes"){
            if (fresh_start){
              BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting [fresh-start] " S_CYAN "`light exe`" S_NOR " for cnsss");
              exe.light = make_unique<LightExeAndPartners>(w.iWorldChainStateSettable,
                                                           w.iAcnGettable);
            }else{
              BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting [persisted] " S_CYAN "`light exe`" S_NOR
                                                " for cnsss, current chain size: " S_CYAN " %d " S_NOR) % (*latest_blk_hash);
              exe.light = make_unique<LightExeAndPartners>(w.iWorldChainStateSettable,
                                                           w.iAcnGettable,
                                                           boost::numeric_cast<uint64_t>(*latest_blk_num) + 1,
                                                           *latest_blk_hash
                                                           );
            }

            exe.iForConsensusExecutable = dynamic_cast<::pure::IForConsensusExecutable*>(&(*(exe.light->exe)));
          }else{
            BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting " S_CYAN "`normal exe`" S_NOR " for cnsss");
            exe.normal = make_unique<ExeAndPartners>(w.iWorldChainStateSettable,
                                                     w.iAcnGettable,
                                                     dynamic_cast<IPoolSettable*>(&pool));

            exe.iForConsensusExecutable = dynamic_cast<::pure::IForConsensusExecutable*>(&(*(exe.normal->exe)));
          }
        }

        // 4.2 net

        {
          string endpoint = IPBasedHttpNetAsstn::combine_addr_port(o.my_endpoint,o.port);
          BOOST_LOG_TRIVIAL(info) << format("\t⚙️ using p2p endpoint: " S_CYAN "%s" S_NOR) % endpoint;
          string endpoint_for_cnsss = ::pure::SignedData::serialize_3_strs("<mock-pk>",endpoint,"");
          ::pure::NaiveMsgMgr msg_mgr{endpoint_for_cnsss}; // the default mgr

          IPBasedHttpNetAsstn net{&srv,dynamic_cast<::pure::IMsgManageable*>(&msg_mgr)};


          ICnsssPrimaryBased * cnsss;
          using ::pure::ListenToOneConsensus;
          {
            /* 🦜 : Here we should have a series of
               shared_ptr<C1> p_C1;
               shared_ptr<C2> p_C2;
               shared_ptr<C3> p_C3;
            */
            shared_ptr<ListenToOneConsensus> p_ListenToOneConsensus;


            if (o.consensus_name == "Solo"){
              string endpoint_node_to_connect;

              if (not o.Solo_node_to_connect.empty()){
                BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting as Solo sub");
                endpoint_node_to_connect = ::pure::SignedData::serialize_3_strs("<mock-pk>",o.Solo_node_to_connect,"");
                // 🦜 : It's important to make this <mock-pk> same as the one registered to msg_msg for primary node.
              }else{
                BOOST_LOG_TRIVIAL(info) << format("\t⚙️ Starting as Solo primary");
              }

              p_ListenToOneConsensus = ListenToOneConsensus::create(dynamic_cast<::pure::IEndpointBasedNetworkable*>(&net),
                                                                    exe.iForConsensusExecutable,
                                                                    endpoint_node_to_connect);


              cnsss = dynamic_cast<ICnsssPrimaryBased*>(&(*p_ListenToOneConsensus));
            }else{
              BOOST_LOG_TRIVIAL(error) << format("❌️ Unknown consensus method: " S_RED "%s" S_NOR) % o.consensus_name;
              std::exit(EXIT_FAILURE);
            }

            CnsssBlkChainAsstn cnsss_asstn{cnsss};

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
                HttpRpcAsstn hra{&srv};
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
