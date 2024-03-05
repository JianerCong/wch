import pytest, ast
from verifier import verify_and_parse_func_str, verify_and_parse_func

class S:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    MAG = '\033[93m'
    RED = '\033[91m'
    NOR = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def assert_bad_top_level_statements(s: str):
    # with pytest.raises(AssertionError) as e:
    with pytest.raises(AssertionError) as e:
        tree = verify_and_parse_func_str(s)

    print(f'🚮️: captured e.value:{S.CYAN} {e.value} {S.NOR}')
    assert str(e.value).startswith('verify_top_level_statements')

def assert_ok_verify(s: str):
    ok = verify_and_parse_func_str(s)
    assert ok

def test_ok_top_level():
    s = """
def hi() -> str:
    return "hi"
    """
    assert_ok_verify(s)
    s = """
# I am a comment
\"\"\" This is a doc string \"\"\"
def f(x:int, y:int) -> int:
    return x + y
"""
    assert_ok_verify(s)

def test_bad_top_levels():
    s = """
x = 1
    """
    assert_bad_top_level_statements(s)

    s = """
def hi() -> str:
    return "hi"

x = 1
    """
    assert_bad_top_level_statements(s)

def test_ok_imports():
    s = """
import math

def hi() -> str:
    from math import sin
    return sin(1.0)
    """
    assert_ok_verify(s)

def assert_bad_imports(s: str):
    with pytest.raises(AssertionError) as e:
        tree = verify_and_parse_func_str(s)

    print(f'🚮️: captured e.value:{S.CYAN} {e.value} {S.NOR}')
    assert str(e.value).startswith('verify_imports')

def test_bad_imports():
    s = """
import sys
    """
    assert_bad_imports(s)

    s = """
import math

def hi() -> str:
    from sys import api_version
    return str(api_version)
    """
    assert_bad_imports(s)       # 🦜 : `import sys` is not allowed

def assert_bad_ids(s: str):
    with pytest.raises(AssertionError) as e:
        tree = verify_and_parse_func_str(s)

    print(f'🚮️: captured e.value:{S.CYAN} {e.value} {S.NOR}')
    assert str(e.value).startswith('verify_identifiers')

def test_bad_verify_ids():
    s = """
def hi() -> str:
    x = open('hi.txt', 'r')
    s = x.read()
    x.close()
    return s
    """

    assert_bad_ids(s)

    s = """
def hi() -> str:
    open = 'hi'
    return open
    """
    assert_bad_ids(s)

    s = """
def hi(f=open('hi.txt')):
    return f
   """
    assert_bad_ids(s)

def test_ok_parse_func():
    s = """
def hi() -> str:
    return "hi"
    """
    o = verify_and_parse_func_str(s, parse_it=True)
    print(f'🦜 : o:{S.CYAN} {o} {S.NOR}')
    o0 = {'hi' : {}}
    assert o == o0


@pytest.mark.parametrize(
    "s,o0",
    [
        ("""
def set(key: str, value: Any, _storage: dict[str,Any]) -> None:
    _storage[key] = value
        """,
        {'set' : {'args': ['key', 'value'], 'special_args' : ['_storage']}}
            ),
        # --------------------------------------------------

        (
            """
def init(_storage: dict[str,Any], _tx_context : dict[str,Any]) -> None:
    _storage["tx_origin"] = _tx_context["tx_origin"]  # address hex string
            """,
            {'init' : {'special_args' : ['_storage', '_tx_context']}}
        ),

        # --------------------------------------------------
        (
            """
def plus_one(x : int) -> int:
    return x + 1
            """,
            {'plus_one' : {'args': ['x']}}
        ),

        # --------------------------------------------------
        (
            """
def get(key: str, _storage: dict[str,Any]) -> Any:
    return _storage[key]
            """,
            {'get' : {'args': ['key'], 'special_args' : ['_storage']}}
        ),
    ])
def test_more_ok_parse_func(s, o0):
    o = verify_and_parse_func_str(s, parse_it=True)
    print(f'🦜 : o:{S.CYAN} {o} {S.NOR}')
    assert o == o0

def test_example_files_parse():
    f = open('example/ok-basic.py', 'r')
    o0 = {
    'hi' : {},
    'plus_one' : {'args': ['x']},
    'set' : {'args': ['key', 'value'], 'special_args' : ['_storage']},
    'get' : {'args': ['key'], 'special_args' : ['_storage']},
    'init' : {'special_args' : ['_storage', '_tx_context']}
    }
    o = verify_and_parse_func(f, parse_it=True)
    print(f'🦜 : o:{S.CYAN} {o} {S.NOR}')
    assert o == o0
