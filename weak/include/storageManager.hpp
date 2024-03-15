#pragma once
#include "core.hpp"
#include <cassert>
#include <unordered_map>
#include <map>

#include <rocksdb/convenience.h> // grab db.h,status.h,table.h (BlockBasedTableOptions)
#include <rocksdb/slice_transform.h> // NewCappedPrefixTransform
#include <rocksdb/filter_policy.h> // NewBloomFilterPolicy

#include <filesystem>
namespace filesystem = std::filesystem;
using rocksdb::NewCappedPrefixTransform;
using std::map;

#include <boost/throw_exception.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include <exception>
// #include <evmc/evmc.hpp>
// #include <boost/format.hpp>
// #include <intx/intx.hpp>
// #include <boost/log/trivial.hpp> // For BOOST_LOG_TRIVIAL, trace, debug,..,fatal
// #include <boost/log/core.hpp>
// #include <boost/log/expressions.hpp>
// #include <algorithm>

namespace weak {

  

  /**
   * @brief The core storage.
   *  - accessed by `CoreManager` through `IWorldChainStateSettable`.
   *  - accessed by `EVMExecutor` through `IAcnGettable`
   *
   *  The internal data is made public so that it can be accessible from
   *  unittest and StorageManager.
   */
  class WorldStorage: public virtual IWorldChainStateSettable,
                      public virtual IAcnGettable,
                      public virtual IChainDBGettable2
  {
  public:
    rocksdb::DB* chainDB;
    rocksdb::DB* stateDB;

    WorldStorage(filesystem::path d =
                 filesystem::current_path()) {

      // The DB-dir
      filesystem::path chainDir = d / "chainDB",
        stateDir  = d / "stateDB";

      BOOST_LOG_TRIVIAL(debug) << format("🌍 Initializing WorldStorage at\n\t"
                                         S_GREEN "chainDir: %s\tstateDir: %s" S_NOR
                                         ) % chainDir % stateDir;

      /*
        🦜 : Nope, these folder may already exist, and those contain the
        persistant data. So we should not delete these. Also it is possible that
        things in those folder are just rubbish...

        🐢 : Yeah, the next [Todo] is to check wether things in these folder are
        sane...
       */

      // removeDirIfExist(chainDir);
      // removeDirIfExist(stateDir);

      openChainDB(chainDir);
      openStateDB(stateDir);
      BOOST_LOG_TRIVIAL(debug) << format("🌍 WorldStorage ctor done.");
    };

    ~WorldStorage(){
      BOOST_LOG_TRIVIAL(info) << format("❄ Closing WorldStorage");
      delete chainDB;
      delete stateDB;
}

    vector<string> getKeysStartWith(string_view prefix) const override{
      return WorldStorage::find_keys_with_prefix(this->chainDB,prefix);
    }

    static vector<string> find_keys_with_prefix(rocksdb::DB * db,string_view p){
      rocksdb::Iterator* iter = db->NewIterator(rocksdb::ReadOptions());
      vector<string> ks;
      rocksdb::Slice prefix{p};
      for (iter->Seek(prefix); iter->Valid() && iter->key().starts_with(prefix); iter->Next()) {
        ks.push_back(iter->key().ToString());
      }
      return ks;
    }

    bool setInChainDB(const string k, const string v) override{
      BOOST_LOG_TRIVIAL(debug) << format("Setting chainDB\n\t" S_CYAN "k=%s\n\tv=%s" S_NOR)
        % k % pure::get_data_for_log(v);
      rocksdb::Status s = this->chainDB->Put(rocksdb::WriteOptions(), k, v);
      return checkStatus(s,"",false/*not fatal*/);
    };

    optional<string> getFromChainDB(const string k) const override{
      BOOST_LOG_TRIVIAL(debug) << format("Getting chainDB\n\t" S_CYAN "k=%s" S_NOR)
        % k;
      return tryGetKvString(this->chainDB,k);
    };

    bool applyJournalStateDB(const vector<StateChange> & j) override{
      rocksdb::WriteBatch b;
      for (const StateChange & i : j){
        bool ok;
        if (i.del){
          BOOST_LOG_TRIVIAL(info) << format("Adding 🚮️ Deletion " S_MAGENTA "k=%s" S_NOR) % i.k;
          b.Delete(i.k);
        }else{
          // <2024-03-15 Fri> 🦜 : Only log the content if not with pb
          BOOST_LOG_TRIVIAL(debug) << format("Adding ⚙️ Insertion "
                                             S_CYAN "(k,v) = (%s,%s)" S_NOR) % i.k
            /*% i.v;*/ % pure::get_data_for_log(i.v); // <- 🦜 : considers pb
          b.Put(i.k,i.v);
        }
      }
      BOOST_LOG_TRIVIAL(info) << format("Applying batch");
      rocksdb::Status s = this->stateDB->Write(rocksdb::WriteOptions(),&b);
      return checkStatus(s,"Failed to apply journal batch to StateDB");
    };


    /**
     * @brief Get the serialized Acn from the stateDB.
     * @param addr the Account address.
     */
    optional<Acn> getAcn(evmc::address addr) const noexcept{
      string k = addressToString(addr);
      BOOST_LOG_TRIVIAL(debug) << format("Getting Acn\n\t" S_CYAN "k=%s" S_NOR) % k;

      optional<string> v = tryGetKvString(this->stateDB,k);
      if (not v) return {}; // 🦜: Already have debugging msg in tryGetKvString()

      // Make the Acn
      Acn a;
      bool ok = a.fromString(v.value());
      if (not ok){
        BOOST_LOG_TRIVIAL(error) << format( S_RED "❌️ Invalid Acn string format in stateDB,returning empty Acn."
                                            S_NOR);
        return {};
      }

      return a;
    };

  private:

    static optional<string> tryGetKvString(rocksdb::DB * const db, const string & k) {
      string v;
      rocksdb::Status s = db->Get(rocksdb::ReadOptions(), k, &v);
      if (s.IsNotFound()){
        BOOST_LOG_TRIVIAL(debug) << format("key not found: " S_MAGENTA "%s" S_NOR)
          % k;
        return {};
      }
      if (checkStatus(s,"",false/*not fatal*/))
        return v;
      return {};
    }

    /**
     * @brief Open the chainDB in the specified empty folder `chainDir`.
     */
    void openChainDB(const filesystem::path chainDir){
      BOOST_LOG_TRIVIAL(info) << format("Opening " S_CYAN "chain DB" S_NOR);
      auto o = getInitOptions();
      rocksdb::Status s = rocksdb::DB::Open(o, chainDir.string() , &chainDB);
      checkStatus(s,"Failed to open chainDB at" +  chainDir.string());
      // 🦜 : the conversion from filesystem::path to std::string is only available in POSIX.
    }

    /**
     * @brief Open the stateDB in the specified empty folder `stateDir`.
     */
    void openStateDB(const filesystem::path stateDir){
      BOOST_LOG_TRIVIAL(info) << format("Opening " S_CYAN "state DB" S_NOR);
      auto o = getInitOptions(false /*prefix mode*/);
      rocksdb::Status s = rocksdb::DB::Open(o, stateDir.string() , &stateDB);
      checkStatus(s,"Failed to open stateDB at" + stateDir.string());
    }

    /**
     * @brief check the return value of an rocksdb::Status.
     * @param s The status to be checked
     * @param msg The optional message to display upon error
     * @param required If true, the function throws error, else issues warning.
     * @return whether the check is passed
     */
    static inline bool checkStatus(const rocksdb::Status & s,
                     const string & msg = "",
                     bool required=true){

      const string m = (format( S_RED "❌️ rocksdb error: %s \n\t%s" S_NOR)
                        % msg % s.ToString()).str();
      if (not s.ok()){
        if (required)
          BOOST_THROW_EXCEPTION(std::runtime_error(m));
        else
          BOOST_LOG_TRIVIAL(warning) << m;
      }
      return s.ok();
    }

    /**
     * @brief Get the initialization options for opening rocksdb.
     *
     * @param with_prefix_mode Whether enable the prefix bloom filter for the
     * Db.
     */
    static inline rocksdb::Options getInitOptions(bool with_prefix_mode=true){
      rocksdb::Options options;
      options.IncreaseParallelism();
      options.OptimizeLevelStyleCompaction();
      options.create_if_missing = true;
      if (with_prefix_mode){
        /*
          🦜 : Set up the bloom filter to use the first 4 bytes of key's prefix.
          This is just what the official github Wiki did.
         */
        rocksdb::BlockBasedTableOptions table_options;
        table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, false));
        table_options.whole_key_filtering = true;
        // If you also need Get() to use whole key filters, leave it to true.
        options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
        // For multiple column family setting, set up specific column family's ColumnFamilyOptions.table_factory instead.

        // Define a prefix. In this way, a fixed length prefix extractor. A recommended one to use.
        options.prefix_extractor.reset(NewCappedPrefixTransform(4)); // use first 4 bytes as prefix
      }
      return options;
    }

    /// Util func
    static inline void removeDirIfExist(filesystem::path d) {
      if (filesystem::exists(d)){
        BOOST_LOG_TRIVIAL(info) << format("🚮removing existing path " S_MAGENTA "%s" S_NOR)
          % d.string();
        bool ok = filesystem::remove_all(d);
        if (not ok){
          // auto e = filesystem::filesystem_error((format("Error remove existing dir %s")
          //                                        % string(d)
          //                                        ).str(),d,
          //                                       std::error_code()
          //                                       );

          // 🦜: For now, I'd rather use runtime error...
          auto e = std::runtime_error(
                                      (format("Error remove existing dir %s") % d.string()).str()
                                      );
          BOOST_THROW_EXCEPTION(e);

        }
      }
    }

  };

  /**
   * @brief The world storage that uses two map<string,string> to simulate two
   * rocksdbs. So nothing will be written to the disk. Used for testing.
   * 
   */
  class InRamWorldStorage: public virtual IWorldChainStateSettable,
                      public virtual IAcnGettable,
                      public virtual IChainDBGettable2{
  public:
    map<string /*address hex*/
                  ,string> stateDB;
    map<string,string> chainDB;
    std::optional<Acn> getAcn(evmc::address addr)
      const noexcept override{
      string k = addressToString(addr);
      if (not stateDB.contains(k))
        return {};

      string v = this->stateDB.at(k);
      Acn a;
      if (not a.fromString(v))
        return {};
      return a;
    }

    bool setInChainDB(const string k, const string v) override{
      chainDB[k] = v;
      return true;
    }

    optional<string> getFromChainDB(const string k) const override{
      if (chainDB.contains(k))
        return chainDB.at(k);
      return {};
    }

    bool applyJournalStateDB(const vector<StateChange> & j) override{
      for (const StateChange & c : j){
        BOOST_LOG_TRIVIAL(debug) << S_CYAN <<
          format("Applying state-change %s on addr %s")
          % (c.del ? "DELETE" : "PUT")
          % (c.k) << S_NOR ;
        if (c.del){
          this->stateDB.erase(c.k);
        }
        else{
          this->stateDB.insert_or_assign(c.k,c.v);
        }
      }
      return true;
    }

    vector<string> getKeysStartWith(string_view prefix)const override{
      vector<string> o;
      for (const auto &[k, v]: this->chainDB){
        if (k.starts_with(prefix))
          o.push_back(k);
      }
      return o;
    }
  };

  // /**
  //  * 存储管理员：
  //  *
  //  * 危险⚡，这个会直接访问底层存储的数据。除非你来自数据归档和日志回放团队，否
  //  * 则慎用。
  //  */
  // class StorageManager {
  //   enum DBTYPE {
  //     DBTYPE_BLOCK,
  //     DBTYPE_JOURNAL,
  //     DBTYPE_RECEIPT,

  //     DBTYPE_CHAIN,
  //     DBTYPE_STATE,
  //   };
  // };

}
