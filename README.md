![Logo](./weak/doc/logo.svg)

[**English**](./README.md) | [**中文**](./README_CN.md)

# Welcome to Weak chain
*Weak chain* is a blockchain written in C++20. It's designed to be a working
private chain that's as simple as possible in the hope that it could help you
get started with blockchain.

## Features

+ Ready to run binary on Windows x64 and Linux x86/x64 (< 50MB)
+ Three types of Consensus: Raft, PBFT, Solo; PBFT supports dynamic membership.
+ EVM and Python smart contract.
+ Pure data transaction. Easier way to "put data on the chain".
+ Efficient P2P network with UDP and Protobuf.
+ No built-in crypto currency.
+ Built-in command-line cryptography toolbox to help you create and use digital
  signature. No need for third-party tools like `openssl`.

**📗️GPLv3: This project is for learning and research purposes only. Please do not
use it for any commercial purposes. In particular, this project is not designed to
be a competitor to any similar project or product.**

# Get started

Start a node in Solo mode, listening on port 7777
```bash
wch --port 7777
# [enter] to quit
```

## Simple usage: Put some data on the chain:
```bash
# 1.put some data on the chain
txs='[{"type" : "data", "from" : "01", "to" : "", "data" : "hi", "nonce" : 123}]'
curl --data $txs http://localhost:7777/add_txs

# [{"hash":"b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b"}]

# 2. get the uploaded data
curl http://localhost:7777/get_tx?hash=b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b

# {"from":"0000000000000000000000000000000000000001","to":"0000000000000000000000000000000000000000",
# "nonce":123,"timestamp":1717141105,"hash":"b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b",
# "type":"data","data":"hi"}
```

## More advanced usage: Python Smart Contract
📗️ :Make sure you have `python3` in your path.

```bash
mkdir -p tmp
# 1. prepare the contract (make sure we are in the folder where wch is started if not using docker)
echo '
from typing import Any
def hi(_storage: dict[str, Any], y : int) -> int:
    _storage["x"] = _storage.get("x", 1) + y
    return _storage["x"]
' > tmp/tmp.py

txs='[{"from" : "01","to" : "",
 "data" : "@tmp/tmp.py",
 "nonce" : 123,
 "type" : "python"
}]'

# 2. deploy the contract (use jq to capture the output)
e=http://localhost
out=$(curl --data $txs $e:7777/add_txs)
da=$(echo $out | jq -M -r '.[0] | .deployed_address')

# 3. prepare the invoke transaction
echo '{
        "method" : "hi",
        "args" : {"y" : 122}
}' > tmp/tmp.json

txs='[{
    "from" : "01",
    "to" : "'"$da"'",
    "data" : "@tmp/tmp.json",
    "nonce" : 124,
    "type" : "python"
}]
'

# 4. invoke
out=$(curl --data $txs $e:7777/add_txs)
h=$(echo $out | jq -M -r '.[0] | .hash')

# 5. the result of the invocation:
curl "$e:7777/get_receipt?hash=$h"
# {"ok":true,"result":123,"type":"python","log":""}
```

## More advanced usage: build from source

If you prefer build from source. Check out `build.sh` on linux or Mac, and `build.ps1` on
Windows. Make sure you have `cmake` and internet available, because the script
will create a `./.pre` folder and download some dependencies there.

# Wanna Know More? 
Read the Doc! : https://gitee.com/cong-jianer/wch/releases/download/v1.0/wch-latest-doc.pdf

If you have any issues, feel free to contact me at `congjianer@outlook.com` or open an issue.

