
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

    // 1.3 execute the `init` function if it exists
  }else{
    // Contract call
  }
}
