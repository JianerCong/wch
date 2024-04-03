#include "h.hpp"
#include "toolbox.hpp"
using namespace weak;

BOOST_AUTO_TEST_CASE(test_hi){
  BOOST_CHECK_EQUAL(1,1);
}

BOOST_AUTO_TEST_CASE(test_new_key_pair){
  path out_pk, out_sk;
  out_pk = filesystem::temp_directory_path() / "pk.pem";
  out_sk = filesystem::temp_directory_path() / "sk.pem";

  BOOST_REQUIRE(not filesystem::exists(out_pk));
  BOOST_REQUIRE(not filesystem::exists(out_sk));

  Toolbox::new_key_pair(out_pk, out_sk);

  BOOST_REQUIRE(filesystem::exists(out_pk));
  BOOST_REQUIRE(filesystem::exists(out_sk));

  // see the content
  string pk_pem = readAllText(out_pk);
  string sk_pem = readAllText(out_sk);
  BOOST_REQUIRE(not pk_pem.empty());
  BOOST_REQUIRE(not sk_pem.empty());
  BOOST_TEST_MESSAGE("🦜 : Generated pk pem: " + pk_pem);
  BOOST_TEST_MESSAGE("🦜 : Generated sk pem: " + sk_pem);

  // clean up
  filesystem::remove(out_pk);
  filesystem::remove(out_sk);
}

BOOST_AUTO_TEST_CASE(test_do_sign){
  path pk_p,sk_p, msg_p, out_sig_p;
  pk_p = filesystem::temp_directory_path() / "pk.pem";
  sk_p = filesystem::temp_directory_path() / "sk.pem";
  msg_p = filesystem::temp_directory_path() / "msg.txt";
  out_sig_p = filesystem::temp_directory_path() / "sig.bin";

  // 1. generate key pair
  Toolbox::new_key_pair(pk_p, sk_p);

  // 2. write a message
  weak::writeToFile(msg_p, "aaa");

  // 3. sign the message
  Toolbox::do_sign(sk_p, msg_p, out_sig_p);

  // 4. check the signature
  BOOST_REQUIRE(filesystem::exists(out_sig_p));
  string sig = readAllText(out_sig_p);
  BOOST_REQUIRE(not sig.empty());

  // 5. verify
  bool is_valid = Toolbox::do_verify(pk_p, msg_p, out_sig_p);
  BOOST_CHECK(is_valid);

  // 5.1 if we change the message, the signature should be invalid
  weak::writeToFile(msg_p, "bbb");
  is_valid = Toolbox::do_verify(pk_p, msg_p, out_sig_p);
  BOOST_CHECK(not is_valid);

  // clean up
  filesystem::remove(pk_p);
  filesystem::remove(sk_p);
  filesystem::remove(msg_p);
  filesystem::remove(out_sig_p);
}

void prepare_json_file(path p, json::object o){
  weak::writeToFile(p, json::serialize(o));
}
BOOST_AUTO_TEST_CASE(test_tx_sign){
  // path tx_p, sk_p, pk_p, crt_sig_p, out_tx_p;
  std::unordered_map<string, path> m;
  m["tx_p"] = filesystem::temp_directory_path() / "tx.json";
  m["sk_p"] = filesystem::temp_directory_path() / "sk.pem";
  m["pk_p"] = filesystem::temp_directory_path() / "pk.pem";
  m["crt_sig_p"] = filesystem::temp_directory_path() / "crt.sig";
  m["out_tx_p"] = filesystem::temp_directory_path() / "out-tx.json";

  // 1. generate key pair
  Toolbox::new_key_pair(m["pk_p"], m["sk_p"]);

  // 2. write a transaction
  json::object o;
  o["data"] = "aabb";           // a valid hex
  o["nonce"] = 123;             // uint64_t
  prepare_json_file(m["tx_p"], o);

  // 3. prepare a crt (in raw bytes)
  weak::writeToFile(m["crt_sig_p"], "\xaa\xbb\xcc", true /* binary*/);

  // 3. sign the transaction
  Toolbox::tx_sign(m["tx_p"], m["sk_p"], m["crt_sig_p"], m["out_tx_p"]);
  BOOST_REQUIRE(filesystem::exists(m["out_tx_p"]));

  // 4. read the signed transaction
  json::object o2 = json::parse(readAllText(m["out_tx_p"])).as_object();
  BOOST_TEST_MESSAGE("🦜 : Signed transaction: " + json::serialize(o2));
  // the `.signature`, `.pk_pem`, `.pk_crt` is there.
  BOOST_REQUIRE(o2.contains("signature"));
  BOOST_REQUIRE(o2.contains("pk_pem"));
  BOOST_REQUIRE(o2.contains("pk_crt"));

  // --------------------------------------------------
  // 🦜 : we can also sign it with no crt
  filesystem::remove(m["out_tx_p"]);
  Toolbox::tx_sign(m["tx_p"], m["sk_p"], {} , m["out_tx_p"]);
  BOOST_REQUIRE(filesystem::exists(m["out_tx_p"]));
  json::object o3 = json::parse(readAllText(m["out_tx_p"])).as_object();
  BOOST_REQUIRE(o3.contains("pk_pem"));
  BOOST_REQUIRE(not o3.contains("pk_crt"));

  // clean up
  for (auto [k,v] : m){
    filesystem::remove(v);
  }
}

