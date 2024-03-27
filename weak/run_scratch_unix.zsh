w=./build-weak/wch

# start as Solo-primary at 7777
$w --port 7777 --mock-exe --unix-socket /tmp/hi-weak.sock

# disable ram
$w --port 7777 --consensus Solo-static --unix-socket /tmp/hi-weak.sock
# disable log too 🦜 : Use this for the speed test
$w --port 7777 --consensus Solo-static --verbose=no --unix-socket /tmp/hi-weak.sock


e=http:/localhost               # <- 🦜 `localhost` is set to the Host header.
curl --unix-socket /tmp/hi-weak.sock $e/get_node_status

# 🦜 : send a request via socket, to see
txs='[{"type" : "evm", "from" : "01", "to" : "", "data" : "hi", "nonce" : 123}]'
curl --unix-socket /tmp/hi-weak.sock $e/get_latest_Blk
# => [] : no Blk

clear &&  pytest -s build-weak/test_with_unix_and_pb.py
clear &&  pytest -s build-weak/test_with_unix_and_pb.py::test_serv_open_close
clear &&  pytest -s build-weak/test_with_unix_and_pb.py::test_serv_send_basic_node_status
clear &&  pytest -s build-weak/test_with_unix_and_pb.py::test_add_txs

python ./weak/send_a_tx_pb.py      # try the pb
python ./weak/weak_txs_tool.py      # try the tool
# try the tool txs-per-batch, n-batch (default = 10, 2)
# try the tool
python ./weak/weak_txs_tool2.py
python ./weak/weak_txs_tool2.py 10 2 True # ctps : 5567
python ./weak/weak_txs_tool2.py 10000 2 True # ctps : 26104
python ./weak/weak_txs_tool2.py 50000 2 True # ctps : 26754
python ./weak/weak_txs_tool2.py 50000 1 True # ctps : 41124
python ./weak/weak_txs_tool2.py 60000 1 True # ctps : 41263

# check the latest block
e=http:/localhost               # <- 🦜 `localhost` is set to the Host header.
out=$(curl --unix-socket /tmp/hi-weak.sock $e/get_latest_Blk)
echo $out | jq .

out=$(curl --unix-socket /tmp/hi-weak.sock $e/get_latest_Blk)
