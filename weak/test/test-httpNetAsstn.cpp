#include "h.hpp"
#include "net/pure-httpNetAsstn.hpp"

#include <fstream>      // std::ofstream

using namespace pure;

// {{{ test_some_stl_and_boost

BOOST_AUTO_TEST_SUITE(test_some_stl_and_boost);
template<typename T>
void veq(vector<T> v, vector<T> v2){
  for (int i=0;i<v.size();i++) \
    BOOST_CHECK_EQUAL(v[i],v2[i]);
};

BOOST_AUTO_TEST_CASE(test_split){
  string s{"aaa\tbbb\tccc"};
  vector<string> r;
  split(r,s,boost::is_any_of("\t"));
  BOOST_CHECK_EQUAL(r.size(),3);
  veq(r,{"aaa","bbb","ccc"});
}


BOOST_AUTO_TEST_CASE(test_string_to_int){
  BOOST_CHECK_THROW({
      int i = lexical_cast<int>("abc");
    },boost::bad_lexical_cast);
  BOOST_CHECK_EQUAL(lexical_cast<int>("123"),123);
}
BOOST_AUTO_TEST_SUITE_END();

BOOST_AUTO_TEST_SUITE(test_asstn_helpers);
BOOST_AUTO_TEST_CASE(test_split_addr_port){
  auto split_addr_port = IPBasedHttpNetAsstn::split_addr_port;
  // take the static function
  BOOST_CHECK_THROW(split_addr_port("abc"),std::exception);
  BOOST_CHECK_THROW(split_addr_port("abc:abc"),boost::bad_lexical_cast);

  auto [a,p] = split_addr_port("abc:1234");
  BOOST_CHECK_EQUAL(a,"abc");
  BOOST_CHECK_EQUAL(p,1234);
}

BOOST_AUTO_TEST_CASE(test_combine_addr_port){
  auto combine_addr_port = IPBasedHttpNetAsstn::combine_addr_port;
  BOOST_CHECK_EQUAL(combine_addr_port("abc",12),"abc:12");
}
BOOST_AUTO_TEST_SUITE_END();

// }}}

// {{{ msg_mgr related (msg conversion)

BOOST_AUTO_TEST_SUITE(test_msg_mgr_related);

void check_toString(string f,string s, string d, string o){
  SignedData sd{f,s,d};
  BOOST_CHECK_EQUAL(sd.toString(), o);
}

char b2c(uint8_t b){
  return std::bit_cast<char>(b);
}

BOOST_AUTO_TEST_CASE(test_signedData_toString){
  check_toString("a","","", b2c(0) + string("a"));
  check_toString("a","b","", b2c(0) + string("a") + b2c(1) + "b");
  check_toString("aa","","bbb",b2c(1) + string("aa") + b2c(0) + "bbb");
  check_toString("aa","b","ccc",b2c(1) + string("aa") + b2c(1) + "b" + "ccc");
}

void check_fromString(string f,string s, string d, string o){
  SignedData sd;
  sd.fromString(o);
  BOOST_CHECK_EQUAL(sd.from,f);
  BOOST_CHECK_EQUAL(sd.sig,s);
  BOOST_CHECK_EQUAL(sd.data,d);
}

BOOST_AUTO_TEST_CASE(test_signed_Data_fromString){
  check_fromString("a","","", b2c(0) + string("a"));
  check_fromString("a","b","", b2c(0) + string("a") + b2c(1) + "b");
  check_fromString("aa","","bbb",b2c(1) + string("aa") + b2c(0) + "bbb");
  check_fromString("aa","b","ccc",b2c(1) + string("aa") + b2c(1) + "b" + "ccc");
}


namespace mockedMsgMgr {
  class A : public virtual IMsgManageable{
  public:
    string prepare_msg(string && data)const noexcept override{
      return data;
    }
    optional<tuple<string,string>> tear_msg_open(string_view msg)const noexcept override {
      return make_tuple("N1",string(msg));
    }
    string my_endpoint()const noexcept override{
      return "N0";
    }
  };
}

BOOST_AUTO_TEST_CASE(test_mockedMsgMgr){
  mockedMsgMgr::A mh;
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);

  BOOST_CHECK_EQUAL(m->prepare_msg("abc"),"abc");
  auto r = m->tear_msg_open("abc");

  BOOST_REQUIRE(r);
  auto [from, data] = r.value();
  BOOST_CHECK_EQUAL(from,"N1");
  BOOST_CHECK_EQUAL(data,"abc");
  BOOST_CHECK_EQUAL(m->my_endpoint(),"N0");
}

BOOST_AUTO_TEST_CASE(test_NaiveMsgMgr){
  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);

  string msg = b2c(1) + string("N0") + b2c(0) + "abc"; // pack the msg manually
  BOOST_CHECK_EQUAL(m->prepare_msg("abc"),msg);

  auto r = m->tear_msg_open(std::move(msg));
  BOOST_CHECK(r);
  auto  [from, data] = r.value();
  BOOST_CHECK_EQUAL(from,"N0");
  BOOST_CHECK_EQUAL(data,"abc");

  BOOST_CHECK_EQUAL(m->my_endpoint(),"N0");
}

BOOST_AUTO_TEST_SUITE_END();

// }}}

// {{{ test_the_overriden_methods

BOOST_AUTO_TEST_SUITE(test_the_overriden_methods);

BOOST_AUTO_TEST_CASE(test_listend_endpoint){

  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);
  IPBasedHttpNetAsstn a{nullptr,m};

  /*
    🐢 : Here we passed a nullptr, because we are not going to call any of the
    listen() and clear(). The following will cause SIGABORT
    a.listen(...);
  */

  IEndpointBasedNetworkable * n = dynamic_cast<IEndpointBasedNetworkable*>(&a);
  BOOST_CHECK_EQUAL(n->listened_endpoint(),"N0");
}

BOOST_AUTO_TEST_CASE(test_make_post_handler_basic){
  /*
    🦜 : the make_post_handler() also doesn't need a server, so we can pass a nulptr
  */

  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);
  IPBasedHttpNetAsstn a{nullptr,m};

  function<optional<string>(string,string)> h = [](string from,
                                                   string data)->optional<string>{
    return (format("from=%s,data=%s") % from % data ).str();
  };

  auto h1 = a.make_post_handler(h);

  string msg = m->prepare_msg("aaa");
  auto [ok,res]  = h1("irrelevent-host",1234,msg);
  BOOST_CHECK_EQUAL(res,"from=N0,data=aaa"); // okay

  msg = "";                     // bad parse
  std::tie(ok,res)  = h1("irrelevent-host",1234,msg);
  BOOST_CHECK(not ok);
  BOOST_CHECK(res.starts_with("❌️ Error unpacking")); // okay
}


BOOST_AUTO_TEST_CASE(test_make_post_bad_handler){
  /*
    🦜 : the make_post_handler() also doesn't need a server, so we can pass a nulptr
  */

  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);
  IPBasedHttpNetAsstn a{nullptr,m};

  function<optional<string>(string,string)> h = [](string from,
                                                   string data)->optional<string>{
    return {};
  };

  auto h1 = a.make_post_handler(h);

  string msg = m->prepare_msg("aaa");
  auto [ok,res]  = h1("irrelevent-host",1234,msg);
  BOOST_CHECK( not ok);
  BOOST_CHECK(res.starts_with("Error executing handler")); // okay
}

BOOST_AUTO_TEST_SUITE(test_SslMsgMgr, MY_TEST_THIS);

BOOST_AUTO_TEST_CASE(test_load_keys_from_pem){
  string s = "-----BEGIN PRIVATE KEY-----\n"
    "MC4CAQAwBQYDK2VwBCIEIDdCupRSMP7AqAT50TZwDzlYIfrgDpLL+km+0usqrWpB\n"
    "-----END PRIVATE KEY-----\n";

  string p = "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VwAyEAag4tsjNQHNpXrWkEfTEygtwjjXZKXZJJ2/09srM0RDs=\n"
    "-----END PUBLIC KEY-----\n";

  auto r = SslMsgMgr::load_key_from_pem(s,true /*is_secret*/);
  auto r1 = SslMsgMgr::load_key_from_pem(p,false /*is_secret*/);
  auto r2 = SslMsgMgr::load_key_from_pem("123",false /*is_secret*/);

  BOOST_REQUIRE(r.value());
  BOOST_REQUIRE(r1.value());
  BOOST_REQUIRE(not r2);
  // UniquePtr<EVP_PKEY> sk = r.value();
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r.value().get()),"ED25519");
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r1.value().get()),"ED25519");
}


BOOST_AUTO_TEST_CASE(test_dump_key_to_pem1){
  UniquePtr<EVP_PKEY> sk1(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"));
  UniquePtr<EVP_PKEY> pk1 = SslMsgMgr::extract_public_key(sk1.get());

  string pem = SslMsgMgr::dump_key_to_pem(sk1.get(), true /*is_secret*/);
  string pem1 = SslMsgMgr::dump_key_to_pem(pk1.get(), false /*is_secret*/);
  BOOST_TEST_MESSAGE( "dumped sec key: " + pem );
  BOOST_TEST_MESSAGE( "dumped pub key: " + pem1 );

  auto r = SslMsgMgr::load_key_from_pem(pem,true /*is_secret*/);
  BOOST_REQUIRE(r);
  BOOST_REQUIRE(pem.starts_with("-----BEGIN PRIVATE KEY-----"));
  BOOST_REQUIRE(pem1.starts_with("-----BEGIN PUBLIC KEY-----"));
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r.value().get()),"ED25519");
}

BOOST_AUTO_TEST_CASE(test_extract_public_key){

  UniquePtr<EVP_PKEY> sk1(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"));
  BOOST_TEST_MESSAGE("Both Parts:");

  SslMsgMgr::print_key(sk1.get(), true /* sk*/);
  auto pk1 = SslMsgMgr::extract_public_key(sk1.get());
  BOOST_REQUIRE(pk1);
  BOOST_TEST_MESSAGE("Public Part:");
  SslMsgMgr::print_key(pk1.get(), false /* is_secret*/);
}

BOOST_AUTO_TEST_CASE(test_load_keys_from_file){
  string s = "-----BEGIN PRIVATE KEY-----\n"
    "MC4CAQAwBQYDK2VwBCIEIDdCupRSMP7AqAT50TZwDzlYIfrgDpLL+km+0usqrWpB\n"
    "-----END PRIVATE KEY-----\n";
  /*
    🦜 : Do not delete a single '-' in this string, otherwise the following will
    cause segmentation fault.
   */

  string p = "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VwAyEAag4tsjNQHNpXrWkEfTEygtwjjXZKXZJJ2/09srM0RDs=\n"
    "-----END PUBLIC KEY-----\n";

  // write to tmpdir
  path f = filesystem::temp_directory_path() / "secret.pem";
  path f1 = filesystem::temp_directory_path() / "public.pem";
  // remove the file if it exists
  if (filesystem::exists(f)) filesystem::remove(f);
  if (filesystem::exists(f1)) filesystem::remove(f1);

  (std::ofstream(f.c_str()) << s).close();
  (std::ofstream(f1.c_str()) << p).close();

  optional<UniquePtr<EVP_PKEY>> r = SslMsgMgr::load_key_from_file(f,true /*is_secret*/);
  auto r1 = SslMsgMgr::load_key_from_file(f1,false /*is_secret*/);
  auto r2 = SslMsgMgr::load_key_from_file("/tmp/no-such-file.txt");


  BOOST_REQUIRE(r);
  BOOST_REQUIRE(r1.value());
  BOOST_REQUIRE(not r2);
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r.value().get()),"ED25519");
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r1.value().get()),"ED25519");
  }

BOOST_AUTO_TEST_CASE(test_sign){
  string s = "-----BEGIN PRIVATE KEY-----\n"
    "MC4CAQAwBQYDK2VwBCIEIDdCupRSMP7AqAT50TZwDzlYIfrgDpLL+km+0usqrWpB\n"
    "-----END PRIVATE KEY-----\n";
  auto r = SslMsgMgr::load_key_from_pem(s,true /*is_secret*/);
  UniquePtr<EVP_PKEY> sk = std::move(r.value());

  string p = "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VwAyEAag4tsjNQHNpXrWkEfTEygtwjjXZKXZJJ2/09srM0RDs=\n"
    "-----END PUBLIC KEY-----\n";
  r = SslMsgMgr::load_key_from_pem(p,false /*is_secret*/);
  UniquePtr<EVP_PKEY> pk = std::move(r.value());

  // 🦜 : sign with the secret key
  string msg = "abc";
  string sig = SslMsgMgr::do_sign(sk.get(),msg);
  BOOST_CHECK_EQUAL(sig.size(),64);

  // 🦜 : verify with the public key
  BOOST_CHECK(SslMsgMgr::do_verify(pk.get(),msg,sig));

  // 🦜 : verify with the wrong public key
  UniquePtr<EVP_PKEY> sk2(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"));
  BOOST_CHECK(not SslMsgMgr::do_verify(sk2.get(),msg,sig));

  // 🦜 : you can't sign with the public key, the following will cause "memory access violation"
  // SslMsgMgr::do_sign(pk.get(),msg)
  }

/**
 * @brief Prepare the keys for testing
 *
 * @return (ca_sk,ca_sk_pem,node_sks, node_certs)
 */
tuple<UniquePtr<EVP_PKEY>,
      string,
      unordered_map<string,UniquePtr<EVP_PKEY>>,
      unordered_map<string,string>
      > prepare_keys(vector<string> nodes = {"localhost:7777",
                                             "localhost:7778",
                                             "localhost:7779"}
                     ){
  /*
    🐢 : To initialize a valid SslMsgMgr, we need to prepare something. Namely:

    1. a key pair for CA (ca_sk,ca_pk)

    2. a key pair for each node [(n1_sk, n1_pk), (n2_sk, n2_pk),...]

    3. the signature of the node's public key by the CA, this acts as the
    certificate for the node [(n1_pk, n1_sig), (n2_pk, n2_sig),...]

   */

  // 1. generate the key pair for CA
  UniquePtr<EVP_PKEY> ca_sk(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"));
  string ca_pk_pem = SslMsgMgr::dump_key_to_pem(ca_sk.get(),false /* is_secret*/);

  // 2. generate the key pair for each node and sign
  unordered_map<string,UniquePtr<EVP_PKEY>> node_sks;
  unordered_map<string,string> node_certs;
  for (string s : nodes){
    UniquePtr<EVP_PKEY> sk(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"));
    string pk_pem = SslMsgMgr::dump_key_to_pem(sk.get(), false /*is_secret*/);
    string sig = SslMsgMgr::do_sign(ca_sk.get(),pk_pem);       // sign the pk pem

    node_certs[s] = sig;        // save the cert
    node_sks[s] = std::move(sk); // save the secret key
  }

  return make_tuple(std::move(ca_sk), ca_pk_pem,std::move(node_sks), node_certs);
}

BOOST_AUTO_TEST_CASE(test_ctor_ok){
  auto [ca_sk, ca_pk_pem, node_sks, node_certs] = prepare_keys();
  // Now we can initialize the SslMsgMgr
  /*

    explicit SslMsgMgr(string my_sk_pem,
                string my_addr_port_str,
                string mmy_cert = "",
                string ca_pk_pem = "")

   */

  string my_sk_pem = SslMsgMgr::dump_key_to_pem(node_sks.at("localhost:7777").get(), true /*is_secret*/);
  SslMsgMgr m(my_sk_pem, "localhost:7777", node_certs["localhost:7777"], ca_pk_pem);
  BOOST_REQUIRE(m.we_check_cert());
}

BOOST_AUTO_TEST_CASE(test_ssl_msgMgr_basic){
  vector<string> nodes = {"localhost:7777", "localhost:7778", "localhost:7779"};
  auto [ca_sk, ca_pk_pem, node_sks, node_certs] = prepare_keys(nodes);

  // Initialize N0
  string my_sk_pem = SslMsgMgr::dump_key_to_pem(node_sks.at(nodes[0]).get(), true /*is_secret*/);
  SslMsgMgr m0(my_sk_pem, nodes[0], node_certs[nodes[0]], ca_pk_pem);

  // Init N1
  my_sk_pem = SslMsgMgr::dump_key_to_pem(node_sks.at(nodes[1]).get(), true /*is_secret*/);
  SslMsgMgr m1(my_sk_pem, nodes[1], node_certs[nodes[1]], ca_pk_pem);


  // --------------------------------------------------
  // N0 signs and N1 verifies
  string msg = "abc";
  string data = m0.prepare_msg(std::move(msg));
  auto r = m1.tear_msg_open(data);
  BOOST_REQUIRE(r);

  auto [from, msg1] = r.value();
  BOOST_CHECK_EQUAL(from,m0.my_endpoint());
  BOOST_CHECK_EQUAL(msg1,"abc");

  // N0 should also be able th unpack it
  r = m0.tear_msg_open(data);
  BOOST_REQUIRE(r);

  // 🦜 : If you fiddle with the data, the verification will fail
  data = data + "123";
  r = m1.tear_msg_open(data);
  BOOST_CHECK(not r);
}

BOOST_AUTO_TEST_SUITE_END(); //test_SslMsgMgr

BOOST_AUTO_TEST_SUITE_END();

// }}}


