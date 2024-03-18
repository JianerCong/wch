
/**
 * @brief Execute a python-vm transaction
 *
 * @return similar to other execute?Tx functions, but the python-vm would only
 * modify the account of its own address. So if the tx is a contract call, it's
 * `t.to`. If it's a contract creation, it's `Tx::getContractDeployAddress(t)`.
 */
optional<tuple<vector<StateChange>,bytes>> executePyTx(IAcnGettable * const w, const Tx & t) const noexcept{
  if (evmc::is_zero(t.to)) {
    // Contract creation

    // 0. Verify the python-vm contract, if every thing went well, a json
    // representation of the contract abi would be returned.
    string_view py_code{t.data.data(), t.data.size()}; // byte -> string
    optional<string> abi = verifyPyContract(py_code);
    if (not abi) {
      BOOST_LOG_TRIVIAL(debug) <<  "❌️ Failed to verify python-vm contract" S_RED << py_code << S_NOR;
      return {};
    }

    // 1. Deploy the contract
    //   1.1 new account
    /*
      🦜 : For python-vm, let's store the following at `disk_storage`
      [0] : abi.json
      [1] : storage.json
    */
    Acn a{t.nonce, t.data};
    // 1.2 store the abi
    // store the abi
    a.disk_storage.push_back(abi.value());

    // 1.3 parse the abi from json
    json::object abi_json = json::parse(abi.value()).as_object();

    // 1.3 execute the `init` function if it exists, pass it an empty storage
    if (abi_json.contains("init")) {
      json::object r = this.invokePyMethod("init", py_code, abi_json,t, "");
    }
  }else{
    // Contract call
  }
}

/**
 * @brief Invoke the `method` of a python-vm contract.
 *
 * @param method the method to invoke, must be a member of the contract's abi
 * @param py_code the contract's code
 * @param abi the contract's abi, key = method name, value = method's signature
 * @param t the transaction
 * @param storage_json the contract's storage
 *
 * @return the result of the method invocation. Usually something like :
 *  - {"result" : 1 "log" : "Here is some log printed by the methods"}
 *  - {"error" : "Some error message"}
 */
json::object invokePyMethod(string_view method, string_view py_code, json::object abi, const Tx & t, string_view storage_json) const noexcept{
  if (not abi.contains(method)) {
    return {{"error",
               json::string("Method `" + string(method) + "` not found in the contract's abi")
      }};
  }

  // 0. prepare our working dir
  path wd = filesystem::current_path() / ".pyvm_aaa";
  // 0.1 create if not exists
  if (not filesystem::exists(wd)) {
    filesystem::create_directory(wd);
  }

  // 1. prepare the input for the python-vm
  //    1.1 prepare the _tx_context, and write that to `tx_context.json` (clear the file if it exists)
  json::object tx_ctx = {
    {"to", json::string(weak::addressToString(t.to))},
    {"from", json::string(weak::addressToString(t.from))},
    {"timestamp", json::number(t.timestamp)},
    {"hash", json::string(weak::hashToString(t.hash()))},
  };
  (ofstream(wd / "tx_context.json") << json::stringify(tx_ctx)).flush();

  //    1.2 prepare the storage
  (ofstream(wd / "storage.json") << storage_json).flush();

  // 2. make the python script
}
