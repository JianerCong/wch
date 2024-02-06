/**
 * @file forCnsss.hpp
 * @brief Something shared between different consensus components.
 */
#pragma once
#include "core.hpp"

namespace weak{

  /**
   * @brief The Blk that is passed around between nodes during consensus.
   *
   * During the process of consensus, `BlkForConsensus` is passed around instead
   * of the actual `Blk`.The difference is that the `Blk` contains the actual
   * transactions `vector<Tx>`, while `BlkForConsensus` contains the hashes of
   * the
   */
  class BlkForConsensus : public BlkHeader,
                          virtual public IJsonizable,
                          virtual public ISerializable{
  public:
    vector<hash256> txhs;
    BlkForConsensus() = default;
    BlkForConsensus(const uint64_t n,const hash256 p,vector<hash256> ttxhs):
      BlkHeader(n,p), txhs(ttxhs){
      BOOST_LOG_TRIVIAL(info)
        << format("Making BlkForConsensus-%d,\n\tparentHash=%s,\n\ttx size=%d")
        % number % parentHash % txhs.size();

    }

    hash256 hash() const noexcept override {
      // Calculate hash based on the hashes of txs
      // TODO: 🦜 For now, we use serial hashing, later should be changed to Merkle tree.
      hash256 h = this->parentHash;
      static uint8_t s[64];     // a buffer for hashing

      for (const hash256& h1 : txhs){
        // BOOST_LOG_TRIVIAL(debug) << format("Hashing for tx-%d") % tx.nonce;
        std::copy_n(std::begin(h1.bytes),32,s);
        std::copy_n(std::begin(h.bytes),32,s + 32);
        // s[:32] = tx.hash; s[32:] = h
        h = ethash::keccak256(reinterpret_cast<uint8_t*>(s),64);
        // BOOST_LOG_TRIVIAL(debug) << format("Now hash is %s") % hashToString(h);
      }
      return h;
    }

    /**
     * @brief Convert this to an actual Blk.
     *
     * @param p The source to fetch `Tx` from according to the hashes in
     * `this->txhs`.
     */
    optional<Blk> toBlk(IByHashTxGettable * const p){
      BOOST_LOG_TRIVIAL(info) << format("Converting BlkForConsensus to Blk");
      vector<Tx> txs;
      txs.reserve(this->txhs.size()); // reserve size

      for (hash256 & txh : this->txhs){
        optional<Tx> r = p->getTxByHash(txh);
        if (not r){
          BOOST_LOG_TRIVIAL(error) <<
            format("❌️ Tx with hash=\n\t" S_RED "%s" " \n\t\tnot found" S_NOR " in pool")
            % txh;
          return {};
        }
        txs.push_back(r.value());
      }
      return Blk(this->number,this->parentHash,txs);
    }

    // For now, use UTF8 JSON for serialization. later we can change it to other
    string toString() const noexcept override {
      return IJsonizable::toJsonString();
    };

    bool fromString(string_view s) noexcept override{
      BOOST_LOG_TRIVIAL(debug) <<  "Forming BlkForConsensus from string.";
      return IJsonizable::fromJsonString(s);
    };

    json::value toJson() const noexcept override {
      return json::value_from(*this);
    };

    bool fromJson(const json::value &v) noexcept override {

      // Parse number
      BOOST_LOG_TRIVIAL(debug) << format("Forming Blk from Json");
      if (not BlkHeader::fromJson(v)) return false;
      try {
        // json::object const& o = v.as_object();
        vector<string> txhs_str = value_to<vector<string>>(v.at("txhs"));

        this->txhs.clear();
        std::ranges::transform(txhs_str,std::back_inserter(this->txhs),
                               [](string s) -> hash256 {
                                 return evmc::from_hex<hash256>(s).value();
                               });

        // boost::json knows about vector
      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }

      BOOST_LOG_TRIVIAL(debug) << format("🐸 BlkForConsensus-%d fromed from Json") % this->number;
      return true;
    };

  };

  // json functions for Blk
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, BlkForConsensus const& b ){
    jv = json::value_from((const BlkHeader &) b);
    jv.as_object()["txhs"] = json::value_from(b.txhs);
    /* 🦜 : boost:json knows about std::vector, they turn it into array. We have
       tought boost::json how to convert hash in core.hpp
    */
  };

  /**
   * @brief The interface of Consensus exposed to sealer.
   *
   * The the sealer will seal a Blk and pass it to the consensus if the current
   * node is primary.
   *
   * 🦜: These should be thread-safe. The postBlk() will wait until the Blk is
   * committed by the cluster, so it should be a long process.
   */
  class IForSealerBlkPostable{
  public:
    virtual bool postBlk(const BlkForConsensus & b) noexcept=0;
    virtual bool isPrimary() noexcept=0;
  };

}
