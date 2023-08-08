/**
 * @brief Test the weakHttpClient independently.
 *
 * 🦜 : This will need an external server at 8080
 */
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include "../pure-greenHttpClient.hpp"
#include "../"
using pure::GreenHttpClient;

using namespace weak;

BOOST_AUTO_TEST_CASE(test_impl_get) {
  // init a json
  uint16_t port = 7777;

  int N{100},M{1000};

  // make json of 1000 txs
  json::array a;
  for (int i =0;i<M;i++){
    json::value jv{
      {"from", "01"},
      {"to", ""},
      {"data", ""},
      {"nonce",i};
    };

    a.emplace_back(jv);
  }
  BOOST_CHECK_EQUAL(a.size(),1000);

  string c_txs = json::serialize(a);
  {
    GreenHttpClient c;
    sleep_for(1);

    c.get("localhost","/get_node_status",port,c_txs);
    // this establish a connection

    // start timmer
    using namespace std::chrono;
    time_point<high_resolution_clock> start, end;
    boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);

    start = high_resolution_clock::now(); // --------------------------------------------------
    for (int i = 0;i<N;i++){
      c.post("localhost","/add_txs",port,c_txs);
    }
    // end timmer
    end = high_resolution_clock::now();    // --------------------------------------------------

    duration<double> elapsed_seconds = end - start;
    double s = elapsed_seconds.count();
    double tps = M * N/s;
    BOOST_TEST_MESSAGE(format("sec elapsed: %4.2e sec, so TPS = %.0f") % s % (tps));

    // BOOST_REQUIRE(r);
    // BOOST_CHECK_EQUAL(r.value(),"aaa received bbb");
    // BOOST_CHECK_EQUAL(c.conns.size(),1);

    // start finish
  } // client closed, which closes the connection
  sleep_for(1);

  // 
} // BOOST_AUTO_TEST_CASE(test_no_1)
