#pragma once
#include ".generated_pb/hi.pb.h"
#include <algorithm>
#include <string_view>
// using std::string;


#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
#include <boost/format.hpp>
// #include <boost/log/core.hpp> // Needed when filtering
// #include <boost/log/expressions.hpp>

#include <boost/lexical_cast.hpp>
#include <evmc/evmc.hpp>
#include <evmc/hex.hpp>
#include <string>
#include <intx/intx.hpp>
#include <ctime>
#include <vector>
#include <ethash/keccak.hpp>
#include <memory>
#include <boost/json.hpp>
#include <string>
#include<tuple> // for tuple

#include <boost/throw_exception.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include "pure-common.hpp"

namespace weak {
  using boost::lexical_cast;
  using std::function;
  using std::tuple;
  using std::make_tuple;
  using std::move;

  using std::optional;
  using std::unique_ptr;
  using std::shared_ptr;
  using boost::format;
  using std::string_view;
  using std::vector;
  using std::string;
  using evmc::bytes;              // in hex.cpp
  using evmc::bytes_view;              // in hex.cpp
  using evmc::address;
  using std::make_unique;
  using std::make_shared;

  using pure::IJsonizable;
  using pure::ISerializable;
  using pure::ISerializableInPb;
  using pure::pluralizeOn;

  // These two are actually very similar, all have member `uint8_t bytes[32]`
  // We use hash256 when we want to make it clear that it's a hash.
  using evmc::bytes32;
  using ethash::hash256;

  namespace json = boost::json;
  using json::value_to;

  bool contains(json::array a, string s){
    for (const json::value & v : a){
      if (v.as_string() == s) return true;
    }
    return false;
  }

string resultToString(const evmc::Result& result){
  bytes o{result.output_data,result.output_size};
  return evmc::hex(o);
}

  /**
   * @brief Convert a type with a member `uint8_t bytes[?]` to string.
   *
   * @param t The type to convert. Options include `evmc::address`, `evmc::bytes32`
   */
  template<typename T>
  string toByteString(const T t){
    return string(reinterpret_cast<const char*>(t.bytes),sizeof(t.bytes));
  }

  /**
   * @brief Explicit specialization of `toByteString` for `evmc::address`, ignoring leading zeros.
   *
   * 🦜 : Often during testing, we encounter address such as '0x1' and '0x2'.
   * And if we serialize/parse these as 20-byte long string, it would be a bit
   * wasteful, so we provided this specialization, which skips the leading zeros.
   */
  template<>
  string toByteString<address>(const address t){
    // 1. find the first non-zero byte
    size_t i = 0;
    for (;i<sizeof(t.bytes);i++){
      if (t.bytes[i] != 0) break;
    }
    // 2. copy the rest
    return string(reinterpret_cast<const char*>(t.bytes+i),sizeof(t.bytes)-i);
  }

  template<typename T>
  T fromByteString(string_view s){
    if (s.size() != sizeof(T::bytes)){
      BOOST_THROW_EXCEPTION(std::runtime_error((format("Invalid size for %1%, should be %2%, but got %3%") % typeid(T).name() % sizeof(T::bytes) % s.size()).str()));
    }

    T t;
    std::copy_n(reinterpret_cast<const uint8_t*>(s.data()),sizeof(t.bytes),std::begin(t.bytes));
    return t;
  }

  /**
   * @brief Explicit specialization of `fromByteString` for `evmc::address`,
   * adding leading zeros maybe.
   *
   * 🦜 : Similarly, we would like strings like "\x01" to be parsed as '0x1'
   * address, in that case, we need to add leading zeros.
   */
  template<>
  address fromByteString<address>(string_view s){
    // throw if there're more than 20 bytes
    if (s.size() > 20){
      BOOST_THROW_EXCEPTION(std::runtime_error((format("Invalid size for address, should be at most 20, but got %d") % s.size()).str()));
    }
    // 1. caculate how many zeros to add
    size_t n = 20 - s.size();
    // 2. loop
    address a;
    std::fill_n(a.bytes,n,0x00);
    std::copy_n(reinterpret_cast<const uint8_t*>(s.data()),s.size(),std::begin(a.bytes)+n);
    return a;
  }

  // bytes 2 string<2024-01-30 Tue>  🦜 : They are needed when saving and reading in/from pb
  string toString(const bytes & b){
    return string(reinterpret_cast<const char*>(b.data()),b.size());
  } // ^v a pair
  bytes bytesFromString(string_view s){
    return bytes(reinterpret_cast<const uint8_t*>(s.data()),s.size());
  }

  // byte32 <-> string
  string toString(const bytes32 & b){
    return string(reinterpret_cast<const char*>(b.bytes),sizeof(b.bytes));
    // return string(reinterpret_cast<const char*>(
  }
  bytes32 bytes32FromString(string_view s){
    if (s.size() < 32){
      BOOST_THROW_EXCEPTION(std::runtime_error((format("Invalid size for bytes32, should be at least 32, but got %d") % s.size()).str()));
    }
    bytes32 b;
    std::copy_n(reinterpret_cast<const uint8_t*>(s.data()),32,std::begin(b.bytes));
    return b;
  }

  evmc::address makeAddress(int x) noexcept {
    // string s = (format("0x%020x") % x).str();
    // BOOST_LOG_TRIVIAL(trace) <<  format("🐸 Making address %s from %d \n") % s % x;
    // BOOST_LOG_TRIVIAL(trace) <<  format(  "Making addr" S_GREEN " %d" S_NOR) % x;
    return evmc::address(static_cast<uint64_t>(x));
  }

  string hashToString(const ethash::hash256 & h) noexcept{
    auto bs = evmc::bytes_view(reinterpret_cast<const uint8_t*>(h.bytes),
                               sizeof(h.bytes));
    // std::basic_string_view()
    return evmc::hex(bs);
  }

  string addressToString(evmc::address a) noexcept{
    auto bs = evmc::bytes_view(a);
    // in evmc.hpp ln67, evmch gives a convertion to bytes_view from address
    return evmc::hex(bs);
    // In fact, we can just use:
    //    string s  = evmc::hex(this->from);
    // But this one is more readable and less C++
  }

  /**
   * @brief Returns text representation of the ::evmc_call_kind.
   */
  inline const char* evmc_call_kind_to_string(enum evmc_call_kind k){
    switch (k){
    case EVMC_CALL: return "EVMC_CALL";
    case EVMC_DELEGATECALL: return "EVMC_DELEGATECALL";
    case EVMC_CALLCODE: return "EVMC_CALLCODE";
    case EVMC_CREATE: return "EVMC_CREATE";
    case EVMC_CREATE2: return "EVMC_CREATE2";
    }
    return "<unknown>";
  }


  tuple<evmc_message,evmc::bytes> makeMsg(string_view code_hex="",
                                         string_view input_hex="",
                                         evmc_call_kind k = EVMC_CALL
                                         ) noexcept {
  // evmc_call_kind k = EVMC_CALL;
  evmc_message msg{.kind=k,.flags=0}; // for now valid flags are only 0;
  // string_view code_hex = "";
  // string_view input_hex = "";

  // Make them static for some memory
  /*
    🦜: Are the data gone after return ?

    🐢: Seem so. They are std::basic_string.

    🦜: But the result contains pointer to these data. How do these work when
    the return value contains pointer to those data?

    🐢 : Propbably because previously, we initialized the msg and then call the
    function that processes it. That will work
   */
  static evmc::bytes code = evmc::from_hex(code_hex).value();
  static evmc::bytes input = evmc::from_hex(input_hex).value();

  // ! 1~9 are precompiled addresses (🦜: We don't have em yet.)
  // 0x 00000 00000 00000 00000
  //    00000 00000 00000 00001_
  // msg.sender = parse<evmc::address>("0x0000000000000000000000000000000000000001");
  // msg.code_address = msg.recipient = 0x0000000000000000000000000000000000000002_address;
  msg.sender = makeAddress(1);
  msg.code_address = msg.recipient = makeAddress(2);
  /**
   * @brief The address of the code to be executed.
   *
   * For ::EVMC_CALLCODE or ::EVMC_DELEGATECALL this may be different from the
   * evmc_message::recipient. Not required when invoking evmc_execute_fn(), only
   * when invoking evmc_call_fn(). Ignored if kind is ::EVMC_CREATE or
   * ::EVMC_CREATE2.
   */
  msg.gas = std::numeric_limits<int64_t>::max();
  msg.input_data = input.data();
  msg.input_size = input.size();

  // set the transfer value to 0. For contract creation and call, this is neccesary
  // for (int i = 0;i<32;i++){printf("value[%d] = %d\n",i,msg.value.bytes[i]);}
  // std::fill_n(msg.value.bytes,32,0x00);

  // so the following line will break the tests
  // msg.value.bytes[31] = 0x1; // set the value to 1
  return make_tuple(msg,code);
}

  void logMsg(const evmc_message &msg)noexcept {
    BOOST_LOG_TRIVIAL(debug) << format("recieved msg:\n\t"
                                       S_CYAN
                                       "recipient: %s\n\t"
                                       "sender: %s\n\t"
                                       "code_address: %s\n\t"
                                       "data : %s\n\t"
                                       "depth : %d\n\t"
                                       "create2_salt: %s\n\t"
                                       "kind: %s\n\t"
                                       S_NOR)
      % address(msg.recipient)
      % address(msg.sender)
      % address(msg.code_address)
      % bytes(msg.input_data,msg.input_size)
      % msg.depth
      % bytes32(msg.create2_salt)
      % evmc_call_kind_to_string(msg.kind)
      ;
  }



  // {{{ Interfaces


  using std::runtime_error;
  using std::unordered_map;


  class Acn: virtual public IJsonizable
           , virtual public ISerializableInPb<hiPb::Acn>
           , virtual public ISerializable
  {
  public:

    ADD_TO_FROM_STR_WITH_JSON_OR_PB
    // ^^ 🦜 add the to/fromString method

    // <2024-03-15 Fri> 🦜 : Today we try to pb the Acn, and also make the
    // hashcode a caculated property
    hiPb::Acn toPb() const override {
      hiPb::Acn pb;
      pb.set_nonce(this->nonce);
      pb.set_code(weak::toString(this->code));

      /*
        set the
        repeated bytes storage_ks = 4;
        repeated bytes storage_vs = 5;
       */
      for (const auto& [k,v] : this->storage){
        pb.add_storage_ks(weak::toString(k));
        pb.add_storage_vs(weak::toString(v));
      }

      // set the disk_storage
      for (const string & s : this->disk_storage){
        pb.add_disk_storage(s);
      }

      return pb;
    }

    void fromPb(const hiPb::Acn & pb) override{
      this->nonce = pb.nonce();
      this->code = weak::bytesFromString(pb.code());

      // parse the storage
      for (int i = 0;i<pb.storage_ks_size();i++){
        bytes32 k = weak::bytes32FromString(pb.storage_ks(i));
        bytes32 v = weak::bytes32FromString(pb.storage_vs(i));
        this->storage[k] = v;
      }

      // parse the disk_storage
      for (int i = 0;i<pb.disk_storage_size();i++){
        this->disk_storage.push_back(pb.disk_storage(i));
      }
    }



    Acn() = default;
    Acn(const uint64_t n,bytes c): nonce(n), code(c){};

    // data
    // --------------------------------------------------
    uint64_t nonce = 0; /// The account nonce.
    /// The account code.
    bytes code;
    /// The account storage map.
    unordered_map<bytes32,bytes32> storage;
    /*
      <2024-03-15 Fri> The "disk" storage. 🦜 : Different executors will use the
      `disk_storage` differently.
     */
    vector<string> disk_storage;

    /// The code hash. Can be a value not related to the actual code.
    hash256 codehash() const noexcept{
      return ethash::keccak256(reinterpret_cast<const uint8_t*>(code.data()), code.size());
    }

    // methods
    // --------------------------------------------------

    json::value toJson() const noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("📗Jsonizing Acn");
      return json::value_from(*this);
    };

    bool fromJson(const json::value &v) noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("📗️ Forming Acn from Json");

      try{
        json::object const& o = v.as_object();
        this->nonce = value_to<uint64_t>(o.at("nonce"));

        // code and hashes
        string c;
        c = value_to<string>(v.at("code"));

        this->code = evmc::from_hex(c).value();
        /* Implementation of from_hex:

           If the length of string is smaller than the target array copy the bytes
           from the end.

           if (num_in_bytes > num_out_bytes)
           return {};
           if (!from_hex(s.begin(), s.end(), &r.bytes[num_out_bytes - num_in_bytes]))
           ^^^^^^^
           return {};
           return r;
        */

        // storage
        for (const json::key_value_pair & kv : o.at("storage").as_object()){
          string s_key = kv.key();
          string s_val = value_to<string>(kv.value());

          // Check key
          if (s_key.size() > 32){
            BOOST_LOG_TRIVIAL(error) << format("Invalid storage key,should be at most 32-long, but got %d")
              % s_key.size();
            return false;
          }

          // Check value
          if (s_val.size() > 32){
            BOOST_LOG_TRIVIAL(error)
              << format("Invalid storage value,should be at most 32-long, but got %d")
              % s_val.size();
            return false;
          }

            // string to bytes
            bytes32 key{uint64_t{0}}; // start all zero value
            bytes32 val{uint64_t{0}};

            // 🦜 :copy from back (Big-Endian) "123" -> [00000....123] Since many keys
            // are just "01", "02", etc.. removing the leading zero should save a
            // non negligible amount of space.
            std::copy_backward(s_val.begin(),s_val.end(),std::end(val.bytes));
            std::copy_backward(s_key.begin(),s_key.end(),std::end(key.bytes));

            // std::copy_n(reinterpret_cast<uint8_t*>(s_key.data()),32,std::begin(key.bytes));
            // std::copy_n(reinterpret_cast<uint8_t*>(s_key.data()),32,std::begin(val.bytes));

            // Put in the storage
            this->storage[key] = val;

        }

        // get the disk storage if exists
        if (o.contains("disk_storage")){
          for (const json::value & s : o.at("disk_storage").as_array()){
            this->disk_storage.push_back(value_to<string>(s));
          }
        }
      }catch(std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }

      BOOST_LOG_TRIVIAL(debug) << format("\t" S_CYAN "Acn-%d" S_NOR " formed from Json")
        % this->nonce;
      return true;
    }
  };

  // json functions for Acn
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& v, Acn const& a ){
    v = {
      {"nonce",a.nonce},
        {"code",evmc::hex(a.code)},
        {"codehash",hashToString(a.codehash())}
    };

    json::object s;             // storage
    // copy the storage
    BOOST_LOG_TRIVIAL(debug) << format("Start jsonizing storage of size %d") % a.storage.size();
    for (const auto& [key,val] : a.storage){
      // bytes32 to string
      string s_key(reinterpret_cast<const char*>(key.bytes),32),
        s_val(reinterpret_cast<const char*>(val.bytes),32);

      BOOST_LOG_TRIVIAL(trace) << format("key-hex: %s") % evmc::hex(key);
      BOOST_LOG_TRIVIAL(trace) << format("val-hex: %s") % evmc::hex(val);

      // trim leading zeros --------------------------------------------------
      BOOST_LOG_TRIVIAL(debug) << format("Triming leading-zero for storage key of length %d")
        % s_key.size();
      size_t i = s_key.find_first_not_of('\x00');
      if (i != string::npos){
        // found leading zero
        s_key = s_key.substr(i);
        BOOST_LOG_TRIVIAL(debug) << format("Found " S_GREEN "%1%"
                                           S_NOR " leading-zero%2%, trimmed key has "
                                           S_CYAN " %3% byte%4%." S_NOR)
          % i
          % pluralizeOn(i)
          % s_key.size()
          % pluralizeOn(s_key.size());
      }

      BOOST_LOG_TRIVIAL(debug) << format("Triming leading-zero for storage value");

      i = s_val.find_first_not_of('\x00');
      if (i != string::npos){
        // found leading zero
        s_val = s_val.substr(i);
        BOOST_LOG_TRIVIAL(debug) << format("Found " S_GREEN "%1%"
                                           S_NOR " leading-zero%2%, trimmed val has "
                                           S_CYAN " %3% byte%4%." S_NOR)
          % i
          % pluralizeOn(i)
          % s_val.size()
          % pluralizeOn(s_key.size());
      }

      // Store the k,v --------------------------------------------------
      s[s_key] = s_val;
    }

    v.as_object()["storage"] = s;

    // <2024-03-15 Fri> the disk_storage
    if (not a.disk_storage.empty()){
      json::array d;
      for (const string & s : a.disk_storage){
        d.push_back(json::string(s));
      }
      v.as_object()["disk_storage"] = d;
    }
  }

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  Acn tag_invoke(json::value_to_tag<Acn>, json::value const& v ) noexcept{
    Acn a;
    if (a.fromJson(v)) return a;
    return {};
  }


  /**
   * @brief Representing a type that can provide Acn.
   * The set of accounts is the canonical world state.
   */
  class IAcnGettable{
  public:
    virtual optional<Acn> getAcn(evmc::address addr) const noexcept=0;
  };


  /**
   * @brief A state change, should be generated by executor.
   */

  struct StateChange{bool del=false; string k; string v;};

  /**
   * @brief The interface drawn from `IWorldChainStateSettable`.
   *
   * 🦜 : This is for RPC for now.
   */
  class IChainDBGettable {
  public:
    virtual optional<string> getFromChainDB(const string k) const =0;
  };

  /**
   * @brief The interface that `WorldStorage` exposes to `CoreManager`. It
   * allows the `CoreManager` to change
   *   - the *chain state* (e.g. Transaction info, Block info stored in chainDB)
   *   _ the *world state* (stored in stateDB)
   *
   *   🦜: We see that the `CoreManager` is able to read/write chainDB (no
   *   deletion). This agree with fact that "blockchain is append only". On the
   *   other hand, StateDB can only be write/delete by the `CoreManager` because
   *   the state only need to be visible to the execution module.
   *
   *   🐢: Are U sure that making the StateDB write-only to `CoreManager` is not
   *   being lazy?
   *
   *   🦜: No.
   */
  class IWorldChainStateSettable: public virtual IChainDBGettable {
  public:
    virtual bool setInChainDB(const string k, const string v) =0;

    /// Apply the journal (state changes) to the stateDb, usually it's one
    /// journal per Blk.
    virtual bool applyJournalStateDB(const vector<StateChange> & j)=0;
  };

  /**
   * @brief The interface that `WorldStorage` exposes to get a series of keys
   * with certain prefix in the DB.
   *
   * 🐢 : This interface is created here initially for the initializer, who
   * needs to get all keys start which "/tx/" in order to figure out what Txs
   * have been written to the chain.
   */
class IChainDBPrefixKeyGettable {
public:
  virtual vector<string> getKeysStartWith(string_view prefix)const =0;
};

  /**
   * @brief The interface that sticks two interfaces.
   *
   * 🦜 : Why?
   *
   * 🐢 : Sometimes it's easier to pass this around in functions. This is
   * created for `init`.
   */
class IChainDBGettable2 :public virtual IChainDBPrefixKeyGettable,
                         public virtual IChainDBGettable{};


  // }}}


  class Tx: virtual public IJsonizable
          , virtual public ISerializableInPb<hiPb::Tx>
          , virtual public ISerializable
  {
  public:

    // <2024-01-30 Tue>
    hiPb::Tx toPb() const override {
      hiPb::Tx pb;

      if (this->type == Type::data)
        pb.set_type(hiPb::TxType::DATA);
      else
        pb.set_type(hiPb::TxType::EVM);

      pb.set_from_addr(weak::toByteString<address>(this->from));
      pb.set_to_addr(weak::toByteString<address>(this->to));
      pb.set_data(weak::toString(this->data));
      pb.set_timestamp(this->timestamp);
      pb.set_nonce(this->nonce);

      // 🦜 : The following may be empty, but that's fine.
      if (this->pk_pem != "")
        pb.set_pk_pem(this->pk_pem);

      if (this->signature.size() > 0)
        pb.set_signature(weak::toString(this->signature));

      if (this->pk_crt.size() > 0)
        pb.set_pk_crt(weak::toString(this->pk_crt));
      return pb;
    }

    // <2024-01-30 Tue>
    void fromPb(const hiPb::Tx & pb) override{
      string s;
      /*
        🦜 : Similar process ad in fromJson0
       */
      if (not pb.pk_pem().empty()){
        this->pk_pem = pb.pk_pem();
        this->from = this->getFromFromPkPem();
      }else{
        // parse the from
        s = pb.from_addr();
        // should be 20 bytes
        this->from = weak::fromByteString<address>(s); // might throw
      }

      // parse sig if exists
      if (not pb.signature().empty()){
        this->signature = weak::bytesFromString(pb.signature());
      }

      // parse pk_crt if exists
      if (not pb.pk_crt().empty()){
        this->pk_crt = weak::bytesFromString(pb.pk_crt());
      }

      // parse the rest
      this->nonce = pb.nonce();
      this->timestamp = pb.timestamp();
      this->data = weak::bytesFromString(pb.data());

      // if it has a type field, use it , Otherwise set it to evm (default)
      if (pb.type() == hiPb::TxType::DATA)
        this->type = Type::data;
      else
        this->type = Type::evm;

      // parse the to
      s = pb.to_addr();
      this->to = weak::fromByteString<address>(s); // throws for bad lengths
    }

    Tx() = default; // Upon you make a constructor yourself, the build-in
                    // constructor is gone,use this to bring it back

    // --------------------------------------------------
    // Type related
    enum class Type {evm, data};

    static string typeToString(Type t){
      static const string s[] = {"evm", "data"};
      return string(s[static_cast<int>(t)]);
    }

    static Type typeFromString(string_view s){
      if (s == "" or s == "evm") return Type::evm;
      if (s == "data") return Type::data;
      BOOST_THROW_EXCEPTION(std::runtime_error((format("Invalid Tx type: %1%") % s).str()));
    }
    // --------------------------------------------------

    Tx(const address f,const address t,const bytes d,const uint64_t n,
        Type type = Type::evm):
      from(f),to(t),data(d),nonce(n){
      timestamp = std::time(nullptr);
    };

    /* [2024-01-22] 🐢 : Let's do some serious tx-check.

       🦜 : Currently, every user (http client) can send Tx in behalf of any
       user, i.e. I just just say: "I'm addr 0x2, and I wanna send ..." This is
       not that useful in serious cases.

       🐢 : Em. So let's add this cmdline option

       --serious-tx-check-level [0-3]

        0,no-check: no check (default, same as now, good for testing)

        1,public-chain-mode: public chain mode, in this mode we check the `pk_pem` and `signature`
        of a tx. In this mode, we derive the `from` address from the `pk_pem` by
        sha3 (keccak256) the raw public key and take the last 20 bytes. (similar
        to Ethereum). The signature should be `sign(nonce,data)`. So in order to
        send a tx, the client needs to generate a key pair.

        2,private-chain-mode: private chain mode, the `pk_crt` is checked, which is the signature
        of sender's pk_pem by the CA. As a result, only users allowed by CA can
        send tx.

        🦜 : Emmm... This seems enough. So we add three optional fields to Tx
        right?

        🐢 : Yeah. And I think these should only be checked when transforming a
        Tx into an `evmc_msg`.(or similar stage for other VM types.)

        🦜 : Oh, so it's at the TxExecutor there right? What's the formal name?

        🐢 : `Div2Executor` in `div2Executor.hpp`

        🦜 : Okay, so when serious-tx-check-level > 0, `from` field is ignored right?

        🐢 : Yeah. Also , make sure that the pem file is not modified after
        created by openssl...Because we are signing against the hash of the pem
        file, so it's sensitive to things like `\n`

        🦜 : Ohoh, dangerous...
     */
    string pk_pem;                  // [2024-01-22] the sender's public key in
                                    // pem format, if this is given, then `from` is derived from it.
    bytes signature;               // [2024-01-22] the signature
    bytes pk_crt;                  // [2024-01-22] the sender's crt

    Type type = Type::evm;
    // --------------------------------------------------

    address from;
    /// if to is empty, create contract, else call
    address  to;
    /// if CREATE: contract bin code, else abi input
    bytes data;
    /// unique_id
    uint64_t nonce;
    /// ms since Epoch
    std::time_t timestamp;
    // usually = std::time(nullptr);


    /// Tx hash
    /**
     * @brief the hash of the Tx.
     *
     * [2024-01-23] 🦜 it's used to be a field, now it's a method. 
     */
    hash256 hash() const {
      // convert nonce to array
      constexpr auto size = sizeof(uint64_t);
      uint8_t a[size]{};
      intx::be::store(a,nonce);

      uint8_t b[size + 20];
      // std::copy_n(std::begin(f.bytes)
      //             ,20, std::begin(b));
      // std::copy_n(std::begin(a),size,std::begin(b)+20);

      std::copy_n(this->from.bytes ,20, b);
      std::copy_n(a,size,b+20);

      // Get the hash(nonce)
      return  ethash::keccak256(reinterpret_cast<uint8_t*>(b), size + 20);
    }
    /*
      For now, use UTF8 JSON for serialization. later we can change it to other
      🦜 :<2024-01-30 Tue> now we use protobuf
     */
    ADD_TO_FROM_STR_WITH_JSON_OR_PB
    /*
      🐢 defines the to/fromString() methods according to WITH_PROTOBUF, so we
      don't need to write the following...
     */
    // string toString() const noexcept override {
    //   // return IJsonizable::toJsonString();
    //   return toPbString();
    // };
    // bool fromString(string_view s) noexcept override{
    //   BOOST_LOG_TRIVIAL(debug) <<  "Forming Tx from string.";
    //   // return IJsonizable::fromJsonString(s);
    //   return fromPbString(s);
    // };

    /**
     * @brief obtain the `from` address from the `pk_pem` field.
     *
     * 🦜 : This is the same as Ethereum: hash the pk_pem and take the last 20
     * bytes. Sorry about the awkward name...
     */
    address getFromFromPkPem() const noexcept{
      hash256 h = ethash::keccak256(reinterpret_cast<const uint8_t*>(pk_pem.data()), pk_pem.size());
      address a;
      // copy it
      std::copy_n(h.bytes + 12, 20, a.bytes);
      return a;
    }


    /**
     * @brief The fromJson function that can throw
     * <2024-01-30 Tue>
     * 🐢 : In fact there are many kinds of valid Tx json. But the most common two are
     *
     * 1. Json from the client rpc. (the minimal of such json is {from,to,data,nonce}). We need to give it a timestamp.
     *
     * 2. Json from other sources (e.g. the chainDB). This json should contain
     * all fields, including a hash. However, the hash is not read, because
     * since 2024-01-22, we have changed to make hash a method. 
     *
     * 🦜 : Oh, so the only difference is that if the `timestamp` field is present or not?
     *
     * 🐢 : Yeah, I think as long as we add that in the rpc endpoint, then we are good
     * to share the `fromJson0` function with other sources.
     */
    void fromJson0(const json::object &o){
      string s;
      optional<bytes> ob;
      optional<address> oa;

      // --------------------------------------------------
      // [2024-01-22] 🐢 : Add the optional field if they are given.
      if (o.contains("pk_pem")){
        this->pk_pem = value_to<string>(o.at("pk_pem"));
        this->from = getFromFromPkPem();
      }else{
        // 🦜 : if pk_pem is not given, parse the `from`, otherwise it's ignored.
        s= value_to<string>(o.at("from"));
        optional<evmc::address> oa = evmc::from_hex<evmc::address>(s);
        if (not oa) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid from = " + s));
        this->from = oa.value();
      }

      if (o.contains("signature")){ // parse sig if exists
        ob = evmc::from_hex(o.at("signature").as_string());
        if (not ob) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid `signature` format" + json::serialize(o.at("signature"))));
        this->signature = ob.value();
      }

      if (o.contains("pk_crt")){ // parse pk_crt if exists
        ob = evmc::from_hex(o.at("pk_crt").as_string());
        if (not ob) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid `pk_crt` format" + json::serialize(o.at("pk_crt"))));
        this->pk_crt = ob.value();
      }

      // [2024-01-22]
      // --------------------------------------------------
      BOOST_LOG_TRIVIAL(trace) << format("parsing nonce");
      this->nonce = value_to<uint64_t>(o.at("nonce"));

      BOOST_LOG_TRIVIAL(trace) << format("parsing timestamp");
      this->timestamp = static_cast<time_t>(value_to<uint64_t>(o.at("timestamp")));

      // 🦜 if v has a `type` field, use it. Otherwise, use default (evm).
      if (o.contains("type")){
        string s = value_to<string>(o.at("type"));
        this->type = Tx::typeFromString(s);
      }else{
        this->type = Type::evm;
      }


      /*
        🐢 : parse<...>() will call std::terminate() on failure. So we
        should only use it if we are sure.
      */
      s= value_to<string>(o.at("data"));
      /* <2024-02-05 Mon> 🦜 :
         If the type is evm, then data must be
         hex-encoded. Otherwise, we don't check (for now). (and just save
         whatever is stored in the json.)
       */
      if (this->type == Type::evm){
        ob = evmc::from_hex(s);
        if (not ob) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid data = " + s));
        this->data = ob.value();
      }else{
        this->data = weak::bytesFromString(s);
      }

      s= value_to<string>(o.at("to"));
      // this->from = evmc::literals::parse<address>(f); // 🦜 this do abort (not what we wan)
      oa = evmc::from_hex<address>(s);
      if (not oa) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid to = " + s));
      this->to = oa.value();

      /// Decodes hex-encoded string into custom type T with .bytes array of
      /// uint8_t. so ethash::hash256 is such type
      // h= value_to<string>(o.at("hash"));
      // optional<hash256> oh;
      // oh = evmc::from_hex<hash256>(h);
      // if (not oh) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid hash = " + h));
      // this->hash = oh.value();
    }

    bool fromJson(const json::value &v) noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("Forming Tx from Json");

      try {
        json::object const& o = v.as_object();
        this->fromJson0(o);
      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }

      BOOST_LOG_TRIVIAL(debug) << format("🐸 Tx-%d formed from Json") % this->nonce;
      return true;
    }
    json::value toJson() const noexcept override {
      return json::value_from(*this);
    };


    /**
     * @brief The Index of a Tx.
     *
     * Each Tx is identified by an Index, which is the pair (address, nonce).
     * Transactions with same Index are considered "double-spent", and for now,
     * the later one (one with greater timestamp) will not be added to the mempool.
     */
    struct Index{
      address addr;
      uint64_t nonce;
      Index(address a,uint64_t n): addr(a), nonce(n){}
      bool operator<(Index const& s) const {
        if (bytes_view(addr) == bytes_view(s.addr)){
          if (nonce < s.nonce) return true;      // [=][<]
        } // address is different
        return bytes_view(addr).compare(bytes_view(s.addr)) < 0; // [<]
      }
    };

    /*
      🦜 : How Tx is sorted:
      1. first by timestamp,
      2. then by Index
    */
    bool operator<(const Tx& r)const{
      if (timestamp == r.timestamp){
        if (Index{from,nonce} < Index{r.from,r.nonce})
          return true;
      }   // timestamp is different
      return timestamp < r.timestamp; // [<]
    }

    /**
     * @brief Get the contract address for a CREATE tx
     * @param t The CREATE tx. It is supposed that t.to = 0
     *
     * 🦜 : At begining we used tx.nonce -> contract address. Is that a bit obvious?
     * 🐢 : For now, let's do it.
     * 🦜 : Later maybe we can do things like hash the nonce and take the first 20 bytes?
     * 🐢 : Nope we use XOR + timestamp should be alright.
     */
    static address getContractDeployAddress(const Tx & t) noexcept{
      // return makeAddress(t.nonce  ^ t.timestamp);
      /*
        🐢  : Note that (0010,0010) and (0011,00011) got the same address... This
        is something we wanna avoid (Although it's rare, but not too rare).

        🦜 : How about we flip the timestamp?

        🐢 : Let's see....
      */
      return makeAddress(t.nonce  ^ (~t.timestamp));
    };

    /**
     * @brief Conversion between vector<Tx> and string.
     *
     * 🦜 : For now, we use JSON.
     * 
     * 🐢 : But we kinda need to make sure that everything relating to
     * serialization is contained in this class.
     */
    static vector<Tx> parse_to_array(string_view arg){
      // --------------------------------------------------
      /* [2024-02-04]

        🦜: In addition to `toString` and `fromString`, here we also need to
        switch these two for Txs
       */
#ifdef WITH_PROTOBUF
      hiPb::Txs txs;
      txs.ParseFromString(string(arg));
      vector<Tx> v;
      for (const hiPb::Tx & tx : txs.txs()){
        Tx t;
        t.fromPb(tx);
        v.push_back(t);
      }
      return v;
#else
      // Parse the Json
      /* 🦜: std:string_view -> json::string_view is
         available in Boost 1.82.0*/
      // json::value v = json::parse(s);
      json::value v = json::parse(arg); // throw on fail
      return value_to<vector<Tx>>(v);
#endif

    }
    static string serialize_from_array(const vector<Tx> & txs){
#ifdef WITH_PROTOBUF
      hiPb::Txs pb;
      for (const Tx & t : txs){
        pb.add_txs()->CopyFrom(t.toPb());
      }
      return pb.SerializeAsString();
#else
      return json::serialize(json::value_from(txs));
#endif
    }

    /**
     * @brief Help Rpc to parse the json that should represent an array of Txs.
     *
     * @param s The array of Tx in JSON
     *
     * @return The Json string to be returned to client and the newly created Txs.
     *
     * 🦜 : The detail of the input and output of this command @see Rpc().
     */
    static optional<tuple<string,vector<Tx>>> parse_txs_jsonString_for_rpc(string_view s) noexcept {
      try {
        // BOOST_LOG_TRIVIAL(debug) << format("Parsing txs JSON sent from client. %s") % s ;
        // Parse the Json (let it throw)
        #ifdef _WIN32
        json::value v = json::parse(string(s));
        #else
        /* 🦜: std:string_view -> json::string_view is
           available in Boost 1.82.0, at least for GCC*/
        json::value v = json::parse(s);
        #endif
        // Turning Json into array
        json::array a = v.as_array();

        return Tx::parse_txs_json_for_rpc(move(a));
      }catch (std::exception const & e){
        BOOST_LOG_TRIVIAL(debug) << format("❌️ Error parsing client JSON:\n" S_MAGENTA "%s" S_NOR
                                           "\n" "Info:" S_RED "\n%s" S_NOR) %
          s % boost::diagnostic_information(e);
        return {};
      }
    }

    /**
     * @brief The helper of parse_txs_jsonString_for_rpc().
     *
     * @param a The array of Txs in JSON sent by client.
     *
     * @return The pair (s,txs), where `s` is the string to be sent back to
     * client and `txs` is the parse Txs. This function throws on error.(🦜 :
     * The exception should be caught by `parse_txs_jsonString_for_rpc()`).
     */
    static optional<tuple<string,vector<Tx>>> parse_txs_json_for_rpc(json::array && a) {
      json::array os;           // output object

      vector<Tx> txs;
      for (int i=0;i<a.size();i++){
        json::object o = a[i].as_object(); // the tx object
        // string f,t,d;
        // uint64_t n;
        // f = value_to<string>(o.at("from"));
        // t = value_to<string>(o.at("to"));
        // d = value_to<string>(o.at("data"));
        // n = value_to<uint64_t>(o.at("nonce"));

        // // string to specific type --------------------------------------------------
        // optional<evmc::address> from = evmc::from_hex<evmc::address>(f);
        // if (not from){
        //   BOOST_THROW_EXCEPTION(std::runtime_error("Invalid from = " + f));
        // }

        // optional<evmc::address> to = evmc::from_hex<evmc::address>(t);
        // if (not to){
        //   BOOST_THROW_EXCEPTION(std::runtime_error("Invalid to = " + t));
        // }

        // optional<bytes> data = evmc::from_hex(d);
        // if (not data)
        //   BOOST_THROW_EXCEPTION(std::runtime_error("Invalid data = " + d));

        // BOOST_LOG_TRIVIAL(debug) << format("⚙️ Constructing Tx and back-to-client object");
        // Tx tx{from.value(),to.value(),data.value(),n};
        // ^^ 🦜 : refactored

        // 🦜 : but we need to add a `timestamp` field for the client, and
        // because this is a required field, so we kinda need to add it here.
        // This is unlike pb, where all fields are optional.

        o.emplace("timestamp",std::time(nullptr));
        Tx tx;
        tx.fromJson0(o);
        // tx.timestamp = std::time(nullptr);

        json::object o0;
        o0["hash"] = hashToString(tx.hash());
        if (tx.type != Tx::Type::data and not bool(tx.to)){ // is a CREATE tx
          /* 🦜 : Here we use the helper from evmc.hpp
         /// Checks if the given address is the zero address.
         inline constexpr bool is_zero(const address& a) noexcept
         {return a == address{};}

         inline constexpr address::operator bool() const noexcept
         {return !is_zero(*this); }
           */
          BOOST_LOG_TRIVIAL(debug) << format("Tx-" S_GREEN "%d" S_NOR " from " S_CYAN "%s"
                                             S_NOR " is " S_CYAN "CREATE" S_NOR) % tx.nonce
            %tx.from;
          o0["deployed_address"] = addressToString(Tx::getContractDeployAddress(tx));
        }

        os.emplace_back(o0);
        txs.push_back(tx);
      }

      return make_tuple(json::serialize(os),txs);
    }


    /**
     * @brief [2024-02-05] Help Rpc to parse the pbString that should represent an array of Txs.
     *
     * @param s The array of Tx in pbString, the serialized Txs.
     *
     * 🦜 This should mimic `parse_txs_jsonString_for_rpc()`
     *
     * message AddTxReply {
     *   bytes hash = 1;
     *   bytes deployed_addr = 2;
     * }

     * message AddTxsReply {
     *   repeated AddTxReply txs = 1;
     * }
     *
     *
     * message Tx {
     *   TxType type = 1;
     *   bytes from_addr = 2;
     *   bytes to_addr = 3;
     *   bytes data = 4;
     *   uint64 timestamp = 5;         // [not needed in rpc]
     *   uint64 nonce = 6;
     *   bytes hash = 7;               // will be serialized, but not parsed
     *   // 🦜 : The following 3 are optional in rpc param
     *   string pk_pem = 8;
     *   bytes signature = 9;          // key.sign(hash)
     *   bytes pk_crt = 10;            // ca_key.sign(pk_pem)
     * }                               // [x]
     * message Txs {repeated Tx txs = 1;} // [x]
     */
    static optional<tuple<string,vector<Tx>>> parse_txs_pbString_for_rpc(string_view s) noexcept {
      hiPb::Txs pb;
      if (not pb.ParseFromString(string(s))){
        BOOST_LOG_TRIVIAL(debug) << format("❌️ Error parsing Txs pbString");
        return {};
      }

      return Tx::parse_txs_pb_for_rpc(pb);
    }

    /**
     * @brief The helper of parse_txs_pbString_for_rpc().
     *
     * @param pb The Txs in pb format.
     *
     * @return The pair (s,txs), where `s` is the string to be sent back to
     * client and `txs` is the parse Txs. This function throws on error.(🦜 :
     * The exception should be caught by `parse_txs_pbString_for_rpc()`).
     */
    static optional<tuple<string,vector<Tx>>> parse_txs_pb_for_rpc(const hiPb::Txs & pb) {
      hiPb::AddTxsReply atx;    // the output object

      vector<Tx> txs;
      try{
        for (const hiPb::Tx & tx : pb.txs()){
          Tx t;
          t.fromPb(tx);         // may throw

          // 🦜 : similarly, add the `timestamp` field for the client
          t.timestamp = std::time(nullptr);
          txs.push_back(t);

          hiPb::AddTxReply atx0;
          atx0.set_hash(weak::toByteString<hash256>(t.hash()));
          if (t.type != Tx::Type::data and not bool(t.to)){ // is a CREATE tx
            atx0.set_deployed_addr(weak::toByteString<address>(Tx::getContractDeployAddress(t)));
          }
          atx.add_txs()->CopyFrom(atx0);
        }
      }catch (std::exception const & e){
        BOOST_LOG_TRIVIAL(debug) << format("❌️ Error parsing Txs pbString:\n%s") % boost::diagnostic_information(e);
        return {};
      }

      return make_tuple(atx.SerializeAsString(),txs);
    }

  };

  // json functions for Tx
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, Tx const& c ){
    jv = {
      {"from", addressToString(c.from)},
      {"to", addressToString(c.to)},
      // {"data", evmc::hex(c.data)}, <2024-02-05 Mon> 🦜 : We don't need to hex it unless it's evm
      {"nonce", c.nonce},
      {"timestamp",static_cast<uint64_t>(c.timestamp)},
      {"hash",hashToString(c.hash())}
    };

    if (c.type != Tx::Type::evm){
      jv.as_object()["type"] = Tx::typeToString(c.type);
      jv.as_object()["data"] = toString(c.data);
    }else{
      jv.as_object()["data"] = evmc::hex(c.data);
    }

    // [2024-01-22] 🐢 : Add the optional field if they are given.
    if (not c.pk_pem.empty()){
      jv.as_object()["pk_pem"] = c.pk_pem;
    }
    if (not c.signature.empty()){
      jv.as_object()["signature"] = evmc::hex(c.signature);
    }
    if (not c.pk_crt.empty()){
      jv.as_object()["pk_crt"] = evmc::hex(c.pk_crt);
    }
  }

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  // Tx tag_invoke(json::value_to_tag<Tx>, json::value const& v ) noexcept{
  //   Tx t;
  //   if (t.fromJson(v)) return t;
  //   return {};
  // }
  // 🦜 ^^^ above is replaced by the following
  ADD_FROM_JSON_TAG_INVOKE(Tx);


class ITxExecutable {
  public:
    /**
     * @brief Probably the most important function that the execution module expose.
     *
     * If the execution is successful, a series of state-changes (called
     * "journal") and the execution-result is returned.
     *
     * @param w The read-only stateDb (world state).
     * @param t The transaction to be executed.
     * @return the pair (journal,result). {} if the execution fails.
     */
    virtual optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                                 const Tx & t) const noexcept = 0;
    // determine the type of tx: CREATE/CALL
    // Initialize host with the stateDb
    // Make VM
    // Execute and get result
    // If successful : make_tuple
  };


  // using std::move;

  class BlkHeader: virtual public IJsonizable {
  public:
    uint64_t number;
    /// empty for genesis block
    hash256 parentHash;
    BlkHeader() = default;
    BlkHeader(const uint64_t n,const hash256 p): number(n),parentHash(p){};

    // hash256 hash;
    virtual hash256 hash() const {
      // 🦜 : we kinda have to provide a default implementation, because if we
      // wanna jsonize it, it must have a default, ctor...
      BOOST_THROW_EXCEPTION(std::runtime_error("hash() not implemented for BlkHeader"));
    }

    json::value toJson() const noexcept override {
      return json::value_from(*this);
    }

    bool fromJson(const json::value &v) noexcept override {

      BOOST_LOG_TRIVIAL(debug) << format("Forming BlkHeader from Json");
      try {
        optional<hash256> oh;
        // json::object const& o = v.as_object();
        this->number = value_to<uint64_t>(v.at("number"));
        string p,h;
        p= value_to<string>(v.at("parentHash"));

        // <2024-02-06 Tue> 🦜 : change hash to method
        // h= value_to<string>(v.at("hash"));
        // oh = evmc::from_hex<hash256>(h);
        // if (not oh) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid hash = " + h));
        // this->hash = oh.value();

        oh = evmc::from_hex<hash256>(p);
        if (not oh) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid hash = " + h));
        this->parentHash = oh.value();
      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }

      // BOOST_LOG_TRIVIAL(debug) << format("🐸 BlkHeader-%d fromed from Json") % this->number;
      return true;
    }
  };

  class Blk: public BlkHeader
           ,virtual public IJsonizable
           ,public ISerializableInPb<hiPb::Blk>
           ,virtual public ISerializable
  {
  public:
    /*
      message BlkHeader {uint64 number = 1; bytes parentHash = 2; bytes hash = 3;}

      message Blk {BlkHeader header = 1; repeated Tx txs = 2;} // []
     */
    // --------------------------------------------------
    //<2024-01-30 Tue> 🦜 : let's add some pb
    void fromPb(const hiPb::Blk & pb) override{
      BOOST_LOG_TRIVIAL(debug) << format("Forming Blk from pb");
      // 1. form the BlkHeader part
      // --------------------------------------------------
      this->number = pb.header().number();

      string s = pb.header().parenthash();   // should be hash256 = 32 bytes

      // BOOST_ASSERT(s.size() == 32); // throws my_assertion_error
      this->parentHash = weak::fromByteString<hash256>(s); // may throw

      // 2. form the txs part
      // --------------------------------------------------
      for (const hiPb::Tx & tx : pb.txs()){
        Tx t;
        t.fromPb(tx);          // may throw
        this->txs.push_back(t);
      }
    }

    hiPb::Blk toPb() const override {
      hiPb::Blk pb;

      // set the header
      pb.mutable_header()->set_number(this->number);
      pb.mutable_header()->set_parenthash(weak::toByteString<hash256>(this->parentHash));

      // set the txs
      for (const Tx& tx : txs){
        pb.add_txs()->CopyFrom(tx.toPb());
      }
      return pb;
    }

    //<2024-01-30 Tue>
    // --------------------------------------------------


    Blk() = default;

    /**
     * @brief Construct the Blk and calculate the hash.
     *
     * In fact, most of the time the next ctor is used to make a `Blk` from
     * `BlkForConsensus`
     */
    Blk(const uint64_t n,const hash256 p,vector<Tx> t):
      BlkHeader(n,p),
      txs(t)
    {
      BOOST_LOG_TRIVIAL(info) << format("Making block-%d,\n\tparentHash=%s,\n\ttx size=%d")
        % number % hashToString(parentHash) % txs.size();
    }

    hash256 hash() const noexcept override {
      // Calculate hash based on the hashes of txs
      // TODO: 🦜 For now, we use serial hash, later should be changed to Merkle tree.
      hash256 h = this->parentHash;
      static uint8_t s[64];     // a buffer for hashing
      for (const Tx& tx : this->txs){
        // BOOST_LOG_TRIVIAL(debug) << format("Hashing for tx-%d") % tx.nonce;
        std::copy_n(std::cbegin(tx.hash().bytes),32,s);
        std::copy_n(std::cbegin(h.bytes),32,s + 32);
        // s[:32] = tx.hash; s[32:] = h
        h = ethash::keccak256(reinterpret_cast<uint8_t*>(s),64);
        // BOOST_LOG_TRIVIAL(debug) << format("Now hash is %s") % hashToString(h);
      }
      return h;
    }

    vector<Tx> txs;
    ADD_TO_FROM_STR_WITH_JSON_OR_PB
    json::value toJson() const noexcept override {
      return json::value_from(*this);
    }

    bool fromJson(const json::value &v) noexcept override {

      // Parse number
      BOOST_LOG_TRIVIAL(debug) << format("Forming Blk from Json");

      if (not BlkHeader::fromJson(v)) return false;

      try {
        // json::object const& o = v.as_object();
        this->txs = value_to<vector<Tx>>(v.at("txs"));
        // boost::json knows about vector

      }catch (std::exception &e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ error parsing json: %s") % e.what();
        return false;
      }

     BOOST_LOG_TRIVIAL(debug) << format("🌱 Blk-%d fromed from Json") % this->number;
      return true;
    }

  };




  // json functions for BlkHeader
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, BlkHeader const& b ){
    jv = {
      {"number", b.number},
      {"hash", hashToString(b.hash())},
      {"parentHash", hashToString(b.parentHash)},
      // {"txs", json::value_from(b.txs)}
      /* 🦜 : boost:json knows about std::vector, they turn it into array*/
    };
  };

  // json functions for Blk
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, Blk const& b ){
    // jv = {
    //   {"number", b.number},
    //   {"hash", hashToString(b.hash)},
    //   {"parentHash", hashToString(b.parentHash)},
    //   {"txs", json::value_from(b.txs)}
    //   /* 🦜 : boost:json knows about std::vector, they turn it into array*/
    // };

    // BOOST_LOG_TRIVIAL(debug) << format("Now json: %s") % json::serialize(jv);
    jv = json::value_from((const BlkHeader &) b);
    // BOOST_LOG_TRIVIAL(debug) << format("Now json: %s") % json::serialize(jv);
    jv.as_object()["txs"] = json::value_from(b.txs);
    // BOOST_LOG_TRIVIAL(debug) << format("Now json: %s") % json::serialize(jv);
    /* 🦜 : boost:json knows about std::vector, they turn it into array*/

  };

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  // Blk tag_invoke(json::value_to_tag<Blk>, json::value const& v ) noexcept{
  //   Blk b;
  //   if (b.fromJson(v)) return b;
  //   return {};
  // }
  // 🦜 ^^^ above is replaced by the following
  ADD_FROM_JSON_TAG_INVOKE(Blk);
  ADD_FROM_JSON_TAG_INVOKE(BlkHeader);



  /**
   * @brief The type that can get Tx by the corresponsing hash.
   *
   * This is usually used by a `BlkForConsensus` to convert to an actual `Blk`.
   */
  class IByHashTxGettable{
  public:
    virtual optional<Tx> getTxByHash(hash256 h) noexcept=0;
  };

  /**
   * @brief The interface of pool exposed to consensus's executor.
   *
   * 🦜 : This is pretty much everything the consensus's executor need from pool.
   */
  class IPoolSettable : public virtual IByHashTxGettable{
  public:
    virtual bool verifyTx(const Tx & t) const noexcept=0;
    virtual bool addTx(const Tx & t) noexcept=0;
  };

  /**
   * @brief The interface of pool exposed to sealer.
   *
   * The sealer is supposed to make a BlkForConsensus with the BlkNumber and the
   * Tx hashes obtained from here.
   *
   *  This need to be thread-safe.
   */
  class IForSealerTxHashesGettable{
  public:
    virtual vector<hash256> getTxHashesForSeal() noexcept=0;
  };

  /**
   * @brief The interface that can be queried by rpc.
   *
   * The interface that starts with `IForRpc` (usually) should inherit this.
   *
   * 🦜 :Of course, there're exceptions. `IForRpcNetworkable` for example is
   * just utility function that used by RPC
   */
  class IForRpc{
  public:
    virtual string info() noexcept=0;
  };

  /**
   * @brief The interface of the blockchain world exposed to RPC.
   *
   * This should be the only interface that the RPC can use to `change` the
   * world state.
   */
  class IForRpcTxsAddable: public virtual IForRpc{
  public:
    virtual bool addTxs(vector<Tx> && txs) noexcept=0;
  };

}

/**
 * @brief Some helper functions for stl
 */
namespace std {


  // 🦜: help std::less to use set<address>, converts to uint256 and compare.
  template<>
  struct less<evmc::address>{
    bool operator()(const evmc::address & l,
                    const evmc::address & r) const{
      intx::uint256 li = intx::be::load<intx::uint256,20>(l.bytes);
      intx::uint256 ri = intx::be::load<intx::uint256,20>(r.bytes);
      return li < ri;
    };
  };

  // 🦜: help std::less to use set<address>, converts to uint256 and compare.
  template<>
  struct less<ethash::hash256>{
    bool operator()(const ethash::hash256 & l,
                    const ethash::hash256 & r) const{
      intx::uint256 li = intx::be::load<intx::uint256,ethash::hash256>(l);
      // 🦜 : load a type which hash `.bytes` fields. ethash::hash256 is such a
      // type.
      intx::uint256 ri = intx::be::load<intx::uint256,ethash::hash256>(r);
      return li < ri;
    };
  };


  // 🦜 Define this to help ostream to display uint256
  ostream& operator<<(ostream& os, const intx::uint256& t){
    os << intx::to_string<256>(t,16); // base=10
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::address
  ostream& operator<<(ostream& os, const evmc::address& a){
    // os << weak::addressToString(a);
    os << intx::be::load<intx::uint<256>,evmc::address>(a);
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::bytes32
  ostream& operator<<(ostream& os, const evmc::bytes32& a){
    os << intx::be::load<intx::uint<256>,evmc::bytes32>(a);
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::bytes =
  // std::basic_string<uint8_t>
  ostream& operator<<(ostream& os, const std::basic_string<uint8_t>& a){
    // os << intx::be::load<intx::uint<256>,evmc::bytes32>(a);
    os << evmc::hex(a);         // bytes -> bytes_view ⇒ string
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::bytes_view =
  // std::basic_string_view<uint8_t>
  ostream& operator<<(ostream& os, const std::basic_string_view<uint8_t>& a){
    // os << intx::be::load<intx::uint<256>,evmc::bytes32>(a);
    os << evmc::hex(a);         // bytes_view ⇒ string
    return os;
  };

  // 🦜 Define this to help ostream to display ethash_hash256 == ethash::hash256
  ostream& operator<<(ostream& os, const ethash_hash256& h){
    // os << intx::be::load<intx::uint<256>,ethash_hash256>(h);
    os << weak::hashToString(h);
    return os;
  };

  /*
    Hash operator template specialization for ethash_hash256. Needed for
    unordered containers.

    🦜 : This is just the hash function defined in evmc.hpp. But the structure
    of ethash_hash256 and evmc::bytes32 are so similar, we can just kinda re-use
    that.
   */
  template <>
  struct hash<ethash_hash256>
  {
    /// Hash operator using FNV1a-based folding.
    constexpr size_t operator()(const ethash_hash256& s) const noexcept
    {
      using namespace evmc;
      using namespace fnv;
      return static_cast<size_t>(
                                 fnv1a_by64(fnv1a_by64(fnv1a_by64(fnv1a_by64(fnv::offset_basis, load64le(&s.bytes[0])),
                                                                  load64le(&s.bytes[8])),
                                                       load64le(&s.bytes[16])),
                                            load64le(&s.bytes[24])));
    }
  };
}


// 🦜 : Teach boost::json how to convert a hash
void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ethash_hash256 const& h ){
  jv.emplace_string() = weak::hashToString(h);
}

// Teach C++ to compare things
bool operator==(const ethash_hash256 &l, const ethash_hash256 &r){
  return std::equal(std::begin(l.bytes),
                    std::end(l.bytes) ,
                    std::begin(r.bytes));
};
bool operator!=(const ethash_hash256 &l, const ethash_hash256 &r){
  return not (l == r);
};
