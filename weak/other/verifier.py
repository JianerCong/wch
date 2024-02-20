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

def debug_node_str(node: ast.AST, l: list[str]):
    """
    l : py_lines_for_debugging
    """
    region = '\n'.join(l[node.lineno-1:node.end_lineno])
    rule = '-----------------------------------------------------------------'
    return (f'{node.__class__} at line {node.lineno}, col {node.col_offset}').ljust(len(rule), '-') + '\n' + \
        region + \
        '\n' + rule + '\n'

def verify_top_level_statements(tree : ast, lines: list[str]):
    for node in tree.body:
        # the top-level nodes
        if not all([isinstance(node, allowed_node) for allowed_node in Allowed_top_level_statements]):
            raise AssertionError('Only')

def verify_imports(tree : ast, lines: list[str]):
    pass
