/**
 * @file pure-rbft.hpp
 *
 * @brief The (probably) 'robust' version of PBFT.
 *
 * 🐢 : RBFT is essentialy PBFT but with some additional features. The important ones
 * are:
 *
 *    1. The primary will filter out invalid Txs and prevent them from being
 *    boardcast to the cluster.
 *
 *    2. Nodes can be added dynamically to the cluster, which means that
 *    there're mechanisms that help new node to get synced.
 *
 */
#pragma once

#include <unordered_map>

#include <boost/algorithm/string.hpp> // for join()
#include <mutex>
#include <memory>

#include <boost/json.hpp>
#include <vector>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include "pure-forCnsss.hpp"


#include <mutex>
namespace pure{
  using boost::format;
  using std::move;
  using std::vector;


  using std::unique_ptr;

  using std::unordered_map;
  namespace json = boost::json;
  using namespace std::placeholders;  // for _1, _2, _3...
  using std::bind;
  using std::shared_ptr;

  template<typename T>
  // 🦜 : Bind the object with a lock.
  struct LockedObject{
    T o;
    std::mutex lock;
  };

  class RbftConsensus:
    public virtual ICnsssPrimaryBased,
    public std::enable_shared_from_this<RbftConsensus> // define
                                                              // shared_from_this()
  {
  private:
    /*
     * 🐢 : Make the ctor private, see pure-ListenToOneConsensus.hpp for reason.
     */
  };
} // namespace pure
