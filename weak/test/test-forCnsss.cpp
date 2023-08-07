#include "h.hpp"
#include "cnsss/forCnsss.hpp"
#include "mock.hpp"


using namespace weak;


BOOST_AUTO_TEST_CASE(test_jsonize_hash){
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),uint8_t{0xaa});
  json::value jv  = json::value_from(h);
  BOOST_CHECK_EQUAL(jv.as_string(),string(32*2,'a'));
}



// {{{ Test BlkCns

BOOST_AUTO_TEST_CASE(test_make_BlkCns){
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),uint8_t{0xaa});

  vector<hash256> v;
  for (uint8_t i : {0x11,0x22,0x33}){
    hash256 h0;
    std::fill(std::begin(h0.bytes),std::end(h0.bytes),i);
    v.push_back(h0);
  }

  BlkForConsensus b{123,h,v};

  BOOST_CHECK_EQUAL(b.number,123);
  BOOST_CHECK_EQUAL(hashToString(b.parentHash),
                    string(32*2,'a'));

  BOOST_CHECK_EQUAL(b.txhs.size(), 3);
  BOOST_CHECK_EQUAL(hashToString(b.txhs[0]),string(32*2,'1'));
  BOOST_CHECK_EQUAL(hashToString(b.txhs[1]),string(32*2,'2'));
  BOOST_CHECK_EQUAL(hashToString(b.txhs[2]),string(32*2,'3'));
}

BOOST_AUTO_TEST_CASE(test_BlkCns_to_json){
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),uint8_t{0xaa});

  vector<hash256> vh;
  for (uint8_t i : {0x11,0x22}){
    hash256 h0;
    std::fill(std::begin(h0.bytes),std::end(h0.bytes),i);
    vh.push_back(h0);
  }

  BlkForConsensus b{123,h,vh};

  json::value v = b.toJson();
  json::object o = v.as_object();

  BOOST_CHECK_EQUAL(value_to<uint64_t>(o["number"]),123);
  BOOST_CHECK_EQUAL(value_to<string>(o["parentHash"]), hashToString(h));
  BOOST_CHECK_EQUAL(value_to<string>(o["hash"]), hashToString(b.hash));

  // Check the hashes
  BOOST_CHECK_EQUAL(o["txhs"].as_array().size(),2);
  BOOST_CHECK_EQUAL(value_to<string>(o["txhs"].as_array()[0]),
                    string(32*2,'1'));
  BOOST_CHECK_EQUAL(value_to<string>(o["txhs"].as_array()[1]),
                    string(32*2,'2'));
}

BOOST_AUTO_TEST_CASE(test_BlkCns_from_json){
  vector<hash256> vh;
  for (uint8_t i : {0x11,0x22}){
    hash256 h0;
    std::fill(std::begin(h0.bytes),std::end(h0.bytes),i);
    vh.push_back(h0);
  }

  json::value v = {
    {"number", 1},
    {"parentHash", string(32*2,'b')},
    {"hash", string(32*2,'a')},
    {"txhs", json::value_from(vector<hash256>(vh))}
  };

  BlkForConsensus b;
  BOOST_REQUIRE(b.fromJson(v));

  BOOST_CHECK_EQUAL(b.number,1);
  BOOST_CHECK_EQUAL(hashToString(b.parentHash), string(32*2,'b'));
  BOOST_CHECK_EQUAL(hashToString(b.hash), string(32*2,'a'));

  BOOST_CHECK_EQUAL(b.txhs.size(), 2);
  BOOST_CHECK_EQUAL(hashToString(b.txhs[0]),string(32*2,'1'));
  BOOST_CHECK_EQUAL(hashToString(b.txhs[1]),string(32*2,'2'));
}

BlkForConsensus get_example_BlkForConsensus(){
  // make BlkForConsensus
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),uint8_t{0xaa});

  vector<hash256> txhs;
  for (uint8_t i : {0x11,0x22}){
    hash256 h0;
    std::fill(std::begin(h0.bytes),std::end(h0.bytes),i);
    txhs.push_back(h0);
  }

  return BlkForConsensus{123,h,txhs};
}

BOOST_AUTO_TEST_CASE(test_BlkCns_ToBlk_no){
  // 1. --------------------------------------------------
  BlkForConsensus b = get_example_BlkForConsensus();

  // 2. --------------------------------------------------
  // make hashtx source (an empty one)
  mockedHashTxGetter::A ah;
  IByHashTxGettable* p = dynamic_cast<IByHashTxGettable*>(&ah);

  // 3. --------------------------------------------------
  // Try to turn into Blk
  BOOST_CHECK(not b.toBlk(p));
}

void CHECK_BLK_EQUAL(const Blk & b0, const BlkForConsensus & b){
  BOOST_CHECK_EQUAL(b0.number,b.number);
  BOOST_CHECK_EQUAL(b0.hash,b.hash);
  BOOST_CHECK_EQUAL(b0.parentHash,b.parentHash);
  BOOST_CHECK_EQUAL(b0.txs.size(), b.txhs.size());

  for (int i=0;i<b0.txs.size();i++){ // the hash matches
    BOOST_CHECK_EQUAL(b0.txs[i].hash,b.txhs[i]);
  }
}

BOOST_AUTO_TEST_CASE(test_BlkCns_ToBlk_yes){
  // 1. --------------------------------------------------
  BlkForConsensus b = get_example_BlkForConsensus();
  // 2. --------------------------------------------------
  // make hashtx source (the answer-all one)
  mockedHashTxGetter::B ah;
  IByHashTxGettable* p = dynamic_cast<IByHashTxGettable*>(&ah);
  // 3. --------------------------------------------------
  // Try to turn into Blk
  auto r = b.toBlk(p);
  BOOST_CHECK(r);
  Blk b0 = r.value();
  CHECK_BLK_EQUAL(b0,b);
}

#include "cnsss/mempool.hpp"
BOOST_AUTO_TEST_CASE(test_BlkCns_ToBlk_real_mempool_yes){
  // 1. --------------------------------------------------
  // make BlkForConsensus
  Mempool ph;                   // pool host
  IByHashTxGettable* p = dynamic_cast<IByHashTxGettable*>(&ph);

  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  vector<hash256> txhs;

  for (uint64_t i : {1,2,3}){
    Tx t{a1,a2,data,i/*nonce*/};
    ph.addTx(t);       // put the Tx into the pool
    txhs.push_back(t.hash);
  }


  // 2. --------------------------------------------------
  // make the BlkForConsensus
  BlkForConsensus b = BlkForConsensus{123,{}/*parent hash*/,txhs};

  // 3. --------------------------------------------------
  // Try to turn into Blk
  auto r = b.toBlk(p);
  BOOST_CHECK(r);
  Blk b0 = r.value();
  CHECK_BLK_EQUAL(b0,b);
  BOOST_CHECK_EQUAL(ph.txs.size(),0); // pool is dried not
}

BOOST_AUTO_TEST_CASE(test_BlkCns_ToBlk_real_mempool_no){
  // 1. --------------------------------------------------
  // make BlkForConsensus
  Mempool ph;                   // pool host
  IByHashTxGettable* p = dynamic_cast<IByHashTxGettable*>(&ph);

  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  vector<hash256> txhs;

  Tx t{a1,a2,data,1/*nonce*/};
  Tx t2{a1,a2,data,2/*nonce*/};
  txhs.push_back(t.hash);
  ph.addTx(t2);       // put the Tx into the pool
  // tx in the blk header but not in the pool

  // 2. --------------------------------------------------
  // make the BlkForConsensus
  BlkForConsensus b = BlkForConsensus{123,{}/*parent hash*/,txhs};

  // 3. --------------------------------------------------
  // Try to turn into Blk
  auto r = b.toBlk(p);
  BOOST_CHECK(not r);
}

// }}}
