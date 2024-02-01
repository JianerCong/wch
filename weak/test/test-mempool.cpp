#include "h.hpp"
#include "mock.hpp"

#include "core.hpp"
#include "cnsss/mempool.hpp"

#include <thread>               // for sleep
#include <tuple>

using std::unordered_set;
using namespace weak;
BOOST_AUTO_TEST_SUITE(test_mempool,MY_TEST_THIS);

tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
}

BOOST_AUTO_TEST_CASE(test_mempool_basic){
  Mempool p;

  auto [a1,a2,data] = get_example_address_and_data();
  vector<Tx> txs = {
    Tx(a1,a2,data,123/*nonce*/),
    Tx(a1,a2,data,234/*nonce*/),
    Tx(a2,a1,data,123/*nonce*/) // same nonce, but different from
  };

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(100ms);
  Tx t4 = Tx(a2,a1,data,123/*nonce*/); // just different timestamp

  // 1. --------------------------------------------------
  // Add the txs
  BOOST_REQUIRE(p.addTx(txs[0]));
  BOOST_CHECK_EQUAL(p.txs.size(),1);
  BOOST_REQUIRE(p.addTx(txs[1]));
  BOOST_CHECK_EQUAL(p.txs.size(),2);
  BOOST_REQUIRE(not p.addTx(txs[1])); // double spend
  BOOST_CHECK_EQUAL(p.txs.size(),2);
  BOOST_REQUIRE(p.addTx(txs[2]));
  BOOST_CHECK_EQUAL(p.txs.size(),3);

  BOOST_REQUIRE(not p.addTx(t4));
  BOOST_CHECK_EQUAL(p.txs.size(),3);
}


// test iteration --------------------------------------------
BOOST_AUTO_TEST_CASE(test_mempool_iter){
  Mempool p;

  // the txs
  auto [a1,a2,data] = get_example_address_and_data();

  // using namespace std::chrono_literals;
  Tx t = Tx(a1,a2,data,123/*nonce*/);
  // std::this_thread::sleep_for(1000ms);
  Tx t2 = Tx(a1,a2,data,234/*nonce*/);
  // std::this_thread::sleep_for(1000ms);
  Tx t3 = Tx(a2,a1,data,123/*nonce*/);

  // 1. --------------------------------------------------
  // Add the txs
  BOOST_REQUIRE(p.addTx(t2));
  BOOST_CHECK_EQUAL(p.txs.size(),1);
  BOOST_REQUIRE(p.addTx(t));
  BOOST_CHECK_EQUAL(p.txs.size(),2);

  int c{0};
  vector<Tx> v;
  for (Tx t : p.txs){
    c++;
    v.push_back(t);
  }
  BOOST_CHECK_EQUAL(c,2);
  BOOST_CHECK_EQUAL(v[0].nonce,123); // tx are ordered
  BOOST_CHECK_EQUAL(v[1].nonce,234); // tx are ordered
}

// test iteration and remove ---------------------------------
BOOST_AUTO_TEST_CASE(test_mempool_erase){
  Mempool p;
  auto [a1,a2,data] = get_example_address_and_data();

  using namespace std::chrono_literals;
  Tx t = Tx(a1,a2,data,123/*nonce*/);
  // std::this_thread::sleep_for(1000ms);
  Tx t2 = Tx(a1,a2,data,234/*nonce*/);
  // std::this_thread::sleep_for(1000ms);
  Tx t3 = Tx(a2,a1,data,123/*nonce*/);

  // 1. --------------------------------------------------
  // Add the txs
  BOOST_REQUIRE(p.addTx(t2));
  BOOST_CHECK_EQUAL(p.txs.size(),1);
  BOOST_REQUIRE(p.addTx(t));
  BOOST_CHECK_EQUAL(p.txs.size(),2);

  vector<Tx> v;
  for (Tx t : p.txs){
    v.push_back(t);
  }

  // remove from sets
  p.txs.erase(v[0]);
  BOOST_CHECK_EQUAL(p.txs.size(),1);
}

// test pop --------------------------------------------------
BOOST_AUTO_TEST_CASE(test_mempool_getTxHashesForSeal){
  // Check the first two items
  int N = 2;
  Mempool p(make_unique<Mempool::Hash_set>()
            ,N /*number per batch*/);

  auto [a1,a2,data] = get_example_address_and_data();

  const int M = 5;
  // Add the txs
  for (int i=0;i<M;i++){
    // BOOST_LOG_TRIVIAL(info) << format("Making tx %d") % i;
    BOOST_CHECK(p.addTx(Tx(a1,a2,data,i/*nonce*/)));
  }

  vector<hash256> v = p.getTxHashesForSeal();

  BOOST_CHECK_EQUAL(v.size(),N); // 2 popped
  BOOST_CHECK_EQUAL(p.txs.size(),M); // all the txs remains

  for (int i=0;i<N;i++){
    BOOST_CHECK(p.txhs.contains(v[i]));
  }

}


BOOST_AUTO_TEST_CASE(test_mock_IByHashTxGettable_no){
  mockedHashTxGetter::A ah;
  IByHashTxGettable* p = dynamic_cast<IByHashTxGettable*>(&ah);

  hash256 h = {};
  BOOST_CHECK(not p->getTxByHash(h));
}

BOOST_AUTO_TEST_CASE(test_mock_IByHashTxGettable_yes){
  mockedHashTxGetter::B ah;
  IByHashTxGettable* p = dynamic_cast<IByHashTxGettable*>(&ah);

  hash256 h = ah.tx.hash();

  auto r = p->getTxByHash(h);
  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(r.value().hash(),h);
}

BOOST_AUTO_TEST_CASE(test_getTxByHash_from_mempool){
  Mempool ph;                   // pool host
  IByHashTxGettable* p = dynamic_cast<IByHashTxGettable*>(&ph);

  auto [a1,a2,data] = get_example_address_and_data();
  Tx t(a1,a2,data,123/*nonce*/);
  ph.addTx(t);       // put a Tx

  BOOST_CHECK_EQUAL(ph.txs.size(),1);
  BOOST_CHECK(not p->getTxByHash({})); // non-existing hash
  BOOST_CHECK_EQUAL(ph.txs.size(),1);

  auto r = p->getTxByHash(t.hash());
  BOOST_CHECK(r);
  BOOST_CHECK_EQUAL(ph.txs.size(),0);
  BOOST_CHECK_EQUAL(r.value().nonce,t.nonce);
  BOOST_CHECK_EQUAL(r.value().from,a1);
  BOOST_CHECK_EQUAL(r.value().hash(),t.hash());
}

/*
  🦜: Here we initialize the mempool with a set of hashes
 */
BOOST_AUTO_TEST_CASE(test_given_previous_hashes_basic_verify){
  auto [a1,a2,data] = get_example_address_and_data();
  vector<Tx> txs = {
    Tx(a1,a2,data,1/*nonce*/),
    Tx(a1,a2,data,2),
    Tx(a2,a1,data,1),// same nonce, but different from
    Tx(a1,a2,data,3),
    Tx(a1,a2,data,4)
  };

  Mempool p(
            unique_ptr<unordered_set<hash256>>(new unordered_set<hash256>({txs[0].hash(),txs[1].hash()}))
            ); // these two are executed previously
  // 🦜 Yep, we kinda have to initialize it this way..

  BOOST_CHECK(not p.verifyTx(txs[0]));
  BOOST_CHECK(not p.verifyTx(txs[1]));
  BOOST_CHECK(p.verifyTx(txs[2]));
  BOOST_CHECK(p.verifyTx(txs[3]));
}

vector<Tx> get_example_txs(){
  auto [a1,a2,data] = get_example_address_and_data();
  vector<Tx> txs = {
    Tx(a1,a2,data,1/*nonce*/),
    Tx(a1,a2,data,2),
    Tx(a2,a1,data,1),// same nonce, but different from
    Tx(a1,a2,data,3),
    Tx(a1,a2,data,4)
  };
  return txs;
}

BOOST_AUTO_TEST_CASE(test_given_previous_hashes_verify){
  vector<Tx> txs = get_example_txs();
  Mempool p(
            unique_ptr<unordered_set<hash256>>(new unordered_set<hash256>({txs[0].hash(),txs[1].hash()}))
            ); // these two are executed previously

  BOOST_CHECK(not p.verifyTx(txs[0]));
  BOOST_CHECK(not p.verifyTx(txs[1]));
  BOOST_CHECK(p.verifyTx(txs[2]));
  BOOST_CHECK(p.verifyTx(txs[3]));
}


BOOST_AUTO_TEST_CASE(test_given_previous_hashes_add_verify){
  vector<Tx> txs = get_example_txs();
  Mempool p(
            unique_ptr<unordered_set<hash256>>(new unordered_set<hash256>({txs[0].hash(),txs[1].hash()}))
            ); // these two are executed previously

  // 🦜 : verifyTx() should mean: "Whether the tx can be added"
  for (const Tx & tx : txs){
    if (p.verifyTx(tx))
      BOOST_CHECK(p.addTx(tx));
    else
      BOOST_CHECK(not p.addTx(tx));
  }
}

BOOST_AUTO_TEST_CASE(test_given_previous_hashes_pop_verify){
  vector<Tx> txs = get_example_txs();
  Mempool p(
            unique_ptr<unordered_set<hash256>>(new unordered_set<hash256>({txs[0].hash(),txs[1].hash()}))
            ); // these two are executed previously


  // add all txs
  for (const Tx & tx : txs)
    p.addTx(tx);

  // pop
  BOOST_CHECK_EQUAL(p.txs.size(),3);
  BOOST_CHECK(not p.getTxByHash(txs[0].hash())); // hash doesn't exsit
  BOOST_CHECK_EQUAL(p.txs.size(),3);
  BOOST_CHECK(p.getTxByHash(txs[2].hash())); // this exists
  BOOST_CHECK_EQUAL(p.txs.size(),2);       // popped
  BOOST_CHECK(not p.verifyTx(txs[2]));     // but you can't add it back again.
  BOOST_CHECK(not p.addTx(txs[2]));
}

BOOST_AUTO_TEST_CASE(test_mempool_info){
  vector<Tx> txs = get_example_txs();
  Mempool p(
            unique_ptr<unordered_set<hash256>>(new unordered_set<hash256>({txs[0].hash(),txs[1].hash()}))
            ); // these two are executed previously

  // add all txs
  for (const Tx & tx : txs)
    p.addTx(tx);

  string s = p.info();

  json::error_code ec;
  json::value jv = json::parse(s, ec );
  BOOST_TEST_MESSAGE((format("Formed info %s") % s).str());
  BOOST_REQUIRE(not ec);
  }

BOOST_AUTO_TEST_SUITE_END();
