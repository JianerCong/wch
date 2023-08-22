w=./build-weak/wch

t=$PWD/my-tmp
dirs=($t/d1 $t/d2 $t/d3 $t/d4)

# prepare the dirs --------------------------------------------------
mkdir $t -v
mkdir $dirs[1] -v
mkdir $dirs[2] -v
mkdir $dirs[3] -v
mkdir $dirs[4] -v

# mock-exe --------------------------------------------------
$w --port 7777 --mock-exe --consensus Rbft --Bft.node-list localhost:7777
# wait it
$w --port 7778 --mock-exe --consensus Rbft --Bft.node-list localhost:7777 # newcomer
$w --port 7779 --mock-exe --consensus Rbft --Bft.node-list localhost:7777 localhost:7778 # newcomer
$w --port 7780 --mock-exe --consensus Rbft --Bft.node-list localhost:7777 localhost:7778 localhost:7779 # newcomer
# Start as the single primary in bft cluster


# args=(--light-exe --consensus Rbft --Bft.node-list localhost:7777 localhost:7778 localhost:7779 localhost:7780)
args=(--light-exe --consensus Rbft --Bft.node-list localhost:7777)
# light-exe --------------------------------------------------
$w --port 7777 $args --data-dir=$dirs[1]
$w --port 7778 $args --data-dir=$dirs[2]
$w --port 7779 $args --data-dir=$dirs[3]
$w --port 7780 $args --data-dir=$dirs[4]



# newcomer --------------------------------------------------
$w --port 7781 --light-exe --consensus Rbft --Bft.node-list localhost:7777 localhost:7778 localhost:7779 localhost:7780 

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

