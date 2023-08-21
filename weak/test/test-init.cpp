#include "h.hpp"

#include "init/init.hpp"

using namespace weak;
BOOST_AUTO_TEST_SUITE(test_init_helpers,MY_TEST_THIS);

void test_ok_hash(const string &hex){
  hash256 h = get_hash_from_key("/tx/" + hex);
  auto h1 = evmc::from_hex<hash256>(hex);
  BOOST_REQUIRE(h1);
  BOOST_CHECK_EQUAL(h,h1.value());
}

void test_bad_key(const string & key){
  BOOST_CHECK_THROW(
                    {
                      hash256 h = get_hash_from_key(key);
                    }
                    ,std::runtime_error);
}

void test_bad_hash(const string &hex){
  test_bad_key("/tx/" + hex);
}

BOOST_AUTO_TEST_CASE(test_get_hash_from_key_ok){
  // string hex(64,'0');
  test_ok_hash(string(64,'0'));
  test_ok_hash(string(64,'1'));
  test_ok_hash(string(64,'f'));
}

BOOST_AUTO_TEST_CASE(test_get_hash_from_key_bad_hex){
  test_bad_hash(string(65,'0'));
  test_bad_hash(string(63,'0'));
  test_bad_hash(string(64,'x'));
}

BOOST_AUTO_TEST_CASE(test_get_hash_from_key_bad_key){
  test_bad_key("abc");
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

tuple<ExecBlk,vector<Tx>> prepare_ExecBlk(){
  // 1. --------------------------------------------------
  // Make execBlk
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

  return make_tuple(ExecBlk{b0,j,r},txs);
}
namespace mockedChainDBGettable2{
  // the empty one
  class A : public virtual IChainDBGettable2{
    optional<string> getFromChainDB(const string k) const noexcept override{
      return {};
    }
    vector<string> getKeysStartWith(string_view prefix) const override{
      return {};
    }
  };

  /* 🦜 : This one can provide the Blk 1*/
  class B : public virtual IChainDBGettable2{
  public:
    ExecBlk b;
    vector<Tx> txs;
    B(){
      std::tie(b,txs) = prepare_ExecBlk();
    }
    optional<string> getFromChainDB(const string k) const noexcept override{
      if (k == "/blk/1"){
        return this->b.toString();
      }
      return {};
    }
    vector<string> getKeysStartWith(string_view prefix) const override{
      return {};
    }
  };

  /* 🦜 : This one can provide the Blk 1 and also the provided keys*/
  class C : public virtual IChainDBGettable2{
  public:
    ExecBlk b;
    vector<Tx> txs;
    vector<string> ks;
    C(vector<string> kks): ks(kks){
      std::tie(b,txs) = prepare_ExecBlk();
    }
    optional<string> getFromChainDB(const string k) const noexcept override{
      if (k == "/blk/1"){
        return this->b.toString();
      }
      return {};
    }
    vector<string> getKeysStartWith(string_view prefix) const override{
      return this->ks;
    }
  };
}


BOOST_AUTO_TEST_CASE(test_get_required_things_from_ChainDB_ok){
  /*
    🦜 : How to set this up?

    🐢 : We need to
    1. make a IChainDBGettable2*
         
    2. put the corresponding '/blk/<blk_number>' in it (no need to put the whole chain).
         
    3. Finally, stores a series of '/tx/<hash>' in it. (🦜 : Oh, in fact,
    we don't need to actually store these,we just need to let the
    getKeysStartWith("/tx/") returns a series of '/tx/<hash>'.)
  */

  mockedChainDBGettable2::B wh1; // B provides Blk1 only
  IChainDBGettable2 * w1 = dynamic_cast<IChainDBGettable2*>(&wh1);
  auto [n,h,vh] = get_required_things_from_ChainDB(w1,"1");
  BOOST_CHECK_EQUAL(*n,1);      // Got it
  BOOST_CHECK_EQUAL(*h,wh1.b.hash); // the Blk hash
  BOOST_REQUIRE(vh->empty());
}

template<typename T>
void veq(vector<T> v, vector<T> v2){
  BOOST_REQUIRE_EQUAL(v.size(),v2.size());
  for (int i=0;i<v.size();i++) \
    BOOST_CHECK_EQUAL(v[i],v2[i]);
};

template<typename T>
void seq(const unordered_set<T> & s, const unordered_set<T> & s1){
  BOOST_REQUIRE_EQUAL(s.size(),s1.size());
  for (auto x : s){
    BOOST_REQUIRE(s1.contains(x));
  }
}

BOOST_AUTO_TEST_CASE(test_get_required_things_from_ChainDB_ok_and_get_some_hashes){
  string hs1(64,'a'), hs2(64,'b');
  mockedChainDBGettable2::C wh1({
      "/tx/" + hs1,
      "/tx/" + hs2
    }); // B provides Blk1 and supplied keys

  IChainDBGettable2 * w1 = dynamic_cast<IChainDBGettable2*>(&wh1);
  auto [n,h,vh] = get_required_things_from_ChainDB(w1,"1");
  BOOST_CHECK_EQUAL(*n,1);      // Got it
  BOOST_CHECK_EQUAL(*h,wh1.b.hash); // the Blk hash
  seq(*vh,
      {
        evmc::from_hex<hash256>(hs1).value(),
        evmc::from_hex<hash256>(hs2).value(),
      }
      );
  BOOST_REQUIRE(not vh->empty());
  BOOST_CHECK_NE(vh->size(),3); // 🦜 : It's just those two right
}

BOOST_AUTO_TEST_CASE(test_get_required_things_from_ChainDB_bad){
  mockedChainDBGettable2::A wh; // A doesn't provide anything
  IChainDBGettable2 * w = dynamic_cast<IChainDBGettable2*>(&wh);
  BOOST_CHECK_THROW({get_required_things_from_ChainDB(w,"1");},std::runtime_error);
  BOOST_CHECK_THROW({get_required_things_from_ChainDB(w,"2");},std::runtime_error);

  mockedChainDBGettable2::B wh1; // B provides Blk1 only
  IChainDBGettable2 * w1 = dynamic_cast<IChainDBGettable2*>(&wh1);
  BOOST_CHECK_THROW({get_required_things_from_ChainDB(w1,"2");},std::runtime_error);

}

BOOST_AUTO_TEST_CASE(test_prepare_endpoint_list){
  vector<string> v{"a1:123","a2:456"};
  vector<string> r = prepare_endpoint_list(v);
  BOOST_CHECK_EQUAL(r.size(),2);

  // now we can extract the part. They will be sorted.
  optional<string> r0;
  for (int i =0; i<v.size() ;i++){
    r0 = ::pure::NetAsstn::extract_addr_and_port_from_endpoint(r[i]);
    BOOST_REQUIRE(r0);
    BOOST_REQUIRE(::pure::contains(v,r0.value()));
    BOOST_TEST_MESSAGE("Got value: " + r0.value());
  }
}

BOOST_AUTO_TEST_CASE(test_prepare_endpoint_list_nonstandard_format){
  // 🦜 : This function won't check wether each of your item is in the form
  // "<host>:<port>".
  vector<string> v{"aaa","bbb"};
  vector<string> r = prepare_endpoint_list(v);
  BOOST_CHECK_EQUAL(r.size(),2);

  // now we can extract the part. They will be sorted.
  optional<string> r0;
  for (int i =0; i<v.size() ;i++){
    r0 = ::pure::NetAsstn::extract_addr_and_port_from_endpoint(r[i]);
    BOOST_REQUIRE(r0);
    BOOST_REQUIRE(::pure::contains(v,r0.value()));
    BOOST_TEST_MESSAGE("Got value: " + r0.value());
  }
}

BOOST_AUTO_TEST_CASE(test_prepare_endpoint_list_bad_nonunique){
  vector<string> v{"aaa","bbb","aaa"};
  BOOST_CHECK_THROW(
                    {
                      vector<string> r = prepare_endpoint_list(v);
                    }
                    ,std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END();
