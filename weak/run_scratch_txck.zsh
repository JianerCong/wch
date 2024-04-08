
w=./build-weak/wch
# 1. start in public mode (tx needs a signature)
$w --port 7777 --light-exe --tx-mode-serious public

# 2. new a keypair
w=./build-weak/wch
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
# [] means rejected at the front door..

# --------------------------------------------------
# 🦜 : start the chain in ca tx-mode
# 0. generate ca keypair
w=./build-weak/wch
capk=$(pwd)/build-weak/ca-pk.pem
cask=$(pwd)/build-weak/ca-sk.pem
$w toolbox new-keypair $capk $cask

# 1. start in ca tx-mode (pk needs to be signed by ca)
$w --port 7777 --light-exe --tx-mode-serious ca@$capk

# 2. new a keypair
pk=$(pwd)/build-weak/acn-pk.pem
sk=$(pwd)/build-weak/acn-sk.pem
$w toolbox new-keypair $pk $sk

# 2.1 sign the pk
cask=$(pwd)/build-weak/ca-sk.pem
crt=$(pwd)/build-weak/acn-crt.bin
$w toolbox sign-pk $cask $pk $crt

# 3. write a tx
txf=$(pwd)/build-weak/tx-pre.json
txf2=$(pwd)/build-weak/tx-post.json
echo '[{"type" : "data", "to" : "", "data" : "hi", "nonce" : 123}]' > $txf
# 3.1 sign the tx
$w toolbox tx-sign $txf $sk $crt $txf2

# 3.2 view it
cat $txf2 | jq .

# 4. send the tx
out=$(curl --data @$txf2 http://localhost:7777/add_txs)
# 4.1 view the result
echo $out | jq .
# 4.2 see the tx-receipt
h=$(echo $out | jq -M -r '.[0] | .hash')
curl "http://localhost:7777/get_receipt?hash=$h" | jq .


# 5. 🦜 : If we remove the `pk_crt`, the tx will be rejected

# 5.1 for each object in array, delete the field `pk_crt`
txf3=$(pwd)/build-weak/tx-post-bad.json
cat $txf2 | jq '[ .[] | del(.pk_crt) ]'
cat $txf2 | jq '[ .[] | del(.pk_crt) ]' --monochrome-output > $txf3

# 5.1.1 send (should be rejected)
out=$(curl --data @$txf3 http://localhost:7777/add_txs)
# [] means rejected at the front door..
echo $out | jq .

# 5.2 if we modify the crt, it will be rejected too (should be valid hex.)
txf4=$(pwd)/build-weak/tx-post-bad2.json
cat $txf2 | jq '[ .[] | .pk_crt = "bbad" ]'
cat $txf2 | jq '[ .[] | .pk_crt = "bbad" ]' --monochrome-output
cat $txf2 | jq '[ .[] | .pk_crt = "bbad" ]' --monochrome-output > $txf4
# 5.2.1 send (should be rejected)
out=$(curl --data @$txf4 http://localhost:7777/add_txs)
# [] means rejected at the front door..
echo $out | jq .


