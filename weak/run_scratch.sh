# Start the chain

w=./build-weak/wch
# start quitely 🦜 : But it will only be quite during execution.
# $w --port 7777 --light-exe --verbose no
$w --port 7777 --light-exe
$w --port 7778 --light-exe --consensus  Solo --Solo.node-to-connect localhost:7777

$w --port 7777 --mock-exe               # start as Solo-primary at 7777
$w --port 7778 --mock-exe --consensus  Solo --Solo.node-to-connect localhost:7777

# --------------------------------------------------
# starts manually the normal exe
$w --port 7777
$w --port 7778 --consensus  Solo --Solo.node-to-connect localhost:7777

# --------------------------------------------------
# starts manually the light exe
$w --port 7777 --light-exe
$w --port 7778 --light-exe --consensus  Solo --Solo.node-to-connect localhost:7777

# deploy a contract --------------------------------------------------
# --------------------------------------------------
# init="608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033"
txs='[
  {"from" : "01",
   "to" : "",
   "data" : "608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033",
   "nonce" : 123
  }
]'
e=http://localhost
curl --data $txs $e:7777/add_txs
deployed_addr="0000000000000000000000000000000064d1bf89"

# [
#     {
#         "hash": "b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b",
#         "deployed_address": "0000000000000000000000000000000064d05d1b"
#     }
# ]
# Get the latest Blk, number=0
curl $e:7777/get_latest_Blk
curl $e:7778/get_latest_Blk     # get from node 1
# Get the corresponding receipt

# curl "$e:7777/get_receipt?hash=b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b"
curl "$e:7777/get_receipt?hash=a46e1806155b08cf95763b2e2dabeebff40859727d07d457d75a23069a66ca15"
# b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b"

# set 123 --------------------------------------------------
# --------------------------------------------------
txs="[
  {
  \"from\" : \"01\",
  \"to\" : \"$deployed_addr\",
   \"data\" : \"60fe47b1000000000000000000000000000000000000000000000000000000000000007b\",
   \"nonce\" : 125
  }
]"
curl --data $txs $e:7778/add_txs # call to node 1
curl --data $txs $e:7777/add_txs
# [{"hash":"a46e1806155b08cf95763b2e2dabeebff40859727d07d457d75a23069a66ca15"}]
curl $e:7777/get_latest_Blk     # latest Blk number=1

# get 123 --------------------------------------------------

txs="[
  {
  \"from\" : \"01\",
  \"to\" : \"$deployed_addr\",
   \"data\" : \"6d4ce63c\",
   \"nonce\" : 126
  }
]"
curl --data $txs $e:7777/add_txs
# [{"hash":"f192381aa197b851870e98fc515414c51d2311fea7e5bc01c16dceb6a7fed3d9"}]
curl $e:7777/get_latest_Blk     # latest Blk number=2
curl "$e:7777/get_receipt?hash=c63fce1da4f0a30c0ae6c8ce332286f188d71dc13638beca74fb5939c5a79ec8"
curl "$e:7777/get_receipt?hash=c63fce1da4f0a30c0ae6c8ce332286f188d71dc13638beca74fb5939c5a79ec8"

# 🦜 : The result of get() is 0x7b = 123
# {"ok":true,"result":"000000000000000000000000000000000000000000000000000000000000007b"}


# send to node 2 --------------------------------------------------
# --------------------------------------------------
# pkill wch
sudo lsof -i -P -n | grep LISTEN

e=http://localhost
e1=$e:7781
curl $e1/get_node_status
curl $e1/get_pool_status

e=http://localhost
txs='[
  {"from" : "01",
   "to" : "02",
   "data" : "ffff",
   "nonce" : 123
  },
  {"from" : "01",
   "to" : "",
   "data" : "ffff",
   "nonce" : 123
  }
]'
curl --data $txs $e:7778/add_txs

# --------------------------------------------------
# run subset of tests individual test

clear && pytest -s build-weak/test_with_mocked_exe.py
clear && pytest -s build-weak/test_with_mocked_exe.py::test_serv_two_nodes_add_txs
clear && pytest -s build-weak/test_with_mocked_exe.py::test_serv_three_nodes_add_txs


clear && pytest -s build-weak/test_with_light_exe.py::test_serv_open_close
clear && pytest -s build-weak/test_with_light_exe.py::test_single_primary_set_get_123
clear && pytest -s build-weak/test_with_light_exe.py::test_two_nodes_set_get_123

clear &&  pytest -s build-weak/test_with_normal_exe.py
clear &&  pytest -s build-weak/test_with_normal_exe.py::test_serv_open_close
clear &&  pytest -s build-weak/test_with_normal_exe.py::test_serv_send_add_txs
clear &&  pytest -s build-weak/test_with_normal_exe.py::test_serv_send_add_txs_and_check_pool
clear &&  pytest -s build-weak/test_with_normal_exe.py::test_serv_two_nodes_add_txs


clear &&  pytest -s build-weak/test_psstn.py::test_one
clear &&  pytest -s build-weak/test_psstn.py::test_two
clear &&  pytest -s build-weak/test_psstn.py
