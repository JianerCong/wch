#pragma once
#include "core.hpp"
#include "forPostExec.hpp"
#include "txVerifier.hpp"
#include <boost/numeric/conversion/cast.hpp>
#include <boost/lexical_cast.hpp>


namespace weak{

  /**
   * @brief a naive type that implements IBlkExecutable
   *
   * 🦜 : This executor just do as few as possible to implement the
   * IBlkExecutable.
   *
   * For executeBlk():
   *   for each tx in Blk:
   *      1. it executes it,
   *      2. gets the TxReceipt and StateChanges for this tx
   *      3. append them in two vectors `txReceipts` and `stateChanges`
   *      4. use these to make the executed blk and return.
   *
   * For commitBlk():
   *   for each tx in ExecBlk:
   *      3. It makes a `TxOnBlkInfo` for this tx, and store it in chaindb with key = "/tx/<tx.hash>"
   *      2. applies the journal to the stateDB
   *   It then saves the ExecBlk in chaindb with key = "/blk/<blk.number>".
   *
   *   Also, it saves the block number in chaindb with key =
   *   "/other/blk_number".
   *
   */
  class BlkExecutor: public virtual IBlkExecutable{
  public:

    IWorldChainStateSettable* const world;
    ITxExecutable* const txExecutor;
    IAcnGettable* const readOnlyWorld;
    ITxVerifiable * const txVerifier;

    BlkExecutor(IWorldChainStateSettable* const w,
                ITxExecutable* const e,
                IAcnGettable* const r,
                ITxVerifiable * const v = nullptr
                ): world(w),
                   txExecutor(e),
                   readOnlyWorld(r),
                    txVerifier(v)
    {};

    ExecBlk executeBlk(Blk && b) const noexcept override{
      BOOST_LOG_TRIVIAL(warning) << format("Executing " S_CYAN "blk-%d" S_NOR) % b.number;

      // <2024-04-07 Sun> 🦜 : We filter out the failed txs
      if (this->txVerifier)
        this->txVerifier->filterTxs(b.txs);

      // The results
      vector<vector<StateChange>> J;
      vector<TxReceipt> R;

      for (const Tx & t : b.txs){
        optional<tuple<vector<StateChange>,bytes>> r =
          this->txExecutor->executeTx(this->readOnlyWorld,t);
        if (not r){
          BOOST_LOG_TRIVIAL(debug) << format("❌️ Failed to execute " S_RED "tx-%d" S_NOR
                                             " [this is usually contract-author's fault]") % t.nonce;
          J.push_back({});
          R.push_back(TxReceipt(false,t.type)); // failed Tx
        }else{
          auto [j /* formed journal*/
                ,b /*result bytes*/] = r.value();
          BOOST_LOG_TRIVIAL(debug) << format("🐸 Tx-" S_CYAN "%d" S_NOR " executed") % t.nonce;
          J.push_back(j);
          R.push_back(TxReceipt(b,t.type)); /*Implicitly call TxReceipt(bytes)*/
        }
      }

      // --------------------------------------------------

      return ExecBlk(b,J,R);
    }

    bool commitBlk(const ExecBlk& b) noexcept override {
      using boost::numeric_cast;
      using boost::lexical_cast;
      string k;
      bool ok;
      for (int i = 0; i<b.txs.size();i++){
        TxOnBlkInfo bi{b.number,numeric_cast<uint64_t>(i) /*the position of tx on blk*/};
        const Tx & tx = b.txs[i];

        // Save the TxOnBlkInfo
        // k = (format("/tx/%s") % hashToString(tx.hash)).str();
        k = "/tx/" + hashToString(tx.hash());
        ok = world->setInChainDB(k, bi.toString());


        // Apply the journal
        ok = world->applyJournalStateDB(b.stateChanges[i]);
        if(not ok){
          BOOST_LOG_TRIVIAL(error) << format( S_RED "❌️ Error applying journal of tx-%d at location %d" S_NOR) %
            tx.nonce % i;
          return false;
        }
      }

      // save the (executed) blk
      /*
        🦜 : So, as long as we have all the "ExecBlk", everything can replay * 3
       */
      // k = (format("/blk/%d") % b.number).str();
      string bn = lexical_cast<string>(b.number);
      k = "/blk/" + bn;
      ok = world->setInChainDB(k,b.toString());
      if (not ok){
        BOOST_LOG_TRIVIAL(error) << format( S_RED "❌️ Error setting chaindb k=%s" S_NOR) % k;
        return false;
      }

      ok = world->setInChainDB("/other/blk_number",bn);
      if (not ok){
        BOOST_LOG_TRIVIAL(error) << format( S_RED "❌️ Error setting blk number on chaindb" S_NOR) % k;
        return false;
      }

      return true;
    }
    // ExecBlk executeBlk(const Blk & )
  };
}
