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
  /*
    🦜 : Because the most cases are checked in python, here we can just check a few
   */
}

BOOST_AUTO_TEST_CASE(test_verifyPyContract_bad){
  string s = "bad";
  optional<string> abi = PyTxExecutor::verifyPyContract(s);
  BOOST_CHECK(not abi);
}

void AssertError(json::object o, string s = ""){
  // BOOST_CHECK_EQUAL(o.size(), 1); //<! 🦜 : Feels irrelevent
  if (s.size() > 0){
    BOOST_LOG_TRIVIAL(debug) <<  "📗️ Got error object" S_CYAN << o << S_NOR;
    BOOST_CHECK(o["quit"].as_string().starts_with(s));
  }else{
    BOOST_REQUIRE(o.contains("quit"));
  }
}

BOOST_AUTO_TEST_CASE(test_checkAndPrepareArgs_bad){

  // 1. invoke should have `method`
  {
    json::object i,a,o;             // invoke and abi object
    i["k1"] = "v1";
    o = PyTxExecutor::checkAndPrepareArgs(i, a);
    AssertError(o, "Malformed invoke json: `method` not found in the invoke object");
  }

  // 2. abi should contains the error
  {
    json::object i,a,o;             // invoke and abi object
    i["method"] = "hi";
    o = PyTxExecutor::checkAndPrepareArgs(i, a);
    AssertError(o, "Method not found in abi: ");
  }

  // 3. args should be object
  {
    json::object i,a,o;             // invoke and abi object
    i["method"] = "hi";
    i["args"] = "not-object";
    a["hi"] = json::array();
    o = PyTxExecutor::checkAndPrepareArgs(i, a);
    AssertError(o, "Malformed invoke json: `args` should be an object");
  }

  // 4. required args should be provided
  {
    json::object i,a,o;             // invoke and abi object
    i["method"] = "hi";
    i["args"] = json::object();
    i["args"].as_object()["x"] = 1;

    a["hi"] = json::array();
    a["hi"].as_array().push_back("x");
    a["hi"].as_array().push_back("y");

    o = PyTxExecutor::checkAndPrepareArgs(i, a);
    AssertError(o, "Required argument not found: `y`");
  }

  // 5. no excess args should be provided.
  {
    json::object i,a,o;
    json::value jv = {
      {"method", "hi"},
      {"args", {
          {"x", 1},
          {"y", 2}}}};

    i = jv.as_object();
    jv = {
      {"hi", json::array({"x"})}};
    a = jv.as_object();

    o = PyTxExecutor::checkAndPrepareArgs(i, a);
    AssertError(o, R"--(Excess arguments provided: {"y":2})--");
  }
} // test_checkAndPrepareArgs_bad

BOOST_AUTO_TEST_CASE(test_checkAndPrepareArgs_ok){
  json::object i,a,o;
  json::value jv = {
    {"method", "hi"},
    {"args", {
        {"x", 1},
        {"y", 2}}}};

  i = jv.as_object();
  jv = {
    {"hi", json::array({"x", "y"})}};
  a = jv.as_object();

  o = PyTxExecutor::checkAndPrepareArgs(i, a);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Prepared args should be just the args: " << o;
  BOOST_CHECK_EQUAL(json::serialize(o), json::serialize(i["args"]));
}
