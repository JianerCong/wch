#pragma once

#include "core.hpp"
#include <set>
#include <unordered_set>


#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#include <mutex>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
// #include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

namespace weak{

  namespace multi_index = boost::multi_index;
  using multi_index::ordered_unique;
  using multi_index::identity;

  using multi_index::const_mem_fun;
  using boost::multi_index::identity;
  // using multi_index::member;
  using multi_index::ordered_non_unique;
  using multi_index::multi_index_container;
  using multi_index::indexed_by;

  /**
   * @brief The mempool type
   *
   * It stores Tx and sort them in the following order:
   * 1. by timestamp (earlier)
   * 2. by address (smaller)
   * 3. by nonce (smaller)
   *
   * You can also call `getTxByHash()` on it. The underlying multi-indexed
   * container should be able to make the speed not too slow (logn instead of
   * linear).
   *
   * 🦜 : `IPoolSettable` inherits `IByHashTxGettable`
   */
  class Mempool: public virtual IForSealerTxHashesGettable,
                 public virtual IPoolSettable,
                 public virtual IForRpc
  {
  public:
    ~Mempool(){
      BOOST_LOG_TRIVIAL(debug) << format("👋 pool closed");
    }
    /* [2024-01-24]
       🦜 : What's this ?

       🐢 : It's just a set. But it allows us to fetch an element by its hash.
       and sometimes by Tx's own operator<, which is sorted first by timestamp,
       then by the pair (addr,nonce).
     */
    typedef multi_index_container<
    Tx,
    indexed_by<
      // sort by Tx::operator<
      ordered_unique<identity<Tx>>,
      // sort by less<hash256> on Tx::
      // 🦜: We defined it in core.hpp.
      // ordered_unique<member<Tx,ethash::hash256,&Tx::hash>>
      ordered_unique<const_mem_fun<Tx,ethash::hash256 ,&Tx::hash>>
      >
    > Tx_set;

    Tx_set txs;                 // 🦜 : Boost allows us to use it as in
                                // STL.(friendly)
    const int max_txs_per_batch{2};
    mutable std::mutex lock_for_txs; // The "M&M rule": mutable and mutex go together
    Tx_set::nth_index<1>::type& txhs; // the hash-sorted set

    // using Hash_set = std::set<hash256>;
    using Hash_set = std::unordered_set<hash256>;
    unique_ptr<Hash_set> hs; //<! The tx hashes of all txs, including
    mutable std::mutex lock_for_hs; // The "M&M rule": mutable and mutex go together

    //those executed and popped from the pool.
    //This containter will only grow,never shrink.

    /**
     * @brief The constructor of the pool.
     *
     * @param hhs The hash of previously executed Tx. The hash is calculated
     * based on (Tx.from, Tx.nonce).
     * @See Tx()
     */
    Mempool(unique_ptr<Hash_set> && hhs=make_unique<Hash_set>(),int m=2): hs(move(hhs)),
                                      txhs(txs.get<1>()), // txhs is a view.
                                      max_txs_per_batch(m)
    {
      // BOOST_LOG_TRIVIAL(debug) << format("pool entered");
      BOOST_LOG_TRIVIAL(debug) << format( S_MAGENTA "%d" S_NOR " hashes thrown in the pool 🚮️") % hs->size();
    }

    /**
     * @brief Get a Tx by its hash. This will pop the Tx from the underlying
     * set.
     */
    optional<Tx> getTxByHash(hash256 h) noexcept override{
      std::unique_lock g(this->lock_for_txs); // movable lock
      if (not txhs.contains(h)) return {};
      auto it = txhs.find(h);
      Tx t = *it;
      txhs.erase(it);
      BOOST_LOG_TRIVIAL(debug) << format("🚮️ pop Tx:hash="
                                         S_CYAN "%s" S_NOR
                                         ", now " S_CYAN "%d" S_NOR " left")
        % h % txhs.size();
      return t;
    }

    vector<hash256> getTxHashesForSeal() noexcept override{
      int max = this->max_txs_per_batch;
      // BOOST_TEST_MESSAGE(format("getting tx called").str());
      vector<hash256> txhs_out;
      {std::unique_lock g(this->lock_for_txs); // movable lock
        for (auto it = this->txs.begin();
             it != this->txs.end() and max > 0;max--){
          // BOOST_LOG_TRIVIAL(debug) << format("iter");
          txhs_out.push_back(it->hash());
          ++it;
        }
      }// unlocks here
      return txhs_out;
    }

    /**
     * @brief Verify an Tx.
     */
    bool verifyTx(const Tx & t) const noexcept override{
      std::unique_lock g(this->lock_for_hs); // movable lock
      return not this->hs->contains(t.hash());
    }

    /**
     * @brief Add a tx to the pool
     *
     * @return Wether the tx is added successfully. The tx will only be added if
     * the nonce has not been used.
     */
    bool addTx(const Tx & t) noexcept override{
      std::unique_lock g(this->lock_for_txs); // movable lock
      std::unique_lock g1(this->lock_for_hs); // movable lock
      if (not this->hs->insert(t.hash()).second)
        return false;

      return txs.insert(t).second;

      // // 0.--------------------------------------------------
      // 🦜 : This is equivalent to what's below
      // 🐢 : STL expert you are.
      // // 1. --------------------------------------------------
      // if (txs.contains(t))
      //   return false;

      // txs.insert(t);
      // return true;
    } // unlock here

    string info() noexcept override{
      json::array hash_history;
      {
        std::unique_lock g(this->lock_for_hs); // movable lock
        for (const hash256 & h : *this->hs){
          hash_history.emplace_back(hashToString(h));
        }
      } // unlocks here

      json::array txs_in_pool;
      {
        std::unique_lock g(this->lock_for_txs); // movable lock
        for (const Tx & tx : this->txs){
          txs_in_pool.emplace_back(tx.toJson());
        }
      } // unlocks here

      json::value jv={
        {"max_txs_per_batch", this->max_txs_per_batch},
        {"hash_history", hash_history},
        {"txs_in_pool",txs_in_pool},
        {"txs_in_pool_count",txs_in_pool.size()}
      };
      return json::serialize(jv);
    }
  };
}
