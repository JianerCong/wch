/**
 * @file test-pure-net.cpp
 * @brief Here we tests some helpler functions in the rcp server.
 *
 * 🦜 : We test them here, because they don't need to resort to an external
 * server or client. These are usually some static methods;
 */

#include "h.hpp"
#include "net/pure-weakUdpServer.hpp"
#include "net/pure-weakUdpClient.hpp"


using namespace pure;

BOOST_AUTO_TEST_CASE(test_one){
  BOOST_CHECK(1==1);
}


void sleep_for(int i=1){
  std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
}

BOOST_AUTO_TEST_CASE(test_udp_send_get){
  string s0;
  {
    WeakUdpServer serv{7777};

    serv.listen("/aaa",
                [&s0](string_view s){
                  BOOST_TEST_MESSAGE("Recieved " + string(s));
                  s0 = string(s);
                }
                );


    // send the request

    WeakUdpClient::send("localhost","7777","/aaa:my_msg");
    sleep_for(1);
    // 🦜 Because we told the server not to close before all handlers are run.
    // We don't need a magical sleep here for server. But we do need to wait
    // until the msg is sent.
  } // closed
  BOOST_CHECK_EQUAL(s0, "my_msg");
}
