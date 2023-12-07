/**
 * @file pure-netAsstn.hpp
 * @author Jianer Cong
 * @brief Common things for net asstns. Such as signed data
 */
#pragma once
#include "cnsss/pure-forCnsss.hpp"

#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <vector>
#include <bit>
#include <tuple>
#include <filesystem>


// 🦜 : We don't need backwards compatibility
#define OPENSSL_API_COMPAT 30000 // use 3.0.0 API
#define OPENSSL_NO_DEPRECATED 1
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
namespace pure{
  using std::bit_cast;
  using std::tuple;
  using std::make_tuple;
  using std::vector;
  using boost::lexical_cast;
  using boost::algorithm::split;
  using boost::lockfree::spsc_queue;
  namespace filesystem = std::filesystem;
  using std::filesystem::path;

  /**
   * @brief Interface for a message manager.
   *
   * The implementer is supposed to help IPBasedHttpNetAsstn managing the data
   * input and output.
   */
  class IMsgManageable{
  public:
    /**
     * @brief Prepare a msg to be sent.
     * @param data The data to be sent.
     * @return A signed msg, which should contains the following:
     *     1. who sent the data
     *     2. a digital signature
     *     3. the payload (the actual data)
     *
     * The returned string is supposed to be passed to the remote host's
     * `tear_msg_open()` function.
     *
     * 🐢 : Note that in order to prepare the data, the implementer should knows
     * "Who am I" (i.e. the identity of `this host`, usually the public key of this node)
     *
     * 🦜 : What does an endpoint usually look like ?
     *
     * 🐢 : It's usually have the form
     * "<pk><addr:port><"">". It should be created from something like:

     string pk,e, endpoint;
     pk = my_public_key();
     e = IPBasedHttpNetAsstn::combine_addr_port("10.0.0.1",1234);
     endpoint = SignedData.serialize_3_strs(pk,e,"");


    */
    virtual string prepare_msg(string && data)const noexcept =0;
    /**
     * @brief Tear a msg open.
     *
     * @param msg The message prepared by the sender, usually prepared by a
     * `prepare_msg()` call.
     * @return The pair <from,data> if the signature verification is passed.
     *
     * This function should unpack the `msg` recieved, verify the digital
     * signature therein and return the `sender` and `data` if the verification is
     * passed.
     */
    virtual optional<tuple<string,string>> tear_msg_open(string_view msg /*throw a string in */
                                                         )const noexcept =0;
    virtual string my_endpoint()const noexcept=0;

    virtual ~IMsgManageable() = default; // 🦜 : This is a virtual d'tor, we kinda need it for the linter..
  };                              // class IMsgManageable

  /**
   * @brief The signed data.
   *
   * This class wraps three string:
   *
   *     1. `from`, identifying the sender, usually a public key. This should have
   *     length >= 1.
   *
   *     2. `sig`, the signature, can be empty.
   *     3. `data`, the payload, can be empty.
   *
   * 🦜 : Because the structure of this class is simple enough, we parse and
   *     serialize ourselves. We use the following encoding. Suppose `s` is the
   *     serialized string, then
   *
   *     s = <from.size()-1><from>[<sig.size()><sig><data>]
   *
   *     In detail:
   *         + s[0] = from.size() - 1
   *         + s[1:1+from.size()] = from
   *         + s[1+from.size()] = sig.size()
   *         + s[2+from.size(),sig.size()] = sig
   *         + the rest is the data.
   *     Some examples:
   *
   *         + (from='a',sig='',data='') ⇒ '\x00' + 'a'
   *         + (from='a',sig='b',data='') ⇒ '\0x00' + 'a' + '\x01' + 'b'
   *         + (from='aa',sig='',data='bbb') ⇒ '\0x01' + 'aa' + '\0x00' + 'bbb'
   *         + (from='aa',sig='b',data='ccc') ⇒ '\0x01' + 'aa' + '\0x01' + 'b' + 'ccc'
   */

  class SignedData : public virtual ISerializable{
  public:
    SignedData() = default;
    SignedData( string f, string s="",string d=""): from(f),sig(s),data(d){
      if (f.empty())
        BOOST_THROW_EXCEPTION(std::runtime_error("SignedData.from cannot be empty."));
    }
    string from;
    string sig;
    string data;

    /**
     * @brief Parse the data from a string.
     *
     * This expects that the instance is just initialized. Example usage:
     *
     *     SignedData d;
     *     d.fromString(s);
     */
    bool fromString(string_view s) noexcept override{
      auto r = parse_3_strs(s);
      if (not r) return false;
      std::tie(this->from,this->sig,this->data) = r.value();
      return true;

    }

    string toString() const noexcept override{
      return serialize_3_strs(this->from, this->sig,this->data);
    }

    /**
     * @brief Parse three string.
     *
     * @param s The string that is to be parsed. It should have been serialized
     *  using `serialize_3_strs()`
     *
     * @return the parsed three string if successful.
     *
     *  🦜 : The core logic of this type is in fact parsing and serializing
     *  three strings. We can draw them out.
     */
    static optional<tuple<string,string,string>> parse_3_strs(string_view s){
      string s1,s2,s3;

      // BOOST_LOG_TRIVIAL(debug) << format("fromString() started");
      if (s.size() < 2) return {};
      // #include <bit>

      // BOOST_LOG_TRIVIAL(debug) << format("fromString() started to parse");
      // BOOST_LOG_TRIVIAL(debug) << format("started s=%s") % s;

      string_view::iterator it = s.begin();
      uint8_t l1 = bit_cast<uint8_t>(*(it++)) + 1; // f.size()
      // BOOST_LOG_TRIVIAL(debug) << format("l1 = %d") % static_cast<int>(l1);

      // s1.clear();
      s1.reserve(l1);

      for (int i = 0;i < l1;i++){
        s1 += *(it++);
        // BOOST_LOG_TRIVIAL(debug) << format("Now from=%s,i=%d") % s1 % i;
      }

      if (it == s.end()){
        // BOOST_LOG_TRIVIAL(trace) << "No more data to parse";
        return make_tuple(s1,s2,s3);
      }

      uint8_t l2 = bit_cast<uint8_t>(*(it++));
      s2.reserve(l2);
      for (int i = 0;i < l2;i++){
        s2 += *(it++);
        // BOOST_LOG_TRIVIAL(debug) << format("Now sig=%s,i=%d") % this->sig % i;
      }

      if (it == s.end()) return make_tuple(s1,s2,s3);  // no more data to parse
      s3.reserve(s.size() - l1 - l2);
      while (it != s.end()){
        s3 += *(it++);
      }

      return make_tuple(s1,s2,s3);  // no more data to parse
    }
    static string serialize_3_strs(string_view s1, string_view s2,string_view s3) noexcept {
      string s;
      s.reserve(2 + s1.size() +
                s2.size() +
                s3.size());

      s += bit_cast<char>(static_cast<uint8_t>(s1.size() - 1));
      s += s1;

      if (s2.empty() and s3.empty()){
        return s;
      } // 🦜 We can stop here only if both `sig` and `data` are empty.
      s += bit_cast<char>(static_cast<uint8_t>(s2.size()));
      s += s2;
      s += s3;
      return s;
    }
  };                            // class SignedData


  /**
   * @brief The message manager that does not sign at all.
   */
  class NaiveMsgMgr: public virtual IMsgManageable{
  public:
    const string ep;

    /**
     * @brief Construct a new Naive Msg Mgr object
     *
     * @param eep The endpoint of this node. It should be a string that can be parsed into a <public key> and <addr:port> pair.
     */
    NaiveMsgMgr(string eep): ep(eep){}
    string prepare_msg(string && data)const noexcept override{
      return SignedData(this->ep,"",data).toString();
    }

    optional<tuple<string,string>> tear_msg_open(string_view msg)const noexcept override {
      // BOOST_LOG_TRIVIAL(debug) << format("tearing msg open");
      SignedData d;
      if (not d.fromString(msg))
        return {};
      return make_tuple(d.from,d.data);
    }

    string my_endpoint()const noexcept override{
      return this->ep;
    }
  };                            // class NaiveMsgMgr

  // --------------------------------------------------
  /*
    the openssl_helper

    🦜 : These are the helper class that helps us manage the openssl stuff.

    🐢 : Where did you learn this?

    🦜 : An online tutorial, which I find quite useful, it tought how to use c++
    style smart pointers to manage the openssl stuff.

   */
  template<class T> struct DeleterOf;
  template<> struct DeleterOf<BIO> { void operator()(BIO *p) const { BIO_free_all(p); }};
  template<> struct DeleterOf<EVP_PKEY> {void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }};
  template<class OpenSSLType>
  using UniquePtr = std::unique_ptr<OpenSSLType, DeleterOf<OpenSSLType>>;

  /**
   * @brief The message manager that signs the data.
   *
   * 🐢 : This is the serious one. It depends on the `crypto` module from
   * OpenSSL. This should probably include all the code dependent on the OpenSSL
   * library.
   *
   * 🦜 : Okay, so what does this class contain ?
   *
   * 🐢 : I think this should accept the following:
   *
   *    1. The public key of CA.
   *
   *    2. The pair (<this-endpoint>, <secret-key>)
   *
   *    3. A map : {<e1> : <pk1> , <e2> : <pk2> , ...} that contains all the
   *    public keys of other nodes (peers).
   *
   *
   * 🦜 : Okay, but how do we store it in the RAM?
   *
   * 🐢 : I think it makes sense to store it as the UniquePtr<EVP_PKEY>, the
   * (wrapped) native OpenSSL key.
   *
   * 🦜 : But sometimes the incomming msg can contain the PKEY that we don't
   * know, i.e. the PKEY of the newcomer.
   *
   * 🐢 : Yeah , let's just for now keep the PKEY, and when we verify new keys,
   * we parse the key?
   *
   * 🦜 : Okay, let's just write some and see ?
   */
  class SslMsgMgr: public virtual IMsgManageable{
  public:

    UniquePtr<EVP_PKEY> ca_public_key;

    UniquePtr<EVP_PKEY> my_secret_key;
    string my_addr_port;
    string my_cert;

    /**
     * @brief Construct a new SslMsgMgr object
     *
     * @param my_sk_str The secret key of this node, in PEM format. @param
     * my_addr_port_str The endpoint of this node, in the form of <addr>:<port>
     * @param ca_pk_str The public key of the CA, in PEM format.If not provided,
     * all public keys are trusted. (so everyone can join)
     * 
     * @param mmy_cert The certificate of this endpoint, should be issued by the CA.
     *
     * 🦜 : It's always a tough decision to decide whether we should load things
     * from file or from string.
     *
     * 🐢 : Let's layout some principles:
     *
     *          All non-static methods of this class doesn't use the
     *          <filesystem> module. In another word, it doesn't read from file,
     *          and always use string(or raw bytes in string).
     *
     */
    explicit SslMsgMgr(string my_sk_pem,
                       string my_addr_port_str,
                       string mmy_cert = "",
                       string ca_pk_pem = ""
                       ):
      my_addr_port(my_addr_port_str),
      my_cert(mmy_cert)
    {
      BOOST_LOG_TRIVIAL(trace) <<  "🌱 Initializing " S_CYAN "SslMsgMgr" S_NOR;
      // 1. init the ca_public_key is provided
      if (not ca_pk_pem.empty()){
        auto r = load_key_from_pem(ca_pk_pem,false);
        if (not r){
          BOOST_THROW_EXCEPTION(std::runtime_error("❌ Error reading CA public key:\n" + ca_pk_pem));
        }
        this->ca_public_key = std::move(r.value());
      }else{
        BOOST_LOG_TRIVIAL(info) <<  S_MAGENTA "⚠️ No CA public key provided, all public keys are trusted." S_NOR;
        this->ca_public_key = nullptr;
      }

      // 2. init our secret key
      auto r = load_key_from_pem(my_sk_pem,true);
      if (not r){
        // BOOST_LOG_TRIVIAL(error) <<  S_RED "❌ Error reading my secret key:\n" + my_sk_pem + S_NOR;
        // exit(1);
        BOOST_THROW_EXCEPTION(std::runtime_error("❌ Error reading my secret key:\n" + my_sk_pem));
      }
      this->my_secret_key = std::move(r.value());

      /*
        3. (optional)

        🦜 : You know what? In fact I can just check that if my cert is valid to my ca.

        🐢 : Yeah, we should try that
       */

      if (this->we_check_cert()){
          string my_pk_pem = dump_key_to_pem(this->my_secret_key.get(), false /*is_secret*/);
          if (not this->do_verify(this->ca_public_key.get(), my_pk_pem, this->my_cert)){
            BOOST_THROW_EXCEPTION(std::runtime_error("❌️ My cert is not valid to my CA"));
          }
      }
    }

    /**
     * @brief Determine whether we check the certificate of the peer.
     */
    bool we_check_cert() const noexcept{
      return this->ca_public_key.get() != nullptr;
    }

    bool test_trusted_peer(string pk_pem, string cert) const noexcept {

      if (we_check_cert()){
        // 1. verify that the cert = CA.sign(pk_pem)
        if (not this->do_verify(this->ca_public_key.get(), pk_pem, cert)){
          BOOST_LOG_TRIVIAL(error) <<  S_RED "❌️ Key verification failed: " << pk_pem  << S_NOR;
          return false;
        }
      }
      return true;
    }

    /**
     * @brief Package the data
     *
     * 🦜 : This should pass the SignedData (ep, sig_of_data, data)
     * the endpoint tells the public key and the address usually formed as:
     *
     * string endpoint = ::pure::SignedData::serialize_3_strs("<my-pk-pem>","localhost:1234","<my-cert>");
     */
    string prepare_msg(string && data)const noexcept override{

      // BOOST_LOG_TRIVIAL(debug) << "Preparing msg with my_pk_pem = :"
      //                          << dump_key_to_pem(this->my_secret_key.get(), false /*is_secret*/);

      string sig = SslMsgMgr::do_sign(this->my_secret_key.get(),data);

      return SignedData(this->my_endpoint(),sig,data).toString();
    }

    optional<tuple<string,string>> tear_msg_open(string_view msg)const noexcept override {
      BOOST_LOG_TRIVIAL(debug) << format("tearing msg open");
      SignedData d;
      if (not d.fromString(msg)){
        BOOST_LOG_TRIVIAL(warning) <<  "⚠️ Wrong format for <msg>";
        return {};              // failed to parse data
}

      // --------------------------------------------------
      // get the from <endpoint>
      auto r = ::pure::SignedData::parse_3_strs(d.from);
      if (not r){
        BOOST_LOG_TRIVIAL(warning) <<  "⚠️ Wrong format for <endpoint>";
        return {};
      }

      // --------------------------------------------------
      /*
        2. verify the peer

        🐢 : Yeah.. It seems so, because we can't actually verify that the pk
        here is valid or not, because that requires a special signiture just for
        that. wait, why don't we just encode the sig in the endpoint?

       */
      auto [from_pk_pem, from_addr_port, from_cert] = r.value();
      // BOOST_LOG_TRIVIAL(debug) << "⚙️ Parsed from_pk_pem" << from_pk_pem;
      if (not test_trusted_peer(from_pk_pem, from_cert)){
        BOOST_LOG_TRIVIAL(warning) <<  S_MAGENTA "⚠️ Untrusted peer: " << from_pk_pem << S_NOR;
        return {};
      }
      // --------------------------------------------------
      /*
        3. finally, verify the signature
       */
      if (not SslMsgMgr::do_verify(from_pk_pem,d.data,d.sig)){
        BOOST_LOG_TRIVIAL(warning) <<  S_MAGENTA "⚠️ Signature verification failed" S_NOR;
        return {};
      }

      return make_tuple(d.from,d.data);
    }

    string my_endpoint()const noexcept override{
      /*
        🦜 : ⚠️ If you make it static, that's gonna be one for all instance (e.g.
        one per class), not one per instance...
       */
      string my_pk_pem = dump_key_to_pem(this->my_secret_key.get(), false /*is_secret*/);
      string ep = ::pure::SignedData::serialize_3_strs(my_pk_pem, this->my_addr_port,this->my_cert);
      return ep;
    }

    /**
     * @brief Read secret or public key from a string
     *
     * @param s : the content of a PEM file
     * @param is_secret : wether it's secret or public key
     */
    static optional<UniquePtr<EVP_PKEY>> load_key_from_pem(const std::string& s,
                                                           bool is_secret = true) {

      UniquePtr<BIO> bio(BIO_new_mem_buf(s.c_str(), -1));
      if (not bio.get()){
        BOOST_LOG_TRIVIAL(error) << S_RED "❌ Error reading bio from string" S_NOR;
        return {};
      }

      UniquePtr<EVP_PKEY> sk;
      if (is_secret){
        sk = UniquePtr<EVP_PKEY>(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
      }else{
        sk = UniquePtr<EVP_PKEY>(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));
      }
      if (not sk.get()){
        BOOST_LOG_TRIVIAL(error) << S_RED "❌️ Error reading secret key" S_NOR;
        return {};
      }
      return sk;
    }

    /**
     * @brief dump the key to a pem string
     */
    static string dump_key_to_pem(EVP_PKEY *key, bool is_secret = true){
      UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
      if (is_secret){
        PEM_write_bio_PrivateKey(bio.get(), key, nullptr, nullptr, 0, nullptr, nullptr);
      }else{
        PEM_write_bio_PUBKEY(bio.get(), key);
      }
      char *pem_data;
      long pem_size = BIO_get_mem_data(bio.get(), &pem_data);
      return string(pem_data, pem_size);
    }


    /**
     * @brief Read secret or public key from a string.
     */
    static optional<UniquePtr<EVP_PKEY>> load_key_from_file(path p, bool is_secret = true){
      FILE * fp = fopen(p.c_str(),"rb");
      UniquePtr<EVP_PKEY> sk;

      if (not fp){
        BOOST_LOG_TRIVIAL(error) << S_RED "❌️ Error reading file " << p << S_NOR;
        return {};
      }

      if (is_secret){
        sk = UniquePtr<EVP_PKEY>(PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr));
      }else{
        sk = UniquePtr<EVP_PKEY>(PEM_read_PUBKEY(fp, nullptr, nullptr, nullptr));
      }
      if (not sk.get()){
        BOOST_LOG_TRIVIAL(error) << S_RED "❌️ Error reading secret key" S_NOR;
        return {};
      }
      return sk;
    }

    /**
       ,* @brief Sign a message with a key (ED25519).
       ,* 🦜 : This function is copied from the Doc.
       ,*/
    static string do_sign(EVP_PKEY *ed_key, unsigned char *msg, size_t msg_len){
      size_t sig_len;
      unsigned char *sig = NULL;

      EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();

      EVP_DigestSignInit(md_ctx, NULL, NULL, NULL, ed_key);
      /* Calculate the requires size for the signature by passing a NULL buffer */
      EVP_DigestSign(md_ctx, NULL, &sig_len, msg, msg_len);
      sig = (unsigned char*) OPENSSL_zalloc(sig_len);
      EVP_DigestSign(md_ctx, sig, &sig_len, msg, msg_len);


      string sig_str((char*)sig, sig_len);

      OPENSSL_free(sig);
      EVP_MD_CTX_free(md_ctx);

      return sig_str;
    }

    static string do_sign(EVP_PKEY *ed_key, string msg){
      return do_sign(ed_key, (unsigned char*)msg.c_str(), msg.size());
    }

    static bool do_verify(EVP_PKEY *ed_key,const string msg, const string sig){
      // 🐢 : Use EVP_DigestVerify
      // init the contex
      EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
      if (EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL, ed_key) != 1){
        BOOST_LOG_TRIVIAL(debug) << "❌️ EVP_DigestVerifyInit failed";
        return false;
      }

      int r = EVP_DigestVerify(md_ctx, (unsigned char*)sig.c_str(), sig.size(),
                               (unsigned char*)msg.c_str(), msg.size());
      // free the context
      EVP_MD_CTX_free(md_ctx);

      return r == 1;
    }

    static bool do_verify(string ed_key_pem, const string msg, const string sig){
      auto r = load_key_from_pem(ed_key_pem,false);
      if (not r){
        BOOST_LOG_TRIVIAL(error) << S_RED "❌️ Error reading public key" S_NOR;
        return false;
      }
      UniquePtr<EVP_PKEY> ed_key = std::move(r.value());
      bool ok = do_verify(ed_key.get(),msg,sig);
      if (not ok){
        BOOST_LOG_TRIVIAL(debug) <<  S_RED "❌️ Signature verification failed with pk_pem: \n" << ed_key_pem
                                 << "\nmsg:" << msg << "\nsig-len:" << sig.size() <<
          S_NOR;
}
      return ok;
      // return do_verify(r.value().get(),msg,sig);
    }



    /**
     * @brief Get raw public or secret key in bytes
     */
    static optional<string> key_to_raw(const EVP_PKEY *  const p, bool is_secret=false) noexcept{
      char raw_key[40];
      // resize to 32 bytes

      // get the raw is_secretate key
      size_t L{40};                 // 🦜 : In fact, 32 bytes is just enough.
      // L = the size of the buffer when calling the function, the the function will
      // change it to the number of bytes written after the call.
      int r;
      if (is_secret){
        r = EVP_PKEY_get_raw_private_key(p, (unsigned char*)raw_key, &L);
      }else{
        r = EVP_PKEY_get_raw_public_key(p, (unsigned char*)raw_key, &L);
      }
      if (r != 1 or L != 32){
        return {};
      }

      return string(raw_key, L);
    }

    static UniquePtr<EVP_PKEY> raw_to_key(const string_view raw_key, bool is_secret=false) noexcept {

      if (is_secret){
        return UniquePtr<EVP_PKEY>(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                                                reinterpret_cast<const unsigned char*>(raw_key.data()),
                                                                raw_key.size()));
      }else{
        // return P(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, data, len),f);
        return UniquePtr<EVP_PKEY>(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL,
                                                               reinterpret_cast<const unsigned char*>(raw_key.data()),
                                                               raw_key.size()));
      }
    }

    static UniquePtr<EVP_PKEY> extract_public_key(EVP_PKEY *sk) noexcept{
      // get raw public key
      auto r = key_to_raw(sk,false /*is_secret*/);
      if (not r) return {};
      string raw_key = r.value();
      return raw_to_key(raw_key,false /*is_secret*/);
    }

    static void print_key(EVP_PKEY *p,bool is_secret=false) noexcept {
      if (is_secret)
        EVP_PKEY_print_private_fp(stdout, p, 2, NULL); // print both pub and secret
      else
        EVP_PKEY_print_public_fp(stdout, p, 2, NULL); // print pub
    }

  };                            // class SslMsgMgr

  // --------------------------------------------------



  /**
   * @brief The base class for network Asstn.
   *
   * 🦜 : This one is created because IPBasedHttpNetAsstn and IPBasedUdpNetAsstn
   * shared these code.
   */
  class NetAsstn : public virtual IForCnsssNetworkable{
  public:
    IMsgManageable * const mgr;
    NetAsstn(IMsgManageable * const m): mgr(m){}

    string listened_endpoint() noexcept override{
      return this->mgr->my_endpoint();
    }

    static tuple<string,uint16_t> split_addr_port(string s){
      using boost::algorithm::split;
      using boost::is_any_of;
      vector<string> r;
      split(r,s,is_any_of(":"));
      if (r.size() != 2)
        BOOST_THROW_EXCEPTION(std::runtime_error(format("Error splitting <addr>:<port>").str()));
      return make_tuple(r[0],lexical_cast<uint16_t>(r[1]));
    }

    static optional<string> extract_addr_and_port_from_endpoint(const string & endpoint){
      /*
        🦜 : parse the endpoint, which should have the form
        "<pk><addr:port><crt-of-pk>". It should be created from something like:

        string pk,e, endpoint,crt;
        pk = my_public_key();
        crt = my_cert();
        e = IPBasedHttpNetAsstn::combine_addr_port("my_endpoint",1234);
        endpoint = SignedData.serialize_3_strs(pk,e,crt);

        🐢 : Note that, in this function, we ignored the "pk"
      */

      optional<tuple<string,string,string>> r = SignedData::parse_3_strs(endpoint);
      if (not r){
        return {};
      }

      string addr_and_port;
      std::tie(std::ignore, /*pk is ignored*/
               addr_and_port,/*<addr>:<port>*/
               std::ignore /*crt is ignored*/) = r.value();

      return addr_and_port;
    }

    /*
      🦜 : It looks like this function is unused?

      🐢 : Or I think it should be used on the client side...
    */
    static string combine_addr_port(string addr,uint16_t port){
      return addr + ':' + boost::lexical_cast<std::string>(port);
      // return (format("%s:%d") % addr % port).str();
    }
  };

} // namespace pure

