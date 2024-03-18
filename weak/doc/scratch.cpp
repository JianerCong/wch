
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
      json::object r = this.invokePyMethod(json::object{{"method", "init"}},
                                           , py_code, abi_json,t,
                                            json::object{{}});
    }


    //  // 1.3 prepare the _storage
    // json::error_code ec;
    // json::value storage = json::parse(storage_json, ec);
    // if (ec or (not storage.is_object())) {
    //   return {{"error", json::string("Failed to parse the storage json `" +
    //                                  string(storage_json) + "`"
    //                                  )}};
    // }
  }else{
    // Contract call
  }
}

/**
 * @brief Invoke the `method` of a python-vm contract.
 *
 * @param invoke the invoke object, usually something like : {"method" : "hi",
 * "args" : {"name" : "Alice"}}. The `args` is optional.
 * 
 * @param py_code the contract's code
 * @param abi the contract's abi, key = method name, value = method's signature
 * @param t the transaction
 * @param storage_json the contract's storage
 *
 * @return the result of the method invocation. Usually something like :
 *  - {"result" : 1 "log" : "Here is some log printed by the methods"}
 *  - {"error" : "Some error message"}
 */
json::object invokePyMethod(json::object invoke, string_view py_code,
                            json::object abi,
                            const Tx & t,
                            json::object storage
                            ) const noexcept{
  if (not invoke.contains("method")) {
    return {{"error", json::string("Malformed invoke json: `method` not found in the invoke object")}};
  }

  string method = invoke["method"].as_string();
  if (not abi.contains(method)) {
    return {{"error",
               json::string("Malformed abi json: Method `" + string(method) + "` not found in the contract's abi")
      }};
  }

  try{
    // 0. prepare our working dir
    path wd = filesystem::current_path() / ".pyvm_aaa";
    // 0.1 create if not exists
    if (not filesystem::exists(wd)) {
      filesystem::create_directory(wd);
    }

    // 1. prepare the input for the python-vm
    //    1.0 get the argument list and start preparing the args
    json::array required_args = abi[method].as_array(); // 🦜 : we assume that abi is well-formed.
    json::object args;
    if (invoke.contains("args")) {
      if (not invoke["args"].is_object()) { // 🦜 : provided by the user, so we probably should check it
        return {{"error", json::string("invoke json: `args` should be an object")}};
      }
      args = invoke["args"].as_object();
    }

    /*
           1.1 Check if the required args are provided.
      🦜 : All args that doesn't start with `_` are required, and they should be matched excatly. (no more, no less)
    */
    int c = 0;
    for (const auto & a : required_args) {
      if (not a.is_string()) {
        return {{"error", json::string("Invalid abi: The required args should be strings")}};
      }

      string s = a.as_string();
      if (s[0] == '_') {
        continue;
      }

      if (not args.contains(s)) {
        return {{"error", json::string("Argument `" + string(a.as_string()) + "` is required")}};
      }
      c++;
    }

    // count (all args should be provided, no more, no less)
    if (c != args.size()) {
      return {{"error", json::string((format("Argument mismatch: Expected %d arguments, but got %d") % c % args.size()).str())}};
    }

    // 1.2 prepare the spacial args
    if (required_args.contains("_tx_context")) {
      json::object tx_ctx = {
        {"to", json::string(weak::addressToString(t.to))},
        {"from", json::string(weak::addressToString(t.from))},
        {"timestamp", json::number(t.timestamp)},
        {"hash", json::string(weak::hashToString(t.hash()))},
      };
      args["_tx_context"] = tx_ctx;
    }

    //    1.3 prepare the _storage
    if (required_args.contains("_storage")) {
      args["_storage"] = storage;
    }

    // 2. write the args to args.json
    using std::ios::trunc;
    using std::ios::out;
    path args_path = wd / "args.json";
    (ofstream(args_path.c_str(), out | trunc) << json::serialize(args)).flush();
    // trunc :: clear the file if it exists

    // 3. make the python script
    /*
      import json
      args = None
      with open('args.json', 'r') as f:
          args = json.load(f)
      r = METHOD(**args)
      with open('result.json', 'w') as f:
          json.dump(r, f)
     */
    string py_0 = py_code + "\n\n";
    string py_1 = PY_INVOKE_METHOD_SCRIPT;
    py_1 = replace(py_1.find("METHOD"), 6, method);

    string py_code_content = py_0 + py_1;
    // 3. Finally, invoke the python
    // static tuple<int,string> exec_py(string  py_code_content, const int timeout_s = 2,
    //                                  path wd = std::filesystem::temp_directory_path() // 🦜: let's keep it simple.

    auto [exit_code, output] = exec_py(py_code_content, 3, wd);
    if (exit_code != 0) {
      return {{"error", json::string("Python-vm exited with non-zero exit code: " + std::to_string(exit_code) + "Output: " + output)}};
    }

    // 4. 🦜 : read the result and return it
    path result_path = wd / "result.json";
    string result_json;
    ifstream(result_path.c_str()) >> result_json;
    json::error_code ec;
    json::value result = json::parse(result_json, ec);
    if (ec) {
      return {{"error", json::string("Failed to parse the result json `" + result_json + "`")}};
    }

    json::object r;             // the acutal result for the user = return val + log
    r.insert({"result", result});
    r.insert({"log", json::string(output)});

  }catch(const std::exception & e){
    return {{"error", json::string(e.what())}};
  }

}
