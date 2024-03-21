w=./build-weak/wch
# start quitely 🦜 : But it will only be quite during execution.
# $w --port 7777 --light-exe --verbose no
# start it
$w --port 7777 --light-exe

echo '
from typing import Any
def hi(_storage: dict[str, Any], y : int) -> int:
    _storage["x"] = _storage.get("x", 0) + y
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
deployed_addr="000000000000000000000000ffffffff9a04023e"

curl $e:7777/get_latest_Blk
