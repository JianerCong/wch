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

"""
The following identifiers are considered dangerous and are not allowed in the source code.

    \texttt{\_\_import\_\_} &  import a module \\
    \texttt{breakpoint} &  enter the debugger \\
    \texttt{compile} &  compile a string into a code object \\
    \texttt{eval} &  evaluate a string as a python expression \\
    \texttt{execfile} & execute a file \\
    \texttt{exec} &  execute a string as a python statement \\
    \texttt{get\_ipython} &  get the current IPython instance \\
    \texttt{globals} &  return the global symbol table \\
    \texttt{memoryview} & create a memoryview object \\
    \texttt{help} &  get help on an object \\
    \texttt{id} & get the identity of an object, usually the memory address.
    This should be removed because the result is not predictable. \\
    \texttt{input} &  read a line from the standard input \\
    \texttt{open} &  open a file \\
    \texttt{quit / exit} &  exit the interpreter \\
    \texttt{runfile} &  run a file \\
    \texttt{vars} & return the \texttt{\_\_dict\_\_} attribute of an object \\
"""
Dangerous_identifiers : list[str] = ['__import__', 'breakpoint', 'compile', 'eval', 'execfile', \
                                     'exec', 'get_ipython', 'globals', 'memoryview', 'help', \
                                     'id', 'input', 'open', 'quit', 'exit', 'runfile', 'vars']

def verify_and_parse_func(py_file : TextIO) -> ast.AST:
    """
    Verifies the source code. source should be the content of a python file.
    """
    py_code_content = py_file.read()
    py_file.seek(0)
    py_lines_for_debugging = py_file.readlines()
    tree = ast.parse(py_code_content)
    verify(tree, py_lines_for_debugging)
    return tree

def verify_and_parse_func_str(py_code_content : str) -> ast.AST:
    py_lines_for_debugging = py_code_content.split('\n')
    tree = ast.parse(py_code_content)
    verify(tree, py_lines_for_debugging)
    return tree

def verify(tree : ast, py_lines_for_debugging: list[str]):
    """
    Verify the source code.
    Throws AssertionError if the source code is not valid.
    """
    print(f'🦜 : Verifying tree:\n {S.GREEN}{ast.dump(tree, indent=4)}{S.NOR} ')
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
