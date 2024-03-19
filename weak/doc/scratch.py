from typing import Any
def hi(_tx_context: dict[str, Any]) -> str:
    return _tx_context['from']
