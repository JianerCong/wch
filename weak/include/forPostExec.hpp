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
      {"k",a.k}
      // {"v",a.v}
    };
    // <2024-03-26 Tue> 🦜 it seems like after we have added the pb, the json
    // serialization should be purely for debugging purposes, so we can't just
    // store a.v here, instead, we should store the hex
#if defined(WITH_PROTOBUF)
    v.as_object()["v"] = evmc::hex(weak::bytesFromString(a.v));
    #else
    v.as_object()["v"] = a.v;
#endif
  }

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  // May throw
  StateChange tag_invoke(json::value_to_tag<StateChange>, json::value const& v ){
    StateChange a;
    a.del = value_to<bool>(v.at("del"));
    a.k = value_to<string>(v.at("k"));
    // <2024-03-26 Tue> 🦜 : If we are using protobuf, we should use hex for v,
    // because Acn is serialized to pb.
#if defined(WITH_PROTOBUF)
    a.v = weak::toString(evmc::from_hex(value_to<string>(v.at("v"))).value());
#else
    a.v = value_to<string>(v.at("v"));
#endif
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
      this->type = Tx::typeFromPb(pb.type());
    }

    hiPb::TxReceipt toPb() const override {
      hiPb::TxReceipt pb;
      pb.set_ok(this->ok);
      pb.set_result(weak::toString(this->result));
      pb.set_type(Tx::typeToPb(this->type));
      return pb;
    }


    TxReceipt() = default;
    explicit TxReceipt(bool o, Tx::Type tp = Tx::Type::evm): ok(o), type(tp){
      if (o) throw std::invalid_argument("This constructor is used for Tx that failed to be executed.");
    };
    TxReceipt(bytes r, Tx::Type tp = Tx::Type::evm): result(r), type(tp){};
    bool ok{true};
    bytes result;
    Tx::Type type{Tx::Type::evm};

    bool fromJson(const json::value &v) noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("Forming TxOnBlkInfo from Json");
      try {
        // json::object const& o = v.as_object();
        BOOST_LOG_TRIVIAL(trace) << format("parsing exec-ok");
        this->ok = value_to<bool>(v.at("ok"));
        this->type = v.as_object().contains("type") ? Tx::typeFromString(v.at("type").as_string()) : Tx::Type::evm;
        if (ok){
          BOOST_LOG_TRIVIAL(trace) << format("parsing result");
          /* <2024-03-22 Fri> 🐢 : : If it's python, we serialize it to bytes.
             🦜 : What about data?
             🐢 : They won't have Tx::Receipt.
           */
          if (this->type == Tx::Type::python){
            this->result = weak::bytesFromString(json::serialize(v.at("result")));
          }else{
            string s = value_to<string>(v.at("result"));
            this->result = evmc::from_hex(s).value();
          }
        }
      }catch(std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }

      BOOST_LOG_TRIVIAL(debug) << format("🐸 TxReceipt formed from Json");
      return true;
    }
    json::value toJson() const noexcept override {
      return json::value_from(*this);
    }

    ADD_TO_FROM_STR_WITH_JSON_OR_PB
    /*
      🐢 defines the to/fromString() methods according to WITH_PROTOBUF, so we
      don't need to write the following...
    */

    // // For now, use UTF8 JSON for serialization. later we can change it to other
    // string toString() const noexcept override {
    //   return IJsonizable::toJsonString();
    // };
    // bool fromString(string_view s) noexcept override{
    //   BOOST_LOG_TRIVIAL(debug) <<  "Forming Tx from string.";
    //   return IJsonizable::fromJsonString(s);
    // };

  };

  // json functions for TxReceipt
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, TxReceipt const& c ){
    json::value result;
    // <2024-03-22 Fri> 🦜 : If the tx::type is python, we convert the result to json value
    if (c.type == Tx::Type::python and c.result.size() > 0){
      result = json::value_from(json::parse(weak::toString(c.result)));
    }else{                      // 🦜 : Otherwise, we convert it to hex
      result = evmc::hex(c.result);
    }

    jv = {
      {"ok", c.ok},
      // {"result", evmc::hex(c.result)},
      {"result", result},
      {"type", Tx::typeToString(c.type)}
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
                     , virtual public ISerializableInPb<hiPb::TxOnBlkInfo>
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


      hiPb::TxOnBlkInfo toPb() const override {
        hiPb::TxOnBlkInfo pb;
        pb.set_blknumber(this->blkNumber);
        pb.set_onblkid(this->onBlkId);
        return pb;
      }
      void fromPb(const hiPb::TxOnBlkInfo & pb) override{
        this->blkNumber = pb.blknumber();
        this->onBlkId = pb.onblkid();
      }

      ADD_TO_FROM_STR_WITH_JSON_OR_PB
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
               // , virtual public ISerializableInPb<hiPb::ExecBlk>
  /*🦜 : Nope, because we can't override <T>toPb() using different type, so...
    we kinda have to loose the interface... Next time maybe we should use a
    `has-a` relationship for this kind.

    🐢 : Yeah, ExecBlk already implemented ISerializableInPb<hiPb::Blk> ...
   */
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

    hiPb::ExecBlk toPb0() const {
      hiPb::ExecBlk pb;
      pb.mutable_blk()->CopyFrom(Blk::toPb());
/*
  message StateChange {bool del = 1; bytes key = 2; bytes value = 3;} // []
  message StateChanges {repeated StateChange changes = 1;}            // []
  message ExecBlk {Blk blk = 1; repeated TxReceipt txReceipts = 2; repeated StateChanges stateChanges = 3;} // []
 */

      // add stateChanges
      for (auto & j : this->stateChanges){
        hiPb::StateChanges * psc = pb.add_statechanges();
        for (auto & sc : j){
          hiPb::StateChange * psc0 = psc->add_changes();
          psc0->set_del(sc.del);
          psc0->set_k(sc.k);
          psc0->set_v(sc.v);
        }
      }
      // add txReceipts
      for (auto & r : this->txReceipts){
        pb.add_txreceipts()->CopyFrom(r.toPb());
      }
      return pb;
    }

    void fromPb0(const hiPb::ExecBlk & pb) {
      Blk::fromPb(pb.blk());
      // stateChanges
      for (auto & scs : pb.statechanges()){
        vector<StateChange> scv;
        for (auto & sc : scs.changes()){
          scv.push_back(StateChange{sc.del(),sc.k(),sc.v()});
        }
        this->stateChanges.push_back(scv);
      }
      // txReceipts
      for (auto & r : pb.txreceipts()){
        TxReceipt tr;
        tr.fromPb(r);
        this->txReceipts.push_back(tr);
      }
    }

    /*
      🦜 : Because we had trouble with the toPb() and fromPb() methods, we kinda
      have to override these two methods. manually.. so that to make our
      ADD_TO_FROM_STR_WITH_JSON_OR_PB works... ⚠️
     */
    bool fromPbString(string_view s) noexcept {
      hiPb::ExecBlk pb;
      if (!pb.ParseFromString(string(s))){
        BOOST_LOG_TRIVIAL(error) << format( S_RED "❌️ error parsing pb" S_NOR);
        return false;
      }

      try {
        this->fromPb0(pb);
      }catch(std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing pb: %s") % e.what();
        return false;
      }

      return true;
    }
    string toPbString() const {
      return this->toPb0().SerializeAsString();
    }

    // <2024-02-01 Thu>
    // --------------------------------------------------

    ADD_TO_FROM_STR_WITH_JSON_OR_PB
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
