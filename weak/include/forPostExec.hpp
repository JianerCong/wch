/**
 * @file forPostExec.hpp
 * @author Jianer Cong
 * @brief classes used after execution is finished
 */
#pragma once
#include "core.hpp"
namespace weak{

  /*
    The json conversion for StateChange, we need to save that
   */
  // json functions for StateChange
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& v, StateChange const& a ){
    v = {
      {"del",a.del},
        {"k",a.k},
        {"v",a.v}
    };
  }

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  // May throw
  StateChange tag_invoke(json::value_to_tag<StateChange>, json::value const& v ){
    StateChange a;
    a.del = value_to<bool>(v.at("del"));
    a.k = value_to<string>(v.at("k"));
    a.v = value_to<string>(v.at("v"));
    return a;
  }


  // {{{ class TxReceipt

  class TxReceipt: virtual public IJsonizable
                 , virtual public ISerializableInPb<hiPb::TxReceipt>
                 , virtual public ISerializable{
  public:
    void fromPb(const hiPb::TxReceipt & pb) override {
      this->ok = pb.ok();
      this->result = weak::bytesFromString(pb.result());
    }

    hiPb::TxReceipt toPb() const override {
      hiPb::TxReceipt pb;
      pb.set_ok(this->ok);
      pb.set_result(weak::toString(this->result));
      return pb;
    }


    TxReceipt() = default;
    explicit TxReceipt(bool o): ok(o){
      if (o) throw std::invalid_argument("This constructor is used for Tx that failed to be executed.");
    };
    TxReceipt(bytes r): result(r){};
    bool ok{true};
    bytes result;

    bool fromJson(const json::value &v) noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("Forming TxOnBlkInfo from Json");
      try {
        // json::object const& o = v.as_object();
        BOOST_LOG_TRIVIAL(trace) << format("parsing exec-ok");
        this->ok = value_to<bool>(v.at("ok"));
        if (ok){
          BOOST_LOG_TRIVIAL(trace) << format("parsing result");
          string s = value_to<string>(v.at("result"));
          this->result = evmc::from_hex(s).value();
        }
      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }

      BOOST_LOG_TRIVIAL(debug) << format("🐸 TxReceipt formed from Json");
      return true;
    }
    json::value toJson() const noexcept override {
      return json::value_from(*this);
    }

    // For now, use UTF8 JSON for serialization. later we can change it to other
    string toString() const noexcept override {
      return IJsonizable::toJsonString();
    };
    bool fromString(string_view s) noexcept override{
      BOOST_LOG_TRIVIAL(debug) <<  "Forming Tx from string.";
      return IJsonizable::fromJsonString(s);
    };
  };

  // json functions for TxReceipt
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, TxReceipt const& c ){
    jv = {
      {"ok", c.ok},
      {"result", evmc::hex(c.result)},
    };
  };




  // TxReceipt tag_invoke(json::value_to_tag<TxReceipt>, json::value const& v){
  //   TxReceipt t;
  //   if (t.fromJson(v)) return t;
  //   return {};
  // };

  ADD_FROM_JSON_TAG_INVOKE(TxReceipt);

  // }}}

  // {{{ class TxOnBlkInfo
  /**
   * @brief The block-related information for a Tx.
   * This information is generated after the tx is sealed into a block.
   *
   * 🦜 : Yeah, otherwise, given a tx, we won't be able to find it in the
   * blockchain.
   *
   * 🐢 : This should goes to chaindb I guess.
   *
   * 🦜 : Yeah, along with txReceipt and all the block data.
   */
    class TxOnBlkInfo: virtual public IJsonizable
                     , virtual public ISerializable{
    public:
      TxOnBlkInfo() = default;
      TxOnBlkInfo(uint64_t n,uint64_t i): blkNumber(n), onBlkId(i){}
      uint64_t blkNumber;
      uint64_t onBlkId;

      bool fromJson(const json::value &v) noexcept override {
        BOOST_LOG_TRIVIAL(debug) << format("Forming TxOnBlkInfo from Json");
        try {
          json::object const& o = v.as_object();
          BOOST_LOG_TRIVIAL(trace) << format("parsing blkNumber");
          this->blkNumber = value_to<uint64_t>(o.at("blkNumber"));
          BOOST_LOG_TRIVIAL(trace) << format("parsing onBlkId");
          this->onBlkId = value_to<uint64_t>(o.at("onBlkId"));

        }catch (std::exception &e){
          BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
          return false;
        }

        BOOST_LOG_TRIVIAL(debug) << format("🐸 TxOnBlkInfo[%d-%d] formed from Json") %
          this->blkNumber % this->onBlkId;
        return true;
      }
      json::value toJson() const noexcept override {
        return json::value_from(*this);
      }


      // For now, use UTF8 JSON for serialization. later we can change it to other
      string toString() const noexcept override {
        return IJsonizable::toJsonString();
      };
      bool fromString(string_view s) noexcept override{
        BOOST_LOG_TRIVIAL(debug) <<  "Forming Tx from string.";
        return IJsonizable::fromJsonString(s);
      };

    };


  // json functions for TxOnBlkInfo
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, TxOnBlkInfo const& c ){
    jv = {
      {"blkNumber", c.blkNumber},
      {"onBlkId", c.onBlkId},
    };
  }

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  // TxOnBlkInfo tag_invoke(json::value_to_tag<TxOnBlkInfo>, json::value const& v ) noexcept{
  //   TxOnBlkInfo t;
  //   if (t.fromJson(v)) return t;
  //   return {};
  // }
  ADD_FROM_JSON_TAG_INVOKE(TxOnBlkInfo);

  // }}}

  /**
   * @brief The class representing an executed blk.
   *
   * For each tx in the blks, there's a corresponding entry in `stateChanges` (the journals)
   * and `txReceipts` (the results).
   */
  class ExecBlk: virtual public IJsonizable
                   , virtual public ISerializable
                   , public Blk{
  public:
    vector<vector<StateChange>> stateChanges;
    vector<TxReceipt> txReceipts;
    ExecBlk() = default;
    // TODO: 🦜 add move()  magic here
    ExecBlk(Blk b,
                vector<vector<StateChange>> j,
                vector<TxReceipt> r): Blk(b)
                                    ,stateChanges(j),txReceipts(r){
      if (b.txs.size() != j.size())
        throw std::length_error((format("❌️ txs size != journal size "
                                        S_RED "(%d != %d)" S_NOR
                                        ) % b.txs.size() % j.size()
                                 ).str()
                                );
      if (j.size() != r.size())
        throw std::length_error((format("❌️ receipt size != journal size "
                                        S_RED "(%d != %d)" S_NOR
                                        ) % r.size() % j.size()
                                 ).str()
                                );
    }

    // For now, use UTF8 JSON for serialization. later we can change it to other
    string toString() const noexcept override {return IJsonizable::toJsonString();};
    bool fromString(string_view s) noexcept override{
      BOOST_LOG_TRIVIAL(debug) <<  "Forming ExecBlk from string.";
      return IJsonizable::fromJsonString(s);
    };
    json::value toJson() const noexcept override {return json::value_from(*this);};
    bool fromJson(const json::value &v) noexcept override {

      // Parse number
      BOOST_LOG_TRIVIAL(debug) << format("Forming ExecBlk from Json");
      try {
        // json::object const& o = v.as_object();
        if (not Blk::fromJson(v))
          return false;

        BOOST_LOG_TRIVIAL(info) << format("Parsing stateChanges");
        this->stateChanges = value_to<vector<vector<StateChange>>>(v.at("stateChanges"));
        BOOST_LOG_TRIVIAL(info) << format("Parsing txReceipts");
        this->txReceipts = value_to<vector<TxReceipt>>(v.at("txReceipts"));
        // boost::json knows about vector

      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }

      BOOST_LOG_TRIVIAL(debug) << format("🐸 ExecBlk-%d fromed from Json") % this->number;
      return true;
    };
  };

  // json functions for ExecBlk
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, ExecBlk const& c ){
    BOOST_LOG_TRIVIAL(debug) << format("Forming json in the upper Blk");
    // The Blk part
    jv = json::value_from(dynamic_cast<const Blk&>(c));

    BOOST_LOG_TRIVIAL(debug) << format("Adding stateChanges");
    // Add the additional two fields
    jv.as_object().emplace("stateChanges",json::value_from(c.stateChanges));
    BOOST_LOG_TRIVIAL(debug) << format("Adding txReceipts");
    jv.as_object().emplace("txReceipts",json::value_from(c.txReceipts));
  };

  ADD_FROM_JSON_TAG_INVOKE(ExecBlk);

  /**
   * @brief Representing a type that can execute a blk.
   *
   * 🦜 : The consensus will talk to this instead of the storage and executor
   * underneath. commitBlk() returns true if success. A naive implementation is
   * in `storageManager.hpp`.
   */
  class IBlkExecutable {
  public:
    virtual ExecBlk executeBlk(const Blk & b) const noexcept = 0;
    virtual bool commitBlk(const ExecBlk & b) noexcept = 0;
  };

}
