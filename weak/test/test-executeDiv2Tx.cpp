#include "h.hpp"

#include "div2Executor.hpp"
using namespace weak;

// 🦜 : Here we can test the Div2Executor, an extension of the EvmExecutor.

BOOST_AUTO_TEST_CASE(test_exec0_hi){
  auto [return_val, result] = ExoticTxExecutorBase::exec0("echo hi");
  BOOST_CHECK_EQUAL(result, "hi");
}


BOOST_AUTO_TEST_CASE(test_exec_py){
  auto [return_val, result] = PyTxExecutor::exec_py("print('hi')");
  BOOST_CHECK_EQUAL(result, "hi");
}

// --------------------------------------------------
// the pyvm test

BOOST_AUTO_TEST_CASE(test_readAllText){
  string s = PyTxExecutor::readAllText("no-such-file.txt");
  BOOST_CHECK_EQUAL(s, "");

  s = PyTxExecutor::readAllText("example-contracts/ok-basic.py");
  // BOOST_LOG_TRIVIAL(debug) << "Read: s: " << S_CYAN << s << S_NOR;
  BOOST_CHECK(s.size() > 0);
}

BOOST_AUTO_TEST_CASE(test_verifyPyContract){
  //   optional<string> PyTxExecutor::verifyPyContract(const string  py_code);

  // 1. the check is passed
  string s = PyTxExecutor::readAllText("example-contracts/ok-basic.py");
  optional<string> abi = PyTxExecutor::verifyPyContract(s);
  BOOST_REQUIRE(abi.has_value());
  BOOST_LOG_TRIVIAL(debug) << "parsed abi: " << S_CYAN << abi.value() << S_NOR;

  // 2. check the result
  json::object o = json::parse(abi.value()).as_object();
  /*
    {"hi": [], "plus_one": ["x"], "set": ["key", "value", "_storage"], "get":
    ["key", "_storage"], "init": ["_storage", "_tx_context"]}
  */
  BOOST_REQUIRE(o.contains("hi"));
}
