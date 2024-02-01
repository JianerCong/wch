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

BOOST_AUTO_TEST_SUITE(test_forPostExec);
// BOOST_AUTO_TEST_CASE(test_hi){
//   BOOST_CHECK_EQUAL(1,1);
// }

BOOST_AUTO_TEST_CASE(txReceipt_to_pb){
  TxReceipt r1{false},r2{bytes(size_t{2},uint8_t{0xff})},r11,r22;
  hiPb::TxReceipt pb1 = r1.toPb(),pb2 = r2.toPb();

  r11.fromPb(pb1);
  r22.fromPb(pb2);

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

ExecBlk makeExecBlk(){
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
  return b;
}


BOOST_AUTO_TEST_CASE(test_exec_blk_to_pb){
  ExecBlk b = makeExecBlk();

  hiPb::ExecBlk pb = b.toPb0();

  ExecBlk b2;
  b2.fromPb0(pb);

  BOOST_CHECK_EQUAL(b2.number, 1);
  BOOST_CHECK_EQUAL(b2.txs.size(), 2);
  BOOST_CHECK_EQUAL(b2.stateChanges.size(), 2);
  BOOST_CHECK_EQUAL(b2.txReceipts.size(), 2);
  BOOST_CHECK_EQUAL(b2.stateChanges[0].size(), 2);
  BOOST_CHECK_EQUAL(b2.stateChanges[1].size(), 1);

  BOOST_CHECK_EQUAL(b2.stateChanges[0][0].del, false);
  BOOST_CHECK_EQUAL(b2.stateChanges[0][0].k, "k1");
  BOOST_CHECK_EQUAL(b2.stateChanges[0][0].v, "v1");

  BOOST_CHECK_EQUAL(b2.txReceipts[0].ok, true);
  BOOST_CHECK_EQUAL(b2.txReceipts[1].ok, false);
}

BOOST_AUTO_TEST_CASE(test_tx_on_blk_info_pb){
  TxOnBlkInfo i{1,2};           // 2nd tx on block 1
  TxOnBlkInfo i2;

  auto v = i.toPb();
  i2.fromPb(v);
  BOOST_CHECK_EQUAL(i2.blkNumber,1);
  BOOST_CHECK_EQUAL(i2.onBlkId,2);
}

BOOST_AUTO_TEST_SUITE_END();
