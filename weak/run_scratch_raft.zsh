w=./build-weak/wch

# --------------------------------------------------
# 0. 🦜 : Let's start a three-node mock-exe
./build-weak/wch --port 7777 --mock-exe \
   --consensus Raft --Raft.node-list localhost:7778 localhost:7779

./build-weak/wch --port 7778 --mock-exe \
   --consensus Raft --Raft.node-list localhost:7777 localhost:7779

./build-weak/wch --port 7779 --mock-exe \
   --consensus Raft --Raft.node-list localhost:7777 localhost:7778


# --------------------------------------------------
# 1. send a tx
txs='[
  {"from" : "01",
   "to" : "02",
   "data" : "ffff",
   "nonce" : 123,
   "type" : "data"
  }
]'
e=http://localhost
curl --data $txs $e:7777/add_txs
