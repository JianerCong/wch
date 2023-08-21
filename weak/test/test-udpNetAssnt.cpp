#include "h.hpp"
#include "net/pure-udpNetAsstn.hpp"


using namespace pure;

BOOST_AUTO_TEST_CASE(test_one){
  BOOST_CHECK_EQUAL(1,1);
}


struct F {
  NaiveMsgMgr * mh;
  IMsgManageable * m;
  F() {
    this->mh = new NaiveMsgMgr("N0");
    this->m = dynamic_cast<IMsgManageable*>(this->mh);
    BOOST_TEST_MESSAGE( "setup fixture" );
  }
  ~F() {
    BOOST_TEST_MESSAGE( "teardown fixture" );
    delete this->mh;
  }

  int i;
};

BOOST_FIXTURE_TEST_CASE(test_listen,F){
  IPBasedUdpNetAsstn a{7777,m};

  a.listen("/aaa",[](string,string){});

  BOOST_CHECK_EQUAL(a.serv->lisn_map.size(),1);
}

BOOST_FIXTURE_TEST_CASE(test_listen_clear,F){
  IPBasedUdpNetAsstn a{7777,m};


  a.listen("/aaa",[](string,string){});
  a.listen("/bbb",[](string,string){});
  BOOST_CHECK_EQUAL(a.serv->lisn_map.size(),2);
  a.clear();
  BOOST_CHECK_EQUAL(a.serv->lisn_map.size(),0);
}

BOOST_FIXTURE_TEST_CASE(test_make_handler,F){
  IPBasedUdpNetAsstn a{7777,m};

  string s;
  auto f = a.make_handler([&s](string from,string data){
    BOOST_TEST_MESSAGE("aaa handler called with: from = " + from + " data = " + data);
    s = from + ":" + data;
  });

  string msg = m->prepare_msg("aaa");
  f(msg);

  BOOST_CHECK_EQUAL(s,"N0:aaa");
}
void sleep_for(int i=1){
  std::this_thread::sleep_for(std::chrono::seconds(i)); // wait until its up
}

BOOST_FIXTURE_TEST_CASE(test_send,F){
  string s;

  {
    IPBasedUdpNetAsstn a{7777,m};
    a.listen("/aaa",[&s](string from,string data){
      BOOST_TEST_MESSAGE("aaa handler called with: from = " + from + " data = " + data);
      s = from + ":" + data;
    });

    string endpoint = SignedData::serialize_3_strs("to-be-ignored","localhost:7777","");
    a.send(endpoint,"/aaa","123");
  }
  // wait for the packet
  BOOST_CHECK_EQUAL(s,"N0:123");
}

BOOST_AUTO_TEST_CASE(test_send_to_two){
  NaiveMsgMgr mh{"N0"};
  NaiveMsgMgr mh1{"N1"};
  IMsgManageable * m = dynamic_cast<IMsgManageable*>(&mh);;
  IMsgManageable * m1 = dynamic_cast<IMsgManageable*>(&mh1);;

  string s;
  {
    IPBasedUdpNetAsstn a{7777,m};
    a.listen("/aaa",[&s](string from,string data){
      BOOST_TEST_MESSAGE("aaa handler called with: from = " + from + " data = " + data);
      s = from + ":" + data;
    });

    IPBasedUdpNetAsstn a1{7778,m1};

    string endpoint = SignedData::serialize_3_strs("to-be-ignored","localhost:7777","");
    a1.send(endpoint,"/aaa","123");
  } // wait for the packet

  BOOST_CHECK_EQUAL(s,"N1:123");
}
