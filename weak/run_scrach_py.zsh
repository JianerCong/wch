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
out=$(curl --data $txs $e:7777/add_txs)
echo $out | jq .
da=$(echo $out | jq -M -r '.[0] | .deployed_address')
# -M, --monochrome
# -r, --raw-output : output strings as they are
# echo $out | jq '.[0] | .hash' # the hash

# deployed address
curl $e:7777/get_latest_Blk | jq .
# invoke the contract set 123
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
out=$(curl --data $txs $e:7777/add_txs)
echo $out | jq .
h=$(echo $out | jq -M -r '.[0] | .hash')
echo $h
curl "$e:7777/get_receipt?hash=$h" | jq .
