"""Shared Python benchmark lifecycle logging and readable case selection."""
import fcntl
import itertools
import json
import os
from pathlib import Path
import sys
import time

_sequence = itertools.count(1)


def planning():
    mode = os.environ.get("BENCHMARK_MODE", "run") or "run"
    if mode not in ("run", "plan"):
        raise ValueError(f"Invalid BENCHMARK_MODE: {mode!r}")
    return mode == "plan"


def emit(event):
    line = json.dumps(event, ensure_ascii=True) + "\n"
    path = os.environ.get("BENCHMARK_LOG_FILE")
    if not path:
        print(line, end="", flush=True)
        return
    with open(path, "a", encoding="utf-8") as out:
        fcntl.flock(out, fcntl.LOCK_EX)
        out.write(line)
        out.flush()


def case_identity(case):
    fields = tuple(case[field] for field in ("project", "variant", "name", "params"))
    if not all(isinstance(field, str) for field in fields):
        raise ValueError("case identity fields must be strings")
    return fields


def selector_escape(value):
    escapes = {"\\": "\\\\", "\t": "\\t", "\r": "\\r", "\n": "\\n"}
    return "".join(escapes.get(c, f"\\x{ord(c):02x}" if ord(c) < 32 or ord(c) == 127 else c)
                   for c in value)


def selector_unescape(value):
    out = []
    chars = iter(value)
    for c in chars:
        if c == "\\":
            escape = next(chars, "")
            if escape == "x":
                digits = next(chars, "") + next(chars, "")
                if len(digits) != 2 or any(d not in "0123456789abcdefABCDEF" for d in digits) or int(digits, 16) > 127:
                    raise ValueError("invalid ASCII escape")
                c = chr(int(digits, 16))
            elif escape in ("\\", "t", "r", "n"):
                c = {"\\": "\\", "t": "\t", "r": "\r", "n": "\n"}[escape]
            else:
                raise ValueError("invalid field escape")
        elif ord(c) < 32 or ord(c) == 127:
            raise ValueError("unescaped control character")
        out.append(c)
    return "".join(out)


def parse_selector(text):
    whitespace = " \t\r\n\v\f"
    choices = {}
    for number, line in enumerate(text.split("\n"), 1):
        line = line.removesuffix("\r").lstrip(whitespace)
        if not line:
            continue
        columns = line.split("\t", 5)
        action = columns[0].strip(whitespace)
        commented = action.startswith("#")
        while action.startswith("#"):
            action = action[1:].strip(whitespace)
        try:
            if ("\0" in line or len(columns) < 5
                    or (len(columns) == 6 and not columns[5].lstrip(whitespace).startswith("#"))
                    or any(c in whitespace for c in action) or (not action and not commented)):
                raise ValueError("expected tab-separated action, project, variant, name, params")
            identity = tuple(selector_unescape(field) for field in columns[1:5])
        except ValueError as error:
            if commented:  # Ordinary comment, not a parseable case entry.
                continue
            raise ValueError(f"Invalid selector line {number}: {error}") from error
        if identity in choices:
            raise ValueError(f"Duplicate selector case on line {number}: {identity!r}")
        choices[identity] = "start" if not commented and action in ("run", "yes") else "skipped"
    return choices


def selection(identity):
    path = os.environ.get("BENCHMARK_SELECTOR")
    if not path:
        return "start"
    return parse_selector(Path(path).read_text(encoding="utf-8")).get(identity, "skipped_missing")


def start_test(name, params, file, line=0):
    """Record the decision; return an ID only when the caller should run the body."""
    case_id = f"{os.getpid()}:{next(_sequence)}"
    event = dict(id=case_id, project=os.environ.get("BENCHMARK_PROJECT", ""),
                 variant=os.environ.get("BENCHMARK_VARIANT", ""),
                 run=os.environ.get("BENCHMARK_RUN", ""),
                 name=name, params=params, file=str(file), line=line)
    event["event"] = "plan" if planning() else selection(case_identity(event))
    if event["event"] == "skipped_missing":
        event["warning"] = "not present in selector; skipped"
    if event["event"] == "start":
        event["time_ns"] = time.monotonic_ns()
    emit(event)
    if event["event"] == "skipped_missing":
        print(f"Warning: {event['project']} {event['variant']} {name} [{params}] not present in selector; skipped", file=sys.stderr)
    return case_id if event["event"] == "start" else None


def end_test(case_id, returncode=0, error=""):
    emit(dict(event="end", id=case_id, time_ns=time.monotonic_ns(),
              returncode=returncode, error=error))
