#include "h.hpp"
#include "cnsss/cnsssBlkChainAsstn.hpp"
#include "pure-mock.hpp"


using namespace weak;
using namespace pure;

BlkForConsensus get_example_BlkForConsensus(){
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),uint8_t{0xaa});

  vector<hash256> v;
  for (uint8_t i : {0x11,0x22,0x33}){
    hash256 h0;
    std::fill(std::begin(h0.bytes),std::end(h0.bytes),i);
    v.push_back(h0);
  }

  return BlkForConsensus{123,h,v};
}

BOOST_AUTO_TEST_SUITE(test_cnsssBlkChainAsstn,MY_TEST_THIS);
BOOST_AUTO_TEST_CASE(test_basic_cnsssBlkChainAsstn_primary){
  mockedCnsss::A ch{true};     // primary cnsss
  ICnsssPrimaryBased * c = dynamic_cast<ICnsssPrimaryBased*>(&ch);
  CnsssBlkChainAsstn a{c};

  IForSealerBlkPostable * p = dynamic_cast<IForSealerBlkPostable*>(&a);
  BOOST_CHECK(p->isPrimary());
  BlkForConsensus b = get_example_BlkForConsensus();
  BOOST_CHECK(p->postBlk(b));
}

BOOST_AUTO_TEST_CASE(test_basic_cnsssBlkChainAsstn_sub){
  mockedCnsss::A ch{false};     // sub cnsss
  ICnsssPrimaryBased * c = dynamic_cast<ICnsssPrimaryBased*>(&ch);
  CnsssBlkChainAsstn a{c};

  IForSealerBlkPostable * p = dynamic_cast<IForSealerBlkPostable*>(&a);
  BOOST_CHECK(not p->isPrimary());
  BlkForConsensus b = get_example_BlkForConsensus();
  BOOST_CHECK(p->postBlk(b));
}

BOOST_AUTO_TEST_CASE(test_what_is_submitted_by_exeBlk){
  mockedCnsss::B ch{true};     // primary cnsss
  ICnsssPrimaryBased * c = dynamic_cast<ICnsssPrimaryBased*>(&ch);
  CnsssBlkChainAsstn a{c};

  IForSealerBlkPostable * p = dynamic_cast<IForSealerBlkPostable*>(&a);
  BOOST_CHECK(p->isPrimary());
  BlkForConsensus b = get_example_BlkForConsensus();
  BOOST_CHECK(p->postBlk(b));

  // check what's posted
  BOOST_REQUIRE_EQUAL(ch.cmds.size(),1);
  BOOST_CHECK_EQUAL(ch.cmds[0],CnsssBlkChainAsstn::EXECUTE_BLK + b.toString());
  }

tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
}

BOOST_AUTO_TEST_CASE(test_what_is_submitted_by_addTxs){
  // 0.--------------------------------------------------
  mockedCnsss::B ch{true};     // primary cnsss
  ICnsssPrimaryBased * c = dynamic_cast<ICnsssPrimaryBased*>(&ch);
  CnsssBlkChainAsstn a{c};


  // 1. --------------------------------------------------
  auto [a1,a2,data] = get_example_address_and_data();
  vector<Tx> txs = {
    Tx(a1,a2,data,123/*nonce*/),
    Tx(a1,a2,data,234/*nonce*/),
    Tx(a2,a1,data,123/*nonce*/) // same nonce, but different from
  };
  string s = Tx::serialize_from_array(txs);

  IForRpcTxsAddable * p = dynamic_cast<IForRpcTxsAddable *>(&a);
  BOOST_CHECK(p->addTxs(std::move(txs)));

  // check what's posted
  BOOST_REQUIRE_EQUAL(ch.cmds.size(),1);
  BOOST_CHECK_EQUAL(ch.cmds[0],CnsssBlkChainAsstn::ADD_TXS + s);
}

BOOST_AUTO_TEST_SUITE_END();
