#include "h.hpp"
#include "cnsss/pure-forCnsss.hpp"
#include "pure-mock.hpp"

using namespace pure;


BOOST_AUTO_TEST_SUITE(test_mockedCnsss, MY_TEST_THIS);
BOOST_AUTO_TEST_CASE(test_mockedCnsss_primary){
  mockedCnsss::A ch;
  ICnsssPrimaryBased * c = dynamic_cast<ICnsssPrimaryBased*>(&ch);
  BOOST_CHECK(c->is_primary());
  auto r = c->handle_execute("some-user","some-command");
  BOOST_CHECK(r);
  BOOST_CHECK_EQUAL(r.value(),"Primary result (ignored)");
}

BOOST_AUTO_TEST_CASE(test_mockedCnsss_sub){
  mockedCnsss::A ch{false};     // sub node
  ICnsssPrimaryBased * c = dynamic_cast<ICnsssPrimaryBased*>(&ch);
  BOOST_CHECK(not c->is_primary());
  auto r = c->handle_execute("some-user","some-command");
  BOOST_CHECK(r);
  BOOST_CHECK_EQUAL(r.value(),"Sub result (ignored)");
}

BOOST_AUTO_TEST_SUITE_END();
