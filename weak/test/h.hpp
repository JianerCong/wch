#pragma once
// #define BOOST_TEST_MAIN
// 🦜 : ^^vv Just change name from `master` to `main`
#define BOOST_TEST_MODULE Main
#include <boost/test/unit_test.hpp>
#define MY_TEST_THIS *boost::unit_test::label("aaa")
