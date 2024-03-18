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
    _storage["tx_from"] = _tx_context["from"]  # address hex string
    _storage["tx_to"] = _tx_context["to"]  # address hex string
    _storage["tx_hash"] = _tx_context["hash"]  # hex string
    _storage["tx_timestamp"] = _tx_context["timestamp"]  # int
    # 🦜 : storage is a dictionary that should be serializable into JSON

## 🦜: So this should produce the abi object:
"""
a = {
    'hi' : [],
    'plus_one' : ['x'],
    'set' : ['key', 'value', '_storage'],
    'get' : ['key', '_storage'],
    'init' : ['_storage', '_tx_context']
}

# 🦜 : and to invoke a method, use something like:

a = {
    'method' : 'plus_one',
    'args' : {'x' : 1}
}
"""
