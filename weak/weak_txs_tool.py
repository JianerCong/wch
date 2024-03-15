"""
🦜 : Here is a simple curl of the add_txs_pb endpoint
"""
import requests_unixsocket
from google.protobuf import text_format
import hi_pb2
from pathlib import Path
session = requests_unixsocket.Session()
from tempfile import TemporaryFile
from typing import List, IO


def log(s):
    print('🦜 : ' + s)

def prepare_batches(n_tx_per_batch :int, n_batches : int) -> list[IO[bytes]]:
    txs = hi_pb2.Txs()
    tmp_files : list[IO[bytes]] = []
    nnonce=0
    for i in range(n_batches):
        f = TemporaryFile()
        for j in range(n_tx_per_batch):
            nnonce += 1
            tx = hi_pb2.Tx(type=hi_pb2.TxType.DATA, from_addr=b'1'*20, to_addr=b'', data=b'hi', nonce=nnonce)
            txs.txs.append(tx)
        # print('Writing to file...: ' + text_format.MessageToString(txs))
        f.write(txs.SerializeToString())
        tmp_files.append(f)
    return tmp_files

import requests
import time
def send_through_unix(url:str, tmp_files : list[IO[bytes]]):
    # start timmer
    oks : list[bool] = []
    start = time.time()         # seconds since the epoch
    # send the files
    for f in tmp_files:
        res = session.post(url + 'add_txs_pb', data=f.read())
        oks.append(res.ok)
    end = time.time()
    return oks, end-start

def send_through_http(url:str, tmp_files : list[IO[bytes]]):
    oks : list[bool] = []
    start = time.time()         # seconds since the epoch
    for f in tmp_files:
        res = requests.post(url + 'add_txs_pb', data=f.read())
        oks.append(res.ok)
    end = time.time()
    return oks, end-start

def weak_benchmark(n_tx_per_batch : int = 2, n_batches : int = 2,
                   url : str = 'http+unix://%2Ftmp%2Fhi-weak.sock/'):
    # 1. prepare the batch files
    tmp_files = prepare_batches(n_tx_per_batch, n_batches)
    # 2. start
    if url.startswith('http+unix'):
        oks, secs = send_through_unix(url,tmp_files)
    else:
        oks, secs = send_through_http(url,tmp_files)

    tps = n_tx_per_batch * n_batches / secs
    print('🦜 : Number of tests failed: ' + str(oks))
    print('🦜 : Number of seconds: ' + str(secs))
    print('🦜 : Tx per second: %d' % tps)

# try
# weak_benchmark()                # 1476
# weak_benchmark(10,2)                # 7 566
# weak_benchmark(100,2)                # 78 332
weak_benchmark(1000,2)                # 582 380
# weak_benchmark(1000,5)                # 1081 954
