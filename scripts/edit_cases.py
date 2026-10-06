#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = []
# ///
r"""Change matching case actions without running benchmarks.

Examples:
  uv run scripts/edit_cases.py cases.txt 'N is not None and N >= 2**24' > cases-small.txt
  uv run scripts/edit_cases.py cases.txt \
    'N is not None and N >= 2**24 and batch_size != 4096' --in-place
  uv run scripts/edit_cases.py cases.txt 'project == "sonic"' --action comment --in-place

Expressions can use N, batch_size, project, variant, name, and params.
N and batch_size are numbers, or None when missing/symbolic (e.g. N=capacity).
params contains comma-separated key=value parameters, converted to int/float
where possible; other values remain strings. Use params.get("threads", 1), etc.
Comparisons, arithmetic, membership, and/or/not, and params.get() are supported;
other function calls and attribute access are not. The result must be boolean.

Only matching action fields change; case identities, locations, and unmatched
lines are preserved. Ordinary comments are untouched. --action comment prefixes
matching cases with '# '; --action run/yes can explicitly re-enable them.
Without --in-place, the edited copy goes to stdout. Never redirect to the input
file itself: use --in-place, which validates everything before atomic replacement.
"""
import argparse
import ast
import os
from pathlib import Path
import stat
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "benchmark" / "common"))
from common import parse_selector

FIELDS = {"N", "batch_size", "project", "variant", "name", "params"}


def parameter_value(value):
    for convert in (int, float):
        try:
            return convert(value)
        except ValueError:
            pass
    return value


def expression_fields(identity):
    project, variant, name, raw_params = identity
    params = {}
    for part in raw_params.split(","):
        key, separator, value = part.partition("=")
        if not separator:
            continue
        key = key.strip()
        if key in params:
            raise ValueError(f"duplicate parameter {key!r}")
        params[key] = parameter_value(value.strip())
    fields = dict(project=project, variant=variant, name=name, params=params)
    for key in ("N", "batch_size"):
        value = params.get(key)
        fields[key] = value if isinstance(value, (int, float)) else None
    return fields


def compile_expression(expression):
    try:
        tree = ast.parse(expression, mode="eval")
    except SyntaxError as error:
        raise ValueError(f"invalid expression: {error.msg}") from error
    allowed = (ast.Expression, ast.Constant, ast.Name, ast.Load, ast.BoolOp,
               ast.And, ast.Or, ast.UnaryOp, ast.Not, ast.UAdd, ast.USub,
               ast.BinOp, ast.Add, ast.Sub, ast.Mult, ast.Div, ast.FloorDiv,
               ast.Mod, ast.Pow, ast.LShift, ast.RShift, ast.BitAnd, ast.BitOr,
               ast.BitXor, ast.Compare, ast.Eq, ast.NotEq, ast.Lt, ast.LtE,
               ast.Gt, ast.GtE, ast.Is, ast.IsNot, ast.In, ast.NotIn,
               ast.List, ast.Tuple, ast.Set, ast.Dict, ast.Subscript, ast.Slice)

    def is_params_get(node):
        return (isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name)
                and node.value.id == "params" and node.attr == "get")

    for node in ast.walk(tree):
        if isinstance(node, ast.Name) and node.id not in FIELDS:
            raise ValueError(f"unknown field {node.id!r}; use params.get({node.id!r}) for other parameters")
        if is_params_get(node):
            continue
        if isinstance(node, ast.Call) and is_params_get(node.func) and not node.keywords and 1 <= len(node.args) <= 2:
            continue
        if not isinstance(node, allowed):
            raise ValueError(f"unsupported expression syntax: {type(node).__name__}")
    return compile(tree, "<case expression>", "eval")


def edit_cases(text, expression, action="skip"):
    code = compile_expression(expression)
    parse_selector(text)  # Validate the whole input, including duplicate identities.
    lines = text.split("\n")  # Preserve CRLF, Unicode separators, and final-newline state.
    total = matched = changed = 0
    for index, line in enumerate(lines):
        choices = parse_selector(line)
        if not choices:
            continue
        total += 1
        identity = next(iter(choices))
        try:
            match = eval(code, {"__builtins__": {}}, expression_fields(identity))
            if type(match) is not bool:
                raise ValueError("expression must return True or False")
        except (ValueError, TypeError, LookupError, ArithmeticError) as error:
            raise ValueError(f"line {index + 1}: {error}") from error
        if not match:
            continue
        matched += 1
        body = line.lstrip(" \t\r\v\f")
        indent = line[:len(line) - len(body)]
        if action == "comment":
            replacement = line if body.startswith("#") else indent + "# " + body
        else:
            replacement = indent + action + "\t" + body.split("\t", 1)[1]
        changed += replacement != line
        lines[index] = replacement
    return "\n".join(lines), total, matched, changed


def replace_file(path, text):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="",
                                         dir=path.parent, prefix=path.name + ".", delete=False) as out:
            temporary = Path(out.name)
            out.write(text)
        os.chmod(temporary, stat.S_IMODE(path.stat().st_mode))
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("selector", type=Path, help="input cases.txt")
    parser.add_argument("expression", help="predicate selecting the cases to change (quote it for the shell)")
    parser.add_argument("--action", choices=("skip", "no", "comment", "run", "yes"), default="skip",
                        help="action for matching cases (default: skip)")
    parser.add_argument("--in-place", action="store_true", help="atomically replace the input instead of printing it")
    args = parser.parse_args()
    path = args.selector.resolve(strict=True)  # Follow symlinks without replacing the link itself.
    with path.open(encoding="utf-8", newline="") as source:
        text = source.read()
    result, total, matched, changed = edit_cases(text, args.expression, args.action)
    if args.in_place:
        if result != text:
            replace_file(path, result)
    else:
        sys.stdout.write(result)
    print(f"{matched}/{total} cases matched; {changed} lines changed to {args.action}.", file=sys.stderr)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
