![Logo](./weak/doc/logo.svg)

[**English**](./README.md) | [**中文**](./README_CN.md)

<!-- # Welcome to Weak chain -->
# 欢迎来到早链
<!-- *Weak chain* is a blockchain written in C++20. It's designed to be a working -->
<!-- private chain that's as simple as possible in the hope that it could help you -->
<!-- get started with blockchain. -->

早链是一款用C++20编写的区块链。它旨在成为尽可能简单的联盟链，希望它能帮助您扫盲区块链。

## 招牌

+ 下载即用的二进制文件，支持Windows x64和Linux x86/x64（< 50MB）
+ 三种共识算法：Raft、PBFT、Solo；PBFT支持动态增加节点
+ EVM和Python智能合约
+ 纯数据交易。更简单的“数据上链”
+ 使用UDP和Protobuf的高效P2P网络
+ 无内置加密货币
+ 内置命令行加密工具箱，帮助您创建和使用数字签名。无需像`openssl`这样的第三方工具。

**📗️GPLv3：本项目仅供学习和研究使用，请勿用于商业用途。特别注意，本项目不是为了与任何类似区块链项目或产品对标而设计。**


<!-- # Get started -->
<!-- Start a node in Solo mode, listening on port 7777 -->
# 开始
在端口7777上启动一个Solo模式的节点
```bash
wch --port 7777
# 回车退出
```

<!-- ## Simple usage: Put some data on the chain: -->
## 简单用法：将一些数据放到链上：
```bash
# 1.put some data on the chain
txs='[{"type" : "data", "from" : "01", "to" : "", "data" : "hi", "nonce" : 123}]'
curl --data $txs http://localhost:7777/add_txs

# [{"hash":"b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b"}]

# 2. get the uploaded data
curl http://localhost:7777/get_tx?hash=b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b

# {"from":"0000000000000000000000000000000000000001","to":"0000000000000000000000000000000000000000",
# "nonce":123,"timestamp":1717141105,"hash":"b06b89b665df6f7ff1be967faf9a0601f71c0a3cdb8250c48fe7fcc663b18d1b",
# "type":"data","data":"hi"}
```

## 更高级的用法：Python智能合约
📗️ :确保您的环境中有`python3`。

```bash
# 1. prepare the contract
echo '
from typing import Any
def hi(_storage: dict[str, Any], y : int) -> int:
    _storage["x"] = _storage.get("x", 1) + y
    return _storage["x"]
' > /tmp/tmp.py

txs='[{"from" : "01","to" : "",
 "data" : "@/tmp/tmp.py",
 "nonce" : 123,
 "type" : "python"
}]'

# 2. deploy the contract (use jq to capture the output)
e=http://localhost
out=$(curl --data $txs $e:7777/add_txs)
da=$(echo $out | jq -M -r '.[0] | .deployed_address')

# 3. prepare the invoke transaction
echo '{
        "method" : "hi",
        "args" : {"y" : 122}
}' > /tmp/tmp.json

txs='[{
    "from" : "01",
    "to" : "'"$da"'",
    "data" : "@/tmp/tmp.json",
    "nonce" : 124,
    "type" : "python"
}]
'

# 4. invoke
out=$(curl --data $txs $e:7777/add_txs)
h=$(echo $out | jq -M -r '.[0] | .hash')

# 5. the result of the invocation:
curl "$e:7777/get_receipt?hash=$h"
# {"ok":true,"result":123,"type":"python","log":""}
```

<!-- ## More advanced usage: build from source -->

## 更高级的用法：从源代码构建

<!-- If you prefer build from source. Check out `build.sh` on linux or Mac, and `build.ps1` on -->
<!-- Windows. Make sure you have `cmake` and internet available, because the script -->
<!-- will create a `./.pre` folder and download some dependencies there. -->
如果您更喜欢从源代码构建，请查看`build.sh`（Linux或Mac）和`build.ps1`（Windows）。请确保您的计算机上安装了`cmake`和联网，因为脚本会在创建一个`.pre`文件夹，并在里面下载一些依赖项。

<!-- # Wanna Know More?  -->
<!-- Read the Doc! : https://gitee.com/cong-jianer/wch/releases/download/v1.0/wch-latest-doc.pdf -->

<!-- If you have any issues, feel free to contact me at `congjianer@outlook.com` or open an issue. -->

# 想了解更多？
阅读文档！: https://gitee.com/cong-jianer/wch/releases/download/v1.0/wch-latest-doc.pdf

如果您有任何问题，请随时通过`congjianer@outlook.com`联系我， 或者提交一个issue。
