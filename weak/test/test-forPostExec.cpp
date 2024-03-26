#include "h.hpp"

#include "forPostExec.hpp"
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

BOOST_AUTO_TEST_SUITE(test_forPostExec,MY_TEST_THIS);

BOOST_AUTO_TEST_CASE(test_TxBlkInfo){
  TxOnBlkInfo i{1,2};           // 2nd tx on block 1
  TxOnBlkInfo i2;

  auto v = i.toJson();
  BOOST_REQUIRE(i2.fromJson(v));
  BOOST_CHECK_EQUAL(i2.blkNumber,1);
  BOOST_CHECK_EQUAL(i2.onBlkId,2);
}

BOOST_AUTO_TEST_CASE(test_txReceipt){
  TxReceipt r1{false},r2{bytes(size_t{2},uint8_t{0xff})},r11,r22;
  auto v1 = r1.toJson();
  auto v2 = r2.toJson();

  BOOST_REQUIRE(r11.fromJson(v1));
  BOOST_REQUIRE(r22.fromJson(v2));

  BOOST_CHECK_EQUAL(r11.ok,false);
  BOOST_CHECK_EQUAL(r11.result.size(),0);

  BOOST_CHECK_EQUAL(r22.ok,true);
  BOOST_CHECK_EQUAL(r22.result.size(),2);
}

vector<Tx> getFakeTxs(){
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return {
    Tx(a1,a2,data,123/*nonce*/),
    Tx(a1,a2,data,234/*nonce*/)
  };
}

BOOST_AUTO_TEST_CASE(test_makeExecBlk){
  vector<Tx> txs = getFakeTxs();
  // parent hash
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),0x00);
  // make block
  BOOST_LOG_TRIVIAL(info) << format("Making block");
  Blk b0 = Blk(1,h,txs);

  vector<vector<StateChange>> j = {
    {{false,"k1","v1"}, {true,"k2",""}}, // journal for tx1
    {{false,"k1","v2"}}, // journal for tx2
  };

  vector<TxReceipt> r = {
    bytes(size_t{3},uint8_t{0xff}), // passed tx
    TxReceipt(false)            // failed tx
  };

  ExecBlk b{b0,j,r};


  BOOST_CHECK_EQUAL(b.number,1);
  BOOST_CHECK_EQUAL(b.txs.size(),2);
  BOOST_CHECK_EQUAL(b.stateChanges.size(),2);
  BOOST_CHECK_EQUAL(b.txReceipts.size(),2);
}

BOOST_AUTO_TEST_CASE(test_ExecBlk_to_json){
  // 1. --------------------------------------------------
  // Make execBlk
  vector<Tx> txs = getFakeTxs();
  // parent hash
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),0x00);
  // make block
  BOOST_LOG_TRIVIAL(info) << format("Making block");
  Blk b0 = Blk(1,h,txs);

  vector<vector<StateChange>> j = {
    {{false,"k1","v1"}, {true,"k2",""}}, // journal for tx1
    {{false,"k1","v2"}}, // journal for tx2
  };

  vector<TxReceipt> r = {
    bytes(size_t{3},uint8_t{0xff}), // passed tx
    TxReceipt(false)            // failed tx
  };

  ExecBlk b{b0,j,r};
  // 2. --------------------------------------------------
  // Get json
  json::value v = b.toJson();
  json::value v0 = b0.toJson();

  BOOST_REQUIRE(v.is_object());
  BOOST_REQUIRE(v0.is_object()); // This check is not necessary

  json::object o = v.as_object();
  json::object o0 = v0.as_object();

  // 🦜 : Compared to Blk, two more fields are added.
  BOOST_CHECK_EQUAL(o.size(),
                    o0.size() + 2);
  // Check the fields with the original Blk
  BOOST_CHECK_EQUAL(
                    o["hash"].as_string(),
                    o0["hash"].as_string()
                    );

  // Check specific fields
  BOOST_REQUIRE(o["stateChanges"].is_array());
  BOOST_CHECK_EQUAL(o["stateChanges"].as_array().size(),2);
  BOOST_REQUIRE(o["txReceipts"].is_array());
  BOOST_CHECK_EQUAL(o["txReceipts"].as_array().size(),2);

}

BOOST_AUTO_TEST_CASE(test_ExecBlk_from_json){
  // 1. --------------------------------------------------
  // Make execBlk
  vector<Tx> txs = getFakeTxs();
  // parent hash
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),0x00);
  // make block
  BOOST_LOG_TRIVIAL(info) << format("Making block");
  Blk b0 = Blk(1,h,txs);

  /*
    <2024-03-26 Tue> 🦜 : When we are using pb serialization, the value should be hex
   */
  vector<vector<StateChange>> j = {
    {{false,"k1","11"}, {true,"k2",""}}, // journal for tx1
    {{false,"k1","22"}}, // journal for tx2
  };

  vector<TxReceipt> r = {
    bytes(size_t{3},uint8_t{0xff}), // passed tx
    TxReceipt(false)            // failed tx
  };

  ExecBlk b{b0,j,r};
  // 2. --------------------------------------------------
  // Get json
  json::value v = b.toJson();

  // 3. --------------------------------------------------
  // Apply the json
  ExecBlk b1;
  BOOST_REQUIRE(b1.fromJson(v));
  BOOST_CHECK_EQUAL(b1.number,b.number);
  BOOST_CHECK_EQUAL(b1.txs.size(),b.txs.size());
  BOOST_CHECK_EQUAL(b1.txs[0].nonce,b.txs[0].nonce);
  BOOST_CHECK_EQUAL(b1.txs[1].nonce,b.txs[1].nonce);
  BOOST_CHECK_EQUAL(b.hash(), b1.hash());
  // statechanges --------------------------------------
  BOOST_CHECK_EQUAL(b1.stateChanges.size(),b.stateChanges.size());
  BOOST_CHECK_EQUAL(b1.stateChanges[0].size(),2); // journal for tx1
  BOOST_CHECK_EQUAL(b1.stateChanges[0][0].del,false); // first entry in tx1
  BOOST_CHECK_EQUAL(b1.stateChanges[0][0].k,"k1"); // first entry in tx1
  BOOST_CHECK_EQUAL(b1.stateChanges[0][0].v,"\x11"); // first entry in tx1

  BOOST_CHECK_EQUAL(b1.stateChanges[0][1].del,true); // second entry in tx1
  BOOST_CHECK_EQUAL(b1.stateChanges[0][1].k,"k2"); // second entry in tx1
  BOOST_CHECK_EQUAL(b1.stateChanges[0][1].v,""); // second entry in tx1

  BOOST_CHECK_EQUAL(b1.stateChanges[1][0].del,false); // second entry in tx1
  BOOST_CHECK_EQUAL(b1.stateChanges[1][0].k,"k1"); // second entry in tx1
  BOOST_CHECK_EQUAL(b1.stateChanges[1][0].v,"\x22"); // second entry in tx1

  //  --------------------------------------------------
  // txReceipts ----------------------------------------
  BOOST_CHECK_EQUAL(b1.txReceipts.size(),b.txReceipts.size());
  BOOST_CHECK_EQUAL(b1.txReceipts.size(),2);
  BOOST_CHECK_EQUAL(b1.txReceipts[0].ok,true);
  BOOST_CHECK_EQUAL(b1.txReceipts[0].result,
                    bytes(size_t{3},uint8_t{0xff}) // passed result
                    );
  BOOST_CHECK_EQUAL(b1.txReceipts[1].ok,false);
  //  --------------------------------------------------
}

BOOST_AUTO_TEST_SUITE_END();
