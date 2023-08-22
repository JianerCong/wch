/**
 * @file test-pure-net.cpp
 * @brief Here we tests the async HTTP server.
 *
 */

#include "h.hpp"
#include "net/pure-weakAsyncHttpServer.hpp"
#include "net/pure-weakHttpClient.hpp"

using namespace pure;
BOOST_AUTO_TEST_SUITE(test_parse_query_param);
BOOST_AUTO_TEST_CASE(test_one){
  BOOST_CHECK(1==1);
}

auto parse_query_param = WeakAsyncHttpServer::parse_query_param;
// grab the function

template<typename T>
void veq(vector<T> v, vector<T> v2){
  for (int i=0;i<v.size();i++) \
    BOOST_CHECK_EQUAL(v[i],v2[i]);
}

BOOST_AUTO_TEST_CASE(test_boost_split){
  string s = "aa=A&bb=B&cc=C";
  vector<string> v;
  split(v,s,is_any_of("=&"));
  veq(v,{"aa","A","bb","B","cc","C"});
}

template<typename K, typename V>
void meq(unordered_map<K,V> m1, unordered_map<K,V> m2){
  BOOST_REQUIRE_EQUAL(m1.size(),m2.size());
  for (const auto &[k, v]: m1){
    BOOST_TEST_MESSAGE(format("(k,v) = (%d,%s)") % k % v);
    BOOST_REQUIRE(m2.contains(k));
    BOOST_CHECK_EQUAL(m2.at(k),v);
  }
}

BOOST_AUTO_TEST_CASE(test_parse_query_param_ok){
  string s = "/aaa?a=A&b=B";

  auto r = parse_query_param(s);
  BOOST_CHECK(r);
  unordered_map<string,string> m = r.value();

  meq(m,{{"a","A",}, {"b","B"}});
}

void test_parse_modified_target(string s,string_view after_s, bool bad=true){
  if (bad)
    BOOST_CHECK(not parse_query_param(s));
  else
    BOOST_CHECK(parse_query_param(s));

  BOOST_CHECK_EQUAL(s,after_s);
}

BOOST_AUTO_TEST_CASE(test_parse_query_param_unmatched){
  // no param
  test_parse_modified_target("","");
  test_parse_modified_target("aaa","aaa");
  test_parse_modified_target("aaa?","aaa");
  test_parse_modified_target("aaa?a","aaa");
  test_parse_modified_target("aaa?a=A&b","aaa");

  test_parse_modified_target("aaa?a=A&b=B","aaa",false /*passed*/);
}

BOOST_AUTO_TEST_SUITE_END();

void sleep_for(int i=1){
  std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
}



BOOST_AUTO_TEST_SUITE(test_get_handler);
BOOST_AUTO_TEST_CASE(test_register_a_handler){
  WeakAsyncHttpServer sr{7777};
  sr.listenToGet("/aaa",
                 [](string /*from*/
                    ,uint16_t /*port*/
                    ,optional<unordered_map<string,string>> /*qparam*/
                    ) -> tuple<bool,string>{
                   return make_tuple(true,"aaa");
                 });
  sleep_for(1);

  BOOST_CHECK_EQUAL(sr.getLisnMap.size(),1); // the handler is added
  } // server closed here


BOOST_AUTO_TEST_CASE(test_call_registered_handler){
  WeakAsyncHttpServer sr{7778};
  sr.listenToGet("/aaa",
                 [](string /*from*/
                    ,uint16_t /*port*/
                    ,optional<unordered_map<string,string>> /*qparam*/
                    ) -> tuple<bool,string>{
                   return make_tuple(true,"aaa");
                 });
  sleep_for(1);

  auto r = weakHttpClient::get("localhost","/aaa",7778);
  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(r.value(),"aaa"); // check the returned value
} // server closed here

BOOST_AUTO_TEST_CASE(test_call_registered_handler_with_query_param){
  WeakAsyncHttpServer sr{7779};
  sr.listenToGet("/aaa",
                 [](string /*from*/
                    ,uint16_t /*port*/
                    ,optional<unordered_map<string,string>> q
                    ) -> tuple<bool,string>{
                   BOOST_LOG_TRIVIAL(debug) << format("handler called");

                   if (not q)
                     return make_tuple(true,"aaa received no query param");

                   string s;
                   unordered_map<string,string> m = q.value();

                   for (const auto &[k,v] : m)
                     s += (format("[%s:%s]") % k % v).str();
                   return make_tuple(true,"aaa received: " + s);
                 });

  sleep_for(1);

  auto r = weakHttpClient::get("localhost","/aaa?a=123&b=B",7779);
  BOOST_REQUIRE(r);
  string s = r.value();

  BOOST_TEST_MESSAGE((format("recieved :>>%s<<") % s).str());

  // 🦜: The order is unsure.
  BOOST_CHECK_NE(s.find("[a:123]"),string::npos); // contains
  BOOST_CHECK_NE(s.find("[b:B]"),string::npos); // contains
  /*
    🦜: We can change to following in ++23
    BOOST_CHECK(s.contains("[a:123]"));
   */

} // server closed here

BOOST_AUTO_TEST_CASE(test_call_dynamically_registered_handler_with_query_param){

  const uint16_t PORT = 7780;

  // 0. --------------------------------------------------
  // Start server
  WeakAsyncHttpServer sr{PORT};
  auto f = [](string /*from*/ ,uint16_t /*port*/ ,optional<unordered_map<string,string>> q)
    -> tuple<bool,string>{
    BOOST_LOG_TRIVIAL(debug) << format("handler called");

    if (not q)
      return make_tuple(true,"aaa received no query param");

    string s;
    unordered_map<string,string> m = q.value();

    for (const auto &[k,v] : m)
      s += (format("[%s:%s]") % k % v).str();
    return make_tuple(true,"aaa received: " + s);
  };
  // BOOST_TEST_MESSAGE("adding handler");
  sr.listenToGet("/aaa",f);
  // BOOST_TEST_MESSAGE("handler added");
  sleep_for(1);

  // 1. --------------------------------------------------
  // Send request
  auto r = weakHttpClient::get("localhost","/aaa?a=123&b=B",PORT);
  BOOST_REQUIRE(r);
  string s = r.value();
  BOOST_TEST_MESSAGE((format("recieved :>>%s<<") % s).str());

  // 🦜: The order is unsure.
  BOOST_CHECK_NE(s.find("[a:123]"),string::npos); // contains
  BOOST_CHECK_NE(s.find("[b:B]"),string::npos); // contains

} // server closed here


BOOST_AUTO_TEST_SUITE_END();
