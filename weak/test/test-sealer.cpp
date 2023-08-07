#include "h.hpp"
#include "cnsss/sealer.hpp"
#include "cnsss/mempool.hpp"

using namespace weak;
namespace mockedIForSealerTxHashesGettable{
  // A completely empty provider
  class A: public virtual IForSealerTxHashesGettable{
  public:
    vector<hash256> getTxHashesForSeal() noexcept override{
      return {};
    }
  };

  // A constant provider
  class B: public virtual IForSealerTxHashesGettable{
  public:

    vector<hash256> getTxHashesForSeal() noexcept override{
      address a1 = makeAddress(10);
      address a2 = makeAddress(20);
      bytes data(size_t{2},uint8_t{0xff}); // we would like this.
      vector<Tx> txs ={
        Tx(a1,a2,data,123/*nonce*/),
        Tx(a1,a2,data,234/*nonce*/)
      };
      return {txs[0].hash, txs[1].hash};
    }
  };

}

namespace mockedIForSealerBlkPostable{

  class A: public virtual IForSealerBlkPostable{
  public:
    bool postBlk(const BlkForConsensus & b) noexcept override{
      BOOST_LOG_TRIVIAL(debug) << format("postBlk called with:\n" S_CYAN "%s" S_NOR)
        % b.toJsonString();
      return true;
    }
    bool isPrimary() noexcept override{
      return true;
    }
  };

  // The Blk receiver that record all the posted blks.
  class B: public virtual IForSealerBlkPostable{
  public:
    vector<BlkForConsensus> bs;
    bool postBlk(const BlkForConsensus & b) noexcept override{
      BOOST_LOG_TRIVIAL(debug) << format("postBlk called with:\n" S_CYAN "%s" S_NOR)
        % b.toJsonString();
      bs.push_back(b);
      return true;
    }
    bool isPrimary() noexcept override{
      return true;
    }
  };

}

BOOST_AUTO_TEST_CASE(test_mockedIForSealerTxsGettable){
  mockedIForSealerTxHashesGettable::A h;
  IForSealerTxHashesGettable * const g = dynamic_cast<IForSealerTxHashesGettable*>(&h);

  BOOST_CHECK_EQUAL(g->getTxHashesForSeal().size(),0);
}

BOOST_AUTO_TEST_CASE(test_mockedIForSealerBlkPostable){
  mockedIForSealerBlkPostable::A host;
  IForSealerBlkPostable * const p = dynamic_cast<IForSealerBlkPostable*>(&host);


  // make BlkForConsensus
  vector<hash256> txhs;
  for (uint8_t i : {0x11,0x22}){
    hash256 h0;
    std::fill(std::begin(h0.bytes),std::end(h0.bytes),i);
    txhs.push_back(h0);
  }

  BlkForConsensus blk{123,{},txhs};

  BOOST_CHECK(p->isPrimary());
  BOOST_CHECK(p->postBlk(blk));
}

// BOOST_AUTO_TEST_CASE(test_basic_sealer_empty_provider){

//   // init the source
//   mockedIForSealerBlkPostable::B ch; // consensus host
//   IForSealerBlkPostable * const consensus = dynamic_cast<IForSealerBlkPostable*>(&ch);

//   mockedIForSealerTxHashesGettable::A mh; // mempool host
//   IForSealerTxHashesGettable * const mempool = dynamic_cast<IForSealerTxHashesGettable*>(&mh);

//   {
//     Sealer s{consensus,mempool,0 /*next Blk number*/ ,
//              {} /* parent hash*/ ,2 /* sealing interval*/};
//     std::this_thread::sleep_for(std::chrono::seconds(5));
//   } // sealer ends here
//   BOOST_CHECK_EQUAL(ch.bs.size(),0);
// }

// BOOST_AUTO_TEST_CASE(test_basic_sealer_const_provider){

//   // init the source
//   mockedIForSealerBlkPostable::B ch; // consensus host
//   IForSealerBlkPostable * const consensus = dynamic_cast<IForSealerBlkPostable*>(&ch);

//   mockedIForSealerTxHashesGettable::B mh; // mempool host
//   IForSealerTxHashesGettable * const mempool = dynamic_cast<IForSealerTxHashesGettable*>(&mh);

//   int N;
//   {
//     Sealer s{consensus,mempool,0 /*next Blk number*/ ,
//              {} /* parent hash*/ ,2 /* sealing interval*/};

//     std::this_thread::sleep_for(std::chrono::seconds(5));
//     N = (5 / s.wait_s);         // these many Blks
//   } // sealer ends here

//   BOOST_CHECK_EQUAL(ch.bs.size(),N);
// }


BOOST_AUTO_TEST_CASE(test_basic_sealer_addable_provider){

  // init the source
  mockedIForSealerBlkPostable::B ch; // consensus host
  IForSealerBlkPostable * const consensus = dynamic_cast<IForSealerBlkPostable*>(&ch);

  Mempool mh; // mempool host
  IForSealerTxHashesGettable * const mempool = dynamic_cast<IForSealerTxHashesGettable*>(&mh);

  // a tx
  address a1 = makeAddress(10);
  address a2 = makeAddress(20);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.

  {

    vector<Tx> txs{
      Tx{a1,a2,data,234/*nonce*/},
      Tx{a1,a2,data,235/*nonce*/},
      Tx{a1,a2,data,236/*nonce*/}
    };

    Sealer s{consensus,mempool,0 /*next Blk number*/ ,
             {} /* parent hash*/ ,1 /* sealing interval*/};
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // a 2-txs Blk
    mh.addTx(txs[0]);
    mh.addTx(txs[1]);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // seals here
    mh.getTxByHash(txs[0].hash);
    mh.getTxByHash(txs[1].hash);
    // 🦜 : Pop these two. Usually the executor does it.

    // a 1-tx Blk
    mh.addTx(txs[2]);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  } // sealer ends here

  BOOST_CHECK_EQUAL(ch.bs.size(),2);
  BOOST_CHECK_EQUAL(ch.bs[0].number,0);
  BOOST_CHECK_EQUAL(ch.bs[0].txhs.size(),2);
  BOOST_CHECK_EQUAL(ch.bs[1].number,1);
  BOOST_CHECK_EQUAL(ch.bs[1].txhs.size(),1);
  BOOST_CHECK_EQUAL(ch.bs[1].parentHash, ch.bs[0].hash);
}
