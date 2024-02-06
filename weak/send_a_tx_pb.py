
"""
🦜 : Here is a simple curl of the add_txs_pb endpoint
"""
import requests_unixsocket
from google.protobuf import text_format
import hi_pb2
session = requests_unixsocket.Session()

# 0. prepare the txs
txs = hi_pb2.Txs(
    txs=[
        hi_pb2.Tx(type=hi_pb2.TxType.DATA, from_addr=b'1', to_addr=b'', data=b'hi', nonce=1),
        hi_pb2.Tx(type=hi_pb2.TxType.DATA, from_addr=b'1', to_addr=b'', data=b'hi', nonce=2),
    ]
)
s = txs.SerializeToString()
print('🦜 : Got txs of binary size: %d' % txs.ByteSize())
print('To be sent txs: ' + text_format.MessageToString(txs))
url = f'http+unix://%2Ftmp%2Fhi-weak.sock/'
# --------------------------------------------------
result = session.post(url + 'add_txs_pb', data=txs.SerializeToString())
print('🦜 : Got result: ' + str(result))
assert result.ok

# 2. parse the result
add_txs_reply = hi_pb2.AddTxsReply()
add_txs_reply.ParseFromString(result.content)
print('Got add_txs_reply: ' + text_format.MessageToString(add_txs_reply))
