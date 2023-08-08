w=./build-weak/wch

# prepare the vars --------------------------------------------------
t=$PWD/my-tmp
dirs=($t/d1 $t/d2 $t/d3)

# prepare the dirs --------------------------------------------------
mkdir $t -v
mkdir $dirs[1] -v
mkdir $dirs[2] -v
mkdir $dirs[3] -v

# start --------------------------------------------------
$w --port 7777 --light-exe --data-dir=$dirs[1]               # start as Solo-primary at 7777
$w --port 7778 --light-exe --consensus  Solo --Solo.node-to-connect localhost:7777 --data-dir=$dirs[2]
$w --port 7779 --light-exe --consensus  Solo --Solo.node-to-connect localhost:7777 --data-dir=$dirs[3]

sudo lsof -i -P -n | grep LISTEN

# 🦜 send one To N1
txs='[
  {"from" : "01",
   "to" : "",
   "data" : "",
   "nonce" : 123
  }
]'
e=http://localhost
curl --data $txs $e:7777/add_txs
# get from N2
curl $e:7778/get_latest_Blk
# close --------------------------------------------------

# reopen and check --------------------------------------------------
curl $e:7777/get_latest_Blk
curl $e:7778/get_latest_Blk

# send another
txs='[
  {"from" : "01",
   "to" : "",
   "data" : "",
   "nonce" : 124
  }
]'
curl --data $txs $e:7777/add_txs
curl $e:7778/get_latest_Blk
# $w --port 7778 --mock-exe --consensus  Solo --Solo.node-to-connect localhost:7777 --data-dir=dirs[1]

# close both

# clean up
rm $t -rv
