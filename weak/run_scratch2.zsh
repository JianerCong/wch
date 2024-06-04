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

# --------------------------------------------------
# ca tx-mode
alias wch=~/repo/weak-chain/build-weak/wch
capk=./ca-pk.pem
cask=./ca-sk.pem
wch toolbox new-keypair $capk $cask

# start the chain in ca tx-mode, pointing to the public key
wch --port 7777 --light-exe --tx-mode-serious ca@$capk

# create an account
pk=./acn-pk.pem
sk=./acn-sk.pem
wch toolbox new-keypair $pk $sk

# sign the account with the ca's secret key
cask=./ca-sk.pem
crt=./acn-crt.bin
wch toolbox sign-pk $cask $pk $crt

# 3. write a tx
txf=./tx-pre.json
txf2=./tx-post.json
echo '[{"type" : "data", "to" : "", "data" : "hi", "nonce" : 123}]' > $txf
# 3.1 sign the tx
wch toolbox tx-sign $txf $sk $crt $txf2

# 3.2 view it
cat $txf2 | jq .

# 4. send the tx
e=http://localhost
out=$(curl --data @$txf2 $e:7777/add_txs)
echo $out | jq .
