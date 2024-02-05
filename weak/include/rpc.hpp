#pragma once
/**
 * @file rpc.hpp
 * @brief The Remote Procedure Call (RPC) handlers.
 *
 */

#include "core.hpp"
#include "net/pure-httpCommon.hpp"
#include "forPostExec.hpp"



namespace weak{
  using namespace std::placeholders;  // for _1, _2, _3...
  /**
   * @brief The network server interface for RPC to use.
   *
   * 🦜 : We add this interface because:
   *   + Rpc doesn't need to know where the request came from.
   *
   *   + Rpc itself is busy enough to handle all the works of listening, and we
   *     wanna outsource it.
   *
   *   + In the future we may support protocols other than HTTP.
   */
  class IForRpcNetworkable{
  public:
    using postHandler_t = function<tuple<bool,string> (string_view)>;
    using getHandler_t = function<tuple<bool,string> (optional<unordered_map<string,string>> // the query param
                                                      )>;
  virtual bool listenToPost(string target,postHandler_t h)noexcept=0;
  virtual bool listenToGet(string target,getHandler_t h)noexcept=0;
  };

  /**
   * @brief The RPC server
   *
   * 🦜 : This is the gateway between the outside world and the blockchain
   * world. The outside world communicates with the blockchain world through
   * this class which currently provides the following functionalities:
   *
   *   + POST /add_txs
   *   + GET /get_latest_Blk
   *   + GET /get_pool_status
   *   + GET /get_receipt
   *   + GET /get_node_status
   */
  class Rpc {
  public:
    IForRpcTxsAddable * const cnsss; // <! The consensus info 🦜: currently not used.
    IForRpcNetworkable * const srv; // <! The underlying server
    IForRpc * const pool;         // <! The pool state
    IChainDBGettable * const wrld; // <! The world

    Rpc(IForRpcNetworkable * const n,
        IForRpcTxsAddable * const c,
        IChainDBGettable * const w=nullptr,
        IForRpc * const p =nullptr
        ): cnsss(c), srv(n), pool(p), wrld(w) {

      if (w == nullptr)
        BOOST_LOG_TRIVIAL(warning) << format( "⚠️ Warining: no " S_MAGENTA "IChainDBGettable" S_NOR " passed to rpc. Should be in unit-test");

      if (p == nullptr)
        BOOST_LOG_TRIVIAL(warning) << format( "⚠️ Warining: no " S_MAGENTA "IForRpc from pool" S_NOR " passed to rpc. Should be in unit-test");

      n->listenToPost("/add_txs",bind(&Rpc::handle_add_txs,this,_1));
      n->listenToGet("/get_latest_Blk",bind(&Rpc::handle_get_latest_Blk,this,_1));
      n->listenToGet("/get_pool_status",bind(&Rpc::handle_get_pool_status,this,_1));
      n->listenToGet("/get_receipt",bind(&Rpc::handle_get_receipt,this,_1));
      n->listenToGet("/get_node_status",bind(&Rpc::handle_get_node_status,this,_1));
    }


    /**
     * @brief A hello world handler.
     *
     * This is used for testing.
     */
    tuple<bool,string>  handle_get_node_status(optional<unordered_map<string,string>> /*query_param*/){
      json::value jv ={
        {"status","OK"}
      };
      return make_tuple(true,
                        json::serialize(json::value_from(jv)));
    }

    /**
     * @brief get the latest Blk
     *
     * This handler will get the latest (N) Blk(s) in the chain. For example,
     * you can ask for the last, say 5 Blks. But if there's only 3 Blks on the
     * chain, then it will just return that three Blks.
     *
     * 🦜 : In detail, it will return an array of min(N,number_of_Blks) Blks.
     *
     * @param query_param Optional. If the query param contains a key "n", then
     * it will try to get the last n Blks. If not present, return the latest Blk.
     */
    tuple<bool,string>  handle_get_latest_Blk(optional<unordered_map<string,string>> query_param){
      if (not query_param or (not query_param.value().contains("n")))
        return get_latest_Blk(this->wrld);

      string ns = query_param.value().at("n");
      BOOST_LOG_TRIVIAL(debug) << format("Parsing user-supplied n=" S_MAGENTA "%s" S_NOR) % ns;
      auto [rN,err_msg] = parse_positive_int(ns);
      if (not rN){
        BOOST_LOG_TRIVIAL(debug) << format("Failed to parse user supplied query param n=%s to positive int") % ns;
        return make_tuple(false, err_msg);
      }

      return get_latest_Blk(this->wrld,rN.value());
    }

    /**
     * @brief Helper of handle_get_latest_Blk
     *
     * @param w The DB in which "/other/blk_number" is queried,
     * @param N The latest N `Blk` to fetch from the underlying chainDB. Should be positive.
     *
     * 🐢 : In details, it will.
     *
     *
     *    1. Get the current Blk number `M` stored at "/other/blk_number" (as
     *    stored by BlkExecutor in execManager.hpp).
     *
     *    2. It initialize an Json array `a`
     *
     *    3. It uses a loop `for (int i =0;i<N;i++)`
     *
     *    4. get the `M - i` th Blk, stored at "/blk/<blk.number>" deserialize it ⇒ Bi
     *
     *    5. a.push_back(Bi.toJsonString())`
     */
    static tuple<bool,string> get_latest_Blk(IChainDBGettable * const w, int N = 1) noexcept {
      json::array a;
      if (N==0)
        return make_tuple(true, json::serialize(a)); // user wants none

      if (N < 0)                // 🦜 : Although the input should have checked this in handle_get_latest_Blk();
        return make_tuple(false, (format("You cannot get latest <negative number=%d> Blks") % N).str()); // user wants none

      optional<string> r = w->getFromChainDB("/other/blk_number");
      if (not r)
        return make_tuple(true, json::serialize(a)); // empty chain

      auto [rM,err_msg] = parse_positive_int(r.value());
      if (not rM) return make_tuple(false,err_msg);
      int M = rM.value();

      // Here we get: N = min(N.M+1)
      if (N > (M+1)){
        //     ^ Blks remain
        N = M + 1;
      }

      // 3.
      for (int i=0;i<N;i++){    // do N times
        int bn = M - i;
        auto [rb, err_msg] = get_Blk_from_chain(w,bn);
        if (not rb)
          return make_tuple(false,err_msg);

        a.emplace_back(rb.value().toJson());
      }

      return make_tuple(true, json::serialize(a)); // empty chain
    }

    /**
     * @brief Try to parse_positive_int.
     *
     * @return the parsed number and error msg
     */
    static tuple<optional<int>,string> parse_positive_int(const string & n){
      optional<int> emp;
      // 1.
      int M;
      try {
        M = lexical_cast<int>(n);
      }catch(const boost::bad_lexical_cast &e){
        return make_tuple(emp, (format("Failed to parse %d as integer") % n).str());
      }
      if (M < 0){
        return make_tuple(emp, (format("Failed to parse %d as positive integer") % n).str());
      };

      return make_tuple(M,"");
}

    /**
     * @brief Get the bn-th Blk on the chain
     *
     * @param w The DB to get "/blk/%d"
     * @param bn The Blk number
     *
     * 🐢  : We factored this out mainly because we realized that init() can reuse this.. 
     *
     * @return The Blk found
     */
    static tuple<optional<ExecBlk>,string> get_Blk_from_chain(IChainDBGettable * const w, int bn)noexcept {
      BOOST_LOG_TRIVIAL(debug) << format("⚙️ Getting " S_CYAN "ExecBlk-%d" S_NOR) % bn;
      optional<string> r = w->getFromChainDB("/blk/" + lexical_cast<string>(bn));
      optional<ExecBlk> emp;    // 🦜 : Need this because make_tuple() has trouble accepting parameter `{}`
      string msg;
      if (not r){
        return make_tuple(emp,
                          (format("Data corruption Error: failed to get  /blk/%d on chainDB") % bn).str());
      }

      // 🦜 : got the value
      ExecBlk b;
      if (not b.fromString(r.value()))
        return make_tuple(emp, (format("Data corruption Error: /blk/%d on chainDB doesn't deserialize to Blk") % bn).str());
      return make_tuple(b,"OK");
    }

    /**
     * @brief Helper of handle_get_receipt()
     *
     * @param h The hex of hash
     */
    static tuple<bool,string> get_receipt(IChainDBGettable * const w, const string & h){

      // 1. --------------------------------------------------
      auto r = evmc::from_hex<hash256>(h);
      if (not r)
        return make_tuple(false, "Error: `hash` " + h + " is ill-formed");

      // 2. --------------------------------------------------
      /*
        🦜 : It turns out we don't even need to grab the actual hash here, we
        just need to use the hash-string to query.
      */
      optional<string> o = w->getFromChainDB("/tx/" + h);
      if (not o){
        return make_tuple(false, "Error: `hash` " + h + " does not (yet) exist on the chain.\n"
                          " Maybe you should try again later, or check your hash.");
      }

      TxOnBlkInfo info;
      if (not info.fromString(o.value()))
        return make_tuple(false, "Data corruption Error:: `TxOnBlkInfo` data corrupted on storage.");

      // 3. --------------------------------------------------
      o = w->getFromChainDB("/blk/" + lexical_cast<string>(info.blkNumber));
      if (not o){
        return make_tuple(false, "Data corruption Error: Inconsistant data on chain. ExecutedBlk doesn't yet exists on the chain.");
      }

      // 4. --------------------------------------------------
      ExecBlk b;
      if (not b.fromString(o.value()))
        return make_tuple(false, "Data corruption Error:: `ExecutedBlk` data corrupted on storage.");

      // 5. --------------------------------------------------
      TxReceipt tr = b.txReceipts[info.onBlkId];
      return make_tuple(true, tr.toJsonString());
    }

    /**
     * @brief get the receipt ofa particular hash
     *
     * @param query_param The query parameter which should contains the key `hash` and the corresponding value should be the Tx hash in hex (so it should have 64 char).
     * @return (ok,Json-encoded txReceipt)
     */
    tuple<bool,string>  handle_get_receipt(optional<unordered_map<string,string>> query_param){
      /*
        🦜 : To get the receipt of a particular Tx. We need to do the following:

        1. Use the hash to fetch the `TxOnBlkInfo`. If the TxOnBlkInfo is not
        found, the Tx is not committed on the chain(yet). This should be stored at "/tx/<hash>"

        2. Use `TxOnBlkInfo` to get the blkNumber and onBlkId;

        3. Get the `ExecutedBlk` on the chain. This should be stored at
        "/blk/<number>", usually
        ```c++
        "/blk/" + lexical_cast<string>(blkNumber)
        ```

        4. Now we got the corresponding `ExecutedBlk` b.

        5. The corresponding receipt should be at b.txReceipts[onBlkId];
      */
      if (
          (not query_param) or (not query_param.value().contains("hash"))
          )
        return make_tuple(false,
                          "Error: `/get_receipt` should be used with query parameter `hash=<Tx hash>`\n"
                          "For example: http://localhost:7777/get_receipt?hash=3d9a5bdd62ff8f12aab28cdb1c091918c5ede28c91c09e8f16a68a7c33add70e"
                          );

      string h = query_param.value()["hash"];
      return get_receipt(this->wrld,h);
    }

    /**
     * @brief query the pool status
     *
     * @return the current status of the pool. The logic should be in
     * mempool.hpp. In details, this shoule return a Json string. An example would be:

     {
       "max_txs_per_batch": 2,
       "hash_history": [
         "3d9a5bdd62ff8f12aab28cdb1c091918c5ede28c91c09e8f16a68a7c33add70e",
         "8909fb1e809eed683efa52f9df11532750b8d46028a4bfb85ed6a103d75a0fb6",
         "d34eb037a89b63fd806a90d4c567aaebec8ea529b17bf4d8474530e8353637da",
         "67c32d63391557f2291a461e58b966e8aa4a1678b63f06d368774241a1749251",
         "514e6efeadea95669dae6c3a32a228122647f5e3e5a9e6465993ce3298258c4e"
       ],
       "txs_in_pool": [
         {
           "from": "0000000000000000000000000000000000000001",
           "to": "0000000000000000000000000000000000000002",
           "data": "ffff",
           "nonce": 3,
           "timestamp": 1690859945,
           "hash": "3d9a5bdd62ff8f12aab28cdb1c091918c5ede28c91c09e8f16a68a7c33add70e"
         },
         {
           "from": "0000000000000000000000000000000000000001",
           "to": "0000000000000000000000000000000000000002",
           "data": "ffff",
           "nonce": 4,
           "timestamp": 1690859945,
           "hash": "8909fb1e809eed683efa52f9df11532750b8d46028a4bfb85ed6a103d75a0fb6"
         },
         {
           "from": "0000000000000000000000000000000000000002",
           "to": "0000000000000000000000000000000000000001",
           "data": "ffff",
           "nonce": 1,
           "timestamp": 1690859945,
           "hash": "d34eb037a89b63fd806a90d4c567aaebec8ea529b17bf4d8474530e8353637da"
         }
       ],
       "txs_in_pool_count": 3
     }

     * In detail, the returned Json has the following fields.
     *
     * "max_txs_per_batch" : The number of Txs sealed each time.
     *    🦜 : This is important. This (kinda) decides the number of Txs sealed in each Blk.
     *
     * "hash_history" : The array of Tx-hashes that has been added since Genesis Blk.
     *    🦜 : This should be a long list.
     *
     * "txs_in_pool" : This is the array of the current Tx in pool.
     *    🦜 : These Txs have been added to the pool and not been sealed.
     *
     * "txs_in_pool_count: : The size() of the "txs_in_pool".
     *
     *    🦜 : Although the client is able to calculate the size() from
     * `txs_in_pool` itself, I think it's usually handy for the client.
     */
    tuple<bool,string>  handle_get_pool_status(optional<unordered_map<string,string>> /*query_param*/){
      return make_tuple(true,this->pool->info());
    }

    /**
     * @brief handle Add Txs
     *
     * @param data The array to Txs to be added.
     *
     * @return The pair (OK,hashes), in which hashes contain the array of
     * txHashes, and in case of a contract creation, the address of the newly
     * created contract.

     *  POST /add_txs expects a json string which should be parsed into an
     *  array of Txs. For example:

     *      curl http://localhost:7777/add_txs -d \
     *      "
     *      [
     *        {
     *          \"from\":\"0000000000000000000000000000000000000001\",
     *          \"to\":\"0000000000000000000000000000000000000002\",
     *          \"data\":\"ffff\",
     *          \"nonce\":0,
     *        },
     *        {
     *          \"from\":\"0000000000000000000000000000000000000001\",
     *          \"to\":\"\",
     *          \"data\":\"ffff\",
     *          \"nonce\":0,
     *        }
     *      ]
     *      "
     *  🦜 : The first Tx wants to call the contract at address 0x2 and with
     *       ABI=ffff. The second Tx wants to create a new contract with contract
     *       init code "ffff"
     *
     *  It will return to client:
     */
    tuple<bool,string>  handle_add_txs(string_view data){
      optional<tuple<string,vector<Tx>>> r = Tx::parse_txs_jsonString_for_rpc(data);
      if (not r)
        return make_tuple(false,"Invalid input JSON format.");

      // 🦜 : Here we can start a thread to post the JSON
      auto [s, txs] = r.value();
      // return add_txs_and_return_this(s,txs);
      bool ok = this->cnsss->addTxs(move(txs));
      if (not ok)
        return make_tuple(false,"Sorry, we have encountered some error in our side."
                          "So your requests to add Txs is failed. Please try again latter.");

      return make_tuple(true,s);
    }
    tuple<bool,string> add_txs_and_return_this(const string & s, vector<Tx> txs){
      bool ok = this->cnsss->addTxs(move(txs));
      if (not ok)
        return make_tuple(false,"Sorry, we have encountered some error in our side."
                          "So your requests to add Txs is failed. Please try again latter.");

      return make_tuple(true,s);
    }
  };


  /**
   * @brief The agent that let Rpc uses HTTP.
   *
   * 🦜 : What this class does is in fact just dropping the `addr` and `port`
   * arguments that the WeakHttpServer requires.
   */
  class HttpRpcAsstn : public virtual IForRpcNetworkable{
  public:
    ::pure::IHttpServable * const srv;

    // HttpRpcAsstn(::pure::WeakHttpServer * const s): srv(dynamic_cast<::pure::IHttpServable*>(s)){}

    HttpRpcAsstn(::pure::IHttpServable * const s): srv(s){}

    bool listenToPost(string target,postHandler_t h)noexcept override{
      this->srv->listenToPost(target,
                        [h](string /*addr*/, uint16_t /*port*/, string_view data) -> tuple<bool,string> {
                          return h(data);
                        });
      return true;
    }
    bool listenToGet(string target,getHandler_t h)noexcept override{
      this->srv->listenToGet(target,
                        [h](string /*addr*/, uint16_t /*port*/,
                            optional<unordered_map<string,string>>
                            qparam) -> tuple<bool,string> {
                          return h(qparam);
                        });
      return true;
    }
  };

}
