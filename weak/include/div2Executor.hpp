/**
 * @file div2Executor.hpp
 * @author Jianer Cong
 * @brief The Diverse2Executor and colleagues.
 *
 * 🦜 : This executor extends the EvmExecutor to support the new Tx types. 2 means
 * it's kinda newer than the EvmExecutor
 */

#pragma once
#include "evmExecutor.hpp"

namespace weak{
  class Div2Executor : public EvmExecutor{
  public:
    optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                         const Tx & t) const noexcept override{

      // 🦜 : Here we switch on the type of the Tx
      switch(t.type){
      case Tx::Type::data:
        return executeDataTx(w,t);
      case Tx::Type::evm:
      default:
        return EvmExecutor::executeTx(w,t);
      };

      // 🦜 : This should never happen
      return {};
    }

    /**
     * @brief Execute a data Tx.
     *
     * 🦜 : Just say yes and do nothing.
     */
    optional<tuple<vector<StateChange>,bytes>> executeDataTx(IAcnGettable * const w, const Tx & t) const noexcept{
      vector<StateChange> s = {};
      bytes r = {};
      return make_tuple(s,r);
    }

  };
}
