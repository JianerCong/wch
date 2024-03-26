#include "h.hpp"

#include "cnsss/exeForCnsss.hpp"
#include "mock.hpp"

using namespace pure;
namespace mockedPool{
  // A broken pool.
  class A: public virtual IPoolSettable{
  public:
    optional<Tx> getTxByHash(hash256 h) noexcept override{
      return {};
    }
    bool verifyTx(const Tx & t) const noexcept override{
      return false;
    }
    bool addTx(const Tx & t) noexcept override{
      return false;
    }
  };


  /**
   * @brief The get-OK pool
   *
   * 🦜 : Answer Tx in `getTxByHash()` for some input hash, but it doesn't accept any Txs.
   */
  class B: public virtual IPoolSettable{
  public:
    vector<Tx> txs;
    vector<hash256> getTxhs(){
      vector<hash256> r;
      for (Tx t : this->txs){
        r.push_back(t.hash());
      }
      return r;
    }
    B(){
      address a1 = makeAddress(1);
      address a2 = makeAddress(2);
      bytes data(size_t{2},uint8_t{0xff}); // we would like this.

      this->txs.push_back(Tx(a1,a2,data,123/*nonce*/));
      this->txs.push_back(Tx(a1,a2,data,234/*nonce*/));
      this->txs.push_back(Tx(a1,a2,data,345/*nonce*/));
    }

    optional<Tx> getTxByHash(hash256 h) noexcept override{
      for (Tx t : this->txs){
        if (t.hash() == h)
          return t;
      }

      return {};
    }
    bool verifyTx(const Tx & t) const noexcept override{
      return false;
    }
    bool addTx(const Tx & t) noexcept override{
      return false;
    }
  };

  /**
   * @brief The set-OK pool
   *
   * 🦜 : It accepts any Txs, but it won't provide.
   */
  class C: public virtual IPoolSettable{
  public:
    optional<Tx> getTxByHash(hash256 h) noexcept override{
      return {};
    }
    bool verifyTx(const Tx & t) const noexcept override{
      return true;
    }
    bool addTx(const Tx & t) noexcept override{
      return true;
    }
  };

  /**
   * @brief The set-half-OK pool
   *
   * 🦜 : It accepts Tx, only if it rejected the previous one, but it won't
   * provide. So it will [accept, reject,accept,...]
   */
  class D: public virtual IPoolSettable{
  public:
    bool ok = false;
    optional<Tx> getTxByHash(hash256 h) noexcept override{
      return {};
    }
    bool verifyTx(const Tx & t) const noexcept override{
      // ok = not ok;
      return ok;
    }
    bool addTx(const Tx & t) noexcept override{
      // BOOST_TEST_MESSAGE("D::addTx called");
      ok = not ok;
      return ok;
    }
  };

}

struct F{
  mockedBlkExe::A bh;           // host
  mockedPool::A ph;
  IBlkExecutable * b;
  IPoolSettable * p;
  ExecutorForCnsss *eh;
  IForConsensusExecutable * e;

  F(){
    b = dynamic_cast<IBlkExecutable*>(&bh);
    p = dynamic_cast<IPoolSettable*>(&ph);
    eh= new ExecutorForCnsss(b,p);
    e = dynamic_cast<IForConsensusExecutable*>(eh);
    BOOST_TEST_MESSAGE( "setup fixture" );
  }

  ~F(){
    BOOST_TEST_MESSAGE( "teardown fixture" );
    delete eh;
  }
};

BOOST_FIXTURE_TEST_CASE(test_dispath_to_bad,F){
  string cmd = "1abc";
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,"");
  BOOST_CHECK(r.starts_with("Unknown Command Prefix"));
}

BOOST_FIXTURE_TEST_CASE(test_dispath_to_Blk_parse_BAD,F){
  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::EXECUTE_BLK)+ string("🦜 not a valid BlkForConsensus");
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,"");
  BOOST_TEST_MESSAGE("🦜 Got result: >>" + r + "<<");
  // 🦜 : <2024-03-26 Tue>: okay... It's hard to test this, because we don't know about pb... .
  // BOOST_CHECK(r.starts_with("Error parsing BlkForConsensus"));
}

BOOST_FIXTURE_TEST_CASE(test_dispath_to_Txs_parse_BAD,F){
  // string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS)+ string("abc");
  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS)+ string("🦜 not a valid Tx");
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,"");
  BOOST_TEST_MESSAGE("🦜 Got result: >>" + r + "<<");
  // BOOST_CHECK(r.starts_with("Error parsing Txs"));

}

BOOST_AUTO_TEST_CASE(test_dispath_to_Blk_parse_OK_exec_OK){
  // 1. --------------------------------------------------
  // make Header
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),uint8_t{0xaa});


  // 2. --------------------------------------------------
  // prepare host
  mockedBlkExe::A bh;           // this host doesn't commit
  mockedPool::B ph;             // this host provides Tx
  // --------------------------------------------------


  IBlkExecutable * b = dynamic_cast<IBlkExecutable*>(&bh);
  IPoolSettable * p = dynamic_cast<IPoolSettable*>(&ph);
  ExecutorForCnsss eh{b,p};
  IForConsensusExecutable * e = dynamic_cast<IForConsensusExecutable*>(&eh);
  // --------------------------------------------------
  BlkForConsensus blk{123,h,ph.getTxhs()};

  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::EXECUTE_BLK) + blk.toString();
  string cmd_old = cmd;
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,cmd_old);
  BOOST_CHECK(r.starts_with("OK"));
}

BOOST_AUTO_TEST_CASE(test_dispath_to_Blk_parse_OK_exec_BAD){
  // 1. --------------------------------------------------
  // make Header
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),uint8_t{0xaa});

  mockedPool::B ph;             // this host provides Tx
  vector<hash256> v = ph.getTxhs();

  BlkForConsensus blk{123,h,v};

  // 2. --------------------------------------------------
  // prepare host
  mockedBlkExe::B bh;           // this host doesn't commit
  IBlkExecutable * b = dynamic_cast<IBlkExecutable*>(&bh);
  IPoolSettable * p = dynamic_cast<IPoolSettable*>(&ph);
  ExecutorForCnsss eh{b,p};
  IForConsensusExecutable * e = dynamic_cast<IForConsensusExecutable*>(&eh);

  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::EXECUTE_BLK) + blk.toString();
  // string cmd_old = cmd;
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,"");
  BOOST_CHECK(r.starts_with("Error committing Blk"));
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

BOOST_AUTO_TEST_CASE(test_dispath_to_Txs_parse_OK_add_OK){
  // 1. --------------------------------------------------
  // make Txs
  vector<Tx> txs = getFakeTxs();

  // 2. --------------------------------------------------
  // prepare host
  mockedBlkExe::B bh;           // this host doesn't commit
  mockedPool::C ph;             // this pool accepts Tx

  IBlkExecutable * b = dynamic_cast<IBlkExecutable*>(&bh);
  IPoolSettable * p = dynamic_cast<IPoolSettable*>(&ph);
  ExecutorForCnsss eh{b,p};
  IForConsensusExecutable * e = dynamic_cast<IForConsensusExecutable*>(&eh);

  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS) +
    Tx::serialize_from_array(txs);

  string cmd_old = cmd;
  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,cmd_old);
  BOOST_CHECK(r.starts_with("OK"));
}

BOOST_AUTO_TEST_CASE(test_dispath_to_Txs_parse_OK_add_someOK){
  // 1. --------------------------------------------------
  // make Txs
  vector<Tx> txs = getFakeTxs();

  // 2. --------------------------------------------------
  // prepare host
  mockedBlkExe::B bh;           // this host doesn't commit
  mockedPool::D ph;             // this pool accepts partially Tx

  IBlkExecutable * b = dynamic_cast<IBlkExecutable*>(&bh);
  IPoolSettable * p = dynamic_cast<IPoolSettable*>(&ph);
  ExecutorForCnsss eh{b,p};
  IForConsensusExecutable * e = dynamic_cast<IForConsensusExecutable*>(&eh);

  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS) +
    Tx::serialize_from_array(txs);

  string cmd_old = cmd;
  string r = e->execute(cmd);
  BOOST_CHECK_NE(cmd,cmd_old);  // the cmd is changed, the bad tx is removed
  BOOST_CHECK_EQUAL(static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS),cmd.at(0));
  vector<Tx> txs0 = Tx::parse_to_array(cmd.substr(1));
  BOOST_CHECK_EQUAL(txs0.size(),1);
  BOOST_CHECK(r.starts_with("OK"));
}

BOOST_AUTO_TEST_CASE(test_dispath_to_Txs_parse_OK_add_all_thrown){
  // 1. --------------------------------------------------
  // make Txs
  vector<Tx> txs = getFakeTxs();

  // 2. --------------------------------------------------
  // prepare host
  mockedBlkExe::B bh;           // this host doesn't commit
  mockedPool::A ph;             // this pool doesn't accept Tx

  IBlkExecutable * b = dynamic_cast<IBlkExecutable*>(&bh);
  IPoolSettable * p = dynamic_cast<IPoolSettable*>(&ph);
  ExecutorForCnsss eh{b,p};
  IForConsensusExecutable * e = dynamic_cast<IForConsensusExecutable*>(&eh);

  string cmd = static_cast<char>(ExecutorForCnsss::Cmd::ADD_TXS) +
    Tx::serialize_from_array(txs);

  string r = e->execute(cmd);
  BOOST_CHECK_EQUAL(cmd,"");  // the cmd is emptied, the bad tx is removed

  BOOST_CHECK(r.starts_with("All txs rejected"));
}
