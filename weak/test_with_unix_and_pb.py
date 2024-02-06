import requests_unixsocket
from google.protobuf import text_format
import hi_pb2
from helpers import *

session = requests_unixsocket.Session()
port = 7777

def close_and_check(p):
    o,e = p.communicate('\n')         # send a \n and wait for completion
    if p.returncode != 0:
        print(f'OUT--------------------------------------------------')
        print(o)
        print(f'ERR--------------------------------------------------')
        print(e)
    assert p.returncode == 0

def test_serv_open_close(): #9
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    p = Popen([wc, '--port', str(port), '--mock-exe','--unix-socket', '/tmp/hi-weak.sock'],
              stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    close_and_check(p)

def test_serv_send_basic_node_status(): #19
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    port += 1

    url = f'http+unix://%2Ftmp%2Fhi-weak.sock/'
    p = Popen([wc, '--port', str(port), '--mock-exe','--unix-socket', '/tmp/hi-weak.sock'],
              stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)

    time.sleep(2)                   #  wait until is up
    try:
        result = session.get(url + 'get_node_status')
        assert result.ok
    finally:
        close_and_check(p)

def test_add_txs(): #29

    """
message Tx {
  TxType type = 1;
  bytes from_addr = 2;
  bytes to_addr = 3;
  bytes data = 4;
  uint64 timestamp = 5;         // [not needed in rpc]
  uint64 nonce = 6;
  bytes hash = 7;               // will be serialized, but not parsed

  // 🦜 : The following 3 are optional in rpc param
  string pk_pem = 8;
  bytes signature = 9;          // key.sign(hash)
  bytes pk_crt = 10;            // ca_key.sign(pk_pem)
}                               // [x]

message Txs {repeated Tx txs = 1;} // [x]


message AddTxReply {
  bytes hash = 1;
  bytes deployed_addr = 2;
}

message AddTxsReply {
  repeated AddTxReply txs = 1;
}
    """

    # 0. prepare the txs
    txs = hi_pb2.Txs(
        txs=[
            hi_pb2.Tx(type=hi_pb2.TxType.DATA, from_addr=b'1'*20, to_addr=b'0'*20, data=b'hi', nonce=1),
            hi_pb2.Tx(type=hi_pb2.TxType.DATA, from_addr=b'1'*20, to_addr=b'0'*20, data=b'hi', nonce=2),
            ]
        )
    s = txs.SerializeToString()
    print('🦜 : Got txs of binary size: %d' % txs.ByteSize())
    print('To be sent txs: ' + text_format.MessageToString(txs))

    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    # global port
    # port += 1

    # # 1. prepare the chain
    p = Popen([wc, '--port', str(port), '--mock-exe','--unix-socket', '/tmp/hi-weak.sock'],
              stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    time.sleep(2)                   #  wait until is up

    try:
        url = f'http+unix://%2Ftmp%2Fhi-weak.sock/'
        # --------------------------------------------------
        result = session.post(url + 'add_txs_pb', data=txs.SerializeToString())
        print('🦜 : Got result: ' + str(result))
        assert result.ok

        # 2. parse the result
        add_txs_reply = hi_pb2.AddTxsReply()
        add_txs_reply.ParseFromString(result.content)
        print('Got add_txs_reply: ' + text_format.MessageToString(add_txs_reply))
    finally:
        close_and_check(p)
