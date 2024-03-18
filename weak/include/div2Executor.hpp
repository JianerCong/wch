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

#define WITH_PYTHON             // 🦜 : This is a flag to enable the Python Tx type.


#if defined(WITH_PYTHON)
#define BOOST_PROCESS_NO_DEPRECATED 1
#include <boost/process.hpp>

#include <filesystem>
namespace filesystem = std::filesystem;
using std::filesystem::path;
using std::filesystem::current_path;

#include <fstream>
using std::ofstream;

#include <thread>
using std::this_thread::sleep_for;
namespace bp =  boost::process;

#endif



namespace weak{
  class Div2Executor : public EvmExecutor{
  public:
    optional<tuple<vector<StateChange>,bytes>> executeTx(IAcnGettable * const w,
                                                         const Tx & t) const noexcept override{

      // 🦜 : Here we switch on the type of the Tx
      switch(t.type){
      case Tx::Type::data:
        return executeDataTx(w,t);
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

#if defined(WITH_PYTHON)

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
  static tuple<int,string> exec0(const string cmd, const int timeout_s = 2) {
    bp::ipstream out, err;
    bp::child c(cmd, bp::std_out > out, bp::std_err > err);

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

    BOOST_LOG_TRIVIAL(debug) <<  "\tGot stderr: " << err.rdbuf();
    return make_tuple(c.exit_code(), output); // joined here
  }

    static tuple<int,string> exec_py(string  py_code_content, const int timeout_s = 2,
                                     path wd = std::filesystem::temp_directory_path(); // 🦜: let's keep it simple.
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
    return exec0("python3 -I " + p.string(), timeout_s);
  }
#endif

  };



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
