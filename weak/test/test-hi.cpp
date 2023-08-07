#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#define MY_TEST_THIS *boost::unit_test::label("aaa")

// #include "test-core.cpp"
// #include "test-ethash.cpp"
// #include "test-storageManager.cpp"
// // 🦜 : There's a problem with this file, if I include this, test-forPostExec.cpp won't be able to makeBlk...
// #include "test-executeEvmTx.cpp"
// #include "test-mempool.cpp"
// #include "test-forPostExec.cpp"

BOOST_AUTO_TEST_CASE(test_hi) {
  BOOST_CHECK(1 == 1);
} // BOOST_AUTO_TEST_CASE(test_no_1)

