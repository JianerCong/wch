w=./build-weak/wch
# start quitely 🦜 : But it will only be quite during execution.
# $w --port 7777 --light-exe --verbose no
# start it
$w --port 7777 --mock-exe --unix-socket /tmp/hi-weak.sock


echo '
from typing import Any
def hi(_storage: dict[str, Any], y : int) -> int:
    _storage["x"] = _storage.get("x", 0) + y
    return _storage["x"]
' > hi.py

txs='[{"from" : "01","to" : "",
 "data" : "'$(jq -Rs . hi.py)'",
 "nonce" : 123,
 "type" : "python"
}]'
# jq converts the text file into valid jsonstring

e=http://localhost
curl --unix-socket /tmp/hi-weak.sock \
     --data $txs \
     $e/add_txs
