#include "h.hpp"

#include "core.hpp"
using namespace weak;
#include <boost/format.hpp>
#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
using boost::format;
#include <chrono>
#include <thread>

#include <algorithm>
using std::begin;
using std::end;
#include <iostream>
using std::cout;
BOOST_AUTO_TEST_SUITE(test_suite_core);

tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
}

BOOST_AUTO_TEST_CASE(test_makeTx){
  auto [a1,a2,data] = get_example_address_and_data();

  using namespace std::chrono_literals;
  Tx t = Tx(a1,a2,data,123/*nonce*/);
  std::this_thread::sleep_for(1000ms);
  Tx t2 = Tx(a1,a2,data,234/*nonce*/);
  std::this_thread::sleep_for(1000ms);
  Tx t3 = Tx(a2,a1,data,123/*nonce*/);
  Tx t4 = Tx(a2,a1,data + uint8_t{0xff},123/*nonce*/);

  BOOST_CHECK_EQUAL(t.nonce,123);
  BOOST_CHECK_EQUAL(t.data.size(),2);
  BOOST_CHECK_EQUAL(t.data[0],0xff);
  BOOST_CHECK_EQUAL(t.data[1],0xff);
  BOOST_CHECK_EQUAL(addressToString(t.from),addressToString(a1));
  BOOST_CHECK_EQUAL(addressToString(t.to),addressToString(a2));
  BOOST_CHECK_NE(addressToString(t.from),addressToString(a2));

  // Different nonces give different hashes
  BOOST_CHECK_NE(t.hash,t2.hash);
  // Different address also gives different hashes
  BOOST_CHECK_NE(t3.hash,t.hash);
  // But different data won't matter
  BOOST_CHECK_EQUAL(t3.hash,t4.hash);


  // Same nonces, produce same hash
  BOOST_CHECK_NE(t.timestamp,t2.timestamp);
  BOOST_CHECK(t.timestamp < t2.timestamp);
  BOOST_CHECK_NE(t3.timestamp,t2.timestamp);
  BOOST_CHECK(t2.timestamp < t3.timestamp);
}

BOOST_AUTO_TEST_CASE(tx_toJsonString){
  address a1 = makeAddress(1);
  // bytes data{size_t{2},uint8_t{0xff}}; // !called initializer
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.

  hash256 h;
  std::fill(begin(h.bytes), end(h.bytes), 0xff);
  Tx t;
  t.from=a1;t.to=a1;t.data=data;t.nonce=0;t.timestamp=0;t.hash=h;

  string s = t.toJsonString();
  // cout << s << '\n';
  BOOST_CHECK_EQUAL("{"
                    "\"from\":\"0000000000000000000000000000000000000001\","
                    "\"to\":\"0000000000000000000000000000000000000001\","
                    "\"data\":\"ffff\","
                    "\"nonce\":0,"
                    "\"timestamp\":0,"
                    "\"hash\":\"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\""
                    "}",s);
}

BOOST_AUTO_TEST_CASE(tx_fromJsonString){
  const char* s ="{"
    "\"data\":\"ffff\","
    "\"from\":\"0000000000000000000000000000000000000001\","
    "\"hash\":\"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\","
    "\"nonce\":123,"
    "\"timestamp\":456,"
    "\"to\":\"0000000000000000000000000000000000000001\""
    "}";
  Tx t;
  BOOST_REQUIRE(t.fromJsonString(s));
  BOOST_CHECK_EQUAL(t.nonce, 123);
  BOOST_CHECK_EQUAL(t.timestamp,456);

  BOOST_CHECK_EQUAL(t.data.size(),2);
  BOOST_CHECK_EQUAL(t.data[0],0xff);
  BOOST_CHECK_EQUAL(t.data[1],0xff);

  BOOST_CHECK_EQUAL(addressToString(t.from),
                    addressToString(makeAddress(1)));
  BOOST_CHECK_EQUAL(addressToString(t.to),
                    addressToString(makeAddress(1)));
}

BOOST_AUTO_TEST_CASE(test_makeBlk){
  auto [a1,a2,data] = get_example_address_and_data();
  vector<Tx> txs ={
    Tx(a1,a2,data,123/*nonce*/),
    Tx(a1,a2,data,234/*nonce*/)
  };

  // parent hash
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),0x00);

  // make block
  Blk b = Blk(1,h,txs);
  Blk b2 = Blk(1,b.hash,txs);

  BOOST_CHECK_EQUAL(b.number,1);
  BOOST_CHECK_EQUAL(b.txs.size(),2);
  for (int i=0;i<32;i++)
    BOOST_CHECK_EQUAL(b.hash.bytes[i],b2.parentHash.bytes[i]);

  BOOST_CHECK(!std::equal(std::begin(b.hash.bytes),
                          std::end(b.hash.bytes),
                          std::begin(b2.hash.bytes)
                          ));
}



BOOST_AUTO_TEST_CASE(blk_toJson){
  auto [a1,a2,data] = get_example_address_and_data();

  Tx t1 = Tx(a1,a2,data,123/*nonce*/);
  Tx t2 = Tx(a1,a2,data,234/*nonce*/);
  vector<Tx> txs ={t1,t2};

  // parent hash
  hash256 p;
  std::fill(std::begin(p.bytes),std::end(p.bytes),0x00);

  // make block
  Blk b = Blk(1,p,txs);

  json::value v = b.toJson();
  json::object o = v.as_object();

  BOOST_CHECK_EQUAL(value_to<uint64_t>(v.at("number")),1);
  BOOST_CHECK_EQUAL(value_to<string>(v.at("parentHash")), hashToString(p));
  BOOST_CHECK_EQUAL(value_to<string>(v.at("hash")), hashToString(b.hash));

  BOOST_CHECK_EQUAL(o["txs"].as_array().size(),2);
  BOOST_CHECK_EQUAL(value_to<uint64_t>(o["txs"].as_array()[0].at("nonce")),123);
  BOOST_CHECK_EQUAL(value_to<uint64_t>(o["txs"].as_array()[1].at("nonce")),234);

  // Check the transactions
  for (const int i : {0,1}){
    BOOST_CHECK(o["txs"].as_array()[i].is_object());
    BOOST_CHECK_EQUAL(value_to<string>(o["txs"].as_array()[i].at("data")),"ffff");
    BOOST_CHECK_EQUAL(value_to<string>(o["txs"].as_array()[i].at("from")),addressToString(a1));
    BOOST_CHECK_EQUAL(value_to<string>(o["txs"].as_array()[i].at("to")),addressToString(a2));
    BOOST_CHECK_EQUAL(value_to<string>(o["txs"].as_array()[i].at("hash")),hashToString(txs[i].hash));
    BOOST_CHECK_EQUAL(value_to<uint64_t>(o["txs"].as_array()[i].at("timestamp")),txs[i].timestamp);

    // Deprecated Jsoncpp --------------------------------------------------
    // BOOST_CHECK_EQUAL(v["txs"][i]["data"].as_string(),"ffff");
    // BOOST_CHECK_EQUAL(v["txs"][i]["from"].as_string(),
    //                   addressToString(a1));
    // BOOST_CHECK_EQUAL(v["txs"][i]["to"].as_string(),
    //                   addressToString(a2));
    // BOOST_CHECK_EQUAL(v["txs"][i]["hash"].as_string(),
    //                   hashToString(txs[i].hash));
    // BOOST_CHECK_EQUAL(v["txs"][i]["timestamp"].as_uint64(),
                      // txs[i].timestamp);
  }
}

BOOST_AUTO_TEST_CASE(blk_fromJson){
  auto [a1,a2,data] = get_example_address_and_data();

  Tx t1 = Tx(a1,a2,data,123/*nonce*/);
  Tx t2 = Tx(a1,a2,data,234/*nonce*/);

  json::value v{
    {"number", 1},
    {"parentHash", string(32*2,'b')},
    {"hash", string(32*2,'a')},
    {"txs", json::value_from(vector<Tx>({t1,t2}))}
    // 🦜 :json knows about vector and array
    // 🐢 :U have mentioned it already.
    // 🦜 : I known, but this did save us from alot  of for-loop
  };

  Blk b;
  bool ok = b.fromJson(v);

  BOOST_REQUIRE(ok);

  // Block info
  BOOST_CHECK_EQUAL(b.number,1);
  BOOST_CHECK_EQUAL(hashToString(b.hash), value_to<string>(v.at("hash")));
  BOOST_CHECK_EQUAL(hashToString(b.parentHash), value_to<string>(v.at("parentHash")));


  // Check Txs
  BOOST_CHECK_EQUAL(b.txs.size(),2);
  BOOST_CHECK_EQUAL(b.txs[0].nonce,123);
  BOOST_CHECK_EQUAL(b.txs[1].nonce,234);
  for (const Tx tx : b.txs){
    BOOST_CHECK_EQUAL(tx.data.size(),2);
    BOOST_CHECK_EQUAL(tx.data[0],0xff);
    BOOST_CHECK_EQUAL(tx.data[1],0xff);
    // We can check the addresses in txs， but let's stop here.
  }
}

BOOST_AUTO_TEST_SUITE_END();
