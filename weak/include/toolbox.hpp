/**
 * @file pure-toolbox.hpp
 * @author Jianer Cong
 * @brief The cmdline tool box
 */

#pragma once

#include "net/pure-netAsstn.hpp" // for ssl stuff
#include "core.hpp"

using std::endl;
using std::cout;
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

    std::ifstream ifs(p.c_str(), std::ios::binary);
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
    static constexpr const char * const help = R"---(
Usage :
   wch toolbox --help
   wch toolbox -h
   wch toolbox new-keypair <out-pk.pem> <out-sk.pem>
   wch toolbox do-sign <sk.pem> <msg.txt> <out-sig.bin>
   wch toolbox do-verify <pk.pem> <msg.txt> <sig.bin>
   wch toolbox tx-sign <tx.json> <sk.pem> <crt.sig> <out-tx.json>
   wch toolbox tx-sign-no-crt <tx.json> <sk.pem> <out-tx.json>

🦜 : No argument is optional, and all arguments in <...> are paths. The args
starting with `out-` are generated files, otherwise the file must exist.
)---";
  public:
    static void new_key_pair(path out_pk, path out_sk){
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
      writeToFile(out_sig_p, sig, true /* binary*/);
    }

    static bool do_verify(path pk_p, path msg_p, path sig_p){
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

    static void tx_sign(path tx_p, path sk_p, optional<path> crt_sig_p, path out_tx_p){
      BOOST_LOG_TRIVIAL(debug) << "🦜 : tx_sign called with:" S_CYAN
                               << "\n\ttx_p: " << tx_p
                               << "\n\tsk_p: " << sk_p
                               << "\n\tcrt_sig_p: " << (crt_sig_p.has_value() ? crt_sig_p.value() : "nullopt")
                               << "\n\tout_tx_p: " << out_tx_p << S_NOR
        ;
      // 1. load the secret key
      optional<pure::UniquePtr<EVP_PKEY>> osk = SslMsgMgr::load_key_from_file(sk_p, true /* is_secret*/);
      BOOST_ASSERT_MSG(osk.has_value(), "Failed to load the secret key");
      pure::UniquePtr<EVP_PKEY> sk = std::move(osk.value());

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
      BOOST_LOG_TRIVIAL(debug) << "🦜 : parsed json: " << jv ;
      BOOST_ASSERT(jv.is_object());
      json::object o = jv.as_object();
      BOOST_ASSERT(o.contains("data"));
      BOOST_ASSERT(o.contains("nonce"));

      string data = json::value_to<string>(o["data"]);uint64_t nonce = json::value_to<uint64_t>(o["nonce"]);
      /*
        2.2 if .type is present, we check it. The `data` field is in hex unless `.type == "python" | "data"`.
      */
      json::string type = o.contains("type") ? o["type"].as_string() : "";
      if (type == "" or type == "evm"){
        optional<bytes> ob = evmc::from_hex(data);
        if (not ob)
          BOOST_THROW_EXCEPTION(std::runtime_error("The data field for an evm Tx should be in hex"));
        data = weak::toString(ob.value());
      }

      // 3. get the signature
      // static string getToSignPayload0(const uint64_t nonce, const bytes & data) noexcept{
      string sig = SslMsgMgr::do_sign(sk.get(), Tx::getToSignPayload0(nonce, data));

      // 4. add the fields to the json object:
      // .signature (hex), .pk_pem (string), .pk_crt (optional hex)
      o["signature"] = evmc::hex(weak::bytesFromString(sig));
      o["pk_pem"] = SslMsgMgr::dump_key_to_pem(sk.get(), false /* is_secret*/);
      if (crt_sig_p){
        string crt_sig = read_file(crt_sig_p.value().string());
        o["pk_crt"] = evmc::hex(weak::bytesFromString(crt_sig));
      }

      // 5. write the json object to the file
      writeToFile(out_tx_p, json::serialize(o));
    }

#define ARGV_SHIFT()  { argc--; argv++; }
    static int run(int argc, char* argv[]){
      try{
        // Skip `executable name` and the `toolbox` keyword
        ARGV_SHIFT();
        ARGV_SHIFT();

        if (!strcmp("--help", argv[0]) or !strcmp("-h", argv[0])){
          cout << Toolbox::help << endl;
        }

        // switch on the method:
        if (!strcmp("new-keypair", argv[0])){
          BOOST_ASSERT_MSG(argc == 2, "new-keypair requires 2 arguments: <out-pk.pem> <out-sk.pem>");
          new_key_pair(argv[1], argv[2]);
        }else if (!strcmp("do-sign", argv[0])){
          BOOST_ASSERT_MSG(argc == 3, "do-sign requires 3 arguments: <sk.pem> <msg.txt> <out-sig.bin>");
          do_sign(argv[1], argv[2], argv[3]);
        }else if (!strcmp("do-verify", argv[0])){
          BOOST_ASSERT_MSG(argc == 3, "do-verify requires 3 arguments: <pk.pem> <msg.txt> <sig.bin>");
          if (do_verify(argv[1], argv[2], argv[3]))
            std::exit(0);
          else
            std::exit(1);
        }else if (!strcmp("tx-sign", argv[0])){
          BOOST_ASSERT_MSG(argc == 4, "tx-sign requires 4 arguments: <tx.json> <sk.pem> <crt.sig> <out-tx.json>");
          tx_sign(argv[1], argv[2], argv[3], argv[4]);
        }else if (!strcmp("tx-sign-no-crt", argv[0])){
          BOOST_ASSERT_MSG(argc == 3, "tx-sign-no-crt requires 3 arguments: <tx.json> <sk.pem> <out-tx.json>");
          tx_sign(argv[1], argv[2], std::nullopt, argv[3]);
        }else{
          cout << "Unknown command: " << argv[0] << endl;
        }
        std::exit(0);
      } catch (std::exception & e){
        cout << "Error: " << e.what() << endl;
        std::exit(1);
      }
    }
  };
}
