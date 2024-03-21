w=./build-weak/wch
# start quitely 🦜 : But it will only be quite during execution.
# $w --port 7777 --light-exe --verbose no
# start it
$w --port 7777 --light-exe

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
# jq converts the text file into valid jsonstring

e=http://localhost
curl --data $txs $e:7777/add_txs

# deployed address
curl $e:7777/get_latest_Blk

# invoke the contract set 123
echo '{
        "method" : "hi",
        "args" : {"y" : 122}
}' > /tmp/tmp.json
deployed_addr="000000000000000000000000ffffffff9a03fc88"
txs='[{
    "from" : "01",
    "to" : "'"$deployed_addr"'",
    "data" : "@/tmp/tmp.json",
    "nonce" : 124,
    "type" : "python"
}]
'
curl --data $txs $e:7777/add_txs
h=a46e1806155b08cf95763b2e2dabeebff40859727d07d457d75a23069a66ca15
curl "$e:7777/get_receipt?hash=$h"
