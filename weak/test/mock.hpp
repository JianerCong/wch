#pragma once
#include "core.hpp"

/**
 * @file mock.hpp
 * @brief Provide some mocked interface for testing. The mocks here are those
 * need to be shared.
 */
using namespace weak;
namespace mockedAcnPrv {
  // mockedAcnProviders: they implement IAcnGettable for test.

  /// An empty Acn Provider
  class A: public virtual weak::IAcnGettable{
  public:
    std::optional<Acn> getAcn(evmc::address addr)
      const noexcept override{
      return {};
    };
  };

  /// A provider who only have one Acn;
  class B: public virtual weak::IAcnGettable{
  public:
    std::optional<Acn> getAcn(evmc::address addr)
      const noexcept override{
      return Acn{123,evmc::from_hex("0011").value()};
    };
  };

  /// A provider who only have Acn 0x1;
  class C: public virtual weak::IAcnGettable{
  public:
    std::optional<Acn> getAcn(evmc::address addr)
      const noexcept override{
      if (addressToString(addr) ==
          addressToString(makeAddress(1)))
        return Acn{123,evmc::from_hex("0011").value()};

      return {};
    };
  };

  /// A provider who only have Acn 0x1; which has code that returns aabbcc(padded to 32 bytes)
  class D: public virtual weak::IAcnGettable{
  public:
    std::optional<Acn> getAcn(evmc::address addr)
      const noexcept override{
      if (addressToString(addr) ==
          addressToString(makeAddress(1))){
        string deploy_hex =
          "62" /*PUSH3*/
          "aabbcc"
          "60" /*PUSH1*/ "00"
          "52" /* mstore: store (padded-to-32)aabbcc at Mem[0] */
          "6020" /*PUSH1 32 = 20 in hex*/ "6000" /*PUSH1 00*/ "f3" /*return Mem[00:20]*/;

        Acn acn{0,evmc::from_hex(deploy_hex).value()};
        return acn;
      }
      return {};
    };
  };

  /// A provider who holds a map that can acturally store Acn
  //
  // 🦜 : We can apply journal to it, so it's pretty-much a in-RAM (state-only) rocksdb.
  class E: public virtual weak::IAcnGettable,
           public virtual weak::IWorldChainStateSettable
  {
  public:
    unordered_map<string /*address hex*/
                  ,Acn> accounts;
    std::optional<Acn> getAcn(evmc::address addr)
      const noexcept override{
      string k = addressToString(addr);
      if (not accounts.contains(k))
        return {};
      return accounts.find(k)->second;
    };

    bool setInChainDB(const string k, const string v) override{return false;};

    optional<string> getFromChainDB(const string k) const override{return {};};

    bool applyJournalStateDB(const vector<StateChange> & j) override{
      for (const StateChange & c : j){
        BOOST_TEST_MESSAGE(format("Applying state-change %s on addr %s")
                           % (c.del ? "DELETE" : "PUT")
                           % (c.k)
                           );
        if (c.del){
          this->accounts.erase(c.k);
        }
        else{
          Acn a;
          if (not a.fromString(c.v))
            return false;
          this->accounts[c.k] = a;
        }
      }
      return true;
    };
  };


  /// A provider who holds a map that can acturally store chain data and Acn
  //
  // 🦜 : We can apply journal to it, so it's pretty-much a in-RAM rocksdb.
  class F: public virtual weak::IAcnGettable,
           public virtual weak::IWorldChainStateSettable
  {
  public:
    unordered_map<string /*address hex*/
                  ,Acn> accounts;
    unordered_map<string /*address hex*/
                  ,string> chainDB;
    std::optional<Acn> getAcn(evmc::address addr)
      const noexcept override{
      string k = addressToString(addr);
      if (not accounts.contains(k))
        return {};
      // return accounts.find(k)->second;
      return accounts.at(k);
    };

    bool setInChainDB(const string k, const string v) override{
      chainDB[k] = v;
      return true;
    };

    optional<string> getFromChainDB(const string k) const override{
      if (chainDB.contains(k))
        return chainDB.at(k);
      return {};
    };

    bool applyJournalStateDB(const vector<StateChange> & j) override{
      for (const StateChange & c : j){
        BOOST_TEST_MESSAGE(format("Applying state-change %s on addr %s")
                           % (c.del ? "DELETE" : "PUT")
                           % (c.k)
                           );
        if (c.del){
          this->accounts.erase(c.k);
        }
        else{
          Acn a;
          if (not a.fromString(c.v))
            return false;
          this->accounts[c.k] = a;
        }
      }
      return true;
    };
  };


  /// A provider who holds a map that can acturally store chain data and Acn

  // 🦜 : We can apply journal to it, so it's pretty-much a in-RAM rocksdb. The
  // difference between this and F is that F2 uses string:string to store
  // accounts
  class F2: public virtual weak::IAcnGettable,
           public virtual weak::IWorldChainStateSettable
  {
  public:
    unordered_map<string /*address hex*/
                  ,string> accounts;
    unordered_map<string /*address hex*/
                  ,string> chainDB;
    std::optional<Acn> getAcn(evmc::address addr)
      const noexcept override{
      string k = addressToString(addr);
      if (not accounts.contains(k))
        return {};


      string v = this->accounts.at(k);
      Acn a;
      if (not a.fromString(v))
        return {};
      return a;
    };

    bool setInChainDB(const string k, const string v) override{
      chainDB[k] = v;
      return true;
    };

    optional<string> getFromChainDB(const string k) const override{
      if (chainDB.contains(k))
        return chainDB.at(k);
      return {};
    };

    bool applyJournalStateDB(const vector<StateChange> & j) override{
      for (const StateChange & c : j){
        BOOST_TEST_MESSAGE(format("Applying state-change %s on addr %s")
                           % (c.del ? "DELETE" : "PUT")
                           % (c.k)
                           );
        if (c.del){
          this->accounts.erase(c.k);
        }
        else{
          this->accounts.insert_or_assign(c.k,c.v);
        }
      }
      return true;
    };
  };
}


namespace mockedHashTxGetter{
  // The empty source
  class A: public virtual weak::IByHashTxGettable{
  public:
    optional<Tx> getTxByHash(hash256 h) noexcept override{
      return {};
    }
  };

  // The constant source
  class B: public virtual weak::IByHashTxGettable{
  public:
    vector<Tx> txs;
    Tx tx;
    B(){
      address a1 = makeAddress(1);
      address a2 = makeAddress(2);
      bytes data(size_t{2},uint8_t{0xff}); // we would like this.

      this->tx = Tx(a1,a2,data,123/*nonce*/);
      this->txs.push_back(this->tx);
      this->txs.push_back(Tx(a1,a2,data,234/*nonce*/));
      this->txs.push_back(Tx(a1,a2,data,345/*nonce*/));
    }
    optional<Tx> getTxByHash(hash256 h) noexcept override{
      /*
        🦜 : Now the hash has changed to a method, so ....
       */
      for (Tx t : this->txs){
        if (t.hash() == h)
          return t;
      }

      return {};
    }
  };
}

#include "forPostExec.hpp"
namespace mockedBlkExe{
  class A: public virtual IBlkExecutable{
  public:
    ExecBlk executeBlk(const Blk & b) const noexcept override{
      return {};
    }
    bool commitBlk(const ExecBlk & b) noexcept override{
      return true;
    }
  };

  class B: public virtual IBlkExecutable{
  public:
    ExecBlk executeBlk(const Blk & b) const noexcept override{
      return {};
    }
    bool commitBlk(const ExecBlk & b) noexcept override{
      return false;
    }
  };

  /*🦜 : This one remembers what are executed and committed.*/
  class C: public virtual IBlkExecutable{
  public:
    mutable vector<int> blk_numbers_executed;
    vector<int> blk_numbers_commited;
    // ^^^^ can be modified by const method
    ExecBlk executeBlk(const Blk & b) const noexcept override{
      this->blk_numbers_executed.push_back(b.number);
      // make the ExecBlk
      vector<vector<StateChange>> j;
      vector<TxReceipt> r;

      for (int i=0;i<b.txs.size();i++){
        j.push_back({{false,"k1","v1"}}); // journal for k1 v1
        r.push_back(TxReceipt(false)); // failed tx
      }

      ExecBlk eb{b,j,r};
      return eb;
    }
    bool commitBlk(const ExecBlk & b) noexcept override{
      this->blk_numbers_commited.push_back(b.number);
      return true;
    }
  };
}

