#include "h.hpp"

#include "storageManager.hpp"
#include "mock.hpp"
#include <boost/log/trivial.hpp> // for boost_log_trivial, trace, debug,..,fatal
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <thread>

using namespace weak;


template<typename T>
void veq(vector<T> v, vector<T> v2){
  BOOST_CHECK_EQUAL(v.size(), v2.size());
  for (int i=0;i<v.size();i++) \
    BOOST_CHECK_EQUAL(v[i],v2[i]);
};

BOOST_AUTO_TEST_CASE(acn_fromJson_withDiskStorage){
  Acn a;
  json::object  v;

  v["nonce"] = 123;
  v["code"] = "0011";
  // evmc::from_hex() will place these at the end of hex
  v["storage"] = json::object{};
  v["disk_storage"] = json::array{};

  // 🦜 : push some string
  v["disk_storage"].as_array().push_back(json::string("aaa"));
  v["disk_storage"].as_array().push_back(json::string("bbb"));

  BOOST_REQUIRE(a.fromJson(v));
  veq(a.disk_storage,{"aaa","bbb"});
}

BOOST_AUTO_TEST_CASE(acn_toJson_withDiskStorage){
  Acn a{123,evmc::from_hex("0011").value()};
  a.disk_storage.push_back("aaa");
  a.disk_storage.push_back("bbb");

  json::object v = a.toJson().as_object();

  BOOST_CHECK_EQUAL(v["nonce"].as_uint64(),123);
  BOOST_CHECK_EQUAL(v["code"].as_string(),"0011");
  BOOST_CHECK_EQUAL(v["codehash"].as_string(), hashToString(a.codehash()));
  BOOST_CHECK_EQUAL(v["storage"].as_object().size(),0);
  BOOST_REQUIRE(v["disk_storage"].is_array());

  json::array ar = v["disk_storage"].as_array();
  BOOST_CHECK_EQUAL(ar.size(),2);
  BOOST_CHECK_EQUAL(ar[0].as_string(), "aaa");
  BOOST_CHECK_EQUAL(ar[1].as_string(), "bbb");

}

BOOST_AUTO_TEST_CASE(test_acn_to_pb){
  Acn a{123,evmc::from_hex("0011").value()};
  a.disk_storage.push_back("aaa");
  a.disk_storage.push_back("bbb");

  // 🦜 : Save a kv
  // call bytes32(uint64_t)
  a.storage[bytes32(0x1)] = bytes32{0x10};

  hiPb::Acn a0 = a.toPb();
  BOOST_CHECK_EQUAL(a0.nonce(),123);
  BOOST_CHECK_EQUAL(evmc::hex(bytesFromString(a0.code())),"0011");

  // the storage
  BOOST_CHECK_EQUAL(a0.storage_ks_size(),1);
  BOOST_CHECK_EQUAL(a0.storage_vs_size(),1);
  BOOST_CHECK_EQUAL(evmc::hex(bytesFromString(a0.storage_ks(0))),
                    string(62,'0') + "01");
  BOOST_CHECK_EQUAL(evmc::hex(bytesFromString(a0.storage_vs(0))),
                    string(62,'0') + "10");

  // the disk storage
  BOOST_CHECK_EQUAL(a0.disk_storage_size(),2);
  BOOST_CHECK_EQUAL(a0.disk_storage(0),"aaa");
  BOOST_CHECK_EQUAL(a0.disk_storage(1),"bbb");
}

BOOST_AUTO_TEST_CASE(test_acn_from_pb){
  hiPb::Acn a0;
  a0.set_nonce(123);
  a0.set_code(toString(evmc::from_hex("0011").value()));
  a0.add_disk_storage("aaa");
  a0.add_disk_storage("bbb");

  // 🦜 : Save a kv
  a0.add_storage_ks(toString(bytes32(0x1)));
  a0.add_storage_vs(toString(bytes32(0x10)));

  // 🦜 : Load
  Acn a;
  a.fromPb(a0);
  BOOST_CHECK_EQUAL(a.nonce,123);
  BOOST_CHECK_EQUAL(evmc::hex(a.code), "0011");
  BOOST_CHECK_EQUAL(a.disk_storage.size(),2);
  BOOST_CHECK_EQUAL(a.disk_storage[0],"aaa");
  BOOST_CHECK_EQUAL(a.disk_storage[1],"bbb");

  BOOST_CHECK_EQUAL(a.storage.size(),1);
  // has key
  BOOST_REQUIRE(a.storage.contains(bytes32(0x1)));
  BOOST_CHECK_EQUAL(evmc::hex(a.storage[bytes32(0x1)]),
                    string(62,'0') + "10");
}
