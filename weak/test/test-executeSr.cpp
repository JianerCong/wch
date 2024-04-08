#include "h.hpp"
#include "txVerifier.hpp"
using namespace weak;


tuple<address,address,bytes> get_example_address_and_data(){
  // the txs
  address a1 = makeAddress(1);
  address a2 = makeAddress(2);
  bytes data(size_t{2},uint8_t{0xff}); // we would like this.
  return make_tuple(a1,a2,data);
}
Tx get_example_tx(){
  auto [a1,a2,data] = get_example_address_and_data();
  Tx t = Tx(a1,a2,data,123/*nonce*/);
  t.type = Tx::Type::data;
  return t;
}

/*
  🦜 : we test
  public tx_mode : ok
  public tx_mode : fail (bad signature)
  ca tx_mode : ok (cert ok, sig ok)
  ca tx_mode : fail (cert ok, bad sig)
  ca tx_mode : fail (bad cert, sig ok)
 */

BOOST_AUTO_TEST_CASE(test_verify_public_tx_mode){
  /*
    🦜 : To test the div2ExecutorSr.... We first need to generate a keypair.
   */
  using ::pure::SslMsgMgr;
  // 0. Generate a key pair
  pure::UniquePtr<EVP_PKEY> sk = ::pure::SslMsgMgr::new_key_pair();

  // 1. create a tx
  Tx t = get_example_tx();
  // 1.1 append the pk
  t.pk_pem = SslMsgMgr::dump_key_to_pem(sk.get(), false /* is_secret*/);
  t.from = t.getFromFromPkPem();                        // update the from to match pk
  // 1.2 sign the tx
  t.signature = bytesFromString(SslMsgMgr::do_sign(sk.get(), t.getToSignPayload()));

  // 2. up the div2ExecutorSr
  TxVerifier e;
  BOOST_REQUIRE(e.verify(t));

  // 2.1 🦜 : if the `from` is wrong, then the verification should also fail
  t.from = makeAddress(3);
  BOOST_REQUIRE(not e.verify(t));
  // 2.2 if signature is manipulated, fail.
  t.from = t.getFromFromPkPem();
  t.data = bytesFromString("manipulated data");
  BOOST_REQUIRE(not e.verify(t));
}

BOOST_AUTO_TEST_CASE(test_verify_ca_tx_mode){
  // 0. generate the ca keypair
  pure::UniquePtr<EVP_PKEY> ca_sk = ::pure::SslMsgMgr::new_key_pair();
  // 0.1 generate a user keypair
  pure::UniquePtr<EVP_PKEY> user_sk = ::pure::SslMsgMgr::new_key_pair();
  // 0.2 sign the user's pk to get the crt
  string crt = SslMsgMgr::do_sign(ca_sk.get(), SslMsgMgr::dump_key_to_pem(user_sk.get(), false /* is_secret*/));

  // 1. create a tx
  Tx t = get_example_tx();
  // 1.1 append the pk and sig
  t.pk_pem = SslMsgMgr::dump_key_to_pem(user_sk.get(), false /* is_secret*/);
  t.pk_crt = bytesFromString(crt);
  t.from = t.getFromFromPkPem();                        // update the from to match pk
  // 1.2 sign the tx
  t.signature = bytesFromString(SslMsgMgr::do_sign(user_sk.get(), t.getToSignPayload()));

  // 2. up the div2ExecutorSr and give it the ca pk
  TxVerifier e{SslMsgMgr::dump_key_to_pem(ca_sk.get(), false /* is_secret*/)};
  BOOST_REQUIRE(e.verify(t));
  // 2.1 🦜 : if we don't have a sig, then the verification should fail
  t.signature = bytes{};
  BOOST_REQUIRE(not e.verify(t));
}

/**
 * @brief Prepare a tx in public tx-mode
 */
Tx get_prepared_tx(EVP_PKEY* sk){
  Tx t = get_example_tx();

  t.pk_pem = SslMsgMgr::dump_key_to_pem(sk, false /* is_secret*/);
  t.from = t.getFromFromPkPem();                        // update the from to match pk
  // 1.2 sign the tx
  t.signature = bytesFromString(SslMsgMgr::do_sign(sk, t.getToSignPayload()));
  return t;
}

BOOST_AUTO_TEST_CASE(test_filter_txs){
  // 1. prepare the txs
  pure::UniquePtr<EVP_PKEY> user_sk = ::pure::SslMsgMgr::new_key_pair();
  vector<Tx> txs = {
    get_prepared_tx(user_sk.get()),
    get_prepared_tx(user_sk.get())
  };

  // 2. modify the second tx's nonce
  txs[1].nonce = 234;

  // 3. init the verifier
  TxVerifier e;
  e.filterTxs(txs);

  // 4. check that the second tx is removed
  BOOST_CHECK_EQUAL(txs.size(),1);
}
