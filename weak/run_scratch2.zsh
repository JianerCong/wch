# 1. start a node
w=./build-weak/wch
# start quitely 🦜 : But it will only be quite during execution.
# $w --port 7777 --light-exe --verbose no
$w --port 7777 --light-exe

# 2.send txs
txs='[{"type" : "data", "from" : "01", "to" : "", "data" : "hi", "nonce" : 123}]'
e=http://localhost
out=$(curl --data $txs $e:7777/add_txs)
echo $out | jq .

# 3.get the tx
h=$(echo $out | jq -M -r '.[0] | .hash')
echo 🦜 : getting tx for hash $h
out=$(curl "$e:7777/get_tx?hash=$h")
echo $out | jq .

# 3.1 get the non-existent tx
curl "$e:7777/get_tx?hash=aa" | jq .

# 3.2 get Blk 0
curl "$e:7777/get_blk?n=0" | jq .
