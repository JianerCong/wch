#include "h.hpp"

#include "net/pure-greenHttpClient.hpp"

using namespace pure;
BOOST_AUTO_TEST_CASE(test_combine_addr_port){
  string s = GreenHttpClient::combine_addr_port("abc",123);
  BOOST_CHECK_EQUAL(s, "abc:123");
}

BOOST_AUTO_TEST_SUITE(test_with_example_com);
/*

  🐢  : On the command line, we can do:

  telnet example.com 80

  // connection opened

  GET / HTTP/1.1
  Host: example.org
  [return][return]

  <Got a html response>

  GET / HTTP/1.1
  Host: example.org
  [return][return]

  <Got another html response>

  Which sends two HTTP GET in a single TCP connection. Let's try do the
  same with our GreenHttpClient.
*/

BOOST_AUTO_TEST_CASE(test_get_example_com_once){
  GreenHttpClient c;
  optional<string> r = c.get("example.com","/",80);
  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(c.conns.size(),1);
  BOOST_REQUIRE(c.conns.contains("example.com:80"));
}

BOOST_AUTO_TEST_CASE(test_get_example_com_twice){
  GreenHttpClient c;
  optional<string> r = c.get("example.com","/",80);
  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(c.conns.size(),1);

  r = c.get("example.com","/",80);
  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(c.conns.size(),1);
}
BOOST_AUTO_TEST_SUITE_END();

#include "net/pure-weakHttpServer.hpp"
#include <thread>
#include <chrono>


void sleep_for(int i=1){
  std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
}

BOOST_AUTO_TEST_SUITE(test_with_weak_client, MY_TEST_THIS);
BOOST_AUTO_TEST_CASE(test_get){
  WeakHttpServer s{7777};

  s.listenToGet("/aaa",
                  [](string h, uint16_t p,
                   optional<unordered_map<string,string>> /* the query param */
                     ) -> tuple<bool,string>{
                    return make_tuple(true,"aaa from server");
                  });

  {
    GreenHttpClient c;
    sleep_for(1);
    optional<string> r = c.get("localhost","/aaa",7777);
    BOOST_REQUIRE(r);
    BOOST_CHECK_EQUAL(r.value(),"aaa from server");
  } // client closed, which closes the connection
  sleep_for(1);
}


BOOST_AUTO_TEST_CASE(test_get_twice){
  uint16_t port{7778};
  WeakHttpServer s{port};

  s.listenToGet("/aaa",
                  [](string h, uint16_t p,
                     optional<unordered_map<string,string>> /* the query param */
                     ) -> tuple<bool,string>{
                    return make_tuple(true,"aaa from server");
                  });

  {
    GreenHttpClient c;
    sleep_for(1);
    optional<string> r = c.get("localhost","/aaa",port);
    BOOST_REQUIRE(r);
    BOOST_CHECK_EQUAL(r.value(),"aaa from server");
    BOOST_CHECK_EQUAL(c.conns.size(),1);

    r = c.get("localhost","/aaa",port);
    BOOST_REQUIRE(r);
    BOOST_CHECK_EQUAL(r.value(),"aaa from server");
    BOOST_CHECK_EQUAL(c.conns.size(),1);
  } // client closed, which closes the connection
  sleep_for(1);

}

BOOST_AUTO_TEST_CASE(test_post){
  uint16_t port{7779};
  WeakHttpServer s{port};

  s.listenToPost("/aaa",
                [](string h, uint16_t p,
                   string_view d
                   ) -> tuple<bool,string>{
                  return make_tuple(true,(format("aaa received %s") % d).str());
                });

  {
    GreenHttpClient c;
    sleep_for(1);
    optional<string> r = c.post("localhost","/aaa",port,"bbb");
    BOOST_REQUIRE(r);
    BOOST_CHECK_EQUAL(r.value(),"aaa received bbb");
    BOOST_CHECK_EQUAL(c.conns.size(),1);
  } // client closed, which closes the connection
  sleep_for(1);

}
BOOST_AUTO_TEST_SUITE_END();
