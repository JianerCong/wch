#include "h.hpp"

#include "execManager.hpp"
#include "mock.hpp"
using namespace weak;




BOOST_AUTO_TEST_CASE(test_mockedBlkExe){
  mockedBlkExe::A me;           // complete empty Blk exe
  IBlkExecutable* const p = dynamic_cast<IBlkExecutable*>(&me);

  Blk b0;
  ExecBlk b = p->executeBlk(b0);
  BOOST_REQUIRE(p->commitBlk(b));
}

#include <boost/random.hpp>
#include <ctime>
namespace mockedTxExe{
  // 🦜 : Fail for every tx;
  class A : public virtual ITxExecutable{
  public:
    optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                         const Tx & t) const noexcept override{
      return {};
    }
  };

  // 🦜 : Pass for every tx;
  class B : public virtual ITxExecutable{
  public:
    optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                         const Tx & t) const noexcept override{
      return make_tuple(vector<StateChange>({}),bytes({}));
    }
  };


  // 🦜 : Pass for 50% percent;
  class C : public virtual ITxExecutable{
  public:
    optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                         const Tx & t) const noexcept override{

      std::time_t now = std::time(0);
      static boost::random::mt19937 gen{static_cast<uint32_t>(now)};
      if (gen() % 2 == 1)
        return make_tuple(vector<StateChange>({}),bytes({}));
      else
        return {};
    }
  };
}

void my_test_executeBlk(auto ex){
  // 🦜 : Bring in our IN-RAM rocksdb
  mockedAcnPrv::E w;
  BlkExecutor be(
                 dynamic_cast<IWorldChainStateSettable*>(&w),
                 dynamic_cast<ITxExecutable*>(&ex),
                 dynamic_cast<IAcnGettable*>(&w)
                 );
  IBlkExecutable* e = dynamic_cast<IBlkExecutable*>(&be);

  // 1. --------------------------------------------------
  // Make a blk
  address a1 = makeAddress(10);
  address a2 = makeAddress(20);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  vector<Tx> txs ={
    Tx(a1,a2,data,123/*nonce*/),
    Tx(a1,a2,data,123/*nonce*/),
    Tx(a1,a2,data,234/*nonce*/)
  };

  // parent hash
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),0x00);
  // make block
  Blk b0 = Blk(1,h,txs);

  // 2. --------------------------------------------------
  // Execute in with that aways-fail executor
  ExecBlk b = e->executeBlk(b0);
  BOOST_CHECK_EQUAL(b.number,1);
}

BOOST_AUTO_TEST_CASE(test_blkexe_execute_Tx_pass){
  // 🦜 : The aways-pass Tx-executable
  my_test_executeBlk(mockedTxExe::B{});
}

BOOST_AUTO_TEST_CASE(test_blkexe_execute_Tx_random_pass){
  // 🦜 : The random-fail Tx-executable
  my_test_executeBlk(mockedTxExe::C{});
}

BOOST_AUTO_TEST_CASE(test_commit_blk){
  // 1. --------------------------------------------------
  // Make ExecBlk
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.

  Tx tx1{a1,a2,data,234/*nonce*/};
  vector<Tx> txs{
    Tx(a1,a2,data,123/*nonce*/),
    tx1
  };

  // parent hash
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),0x00);
  // make block
  BOOST_LOG_TRIVIAL(info) << format("Making block");
  Blk b0 = Blk(1,h,txs);

  Acn an1{123, evmc::from_hex("0000aabb").value()};
  Acn an2{234, evmc::from_hex("0000aabb").value()};
  vector<vector<StateChange>> j = {
    {{false,"k1",an1.toString()}, {true,"k2",""}}, // journal for tx1
    {{false,"k1",an2.toString()}}, // journal for tx2
  };

  vector<TxReceipt> rc = {
    bytes(size_t{3},uint8_t{0xff}), // passed tx
    TxReceipt(false)            // failed tx
  };

  ExecBlk b{b0,j,rc};

  // 2. --------------------------------------------------
  // Commit to the in-RAM rocksdb
  mockedTxExe::A ex;            // the always fail exec
  mockedAcnPrv::F w;            // Real in-RAM World-state
  BlkExecutor be(dynamic_cast<IWorldChainStateSettable*>(&w),
                 dynamic_cast<ITxExecutable*>(&ex),
                 dynamic_cast<IAcnGettable*>(&w)
                 );
  IBlkExecutable* e = dynamic_cast<IBlkExecutable*>(&be);
  BOOST_REQUIRE(e->commitBlk(b));
  BOOST_CHECK(bool(w.getFromChainDB("/blk/1")));
  BOOST_CHECK(not bool(w.getFromChainDB("/blk/2")));


  // Is tx hash: Tx Info there ?
  auto r = w.getFromChainDB("/tx/" + hashToString(tx1.hash));
  BOOST_REQUIRE(bool{r});
  TxOnBlkInfo bi{1 /*block number*/,1/* the index (1-based)*/};
  // This represents the second Tx on Blk-1 ⇒ b1.txs[1]
  BOOST_CHECK_EQUAL(r.value(),bi.toString());

  // Is the blk_number there ?
  r = w.getFromChainDB("/other/blk_number");
  BOOST_REQUIRE(bool{r});
  BOOST_CHECK_EQUAL(r.value(),"1");
}
