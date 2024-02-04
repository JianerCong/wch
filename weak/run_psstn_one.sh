#!/usr/bin/zsh -f

w=./build-weak/wch

# prepare the vars --------------------------------------------------
t=$PWD/my-tmp
dirs=($t/d1 $t/d2)

# prepare the dirs --------------------------------------------------
mkdir $dirs[1] -v -p
mkdir $dirs[2] -v -p

# start --------------------------------------------------
$w --port 7777 --light-exe --data-dir=$dirs[1]               # start as Solo-primary at 7777

# send one
txs='[
  {"from" : "01",
   "to" : "",
   "data" : "",
   "nonce" : 123
  }
]'
e=http://localhost
curl --data $txs $e:7777/add_txs
curl $e:7777/get_latest_Blk
# close --------------------------------------------------

# reopen and check --------------------------------------------------
$w --port 7777 --light-exe --data-dir=$dirs[1]               # start as Solo-primary at 7777
curl $e:7777/get_latest_Blk

# send another
txs='[
  {"from" : "01",
   "to" : "",
   "data" : "",
   "nonce" : 124
  }
]'
curl --data $txs $e:7777/add_txs
curl $e:7777/get_latest_Blk

rm $t -rv

# do M-x sh-set-shell RET zsh RET
