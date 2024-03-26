w=./build-weak/wch

# start as Solo-primary at 7777
$w --port 7777 --mock-exe --unix-socket /tmp/hi-weak.sock

# disable ram
$w --port 7777 --consensus Solo-static --unix-socket /tmp/hi-weak.sock
# disable log
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
python ./weak/weak_txs_tool2.py      # try the tool
# try the tool txs-per-batch, n-batch (default = 10, 2)
python ./weak/weak_txs_tool.py 1000 2
python ./weak/weak_txs_tool.py 1000 5
python ./weak/weak_txs_tool.py 10000 2
# python ./weak/weak_txs_tool.py 100000 2 # ❌️ 🦜 : failed, boost::beast default to limit the request body to 8MB.
# python ./weak/weak_txs_tool.py 50000 1 # ❌️ failed
python ./weak/weak_txs_tool.py 20000 1

python ./weak/weak_txs_tool.py 1000 3
python ./weak/weak_txs_tool.py 20000 3

# check the latest block
e=http:/localhost               # <- 🦜 `localhost` is set to the Host header.
out=$(curl --unix-socket /tmp/hi-weak.sock $e/get_latest_Blk)
echo $out | jq .

out=$(curl --unix-socket /tmp/hi-weak.sock $e/get_latest_Blk)
