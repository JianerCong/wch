w=./build-weak/wch

# 1. start in public mode (tx needs a signature)
$w --port 7777 --light-exe --tx-mode-serious public

# 2. new a keypair
pk=$(pwd)/build-weak/acn-pk.pem
sk=$(pwd)/build-weak/acn-sk.pem

$w toolbox new-keypair $pk $sk

# 3. write a tx 🦜 : from doesn't need to be filled
txf=$(pwd)/build-weak/tx-pre.json
txf2=$(pwd)/build-weak/tx-post.json
echo '[{"type" : "data", "to" : "", "data" : "hi", "nonce" : 123}]' > $txf
# 3.1 sign the tx
$w toolbox tx-sign-no-crt $txf $sk $txf2

# 3.2 view it
cat $txf2 | jq .

# 4. send the tx
out=$(curl --data @$txf2 http://localhost:7777/add_txs)
# 4.1 view the result
echo $out | jq .
# 4.2 see the tx-receipt
h=$(echo $out | jq -M -r '.[0] | .hash')
curl "http://localhost:7777/get_receipt?hash=$h" | jq .


# 5. 🦜 : If we just change the data, the tx will be rejected (i.e. we need to resign)
txf3=$(pwd)/build-weak/tx-post2.json
sed 's/hi/hihi/' $txf2 > $txf3
cat $txf3 | jq .
# 5.1 send
out=$(curl --data @$txf3 http://localhost:7777/add_txs)
# 5.2 view the result
h=$(echo $out | jq -M -r '.[0] | .hash')
curl "http://localhost:7777/get_receipt?hash=$h" | jq .
