w=./build-weak/wch

# mock-exe --------------------------------------------------
$w --port 7777 --mock-exe --consensus Rbft --Bft.node-list localhost:7777
# wait it
$w --port 7778 --mock-exe --consensus Rbft --Bft.node-list localhost:7777 # newcomer
$w --port 7779 --mock-exe --consensus Rbft --Bft.node-list localhost:7777 localhost:7778 # newcomer
$w --port 7780 --mock-exe --consensus Rbft --Bft.node-list localhost:7777 localhost:7778 localhost:7779 # newcomer
# Start as the single primary in bft cluster



# light-exe --------------------------------------------------
$w --port 7777 --light-exe --consensus Rbft --Bft.node-list localhost:7777
$w --port 7778 --light-exe --consensus Rbft --Bft.node-list localhost:7777 # newcomer


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
curl --data $txs $e:7777/add_txs
curl $e:7777/get_latest_Blk     # latest Blk number=2

clear && pytest -s build-weak/test_with_mocked_exe_bft.py::test_serv_send_basic_node_status
clear && pytest -s build-weak/test_with_mocked_exe_bft.py::test_serv_send_add_txs
clear && pytest -s build-weak/test_with_mocked_exe_bft.py::test_serv_two_nodes_add_txs
clear && pytest -s build-weak/test_with_mocked_exe_bft.py::test_serv_three_nodes_add_txs
clear && pytest -s build-weak/test_with_mocked_exe_bft.py::

