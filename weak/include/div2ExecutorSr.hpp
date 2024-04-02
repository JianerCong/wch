
/**
 * @file div2Executor.hpp
 * @author Jianer Cong
 * @brief The Diverse2Executor and colleagues.
 *
 * 🦜 : This executor extends the EvmExecutor to support the new Tx types. 2 means
 * it's kinda newer than the EvmExecutor
 */

#pragma once
#include "div2Executor.hpp"

#include "intx/intx.hpp"
#include "net/pure-netAsstn.hpp"
namespace weak {
  using ::pure::SslMsgMgr;

  /**
   * @brief The Div2Executor that checks the validity of the Tx. [2024-01-22]
   *
   * 🐢 : This executor can do SSL stuff such
   *  1. checking the `Tx.signature` of the tx
   *  2. deriving the `Tx.from` address from the `Tx.pk_pem`. (🦜 : In fact, we don't have to do this, we just need to make sure check that `t.from` == `t.getFromFromPkPem()`)
   *  3. checking `Tx.pk_crt` if needed.
   *
   * This is to really make the chain serious.
   *
   * 🐢 : So this is kinda a `Div2Executor` + `Tx` verifyer. So when this
   * executor is used, two tx_modes are possible:
   *
   *     1. public tx_mode : every body can generate key pairs, and use it to
   *     send tx.
   *
   *     2. ca tx_mode : only keys signed by `the ca` can send tx.
   *
   *     When operating in tx_mode 2, the `ca_crt_pem` argument needs to be passed
   *     to the ctor, otherwise, it's tx_mode 1.
   *
   *  🦜 : So how does this work?
   *
   *  🐢 : Just like the other executors, it exposes only one function :
   *  `executeTx`. But in fact it's just like a firewall, it checks wether the
   *  tx is valid, if so, it calls `Div2Executor::executeTx`
   *
   * @see Tx in `core.hpp` and its notes on [2024-01-22]
   */
  class SeriousDiv2Executor : public Div2Executor{
  public:
    ::pure::UniquePtr<EVP_PKEY> ca_public_key;
    const bool is_public_tx_mode;
    SeriousDiv2Executor(const string & ca_crt_pem = ""): is_public_tx_mode(ca_crt_pem.empty()){
      /*
        🦜 : What's the difference between `const string &` and `string_view` ?
        🐢 : `string_view` can only get `.data()`, but `const string &` can get `.c_str()`
       */
      if (not ca_crt_pem.empty()) {
        BOOST_LOG_TRIVIAL(debug) <<  S_GREEN "\t📗️ SeriousDiv2Executor started in ca tx_mode."  S_NOR;
        optional<pure::UniquePtr<EVP_PKEY>> o = SslMsgMgr::load_key_from_pem(ca_crt_pem, false /* is_sk*/);
        if (not o){
          BOOST_THROW_EXCEPTION(std::runtime_error("Failed to load the ca public key from the pem:" +
                                                   ca_crt_pem));
        }
        ca_public_key = std::move(o.value()); // move
      }else{
        BOOST_LOG_TRIVIAL(debug) <<  S_GREEN "\t📗️ SeriousDiv2Executor started in public tx_mode."  S_NOR;
      }
    }

    /**
     * @brief Verify a Tx
     * @param t the tx to verify
     *
     * When operating in public tx_mode, we check that
     *
     * 1. `t.from` == `t.getFromFromPkPem()`
     * 2. `do_verify(t.pk,t.nonce + t.data,t.signature)`
     *
     * In addition, when operating in ca tx_mode, we also check that:
     *
     * 0. `do_verify(ca_public_key,tx.pk_pem,tx.pk_crt)`
     */
    bool verify(const Tx & t)const {
      // 1. check the pk if we are in ca tx_mode

      if (not this->is_public_tx_mode) {
        if (not SslMsgMgr::do_verify(ca_public_key.get(), t.pk_pem, toString(t.pk_crt))) {
          BOOST_LOG_TRIVIAL(info) <<  "\t Error verifying the tx.pk_crt";
          return false;
        }
      }

      // 2. check that t.from == t.getFromFromPkPem()
      if (t.from != t.getFromFromPkPem()) {
        BOOST_LOG_TRIVIAL(info) <<  "❌️ Malformed tx: t.from != t.getFromFromPkPem()";
        return false;
      }

      // 3. check the signature
      // 🦜 : t.nonce is uint64, we first transform it to string
      // Method 1: the hard way, let's do a cast:
      // --------------------------------------------------
      /*
        uint8_t dst[8];
        intx::be::store<uint64_t>(dst,t.nonce);
        // 🦜 : Let's turn that to an std::string
        string n(reinterpret_cast<const char *>(&(dst[0])),8);
      */
      // Method 2: 🦜 : Wait, why don't we just use toString?
      // Now let's check
      if (not SslMsgMgr::do_verify(t.pk_pem,
                                   t.getToSignPayload(),
                                   toString(t.signature))){
        BOOST_LOG_TRIVIAL(debug) << "❌️ Invalid signature";
        return false;
      }
      return true;
    } // verifyTx

    /**
     * @brief Execute a Tx
     * @param t the tx to execute
     *
     * This function first verifies the tx, if it's valid, it calls
     * `Div2Executor::executeTx`
     */

    optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                         const Tx & t) const noexcept override{
      if (not this->verify(t)){
        return {};
      }
      return Div2Executor::executeTx(w,t); // call the base
    }
  };  // SeriousDiv2Executor
}     // weak
