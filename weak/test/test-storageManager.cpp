#include "h.hpp"

#include "storageManager.hpp"
#include "mock.hpp"
#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <thread>

using namespace weak;

BOOST_AUTO_TEST_SUITE(test_acn);
BOOST_AUTO_TEST_CASE(hi){
  BOOST_CHECK(true);
}

// AUTO TEST suite cannot be nested??
// BOOST_AUTO_TEST_SUITE(Acn);
// BOOST_AUTO_TEST_SUITE_END();
// {{{ Acn tests

BOOST_AUTO_TEST_CASE(acn_basic){
  Acn a{123,
        evmc::from_hex("0000aabb").value()};

  BOOST_CHECK(a.storage.empty());

  BOOST_CHECK_EQUAL(a.nonce,123);
  BOOST_CHECK_EQUAL(a.code[0],0x0);
  BOOST_CHECK_EQUAL(a.code[1],0x0);
  BOOST_CHECK_EQUAL(a.code[2],0xaa);
  BOOST_CHECK_EQUAL(a.code[3],0xbb);
}

BOOST_AUTO_TEST_CASE(acn_codehash){
  // different code gives different hash
  Acn a{123,evmc::from_hex("0011").value()};
  Acn a1{123,evmc::from_hex("0022").value()};
  Acn a2{234,evmc::from_hex("0011").value()};

  BOOST_CHECK_NE(hashToString(a.codehash()),
                 hashToString(a1.codehash()));
  // different nonce dosen't matter
  BOOST_CHECK_EQUAL(hashToString(a.codehash()),
                    hashToString(a2.codehash())
                    );
}

BOOST_AUTO_TEST_CASE(acn_toJson_noStorage){
  Acn a{123,evmc::from_hex("0011").value()};

  json::object v = a.toJson().as_object();
  BOOST_CHECK_EQUAL(v["nonce"].as_uint64(),123);
  BOOST_CHECK_EQUAL(v["code"].as_string(),"0011");
  BOOST_CHECK_EQUAL(v["codehash"].as_string(), hashToString(a.codehash()));
  BOOST_CHECK_EQUAL(v["storage"].as_object().size(),0);
  // ^^^ throw is not an object
}

BOOST_AUTO_TEST_CASE(acn_toJson_withStorage){
  Acn a{123,evmc::from_hex("0011").value()};

  // Save a kv
  // call bytes32(uint64_t)
  a.storage[bytes32(0x1)] = bytes32{0x10};

  json::object v = a.toJson().as_object();
  BOOST_CHECK_EQUAL(v["storage"].as_object().size(),1);

  // Check the storage values
  vector<string> ks = {"\x01"};
  vector<string> vs = {"\x10"};
  for (const json::key_value_pair & kv : v.at("storage").as_object()){
    string s_key = kv.key();
    string s_val = value_to<string>(kv.value());

    BOOST_CHECK_EQUAL(ks.back(),s_key);
    BOOST_CHECK_EQUAL(vs.back(),s_val);
    ks.pop_back(); vs.pop_back();
}

}

BOOST_AUTO_TEST_CASE(acn_toJson_withMultiStorage ){
  Acn a{123,evmc::from_hex("0011").value()};

  // Save two kv
  // call bytes32(uint64_t)
  a.storage[bytes32(0x1)] = bytes32{0x10};
  a.storage[bytes32(0x2)] = bytes32{0x20};


  json::object v = a.toJson().as_object();
  BOOST_REQUIRE_EQUAL(v["storage"].as_object().size(),2);

  // Check the storage values
  // 🦜 : Note that the order is not graranteed.
  vector<string> ks = {"\x01","\x02"};
  vector<string> vs = {"\x10","\x20"};
  unordered_map<string,string> m = {
    {"\x01", "\x10"},
    {"\x02", "\x20"},
  };
  for (const json::key_value_pair & kv : v.at("storage").as_object()){
    string s_key = kv.key();
    string s_val = value_to<string>(kv.value());

    BOOST_REQUIRE(m.contains(s_key));
    BOOST_CHECK_EQUAL(m[s_key],s_val);
    m.erase(s_key);
  }
}

BOOST_AUTO_TEST_CASE(acn_fromJson_noStorage){
  Acn a;
  json::object  v;

  v["nonce"] = 123;
  v["code"] = "0011";
  // evmc::from_hex() will place these at the end of hex

  v["storage"] = json::object{};

  BOOST_REQUIRE(a.fromJson(v));
  BOOST_CHECK_EQUAL(a.nonce,123);
  BOOST_CHECK_EQUAL(evmc::hex(a.code),"0011");
  BOOST_CHECK(a.storage.empty());
  }

BOOST_AUTO_TEST_CASE(acn_fromJson_withStorage ){
  Acn a;
  json::object  v;


  v["nonce"] = 123;
  v["code"] = "0011";
  // evmc::from_hex() will place these at the end of hex
  // store value
  v["storage"] = json::object{
    {"\x01", "\x10"}
  };

  BOOST_REQUIRE(a.fromJson(v));
  BOOST_CHECK_EQUAL(a.nonce,123);
  BOOST_CHECK_EQUAL(evmc::hex(a.code),"0011");
  BOOST_CHECK_EQUAL(a.storage.size(),1);
  BOOST_CHECK_EQUAL(evmc::hex(a.storage[bytes32(0x1)]),
                    // implicilty convert from bytes32 -> bytes_view
                    string(62/*32x2 - 2*/,'0') + "10");

}

BOOST_AUTO_TEST_CASE(acn_fromJson_withMultiStorage){
  Acn a;
  json::object  v;

  v["nonce"] = 123;
  v["code"] = "0011";
  // store value
  v["storage"] = json::object{
    {"\x01", "\x10"},
    {"\x02", "\x20"}
  };

  BOOST_REQUIRE(a.fromJson(v));
  BOOST_CHECK_EQUAL(a.nonce,123);
  BOOST_CHECK_EQUAL(evmc::hex(a.code),"0011");
  BOOST_CHECK_EQUAL(a.storage.size(),2);
  BOOST_CHECK_EQUAL(evmc::hex(a.storage[bytes32(0x1)]),
                    // implicilty convert from bytes32 -> bytes_view
                    string(62/*32x2 - 2*/,'0') + "10");
  BOOST_CHECK_EQUAL(evmc::hex(a.storage[bytes32(0x2)]),
                    // implicilty convert from bytes32 -> bytes_view
                    string(62/*32x2 - 2*/,'0') + "20");

}

// }}}
// {{{ IAcnGettable


BOOST_AUTO_TEST_CASE(IAcnGettable){
  // In unique_ptr, we have to specify the full class name
  // So the weak:: prefix is necessary.
  // unique_ptr<IAcnGettable> p(new A());
  unique_ptr<weak::IAcnGettable> p(new mockedAcnPrv::A());
  unique_ptr<weak::IAcnGettable> p2(new mockedAcnPrv::B());
  unique_ptr<weak::IAcnGettable> p3(new mockedAcnPrv::C());



  BOOST_CHECK(!p->getAcn({}));
  BOOST_CHECK(p2->getAcn({}));
  Acn a = p2->getAcn({}).value();
  BOOST_CHECK_EQUAL(a.nonce,123);
  BOOST_CHECK(!p3->getAcn(makeAddress(2)));
  BOOST_CHECK(p3->getAcn(makeAddress(1)));
  Acn a3 = p3->getAcn(makeAddress(1)).value();
  BOOST_CHECK_EQUAL(a3.nonce,123);
  }

// }}}

BOOST_AUTO_TEST_SUITE_END(); // test-acn

BOOST_AUTO_TEST_SUITE(test_WorldStorage);
BOOST_AUTO_TEST_CASE(hi){
  BOOST_CHECK(true);
}

namespace mockedWorldState {
  // impl that based on two map
  class A: public virtual IWorldChainStateSettable {
  public:
    unordered_map<string,string> m1 /*txinfo db*/, m2 /*state*/;
    bool setInChainDB(const string k/*txHash*/, const string v/*txOnBlkInfo*/) noexcept override{
      m1[k] = v;
      return true;
    };
    optional<string> getFromChainDB(const string k/*txHash*/) const noexcept override{
      // return m1[k];
      if (not m1.contains(k)) return {};
      return m1.find(k)->second;
    };

    bool applyJournalStateDB(const vector<StateChange> & j) noexcept override{
      // 🦜: In practice, here should be a batch
      for (const StateChange & i : j){
        // bool ok;
        if (i.del){
          BOOST_LOG_TRIVIAL(info) << format("Deleting k=%s") % i.k;
          if (m2.contains(i.k)) m2.erase(i.k);
        }else{
          BOOST_LOG_TRIVIAL(debug) << format("Adding (k,v) = (%s,%s)") % i.k % i.v;
          m2[i.k] = i.v;
        }
      }
      return true;
    };
  };

}

BOOST_AUTO_TEST_CASE(testMockedWorldState_apply_jounal){
  mockedWorldState::A a;
  unique_ptr<weak::IWorldChainStateSettable> p(&a);

  vector<StateChange> j = {
    {false, "k1", "v1"},
    {false, "k2", "v2"},
    {false, "k3", "v3"},
    {true, "k3", ""}
  };

  p->applyJournalStateDB(j);
  BOOST_CHECK_EQUAL(a.m2.size(),2);
}


BOOST_AUTO_TEST_CASE(testMockedWorldState_get_set){
  mockedWorldState::A a;
  unique_ptr<weak::IWorldChainStateSettable> p(&a);

  bool ok = p->setInChainDB("k1","v1");
  string v = p->getFromChainDB("k1").value();
  BOOST_CHECK(ok);
  BOOST_CHECK_EQUAL(v,"v1");
}

BOOST_AUTO_TEST_CASE(WorldStorage_ctor){
  WorldStorage s;               // init at current dir
  filesystem::path p = filesystem::current_path();
  BOOST_REQUIRE(filesystem::exists(p / "chainDB"));
  BOOST_REQUIRE(filesystem::exists(p / "stateDB"));

  // remove the data
  BOOST_CHECK(filesystem::remove_all(p / "chainDB"));
  BOOST_CHECK(filesystem::remove_all(p / "stateDB"));
}


BOOST_AUTO_TEST_CASE(WorldStorage_ctor_at_other_dir){
  filesystem::path p = filesystem::temp_directory_path();
  WorldStorage s{p};               // init at tmp dir
  BOOST_REQUIRE(filesystem::exists(p/ "chainDB"));
  BOOST_REQUIRE(filesystem::exists(p / "stateDB"));

  // remove the data
  BOOST_CHECK(filesystem::remove_all(p / "chainDB"));
  BOOST_CHECK(filesystem::remove_all(p / "stateDB"));
}

struct TmpWorldStorage{
  filesystem::path p{filesystem::temp_directory_path()};

  unique_ptr<WorldStorage> w{new WorldStorage(p)};
  TmpWorldStorage(){
    BOOST_TEST_MESSAGE("Setting up fixture WorldStorage");

    // the data should be there now.
    BOOST_REQUIRE(filesystem::exists(p / "chainDB"));
    BOOST_REQUIRE(filesystem::exists(p / "stateDB"));
  };
  ~TmpWorldStorage(){
    BOOST_TEST_MESSAGE("Tearing down fixture WorldStorage");
    // Manually call the dtor
    /* 🦜: This is necessary, because you have to `delete db` before removing
       the associated folder. Otherwise you'll get a Segementation fault error.
    */
    delete w.release();  // released the managed obj

    // remove the data
    BOOST_CHECK(filesystem::remove_all(p / "chainDB"));
    BOOST_CHECK(filesystem::remove_all(p / "stateDB"));
    BOOST_REQUIRE(not filesystem::exists(p / "chainDB"));
    BOOST_REQUIRE(not filesystem::exists(p / "stateDB"));
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
  rocksdb::Status s = w->stateDB->Get(rocksdb::ReadOptions(),"k1",&v);
  BOOST_REQUIRE(not s.IsNotFound());
  BOOST_CHECK_EQUAL(v,"v1");

  // k2, v2
  s = w->stateDB->Get(rocksdb::ReadOptions(),"k2",&v);
  BOOST_REQUIRE(not s.IsNotFound());
  BOOST_CHECK_EQUAL(v,"v2");

  // k3 should not be found
  s = w->stateDB->Get(rocksdb::ReadOptions(),"k3",&v);
  BOOST_REQUIRE(s.IsNotFound());
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


BOOST_FIXTURE_TEST_CASE(test_get_nonexisting_Acn,TmpWorldStorage){
  // get the Acn
  BOOST_REQUIRE(not w->getAcn(makeAddress(123)));
}


BOOST_FIXTURE_TEST_CASE(TPS_test_storage, TmpWorldStorage, MY_TEST_THIS){

  using namespace std::chrono;
  time_point<high_resolution_clock> start, end;
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);

  // 1. --------------------------------------------------
  // Make a Blk
  address a1 = makeAddress(10); address a2 = makeAddress(20);

  const int N = 15;             // num tx per blk
  const int M = 4;             // num of thread

  vector<Tx> txs;
  for (int i{0};i<N;i++)
    txs.push_back(Tx(a1,a2,{},i/*nonce*/));

  // parent hash
  hash256 h;
  std::fill(std::begin(h.bytes),std::end(h.bytes),0x00);
  // make block
  Blk b = Blk(1,h,txs);

  // 🦜 : Do we need to count in the speed of serilization?
  // 🐢 : No
  string b_str = b.toString();

  // keys
  string keys[M];
  for(int i=0;i<M;i++)
    keys[i] = (format("k%d") % i).str();

  // 2. --------------------------------------------------
  // Time how fast does it store
  start = high_resolution_clock::now(); // --------------------------------------------------
  for (int i{0};i<M;i++)
    std::jthread{[&](){
      w->setInChainDB(keys[i],b_str);
    }};
  // Get timming
  end = high_resolution_clock::now();    // --------------------------------------------------

  duration<double> elapsed_seconds = end - start;
  double s = elapsed_seconds.count();
  double tps = M * N/s;
  BOOST_TEST_MESSAGE(format("sec elapsed: %4.2e sec, so TPS = %.0f") % s % (tps));
  BOOST_CHECK(tps > 40'000);
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

BOOST_AUTO_TEST_SUITE_END(); // WorldStorage
