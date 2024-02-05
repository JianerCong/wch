w=./build-weak/wch

# start as Solo-primary at 7777
$w --port 7777 --mock-exe --unix-socket /tmp/hi-weak.sock


e=http:/localhost               # <- 🦜 `localhost` is set to the Host header.
curl --unix-socket /tmp/hi-weak.sock $e/get_node_status

# 🦜 : send a request via socket, to see
txs='[
  {"from" : "01",
   "to" : "",
   "data" : "608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033",
   "nonce" : 123
  }
]'
curl --unix-socket /tmp/hi-weak.sock $e/get_latest_Blk
# => [] : no Blk

clear &&  pytest -s build-weak/test_with_unix_and_pb.py::test_serv_open_close


# --------------------------------------------------
# python client
pip install requests-unixsocket

x='syntax = "proto3";
package hi;
message Parrot {
  string name = 1;
  int32 age = 2;
}'
echo "$x" > hi.proto
protoc -I=. --pyi_out=. --python_out=. ./hi.proto
