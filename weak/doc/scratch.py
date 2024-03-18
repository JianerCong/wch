from typing import Any

def hi() -> str:
    return "hi"

def plus_one(x : int) -> int:
    return x + 1

def set(key: str, value: Any, _storage: dict[str,Any]) -> None:
    _storage[key] = value

def get(key: str, _storage: dict[str,Any]) -> Any:
    return _storage[key]

def init(_storage: dict[str,Any], _tx_context : dict[str,Any]) -> None:
    _storage["tx_origin"] = _tx_context["tx_origin"]  # address hex string
    _storage["block_number"] = _tx_context["block_number"]  # int
    _storage["tx_timestamp"] = _tx_context["tx_timestamp"]  # int
    # 🦜 : storage is a dictionary that should be serializable into JSON

import json
args = None
with open('args.json', 'r') as f:
    args = json.load(f)


r = init(**args)

def hi(x,y):
    print(f'x={x}, y={y}')

args = {'x' : 1, 'y' : 2}
hi(**args)


args = {'x' : 1, 'y' : 2, 'z' : 'hi'}
hi(**args)


args = {'x' : 1}
hi(**args)
