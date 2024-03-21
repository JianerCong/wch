from typing import Any
def hi(_storage: dict[str, Any], y : int) -> int:
    _storage['x'] = _storage.get('x', 0) + y
    return _storage['x']
