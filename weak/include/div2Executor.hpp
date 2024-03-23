/**
 * @file div2Executor.hpp
 * @author Jianer Cong
 * @brief The Diverse2Executor and colleagues.
 *
 * 🦜 : This executor extends the EvmExecutor to support the new Tx types. 2 means
 * it's kinda newer than the EvmExecutor
 */

#pragma once
#include "evmExecutor.hpp"


#if defined(WITH_PYTHON)
#define BOOST_PROCESS_NO_DEPRECATED 1
#include <boost/process.hpp>

#include <thread>
using std::this_thread::sleep_for;
namespace bp =  boost::process;

#endif


namespace weak{


  /**
   * @brief The ExoticTxExecutorBase that can execute Txs using the magic of command line.
   *
   */
  class ExoticTxExecutorBase {
  public:
    /**
     * @brief Wait for a child process to finish, and terminate it if it's still running
     *
     * @param c the child process
     * @param timeout the time to wait for the child to finish
     *
     * @return true if the child is timed out and terminated, false otherwise
     */
    static bool wait_for_and_terminate(bp::child & c, int timeout_s = 3){
      if ((not c.running()) or timeout_s <= 0) {
        c.wait();                     // to avoid a zombie process & get the exit node
        return false;
      }

      for (int i = 0; i < timeout_s * 1000; ++i) {
        if (c.running()) {
          sleep_for(std::chrono::milliseconds(1));
          if ((i + 1) % 200 == 0) BOOST_LOG_TRIVIAL(debug) << format("Count down %d") % i;
        } else {
          // finishedm to mark it, we set timeout_s to -1
          timeout_s = -1;
        }
      }
      // BOOST_TEST_MESSAGE((format("for-loop exited with timeout_s = %d") % timeout_s).str());

      if (timeout_s > 0) {
        c.terminate();
        c.wait();                     // to avoid a zombie process & get the exit node
        return true;
      }

      c.wait();                     // to avoid a zombie process & get the exit node
      return false;
    }

    /**
     * @brief A function to execute a command and return the output
     */
    static tuple<int,string> exec0(string cmd, const int timeout_s = 2,
                                   filesystem::path wd = filesystem::current_path() // 🦜: let's keep it simple.
                                   ) {
      //get a handle to the current environment
      auto env = boost::this_process::environment();
      //change the current wording directory
      env["PWD"] = wd.string();

      bp::ipstream out, err;
      // on windows, resort to pwsh
#if defined(_WIN32)
      cmd = "powershell -NoProfile -NonInteractive -NoLogo -Command \"& {" + cmd + "}\"";
#endif

      bp::child c(cmd, bp::std_out > out, bp::std_err > err, env);
      /*
        🦜 : Let's just collect both stdout and stderr, and then we can log them.
      */

      string output;
      std::jthread listener([&]{
        string line;
        while (c.running() and out and std::getline(out, line) and !line.empty()) {
          // BOOST_TEST_MESSAGE((format("🦜 recieved: %s") % line).str());
          output += line;
        }
        // BOOST_LOG_TRIVIAL(trace) <<  "\tListener thread exited";
      });

      if (wait_for_and_terminate(c, timeout_s))
        return make_tuple(-1, "timed out");
      /*
        🦜 : In fact, we can get some logs here, but for now let's just ignore it
      */

      BOOST_LOG_TRIVIAL(debug) <<  "\tGot stderr(): " << err.rdbuf();
      BOOST_LOG_TRIVIAL(debug) <<  "\tGot stdout(): " << output;
      std::ostringstream eo;
      eo << err.rdbuf();        // read the error output

      return make_tuple(c.exit_code(), output + eo.str()); // joined here
    }
  };                            // class ExoticTxExecutorBase

#if defined(WITH_PYTHON)
  class PyTxExecutor : public ExoticTxExecutorBase {
  public:
    static tuple<int,string> exec_py(string  py_code_content, const int timeout_s = 2,
                                     path wd = std::filesystem::temp_directory_path() // 🦜: let's keep it simple.
                                     ) {
      BOOST_LOG_TRIVIAL(debug) <<  "exec_py entered";

      // path tmp = std::filesystem::current_path() / "tmp-folder";
      // // create the folder if it does not exist
      // if (not filesystem::exists(tmp)) {
      //   // BOOST_LOG_TRIVIAL(debug) <<  "preparing the tmp folder";
      //   if (not filesystem::create_directory(tmp)){
      //     return make_tuple(-1, "failed to create the tmp folder");
      //   }
      // }

      // 2. prepare the python file
      path p = wd / "hi-tmp.py";

      // BOOST_LOG_TRIVIAL(debug) <<  "writing to the file";
      (ofstream(p.c_str()) << py_code_content).flush();

      // 3. execute
      return ExoticTxExecutorBase::exec0("python3 -I " + p.string(), timeout_s,
                                         wd);
    }


    /**
     * @brief Invoke the verifier.py
     */
    static optional<string> verifyPyContract(const string  py_code){
      // 1. prepare the wd
      path wd = prepareWorkingDir();
      // 2. write the py_code to hi.py
      path p = wd / "hi.py";
      writeToFile(p, py_code);
      // 3. just invoke the PY_VERIFIER_CONTENT
      auto [exit_code, output] = exec_py(PY_VERIFIER_CONTENT, 5, wd);

      // 🦜 : If it failed, it throws
      if (exit_code != 0) {
        BOOST_LOG_TRIVIAL(info) <<  "❌️ Failed to verify python-vm contract:\n" << S_RED << output << S_NOR;
        return {};
      }

      // On success, it should produces verifier-result.json.
      path r = wd / "verifier-result.json";
      BOOST_ASSERT( filesystem::exists(r) );

      // the abi is produced
      return readAllText(r);
    }

    /**
     * @brief Execute a python-vm transaction
     *
     * @param w the world
     * @param t the transaction, it's expected that `t.type` is `Tx::Type::py`
     *
     * @return similar to other execute?Tx functions, but the python-vm would only
     * modify the account of its own address. So if the tx is a contract call, it's
     * `t.to`. If it's a contract creation, it's `Tx::getContractDeployAddress(t)`.
     */
    static optional<tuple<vector<StateChange>,bytes>> executePyTx(IAcnGettable * const w, const Tx & t) noexcept{
      /*
        🦜 : the jounral, but in our case only one account will be touched. The
        deployed contract's account.
      */
      if (evmc::is_zero(t.to)) {
        StateChange s;
        auto [a, res] = deployPyContract(t);
        s.k = weak::addressToString(Tx::getContractDeployAddress(t));
        s.v = a.toString();
        // return the single state change (the deployed account)
        return make_tuple(vector<StateChange>{s}, res);
      }else{
        // BOOST_LOG_TRIVIAL(debug) <<  "0. get the invoke object";
        json::error_code ec;
        json::value jv = json::parse(weak::toString(t.data), ec);
        if (ec or (not jv.is_object())) {
          BOOST_LOG_TRIVIAL(info) <<  "❌️ Failed to parse the invoke object: " << ec.message() << S_RED << weak::toString(t.data) << S_NOR;
          return {};
        }

        // BOOST_LOG_TRIVIAL(debug) <<  "0.1. verify the invoke object";
        json::object invoke = jv.as_object();
        if ((not invoke.contains("method")) or
            (not invoke["method"].is_string())){
          BOOST_LOG_TRIVIAL(info) <<  "❌️ Malformed invoke object: " << S_RED << json::serialize(invoke) << S_NOR;
          return {};
        }

        // BOOST_LOG_TRIVIAL(debug) <<  "1. try get the account";
        optional<Acn> ao = w->getAcn(t.to);
        if (not ao){
          BOOST_LOG_TRIVIAL(info) <<  "❌️ Failed to invoke python-vm contract: The account doesn't exist" S_RED
                                  << weak::addressToString(t.to) << S_NOR;
          return {};
        }

        Acn a = ao.value();
        auto [ao1, res] = invokePyContract(a, invoke, t);
        if (not ao1) {
          return make_tuple(vector<StateChange>{}, res);
        }

        // Acn modified, if it's different from the original, we should return it
        StateChange s;
        s.del = false;
        s.k = weak::addressToString(t.to);
        s.v = ao1.value().toString();
        return make_tuple(vector<StateChange>{s}, res);
      }
      return {};                 // 🦜 : Should not happen
    }

    static tuple<Acn, bytes> deployPyContract(const Tx & t){
      bytes res;

      // Contract creation

      // 0. Verify the python-vm contract, if every thing went well, a json
      // representation of the contract abi would be returned.
      string py_code = weak::toString(t.data); // byte -> string
      optional<string> abi = verifyPyContract(py_code);
      if (not abi) {
        BOOST_LOG_TRIVIAL(info) <<  "❌️ Failed to verify python-vm contract" S_RED << py_code << S_NOR;
        return {};
      }

      // 1. Deploy the cntract
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

      // 1.3 execute the `init` function if it exists, pass it an empty storage, this should init the storage
      if (abi_json.contains("init")) {
        json::object storage = {};
        json::object r = invokePyMethod(json::object{{"method", "init"}}
                                               , py_code, abi_json,t,
                                               storage);
          if (r.contains("quit")) {
            BOOST_LOG_TRIVIAL(info) << "❌️ Failed to execute the `init` method of the python-vm contract" << S_RED << r << S_NOR;
            return {};
          }

          // 1.4 store the storage (if modified by init)
          if (r.contains("storage")) {
            a.disk_storage.push_back(json::serialize(r["storage"]));
            // remove the "storage" key from the result
            r.erase("storage");
          }else{
            a.disk_storage.push_back("{}"); // empty storage
          } // 1.5 The acn is ready, let's store it. (🦜 : and also return the result ? 🐢 : Nope, we just need to prepare the `res` and `s`
          res = weak::bytesFromString(json::serialize(r));
        }else{
          // no init method, 🦜 : Here we do init an empty storage
          a.disk_storage.push_back("{}");
        }
      return make_tuple(a, res);
    } // deployPyContract

    /**
     * @brief Invoke a python method
     *
     * @return <Acn, result>. If Acn is not modified, it's an empty optional.
     */
    static tuple<optional<Acn>, bytes> invokePyContract(Acn a /* copy */ , const json::object & invoke, const Tx & t){
        // Contract call

        string_view msg0 = "❌️ Failed to invoke python-vm contract: ";
        string msg = S_RED + weak::addressToString(t.to) + S_NOR;

        // BOOST_LOG_TRIVIAL(debug) <<  "1. check the disk";
        if (a.disk_storage.size() < 2) { // abi + storage
          BOOST_LOG_TRIVIAL(info) << msg0  << "The account doesn't have the disk_storage " << msg;
          return {};
        }

        // BOOST_LOG_TRIVIAL(debug) <<  "2. get and parse the abi";
        json::error_code ec;
        json::value jv = json::parse(a.disk_storage[0], ec);
        if (ec or not jv.is_object()) {
          BOOST_LOG_TRIVIAL(info) <<  msg0 << "Failed to parse the abi: " << ec.message() << msg;
          return {};
        }

        json::object abi = jv.as_object();

        // BOOST_LOG_TRIVIAL(debug) <<  "3. if the method is not found in the abi, return";
        // 3. if the method is not found in the abi, return
        if (not abi.contains(invoke.at("method").as_string())) {
          BOOST_LOG_TRIVIAL(info) <<  msg0 << "Method " << invoke.at("method")
                                  << "not found" << msg;
          return {};
        }

        // 4. Prepare the storage (🦜 : we now allow non-object storage. <2024-03-21 Thu>)
        json::value storage = json::parse(a.disk_storage[1], ec);
        if (ec) {
          BOOST_LOG_TRIVIAL(info) <<  msg0 << "Failed to parse the storage: " << ec.message() << msg;
          return {};
        }

        // 5. invoke the method
        json::object r = invokePyMethod(invoke, weak::toStringView(a.code), abi, t, storage);
        if (r.contains("quit")) {
          BOOST_LOG_TRIVIAL(info) <<  msg0 << r["quit"].as_string() << msg;
          return {};
        }

        // 6. if the storage is modified, take it from the result to the Acn. Return the Acn
        if (r.contains("storage")) {
          // add it if
          a.disk_storage[1] = json::serialize(r["storage"]);
          r.erase("storage");
          return make_tuple(a, weak::bytesFromString(json::serialize(r)));
        }

        // 7. return just result
        return make_tuple(optional<Acn>({}) , weak::bytesFromString(json::serialize(r)));

    } // invokePyContract

    static path prepareWorkingDir(){
      BOOST_LOG_TRIVIAL(debug) <<  "⚙️ prepareWorkingDir entered";
      // 0. prepare our working dir
      path wd = filesystem::current_path() / ".pyvm_aaa";
      // 0.1 create if not exists
      if (not filesystem::exists(wd)) {
        filesystem::create_directory(wd);
      }
      return wd;
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
     * @param storage the contract's storage
     *
     * @return the result of the method invocation. Usually something like :
     *  - {"result" : 1 "log" : "Here is some log printed by the methods"}
     *  - {"result" : 1 "log" : "Here is some log printed by the methods", "storage" : {"name" : "Alice"}
     *  - {"quit" : "Some error message"}
     */
    static json::object invokePyMethod(json::object invoke, string_view py_code,
                                json::object abi,
                                const Tx & t,
                                json::value storage
                                ) noexcept{

      path wd = prepareWorkingDir();

      try{

        // BOOST_LOG_TRIVIAL(debug) <<  "invokePyMethod entered";
        json::object args = checkAndPrepareArgs(invoke, abi);
        if (args.contains("quit")) {
          return args;
        }

        // BOOST_LOG_TRIVIAL(debug) <<  "Checking method and preparing args passed";
        // 1.2 prepare the spacial args
        string method = invoke["method"].as_string().c_str(); // json::string -> string
        json::array required_args = abi[method].as_array();
        addTxContextToArgsMaybe(required_args, args, t);

        //    1.3 prepare the _storage
        // BOOST_LOG_TRIVIAL(debug) <<  "Adding _storage, required_args" << required_args;
        if (weak::contains(required_args,"_storage")) {
          args["_storage"] = storage;
        }

        // 2. write the args to args.json
        path args_path = wd / "args.json";
        // (ofstream(args_path.c_str(), out | trunc) << json::serialize(args)).flush();
        writeToFile(args_path, json::serialize(args));

        // 3. make the python script
        string py_code_content = makePyTxCode(py_code,PY_INVOKE_TEMPLATE, method);
        // 3. Finally, invoke the python
        // static tuple<int,string> exec_py(string  py_code_content, const int timeout_s = 2,
        //                                  path wd = std::filesystem::temp_directory_path() // 🦜: let's keep it simple.

        auto [exit_code, output] = exec_py(py_code_content, 3, wd);
        if (exit_code != 0) {
          return {{"quit", json::string("Python-vm exited with non-zero exit code: " + std::to_string(exit_code) + "Output: " + output)}};
        }

        // 4. 🦜 : read the result and return it
        path result_path = wd / "result.json";
        // ifstream(result_path.c_str()) >> result_json;
        string result_json = readAllText(result_path);
        json::error_code ec;
        json::value result_v = json::parse(result_json, ec);
        json::object result = result_v.as_object();

        if (ec) {
          return {{"quit", json::string("Failed to parse the result json `" + result_json + "`")}};
        }

        json::object r;             // the acutal result for the user = return val + log
        // r.insert({"result", result["result"]});
        // BOOST_LOG_TRIVIAL(debug) <<  "Adding result";
        r["result"] = result["result"];

        /*
          <2024-03-21 Thu> 🦜 : Here we can do a kinda improvement.... Here we
          only output the storage if it's different from the original one
          (i.e. it's modified by the method)
        */
        if (result.contains("storage") and
            (result["storage"] != storage) // boost::json's != operator, storage can also be json::value
            ) {
          /*
            🦜 : In fact, we see that `storage` can be anything in fact...., but
            for simplicity (and probably manageability), let's just make sure
            that the result is an object. So that user should do something like

            ```
            _storage = 2
            ```

            In their code...

            🐢 : Wait,No, let's just trust the user, and let them....

            🦜 : Okay... that's kinda C-spirit, but let's just do it.
           */
          BOOST_LOG_TRIVIAL(debug) << "Adding storage";
          r["storage"] = result["storage"];
        }
        // BOOST_LOG_TRIVIAL(debug) <<  "Adding log";
        // r.insert({"log", json::string(output)});
        r["log"] = json::string(output);
        // BOOST_LOG_TRIVIAL(debug) <<  "returning result";
        return r;
      }catch(const std::exception & e){
        return {{"quit", json::string(e.what())}};
      }
    }

    static json::object checkAndPrepareArgs(json::object invoke, json::object abi) noexcept{
      if (not invoke.contains("method")) {
        return {{"quit", json::string("Malformed invoke json: `method` not found in the invoke object")}};
      }

      string method = invoke["method"].as_string().c_str();
      if (not abi.contains(method)) {
        return {{"quit",
                   json::string("Method not found in abi: " + string(method))
          }};
      }
      // 1. prepare the input for the python-vm
      //    1.0 get the argument list and start preparing the args
      json::array required_args = abi[method].as_array(); // 🦜 : we assume that abi is well-formed.
      json::object args;

      if (invoke.contains("args")) {
        if (not invoke["args"].is_object()) { // 🦜 : provided by the user, so we probably should check it
          return {{"quit", json::string("Malformed invoke json: `args` should be an object")}};
        }
        args = invoke["args"].as_object();
      }

      /*
        1.1 Check if the required args are provided.
        🦜 : All args that doesn't start with `_` are required, and they should be matched excatly. (no more, no less)
      */
      json::object unused_args = args; // copy
      // 🦜 : It feels like the simplest way is to copy the object.

      for (const auto & a : required_args) {

        // if (not a.is_string()) {
        //   return {{"quit", json::string("Malformed abi: The args should be strings")}};
        // } // 🦜 : again, we assume that the abi is well-formed

        if (a.as_string().starts_with("_")) {
          continue;
        }

        // try find the arg in the args
        json::object::const_iterator it = unused_args.find(a.as_string());
        if (it == unused_args.end()){ // not found
          return {{"quit", json::string("Required argument not found: `" + string(a.as_string()) + '`')}};
        }
        // mark the arg as used (by popping it from the set)
        unused_args.erase(it);
      }

      // if there're unused args, that's an error
      if (not unused_args.empty()) {
        return {{"quit", json::string("Excess arguments provided: " + json::serialize(unused_args))}};
      }

      return args;
    } // checkAndPrepareArgs

    static void addTxContextToArgsMaybe(json::array & required_args, json::object & args, const Tx & t) noexcept{
      if (weak::contains(required_args,"_tx_context")) {
        json::object tx_ctx = {
          {"to", json::string(weak::addressToString(t.to))},
          {"from", json::string(weak::addressToString(t.from))},
          {"timestamp", t.timestamp},
          {"hash", json::string(weak::hashToString(t.hash()))},
        };
        args["_tx_context"] = tx_ctx;
      }
    }

    static string makePyTxCode(string_view py_code, string s /* copy */ , string method) noexcept{
      /*
        import json
        args = None
        with open('args.json', 'r') as f:
        args = json.load(f)
        r = METHOD(**args)
        with open('result.json', 'w') as f:
        json.dump(r, f)
      */
      // string py_0 = py_code + "\n\n";
      boost::replace_all(s, "METHOD", method);
      return string(py_code) + "\n\n" + s;
    }

  }; // class PyTxExecutor

#endif


  class Div2Executor : public EvmExecutor{
  public:
    optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                         const Tx & t) const noexcept override{

      // BOOST_LOG_TRIVIAL(debug) <<  "Div2Executor::executeTx entered, with tx = " << t.toJson();
      // 🦜 : Here we switch on the type of the Tx
      switch(t.type){
      case Tx::Type::data:
        return executeDataTx(w,t);
      case Tx::Type::python:
#if defined(WITH_PYTHON)
        return PyTxExecutor::executePyTx(w,t);
#else
        BOOST_LOG_TRIVIAL(info) <<  "❌️ Python-vm is not enabled";
        return {};
#endif
      case Tx::Type::evm:
      default:
        return EvmExecutor::executeTx(w,t);
      };

      // 🦜 : This should never happen
      return {};
    }

    /**
     * @brief Execute a data Tx.
     *
     * 🦜 : Just say yes and do nothing.
     */
    optional<tuple<vector<StateChange>,bytes>> executeDataTx(IAcnGettable * const w, const Tx & t) const noexcept{
      vector<StateChange> s = {};
      bytes r = {};
      return make_tuple(s,r);
    }

  };                            // class Div2Executor



  /**
   * @brief The Div2Executor that checks the validity of the Tx. [2024-01-22]
   *
   * 🐢 : This executor can do SSL stuff such
   *  1. checking the `Tx.signature` of the tx
   *  2. deriving the `Tx.from` address from the `Tx.pk_pem`
   *  3. checking `Tx.pk_crt` if needed.
   *
   * This is to really make the chain serious.
   *
   * 🦜 : So this is kinda a `Div2Executor` + `Tx` verifyer.
   *
   *
   * @see Tx in `core.hpp` and its notes on [2024-01-22]
   */
  class SeriousDiv2Executor : public Div2Executor{
  };
}
