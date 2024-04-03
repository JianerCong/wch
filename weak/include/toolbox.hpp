/**
 * @file pure-toolbox.hpp
 * @author Jianer Cong
 * @brief The cmdline tool box
 */

#pragma once

#include "net/pure-netAsstn.hpp" // for ssl stuff
#include "core.hpp"

namespace weak{
  using pure::SslMsgMgr;
  /**
   * @brief Read the content of a file.
   *
   * @param path The path to the file.
   * @return The content of the file.
   */
  string read_file(const string & path){
    filesystem::path p(path);
    if (not filesystem::exists(p)){
      BOOST_THROW_EXCEPTION(std::runtime_error((format("File %s does not exist") % path).str()));
    }

    ifstream ifs(p.c_str(), std::ios::binary);
    if (not ifs.is_open()){
      BOOST_THROW_EXCEPTION(std::runtime_error((format("File %s cannot be opened") % path).str()));
    }

    char * buffer;
    // get the length of the file
    // ifs.seekg(0, ifs.end);
    // size_t length = ifs.tellg();
    // ifs.seekg(0, ifs.beg);
    size_t length = filesystem::file_size(p);
    // 🦜 : Yeah, the smart file_size() is better.

    // allocate memory:
    buffer = new char [length];

    // read data as a block:
    ifs.read(buffer,length);
    // close
    ifs.close();
    string o(buffer,length);
    delete[] buffer;

    return o;
  }

  class Toolbox {
    const char * help = R"---(
Usage :
   wcm toolbox --help
   wcm toolbox -h
   wch toolbox new-keypair <out-pk.pem> <out-sk.pem>
   wch toolbox do-sign <sk.pem> <msg.txt> <out-sig.bin>
   wch toolbox do-verify <pk.pem> <msg.txt> <sig.bin>
   wch toolbox tx-sign <tx.json> <sk.pem> <crt.sig> <out-tx.json>
   wch toolbox tx-sign-no-crt <tx.json> <sk.pem> <out-tx.json>

🦜 : No argument is optional, and all arguments in <...> are paths. The args
starting with `out-` are generated files, otherwise the file must exist.
)---";
  public:
    static void new_keypair(path out_pk, path out_sk){
      // 1. generate a new keypair
      pure::UniquePtr<EVP_PKEY> sk = SslMsgMgr::new_key_pair();
      weak::writeToFile(out_sk, SslMsgMgr::dump_key_to_pem(sk.get(), true /* is_secret*/));
      weak::writeToFile(out_pk, SslMsgMgr::dump_key_to_pem(sk.get(), false /* is_secret*/));
    }

    static void do_sign(path sk_p, path msg_p, path out_sig_p){
      // 1. load the secret key
      optional<pure::UniquePtr<EVP_PKEY>> o = SslMsgMgr::load_key_from_file(sk_p, true /* is_secret*/);
      BOOST_ASSERT_MSG(o.has_value(), "Failed to load the secret key");
      pure::UniquePtr<EVP_PKEY> sk = std::move(o.value());

      // 2. read the message and sign it
      string msg = read_file(msg_p);
      string sig = SslMsgMgr::do_sign(sk.get(), msg);

      // 3. write the signature to the file
      writeToFile(out_sig_p, sig);
    }

    bool do_verify(path pk_p, path msg_p, path sig_p){
      // 1. load the public key
      optional<pure::UniquePtr<EVP_PKEY>> o = SslMsgMgr::load_key_from_file(pk_p, false /* is_secret*/);
      BOOST_ASSERT_MSG(o.has_value(), "Failed to load the public key");
      pure::UniquePtr<EVP_PKEY> pk = std::move(o.value());

      // 2. read the message and signature
      string msg = read_file(msg_p);
      string sig = read_file(sig_p);

      // 3. verify the signature
      bool is_valid = SslMsgMgr::do_verify(pk.get(), msg, sig);
      if (is_valid){
        cout << "The signature is valid" << endl;
      } else {
        cout << "The signature is invalid" << endl;
      }
      return is_valid;
    }

    void tx_sign(path tx_p, path sk_p, optional<path> crt_sig_p, path out_tx_p){
      // 1. load the secret key
      optional<pure::UniquePtr<EVP_PKEY>> o = SslMsgMgr::load_key_from_file(sk_p, true /* is_secret*/);
      BOOST_ASSERT_MSG(o.has_value(), "Failed to load the secret key");
      pure::UniquePtr<EVP_PKEY> sk = std::move(o.value());

      // 2. read the transaction
      string tx_json_s = read_file(tx_p);
      /*
        2.1 🦜 : parse the json object, it should contain the fields
        .data
        .nonce
        and optionally
        .type = "" | "evm" | "data" | "python"
       */
      json::value jv = json::parse(tx_json_s);
      BOOST_ASSERT(jv.is_object());
      BOOST_ASSERT(jv.has_field("data"));
      BOOST_ASSERT(jv.has_field("nonce"));

      string data = jv["data"].as_string();
      uint64_t nonce = jv["nonce"].as_uint64();
      /*
        2.2 if .type is present, we check it. The `data` field is in hex unless `.type == "python" | "data"`.
       */
      string type = jv.has_field("type") ? jv["type"].as_string() : "";
      if (type == "" or type == "evm"){
        optional<bytes> o = evmc::from_hex(data);
        if (not o)
          BOOST_THROW_EXCEPTION(std::runtime_error("The data field for an evm Tx should be in hex"));
        data = weak::toString(o.value());
      }

      // 3. get the signature
      // static string getToSignPayload0(const uint64_t nonce, const bytes & data) noexcept{
      string sig = SslMsgMgr::do_sign(sk.get(), Tx::getToSignPayload0(nonce, data));

      // 4. add the fields to the json object:
      // .signature (hex), .pk_pem (string), .pk_crt (optional hex)
      json::object o;
      o["signature"] = evmc::hex(weak::bytesFromString(sig));
      o["pk_pem"] = SslMsgMgr::dump_key_to_pem(sk.get(), false /* is_secret*/);
      if (crt_sig_p){
        string crt_sig = read_file(crt_sig_p.value().string());
        o["pk_crt"] = evmc::hex(weak::bytesFromString(crt_sig));
      }
    }

    void run(int argc, char* argv[]){ }
  };
}
