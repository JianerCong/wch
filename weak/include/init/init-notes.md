@brief Initializer of the system.

🐢 : Here we start the system.


🦜 : So what's the process?

🐢 : The process is as folloing:

  0. Read the options. The among many options, important ones are:

       + string consensus_name: The consensus name to use.

       + <consensus_related_settings>: Then we have different options for
       different consensus, and those options are grouped together.

       + int port: The port to listen

       + string data dir: The folder in which all the chain related data will
       be stored. Backing up this folder should back up the state of the
       nodes. And if the data dir is not empty, we will assume that this
       folder contains the data the chain need. (🦜 : Oh, so usually we need
       to set the data-dir to an empty folder right? 🐢 : Yes)

       + and... That's it.

 🦜 : Okay, so the important options are "3 + 1" = FC2P =
 <folder(data_dir)><consensus_name><consensus_spec><port>.

 🐢 : these are at least, I think.

 // Prepare system --------------------------------------------------

 🦜 : Okay, what's next?

 🐢 : Next we let the crew in. In particular:

## 0. <2024-04-08 Mon> We start the txVerifyer. It's interface `ITxVerifiable`
 will be passed to `Rpc`, `BlkExecutor` and `LightExe`.

         We also added `--tx-mode-serious` option which is
         stored in the `o.tx_mode_serious` variable. It can be `debug`,
         `public` or `ca@/path/to/ca_pk_pem`. 🦜 : So here we need a function
         `[bool serious, string ca_pk_pem=""]
         figure_out_tx_mode(o.tx_mode_serious)`. Let's write it here...

## 1. We start the rpc server.

Here we first start a `::pure::MultiStackHttpServer`, this is a container of
Http-servers. Next we init and add `WeakAsyncTcpHttpServer` to it.
Optionally, we add `::pure::WeakAsyncUnixHttpServer` if we have the option.

 Other part will
 register/unregister listeners on it. (🦜 : Which part? 🐢 : I think for now
 it's just the Rpc and the httpAsstn, which is for cnsss (P2P). 🦜: Okay...)

 We used to use WeakHttpServer, but now we have migrated to use
 WeakAsyncHttpServer. This will let boost::asio to manage threads for our
 handlers. Hopefully this will make it less error-prone when we turn the
 server down.

## 2. We start the WorldStorage(data_dir) instance, which will open (create if
 not present) rocksDB in two folders `<data_dir>/chainDB` and
 `<data_dir>/stateDB`.

## 3. Next, we will try to read the "other/blk_number" key on chainDB. If it's
 not there (which should be the case if <data_dir> itself is empty.), we
 assume that the chain is empty. Otherwise, we have some other things to do.

 (🦜 : "other things" means that we need to some info about the chain to
 initialize some modules right? 🐢: Yes.) In particular, we need

   3.1. the hashes of all Txs that is on the chain so far. (🦜: How do we get
   that? do we need to store all these into a key on the stateDB? 🐢: No, we
   do it the hard way. We count that by ourselves.) Remember that for each Tx
   on the chain, we store a (k="/tx/<tx.hash>",v=<TxReceipt>) right?

   So we can just get all key in the chainDB that starts with "/tx/", then we
   can extract all the <tx.hash>.

     vector<string> v = wrld.getKeysInChainDBStartWith("/tx/");
     vector<string> hashes;
     hashes.reserve(v.size());

     for (const string k & : v){
        hashes.push_back(extract_hash_from_key(k))l
     }

   3.2. The latest_blk_num, which should be chainDB["/other/blk_number"]
   . this (+ 1) is passed to sealer. Also, the `previous_hash`, which should be
   the hash of the Blk located at chainDB["/blk/<blk_number>"].

## 4. Next we start consensus. This has two step:
     + start a pure-consensus: this has all the dirty logic of consensus.

     + start a CnsssBlkChainAsstn: this is like an agent that manage "some"
     of the communication between consensus and other modules (mainly RPC and
     sealer). This doesn't need anything else apart from a pure-consensus.

   To start a pure-consensus, we need two things: an `exe`
   (::pure::IForConsensusExecutable) and an `network`
   (::pure::IEndpointBasedNetworkable or ::pure::IAsyncEndpointBasedNetworkable).

   Optionally, we also need a signer for those cnsss that needs digital
   signature.

   4.1 `exe` is kinda like the "computer" of pure-consensus, which should be
   complex.(🐢 : Here is also a place where we can mock a good amount of
   code.) In real run, we should start an
   `ExecutorForCnsss`(cnsss/exeForCnsss.hpp) for Cnsss here. This will
   require two things

      4.1.1 an `IBlkExecutable`: which can be used to execute and commit Blk.
      Serious instance is the `BlkExecutor` (execManager.hpp). This will need 2 + 1 things:

         4.1.1.1 `IWorldChainStateSettable` and `IAcnGettable`, these are the
         storage related interface that the `WorldStorage` can provide.

         4.1.1.2 `ITxExecutable`, the class that can execute one Tx and
         returns the state changes and result. Serious implementation is
         `Div2Executor` (div2Executor.hpp). This instance don't need anything
         to initialize. ([2024-1-3] 🦜 : This use to be EvmExecutor, but now
         it's Div2Executor, it can switch between VMs.)

      4.1.2 an `IPoolSettable`: this is where exe can throw `Tx` in. Serious
      implementation is `Mempool` (cnsss/mempool.hpp).

   4.2 `net` is the network that cnsss use to communicate with each other (p2p).

   Here we have two types of network component (we called them `network

   assistant`).

   4.2.1. `IEndpointBasedNetworkable`. This is for cnsss that need response for
   each request, such as Solo and Raft. In this case, we currenly have
   http-based network. Serious implementation is `IPBasedHttpNetAsstn`
   (net/pure-httpNetAsstn.hpp).

   4.2.2. `IAsyncEndpointBasedNetworkable`. This is for cnssses that don't need
   response for each request, such as *BFT. In this case, we have
   `IPBasedUdpNetAsstn` (net/pure-udpNetAsstn.hpp).


   `IPBasedHttpNetAsstn` requires a pointer-to-WeakHttpServer (started in
   step 1) and a message manager.(🦜 : What is this? 🐢 : This will "sign"
   the datagram that you send, this lets the receiver know "Who R U".)
   There's `NaiveMsgMgr` in the same file (net/pure-netAsstn.hpp), this takes
   care of some msg wrangling before and after sending and receiving over the
   network.

   🦜 : So message manager will sign the data before sending it? 🐢 : Yes.

   🦜 : So I guess there's a serious implementation using openssl right?

   🐢 : Yeah, and it's called `SslMsgMgr` (net/pure-netAsstn.hpp). Let's talk
   about that.

      4.2.2.1 If the option `--without-crypto=no` is given (default is `yes`),
      then we will try to use a `SslMsgMgr`. This will consume all the
      options that starts with `--crypto`. In particular it contains:

         + crypto.ca_public_key_pem_file: The path to the CA public key file.

         + crypto.node_secret_key_pem_file: The path to the node's secret key
         file.

         + crypto.node_cert_file: The path to the node's certificate
         file. (Which is CA's signature on the node's public key. Should be binary.)

         + crypto.peer_json_file_or_string: The path to the <peer-json> file,
         or the json string itself. This json tells us the public key and
         signature of each peer. @see PeerCryptoInfo.

      These options are only read when `--without-crypto=no` is given.

      🦜 : Okay. But do we need to do some preparation before init an
      instance of `SslMsgMgr`?

      🐢 : Yes. But we don't need to touch any crypto stuff here. We just
      need to read the content of those files.

      string my_sk_pem = read_file(o.crypto.node_secret_key_pem_file); // plain text
      string ca_pk_pem = read_file(o.crypto.ca_public_key_pem_file); // plain text
      string my_cert = read_file(o.crypto.node_cert_file); // binary

      auto mgr = SslMsgMgr(my_sk_pem, my_addr_port_str, my_cert,ca_pk_pem);

      ... And.. that's it. The SslMsgMgr is up.

   `IPBasedUdpNetAsstn` also requires a message manager, but more
   importantly, it needs a `port` to listen on UDP.

   One thing to note that NaiveMsgMgr also need an `id` to start. This `id`
   need to be constructed seriously and it will be returned by the
   cnsss->listened_endpoint(). (🦜 : Okay, so what's the format of this
   string ? 🐢 : This is usually something like

   ::pure::SignedData::serialize_3_strs("<pk>","10.0.0.2:7777","<crt>");
   ).

   🐢 : Note that, if we are doing serious crypto and using Solo as our
   cnsss, then we would need to know the primary's public key and its crt:

      string Solo_node_to_connect_pk_pem = read_file(o.crypto.Solo_node_to_connect_pk_pem_file); // plain text
      string Solo_node_to_connect_cert = read_file(o.crypto.Solo_node_to_connect_cert_file); // binary
      string endpoint_node_to_connect = ::pure::SignedData::serialize_3_strs(Solo_node_to_connect_pk_pem,
                                                                      o.Solo_node_to_connect,
                                                                      o.Solo_node_to_connect_cert)

   4.2e `net` and `exe` are essential to (almost) all cnsss. In addition to
   that, some cnsss requires the magic of digital-signature (most commonly
   *BFT). If that's the case, we will reuse our msg_mgr. (🦜 : That's green.
   Smaller footprint, good.)

   4.3 Now, we have our `net` and `exe` so we can start our cnsss. Different
   cnsss have different initial conditions. We document them here one by one.

      4.3.1 Solo. This cnsss needs an `IEndpointBasedNetworkable`,
      `IForConsensusExecutable` and an optional argument `node_to_connect`.
      `node_to_connect` should be an endpoint, if this one is not given, then
      the node is considered to be the primary node. Otherwise, the node is
      considered to be a sub that will ask `node_to_connect`.

      4.3.2 Rbft. This cnsss needs an `IAsyncEndpointBasedNetworkable`,
      `IForConsensusExecutable` and a list of endpoints. If this endpoint is
      in that list, then this node is considered to be an `initial member` of
      the cluster. All `initial member` must be started (almost) at the same
      time, and all of these `initial member` should have been initialized
      with the same list of endpoints.
      
      4.3.3 Raft. This cnsss, same as Rbft,  needs an `IAsyncEndpointBasedNetworkable`, 
      `IForConsensusExecutable` and a list of endpoints `others`. 
      This list should be stored at `o.Raft_node_list`

      Otherwise, (and usually we wanna do that for subs) if this endpoint is
      not in that endpoint list, then this node is considered to be a
      `newcomer` and it will try to ask the existing cluster to get in. If
      everything goes well, this node will join the group in the next epoch.


## 5. Start the Sealer (cnsss/sealer.hpp). This will need the 1 + 1 + 2 things
   5.1 `IForSealerBlkPostable`, the interface exposed by the CnsssBlkChainAsstn.

   5.2 `IForSealerTxHashesGettable`, the interface exposed by the pool.

   5.3 the next_blk_number `n` (= latest_blk_num + 1) and the `previous_hash`
   obtained before. (🐢: previous_hash will be used to set the parent hash of
   the new Blk).

   (🐢 : But, if we have chosen to use light-exe, or we have the option
   `--without_sealer= yes`, then this step can be skipped.)

## 6. Finally we start the Rpc (rpc.hpp), which requires four things:

   6.1 `IForRpcNetworkable`: the network that rpc will use, this seperate RPC
   from HTTP. Serious implementation is the HttpRpcAsstn defined in the same file (rpc.hpp).

   6.2 `IForRpcTxsAddable` : where RPC can drop vector<Tx> into. should be exposed by CnsssBlkChainAsstn.

   6.3 `IChainDBGettable` : The chainDB that Rpc will ask data for.

   6.4  `IForRpc` : The interface exposed by pool to show its status.

