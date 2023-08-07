#pragma once
#include "core.hpp"
#include <thread>
#include "forCnsss.hpp"


namespace weak{



  /**
   * @brief The sealer
   *
   * This will keeps draw Txs from the IForSealerTxGettable source and seal a
   * Blk if the the current node is primary.
   */
  struct Sealer {

    IForSealerTxHashesGettable * const mempool; // <! The source of hashes
    IForSealerBlkPostable * const consensus;    // <! The consensus to post Blk
    std::atomic_flag running;   // <! The atomic-bool, default init to false
                                // (C++20)

    hash256 previous_hash;    // <! The hash of previous Blk. This is updated
                              // every time a new Blk is sealed.
    uint64_t next_blk_number;   // <! The next Blk number. This can be provided
                                // in the constructor (default to 0). The number
                                // is incremented every time a new Blk is
                                // sealed.

    int wait_s;                 // <! The time interval between sealing.(default
                                // to 0).

    /**
     * @brief Construct a sealer
     *
     * @param c The consensus interface that sealer will post Blk in.
     *
     * @param m The pool interface that sealer will draw Txs from.
     *
     * @param n The next Blk number. Default to 0. If the program is restarted.
     * The number stored on the persistant DB should be used. This is usually
     * stored by the execManager.
     *
     * 🦜 ⚠️ The execManager stores the "current block number". To get the next,
     * you usually need to increment it by one.
     *
     * @param h The hash of Blk-n. This will be the parent hash of the next
     * sealed Blk;
     *
     * @param s The sealing interval in seconds. So the sealer will check the
     * pool and seal Blk every `s` second(s).
     *
     * @see execManager()
     */
    Sealer(IForSealerBlkPostable * const c,
           IForSealerTxHashesGettable * const m,
           uint64_t n = 0,
           hash256 h = {},
           int s = 2
           ): mempool(m), consensus(c),next_blk_number(n),previous_hash(h),
              wait_s(2)         // By default, seal every 2 sec
    {
      // Start the job
      this->running.test_and_set(); // set the flag to TRUE

      std::thread{std::bind(&Sealer::run, this)}.detach();
    }

    void check_and_seal_maybe(){

      vector<hash256> txhs = this->mempool->getTxHashesForSeal();
      if (txhs.empty()){
        BOOST_LOG_TRIVIAL(debug) << format(S_MAGENTA "Pool Clean" S_NOR);
        return;
      }

      // make Blk and push
      BlkForConsensus b{this->next_blk_number++,
                        this->previous_hash,
                        txhs};
      this->previous_hash = b.hash;

      this->consensus->postBlk(b);     // this is a long process
    }

    // using namespace std::chrono_literals;
    void run(){
      BOOST_LOG_TRIVIAL(debug) << format("🦜 sealer running");
      while (this->running.test()){ // while running == true
        std::this_thread::sleep_for(std::chrono::seconds(this->wait_s));
        // check primary
        BOOST_LOG_TRIVIAL(debug) << format(S_MAGENTA
                                           "Try Sealing"
                                           S_NOR);
        if (this->consensus->isPrimary()){
          check_and_seal_maybe();
        }
      }
    }

    ~Sealer(){
      // finish the job
      BOOST_LOG_TRIVIAL(info) << format("👋 Sealer's done");
      this->running.clear();    // running = false
    }
  };
}
