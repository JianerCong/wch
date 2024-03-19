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
  // BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Prepared args should be just the args: " << o;
  BOOST_CHECK_EQUAL(json::serialize(o), json::serialize(i["args"]));
}

tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
}
BOOST_AUTO_TEST_CASE(test_addTxContextToArgsMaybe){
  // static void PyTxExecutor::addTxContextToArgsMaybe(json::array & required_args, json::object & args, const Tx & t);

  json::object a;
  auto [a1,a2,data] = get_example_address_and_data();
  Tx t1 = Tx(a1,a2,data,123/*nonce*/);
  json::array ra = {"_tx_context", "x", "y"};

  PyTxExecutor::addTxContextToArgsMaybe(ra, a, t1);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Tx context now should have been added: " << a;
  // BOOST_CHECK_EQUAL(a["_tx_context"].as_object()["from"].as_string().c_str(),string(19 * 2,'0') + "01");
  BOOST_CHECK_EQUAL(a.at("_tx_context").at("from").as_string(),string(19 * 2,'0') + "01");
  BOOST_CHECK_EQUAL(a.at("_tx_context").at("to").as_string(),string(19 * 2,'0') + "02");
  BOOST_CHECK_EQUAL(a.at("_tx_context").at("timestamp").as_int64(),t1.timestamp);

}

BOOST_AUTO_TEST_CASE(test_addTxContextToArgsMaybe_nope){
  json::object a;
  auto [a1,a2,data] = get_example_address_and_data();
  Tx t1 = Tx(a1,a2,data,123/*nonce*/);
  json::array ra = {"x", "y"};

  PyTxExecutor::addTxContextToArgsMaybe(ra, a, t1);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Tx context now should have been added: " << a;
  BOOST_REQUIRE(not a.contains("_tx_context"));
}

/*
  🦜 : Now, let's try the invokePyMethod
  json::object PyTxExecutor::invokePyMethod(json::object invoke, string_view py_code, json::object abi, const Tx & t, json::object storage);
*/
BOOST_AUTO_TEST_CASE(test_minimal_run){
  string_view py_contract = R"--(
def hi():
    return 123
)--";

  json::object abi = json::parse(R"--(
{"hi": []}
)--").as_object();

  json::object invoke = json::parse(R"--(
{"method": "hi"}
)--").as_object();

  auto [a1,a2,data] = get_example_address_and_data();
  Tx t = Tx(a1,a2,data,123/*nonce*/);
  json::object storage = json::object();

  json::object result = PyTxExecutor::invokePyMethod(invoke, py_contract, abi, t, storage);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Py contract exec result: " << result;
  BOOST_CHECK_EQUAL(result.at("result").as_int64(), 123);
}

BOOST_AUTO_TEST_CASE(test_set_storage){
  string_view py_contract = R"--(
from typing import Any
def hi(_storage: dict[str, Any]):
    _storage['x'] = _storage.get('x', 0) + 1
)--";

  json::object abi = json::parse(R"--(
{"hi": ["_storage"]}
)--").as_object();

  json::object invoke = json::parse(R"--(
{"method": "hi"}
)--").as_object();

  auto [a1,a2,data] = get_example_address_and_data();
  Tx t = Tx(a1,a2,data,123/*nonce*/);

  // --------------------------------------------------
  // 1. invoke
  json::object storage = json::object();
  json::object result = PyTxExecutor::invokePyMethod(invoke, py_contract, abi, t, storage);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Py contract exec result: " << result;
  BOOST_REQUIRE(result.contains("storage"));
  storage = result.at("storage").as_object();
  BOOST_CHECK_EQUAL(storage.at("x").as_int64(), 1);

  // --------------------------------------------------
  // 2. invoke twice
  result = PyTxExecutor::invokePyMethod(invoke, py_contract, abi, t, storage);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Py contract exec result: " << result;
  BOOST_CHECK_EQUAL(result.at("storage").at("x").as_int64(), 2);
}


BOOST_AUTO_TEST_CASE(test_tx_context){
  string_view py_contract = R"--(
from typing import Any
def hi(_tx_context: dict[str, Any]) -> str:
    return _tx_context['from']
)--";

  json::object abi = json::parse(R"--(
{"hi": ["_tx_context"]}
)--").as_object();

  json::object invoke = json::parse(R"--(
{"method": "hi"}
)--").as_object();

  auto [a1,a2,data] = get_example_address_and_data();
  Tx t = Tx(a1,a2,data,123/*nonce*/);

  // --------------------------------------------------
  // 1. invoke
  json::object storage = json::object();
  json::object result = PyTxExecutor::invokePyMethod(invoke, py_contract, abi, t, storage);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Py contract exec result: " S_CYAN << result << S_NOR;
  BOOST_CHECK_EQUAL(result.at("result").as_string(), string(19 * 2,'0') + "01");
}

BOOST_AUTO_TEST_CASE(test_deployPyContract){
  // 🦜 : let's deploy a py_tx
  string_view py_contract = R"--(
def hi():
    return 123
)--";
  bytes data = weak::bytesFromString(py_contract);
  Tx t = Tx(makeAddress(1),makeAddress(0),data, 123 /*nonce*/);
  optional<tuple<vector<StateChange>,bytes>> r = PyTxExecutor::executePyTx(nullptr, t);
  BOOST_REQUIRE(r);
  StateChange s = r.value().first[0];
  bytes b = r.value().second;
  // s.k should serialize to an address
  BOOST_CHECK_EQUAL(s.k,
                    weak::addressToString(Tx::getContractDeployAddress(t)));
  // s.v is the created Acn
  
}
