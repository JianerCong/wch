/**
 * @file exeForCnsss.hpp
 * @brief The executor that execute Blk Chain data.
 */
#pragma once
#include "core.hpp"
#include "pure-forCnsss.hpp"
#include "forCnsss.hpp"
#include "forPostExec.hpp"

namespace weak {
  /**
   * @brief The Block chain command executor.
   *
   * 🦜 :This currently support two commands:
   *
   * 1. EXECUTE_BLK(bh): This will accept a `BlkForConsensus` and execute it. In
   * detail, it does the following:
   *     I.   Consult the underlying mempool interface and construct the `Blk`.
   *
   *     II.  Execute the `Blk` using the Blk Executor.
   *
   *     III. Commit the `Blk`
   *
   * 2. ADD_TXS(txs): This will accept an array of `Tx` and add them to the
   *     underlying pool. These `Tx` should have been verified by the local pool
   *     once, before being boardcast into the cluster.
   *
   */

  class ExecutorForCnsss : public virtual ::pure::IForConsensusExecutable{
    IBlkExecutable * const exe;
    IPoolSettable * const pool;
  public:
    ExecutorForCnsss(IBlkExecutable * const e,
                     IPoolSettable * const p): exe(e),pool(p){};

    // The commands
    enum class Cmd: char{
      EXECUTE_BLK = 'b',
      ADD_TXS = 't'
    };

    /**
     * @brief Execute a Block chain command
     *
     * 🦜 : Here we kinda need a "protocol" to define the structure of the
     * command `cmd` right?
     *
     * 🐢 : Why don't we just use the first byte (char) as our "function
     * switcher" and then the argument follows ?
     *
     * 🦜 : That's fine, but it only allows each command to have one argument
     * right?
     *
     * 🐢 : Let's shift those jobs to other modules. Let's use an enum for now.
     * So in detail: `cmd` has the following stucture:
     *   <switcher[one_byte]><others>
     *
     * Specifically:
     *  + ADD_TXS(txs) : <switcher><serialized array of txs to be added.>
     *  + EXECUTE_BLK(bh): <switcher><serialized BlkForConsensus>
     */
    string execute(string & cmd) noexcept override{
      if (cmd.empty()) return "";
      Cmd c{cmd.at(0)};

      switch (c){
      case Cmd::EXECUTE_BLK:
        return handle_EXECUTE_BLK(cmd);
      case Cmd::ADD_TXS:
        return handle_ADD_TXS(cmd);
      }

      return clear_cmd_and_complain(cmd,
                                    (format("Unknown Command Prefix [%c], valid values are: [%c], [%c]")
                                     % cmd.at(0)
                                     % static_cast<char>(Cmd::EXECUTE_BLK)
                                     % static_cast<char>(Cmd::ADD_TXS)
                                     ).str()
                                    );
    };

    static string clear_cmd_and_complain(string & cmd, const string & reason){
      BOOST_LOG_TRIVIAL(error) << format(S_RED "❌️ %s, emptying cmd" S_NOR) % reason;
      cmd.clear();
      return reason;
    }

    virtual string handle_EXECUTE_BLK(string & cmd) noexcept {
      string arg = cmd.substr(1);
      BOOST_LOG_TRIVIAL(debug) << format("⚙️ Handling " S_CYAN "EXECUTE_BLK()" S_NOR ",data to parse:\n\t"
                                         S_CYAN "%s" S_NOR) % pure::get_data_for_log(arg);
      BlkForConsensus b;
      if (not b.fromString(arg))
        return clear_cmd_and_complain(cmd, "Error parsing BlkForConsensus");

      IByHashTxGettable * const ph = dynamic_cast<IByHashTxGettable*>(this->pool);
      auto r = b.toBlk(ph);

      if (not r)
        return clear_cmd_and_complain(cmd,"Failed to turn BlkForConsensus into Blk");

      if (not execute_Blk(r.value()))
        return clear_cmd_and_complain(cmd,"Error committing Blk");

      return "OK";
    }

    /**
     * @brief Execute and commit a Blk.
     *
     * This will make use of the underlying `IBlkExecutable`
     */
    bool execute_Blk(const Blk & b){
      BOOST_LOG_TRIVIAL(debug) << format("⚙️ Calling underlying IBlkExecutable");
      return exe->commitBlk(exe->executeBlk(b));
    }

    /**
     * @brief Handle the ADD_TXS() cmd(request). If anything went wrong during
     * parsing, we empty the `cmd` string so that the rest of the cluster won't
     * need to repeat it.
     */
    virtual string handle_ADD_TXS(string & cmd) noexcept {
      try{
        string arg = cmd.substr(1);
        BOOST_LOG_TRIVIAL(debug) << format("⚙️ Handling " S_CYAN "ADD_TXS" S_NOR ", data to parse:\n\t"
                                           S_CYAN "%s" S_NOR) % pure::get_data_for_log(arg);

        vector<Tx> txs = Tx::parse_to_array(arg);
        int n = txs.size();     // original size

        BOOST_LOG_TRIVIAL(debug) << format("Adding parsed txs:");
        for (vector<Tx>::iterator it = txs.begin();it != txs.end();){
          if (not this->pool->addTx(*it)){
            BOOST_LOG_TRIVIAL(trace) << format("❌️ failed to add tx-%d") % it->nonce;
            it = txs.erase(it);
          }else{
            it++;
          }
        }

        // BOOST_LOG_TRIVIAL(trace) << format("⚙️ Afterwards txs.size()="
        //                                    S_BLUE "%d" S_NOR " N=" S_GREEN "%d" S_NOR)
        //   % txs.size() % n;

        if (txs.empty())
          return clear_cmd_and_complain(cmd,"All txs rejected");
        if (txs.size() < n){
          BOOST_LOG_TRIVIAL(debug) << format("⚙️ Removing some of the txs. size reduced from "
                                             S_BLUE "%d" S_NOR " to " S_GREEN "%d" S_NOR)
            % n % txs.size();
          arg = Tx::serialize_from_array(txs);
          cmd = static_cast<char>(Cmd::ADD_TXS) + arg;
          // remove bad txs from the cmd.
        }
      }catch(const std::exception & e){
        return clear_cmd_and_complain(cmd,"Error parsing Txs");
      }

      return "OK";
    }

  };

  /**
   * @brief The Block chain command executor optimized for speed.
   *
   * 🐢 : This is executor designed to be faster than its super class by
   * overriding the handle_ADD_TXS() method. The optimization logic is the following:
   *
   * Conventionally, the ADD_TXS() command is
   *
   *    1. executed by primary first,
   *
   *    2. modified by the primary (this removes the invalid Tx from the batch
   *    and stop them from consuming the resource of the whole cluster.)
   *
   *    3. and then boardcast to the cluster.
   *
   * In LightExecutorForCnsss, we embed the sealer into ADD_TXS(). So every time
   * the primary receives the ADD_TXS():
   *
   *    1. It verifies which of the received Txs are valid by dropping then into
   *    the pool, and poping them (some of them) out of the pool.
   *
   *    2. It then seals a Blk for these valid Txs.(🦜 : Oh, then this Executor
   *    needs to know "What Blk-number is it now?" right? 🐢 : Yeah).
   *
   *    3. It execute the Blk. (This can be lauched in another thread)
   *
   *    4. Then, it modifies the `cmd` from `ADD_TXS` to `EXECUTE_BLK`, with the
   *    payload equal to ehe Blk just sealed. Note that this time, the Blk is a
   *    whole Blk which contains the Txs, not `BlkForConsensus` which contains
   *    only the Tx-hashes. (🦜 : So the handle_EXECUTE_BLK() needs to be
   *    modified too right? 🐢 : Yeah, currently it handles BlkForConsensus)
   *
   *    5. Then it boardcast the modified command.
   *
   * 🦜 : So the LightExecutorForCnsss and its super class are not compatible.
   *
   * 🐢 : Yeah.
   *
   * 🦜 : It feels like step 1 can also be improved ?
   *
   * 🐢 : Yeah, if the `optimization_level` is set to be >= 2, then it assumes
   * that the Txs verification has been done in the upper layer of the
   * application stack (i.e. it skips the verification with pool).
   *
   * 🦜 : So if `optimization_level` is >= 2 then `IPoolSettable` and
   * `IForSealerTxHashesGettable` parameter in the ctor can be set to nullptr
   * right?
   *
   * 🐢 : Yes.
   */
  class LightExecutorForCnsss: public virtual ExecutorForCnsss{
  public:
    IForSealerTxHashesGettable * const mempool;
    uint64_t next_blk_number;
    hash256 previous_hash;
    const int optimization_level;

    /**
     * @brief ExecutorForCnsss optimized for speed.
     *
     * @param e The Blk executor
     * @param p The pool interface to push Tx in
     * @param m The pool interface to pop Tx out
     * @param o The optimization_level, max=2 (default)
     * @param n The previous Blk number
     * @param h The previous Blk hash
     */
    LightExecutorForCnsss(IBlkExecutable * const e,
                          IPoolSettable * const p,
                          IForSealerTxHashesGettable * const m,
                          int o = 2,
                          uint64_t n = 0,
                          hash256 h = {}
                          ): ExecutorForCnsss(e,p),
                             next_blk_number(n), mempool(m), previous_hash(h),optimization_level(o)
    {}

    string handle_EXECUTE_BLK(string & cmd) noexcept override{
      string_view arg( cmd.cbegin() + 1, cmd.cend());
      BOOST_LOG_TRIVIAL(debug) << format("⚙️ Handling " S_CYAN "EXECUTE_BLK()" S_NOR ",data to parse:\n\t"
                                         S_CYAN "%s" S_NOR) % pure::get_data_for_log(arg);
      Blk b;
      if (not b.fromString(arg))
        return clear_cmd_and_complain(cmd, "Error parsing Blk");

      if (not execute_Blk(b))
        return clear_cmd_and_complain(cmd,"Error committing Blk");

      return "OK";
    };

    /**
     * @brief Let the txs to go through the pool.
     *
     * This should modifies the txs so that it only contains valid Tx.
     */
    void verify_txs_and_filter_maybe(vector<Tx> & txs){
      BOOST_LOG_TRIVIAL(warning) << format("⚠️ verify_txs_and_filter_maybe called but not implemented yet");
    }

    string  seal_Blk_and_update_cmd(vector<Tx> && txs, string & cmd){
      Blk b{this->next_blk_number,
            this->previous_hash,
            txs};


      if (not execute_Blk(b))
        return clear_cmd_and_complain(cmd,"Error committing Blk");

      // 🦜 : Looks like it's an okay blk, so we update the state.
      this->next_blk_number++;
      this->previous_hash = b.hash();

      BOOST_LOG_TRIVIAL(debug) << format("Making " S_MAGENTA " EXECUTE_BLK() " S_NOR " cmd for "
                                         S_CYAN " Blk-%d, txs size: %d" S_NOR)
        % b.number % b.txs.size();

      cmd = static_cast<char>(Cmd::EXECUTE_BLK) + b.toString();
      BOOST_LOG_TRIVIAL(debug) << format("cmd updated to : " S_MAGENTA " %s" S_NOR) % pure::get_data_for_log(cmd);

      return "OK";
    }

    string handle_ADD_TXS(string & cmd) noexcept override{
      try{
        string_view arg(cmd.cbegin()+1,cmd.cend());
        BOOST_LOG_TRIVIAL(debug) << format("⚙️ Handling " S_CYAN "ADD_TXS" S_NOR ", data to parse:\n\t"
                                           S_CYAN "%s" S_NOR) % pure::get_data_for_log(arg);
        vector<Tx> txs = Tx::parse_to_array(arg);
        if (this->optimization_level < 2){
          verify_txs_and_filter_maybe(txs);
        }
        // <2024-03-26 Tue> 🦜 : It turns out that pb is very kind about parsing
        // error. It seems like it simply returns an empty array when it
        // fails..... So let's check the size of the txs.
        if (txs.empty())
          BOOST_THROW_EXCEPTION(std::runtime_error("Parsing txs results in a zero-length array."));

        return seal_Blk_and_update_cmd(move(txs),cmd);
      }catch(const std::exception & e){
        return clear_cmd_and_complain(cmd,"Error parsing Txs" + string(e.what()));
      }

      return "OK";
    }
  };                            // class LightExecutorForCnsss
} // namespace weak
