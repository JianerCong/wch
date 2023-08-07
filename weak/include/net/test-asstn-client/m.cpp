#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include "../pure-httpNetAsstn.hpp"

using namespace pure;

#include <thread>
#include <chrono>

void sleep_for(int i=1){
  std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
}
BOOST_AUTO_TEST_CASE(test_basic_send_to_example_found){
  /* 🦜: It turns out that testing `send()` won't need a server.
   */
  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);

  {
    IPBasedHttpNetAsstn a{nullptr,m};
    string endpoint = SignedData::serialize_3_strs("to-be-ignored","localhost:8080","");
    auto r = a.send(endpoint,"/aaa","123"); // should call http://localhost:8080/p2p/aaa -d 123

    BOOST_CHECK(r);
    // BOOST_TEST_MESSAGE((format("Got response: %s") % r.value()).str());
    // 🦜 : client should received the "signed data"

    BOOST_CHECK_EQUAL(r.value(),
                      (format("Data recieved: %s") %
                       m->prepare_msg("123")
                       ).str()
                      );
    sleep_for(1);
  }
  // conn closed
}

BOOST_AUTO_TEST_CASE(test_basic_send_to_example_unknown_target){
  /* 🦜: It turns out that testing `send()` won't need a server.
   */
  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);

  IPBasedHttpNetAsstn a{nullptr,m};
  string endpoint = SignedData::serialize_3_strs("to-be-ignored","localhost:8080","");
  optional<string> r = a.send(endpoint,"/unknownTarget","123");

  BOOST_CHECK(not r);
}

BOOST_AUTO_TEST_CASE(test_basic_send_to_example_unknown_host){
  /* 🦜: It turns out that testing `send()` won't need a server.
   */
  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);

  IPBasedHttpNetAsstn a{nullptr,m};
  string endpoint = SignedData::serialize_3_strs("to-be-ignored","localhost:8081","");
  optional<string> r = a.send(endpoint,"/aaa","123");

  BOOST_CHECK(not r);
}
