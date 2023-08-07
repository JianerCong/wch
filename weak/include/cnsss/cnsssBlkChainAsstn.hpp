#pragma once
/**
 * @brief The consensus block chain assistant.
 *
 * This bridge the blockchain world and the pure consensus world.
 */

#include "pure-forCnsss.hpp"    // ICnsssPrimaryBased
#include "forCnsss.hpp"         // IForSealerBlkPostable
#include "exeForCnsss.hpp"      // ExecutorForCnsss 🦜: We need to know its command
#include "core.hpp"             // IForRpcTxsAddable


/*
 * 🦜 : Above is an important moment. It's a bridge between ::weak and
 * ::pure. This is not usual.
 */
namespace weak{

  /**
   * @brief The consensus blockchain assistant.
   *
   * This should be the gateway between the pure consensus and the blockchain world.
   */
  class CnsssBlkChainAsstn: public virtual IForSealerBlkPostable
                          ,
                            public virtual IForRpcTxsAddable{
  public:
    ::pure::ICnsssPrimaryBased * const cnsss;
    CnsssBlkChainAsstn(::pure::ICnsssPrimaryBased * const c): cnsss(c){}

    // the cmds set
    static const char EXECUTE_BLK = static_cast<char>(ExecutorForCnsss::Cmd::EXECUTE_BLK);
    static const char ADD_TXS = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS);

    bool addTxs(vector<Tx> && txs) noexcept override{
      return bool(cnsss->handle_execute("CLIENT", /*the sender, should be irrelevent*/
                                        ADD_TXS + Tx::serialize_from_array(txs)));
    }

    bool postBlk(const BlkForConsensus & b) noexcept override{
      return bool(cnsss->handle_execute("SEALER" /* the sender , should be irrelevent*/
                                        , EXECUTE_BLK + b.toString()));
    }

    bool isPrimary() noexcept override{
      return cnsss->is_primary();
    }

    string info() noexcept override{
      json::value jv = {
        {"consensus_status" ,"Okay"}
      };
      return json::serialize(jv);
    }

  };
}
