#include "h.hpp"

#include "core.hpp"
using namespace weak;
#include <boost/format.hpp>
#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
using boost::format;
#include <chrono>
#include <thread>

#include <algorithm>
using std::begin;
using std::end;
#include <iostream>
using std::cout;

/*

  🦜 : What do we tests here?

  🐢 : I think we should test the following:

  home-made Tx -> pb
  home-made pb -> Tx

*/

tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
}
BOOST_AUTO_TEST_CASE(test_Tx_to_pb){
  BOOST_CHECK_EQUAL(1,1);
  // 1. make a Tx
  auto [a1,a2,data] = get_example_address_and_data();
  Tx t = Tx(a1,a2,data,123/*nonce*/);

  // 2. convert to pb
  hiPb::Tx pb = t.toPb();
  BOOST_CHECK_EQUAL(pb.type(), hiPb::TxType::EVM);
  BOOST_CHECK_EQUAL(pb.from_addr(), toByteString<address>(a1));
  BOOST_CHECK_EQUAL(pb.to_addr(), toByteString<address>(a2));
  BOOST_CHECK_EQUAL(pb.data(), toString(data));
  BOOST_CHECK_EQUAL(pb.nonce(), 123);

  // timestamp should have been set, which is smaller than now
  std::time_t now = std::time(nullptr); // seconds since epoch
  BOOST_TEST_MESSAGE((format("timestamp set to %d, now it's %d")
                      % pb.timestamp()
                      % now
                      ).str());
  BOOST_CHECK(pb.timestamp() <= now);
  BOOST_CHECK(pb.timestamp() > now -10);

  // other fields should be empty
  BOOST_CHECK_EQUAL(pb.signature(), "");
  BOOST_CHECK_EQUAL(pb.pk_pem(), "");
  BOOST_CHECK_EQUAL(pb.pk_crt(), "");
}

BOOST_AUTO_TEST_CASE(test_data_tx_to_pb){
  auto [a1,a2,data] = get_example_address_and_data();
  Tx t = Tx(a1,a2,data,123/*nonce*/);
  t.type = Tx::Type::data;

  // 2. convert to pb
  hiPb::Tx pb = t.toPb();
  BOOST_CHECK_EQUAL(pb.type(), hiPb::TxType::DATA);
}

BOOST_AUTO_TEST_CASE(test_pb_to_tx){
  // 1. make a pb

  auto [a1,a2,data] = get_example_address_and_data();

  hiPb::Tx pb = hiPb::Tx();
  pb.set_type(hiPb::TxType::EVM);
  pb.set_from_addr(toByteString<address>(a1));
  pb.set_to_addr(toByteString<address>(a2));
  pb.set_data(toString(data));
  pb.set_nonce(123);

  // 2. convert to Tx
  Tx t;
  t.fromPb(pb);
  BOOST_CHECK_EQUAL(Tx::typeToString(t.type),
                    Tx::typeToString(Tx::Type::evm));
  BOOST_CHECK_EQUAL(addressToString(t.from), addressToString(a1));
  BOOST_CHECK_EQUAL(addressToString(t.to), addressToString(a2));
  BOOST_CHECK_EQUAL(t.data, data);
  BOOST_CHECK_EQUAL(t.nonce, 123);
}

BOOST_AUTO_TEST_CASE(test_pb_to_tx_bad){
  // 1. make a pb
  hiPb::Tx pb = hiPb::Tx();
  pb.set_from_addr("this is a bad address, because it has more than 20 bytes");
  BOOST_CHECK_THROW(
                    {
                      Tx t;
                      t.fromPb(pb);
                    }
                    ,std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_blk_to_pb){
  auto [a1,a2,data] = get_example_address_and_data();

  Tx t1 = Tx(a1,a2,data,123/*nonce*/);
  Tx t2 = Tx(a1,a2,data,234/*nonce*/);
  vector<Tx> txs ={t1,t2};

  // parent hash
  hash256 p;
  std::fill(std::begin(p.bytes),std::end(p.bytes),0x00);

  // make block
  Blk b = Blk(1,p,txs);

  hiPb::Blk pb = b.toPb();
  BOOST_CHECK_EQUAL(pb.header().number(), 1);
  BOOST_CHECK_EQUAL(pb.header().parenthash(), toByteString<hash256>(p));
  BOOST_CHECK_EQUAL(pb.txs_size(), 2);
  BOOST_CHECK_EQUAL(pb.txs(0).nonce(), 123);
  BOOST_CHECK_EQUAL(pb.txs(1).nonce(), 234);
}

BOOST_AUTO_TEST_CASE(test_pb_to_blk){
  hiPb::Blk pb = hiPb::Blk();
  auto [a1,a2,data] = get_example_address_and_data();
  // header

  // parent hash
  hash256 p;
  std::fill(std::begin(p.bytes),std::end(p.bytes),0x00);

  pb.mutable_header()->set_number(1);
  pb.mutable_header()->set_parenthash(toByteString<hash256>(p));

  // txs
  Tx t1 = Tx(a1,a2,data,123/*nonce*/);
  Tx t2 = Tx(a1,a2,data,234/*nonce*/);
  vector<Tx> txs ={t1,t2};
  for (auto t : txs){
    hiPb::Tx* pt = pb.add_txs();
    pt->CopyFrom(t.toPb());
  }

  // convert to blk
  Blk b;
  b.fromPb(pb);

  BOOST_CHECK_EQUAL(b.number, 1);
  BOOST_CHECK_EQUAL(hashToString(b.parentHash), hashToString(p));
  BOOST_CHECK_EQUAL(b.txs.size(), 2);
  BOOST_CHECK_EQUAL(b.txs[0].nonce, 123);
  BOOST_CHECK_EQUAL(b.txs[1].nonce, 234);
}


BOOST_AUTO_TEST_CASE(test_read_file_feature){
  // 1. make a file in the temp directory
  filesystem::path p = filesystem::temp_directory_path() / "hi.txt";

  // 2. write some content
  pure::writeToFile(p,"aaaa");

  // 3. try the feature defined in Tx::parse_txs_json_rpc
  json::array a;
  json::object tx = {{"from","0x01"},
                     {"to","0x02"},
                     {"data",'@' + p.string()},
                     {"nonce",123}};
  a.push_back(tx);
  // static optional<tuple<string,vector<Tx>>> Tx::parse_txs_json_for_rpc(json::array && a);
  auto r = Tx::parse_txs_json_for_rpc(std::move(a));
  BOOST_REQUIRE(r);
  auto [err,txs] = r.value();
  BOOST_CHECK_EQUAL(txs.size(),1);
  BOOST_CHECK_EQUAL(evmc::hex(txs[0].data), "aaaa");

  // clean up, remove the file
  filesystem::remove(p);
}

BOOST_AUTO_TEST_CASE(test_read_file2_data){
  // 1. make a file in the temp directory
  filesystem::path p = filesystem::temp_directory_path() / "hi.txt";

  // 2. write some content
  pure::writeToFile(p,"aaa");

  // 3. try the feature defined in Tx::parse_txs_json_rpc
  json::array a;
  json::object tx = {{"from","0x01"},
                     {"to","0x02"},
                     {"data",'@' + p.string()},
                     {"type" , "data"},
                     {"nonce",123}};
  a.push_back(tx);
  // static optional<tuple<string,vector<Tx>>> Tx::parse_txs_json_for_rpc(json::array && a);

  auto r = Tx::parse_txs_json_for_rpc(std::move(a));
  BOOST_REQUIRE(r);
  auto [err,txs] = r.value();
  BOOST_CHECK_EQUAL(txs.size(),1);
  BOOST_CHECK_EQUAL(weak::toString(txs[0].data), "aaa");

  // clean up, remove the file
  filesystem::remove(p);
  }
