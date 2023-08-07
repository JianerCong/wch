/**
 * @file test-inRamWrld.cpp
 * @brief Test the inRamWrldStorage.
 *
 * 🦜 : This file basically checks wether the in RAMstorage can simulate the
 * real rocksdb. Most of the code here is copied directly from `test-storageManager.cpp`
 */
#include "h.hpp"

#include "storageManager.hpp"
#include "mock.hpp"
#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <thread>

using namespace weak;

BOOST_AUTO_TEST_CASE(test_basic){
  BOOST_CHECK_EQUAL(1,1);
}

struct TmpWorldStorage{
  unique_ptr<InRamWorldStorage> w{new InRamWorldStorage()};
  TmpWorldStorage(){
    BOOST_TEST_MESSAGE("Setting up fixture WorldStorage");
  };
  ~TmpWorldStorage(){
    BOOST_TEST_MESSAGE("Tearing down fixture WorldStorage");
  }
};

BOOST_FIXTURE_TEST_CASE(get_set_chainDB, TmpWorldStorage){

  BOOST_CHECK(w->setInChainDB("k1","v1"));
  BOOST_CHECK(w->setInChainDB("k2","v2"));
  BOOST_CHECK_EQUAL(w->getFromChainDB("k1").value(),"v1");
  BOOST_CHECK(not w->getFromChainDB("k3"));
}

BOOST_FIXTURE_TEST_CASE(test_applyJournal, TmpWorldStorage){
  vector<StateChange> j = {
    {false, "k1", "v1"},
    {false, "k2", "v2"},
    {false, "k3", "v3"},
    {true, "k3", ""}
  };/*Journal*/

  BOOST_REQUIRE(w->applyJournalStateDB(j));

  string v;
  // k1, v1
  BOOST_CHECK_EQUAL(w->stateDB["k1"],"v1");
  BOOST_CHECK_EQUAL(w->stateDB["k2"],"v2");
  BOOST_REQUIRE(not w->stateDB.contains("k3"));
}

BOOST_FIXTURE_TEST_CASE(test_get_nonexisting_Acn,TmpWorldStorage){
  // get the Acn
  BOOST_REQUIRE(not w->getAcn(makeAddress(123)));
}

BOOST_FIXTURE_TEST_CASE(test_getAcn,TmpWorldStorage){
  // init an Acn
  Acn a{123,
        evmc::from_hex("0000aabb").value()};

  // put the Acn
  evmc::address addr = makeAddress(1);
  string k = addressToString(addr), v = a.toString();
  vector<StateChange> j = {
    {false /*del*/ , k,v},
  };/*Journal*/
  BOOST_REQUIRE(w->applyJournalStateDB(j));

  // get the Acn
  BOOST_REQUIRE(w->getAcn(addr));
  // check the Acn
  a = w->getAcn(addr).value();

  BOOST_CHECK_EQUAL(a.nonce,123);
  BOOST_CHECK_EQUAL(a.code[0],0x0);
  BOOST_CHECK_EQUAL(a.code[1],0x0);
  BOOST_CHECK_EQUAL(a.code[2],0xaa);
  BOOST_CHECK_EQUAL(a.code[3],0xbb);

}

template<typename T>
void veq(vector<T> v, vector<T> v2){
  for (int i=0;i<v.size();i++) \
    BOOST_CHECK_EQUAL(v[i],v2[i]);
};
BOOST_FIXTURE_TEST_CASE(test_prefix, TmpWorldStorage){
  /*
    🦜 : Here we test wether out implementation about IChainDBPrefixKeyGettable works.
  */
  // save some keys
  BOOST_REQUIRE(w->setInChainDB("kkkk1","abc"));
  BOOST_REQUIRE(w->setInChainDB("kkkk2","abc"));
  BOOST_REQUIRE(w->setInChainDB("kkkk3","abc"));
  BOOST_REQUIRE(w->setInChainDB("aaaa1","abc"));
  BOOST_REQUIRE(w->setInChainDB("aaaa2","abc"));
  BOOST_REQUIRE(w->setInChainDB("aaaa3","abc"));

  vector<string> ks = w->getKeysStartWith("kkkk");
  veq(ks, {"kkkk1", "kkkk2", "kkkk3"});
  ks = w->getKeysStartWith("aaaa");
  veq(ks, {"aaaa1", "aaaa2", "aaaa3"});

  ks = w->getKeysStartWith("bad");
  BOOST_REQUIRE(ks.empty());
  // get keys


}
