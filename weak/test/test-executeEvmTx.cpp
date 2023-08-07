#include "h.hpp"

#include "evmExecutor.hpp"
#include "mock.hpp"

#include "../pkgs/evmweak/evmc-10.1.0/include/evmc/mocked_host.hpp"
#include "utils/bytescode.hpp"
#include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <thread>


// 🦜: This is specific to the evmExecutor
namespace mockedEvmMsgExec {
  // mockedAcnProviders: they implement IAcnGettable for test.

  /// An empty executor: always returns SUCCESS and a predefined result;
  class A: public virtual weak::IEvmMsgExecutable{
  public:
    bytes output{0xaa,0xbb,0xcc};
    evmc::Result execMsg(const evmc_message & msg,
                         weak::WeakEvmHost & h)
      const noexcept override{
      BOOST_LOG_TRIVIAL(debug) <<
        format("🐸 mockedEvmMsgExec::A.execMsg started");
      logMsg(msg);

      // the mocked-execution output for testing
      // fake result
      evmc_status_code sc = EVMC_SUCCESS; // or EVMC_FAILURE
      int64_t gas_left= std::numeric_limits<int64_t>::max(),gas_refund=0;
      return evmc::Result{sc,gas_left,gas_refund,&output[0],output.size()};
    };
  };

}

using namespace weak;
BOOST_AUTO_TEST_SUITE(test_evmHostbasic);

BOOST_AUTO_TEST_CASE(test_evmHost_minimal){
  // the world state
  mockedAcnPrv::A mh;
  IAcnGettable* w = dynamic_cast<IAcnGettable*>(&mh);

  // the tx
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(a1,a2,data,123/*nonce*/);

  // the host
  WeakEvmHost h{w,t};
  BOOST_CHECK_EQUAL(h.name,"WeakEvmHost");
}

/**
 * @brief Test the deadAcn std::set
 */
BOOST_AUTO_TEST_CASE(test_evmHost_deadAcn){
  // the world state
  mockedAcnPrv::A mh;
  IAcnGettable* w = dynamic_cast<IAcnGettable*>(&mh);

  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2),
            data, 123/*nonce*/);
  // the host
  WeakEvmHost h{w,t};

  h.deadAcn.insert(makeAddress(1));
  h.deadAcn.insert(makeAddress(2));
  h.deadAcn.insert(makeAddress(1));

  int c{0};
  for (auto & a : h.deadAcn){
    c++;
    BOOST_TEST_MESSAGE(format("suicided addr: %s") % addressToString(a));
  }

  // duplicated accounts are not added.
  BOOST_CHECK_EQUAL(h.deadAcn.size(),2);
  BOOST_CHECK_EQUAL(h.deadAcn.size(),c);

}

BOOST_AUTO_TEST_CASE(test_evmHost_tryAddAcnFromWorldState){
  // the world state
  mockedAcnPrv::A mh;
  IAcnGettable* w = dynamic_cast<IAcnGettable*>(&mh);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2),
            data, 123/*nonce*/);
  // the host
  WeakEvmHost h{w,t};
  BOOST_REQUIRE_EQUAL(h.accounts.size(),0);
  BOOST_CHECK(not h.tryAddAcnFromWorldState(makeAddress(1)));
}

BOOST_AUTO_TEST_CASE(test_evmHost_tryAddAcnFromWorldState2){
  // the world state

  mockedAcnPrv::C mh; /*only provide (1,{.nonce =123})*/
  IAcnGettable* w = dynamic_cast<IAcnGettable*>(&mh);

  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);

  // the host
  WeakEvmHost h{w,t};
  BOOST_REQUIRE_EQUAL(h.accounts.size(),0);
  // Accn exists in underlying DB
  BOOST_CHECK(h.account_exists(makeAddress(1)));

  // get Acn from underlying DB
  BOOST_CHECK(h.tryAddAcnFromWorldState(makeAddress(1)));

  // Now the account is added.
  BOOST_REQUIRE_EQUAL(h.accounts.size(),1);
  BOOST_CHECK(h.account_exists(makeAddress(1)));
  Acn a = h.accounts[makeAddress(1)];
  BOOST_CHECK_EQUAL(a.nonce,123);

  // try get a non-existing Acn
  BOOST_CHECK(not h.tryAddAcnFromWorldState(makeAddress(2)));
  // this->accounts remains unchanged
  BOOST_REQUIRE_EQUAL(h.accounts.size(),1);
  BOOST_CHECK(not h.account_exists(makeAddress(2)));
}

// --------------------------------------------------
// EVM-host method tests

BOOST_AUTO_TEST_CASE(test_account_exists_in_local){
  mockedAcnPrv::A mh;           // empty provider
  IAcnGettable* w = dynamic_cast<IAcnGettable*>(&mh);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // the host
  WeakEvmHost h{w,t};
  address a1 = makeAddress(1);
  h.accounts[a1] = {};
  BOOST_REQUIRE_EQUAL(h.accounts.size(),1);
  BOOST_CHECK(h.account_exists(a1));
}


BOOST_AUTO_TEST_CASE(test_account_exists_in_remote){
  mockedAcnPrv::C mh; /*only provide (1,{.nonce =123})*/
  IAcnGettable* w = dynamic_cast<IAcnGettable*>(&mh);

  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);

  // the host
  WeakEvmHost h{w,t};

  BOOST_REQUIRE_EQUAL(h.accounts.size(),0);
  BOOST_CHECK(h.account_exists(makeAddress(1)));
  BOOST_CHECK(not h.account_exists(makeAddress(2)));
}


BOOST_AUTO_TEST_CASE(test_account_exists_in_remote_but_dead){
  mockedAcnPrv::C mh; /*only provide (1,{.nonce =123})*/
  IAcnGettable* w = dynamic_cast<IAcnGettable*>(&mh);


  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);

  // the host
  WeakEvmHost h{w,t};
  h.deadAcn.insert(makeAddress(1));

  BOOST_REQUIRE_EQUAL(h.accounts.size(),0);
  BOOST_CHECK(not h.account_exists(makeAddress(1)));
}



BOOST_AUTO_TEST_CASE(test_evmHost_set_storage){
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // the host
  WeakEvmHost h{w,t};
  BOOST_REQUIRE_EQUAL(h.accounts.size(),0);

  address a1 = makeAddress(1);

  // set storage, Acn is created
  bytes32 k{0x1}, v{0x11};
  evmc_storage_status s = h.set_storage(a1,k,v);

  // check
  BOOST_CHECK_EQUAL(s,EVMC_STORAGE_MODIFIED);
  // Acn created
  BOOST_REQUIRE_EQUAL(h.accounts.size(),1);
  BOOST_REQUIRE(h.accounts.contains(a1));
  Acn a = h.accounts.find(a1)->second;
  BOOST_REQUIRE(a.storage.contains(bytes32{0x1}));

  bytes32 v_stored = a.storage.find(bytes32{0x1})->second;
  // the stored value
  intx::uint<256> v1 = intx::be::load<intx::uint<256>,bytes32>(v_stored);
  // BOOST_CHECK_EQUAL(uint64_t{v1} /* down-cast, uint256 has no ostream<< */
  //                   ,0x11);

  BOOST_CHECK_EQUAL(v1 /* 🦜 but now we defined that in core.hpp,we can use
                          this now*/
                    ,0x11);

}

BOOST_AUTO_TEST_CASE(test_evmHost_get_storage_local){

  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // the host
  WeakEvmHost h{w,t};
  BOOST_REQUIRE_EQUAL(h.accounts.size(),0);

  address a1 = makeAddress(1);

  // set storage, Acn is created
  bytes32 k{0x1}, v{0x11};
  evmc_storage_status s = h.set_storage(a1,k,v);

  // check
  BOOST_CHECK_EQUAL(s,EVMC_STORAGE_MODIFIED);
  // Acn created
  BOOST_REQUIRE_EQUAL(h.accounts.size(),1);
  BOOST_REQUIRE(h.accounts.contains(a1));

  // get the stored value through get_storage
  bytes32 v_stored = h.get_storage(a1,bytes32{0x1});
  // the stored value
  intx::uint<256> v1 = intx::be::load<intx::uint<256>,bytes32>(v_stored);
  // BOOST_CHECK_EQUAL(uint64_t{v1} /* down-cast, uint256 has no ostream<< */
  //                   ,0x11);

  BOOST_CHECK_EQUAL(v1 /* 🦜 but now we defined that in core.hpp,we can use
                          this now*/
                    ,0x11);
}

BOOST_AUTO_TEST_CASE(test_copy_code_not_found){
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // the host
  WeakEvmHost h{w,t};
  BOOST_REQUIRE_EQUAL(h.accounts.size(),0);

  address a1 = makeAddress(1);
  bytes b(32,0x00);

  size_t i = h.copy_code(a1,0,reinterpret_cast<uint8_t*>(b.data()),b.size());
  BOOST_CHECK_EQUAL(i,0);
}

BOOST_AUTO_TEST_CASE(test_copy_code_found){
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // the host
  WeakEvmHost h{w,t};
  BOOST_REQUIRE_EQUAL(h.accounts.size(),0);

  // put in the Acn with code
  address a1 = makeAddress(1);
  Acn an{123, evmc::from_hex("0000aabb").value() /*code*/};
  h.accounts[a1] = an;

  // get the code
  bytes b(32,0x00);
  size_t i = h.copy_code(a1,0,reinterpret_cast<uint8_t*>(b.data()),b.size());
  BOOST_CHECK_EQUAL(i,4);

  // 🦜: Important Update: we can now compare evmc::bytes directly
  BOOST_CHECK_EQUAL(b.substr(0,i),evmc::from_hex("0000aabb").value());
}

BOOST_AUTO_TEST_SUITE_END();

BOOST_AUTO_TEST_SUITE(test_evmMsgExe);

BOOST_AUTO_TEST_CASE(test_mocked_evmMsgExec){

  unique_ptr<IEvmMsgExecutable> e{new mockedEvmMsgExec::A /*empty executor*/};

  // the host
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  WeakEvmHost h(w,t);
  auto [msg,code] = makeMsg("" /*code hex*/,
                            "aabbcc" /*data hex*/
                            );
  evmc::Result r = e->execMsg(msg,h);
  BOOST_CHECK_EQUAL(r.status_code,EVMC_SUCCESS);

  bytes d(r.output_data,r.output_size);
  bytes d_expected{0xaa,0xbb,0xcc};
  BOOST_CHECK_EQUAL(d,d_expected);
}

BOOST_AUTO_TEST_CASE(weakhost_call_with_nothing){
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // the host
  WeakEvmHost h{w,t};

  auto [msg,code] = makeMsg("" /*code hex*/,
                            "aabbcc" /*data hex*/
                            );
  evmc::Result r = h.call(msg);
  BOOST_CHECK_EQUAL(r.status_code,EVMC_SUCCESS);
}


BOOST_AUTO_TEST_CASE(weakhost_call_passed_nothing){
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);

  // An executor that always returns true
  unique_ptr<IEvmMsgExecutable> e{new mockedEvmMsgExec::A /*empty executor*/};

  // the host
  WeakEvmHost h{w,t,
                "aaa" /*host name*/,
                move(e) /*executor*/
  };

  auto [msg,code] = makeMsg("" /*code hex*/,
                            "aabbcc" /*data hex*/);
  evmc::Result r = h.call(msg);
  BOOST_CHECK_EQUAL(r.status_code,EVMC_SUCCESS);
}
BOOST_AUTO_TEST_SUITE_END();

BOOST_AUTO_TEST_SUITE(test_evmweak_executor);
BOOST_AUTO_TEST_CASE(test_basic_handleCreate){
  // 1. --------------------------------------------------
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // An executor that always returns true
  unique_ptr<IEvmMsgExecutable> e{new mockedEvmMsgExec::A /*empty executor*/};
  // 2. --------------------------------------------------
  // the host
  WeakEvmHost h{w,t, "aaa" /*host name*/,
                move(e) /*executor used for recursive call */};
  BOOST_REQUIRE(not e); // e is not nullptr


  // 3. --------------------------------------------------
  // The message
  address a1 = makeAddress(1);
  evmc_message msg{.kind=EVMC_CREATE,.flags=0}; // for now valid flags are only 0;
  msg.sender = msg.recipient = msg.code_address = a1;
  msg.gas = std::numeric_limits<int64_t>::max();
  // 🦜: In fact, a valid init code is just a code that returns something.
  // During contract creation, it dosen't check whether the generated code are some
  // valid EVM-optcodes.
  //
  // Below code that returns
  // "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef"

  // mstore(0,
  // 0xd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef)
  // return(0, 32)
  string init_hex =
    "7f" /*PUSH32*/
    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "60" /*PUSH1*/ "00"
    "52" /* mstore */
    "6020" /*PUSH1 20*/ "6000" /*PUSH1 00*/ "f3";
  bytes input = evmc::from_hex(init_hex).value();
  msg.input_data = input.data();
  msg.input_size = input.size();

  // 4. --------------------------------------------------
  // Deploy with evmweak
  unique_ptr<IEvmMsgExecutable> e1{new EvmweakMsgExecutor()};
  evmc::Result r = e1->execMsg(msg,h);

  // 5. --------------------------------------------------
  // Now the code is deployed, and the contract account is created.
  BOOST_CHECK_EQUAL(r.status_code, EVMC_SUCCESS);
  BOOST_CHECK_EQUAL(h.accounts.size(),1);
  BOOST_CHECK(h.accounts.contains(a1));
  // The expected created acn.
  Acn acn{0,evmc::from_hex("d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
                           "e0e1e2e3e4e5e6e7e8e9eaebecedeeef").value()};
  BOOST_CHECK_EQUAL(acn.toString(),h.accounts.find(a1)->second.toString());
  }


BOOST_AUTO_TEST_CASE(test_basic_handleCreate_nonce_increment){
  // 1. --------------------------------------------------
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // An executor that always returns true
  unique_ptr<IEvmMsgExecutable> e{new mockedEvmMsgExec::A /*empty executor*/};
  // 2. --------------------------------------------------
  // the host
  WeakEvmHost h{w,t, "aaa" /*host name*/,
                move(e) /*executor used for recursive call */};
  BOOST_REQUIRE(not e); // e is not nullptr

  // 3. --------------------------------------------------
  // The message
  address a1 = makeAddress(1);
  evmc_message msg{.kind=EVMC_CREATE,.flags=0}; // for now valid flags are only 0;
  msg.sender = msg.recipient = msg.code_address = a1;
  msg.gas = std::numeric_limits<int64_t>::max();
  // 🦜: In fact, a valid init code is just a code that returns something.
  // During contract creation, it dosen't check whether the generated code are some
  // valid EVM-optcodes.
  //
  // Below code that returns
  // "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef"

  // mstore(0,
  // 0xd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef)
  // return(0, 32)
  string init_hex =
    "7f" /*PUSH32*/
    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "60" /*PUSH1*/ "00"
    "52" /* mstore */
    "6020" /*PUSH1 20*/ "6000" /*PUSH1 00*/ "f3";
  bytes input = evmc::from_hex(init_hex).value();
  msg.input_data = input.data();
  msg.input_size = input.size();

  // 4. --------------------------------------------------
  // Deploy with evmweak
  unique_ptr<IEvmMsgExecutable> e1{new EvmweakMsgExecutor()};
  evmc::Result r = e1->execMsg(msg,h);

  // 5. --------------------------------------------------
  // Now the code is deployed, and the contract account is created.
  // BOOST_CHECK_EQUAL(r.status_code, EVMC_SUCCESS);
  // BOOST_CHECK_EQUAL(h.accounts.size(),1);
  // BOOST_CHECK(h.accounts.contains(a1));

  // 6. --------------------------------------------------
  // deploy another contract
  address a2 = makeAddress(2);
  msg.recipient = a2;
  r = e1->execMsg(msg,h);
  BOOST_CHECK_EQUAL(r.status_code, EVMC_SUCCESS);
  BOOST_CHECK_EQUAL(h.accounts.size(),2);
  BOOST_CHECK(h.accounts.contains(a2));

  // Note the increment nonce.
  // 🦜: They are not necessarily known since they may depends on other
  // testcase.
  BOOST_CHECK_EQUAL(h.accounts.find(a1)->second.nonce + 1,
                    h.accounts.find(a2)->second.nonce);
  }

/**
 * @brief Try to deploy a contract in hi.sol
 * the init byte code is generated from
 *   solc hi.sol --bin
 *
 * 🦜 although the deploy code from evmweak seems to be different from that
 * generated from
 *
 *   solc hi.sol --bin-runtime
 */
BOOST_AUTO_TEST_CASE(test_real_handleCreate){
  // 1. --------------------------------------------------
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // An executor that always returns true
  unique_ptr<IEvmMsgExecutable> e{new mockedEvmMsgExec::A /*empty executor*/};
  // 2. --------------------------------------------------
  // the host
  WeakEvmHost h{w,t, "aaa" /*host name*/,
                move(e) /*executor used for recursive call */};
  BOOST_REQUIRE(not e); // e is not nullptr


  // 3. --------------------------------------------------
  // The message
  address a1 = makeAddress(1);
  evmc_message msg{.kind=EVMC_CREATE,.flags=0}; // for now valid flags are only 0;
  msg.sender = msg.recipient = msg.code_address = a1;
  msg.gas = std::numeric_limits<int64_t>::max();

  string init_hex{"608060405234801561001057600080fd5b50610150806100206000396000f3fe"
                  "608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033"};
  string deploy = init_hex.substr(2 * 0x20); // The expected deploy code

  bytes input = evmc::from_hex(init_hex).value();
  msg.input_data = input.data();
  msg.input_size = input.size();

  // 4. --------------------------------------------------
  // Deploy with evmweak
  unique_ptr<IEvmMsgExecutable> e1{new EvmweakMsgExecutor()};
  evmc::Result r = e1->execMsg(msg,h);

  // 5. --------------------------------------------------
  // Now the code is deployed, and the contract account is created.
  BOOST_CHECK_EQUAL(r.status_code, EVMC_SUCCESS);
  BOOST_CHECK_EQUAL(h.accounts.size(),1);
  BOOST_CHECK(h.accounts.contains(a1));

  // Check the deploy code
  BOOST_CHECK_EQUAL(h.accounts.find(a1)->second.code,
                    evmc::from_hex(deploy).value());
  }

BOOST_AUTO_TEST_CASE(test_basic_handle_call){
  /*
    🦜: The code to be called can in fact be any code, it dosen't have to be
    take into account anything passed to the msg
   */

  // 1. --------------------------------------------------
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // An executor that always returns true
  unique_ptr<IEvmMsgExecutable> e{new mockedEvmMsgExec::A /*empty executor*/};
  // 2. --------------------------------------------------
  // the host
  WeakEvmHost h{w,t, "aaa" /*host name*/,
                move(e) /*executor used for recursive call */};
  BOOST_REQUIRE(not e); // e is not nullptr


  // 3. --------------------------------------------------
  // deploy a code that return "aabbcc" manually
  address a1 = makeAddress(1);
  string deploy_hex =
    "62" /*PUSH3*/
    "aabbcc"
    "60" /*PUSH1*/ "00"
    "52" /* mstore: store (padded-to-32)aabbcc at Mem[0] */
    "6020" /*PUSH1 32 = 20 in hex*/ "6000" /*PUSH1 00*/ "f3" /*return Mem[00:20]*/;
  Acn acn{0,evmc::from_hex(deploy_hex).value()};
  h.accounts[a1] = acn;


  // 4. --------------------------------------------------
  // The message
  evmc_message msg{.kind=EVMC_CALL,.flags=0}; // for now valid flags are only 0;
  msg.sender = msg.recipient = msg.code_address = a1;
  msg.gas = std::numeric_limits<int64_t>::max();

  //5. --------------------------------------------------
  // Execute to get the result
  unique_ptr<IEvmMsgExecutable> e1{new EvmweakMsgExecutor()};
  evmc::Result r = e1->execMsg(msg,h);
  // 5. --------------------------------------------------
  // Now the code is deployed, and the contract account is created.
  BOOST_CHECK_EQUAL(r.status_code, EVMC_SUCCESS);
  BOOST_CHECK_EQUAL(r.output_size, 0x20);
  BOOST_CHECK_EQUAL(evmc::hex(bytes_view{r.output_data,r.output_size}),
                    string(2*(32 - 3),'0') + "aabbcc"
                    //        ^^^^^^ Number of 0 bytes
                    //     ^ 2 chars for each byte in hex
                    );
}


BOOST_AUTO_TEST_CASE(test_real_handle_call_set){
  /*
    🦜: call the set() method on a contract compiled by solc
   */

  // 1. --------------------------------------------------
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // An executor that always returns true
  unique_ptr<IEvmMsgExecutable> e{new mockedEvmMsgExec::A /*empty executor*/};
  // 2. --------------------------------------------------
  // the host
  WeakEvmHost h{w,t, "aaa" /*host name*/,
                move(e) /*executor used for recursive call */};
  BOOST_REQUIRE(not e); // e is not nullptr


  // 3. --------------------------------------------------
  // the deploy code that expose set(uint256) and get() -> uint256
  address a1 = makeAddress(1);
  string deploy_hex =
    "608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033"
    ;

  Acn acn{0,evmc::from_hex(deploy_hex).value()};

  BOOST_TEST_MESSAGE("Deploying contract manually");
  h.accounts[a1] = acn;

  // 4. --------------------------------------------------
  // The message
  evmc_message msg{.kind=EVMC_CALL,.flags=0}; // for now valid flags are only 0;
  msg.sender = msg.recipient = msg.code_address = a1;
  msg.gas = std::numeric_limits<int64_t>::max();

  // set(uint256=123)
  BOOST_TEST_MESSAGE("EXEC set()--------------------------------------------------\n");
  evmc::bytes input = evmc::from_hex("60fe47b1000000000000000000000000000000000000000000000000000000000000007b").value();
  msg.input_data = input.data();
  msg.input_size = input.size();

  // before executing the msg, the account storage is empty
  BOOST_REQUIRE(h.accounts.find(a1)->second.storage.empty());

  //5. --------------------------------------------------
  // Execute to get the result
  unique_ptr<IEvmMsgExecutable> e1{new EvmweakMsgExecutor()};
  evmc::Result r = e1->execMsg(msg,h);
  // 5. --------------------------------------------------
  // Now the storage is set
  BOOST_REQUIRE_EQUAL(r.status_code, EVMC_SUCCESS);
  BOOST_REQUIRE(not h.accounts.find(a1)->second.storage.empty());
  // one entry is stored
  BOOST_CHECK_EQUAL(h.accounts.find(a1)->second.storage.size(),1);
}

BOOST_AUTO_TEST_CASE(test_real_handle_call_set_get){
  // 🦜: call set(123) and then get() on the contract compiled by solc
  // 1. --------------------------------------------------
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  // An executor that always returns true
  unique_ptr<IEvmMsgExecutable> e{new mockedEvmMsgExec::A /*empty executor*/};
  // 2. --------------------------------------------------
  // the host
  WeakEvmHost h{w,t, "aaa" /*host name*/,
                move(e) /*executor used for recursive call */};
  BOOST_REQUIRE(not e); // e is not nullptr


  // 3. --------------------------------------------------
  // the deploy code that expose set(uint256) and get() -> uint256
  address a1 = makeAddress(1);
  string deploy_hex =
    "608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033"
    ;

  Acn acn{0,evmc::from_hex(deploy_hex).value()};

  BOOST_TEST_MESSAGE("Deploying contract manually");
  h.accounts[a1] = acn;

  // 4. --------------------------------------------------
  // The message
  evmc_message msg{.kind=EVMC_CALL,.flags=0}; // for now valid flags are only 0;
  msg.sender = msg.recipient = msg.code_address = a1;
  msg.gas = std::numeric_limits<int64_t>::max();

  // set(uint256=123)
  BOOST_TEST_MESSAGE("EXEC set()--------------------------------------------------\n");
  evmc::bytes input = evmc::from_hex("60fe47b1000000000000000000000000000000000000000000000000000000000000007b").value();
  msg.input_data = input.data();
  msg.input_size = input.size();

  // before executing the msg, the account storage is empty
  BOOST_REQUIRE(h.accounts.find(a1)->second.storage.empty());

  //5. --------------------------------------------------
  // Execute to set the result
  unique_ptr<IEvmMsgExecutable> e1{new EvmweakMsgExecutor()};
  evmc::Result r = e1->execMsg(msg,h);
  // Now the storage is set
  BOOST_REQUIRE_EQUAL(r.status_code, EVMC_SUCCESS);
  BOOST_REQUIRE(not h.accounts.find(a1)->second.storage.empty());

  // get()
  input = evmc::from_hex("6d4ce63c").value();
  msg.input_data = input.data();
  msg.input_size = input.size();
  BOOST_TEST_MESSAGE("EXEC get()--------------------------------------------------\n");
  evmc::Result r2 = e1->execMsg(msg,h);
  BOOST_CHECK_EQUAL(r2.status_code,EVMC_SUCCESS);
  BOOST_CHECK_EQUAL(r2.output_size,32);

  uint8_t o[32];
  // for (int i=0;i<32;i++) o[i] = *(r2.output_data+i);
  std::copy_n(r2.output_data,32,o);
  // The value from get() is 123
  BOOST_CHECK_EQUAL(int(intx::be::load<intx::uint256,32>(o)),0x7b);
  BOOST_CHECK_EQUAL(0x7b,123);

}
BOOST_AUTO_TEST_SUITE_END();

BOOST_AUTO_TEST_SUITE(test_evmExecutor);
BOOST_AUTO_TEST_CASE(test_getStateChanges){
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  WeakEvmHost h{w,t, "aaa" /*host name*/};

  Acn
    acn1{123,evmc::from_hex("112233").value()},
    acn2{234,evmc::from_hex("445566").value()};

  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  h.accounts[a1] = acn1;
  h.accounts[a2] = acn2;

  h.selfdestruct(makeAddress(3), {} /*beneficiary*/);

  vector<StateChange> j = EvmExecutor::getStateChanges(h);
  BOOST_CHECK_EQUAL(j.size(),3);

  // two insertions and one deletion
  BOOST_CHECK(not j[0].del);
  BOOST_CHECK(not j[1].del);
  // 🦜: Because the map h.accounts is unordered, we can't be sure that acn1
  // comes first.

  BOOST_CHECK(j[2].del);
  BOOST_CHECK_EQUAL(j[2].k,addressToString(makeAddress(3)));
}


BOOST_AUTO_TEST_CASE(test_getStateChanges_hide){
  // the world state
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);
  WeakEvmHost h{w,t, "aaa" /*host name*/};

  Acn
    acn1{123,evmc::from_hex("112233").value()},
    acn2{234,evmc::from_hex("445566").value()};

  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  h.accounts[a1] = acn1;
  h.accounts[a2] = acn2;

  // 2 is dead
  h.selfdestruct(makeAddress(2), {} /*beneficiary*/);

  vector<StateChange> j = EvmExecutor::getStateChanges(h);
  BOOST_CHECK_EQUAL(j.size(),2);

  // two insertions and one deletion
  BOOST_CHECK(not j[0].del);
  BOOST_CHECK(j[1].del);
}

BOOST_AUTO_TEST_CASE(test_txToEvmcMsg_create){
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(0), data, 123/*nonce*/);

  evmc_message msg = EvmExecutor::txToEvmcMsg(t);
  BOOST_CHECK_EQUAL(msg.kind,EVMC_CREATE);
  BOOST_CHECK_EQUAL(bytes(msg.input_data, msg.input_size),data);

  // the to-be-deployed address is the nonce
  BOOST_CHECK_EQUAL(address(msg.recipient),Tx::getContractDeployAddress(t));
  BOOST_CHECK_EQUAL(address(msg.sender),t.from);
  }

BOOST_AUTO_TEST_CASE(test_txToEvmcMsg_to){
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  Tx t = Tx(makeAddress(1), makeAddress(2), data, 123/*nonce*/);

  evmc_message msg = EvmExecutor::txToEvmcMsg(t);
  BOOST_CHECK_EQUAL(msg.kind,EVMC_CALL);
  BOOST_CHECK_EQUAL(bytes(msg.input_data, msg.input_size),data);

  // the to-be-called address
  BOOST_CHECK_EQUAL(address(msg.recipient),address(msg.code_address));
  BOOST_CHECK_EQUAL(address(msg.recipient),t.to);
  BOOST_CHECK_EQUAL(address(msg.sender),t.from);
}


BOOST_AUTO_TEST_CASE(test_create_through_tx){
  mockedAcnPrv::A /*empty provider*/ mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;

  // code that returns a 32-bytes val
  string init_hex =
    "7f" /*PUSH32*/
    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "60" /*PUSH1*/ "00"
    "52" /* mstore */
    "6020" /*PUSH1 20*/ "6000" /*PUSH1 00*/ "f3";
  bytes data = evmc::from_hex(init_hex).value();
  Tx t = Tx(makeAddress(1), makeAddress(0), data, 123/*nonce*/);

  unique_ptr<ITxExecutable> e{new EvmExecutor()};
  auto o = e->executeTx(w,t);
  BOOST_REQUIRE(o);

  // 🦜 : Should have alraedy put the 32-bytes value to the deployed contract
  address a = Tx::getContractDeployAddress(t);
  auto [j,d] = o.value();
  // journal and data to be put into the tx receipt.
  BOOST_CHECK_EQUAL(j.size(),1);
  BOOST_CHECK_EQUAL(j[0].del,false);
  BOOST_CHECK_EQUAL(j[0].k,addressToString(a));

  // The created acn
  Acn acn;
  BOOST_REQUIRE(acn.fromString(j[0].v));
  // The deployed code
  BOOST_CHECK_EQUAL(acn.code,
                    evmc::from_hex("d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
                                            "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
                                            ).value()
                    );
}


BOOST_AUTO_TEST_CASE(test_call_through_tx){
  mockedAcnPrv::D mh;IAcnGettable*w = dynamic_cast<IAcnGettable*>(&mh);;
  /// A provider who only have Acn 0x1; which has code that returns
  /// aabbcc(padded to 32 bytes)

  // the supposed to be the abi-value
  bytes data = evmc::from_hex("aabb").value();
  Tx t = Tx(makeAddress(1), makeAddress(1), data, 123/*nonce*/);

  // Call the code at 0x1 which returns aabbcc
  unique_ptr<ITxExecutable> e{new EvmExecutor()};
  auto o = e->executeTx(w,t);
  BOOST_REQUIRE(o);

  /*
    🦜 :For now, we will re-insert the contract that got called, regardless of
    whether the storage of it is modified.
   */
  auto [j,d] = o.value();

  // journal and data to be put into the tx receipt.
  BOOST_CHECK_EQUAL(j.size(),1);

  /* 🦜 : The more important part for a CALL is perhaps the data returned*/
  BOOST_CHECK_EQUAL(d.size(),32);
  BOOST_CHECK_EQUAL(d,
                    evmc::from_hex("00000000000000000000000000000000"
                                   "00000000000000000000000000aabbcc"
                                   ).value()
                    );
}

/*
  ❌️ :  after running the following test, the tests that follows it cannot make block anymore?
  [Solved]: stack overflow, your tests are too big, making them separate executables solves the problem. 
 */

/**
 * @brief Real-world contract deployment and calling.
 *
 * In this test-case, we deploy a contract compiled from `solc` and call its
 * `get()` `set()` methods, all through tx.
 *
 * 🦜 : This is something we are bold enough to do after the previous two tests.
 */
BOOST_AUTO_TEST_CASE(test_real_contract_create_and_call){
  // shared_ptr<mockedAcnPrv::E> h{new mockedAcnPrv::E}; /* unordered_map based-provider (host) 🦜 :we need to apply journal to it*/
  // Make sure it's on the stack, otherwise it won't be managed by smart ptrs..
  /*
    Doing the folloing is catastrophic:

    ```
    mockedAcnPrv::E h;
    IAcnGettable* w{&h}; // try to manage object on stack.
    ```

    The compiler will not complain anything right away. It simply throw
    unknown location(0): fatal error: in "test_forPostExec/test_makeBlk2": signal: SIGABRT (application abort requested)
    later on
   */
  mockedAcnPrv::E h;
  IAcnGettable *w = dynamic_cast<IAcnGettable *>(&h);
  IWorldChainStateSettable* s = dynamic_cast<IWorldChainStateSettable*>(&h);
  /*
    🦜 : This is the correct way to upcast to an interface (in STACK).
   */

  unique_ptr<ITxExecutable> e{new EvmExecutor()};

  // 1. --------------------------------------------------
  // Deploy the contract
  string init_hex{"608060405234801561001057600080fd5b50610150806100206000396000f3fe"
                  "608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033"};
  bytes data = evmc::from_hex(init_hex).value();
  Tx t = Tx(makeAddress(1), makeAddress(0), data, 123/*nonce*/);
  auto o = e->executeTx(w,t);
  BOOST_REQUIRE(o);
  auto [j, res] = o.value();
  // apply the journal
  BOOST_REQUIRE(s->applyJournalStateDB(j));

  // The deployed address
  address a = Tx::getContractDeployAddress(t);

  // 2. --------------------------------------------------
  // Set the value
  // set(uint256=123)
  BOOST_TEST_MESSAGE("EXEC set(123)--------------------------------------------------\n");
  data = evmc::from_hex("60fe47b1000000000000000000000000000000000000000000000000000000000000007b").value();

  // make new tx
  t = Tx(makeAddress(1), a , data, 124/*nonce*/);
  o = e->executeTx(w,t);
  BOOST_REQUIRE(o);
  std::tie(j,res) = o.value();
  // apply the journal
  BOOST_REQUIRE(s->applyJournalStateDB(j));

  // 3.  --------------------------------------------------
  // Get the value
  BOOST_TEST_MESSAGE("EXEC get()--------------------------------------------------\n");
  data = evmc::from_hex("6d4ce63c").value();
  // make new tx

  t = Tx(makeAddress(1), a , data, 124/*nonce*/);
  o = e->executeTx(w,t);
  BOOST_REQUIRE(o);
  // // Get the result uint256
  // std::tie(j,res) = o.value();
  // BOOST_CHECK_EQUAL(res.size(),32);
  // BOOST_CHECK_EQUAL(res,evmc::from_hex(
  //                                      "00000000000000000000000000000000"
  //                                      "0000000000000000000000000000007b"
  //                                      ).value());

}

BOOST_AUTO_TEST_CASE(TPS_test_evmweak,MY_TEST_THIS){
  // 1. --------------------------------------------------
  evmc::MockedHost h;


  // The message --------------------------------------------------
  // address a1 = makeAddress(1);
  evmc_message msg{.kind=EVMC_CREATE,.flags=0}; // for now valid flags are only 0;
  msg.gas = std::numeric_limits<int64_t>::max();
  // msg.sender = msg.recipient = msg.code_address = a1;

  // // string init_hex = "";
  // // bytes input = evmc::from_hex(init_hex).value();
  // bytecode input{};
  // msg.input_data = input.data();
  // msg.input_size = input.size();

  // 3. --------------------------------------------------
  // Get ready
  using namespace std::chrono;
  time_point<high_resolution_clock> start, end;
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
  evmc::VM vm{evmc_create_evmweak()}; // evmc::VM automatically destroys the vm.

  const int N = 4;
  const int M = 15;
  start = high_resolution_clock::now(); // --------------------------------------------------
  // 4. --------------------------------------------------
  // Start executing
  for (int i=0;i<N;i++)
    std::jthread{[&](){
      for (int j=0;j<M;j++)
        vm.execute(h,EVMC_MAX_REVISION,msg, nullptr,0);
    }
        };

  // evmc::Result r = vm.execute(h,EVMC_MAX_REVISION,msg, nullptr,0);
  // Execute the empty code

  // 5. --------------------------------------------------
  // Get timming
  end = high_resolution_clock::now();    // --------------------------------------------------
  duration<double> elapsed_seconds = end - start;
  double s = elapsed_seconds.count();
  double tps = (N*M/s);
  BOOST_TEST_MESSAGE(format("sec elapsed: %4.2e sec, so TPS = %.0f") % s % tps);
  BOOST_CHECK(tps > 60'000);
  // 6. --------------------------------------------------
  // Now the code is deployed, and the contract account is created.
  // BOOST_CHECK_EQUAL(r.status_code, EVMC_SUCCESS);

  }


BOOST_AUTO_TEST_SUITE_END();
