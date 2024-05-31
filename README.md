![Logo](./weak/doc/logo.svg)
# Welcome to Weak chain
*Weak chain* is a blockchain written in C++20. It's designed to be a working
private chain that's as simple as possible in the hope that it could help you
get started with blockchain.

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

# 2. get the data
curl http://localhost:7777/get_tx?hash=b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b

# {"from":"0000000000000000000000000000000000000001","to":"0000000000000000000000000000000000000000",
# "nonce":123,"timestamp":1717141105,"hash":"b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b",
# "type":"data","data":"hi"}
```

## More advanced usage: Python Smart Contract
```bash
# 1. prepare the contract
echo '
from typing import Any
def hi(_storage: dict[str, Any], y : int) -> int:
    _storage["x"] = _storage.get("x", 1) + y
    return _storage["x"]
' > /tmp/tmp.py

txs='[{"from" : "01","to" : "",
 "data" : "@/tmp/tmp.py",
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
}' > /tmp/tmp.json

txs='[{
    "from" : "01",
    "to" : "'"$da"'",
    "data" : "@/tmp/tmp.json",
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

# Wanna Know More? 
Read the Doc! : https://gitee.com/cong-jianer/wch/releases/download/v1.0/wch-doc-20240531.pdf

If you have any issues, feel free to contact me at `congjianer@outlook.com` or open an issue.
