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

Dangerous_identifiers : list[str] = ['__import__', 'breakpoint', 'compile', 'eval', 'execfile', \
                                     'exec', 'get_ipython', 'globals', 'memoryview', 'help', \
                                     'id', 'input', 'open', 'quit', 'exit', 'runfile', 'vars']

Special_args = ['_storage', '_tx_context']
def verify_and_parse_func(py_file : TextIO, parse_it: bool = False) -> dict[str, dict[str, list[str]]] | bool:
    """
    Verifies the source code. source should be the content of a python file.
    """

    # 1. get tree
    py_code_content = py_file.read()
    py_file.seek(0)
    py_lines_for_debugging = py_file.readlines()
    # tree = ast.parse(py_code_content)
    tree = compile(py_code_content, filename='<string>', mode='exec', flags=ast.PyCF_ONLY_AST)
    # 🦜 : <2024-03-19 Tue> update, we should use `compile()` to get the tree
    # rather than `ast.parse()`, that checks for syntax errors.

    # 2. verify
    verify(tree, py_lines_for_debugging)

    # 3. parse_func
    if parse_it:
        return parse_func(tree, py_lines_for_debugging)
    return True

def verify_and_parse_func_str(py_code_content : str, parse_it : bool = False) -> dict[str, dict[str, list[str]]] | bool:
    py_lines_for_debugging = py_code_content.split('\n')
    tree = ast.parse(py_code_content)
    verify(tree, py_lines_for_debugging)
    if parse_it:
        return parse_func(tree, py_lines_for_debugging)
    return True

def verify(tree : ast, py_lines_for_debugging: list[str]):
    """
    Verify the source code.
    Throws AssertionError if the source code is not valid.
    """
    # print(f'🦜 : Verifying tree:\n {S.GREEN}{ast.dump(tree, indent=4)}{S.NOR} ')
    verify_top_level_statements(tree, py_lines_for_debugging)
    verify_imports_and_ids(tree, py_lines_for_debugging)

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

def debug_node_str(node: ast.AST, l: list[str]) -> str:
    """
    l : py_lines_for_debugging

    🦜 : This shows the ast node and the region of the code that it represents.
    """
    region = '\n'.join(l[node.lineno-1:node.end_lineno])
    rule = '-----------------------------------------------------------------'
    return (f'{node.__class__} at line {node.lineno}, col {node.col_offset}').ljust(len(rule), '-') + '\n' + \
        region + \
        '\n' + rule + '\n'

def verify_top_level_statements(tree : ast, lines: list[str]):
    for node in tree.body:
        # the top-level nodes
        # 🦜 : Also allow top-level const expr
        if isinstance(node, ast.Expr) and isinstance(node.value, ast.Constant):
            continue

        if not any([isinstance(node, allowed_node) for allowed_node in Allowed_top_level_statements]):
            raise AssertionError(f'verify_top_level_statements: Only `import`, `import from` and `def` are allowed at the top level\n' +  \
                                 'Error processing:' + debug_node_str(node, lines))

def verify_imports_and_ids(tree: ast, lines: list[str]):
    """
    Verify the imports and the identifiers in the source code.

    🐢 : We do a single walk and verify the imports and the identifiers.
    """
    for n in ast.walk(tree):
        verify_imports(n, lines)
        verify_identifiers(n, lines)

def verify_imports(n: ast, lines: list[str]):
    """Verify the imports in the source code.

    🐢 : For now, we walk through all the `import` and `import from` nodes to
    make sure that only those allowed modules are imported.
    """

    # print(f'🐢: Verifying node {n}')
    if isinstance(n, ast.ImportFrom):
        # 🦜 : module must be in the list
        if n.module not in Allowed_modules:
            raise AssertionError(f'verify_imports: Importing from module {n.module} is not allowed.\n' + \
                             debug_node_str(n, lines))
    elif isinstance(n, ast.Import):
        for alias in n.names:
            if alias.name not in Allowed_modules:
                raise AssertionError(f'verify_imports: Importing module {alias.name} is not allowed.\n' + debug_node_str(n, lines))

def verify_identifiers(n: ast, lines: list[str]):
    """Verify the identifiers in the source code.

    🦜 : For now, we simply verify that there's no node with an `id` field and
    the value of the `id` field is in the list of dangerous identifiers.
    """
    if not 'id' in n._fields:
        return
    if n.id in Dangerous_identifiers:
        raise AssertionError(f'verify_identifiers: Identifier `{n.id}` is not allowed.\n' + debug_node_str(n, lines))


# --------------------------------------------------
def parse_func(tree : ast, py_lines_for_debugging: list[str]) -> dict[str, dict[str, list[str]]]:
    """Parse the abi from the source code.

    🦜 : This walk through the top-level `FunctionDef` object and try to get
    the `method_name` and `args`

    Note that currently, there's no type checking. Also, default args are not
    ignored, so don't bother setting them...

    """
    o = dict()

    for node in tree.body:
        if isinstance(node, ast.FunctionDef):
            method_name = node.name
            # m = dict()
            m = []
            # 🦜 : <2024-03-18 Mon> : Let's now differentiate between `args` and `special_args`
            for a in [arg.arg for arg in node.args.args]:
                # a : str
                print(f'Processing arg {S.GREEN} {a} {S.NOR}')
                if a.startswith('_') and not a in Special_args:
                    raise AssertionError(f'parse_func: args starts with `_` are preserved for special use. Found {a}')
                #     print(f'\tAdding to {S.BLUE} special_args {S.NOR}')
                #     m['special_args'] = m.get('special_args', []) + [a]
                # else:
                #     print(f'\tAdding to {S.BLUE} args {S.NOR} ')
                #     m['args'] = m.get('args', []) + [a]
                m.append(a)
            o[method_name] = m

    check_init_func(o)

    return o


def check_init_func(o : dict[str, dict[str, list[str]]]):
    """
    The `init` method, if present, should only contain special_args.
    """
    if 'init' in o:
        # assert only special_args are present
        # if 'args' in o['init']:
        #     raise AssertionError(f'check_init_func: `init` method should not have any args, but found {o["init"]["args"]}')
        for a in o['init']:
            if a not in Special_args:
                raise AssertionError(f'check_init_func: `init` method should only have special_args, but found {a}')

import json
import os
if __name__ == '__main__':
    # 🦜 :read PWD/hi.py
    wd = os.environ["PWD"]
    print(f'🦜 Verifyer started: PWD = {wd}')
    p = os.path.join(wd, 'hi.py')
    print(f'🦜 : Verifying {p}')
    with open(p , 'r') as f:
        r = verify_and_parse_func(f, parse_it=True)
        # ^ 🦜 : Let it throw
        p1 = os.path.join(wd, 'verifier-result.json')
        print(f'🦜 : Writing to {p1}')
        with open(p1, "w") as f1:
            json.dump(r, f1)
