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

BOOST_AUTO_TEST_SUITE(test_SslMsgMgr);

BOOST_AUTO_TEST_CASE(test_load_keys_from_string){
  string s = "-----BEGIN PRIVATE KEY-----\n"
    "MC4CAQAwBQYDK2VwBCIEIDdCupRSMP7AqAT50TZwDzlYIfrgDpLL+km+0usqrWpB\n"
    "-----END PRIVATE KEY-----\n";

  string p = "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VwAyEAag4tsjNQHNpXrWkEfTEygtwjjXZKXZJJ2/09srM0RDs=\n"
    "-----END PUBLIC KEY-----\n";

  auto r = SslMsgMgr::load_key_from_string(s,true /*is_secret*/);
  auto r1 = SslMsgMgr::load_key_from_string(p,false /*is_secret*/);
  auto r2 = SslMsgMgr::load_key_from_string("123",false /*is_secret*/);

  BOOST_REQUIRE(r.value());
  BOOST_REQUIRE(r1.value());
  BOOST_REQUIRE(not r2);
  // UniquePtr<EVP_PKEY> sk = r.value();
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r.value().get()),"ED25519");
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r1.value().get()),"ED25519");
}

BOOST_AUTO_TEST_CASE(test_load_keys_from_file){
  string s = "-----BEGIN PRIVATE KEY-----\n"
    "MC4CAQAwBQYDK2VwBCIEIDdCupRSMP7AqAT50TZwDzlYIfrgDpLL+km+0usqrWpB\n"
    "-----END PRIVATE KEY-----\n";

  string p = "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VwAyEAag4tsjNQHNpXrWkEfTEygtwjjXZKXZJJ2/09srM0RDs=\n"
    "-----END PUBLIC KEY-----\n";

  // write to tmpdir
  path f = filesystem::temp_directory_path() / "secret.pem";
  path f1 = filesystem::temp_directory_path() / "public.pem";

  (std::ofstream(f.c_str()) << s).close();
  (std::ofstream(f1.c_str()) << p).close();

  auto r = SslMsgMgr::load_key_from_file(f.string(),true /*is_secret*/);
  auto r1 = SslMsgMgr::load_key_from_file(f1.string(),false /*is_secret*/);
  auto r2 = SslMsgMgr::load_key_from_file("/tmp/no-such-file.txt");

  BOOST_REQUIRE(r.value());
  BOOST_REQUIRE(r1.value());
  BOOST_REQUIRE(not r2);
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r.value().get()),"ED25519");
  BOOST_CHECK_EQUAL(EVP_PKEY_get0_type_name(r1.value().get()),"ED25519");
  }

BOOST_AUTO_TEST_CASE(test_key_to_raw){
  UniquePtr<EVP_PKEY> p(EVP_PKEY_Q_keygen(NULL, NULL, "ED25519"));
  auto raw = SslMsgMgr::key_to_raw(p.get(), true /*is_secret*/);
  BOOST_REQUIRE(raw);
  BOOST_CHECK_EQUAL(raw.value().size(),32);

  UniquePtr<EVP_PKEY> p2 = SslMsgMgr::raw_to_key(raw.value(), true /*is_secret*/);
  auto raw2 = SslMsgMgr::key_to_raw(p2.get(), true /*is_secret*/);
  BOOST_REQUIRE(raw2);
  BOOST_CHECK_EQUAL(raw2.value().size(),32);

  BOOST_CHECK_EQUAL(raw2.value(),raw.value());

  /*
    🦜 : You can also get the raw public key from the secret key, but not the other way around.
   */
  auto raw3 = SslMsgMgr::key_to_raw(p.get(), false /*is_secret*/);
  BOOST_REQUIRE(raw3);
  BOOST_CHECK_EQUAL(raw3.value().size(),32);
  BOOST_CHECK_NE(raw3.value(),raw.value());

  // 🦜 : Try to get the secret key from the raw public key
  UniquePtr<EVP_PKEY> p3 = SslMsgMgr::raw_to_key(raw3.value(), true /*is_secret*/);
  // 🦜 : You "can" get something, but that's not the secret key you want.
  BOOST_CHECK_NE(SslMsgMgr::key_to_raw(p3.get(), true /*is_secret*/).value(),raw.value());
  }

/**
 * @brief Test the public key to raw
 *
 * 🦜 : Here we do a :
 *   PEM -> EVP_PKEY -> raw -> EVP_PKEY -> raw
 */
BOOST_AUTO_TEST_CASE(test_public_key_to_raw){
  string p = "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VwAyEAag4tsjNQHNpXrWkEfTEygtwjjXZKXZJJ2/09srM0RDs=\n"
    "-----END PUBLIC KEY-----\n";
  auto r1 = SslMsgMgr::load_key_from_string(p,false /*is_secret*/);

  auto raw1 = SslMsgMgr::key_to_raw(r1.value().get(), false /*is_secret*/);
  BOOST_REQUIRE(raw1);
  BOOST_CHECK_EQUAL(raw1.value().size(),32);
  BOOST_TEST_MESSAGE( EVP_PKEY_get0_description(r1.value().get()));
  EVP_PKEY_print_public_fp(stdout, r1.value().get(), 2, NULL); // print public key
  // EVP_PKEY_print_private_fp(stdout, r1.value().get(), 2, NULL); // print nothing, because it's public

  UniquePtr<EVP_PKEY> p2 = SslMsgMgr::raw_to_key(raw1.value(), false /*is_secret*/);
  BOOST_REQUIRE(p2.get());                                           // parsed successfully
  auto raw2 = SslMsgMgr::key_to_raw(p2.get(), false /*is_secret*/);
  BOOST_REQUIRE(raw2);
  BOOST_CHECK_EQUAL(raw2.value().size(),32);

  BOOST_CHECK_EQUAL(raw2.value(),raw1.value());
  }

BOOST_AUTO_TEST_CASE(test_secret_key_to_raw){
  string s = "-----BEGIN PRIVATE KEY-----\n"
    "MC4CAQAwBQYDK2VwBCIEIDdCupRSMP7AqAT50TZwDzlYIfrgDpLL+km+0usqrWpB\n"
    "-----END PRIVATE KEY-----\n";

  auto r = SslMsgMgr::load_key_from_string(s,true /*is_secret*/);
  BOOST_TEST_MESSAGE( EVP_PKEY_get0_description(r.value().get()));
  EVP_PKEY_print_private_fp(stdout, r.value().get(), 2, NULL); // print both pub and priv

  auto raw = SslMsgMgr::key_to_raw(r.value().get(), true /*is_secret*/);
  BOOST_REQUIRE(raw);

  UniquePtr<EVP_PKEY> p2 = SslMsgMgr::raw_to_key(raw.value(), true /*is_secret*/);
  BOOST_REQUIRE(p2.get());                                           // parsed successfully
  auto raw2 = SslMsgMgr::key_to_raw(p2.get(), true /*is_secret*/);
  BOOST_REQUIRE(raw2);
  BOOST_CHECK_EQUAL(raw2.value().size(),32);

  BOOST_CHECK_EQUAL(raw2.value(),raw.value());
  }

BOOST_AUTO_TEST_SUITE_END(); //test_SslMsgMgr

BOOST_AUTO_TEST_SUITE_END();

// }}}


