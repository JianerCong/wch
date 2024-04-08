// 🦜 : In this file, we move some implementation to cpps
#pragma once


#include <boost/lexical_cast.hpp>
#include <string>
#include <ctime>
#include <vector>

#include <boost/throw_exception.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include ".generated_pb/hi.pb.h"
#include "pure-common.hpp"
#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
#include <boost/format.hpp>
#include <fstream>
#include <string_view>
#include <filesystem>
#include <boost/json.hpp>

#include <intx/intx.hpp>
#include <memory>
#include <tuple> // for tuple
#include <ethash/keccak.hpp>
#include <evmc/evmc.hpp>
#include <evmc/hex.hpp>
// colors
#define S_RED     "\x1b[31m"
#define S_GREEN   "\x1b[32m"
#define S_YELLOW  "\x1b[33m"
#define S_BLUE    "\x1b[34m"
#define S_MAGENTA "\x1b[35m"
#define S_CYAN    "\x1b[36m"
#define S_NOR "\x1b[0m"

namespace weak{

  using std::move;

  using pure::IJsonizable;
  using pure::ISerializable;
  using pure::ISerializableInPb;
  using pure::pluralizeOn;
  using std::tuple;
  using std::make_tuple;
  using ethash::hash256;
  using boost::format;
  namespace json = boost::json;
  using json::value_to;
  using std::ofstream;
  using std::string_view;
  using std::string;
  namespace filesystem = std::filesystem;
  using std::filesystem::path;
  using std::filesystem::current_path;

  using evmc::bytes;              // in hex.cpp
  using evmc::bytes;              // in hex.cpp
  using evmc::bytes_view;              // in hex.cpp

  using evmc::address;
  using evmc::bytes_view;              // in hex.cpp
  using evmc::address;
  // These two are actually very similar, all have member `uint8_t bytes[32]`
  // We use hash256 when we want to make it clear that it's a hash.
  using evmc::bytes32;

  using std::vector;
  using std::optional;
  using std::unique_ptr;
  using std::shared_ptr;
  using std::make_unique;
  using std::make_shared;

  bool contains(json::array a, string s);

  inline string resultToString(const evmc::Result& result){
    bytes o{result.output_data,result.output_size};
    return evmc::hex(o);
  }

  /**
   * @brief Convert a type with a member `uint8_t bytes[?]` to string.
   *
   * @param t The type to convert. Options include `evmc::address`, `evmc::bytes32`
   */
  template<typename T>
  static string toByteString(const T t){
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
  // <2024-04-01 Mon> 🦜 : g++ says  explicit template specialization cannot have a storage class

  template<typename T>
  static T fromByteString(string_view s){
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
  inline string toString(const bytes & b){
    return string(reinterpret_cast<const char*>(b.data()),b.size());
  } // ^v a pair
  inline bytes bytesFromString(string_view s){
    return bytes(reinterpret_cast<const uint8_t*>(s.data()),s.size());
  }
  inline string_view toStringView(const bytes & b){
    return string_view(reinterpret_cast<const char*>(b.data()),b.size());
  }
  // byte32 <-> string
  inline string toString(const bytes32 & b){
    return string(reinterpret_cast<const char*>(b.bytes),sizeof(b.bytes));
    // return string(reinterpret_cast<const char*>(
  }

  inline bytes32 bytes32FromString(string_view s){
    if (s.size() < 32){
      BOOST_THROW_EXCEPTION(std::runtime_error((format("Invalid size for bytes32, should be at least 32, but got %d") % s.size()).str()));
    }
    bytes32 b;
    std::copy_n(reinterpret_cast<const uint8_t*>(s.data()),32,std::begin(b.bytes));
    return b;
  }

  inline evmc::address makeAddress(int x) noexcept {
    // string s = (format("0x%020x") % x).str();
    // BOOST_LOG_TRIVIAL(trace) <<  format("🐸 Making address %s from %d \n") % s % x;
    // BOOST_LOG_TRIVIAL(trace) <<  format(  "Making addr" S_GREEN " %d" S_NOR) % x;
    return evmc::address(static_cast<uint64_t>(x));
  }

  string hashToString(const ethash::hash256 & h) noexcept;
  string addressToString(evmc::address a) noexcept;

  void logMsg(const evmc_message &msg)noexcept ;
  tuple<evmc_message,evmc::bytes> makeMsg(string_view code_hex="",
                                          string_view input_hex="",
                                          evmc_call_kind k = EVMC_CALL
                                          ) noexcept;


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
    hiPb::Acn toPb() const override ;
    // hashcode a calculated property
    void fromPb(const hiPb::Acn & pb) override;


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
    json::value toJson() const noexcept override;
    bool fromJson(const json::value &v) noexcept override;
  };

  class Tx;

  class ITxVerifiable {
  public:
    virtual bool verify(const Tx & t)const = 0;
    void filterTxs(vector<Tx> & txs){
      // for (vector<Tx>::iterator it = txs.begin();it != txs.end();){
      //   if (not verify(*it)){
      //     // 🦜 : If the verifier is here and it blocked you....
      //     BOOST_LOG_TRIVIAL(debug) << format("❌️ Failed to verify tx-%d") % it->nonce;
      //     it = txs.erase(it);
      //   }else{
      //     it++;
      //   }
      // }
      auto erased = std::erase_if(txs,[this](const Tx & t){
        return not this->verify(t);
      });
      BOOST_LOG_TRIVIAL(debug) <<  "📗️ Erased " << erased << " tx" + pure::pluralizeOn(erased);
    }
  };


  class Tx: virtual public IJsonizable
          , virtual public ISerializableInPb<hiPb::Tx>
          , virtual public ISerializable
  {
  public:
    hiPb::Tx toPb() const override;
    void fromPb(const hiPb::Tx & pb) override;

    Tx() = default; // Upon you make a constructor yourself, the build-in
                    // constructor is gone,use this to bring it back

    // --------------------------------------------------
    // Type related
    enum class Type {evm, data, python};

    /**
     * @brief get the payload that we need to sign
     */
    string getToSignPayload() const noexcept{
      // return std::to_string(this->nonce) + weak::toString(this->data);
      return getToSignPayload0(this->nonce,weak::toString(this->data));
    }
    static string getToSignPayload0(const uint64_t nonce, const string & data) noexcept{
      return std::to_string(nonce) + data;
    }

    static string typeToString(Type t){
      static const string s[] = {"evm", "data", "python"};
      return string(s[static_cast<int>(t)]);
    }

    static hiPb::TxType typeToPb(Type t){
      if (t == Type::data) return hiPb::TxType::DATA;
      if (t == Type::python) return hiPb::TxType::PYTHON;
      return hiPb::TxType::EVM;
    }
    static Type typeFromPb(hiPb::TxType t){
      if (t == hiPb::TxType::DATA) return Type::data;
      if (t == hiPb::TxType::PYTHON) return Type::python;
      return Type::evm;
    }

    static Type typeFromString(string_view s){
      BOOST_LOG_TRIVIAL(debug) <<  "typeFromString called with : "<< s;
      if (s == "" or s == "evm") return Type::evm;
      if (s == "data") return Type::data;
      if (s == "python") return Type::python;
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
    void fromJson0(const json::object &o);
    bool fromJson(const json::value &v) noexcept override ;

    json::value toJson() const noexcept override ;

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
    static optional<tuple<string,vector<Tx>>> parse_txs_jsonString_for_rpc(string_view s, ITxVerifiable * const txf=nullptr) noexcept {
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

        return Tx::parse_txs_json_for_rpc(move(a), txf);
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
    static optional<tuple<string,vector<Tx>>> parse_txs_json_for_rpc(json::array && a, ITxVerifiable * const txf=nullptr) {
      json::array os;           // output object

      vector<Tx> txs;
      for (int i=0;i<a.size();i++){
        json::object o = a[i].as_object(); // the tx object

        // 🦜 : but we need to add a `timestamp` field for the client, and
        // because this is a required field, so we kinda need to add it here.
        // This is unlike pb, where all fields are optional.

        o.emplace("timestamp",std::time(nullptr));

        // <2024-03-21 Thu> 🦜 : If data starts with '@', then it's a text file
        // (learnt from curl), so we need to read it.
        if (o.contains("data") and
            o.at("data").is_string() and
            o.at("data").as_string().starts_with("@")){
          path p{string_view(o.at("data").as_string().subview(1))};
          // 🦜 : boost said it's convertible to std::string_view
          if (not exists(p)){
            BOOST_LOG_TRIVIAL(debug) << "❌️ Error reading data from file (file not found):"  S_RED << p << S_NOR;
            return {};
          }
          o["data"] = json::string(pure::readAllText(p));
        }

        Tx tx;
        tx.fromJson0(o);
        // tx.timestamp = std::time(nullptr);

        // <2024-04-08 Mon> 🦜 : If the verifier is set, then we need to verify the tx
        if (txf and not txf->verify(tx)){
          BOOST_LOG_TRIVIAL(debug) << format("❌️ Failed to verify tx-%d's signature/certificate") % tx.nonce;
          continue;
        }

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
    static optional<tuple<string,vector<Tx>>> parse_txs_pbString_for_rpc(string_view s, ITxVerifiable * const txf=nullptr) noexcept {
      hiPb::Txs pb;
      if (not pb.ParseFromString(string(s))){
        BOOST_LOG_TRIVIAL(debug) << format("❌️ Error parsing Txs pbString");
        return {};
      }

      return Tx::parse_txs_pb_for_rpc(pb, txf);
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
    static optional<tuple<string,vector<Tx>>> parse_txs_pb_for_rpc(const hiPb::Txs & pb, ITxVerifiable * const txf=nullptr) {
      hiPb::AddTxsReply atx;    // the output object

      vector<Tx> txs;
      try{
        for (const hiPb::Tx & tx : pb.txs()){
          Tx t;
          t.fromPb(tx);         // may throw

          // 🦜 : similarly, add the `timestamp` field for the client
          t.timestamp = std::time(nullptr);

          // <2024-04-08 Mon> 🦜 : If the verifier is set, then we need to verify the tx
          if (txf and not txf->verify(t)){
            BOOST_LOG_TRIVIAL(debug) << format("❌️ Failed to verify tx-%d's signature/certificate") % t.nonce;
            continue;
          }

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

  void tag_invoke(json::value_from_tag, json::value& jv, Tx const& c );

  // This helper function deduces the type and assigns the value with the matching key
  // 🦜 : Defining this allows us to use json::value_to<A>
  // Tx tag_invoke(json::value_to_tag<Tx>, json::value const& v ) noexcept{
  //   Tx t;
  //   if (t.fromJson(v)) return t;
  //   return {};
  // }
  // 🦜 ^^^ above is replaced by the following
  ADD_FROM_JSON_TAG_INVOKE(Tx);


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

    json::value toJson() const noexcept override ;
    bool fromJson(const json::value &v) noexcept override ;
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

    json::value toJson() const noexcept override ;
    bool fromJson(const json::value &v) noexcept override ;
  };

  // json functions for BlkHeader
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, BlkHeader const& b );

  // json functions for Blk
  // 🦜 : Defining this method allows us to use json::serialize(value_from(t))
  void tag_invoke(json::value_from_tag, json::value& jv, Blk const& b );

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

} // weak


/**
 * @brief Some helper functions for stl
 */
namespace std {

  // 🦜 : Order matters here.
  // 🦜 Define this to help ostream to display uint256
  static ostream& operator<<(ostream& os, const intx::uint256& t){
    os << intx::to_string<256>(t,16); // base=10
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::bytes32
  static ostream& operator<<(ostream& os, const evmc::bytes32& a){
    os << intx::be::load<intx::uint<256>,evmc::bytes32>(a);
    return os;
  };


  // 🦜 Define this to help ostream to display evmc::address
  static ostream& operator<<(ostream& os, const evmc::address& a){
    // os << weak::addressToString(a);
    os << intx::be::load<intx::uint<256>,evmc::address>(a);
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::bytes =
  // std::basic_string<uint8_t>
  static ostream& operator<<(ostream& os, const std::basic_string<uint8_t>& a){
    // os << intx::be::load<intx::uint<256>,evmc::bytes32>(a);
    os << evmc::hex(a);         // bytes -> bytes_view ⇒ string
    return os;
  };
}

// 🦜 : Teach boost::json how to convert a hash
static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ethash_hash256 const& h ){
  jv.emplace_string() = weak::hashToString(h);
}
