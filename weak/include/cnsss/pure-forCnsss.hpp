/**
 * @file pureForCnsss.hpp
 * @brief Things shared between pure consensus.
 *
 * 🦜 : This file don't include `core.hpp`. Keep it.
 */
#pragma once
#include <string>
#include <optional>
#include <functional>
#include <unordered_map>

#include <boost/algorithm/string.hpp> // for join()
#include <mutex>
#include <memory>
#include <tuple>


#include <boost/json.hpp>
#include <vector>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "../pure-common.hpp"


namespace pure {

  using boost::format;
  using std::move;
  using std::vector;


  using std::unique_ptr;
  using std::unordered_map;
  namespace json = boost::json;
  using namespace std::placeholders;  // for _1, _2, _3...
  using std::bind;
  using std::shared_ptr;
  using std::tuple;
  using std::function;
  using std::optional;
  using std::string;


  /**
   * @brief The common inferfaced of a consensus.
   *
   * This interface is used by mainly used by `CnsssBlkChainAssistant` which is
   * like the connection between the *pure consensus world* and the *block chain
   * world*. `is_primary()` is usuallly called by the sealer to decide wether it
   * should seal a Blk. `handle_execute()` is called by RPC or sealer to execute
   * a command. This should start the consensus process, for example, forwarding
   * to primary or boardcasting to other nodes.
   *
   * 🦜 : The assistant exists to keep the consensus pure (block-chain
   * ignorant).
   */
  class ICnsssPrimaryBased {
    // 🦜 : private, use handle_execute() below
    virtual optional<string> handle_execute_for_primary(string endpoint,
                                                        string data)=0;
    virtual optional<string> handle_execute_for_sub(string endpoint,
                                                    string data)=0;
  public:
    virtual bool is_primary() const noexcept=0;
    optional<string> handle_execute(string endpoint, string data)noexcept{
      if (this->is_primary())
        return handle_execute_for_primary(endpoint,data);
      return handle_execute_for_sub(endpoint, data);
    };
  };

  class IForCnsssNetworkable{
  public:
    virtual string listened_endpoint()noexcept=0;
    virtual void clear()noexcept=0;
  };

  class IEndpointBasedNetworkable: public virtual IForCnsssNetworkable{
  public:
    virtual void listen(const string target,
                        function<optional<string>(string,string)>
                        handler)noexcept=0;
    virtual optional<string> send(string endpoint,
                                  string target,
                                  string data)noexcept=0;
  };

  /**
   * @brief The executor for pure consensus to use.
   *
   * This is like the computer for the consensus. Note that execute() might
   * modify the input parameter. The reason is as follows:
   *
   * The `cmd` is usually executed by the primary first (depends on the
   * consensus writer), after that, the `cmd` is boardcast to other nodes.
   * During the primary's execution, the primary can modify the `cmd` so that
   * some useless things are removed before boardcast. For example, if the input
   * `cmd` ask the group the insert 3 `Tx`s, but 2 of them are invalid (failed
   * to be inserted into the mempool). The (executor of the ) primary can do something like:
   *
   * ```c++
   * vector<Tx> txs = deserialize(cmd);
   * for (vector<Tx>::iterator it = txs.begin(); it!=txs.end();){
   *    if (not pool.addTx(*it))
   *       it = txs.erase(it); // If I failed to add it, I don't bother boardcasting it.
   *    else
   *       it++;
   * }
   * //...
   * cmd = serialize(txs);      // to be passed to other nodes
   * ```
   *
   * 🦜 : This will allow executor to interference with the consensus, but it is
   * also dangerous that other nodes will execute commands different from the
   * primary's. It is the executor's responsibility to ensure That
   *        "The modified `cmd` will still result in the same state-change"
   */
  class IForConsensusExecutable{
  public:
    virtual string execute(string & cmd)noexcept =0;
  };


  // colors
#define S_RED     "\x1b[31m"
#define S_GREEN   "\x1b[32m"
#define S_YELLOW  "\x1b[33m"
#define S_BLUE    "\x1b[34m"
#define S_MAGENTA "\x1b[35m"
#define S_CYAN    "\x1b[36m"
#define S_NOR "\x1b[0m"

  namespace mock{
    using namespace pure;
    class Executable: public virtual IForConsensusExecutable{
    public:
      string id;
      Executable(string i):id(i){};
      string execute(string & cmd) noexcept override{
        BOOST_LOG_TRIVIAL(debug) << format(S_RED "🦜 [%s] Exec: %s" S_NOR)
          % this->id % cmd;
        return "OK";
      };

      virtual ~Executable(){};
    };
}

} // namespace pure
