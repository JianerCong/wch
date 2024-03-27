"""
🦜 : Here is a simple curl of the add_txs_pb endpoint
"""
import requests_unixsocket
from google.protobuf import text_format
import hi_pb2
from pathlib import Path
session = requests_unixsocket.Session()

import asyncio

def log(s):
    print('🦜 : ' + s)

def prepare_batches(n_tx_per_batch :int, n_batches : int) -> list[bytes]:
    txs = hi_pb2.Txs()
    o : list[bytes] = []
    nnonce=0
    for i in range(n_batches):
        for j in range(n_tx_per_batch):
            nnonce += 1
            tx = hi_pb2.Tx(type=hi_pb2.TxType.DATA, from_addr=b'1'*20, to_addr=b'', data=b'hi', nonce=nnonce)
            txs.txs.append(tx)
        # print('Writing to file...: ' + text_format.MessageToString(txs))
        o.append(txs.SerializeToString())
    return o

import requests
import time

async def send_one(url:str, txs:bytes, stub):
    res = stub.post(url + 'add_txs_pb', data=txs)
    return res.ok

async def send_it(url:str, txs_l: list[bytes], stub, ctps: bool):
    # start timmer
    oks : list[bool] = []
    start = time.time()         # seconds since the epoch
    end = 0
    # send the files
    print(f'🦜 : Testing ctbs : {ctps}')

    tasks = []
    for txs in txs_l:           # 🦜 : parallel for
        tasks.append(asyncio.create_task(send_one(url, txs, stub)))

    if not ctps:
        end = time.time()             # [1]🦜 : We can do it here (just put in pool, this is tps)

    for task in tasks:
        oks.append(await task)

    if ctps:
        end = time.time()           # [2]🦜 : or here (concurrent waiting, this is same as ctps)
    return oks, end-start

# def send_through_unix(url:str, txs_l: list[bytes]):
#     return send_it(url, txs_l, session)
# def send_through_http(url:str, txs_l: list[bytes]):
#     return send_it(url, txs_l, requests)

async def weak_benchmark(n_tx_per_batch : int = 2, n_batches : int = 2,
                         ctps : bool = False,
                         url : str = 'http+unix://%2Ftmp%2Fhi-weak.sock/'):

    # 1. prepare the batch files
    txs_l = prepare_batches(n_tx_per_batch, n_batches)
    # 2. start
    if url.startswith('http+unix'):
        # oks, secs = send_through_unix(url,tmp_files)
        oks, secs = await send_it(url, txs_l, session, ctps)
    else:
        # oks, secs = send_through_http(url,tmp_files)
        oks, secs = await send_it(url, txs_l, requests, ctps)

    tps = n_tx_per_batch * n_batches / secs
    print('🦜 : Tests oked: ' + str(oks))
    print('🦜 : Number of seconds: ' + str(secs))
    print('🦜 : Tx per second: %d' % tps)

# try to run (tx-per-batches = argv[1], n-batches argv[2], ctps = argv[3])
from sys import argv
import sys
if __name__ == '__main__':
    # weak_benchmark()                # 1476
    if len(argv) >= 3:
        # default to tps
        if len(argv) == 3:
            asyncio.run(weak_benchmark(int(argv[1]), int(argv[2])))
        else:
            asyncio.run(weak_benchmark(int(argv[1]), int(argv[2]), bool(argv[3])))
        sys.exit(0)
    # weak_benchmark(10,2)                # 7 566
    asyncio.run(weak_benchmark(10,2))                # 7 566
    # weak_benchmark(100,2)                # 78 332
    # weak_benchmark(1000,2)                # 582 380
    # weak_benchmark(1000,5)                # 1081 954
