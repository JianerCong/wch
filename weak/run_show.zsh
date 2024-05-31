w=./build-weak/wch
# 0. start the chain in solo mode
$w --port 7777

# 1.put some data on the chain
txs='[{"type" : "data", "from" : "01", "to" : "", "data" : "hi", "nonce" : 123}]'
curl --data $txs http://localhost:7777/add_txs

# [{"hash":"b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b"}]

# 2. get the data
curl http://localhost:7777/get_tx?hash=b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b

# {"from":"0000000000000000000000000000000000000001","to":"0000000000000000000000000000000000000000",
# "nonce":123,"timestamp":1717141105,"hash":"b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b",
# "type":"data","data":"hi"}

# --------------------------------------------------


# rename the doc
cp -v m.pdf wch-doc-`date +%Y%m%d`.pdf
