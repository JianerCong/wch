#include "h.hpp"
#include "rpc.hpp"
#include <fstream>
#include "execManager.hpp"
#include "mock.hpp"

#include "net/pure-weakAsyncHttpServer.hpp"


using namespace weak;

tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
}

BOOST_AUTO_TEST_CASE(test_add_txs_pb){
  auto [a1,a2,data] = get_example_address_and_data();
  hiPb::Txs txs_pb;

  for (int i =0;i<3;i++){
    hiPb::Tx pb = hiPb::Tx();
    pb.set_type(hiPb::TxType::EVM);

    pb.set_from_addr(toByteString<address>(a1));
    pb.set_to_addr(toByteString<address>(a2));
    pb.set_data(toString(data));
    pb.set_nonce(i);

    txs_pb.add_txs()->CopyFrom(pb);
  }

  optional<tuple<string,vector<Tx>>> r = Tx::parse_txs_pb_for_rpc(txs_pb);


  BOOST_REQUIRE(r);
  auto [rpl, txs] = r.value();

  // 🦜 : The timestamp should be there
  BOOST_CHECK(txs[0].timestamp > (time(nullptr) - 5));
  BOOST_CHECK(txs[0].timestamp <= (time(nullptr)));
  BOOST_CHECK_EQUAL(addressToString(txs[0].from), addressToString(a1));
  BOOST_CHECK_EQUAL(addressToString(txs[0].to), addressToString(a2));
  BOOST_CHECK_EQUAL(toString(txs[0].data), toString(data));
  BOOST_CHECK_EQUAL(txs[0].nonce, 0);

  // 🦜 : The rpl should be hiPb::AddTxsReply
  hiPb::AddTxsReply rpl_pb;
  BOOST_REQUIRE(rpl_pb.ParseFromString(rpl));
  // their are three
  /*
    message AddTxReply {bytes hash = 1; bytes deployed_addr = 2;}
    message AddTxsReply {repeated AddTxReply txs = 1;}
   */
  BOOST_CHECK_EQUAL(rpl_pb.txs_size(),3);
  BOOST_CHECK_EQUAL(rpl_pb.txs(0).hash(),
                    toByteString<hash256>(txs[0].hash()));
}
