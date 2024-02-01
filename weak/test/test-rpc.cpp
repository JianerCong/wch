#include "h.hpp"
#include "rpc.hpp"
#include <fstream>
#include "execManager.hpp"
#include "mock.hpp"

#include "net/pure-weakAsyncHttpServer.hpp"

#include <filesystem>
namespace filesystem = std::filesystem;
using std::filesystem::path;
using std::filesystem::current_path;

using namespace weak;

/**
 * @brief Read all the string from a file.
 *
 * @param p the path to the file
 *
 * @return the file content.
 */
string read_all_string(path & p){
  std::ifstream i(p);
  BOOST_REQUIRE(filesystem::exists(p));
  BOOST_CHECK(i.is_open());
  string s; char c;
  while (i.get(c)) s+=c;
  i.close();
  return s;
}

namespace mockedRpcNetworkable{

  // the empty server
  class A : public virtual IForRpcNetworkable{
  public:
    bool listenToPost(string target,postHandler_t h)noexcept override{
      return true;
    }
    bool listenToGet(string target,getHandler_t h)noexcept override{
      return true;
    }
  };

  // The server that keeps the records
  class B : public virtual IForRpcNetworkable{
  public:
    unordered_map<string,postHandler_t> postMap;
    unordered_map<string,getHandler_t> getMap;
    bool listenToPost(string target,postHandler_t h)noexcept override{
      return this->postMap.insert({target,h}).second;
    }
    bool listenToGet(string target,getHandler_t h)noexcept override{
      return this->getMap.insert({target,h}).second;
    }
  };
}

BOOST_AUTO_TEST_CASE(test_empty_server){
  mockedRpcNetworkable::A nh;   // network host
  IForRpcNetworkable * n = dynamic_cast<IForRpcNetworkable*>(&nh);

  BOOST_CHECK(n->listenToGet("",{}));
  BOOST_CHECK(n->listenToPost("",{}));
}

using om_t = optional<unordered_map<string,string>>;

BOOST_AUTO_TEST_CASE(test_recorded_server){
  mockedRpcNetworkable::B nh;   // network host
  IForRpcNetworkable * n = dynamic_cast<IForRpcNetworkable*>(&nh);

  BOOST_CHECK(n->listenToGet("/aaa",[](om_t r)->tuple<bool,string>{
        return make_tuple(true,"aaa");
      }));

  BOOST_CHECK(n->listenToPost("/bbb",[](string_view d)->tuple<bool,string>{
        return make_tuple(true,"bbb");
      }));

  BOOST_CHECK_EQUAL(nh.getMap.size(),1);
  BOOST_CHECK_EQUAL(nh.postMap.size(),1);

  // call the handlers
  auto [ok,res] = nh.getMap["/aaa"]({});
  BOOST_CHECK(ok);
  BOOST_CHECK_EQUAL(res,"aaa");

  std::tie(ok,res) = nh.postMap["/bbb"]({});
  BOOST_CHECK(ok);
  BOOST_CHECK_EQUAL(res,"bbb");
}

BOOST_AUTO_TEST_SUITE(test_Rpc_handlers);

BOOST_AUTO_TEST_CASE(test_status){
  // 🦜 : The status is like a PING. It doesn't check any thing, so we can
  // safely initialize it with nothing.
  mockedRpcNetworkable::B nh;   // network host
  IForRpcNetworkable * n = dynamic_cast<IForRpcNetworkable*>(&nh);
  Rpc rpc{n,nullptr};

  auto [ok, data] = nh.getMap["/get_node_status"]({});
  BOOST_TEST_MESSAGE((format("Got status >>%s<<") % data).str());
  BOOST_REQUIRE(ok);
}

BOOST_AUTO_TEST_SUITE_END();

void sleep_for(int i=1){
  std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
}

// #include "net/pure-weakHttpServer.hpp"
#include "net/pure-weakHttpClient.hpp"

BOOST_AUTO_TEST_SUITE(test_http_assnt);
BOOST_AUTO_TEST_CASE(test_handle_get){
  ::pure::WeakAsyncTcpHttpServer srv;   // listen 7777
  sleep_for(1);
  HttpRpcAsstn nh{dynamic_cast<::pure::IHttpServable*>(&srv)};
  IForRpcNetworkable * n = dynamic_cast<IForRpcNetworkable*>(&nh);

  n->listenToGet("/aaa",[](om_t o) -> tuple<bool,string> {
      return make_tuple(true,"aaa");
    });

  BOOST_CHECK_EQUAL(srv.getLisnMap.size(),1);
  auto r = ::pure::weakHttpClient::get("localhost","/aaa",7777);
  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(r.value(),"aaa");
} // srv stopped here

BOOST_AUTO_TEST_CASE(test_handle_post){
  constexpr uint16_t PORT = 7778;
  ::pure::WeakAsyncTcpHttpServer srv{PORT};
  sleep_for(1);
  HttpRpcAsstn nh{dynamic_cast<::pure::IHttpServable*>(&srv)};
  IForRpcNetworkable * n = dynamic_cast<IForRpcNetworkable*>(&nh);

  n->listenToPost("/bbb",[](string_view  d) -> tuple<bool,string> {
      return make_tuple(true,"bbb: " + string(d));
    });

  BOOST_CHECK_EQUAL(srv.postLisnMap.size(),1);
  auto r = ::pure::weakHttpClient::post("localhost","/bbb",PORT,"[data]");
  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(r.value(),"bbb: [data]");
} // srv stopped here
BOOST_AUTO_TEST_SUITE_END();

BOOST_AUTO_TEST_SUITE(test_client_request_parse);

tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
}

BOOST_AUTO_TEST_CASE(test_parse_client_txs_json_ok){
  auto [a1,a2,data] = get_example_address_and_data();
  json::value t1 = {
    {"from", addressToString(a1)},
    {"to", addressToString(a2)},
    {"data",evmc::hex(data)},
    {"nonce",123}
  };
  json::value t2 = {
    {"from", addressToString(a1)},
    {"to", ""},
    {"data",evmc::hex(data)},
    {"nonce",123}
  };

  json::array a{t1,t2};

  // +  --------------------------------------------------
  // parse it
  auto r = Tx::parse_txs_json_for_rpc(move(a));
  BOOST_REQUIRE(r);
  auto [s,txs] = r.value();

  BOOST_TEST_MESSAGE("Got >>>"+s+"<<<");
  BOOST_CHECK_EQUAL(txs.size(),2);
  BOOST_CHECK_EQUAL(txs[0].from,a1);
  BOOST_CHECK_EQUAL(txs[1].from,a1);
  BOOST_CHECK_EQUAL(txs[0].to,a2);
  BOOST_CHECK_EQUAL(txs[1].to,makeAddress(0));
  BOOST_CHECK_EQUAL(txs[0].data,data);
  BOOST_CHECK_EQUAL(txs[1].data,data);
  BOOST_CHECK_EQUAL(txs[0].nonce,123);
  BOOST_CHECK_EQUAL(txs[1].nonce,123);

  // The string to be returned to client.
  json::error_code ec;
  json::value jv = json::parse(s,ec);
  BOOST_REQUIRE(not ec);
  json::array o = jv.as_array();
  BOOST_CHECK_EQUAL(o.size(),2);
  // +  --------------------------------------------------
  // get the results to client

  // the result in case of a CALL
  json::object o1 = o[0].as_object();
  BOOST_CHECK(o1["hash"].is_string());
  BOOST_CHECK_EQUAL(o1.size(),1);

  // the result in case of a CREATE
  o1 = o[1].as_object();
  BOOST_CHECK(o1["hash"].is_string());
  BOOST_CHECK(o1["deployed_address"].is_string());
  BOOST_CHECK_EQUAL(o1.size(),2);
}


BOOST_AUTO_TEST_CASE(test_parse_client_txs_json_bad){
  auto [a1,a2,data] = get_example_address_and_data();
  json::value t1 = {
    {"from", "invalidhash"},
    {"to", addressToString(a2)},
    {"data",evmc::hex(data)},
    {"nonce",123}
  };
  json::value t2 = {
    {"from", addressToString(a1)},
    {"to", ""},
    {"data",evmc::hex(data)},
    {"nonce",123}
  };

  json::array a{t1,t2};

  // +  --------------------------------------------------
  // parse it
  BOOST_CHECK_THROW({
      Tx::parse_txs_json_for_rpc(move(a));
    },std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_parse_client_json_txt_good){
  path p = current_path() / "example-jsons" / "client-txs-ok.json";
  BOOST_TEST_MESSAGE("File to read: " + p.string());
  string s = read_all_string(p); // json string

  auto r = Tx::parse_txs_jsonString_for_rpc(s);
  BOOST_REQUIRE(r);
  auto [res,txs] = r.value();
  BOOST_CHECK_EQUAL(txs.size(),2);

  // check
  BOOST_CHECK_EQUAL(txs[0].from,makeAddress(1));
  BOOST_CHECK_EQUAL(txs[1].from,makeAddress(1));
  BOOST_CHECK_EQUAL(txs[0].to,makeAddress(2));
  BOOST_CHECK_EQUAL(txs[1].to,makeAddress(0));

  BOOST_CHECK_EQUAL(txs[0].nonce,123);
  BOOST_CHECK_EQUAL(txs[1].nonce,123);

  BOOST_CHECK_EQUAL(txs[0].data,bytes({0xff,0xff}));
  BOOST_CHECK_EQUAL(txs[1].data,bytes({0xff,0xff}));
}

BOOST_AUTO_TEST_CASE(test_parse_client_json_txt_bad){
  path p = current_path() / "example-jsons" / "client-txs-bad.json";
  BOOST_TEST_MESSAGE("File to read: " + p.string());
  string s = read_all_string(p); // json string

  auto r = Tx::parse_txs_jsonString_for_rpc(s);
  BOOST_REQUIRE(not r);
}
BOOST_AUTO_TEST_SUITE_END();

BOOST_AUTO_TEST_SUITE(test_rpc_handlers);

namespace mockedRpcTxsAddable{
  // 🦜 : The accepter that counts how many tx is added.
  class A : public virtual IForRpcTxsAddable{
  public:
    int count{0};
    bool addTxs(vector<Tx> && txs) noexcept override{
      this->count+= txs.size();
      return true;
    }
    string info() noexcept override{
      return "Okay";
    }
  };
}


BOOST_AUTO_TEST_CASE(test_handle_add_txs_ok){
  mockedRpcNetworkable::A nh;   // network host
  mockedRpcTxsAddable::A ch;    // cnsss host
  IForRpcNetworkable * n = dynamic_cast<IForRpcNetworkable*>(&nh);
  IForRpcTxsAddable * c = dynamic_cast<IForRpcTxsAddable*>(&ch);
  // start the rpc with a mocked cnsss
  Rpc rpc{n,c};

  path p = current_path() / "example-jsons" / "client-txs-ok.json";
  string txs_s = read_all_string(p); // json string
  auto [ok, s] = rpc.handle_add_txs(txs_s);
  BOOST_REQUIRE(ok);
  BOOST_CHECK_EQUAL(ch.count,2); // two txs added
}

BOOST_AUTO_TEST_CASE(test_handle_add_txs_bad){
  mockedRpcNetworkable::A nh;   // network host
  IForRpcNetworkable * n = dynamic_cast<IForRpcNetworkable*>(&nh);
  mockedRpcTxsAddable::A ch;    // cnsss host
  IForRpcTxsAddable * c = dynamic_cast<IForRpcTxsAddable*>(&ch);
  // start the rpc with a mocked cnsss
  Rpc rpc{n,c};

  path p = current_path() / "example-jsons" / "client-txs-bad.json";
  string txs_s = read_all_string(p); // json string
  auto [ok, s] = rpc.handle_add_txs(txs_s);
  BOOST_REQUIRE(not ok);
  BOOST_CHECK_EQUAL(ch.count,0); // no txs added
}

namespace mockedForRpc{
  class A : public virtual IForRpc{
  public:
    string info() noexcept override{
      return "aaa";
    }
  };
}

BOOST_AUTO_TEST_CASE(test_handle_get_pool_status){
  // To test pool status, we don't need to give it a cnsss
  mockedRpcNetworkable::A nh;   // network host
  IForRpcNetworkable * n = dynamic_cast<IForRpcNetworkable*>(&nh);
  mockedForRpc::A ah;
  IForRpc * a = dynamic_cast<IForRpc*>(&ah);
  Rpc rpc{n,nullptr,nullptr,a};

  auto [ok, s] = rpc.handle_get_pool_status({});
  BOOST_REQUIRE(ok);
  BOOST_CHECK_EQUAL(s,"aaa");
}


namespace mockedChainDBGettable{
  class A: public virtual IChainDBGettable{
  public:
    optional<string> getFromChainDB(const string k) const override{
      return "value for " + k;
    }
  };
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

BOOST_AUTO_TEST_CASE(test_handle_get_receipt_ok){
  /*
    🦜 : To test get receipt, we need to give it a "valid" IChainDBGettable.

    This should include
    1. TxOnBlkInfo.toString() at "/tx/<hash>"
    2. ExecBlk.toString() at "/blk/<number>

    Where `number` is the `.blkNumber` obtained from the above TxOnBlkInfo.

    🦜 : I feel like the easiest way is just to construct a ExecBlk and let
    `BlkExecutor` to do it for us...
  */
  mockedRpcNetworkable::A nh;   // network host
  mockedAcnPrv::F2 wh;           // the in-RAM rocksDB
  Rpc rpc{dynamic_cast<IForRpcNetworkable*>(&nh) /*the server*/
          ,nullptr /*cnsss*/
          ,dynamic_cast<IChainDBGettable*>(&wh) /*wrld*/
          ,nullptr/*pool info*/};

  // 1. --------------------------------------------------
  // make a ExecBlk
  auto [b,txs] = prepare_ExecBlk();

  // 2. --------------------------------------------------
  // hire a BlkExecutor.
  //
  // 🦜 : If we only need to commitBlk(), then we only need to pass the world.
  // Okay, BlkExecutor will apply the stateChanges, so that each k must be a valid address, and each v must be a valid address
  BlkExecutor exe{dynamic_cast<IWorldChainStateSettable*>(&wh),
                  nullptr /*ITxExecutable*/,
                  nullptr /*IAcnGettable*/};
  BOOST_REQUIRE(exe.commitBlk(b));

  // 3. --------------------------------------------------
  // Get the tx by hash.

  // Now the tx-hashes and ExecBlk should be there
  auto [ok, s] = rpc.handle_get_receipt(unordered_map<string,string>({{"hash",hashToString(txs[0].hash())}}));
  BOOST_REQUIRE(ok);
  BOOST_TEST_MESSAGE((format("GOT:%s") % s).str());
  // 🦜 : `s` should be the json of TxReceipt
  TxReceipt tr;
  BOOST_REQUIRE(tr.fromJsonString(s));

  // --------------------------------------------------
  // bad cases
  std::tie(ok,s) = rpc.handle_get_receipt(unordered_map<string,string>({{"hash","invalidHash"}}));
  BOOST_REQUIRE(not ok);
  std::tie(ok,s) = rpc.handle_get_receipt(unordered_map<string,string>({{"hash",string(32*2,'f')}})); // nonexsiting hash
  BOOST_REQUIRE(not ok);

  // no input
  std::tie(ok,s) = rpc.handle_get_receipt({}); // qurey param not given
  BOOST_REQUIRE(not ok);
  std::tie(ok,s) = rpc.handle_get_receipt(unordered_map<string,string>({{"bad-k","abc"}}));
  BOOST_REQUIRE(not ok);
}

BOOST_AUTO_TEST_CASE(test_get_latest_blk,MY_TEST_THIS){
  /*
    🦜 : Oh, what do we need to test the `get_latest_blk`?

    🐢 : You need a IChainDBGettable which provides the following key:

    "/other/blk_number" : lexical_cast<string>(latest_blk_number)
    "/blk/<N>","/blk/<N-1>"...: all the blk down to the blk 0.

    🦜 : Wait, is our first blk Blk-0 ?

    🐢 : I think this is decided by the sealer(cnsss/sealer.hpp) and
    BlkExecutor(execManager.hpp). It looks like the number stored at
    "/other/blk_number" is the length of chain - 1. If the chain is empty then
    the "/other/blk_number" key shouldn't exist at all.

    🦜 : So how should we test it?

    🐢 : I think the simplest way is to:
    1. Init a in-RAM wrld
    2. Init one ExecBlk b
    3. store b at /blk/0, /blk/1,/blk/2
    4. store /other/blk_number/ = "2"
    5. Try get
    - without param
    - n = 1
    - n = 2 (more)
    - n = 5 (too much)
  */
  mockedAcnPrv::F2 wh;           // the in-RAM rocksDB
  // make a ExecBlk
  auto [b,txs] = prepare_ExecBlk();
  // 3.
  string bs = b.toString();
  BOOST_REQUIRE(wh.setInChainDB("/blk/" + lexical_cast<string>(0), bs));
  BOOST_REQUIRE(wh.setInChainDB("/blk/" + lexical_cast<string>(1), bs));
  BOOST_REQUIRE(wh.setInChainDB("/blk/" + lexical_cast<string>(2), bs));
  BOOST_REQUIRE(wh.setInChainDB("/other/blk_number",lexical_cast<string>(2)));
  // 5.
  IChainDBGettable * w = dynamic_cast<IChainDBGettable*>(&wh);

  /**
   * @brief test the get_latest_Blk
   *
   * @param expected_i The expected size() of returned array
   *
   * @param it The input param of get_latest_Blk()
   */
  auto test_get_latest_blk = [w,&b](int expected_i=1,
                                 const int * const it=nullptr
                                 ){
    BOOST_TEST_MESSAGE((format("Testing test_get_latest_blk() with expected_i=%d, it=%s")
                        % expected_i % (it==nullptr ? "nullptr" : "&" + lexical_cast<string>(*it))
                        ).str()
                       );
    bool ok;
    string s;
    if (it == nullptr)
      std::tie(ok,s) = Rpc::get_latest_Blk(w);
    else
      std::tie(ok,s) = Rpc::get_latest_Blk(w,*it);

    BOOST_REQUIRE(ok);
    /*
      🐢 : Now `s` should be an array of ExecBlk. In our case, they are all the
      above `b`.

      🦜 : Is there an convinient way we can parse the above `s` into an,say... a
      vector of ExecBlk ?

      🐢 : Let's parse it into json::value first.
    */
    json::error_code ec;
    json::value jv = json::parse(s, ec );
    BOOST_REQUIRE(not ec);
    BOOST_REQUIRE(jv.is_array());

    // BOOST_TEST_MESSAGE((format("Got:\n%s") % s).str());
    // 🐢 : Now it's time for boost::json to show:
    vector<ExecBlk> vb = json::value_to<vector<ExecBlk>>(jv);
    BOOST_REQUIRE_EQUAL(vb.size(),expected_i);
    for (int i=0;i<expected_i;i++)
      BOOST_CHECK_EQUAL(vb[i].hash,b.hash);
  };

  // --------------------------------------------------
  // 5. Start testing
  test_get_latest_blk();
  int i = 1; test_get_latest_blk(1,&i);
  i = 2;test_get_latest_blk(i,&i);
  i = 3;test_get_latest_blk(i,&i);
  i = 5;test_get_latest_blk(3,&i); // ask for 5, but there's only 3

  // 🦜 : Let's test some bad cases
  auto [ok,s] = Rpc::get_latest_Blk(w,-1);
  BOOST_REQUIRE(not ok);
}

BOOST_AUTO_TEST_CASE(test_handle_get_latest_Blk){
  /* 🦜 : Okay here, we test handle_get_latest_Blk. This essentially parse te
     "n" in query_param. The preparation should be similar*/
  mockedAcnPrv::F2 wh;           // the in-RAM rocksDB
  // make a ExecBlk
  auto [b,txs] = prepare_ExecBlk();
  // 3.
  string bs = b.toString();
  BOOST_REQUIRE(wh.setInChainDB("/blk/" + lexical_cast<string>(0), bs));
  BOOST_REQUIRE(wh.setInChainDB("/blk/" + lexical_cast<string>(1), bs));
  BOOST_REQUIRE(wh.setInChainDB("/blk/" + lexical_cast<string>(2), bs));
  BOOST_REQUIRE(wh.setInChainDB("/other/blk_number",lexical_cast<string>(2)));
  // 5.
  IChainDBGettable * w = dynamic_cast<IChainDBGettable*>(&wh);

  /*
    🦜 : To test handle_get_latest_Blk, we only need to give rpc the storage
  */
  mockedRpcNetworkable::A nh;   // network host
  Rpc rpc{dynamic_cast<IForRpcNetworkable*>(&nh) /*the server*/
          ,nullptr /*cnsss*/
          ,dynamic_cast<IChainDBGettable*>(&wh) /*wrld*/
          ,nullptr/*pool info*/};

  auto [ok,s] = rpc.handle_get_latest_Blk({});

  auto test_get_latest_blk = [&rpc,&b](int expected_i=1,
                                     optional<unordered_map<string,string>> m){
    bool ok;
    string s;
    std::tie(ok,s) = rpc.handle_get_latest_Blk(m);

    BOOST_REQUIRE(ok);
    /*
      🐢 : Now `s` should be an array of ExecBlk. In our case, they are all the
      above `b`.

      🦜 : Is there an convinient way we can parse the above `s` into an,say... a
      vector of ExecBlk ?

      🐢 : Let's parse it into json::value first.
    */
    json::error_code ec;
    json::value jv = json::parse(s, ec );
    BOOST_REQUIRE(not ec);
    BOOST_REQUIRE(jv.is_array());

    // BOOST_TEST_MESSAGE((format("Got:\n%s") % s).str());
    // 🐢 : Now it's time for boost::json to show:
    vector<ExecBlk> vb = json::value_to<vector<ExecBlk>>(jv);
    BOOST_REQUIRE_EQUAL(vb.size(),expected_i);
    for (int i=0;i<expected_i;i++)
      BOOST_CHECK_EQUAL(vb[i].hash,b.hash);
  };

  // test good
  test_get_latest_blk(1,{});

  test_get_latest_blk(2,
                      unordered_map<string,string>({{"n","2"}})
                      );

  test_get_latest_blk(3,
                      unordered_map<string,string>({{"n","3"}})
                      );

  test_get_latest_blk(3,
                      unordered_map<string,string>({{"n","5"}})
                      );

  auto test_bad = [&rpc](optional<unordered_map<string,string>> m){
    auto [ok,s] = rpc.handle_get_receipt(m);
    BOOST_REQUIRE(not ok);
  };

  // test bad
  test_bad(unordered_map<string,string>({{"n","-1"}}));
  test_bad(unordered_map<string,string>({{"n","abc"}}));
}
BOOST_AUTO_TEST_SUITE_END();


