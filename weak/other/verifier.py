import ast

from typing import TextIO
"""
The allowed top level statements
"""
Allowed_top_level_statements: list[ast.AST] = \
    [ast.Import, ast.ImportFrom, ast.FunctionDef]

"""
The allowed modules.
"""
Allowed_modules : list[str] = ['math', 'cmath', 'typing', 'hashlib', 'hmac']

def verify_and_parse_func(py_file : TextIO) -> ast.AST:
    """
    Verifies the source code. source should be the content of a python file.
    """
    py_code_content = py_file.read()
    py_file.seek(0)
    py_lines_for_debugging = py_file.readlines()
    tree = ast.parse(py_code_content)
    verify(tree, py_lines_for_debugging)

"""
Verify the source code.
Throws AssertionError if the source code is not valid.
"""
def verify(tree : ast, py_lines_for_debugging: list[str]):
    verify_top_level_statements(tree, py_lines_for_debugging)
    verify_imports(tree, py_lines_for_debugging)

def verify_top_level_statements(tree : ast, py_lines_for_debugging: list[str]):
    for node in tree.body:
        # the top-level nodes

def verify_imports(tree : ast, py_lines_for_debugging: list[str]):
    pass
