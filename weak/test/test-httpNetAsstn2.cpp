
#include "h.hpp"
#include "net/pure-httpNetAsstn.hpp"

using namespace pure;

BOOST_AUTO_TEST_SUITE(test_real_server_up);


void sleep_for(int i=1){
  std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
}

BOOST_AUTO_TEST_CASE(test_basic_listen){
  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);

  WeakHttpServer s;             // listen 7777
  {
    IPBasedHttpNetAsstn a{dynamic_cast<IHttpServable*>(&s),m};
    sleep_for(1);                 // wait until server's up

    IEndpointBasedNetworkable * n = dynamic_cast<IEndpointBasedNetworkable*>(&a);
    BOOST_CHECK(s.postLisnMap.empty());

    auto f = [](string endpoint,string data) -> optional<string>{
      return "aaa recieved";
    };

    n->listen("/aaa",f);

    BOOST_CHECK_EQUAL(s.postLisnMap.size(),1);
    // the map is appended.
    BOOST_CHECK(s.postLisnMap.contains( a.PREFIX + "/aaa"));
    // the map contains the handler now
  } // client closed

  BOOST_CHECK_EQUAL(s.postLisnMap.size(),0);
  /*
    🦜: Note that the asstn cleans up itself
   */


       } // server closed here



BOOST_AUTO_TEST_CASE(test_listen_and_send_to_self){
  // --------------------------------------------------
  // Start a server
  NaiveMsgMgr mh("N0");
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);
  WeakHttpServer s{7778};             // listen 7777
  sleep_for(1);                 // wait until server's up
  {
    IPBasedHttpNetAsstn a{dynamic_cast<IHttpServable*>(&s),m};

    IEndpointBasedNetworkable * n = dynamic_cast<IEndpointBasedNetworkable*>(&a);
    auto f = [](string endpoint,string data) -> optional<string>{
      return (format("aaa recieved: %s") % data).str();
    };
    n->listen("/aaa",f);

    // --------------------------------------------------
    // Send a request
    string endpoint = SignedData::serialize_3_strs("to-be-ignored","localhost:7778","");
    auto r = n->send(endpoint,"/aaa","123"); // should call http://localhost:7777/p2p/aaa -d 123

    BOOST_CHECK(r);
    // BOOST_TEST_MESSAGE((format("Got response: %s") % r.value()).str());
    BOOST_CHECK_EQUAL(r.value(),(format("aaa recieved: %s") % "123").str());
  }               // client closed
  sleep_for(1);                 // wait until server's up
}                               //server closed here

BOOST_AUTO_TEST_CASE(test_listen2_and_send_to_other, MY_TEST_THIS){
  // --------------------------------------------------
  // Start a server
  NaiveMsgMgr mh("N0"); IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);
  NaiveMsgMgr mh1("N1"); IMsgManageable * m1 = dynamic_cast<IMsgManageable*>(&mh1);
  WeakHttpServer s{7779};             // listen 7777
  sleep_for(1);                 // wait until server's up

  {
    BOOST_LOG_TRIVIAL(debug) << format("🐸 starting client");
    IPBasedHttpNetAsstn a{dynamic_cast<IHttpServable*>(&s),m};
    IPBasedHttpNetAsstn a1{nullptr,m1}; // 🦜 sender doesn't need a server up

    IEndpointBasedNetworkable * n = dynamic_cast<IEndpointBasedNetworkable*>(&a);
    IEndpointBasedNetworkable * n1 = dynamic_cast<IEndpointBasedNetworkable*>(&a1);

    auto f = [](string endpoint,string data) -> optional<string>{
      return (format("aaa recieved: %s") % data).str();
    };
    n->listen("/aaa",f);

    // --------------------------------------------------
    // Send a request
    string endpoint = SignedData::serialize_3_strs("to-be-ignored","localhost:7779","");
    auto r = n1->send(endpoint,"/aaa","123"); // should call http://localhost:7779/p2p/aaa -d 123

    BOOST_CHECK(r);
    // BOOST_TEST_MESSAGE((format("Got response: %s") % r.value()).str());
    BOOST_CHECK_EQUAL(r.value(),(format("aaa recieved: %s") % "123").str());
  }             // client closed
  sleep_for(1);                 // wait until server's up
}                               //server closed here


BOOST_AUTO_TEST_SUITE_END();
