from typing import Any
from math import sqrt

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

## 🦜: So this should produce the abi object:
"""
a = {
    'hi' : {},
    'plus_one' : {'args': ['x']},
    'set' : {'args': ['key', 'value'], 'special_args' : ['_storage']},
    'get' : {'args': ['key'], 'special_args' : ['_storage']},
    'init' : {'special_args' : ['_storage', '_tx_context']}
}

# 🦜 : and to invoke a method, use something like:

a = {
    'method' : 'plus_one',
    'args' : {'x' : 1}
}
"""
