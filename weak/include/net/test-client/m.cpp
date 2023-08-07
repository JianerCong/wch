#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include "../pure-weakHttpClient.hpp"

using namespace pure::weakHttpClient;

BOOST_AUTO_TEST_CASE(test_impl_get) {
  auto r = get_post_impl<false>("localhost","/","8080");
  string s = r.body();
  BOOST_CHECK_EQUAL(s,"aaa");
  BOOST_CHECK_EQUAL(r.result_int(),200);

  r = get_post_impl<false>("localhost","/json","8080"); s = r.body();
  BOOST_CHECK_EQUAL(s,"{\"one\": 1, \"two\": 2}");
  BOOST_CHECK_EQUAL(r.result_int(),200);


  r = get_post_impl<false>("localhost","/nope","8080"); s = r.body();
  BOOST_CHECK_EQUAL(r.result_int(),404);

} // BOOST_AUTO_TEST_CASE(test_no_1)

BOOST_AUTO_TEST_CASE(test_get) {
  auto r = get("localhost","/",8080);
  BOOST_REQUIRE(r);
  string s = r.value();
  BOOST_CHECK_EQUAL(s,"aaa");

  r = get("localhost","/json",8080);
  BOOST_REQUIRE(r);
  s = r.value();
  BOOST_CHECK_EQUAL(s,"{\"one\": 1, \"two\": 2}");

  r = get("localhost","/nope",8080);
  BOOST_REQUIRE(not r);
} // BOOST_AUTO_TEST_CASE(test_no_1)

BOOST_AUTO_TEST_CASE(test_impl_post) {

  auto r = get_post_impl<true>("localhost","/json","8080","{\"x\": 1}");
  string s = r.body();
  BOOST_CHECK_EQUAL(s,"{\"msg\": \"Okay\"}");
  BOOST_CHECK_EQUAL(r.result_int(),200);


} // BOOST_AUTO_TEST_CASE(test_no_1)


BOOST_AUTO_TEST_CASE(test_post) {

  auto r = post("localhost","/json",8080,"{\"x\": 1}");
  BOOST_REQUIRE(r);
  string s = r.value();
  BOOST_CHECK_EQUAL(s,"{\"msg\": \"Okay\"}");
} // BOOST_AUTO_TEST_CASE(test_no_1)
