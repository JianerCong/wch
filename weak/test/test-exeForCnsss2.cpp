/**
 * @file test-exeForCnsss2.cpp
 * @brief Test the LightExecutorForCnsss
 */
#include "h.hpp"

#include "cnsss/exeForCnsss.hpp"
#include "mock.hpp"

using namespace pure;

struct F{
  mockedBlkExe::C bh;           // host that remembers which are executed.
  IBlkExecutable * b;
  unique_ptr<LightExecutorForCnsss> eh;
  IForConsensusExecutable * e;

  F(){
    b = dynamic_cast<IBlkExecutable*>(&bh);
    eh= make_unique<LightExecutorForCnsss>(b,
                                           nullptr, // pool
                                           2 /*optimization level, when > 2, skip hash duplication check*/
                                           );
    e = dynamic_cast<IForConsensusExecutable*>(&(*eh));
    BOOST_TEST_MESSAGE( "setup fixture" );
  }

  ~F(){
    BOOST_TEST_MESSAGE( "teardown fixture" );
  }
};

BOOST_FIXTURE_TEST_CASE(test_dispath_to_bad,F){
  string cmd = "1abc";
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,"");
  BOOST_CHECK(r.starts_with("Unknown Command Prefix"));

}

BOOST_FIXTURE_TEST_CASE(test_dispath_to_Blk_parse_BAD,F){
  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::EXECUTE_BLK)+ string("abc");
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,"");
  BOOST_CHECK(r.starts_with("Error parsing Blk"));
}

BOOST_FIXTURE_TEST_CASE(test_dispath_to_Txs_parse_BAD,F){
  // string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS)+ string("abc");
  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS)+ string("🦜 not a valid Tx");
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,"");
  BOOST_CHECK(r.starts_with("Error parsing Txs"));

  // 🦜 : <2024-03-26 Tue>: okay... It's hard to test this, because we don't
  // know about pb...
  // 🦜 : Update: It seems like random data are parsed to empty array, so now we know it.
}


tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
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

BOOST_FIXTURE_TEST_CASE(test_exec_blk_ok,F){
  vector<Tx> txs = getFakeTxs();
  // parent hash
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),0x00);

  // make block
  Blk b = Blk(1,h,txs);

  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::EXECUTE_BLK)+ b.toString();
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(r,"OK");

  // 🦜 it is executed and commited.
  BOOST_CHECK_EQUAL(bh.blk_numbers_commited.size(),1);
  BOOST_CHECK_EQUAL(bh.blk_numbers_executed.size(),1);
  BOOST_CHECK_EQUAL(bh.blk_numbers_executed[0],b.number);
  BOOST_CHECK_EQUAL(bh.blk_numbers_commited[0],b.number);
}

BOOST_FIXTURE_TEST_CASE(test_add_txs_ok,F){
  vector<Tx> txs = getFakeTxs();
  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS)+ Tx::serialize_from_array(txs);

  // 🦜 : Execute once, now the command should have been turned into an EXECUTE_BLK
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(r,"OK");
  BOOST_REQUIRE(cmd.starts_with(
                                static_cast<char>(ExecutorForCnsss::Cmd::EXECUTE_BLK)));
  // the new blk is here
  Blk b0;
  BOOST_TEST_MESSAGE((format("Got cmd: %s") %
                      ::pure::get_data_for_log(cmd)
                      ).str());

  BOOST_REQUIRE(b0.fromString(cmd.substr(1)));
  BOOST_CHECK_EQUAL(b0.txs.size(),txs.size());
  BOOST_CHECK_EQUAL(b0.number,0);

  // And it should have already been executed and commited
  BOOST_CHECK_EQUAL(bh.blk_numbers_commited.size(),1);
  BOOST_CHECK_EQUAL(bh.blk_numbers_executed.size(),1);
  BOOST_CHECK_EQUAL(bh.blk_numbers_executed[0],b0.number);
  BOOST_CHECK_EQUAL(bh.blk_numbers_commited[0],b0.number);
  BOOST_CHECK_EQUAL(eh->next_blk_number,1);
  BOOST_CHECK_EQUAL(eh->previous_hash,b0.hash());
}

BOOST_FIXTURE_TEST_CASE(test_add_txs_ok_twice,F){
  vector<Tx> txs = getFakeTxs();
  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS)+ Tx::serialize_from_array(txs);
  // the new blk is here
  Blk b0,b1;

  // 🦜 : Execute once, now the command should have been turned into an EXECUTE_BLK
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(r,"OK");
  BOOST_REQUIRE(b0.fromString(cmd.substr(1)));
  BOOST_CHECK_EQUAL(b0.txs.size(),txs.size());
  BOOST_CHECK_EQUAL(b0.number,0);

  // 🦜 : execute twice, the same cmd
  cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS)+ Tx::serialize_from_array(txs);
  r = e->execute(cmd);
  BOOST_CHECK_EQUAL(r,"OK");
  // BOOST_TEST_MESSAGE((format("Got cmd: %s") % cmd).str());
  BOOST_REQUIRE(b1.fromString(cmd.substr(1)));
  BOOST_CHECK_EQUAL(b1.txs.size(),txs.size());
  BOOST_CHECK_EQUAL(b1.number,1);

  // 🦜 And they should have already been executed and commited
  BOOST_CHECK_EQUAL(bh.blk_numbers_commited.size(),2);
  BOOST_CHECK_EQUAL(bh.blk_numbers_executed.size(),2);
  BOOST_CHECK_EQUAL(bh.blk_numbers_executed[0],0);
  BOOST_CHECK_EQUAL(bh.blk_numbers_commited[0],0);
  BOOST_CHECK_EQUAL(bh.blk_numbers_executed[1],1);
  BOOST_CHECK_EQUAL(bh.blk_numbers_commited[1],1);
}

BOOST_AUTO_TEST_CASE(test_start_different_init_blk_num){
  mockedBlkExe::C bh;           // host that remembers which are executed.
  IBlkExecutable * b= dynamic_cast<IBlkExecutable*>(&bh);
  string s = "abc";
  hash256 h = ethash::keccak256(reinterpret_cast<uint8_t*>(s.data()), s.size());

  LightExecutorForCnsss e{b, nullptr, 2 /*optimization level*/,
    123 /* next_blk_number*/ ,h /* previous hash*/};

  // Add txs
  vector<Tx> txs = getFakeTxs();
  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS)+ Tx::serialize_from_array(txs);

  // 🦜 : Execute once, now the command should have been turned into an EXECUTE_BLK
  string r = e.execute(cmd);
  BOOST_CHECK_EQUAL(r,"OK");
  BOOST_REQUIRE(cmd.starts_with(static_cast<char>(ExecutorForCnsss::Cmd::EXECUTE_BLK)));
  // the new blk is here
  Blk b0;
  BOOST_TEST_MESSAGE((format("Got cmd: %s") %
                      ::pure::get_data_for_log(cmd)).str());

  BOOST_REQUIRE(b0.fromString(cmd.substr(1)));
  BOOST_CHECK_EQUAL(b0.txs.size(),txs.size());
  BOOST_CHECK_EQUAL(b0.number,123);
  BOOST_CHECK_EQUAL(b0.parentHash,h);
}
