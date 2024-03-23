/**
 * @file evmExecutor.hpp
 * @author Jianer Cong
 * @brief the EVMExecutor and friends.
 *
 * Responsible for executing EVM transaction. The main class is `EvmExecutor`,
 * the rest are helpers.
 */
#pragma once
#include "core.hpp"
// #include "storageManager.hpp"

#include <set>

#include <evmweak/evmweak.h>      // for evmc_create_evmweak
#include<tuple> // for tuple

namespace weak {
  // {{{ Interfaces for executor

  class WeakEvmHost;            // forward declaration
  /**
   * @brief Type that execute a given msg on a given host.
   *
   * An instance that satisfies this interface is passed to WeakEvmHost in order
   * to handle recursive `call()`.
   * @see WeakEvmHost()
   */
  class IEvmMsgExecutable {
  public:
    virtual evmc::Result execMsg(const evmc_message & msg,
                                 WeakEvmHost &  h) const noexcept=0;
  };

  // }}}



  // {{{ class WeakEvmHost

  class WeakEvmHost: public evmc::Host{
  private:
    IAcnGettable * const readOnlyWorldState;
    const Tx & tx;

  public:

    /**
     * @brief Ask for the holding IAcnGettable an Acn
     * @param a The address of Acn to ask for
     * @return whether the Acn is found successfully.
     *
     * This method should be called when the given address `a` is not found in
     * `this->accounts` and we have to resort to the underlying
     * `readOnlyWorldState`. The method checks wether the address exists in the
     * underlying `readOnlyWorldState`. If so, add that to `this->account`.
     *
     * 🦜: So, after calling this, if this returns true, you still should access
     * the account from this->accounts like
     *
     * ```c++
     * if(tryAddAcnFromWorldState(a))
     *   // Acn is not found, should be accessible from `this->accounts`
     *   Acn acn = this->accounts.at(a);
     *   // do sth with acn
     * ```
     *
     * If anything wrong happened (address not found, bad serialization)
     * happened, the method returns false.
     */
    bool tryAddAcnFromWorldState(const address & a, bool fatal = false) noexcept {
      BOOST_LOG_TRIVIAL(info) << format("Trying to fetch Acn from IAcnGettable\n\t"
                                        S_CYAN " addr: %s" S_NOR) % a;
      auto v = this->readOnlyWorldState->getAcn(a);
      if (v){
        BOOST_LOG_TRIVIAL(info) <<
          format("✅️ " S_GREEN "Found Acn in IAcnGettable: nonce=%d" S_NOR)
          % v.value().nonce;
        // add to the accounts, copy ctor
        this->accounts[a] = v.value();
        return true;
      }
      BOOST_LOG_TRIVIAL(info) << format(S_MAGENTA "Not found in IAcnGettable" S_NOR);
      if (fatal){
        BOOST_LOG_TRIVIAL(error) << format( S_RED "❌️ Fatal, it's required" S_NOR);
        exit(1);
      }
      return false;
    };

    /**
     * @brief The constructor
     *
     * @param w The underlying readonly stateDB.
     * @param t The Tx to be executed
     *
     * @param ex The "backend" evmc_message executor. This member is used when
     * the evmc-host-method `call()` is called. This executor is responsible for
     * recursive call e.g.
     *  - contract creates contract
     *  - contract calls contract
     *
     *  If not provided, any `call()` to this host will return EVMC_SUCCESS with
     *  output specified in `this->output`
     */
    explicit WeakEvmHost(IAcnGettable * const w, const Tx & t,
                         string n="WeakEvmHost",
                         IEvmMsgExecutable  * const ex = nullptr):
      readOnlyWorldState(w),tx(t),name(n),msgExe(ex){
      BOOST_LOG_TRIVIAL(info) << format("🎍 Initializing WeakEvmHost for "
                                        S_CYAN "tx-%d" S_NOR) % t.nonce;
    };

    // <! The evmc_message executor. @see WeakEvmHost()
    IEvmMsgExecutable const * const msgExe;
    string name;                // <! The name of the host, mostly cosmetic
    unordered_map<address, Acn> accounts; //<! the In-RAM Acns
    std::set<address> deadAcn;  //<! The acn that called self-destruct
    /* 🦜 : we are able to use this because we defined the std::less operator in
       core.hpp. */

    // <! the mocked-execution output for testing. @see WeakEvmHost()
    bytes output{0xaa,0xbb,0xcc};


    // {{{ evm host methods
    /**
     * @brief Check wether an account exists.
     *
     * Check using the following logic:
     *  - If the account is dead (selfdestructed), return false
     *  - If the account is in local or underlying WorldState, return ture
     *  - Else not found.
     */
    bool account_exists(const address& a) const noexcept override{
      BOOST_LOG_TRIVIAL(debug) << format("Checking wether Acn " S_GREEN "%s" S_NOR " exists")
        % a;

      // Checking the dead accounds (address that have self-destructed).
      /* If an Acn is dead, we require that the same address cannot be used
          during this transaction execution*/
      if (this->deadAcn.contains(a)){
        BOOST_LOG_TRIVIAL(info) << format("Acn already dead 🪦 (found in selfdestruct list)");
        return false;
      }

      if (accounts.contains(a)){
        BOOST_LOG_TRIVIAL(debug) << format("Found in existing accounts");
        return true;
      }

      if (this->readOnlyWorldState->getAcn(a)){
        BOOST_LOG_TRIVIAL(debug) << format("Found in underlying WorldState");
        return true;
      }

      BOOST_LOG_TRIVIAL(trace) <<
        format( S_MAGENTA "Not found in local accounts of size %d and world state" S_NOR)
        % this->accounts.size();

      return false;
    }
    /**
     * @brief Get the account's storage value at the given key (EVMC Host method).
     *
     * 🦜 : !? This method is also const? So we can't fetch from underlying
     * statedb within this too.
     *
     * 🐢 : Yes, that's why we need to fetch Acn in the constructor of this Host.
     *
     * 🦜 : So what Acn should be fetched? sender and recipient?
     *
     * 🐢 : Fair. @see WeakEvmHost()
     */
    bytes32 get_storage(const address& a, const bytes32& key) const noexcept override{
      BOOST_LOG_TRIVIAL(debug) << format("get storage called with:\n" S_CYAN
                                         "\taddr: \t%1%\n"
                                         "\tkey: \t%2%\n" S_NOR)
        % a % key;
      /*
        🦜: thanks to operation<< in core.hpp, we can now display int256 and
        address now, instead of:
        % intx::to_string<256>(intx::be::load<intx::uint<256>,address>(a),16)
        % intx::to_string<256>(intx::be::load<intx::uint<256>,bytes32>(key),16);
       */
      if (not account_exists(a))
        return {};

      if (not accounts.contains(a)){
        // not local but in underlying
        BOOST_LOG_TRIVIAL(debug) << format("Accessing underlying Acn storage");
        Acn acn = this->readOnlyWorldState->getAcn(a).value();
        if (not acn.storage.contains(key)){
          BOOST_LOG_TRIVIAL(debug) << format(" storage Key %s" S_RED " not found" S_NOR
                                             ". This acn has %d storage entr%s")
            % key % acn.storage.size() % pluralizeOn(acn.storage.size(),"y","ies");
          return {};
        }
        return acn.storage.at(key);
      }

      // if (!accounts.contains(a) || !(accounts[a].storage.contains(key)))
      // Acn acn = accounts.find(a)->second;
      Acn acn = accounts.at(a);
      if (not (acn.storage.contains(key))){
        BOOST_LOG_TRIVIAL(debug) << format(" storage Key %s" S_RED " not found" S_NOR
                                           ". This acn has" S_CYAN " %d " S_NOR " storage entr%s")
          % key
          % acn.storage.size()
          % pluralizeOn(acn.storage.size(),"y","ies");
        return {};
      }

      // return accounts[a].storage[key];
      return acn.storage.at(key);
    }
    /// Set the account's storage value (EVMC Host method).
    evmc_storage_status set_storage(const address& a,
                                    const bytes32& key,
                                    const bytes32& value) noexcept override{
      BOOST_LOG_TRIVIAL(debug) << format("set storage called with:\n"
                                         S_CYAN
                                         "\taddr: \t%1%\n"
                                         "\tkey: \t%2%\n"
                                         "\tvalue: \t%3%\n" S_NOR)
        % a % key % value;
      /*<!  Equiv to
         % intx::be::load<intx::uint<256>,address>(a)
         % intx::be::load<intx::uint<256>,bytes32>(key)
          % intx::be::load<intx::uint<256>,bytes32>(value);

          See @get_storage()
       */

      if (not accounts.contains(a)){
        BOOST_LOG_TRIVIAL(debug) << format("Setting storage of non-local Acn");
        if (tryAddAcnFromWorldState(a)){
          BOOST_LOG_TRIVIAL(debug) << format("Target acn fetched");
        }else{
          BOOST_LOG_TRIVIAL(debug) << format("Target acn not found, setting storage of new Acn");
        }
      }

      // create the account if necessary
      accounts[a].storage[key] = value;

      return evmc::is_zero(value) ? EVMC_STORAGE_DELETED : EVMC_STORAGE_MODIFIED;
    }
    evmc::uint256be get_balance(const address& addr) const noexcept override{
      return {};                  // no balance
    }

    /// Copy the account's code to the given buffer (EVMC host method).
    size_t copy_code(const address& a,
                     size_t code_offset,
                     uint8_t* buffer_data,
                     size_t buffer_size) const noexcept override{
      BOOST_LOG_TRIVIAL(trace) << format("Copying code from "
                                         S_CYAN "addr: %s" S_NOR) % a;
      if (not account_exists(a))
        return 0;

      // const auto& code = accounts[a].code;
      const auto& code = accounts.at(a).code;

      if (code_offset >= code.size()) return 0;

      const auto n = std::min(buffer_size, code.size() - code_offset);
      if (n > 0)
        std::copy_n(&code[code_offset], n, buffer_data);
      return n;
    }

    /// Selfdestruct the account (EVMC host method).
    bool selfdestruct(const address& addr,
                      const address& /*beneficiary ignored*/) noexcept override{
      // 🦜: Not writing variable name is an old convention to tell a compiler
      // that you don't actually need the variable.
      BOOST_LOG_TRIVIAL(debug) << format("Address " S_CYAN "%s" S_NOR " selfdestructed")
        % addr;
      this->deadAcn.insert(addr);
      return true;
    }

    /// Call/create other contract (EVMC host method).
    /// 🦜 : Creates the result from the provided arguments.
    ///
    /// @param _status_code  The status code.
    /// @param _gas_left     The amount of gas left.
    /// @param _gas_refund   The amount of refunded gas.
    /// @param _output_data  The pointer to the output.
    /// @param _output_size  The output size.
    evmc::Result call(const evmc_message& msg) noexcept override{
      BOOST_LOG_TRIVIAL(debug) << format("⚙️  evmc-host-method: " S_BLUE "call()" S_NOR "invoked");
      logMsg(msg);

      if (this->msgExe){
        BOOST_LOG_TRIVIAL(debug) << format( S_GREEN "passing to IEvmMsgExecutable" S_NOR );
        return this->msgExe->execMsg(msg,
                                     *this /*pass this host*/
                                     );
      }
      BOOST_LOG_TRIVIAL(debug) << format(S_MAGENTA "No IEvmMsgExecutable provided " S_NOR
                                         ",returning default result.");
      evmc_status_code sc = EVMC_SUCCESS; // or EVMC_FAILURE
      int64_t gas_left= std::numeric_limits<int64_t>::max(),gas_refund=0;
      return evmc::Result{sc,gas_left,gas_refund,&output[0],output.size()};
    }

    /// Get transaction context (EVMC host method).
    /*
      🐢 : During the execution of a transaction, the evm might ask for the tx
      context, which includes
      ```c
        struct evmc_tx_context {
          evmc_uint256be tx_gas_price;
          evmc_address tx_origin;
          evmc_address block_coinbase;
          int64_t block_number;
          int64_t block_timestamp;
          int64_t block_gas_limit;
          evmc_uint256be block_prev_randao;
          evmc_uint256be chain_id;
          evmc_uint256be block_base_fee;
        };
      ```

      🦜 : But in our system (mainly due to our consensus mechanism such as RBFT), we might
      execute the transaction before making it commit it to the chain, right? So
      at that point, we don't known things like `block_number` yet...

      🐢 : Yeah, but at least we should provide the appropriate tx_origin.
      🦜 : true.
     */
    evmc_tx_context get_tx_context() const noexcept override {return {};}
    /// Get the block header hash (EVMC host method).
    evmc::bytes32 get_block_hash(int64_t /*block_number*/) const noexcept override{return {};}

    /// Emit LOG (EVMC host method).
    void emit_log(const address& /*a*/, const uint8_t* /*data*/, size_t /*data_size*/,
                  const bytes32* /*topics*/, size_t /*topics_count*/) noexcept override{}

    /// Record an account access.
    ///
    /// This method is required by EIP-2929 introduced in ::EVMC_BERLIN. It will record the account
    /// access in MockedHost::recorded_account_accesses and return previous access status.
    /// This methods returns ::EVMC_ACCESS_WARM for known addresses of precompiles.
    /// The EIP-2929 specifies that evmc_message::sender and evmc_message::recipient are always
    /// ::EVMC_ACCESS_WARM. Therefore, you should init the MockedHost with:
    ///
    ///     mocked_host.access_account(msg.sender);
    ///     mocked_host.access_account(msg.recipient);
    ///
    /// The same way you can mock transaction access list (EIP-2930) for account addresses.
    ///
    /// @param addr  The address of the accessed account.
    /// @returns     The ::EVMC_ACCESS_WARM if the account has been accessed before,
    ///              the ::EVMC_ACCESS_COLD otherwise.
    evmc_access_status access_account(const address& /*addr*/) noexcept override{
      return EVMC_ACCESS_COLD;
    }
    evmc_access_status access_storage(const address& /*addr*/, const bytes32& /*key*/)
      noexcept override{
      return EVMC_ACCESS_COLD;
    }
    /// Get the account's code size (EVMC host method).
    size_t get_code_size(const address& a) const noexcept override{
      BOOST_LOG_TRIVIAL(debug) << "get_code_size() called\n";
      if (!accounts.contains(a)) return 0;
      // C++ is not that flexible to accept the following:
      // return accounts[a].code.size();
      return accounts.at(a).code.size();
    }
    /// Get the account's code hash (EVMC host method).
    bytes32 get_code_hash(const address& a) const noexcept override{
      BOOST_LOG_TRIVIAL(debug) << "get_code_hash() called\n";
      if (!accounts.contains(a)) return {};
      // return accounts[a].codehash();

      return hash256ToBytes32(accounts.at(a).codehash());
    }

    // }}}

  private:
    static bytes32 hash256ToBytes32(const ethash::hash256 & from){
      // hash256 -> bytes32
      bytes32 to;
      std::copy_n(std::cbegin(from.bytes),
                  32,
                  std::begin(to.bytes));
      return to;
    }
  };

  // }}} WeakEvmHost

  // {{{ class EvmweakMsgExecutor

  /**
   * @brief The IEvmMsgExecutable based on evmweak.
   *
   * There are important logic wrapped in the method `execMsg()`, namely how to
   * call and create contract according to an `evmc_message`.
   *
   * In case of creating contract, the data in the message is supposed to be the
   * `init` code, which is supposed to be executed in the EVM to get actual
   * `code`, which is to be posted on the contract account.
   *
   * In case of calling contract, the data in the message is supposed to be the
   * ABI data. And the code is taken from `msg.code_address` which is supposed
   * to dispatch some contract method based on the given ABI data.
   */
  class EvmweakMsgExecutor: public virtual IEvmMsgExecutable {
  public:
    //<! Const variable
    const static int64_t gas_left= std::numeric_limits<int64_t>::max(),gas_refund=0;

    // The interface method
    evmc::Result execMsg(const evmc_message & msg,
                         WeakEvmHost &  h)
      const noexcept override{
      BOOST_LOG_TRIVIAL(debug) <<
        format("❄ WeakEvmMsgExecutor started executing msg");
      logMsg(msg);

      // check CREATE or CALL
      if (msg.kind >= EVMC_CREATE)
        return handleCreate(msg,h);
      else
        return handleCall(msg,h);
    };

    // --------------------------------------------------
    // if create: input = code <- data (tx.data or msg.data)

    static evmc::Result handleCreate(const evmc_message & msg,
                                     WeakEvmHost & h) noexcept{
      static uint64_t nonce = 0; // <! The nonce for the new contract acn.

      evmc::Result r{EVMC_FAILURE,gas_left,gas_refund,nullptr,0};

      BOOST_LOG_TRIVIAL(debug) << format( S_CYAN " ⚙️  Creating " S_NOR "contract");

      // 1. --------------------------------------------------
      // check acn exist
      /*
        🦜: When creating new contract, create the new account and post the code
        on it. If the addr exists already , return failure.
      */
      if (h.account_exists(msg.recipient)){
        BOOST_LOG_TRIVIAL(error) << format("❌️ address %s already exists, so cannot be used to create contract.")
          % address(msg.recipient);
        return r;
      }

      // 2. --------------------------------------------------
      // execute te code to get `init` code
      // init = data
      evmc::bytes_view init{msg.input_data,msg.input_size};
      // Execute the init
      evmc::VM vm{evmc_create_evmweak()}; // evmc::VM automatically destroys the vm.
      r = vm.execute(h,EVMC_MAX_REVISION,msg, init.data(),init.size());

      if (r.status_code != EVMC_SUCCESS){
        BOOST_LOG_TRIVIAL(error) << format(S_RED "❌️ Error executing contract init, got status %s" S_NOR)
          % evmc::to_string(r.status_code);
        return r;
      }

      BOOST_LOG_TRIVIAL(info) << format("Got result code of size %d") % r.output_size;
      bytes code{r.output_data,r.output_size};
      BOOST_LOG_TRIVIAL(debug) << format("✅️ Contract Creation:\n\t"
                                         "contract addr: " S_CYAN "%s" S_NOR"\n\t"
                                         "nonce: " S_CYAN "%d" S_NOR"\n\t"
                                         "code to deploy:" S_CYAN "\n\t\t%s" S_NOR)
        % address(msg.recipient) % nonce % code;

      // 3. --------------------------------------------------
      // create account for recipient, and put the code on it in one step
      h.accounts[msg.recipient] = Acn{nonce,code};
      nonce++;                  // <! increment for the next acn.

      BOOST_LOG_TRIVIAL(debug) << format( S_GREEN "Contract creation finished" S_NOR);
      // return result
      return r;
    }

    // --------------------------------------------------
    // else (=call): input <- data; code <- code_address.code
    // execute and return result
    static evmc::Result handleCall(const evmc_message & msg,
                                   WeakEvmHost & h) noexcept{
      BOOST_LOG_TRIVIAL(debug) << format( S_CYAN " ⚙️   Calling" S_NOR " contract");
      evmc::Result r{EVMC_FAILURE,gas_left,gas_refund,nullptr,0};

      address a{msg.code_address};

      // 1. --------------------------------------------------
      // Check whether code address exist
      if (not h.account_exists(a)){
        BOOST_LOG_TRIVIAL(error) << format("❌️ code addr %s dosen't exist")
          % a;
        return r;
      }

      // 2. --------------------------------------------------
      // get the code
      if (not h.accounts.contains(a))
        h.tryAddAcnFromWorldState(a,true /*must success*/);

      bytes_view code = h.accounts.at(a).code;
      //         = h.accounts[a].code
      BOOST_LOG_TRIVIAL(debug) << format("Got: \n\t" S_CYAN
                                         "contract code size: %d \n\t"
                                         "contract code: %s \n\t" S_NOR
                                         ) % code.size() % code;

      // 3. --------------------------------------------------
      // execute and return
      BOOST_LOG_TRIVIAL(debug) << format( S_GREEN "Start executing code" S_NOR);
      evmc::VM vm{evmc_create_evmweak()};
      return vm.execute(h,EVMC_MAX_REVISION,msg, code.data(),code.size());
    }

  };

  // }}} EVMExecutor

  /**
   * @brief The executor that use `WeakEvmHost` and `EvmweakMsgExecutor`
   */
  class EvmExecutor: public virtual ITxExecutable{
  public:
    optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                         const Tx & t) const noexcept override{
      // 1. --------------------------------------------------
      // Init the host
      EvmweakMsgExecutor eh;
      IEvmMsgExecutable  * ex = dynamic_cast<IEvmMsgExecutable*>(&eh);
      WeakEvmHost h{w /* the underlying stateDB */
                    ,t /* tx */
                    , "aaa" /*host name*/,
                    // move(unique_ptr<IEvmMsgExecutable>(
                    //                                    new EvmweakMsgExecutor()
                    //                                    )
                    ex
                    /*executor used for recursive call */};

      // 2. --------------------------------------------------
      // Convert tx to msg
      evmc_message msg = txToEvmcMsg(t);

      // 3. --------------------------------------------------
      // exec the msg in evmweak, pass the host, it might change its state
      evmc::Result r = ex->execMsg(msg,h);

      // 4. --------------------------------------------------
      // if execution goes well, read the state-changes in host and the final result
      if (r.status_code != EVMC_SUCCESS)
        return {};
      return make_tuple(getStateChanges(h),
                        bytes{r.output_data,r.output_size});
    };

    /**
     * @brief Generate a evmc_message from Tx
     * 
     * @param t The native transaction, ready to be exported into the EVM world.
     * If `t.to` is empty than deploy the contract to the address specified in
     * according to Tx::getContractDeployAddress(t).
     *
     */
    static evmc_message txToEvmcMsg(const Tx & t) noexcept{
      BOOST_LOG_TRIVIAL(debug) << format("Converting tx-%d to evmc msg") % t.nonce;

      // The message 🦜: order of fields matters
      evmc_message msg{
        .kind=EVMC_CALL,
        .flags=0,
        .depth=0,
        .gas = std::numeric_limits<int64_t>::max(),
        .sender=t.from
      }; // for now valid flags are only 0;

      if (evmc::is_zero(t.to)){
        BOOST_LOG_TRIVIAL(debug) << format("Tx is " S_CYAN "CREATE" S_NOR);
        msg.kind = EVMC_CREATE;
        msg.recipient = msg.code_address = Tx::getContractDeployAddress(t);

      }else{
        BOOST_LOG_TRIVIAL(debug) << format("Tx is " S_CYAN "CALL" S_NOR);
        msg.recipient = msg.code_address = t.to;
      }
      // put in the data
      msg.input_data = t.data.data();
      msg.input_size = t.data.size();

      BOOST_LOG_TRIVIAL(debug) << format("The result msg:\n\t") ;
      logMsg(msg);

      return msg;
    }
    /**
     * @brief Obtain the state change in the host
     *
     * @param h The changes origin. This host must have gone through some msg
     * calls which changed its state such as creating/modifying/deleting some acn.
     *
     * @return the state changes (the "journal")
     *
     * The following procedure is used:
     *  for each address A in the h.accounts:
     *      add a SET(A,h.accounts[A]) if A is not in h.deadAcn
     *  for each address A in h.deadAcn
     *      add a DEL(A)
     */
    static vector<StateChange> getStateChanges(const WeakEvmHost & h) noexcept{
      vector<StateChange> j;
      BOOST_LOG_TRIVIAL(debug) << format("Getting state changes from host which has\n\t\t"
                                         S_GREEN " %d local account%s " S_NOR
                                         "and" S_RED " %d dead account%s" S_NOR
                                         ) % h.accounts.size() % pluralizeOn(h.accounts.size())
        % h.deadAcn.size() % pluralizeOn(h.deadAcn.size());

      // Add SET
      for (const auto&[addr,acn] : h.accounts){
        if (not h.deadAcn.contains(addr)){
          BOOST_LOG_TRIVIAL(debug) << format("Adding live acn " S_BLUE "%d" S_NOR " to journal") % acn.nonce;
          j.push_back(StateChange{.del=false,
                                  .k=addressToString(addr),
                                  .v=acn.toString()});
        }else{
          BOOST_LOG_TRIVIAL(debug) << format("Ignoring dead acn " S_BLUE "%d" S_NOR) % acn.nonce;
        }

      };

      int n = j.size();

      // Add DELETE
      for (const address & a : h.deadAcn){
        BOOST_LOG_TRIVIAL(debug)
          << format("Deleting dead addr " S_BLUE "%d" S_NOR "to journal") % a;
        j.push_back(StateChange{.del=true,
                                .k=addressToString(a),
                                .v=""});
      }

      BOOST_LOG_TRIVIAL(debug) << format("📚 Generated journal has\n\t"
                                         S_GREEN "%d insertion%s" S_NOR " and "
                                         S_RED "%d deletion%s" S_NOR ", so in total "
                                         S_CYAN "%d entr%s" S_NOR
                                         )
        %  n % pluralizeOn(n)
        %  (j.size() - n) % pluralizeOn(j.size() - n)
        %  j.size() % pluralizeOn(j.size(),"y","ies") ;

      return j;
    };
  }; // class EvmExecutor
}
