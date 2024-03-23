#include "h.hpp"

#include "div2Executor.hpp"
using namespace weak;

// 🦜 : Here we can test the Div2Executor, an extension of the EvmExecutor.

BOOST_AUTO_TEST_CASE(test_exec0_hi, MY_TEST_THIS){
  auto [return_val, result] = ExoticTxExecutorBase::exec0("echo hi");
  // BOOST_CHECK_EQUAL(result, "hi");

  // auto [return_val, result] = ExoticTxExecutorBase::exec0("powershell -NoProfile -NonInteractive -NoLogo -Command 'echo hi'");
  // auto [return_val, result] = ExoticTxExecutorBase::exec0("powershell /?");
  // auto [return_val, result] = ExoticTxExecutorBase::exec0(R"--(powershell -NoProfile -NonInteractive -NoLogo -Command "& {echo hi}")--");
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : result = >>" << result  << "<<, hex = " << evmc::hex(bytesFromString(result)) ;
  // 🦜 : see the hex
  BOOST_CHECK_EQUAL(result, "hi\x0d");
  // \x0d is the carriage return
  // should contains hi
  // BOOST_CHECK(result.find(string("hi")) != string::npos);

  BOOST_CHECK_EQUAL(1,1);
}


BOOST_AUTO_TEST_CASE(test_exec_py){
  auto [return_val, result] = PyTxExecutor::exec_py("print('hi')");
  BOOST_CHECK_EQUAL(result, "hi");
}

// --------------------------------------------------
// the pyvm test

BOOST_AUTO_TEST_CASE(test_readAllText){
  string s = weak::readAllText("no-such-file.txt");
  BOOST_CHECK_EQUAL(s, "");

  s = weak::readAllText("example-contracts/ok-basic.py");
  // BOOST_LOG_TRIVIAL(debug) << "Read: s: " << S_CYAN << s << S_NOR;
  BOOST_CHECK(s.size() > 0);
}

BOOST_AUTO_TEST_CASE(test_verifyPyContract){
  //   optional<string> PyTxExecutor::verifyPyContract(const string  py_code);

  // 1. the check is passed
  string s = weak::readAllText("example-contracts/ok-basic.py");
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

tuple<StateChange,Tx> deployIt(string_view py_contract){
  bytes data = weak::bytesFromString(py_contract);
  Tx t = Tx(makeAddress(1),makeAddress(0),data, 123 /*nonce*/);
  optional<tuple<vector<StateChange>,bytes>> r = PyTxExecutor::executePyTx(nullptr, t);
  BOOST_REQUIRE(r);
  // StateChange s = r.value().first[0];
  // bytes b = r.value().second;
  auto [sc, b] = r.value();
  // StateChange s = sc[0];
  return make_tuple(sc[0], t);
}

BOOST_AUTO_TEST_CASE(test_deployPyContract){
  // 🦜 : let's deploy a py_tx
  string_view py_contract = R"--(
def hi():
    return 123
)--";
  auto [s, t] = deployIt(py_contract);

  // s.k should serialize to an address
  BOOST_CHECK_EQUAL(s.k, weak::addressToString(Tx::getContractDeployAddress(t)));
  // s.v is the created Acn
  Acn a;
  BOOST_REQUIRE(a.fromString(s.v));
  // 1. code is uploaded to the Acn
  BOOST_CHECK_EQUAL(evmc::hex(a.code), evmc::hex(t.data));
  // 2. a.disk_storage = [<abi>, <storage>]
  BOOST_CHECK_EQUAL(a.disk_storage.size(), 2);
  BOOST_CHECK_EQUAL(a.disk_storage[0], R"--({"hi": []})--");
  BOOST_CHECK_EQUAL(a.disk_storage[1], R"--({})--");
}

BOOST_AUTO_TEST_CASE(test_deployPyContract_init_storage){
  // 🦜 : let's deploy a py_tx that init the _storage

  string_view py_contract = R"--(
from typing import Any
def init(_storage: dict[str, Any]):
    _storage['x'] = 0
)--";
  auto [s, t] = deployIt(py_contract);

  // s.k should serialize to an address
  BOOST_CHECK_EQUAL(s.k, weak::addressToString(Tx::getContractDeployAddress(t)));
  // s.v is the created Acn
  Acn a;
  BOOST_REQUIRE(a.fromString(s.v));
  // 1. code is uploaded to the Acn
  BOOST_CHECK_EQUAL(evmc::hex(a.code), evmc::hex(t.data));
  // 2. a.disk_storage = [<abi>, <storage>]
  BOOST_CHECK_EQUAL(a.disk_storage.size(), 2);
  // BOOST_CHECK_EQUAL(a.disk_storage[0], R"--({"hi": []})--");
  BOOST_CHECK_EQUAL(a.disk_storage[1], R"--({"x":0})--");
}

BOOST_AUTO_TEST_CASE(test_invoke_no_storage){
  // 1. prepare the Acn
  string_view py_contract = R"--(
def hi() -> int:
    return 123
)--";

  Acn a, a1;
  a.code = weak::bytesFromString(py_contract);
  a.disk_storage = {R"--({"hi": []})--", R"--({})--"};

  // 2. prepare the invoke object
  json::object invoke = json::parse(R"--({"method": "hi"})--").as_object();
  // 3. prepare the tx
  Tx t = Tx(makeAddress(1), makeAddress(0), weak::bytesFromString(string(py_contract)), 123 /*nonce*/);

  // 4. invoke
  auto [oa, r] = PyTxExecutor::invokePyContract(a, invoke, t);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Py contract exec result: " S_CYAN << weak::toString(r) << S_NOR;
  BOOST_CHECK_EQUAL(json::parse(weak::toString(r)).at("result").as_int64(), 123);
  BOOST_REQUIRE(not oa);        // storage not changed

}

BOOST_AUTO_TEST_CASE(test_invoke_with_storage){
  // 1. prepare the Acn
  string_view py_contract = R"--(
from typing import Any
def hi(_storage: dict[str:Any]):
    _storage['x'] = 1
)--";

  Acn a, a1;
  a.code = weak::bytesFromString(py_contract);
  a.disk_storage = {R"--({"hi": ["_storage"]})--", R"--({})--"};

  // 2. prepare the invoke object
  json::object invoke = json::parse(R"--({"method": "hi"})--").as_object();
  // 3. prepare the tx
  Tx t = Tx(makeAddress(1), makeAddress(0), weak::bytesFromString(string(py_contract)), 123 /*nonce*/);

  // 4. invoke
  auto [oa, r] = PyTxExecutor::invokePyContract(a, invoke, t);
  BOOST_LOG_TRIVIAL(debug) <<  "🦜 : Py contract exec result: " S_CYAN << weak::toString(r) << S_NOR;
  BOOST_REQUIRE(oa);        // storage not changed
  // The modified acn has its storage updated (the rest remains unchanged)


  a1 = oa.value();
  BOOST_CHECK_EQUAL(a1.disk_storage[1], R"--({"x":1})--");
  BOOST_CHECK_EQUAL(toString(a1.code), toString(a.code));
  BOOST_CHECK_EQUAL(a1.disk_storage[0], a.disk_storage[0]);
}

BOOST_AUTO_TEST_CASE(test_executePyInvoke_bad){
  // bad invoke object
  // static optional<tuple<vector<StateChange>,bytes>> executePyTx(IAcnGettable * const w, const Tx & t);

  // 0. not json
  Tx t = Tx(makeAddress(1), makeAddress(2), weak::bytesFromString("bad"), 123 /*nonce*/);
  auto r = PyTxExecutor::executePyTx(nullptr, t);
  BOOST_REQUIRE(not r);

  // 1. `method` not found
  t.data = weak::bytesFromString(R"--({})--");
  r = PyTxExecutor::executePyTx(nullptr, t);
  BOOST_REQUIRE(not r);

  // 2. method is not string
  t.data = weak::bytesFromString(R"--({"method": 123})--");
  r = PyTxExecutor::executePyTx(nullptr, t);
  BOOST_REQUIRE(not r);
}

#include "mock.hpp"
BOOST_AUTO_TEST_CASE(test_pyInvoke_acn_notFound){
  // 1. prepare the wrld
  mockedAcnPrv::A ah;
  Tx t = Tx(makeAddress(1), makeAddress(2), weak::bytesFromString(R"--({"method": "hi"})--"), 123 /*nonce*/);
  auto r = PyTxExecutor::executePyTx(dynamic_cast<IAcnGettable*>(&ah), t);
  BOOST_REQUIRE( not r);
}

BOOST_AUTO_TEST_CASE(test_pyInvoke_acn_ok_no_storage){
  // 1. get the in-RAM (state-only) world
  mockedAcnPrv::E ah;

  // 2. prepare the Acn
  string_view py_contract = R"--(
def hi() -> int:
    return 123
)--";
  Acn a, a1;
  a.code = weak::bytesFromString(py_contract);
  a.disk_storage = {R"--({"hi": []})--", R"--({})--"};

  // 3. put the acn in the world
  ah.accounts[addressToString(makeAddress(2))] = a;

  // 4. call
  Tx t = Tx(makeAddress(1), makeAddress(2), weak::bytesFromString(R"--({"method": "hi"})--"), 123 /*nonce*/);
  auto r = PyTxExecutor::executePyTx(dynamic_cast<IAcnGettable*>(&ah), t);
  // invoked, but no stateChange
  BOOST_REQUIRE(r);
  auto [v, res] = r.value();
  BOOST_CHECK_EQUAL(json::parse(weak::toString(res)).at("result").as_int64(), 123);
  BOOST_CHECK_EQUAL(v.size(), 0);
}


BOOST_AUTO_TEST_CASE(test_pyInvoke_acn_ok_with_storage){
  // 1. get the in-RAM (state-only) world
  mockedAcnPrv::E ah;

  // 2. prepare the Acn
  string_view py_contract = R"--(
from typing import Any
def hi(_storage: dict[str, Any], y : int):
    _storage['x'] = _storage.get('x', 0) + y
)--";
  Acn a, a1;
  a.code = weak::bytesFromString(py_contract);
  a.disk_storage = {R"--({"hi": ["_storage", "y"]})--", R"--({"x": 1})--"};

  // 3. put the acn in the world
  ah.accounts[addressToString(makeAddress(2))] = a;

  // 4. call
  Tx t = Tx(makeAddress(1), makeAddress(2), weak::bytesFromString(R"--({"method": "hi", "args" : {"y":1}})--"), 123 /*nonce*/);
  auto r = PyTxExecutor::executePyTx(dynamic_cast<IAcnGettable*>(&ah), t);
  // invoked, and there're stateChange
  BOOST_REQUIRE(r);
  auto [v, res] = r.value();
  BOOST_CHECK_EQUAL(v.size(), 1);

  // 4.1 🦜 : If the storage is unchanged, then the stateChange should not be there.
  t = Tx(makeAddress(1), makeAddress(2), weak::bytesFromString(R"--({"method": "hi", "args" : {"y":0}})--"), 123 /*nonce*/);
  r = PyTxExecutor::executePyTx(dynamic_cast<IAcnGettable*>(&ah), t);
  BOOST_REQUIRE(r);
  std::tie(v, res) = r.value();
  BOOST_CHECK_EQUAL(v.size(), 0);
}

// 🦜 : Finally, let's deploy + invoke
