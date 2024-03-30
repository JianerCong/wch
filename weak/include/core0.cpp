#include "core0.hpp"

namespace weak{


  void writeToFile(path p, string_view content){
    // trunc :: clear the file if it exists
    BOOST_LOG_TRIVIAL(debug) <<  "writing to the file: " << p << " content: " << content;
    (ofstream(p.c_str(), std::ios::out | std::ios::trunc | std::ios::binary) << content).flush();
    // 🦜 : Make sure we used the binary mode, so that the content is written as is. (char wouldn't be escaped
  }

  string readAllText(path p){
    string s;
    // std::ifstream(p.c_str()) >> s;
    std::ifstream file(p.c_str());
    if (not file.is_open()) {
      BOOST_LOG_TRIVIAL(debug) << "❌️ Failed to open the file: " S_RED << p.string() << S_NOR " for reading";
      return "";
    }

    // 1. reserve the space
    s.reserve(filesystem::file_size(p));

    // get char by char (🦜 : The official example from https://cplusplus.com/reference/string/string/reserve/)
    while (not file.eof()){
      s += file.get();
    }

    // ⚠️ Caveat: drop the last char (it's EOF)
    s.pop_back();

    BOOST_LOG_TRIVIAL(debug) <<  "🦜 Text read from file " S_CYAN << p.string() << S_NOR ":\n" S_GREEN << s << S_NOR  "\n";;
    return s;
  }

  bool contains(json::array a, string s){
    for (const json::value & v : a){
      if (v.as_string() == s) return true;
    }
    return false;
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
  const char* evmc_call_kind_to_string(enum evmc_call_kind k){
    switch (k){
    case EVMC_CALL: return "EVMC_CALL";
    case EVMC_DELEGATECALL: return "EVMC_DELEGATECALL";
    case EVMC_CALLCODE: return "EVMC_CALLCODE";
    case EVMC_CREATE: return "EVMC_CREATE";
    case EVMC_CREATE2: return "EVMC_CREATE2";
    }
    return "<unknown>";
  }


  tuple<evmc_message,evmc::bytes> makeMsg(string_view code_hex,
                                          string_view input_hex,
                                          evmc_call_kind k
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


  json::value Acn::toJson() const noexcept {
    BOOST_LOG_TRIVIAL(debug) << format("📗Jsonizing Acn");
    return json::value_from(*this);
  };

  bool Acn::fromJson(const json::value &v) noexcept {
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

  hiPb::Acn Acn::toPb() const {
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

  void Acn::fromPb(const hiPb::Acn & pb){
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


  // <2024-01-30 Tue>
  hiPb::Tx Tx::toPb() const{
    hiPb::Tx pb;

    pb.set_type(Tx::typeToPb(this->type));

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
  void Tx::fromPb(const hiPb::Tx & pb){
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

      // parse the rest (nonce,timestamp,data,to,type, these must exist, 🦜 : Let's manually check that)
      this->nonce = pb.nonce();
      this->timestamp = pb.timestamp();
      this->data = weak::bytesFromString(pb.data());

      // if it has a type field, use it , Otherwise set it to evm (default)
      this->type = Tx::typeFromPb(pb.type());

      // parse the to
      s = pb.to_addr();
      this->to = weak::fromByteString<address>(s); // throws for bad lengths
    }


  void Tx::fromJson0(const json::object &o){
      // BOOST_LOG_TRIVIAL(debug) <<  "fromJson0 called with : "<< o;
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

      // BOOST_LOG_TRIVIAL(debug) <<  "fromJson0 done with " << this->toJson();

      /// Decodes hex-encoded string into custom type T with .bytes array of
      /// uint8_t. so ethash::hash256 is such type
      // h= value_to<string>(o.at("hash"));
      // optional<hash256> oh;
      // oh = evmc::from_hex<hash256>(h);
      // if (not oh) BOOST_THROW_EXCEPTION(std::runtime_error("Invalid hash = " + h));
      // this->hash = oh.value();
    }


  bool Tx::fromJson(const json::value &v) noexcept {
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


  json::value Tx::toJson() const noexcept {
      return json::value_from(*this);
    };


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


  json::value BlkHeader::toJson() const noexcept {
      return json::value_from(*this);
    }

  bool BlkHeader::fromJson(const json::value &v) noexcept{

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

  json::value Blk::toJson() const noexcept {
      return json::value_from(*this);
    }
  bool Blk::fromJson(const json::value &v) noexcept {

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



} // weak



/**
 * @brief Some helper functions for stl
 */
namespace std {

  // 🦜 : Order matters here.
  // 🦜 Define this to help ostream to display uint256
  ostream& operator<<(ostream& os, const intx::uint256& t){
    os << intx::to_string<256>(t,16); // base=10
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::bytes32
  ostream& operator<<(ostream& os, const evmc::bytes32& a){
    os << intx::be::load<intx::uint<256>,evmc::bytes32>(a);
    return os;
  };


  // 🦜 Define this to help ostream to display evmc::address
  ostream& operator<<(ostream& os, const evmc::address& a){
    // os << weak::addressToString(a);
    os << intx::be::load<intx::uint<256>,evmc::address>(a);
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::bytes =
  // std::basic_string<uint8_t>
  ostream& operator<<(ostream& os, const std::basic_string<uint8_t>& a){
    // os << intx::be::load<intx::uint<256>,evmc::bytes32>(a);
    os << evmc::hex(a);         // bytes -> bytes_view ⇒ string
    return os;
  };
}
