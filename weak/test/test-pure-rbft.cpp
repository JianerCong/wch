/**
 * @file test-pure-net.cpp
 * @brief Here we tests some helpler functions in the rcp server.
 *
 * 🦜 : We test them here, because they don't need to resort to an external
 * server or client. These are usually some static methods;
 */

#include "h.hpp"
#include "cnsss/pure-rbft.hpp"

BOOST_AUTO_TEST_CASE(test_one){
  BOOST_CHECK_EQUAL(1,1);
  }
