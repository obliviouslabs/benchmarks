#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = []
# ///
"""Run go.sh; print editable selector lines in plan mode, per-test CSV otherwise.

BENCHMARK_MODE=plan uv run scripts/run.py > cases.txt
BENCHMARK_SELECTOR=cases.txt uv run scripts/run.py > results.csv
uv run scripts/run.py logs/full_runs/FILE.jsonl
uv run scripts/run.py --cases logs/full_runs/FILE.jsonl > cases.txt

Selector columns are tab-separated: action, project, variant, name, params.
Only 'run' and 'yes' enable a case. Any other action, or prefixing a case line
with '#', skips it. Missing entries are not run.
Without BENCHMARK_SELECTOR, execution runs all encountered cases as before.
Plan mode always lists all cases, ignoring any selector. Hooks append records
to BENCHMARK_LOG_FILE. Times are CLOCK_MONOTONIC nanoseconds; a start without
an end is a failed test with unknown duration. No separate registry is used.
"""
import csv
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "benchmark" / "common"))
from common import case_identity, emit, parse_selector, planning, selector_escape


def read_run(path):
    """Recover completed and interrupted tests; retain usable records on damage."""
    rows, pending, problems = [], {}, []
    run_end = None
    run_seen = False
    with open(path, encoding="utf-8", errors="replace") as source:
        for number, line in enumerate(source, 1):
            try:
                event = json.loads(line)
                kind = event["event"]
                if kind == "run_start":
                    if run_seen:
                        raise ValueError("more than one run in this file")
                    run_seen = True
                elif kind == "run_end":
                    run_end = event["returncode"]
                    if type(run_end) is not int:
                        raise ValueError("invalid run returncode")
                elif kind in ("start", "plan", "skipped", "skipped_missing"):
                    for key in ("id", "name", "params", "file"):
                        if not isinstance(event[key], str):
                            raise ValueError(f"invalid {key}")
                    row = {key: event.get(key, "") for key in
                           ("id", "project", "variant", "run", "file", "line", "name", "params")}
                    case_identity(row)  # Validate identity fields; old hash fields are ignored.
                    row.update(status="planned" if kind == "plan" else kind,
                               start_ns="", end_ns="", duration_s="", error=event.get("warning", ""))
                    if kind == "start":
                        if type(event["time_ns"]) is not int or event["time_ns"] < 0:
                            raise ValueError("invalid start time")
                        row.update(status="failed", start_ns=event["time_ns"], error="missing end")
                        # A reused PID/id belongs to the new start, not an old
                        # unfinished test. The old row remains failed.
                        pending[event["id"]] = row
                    elif "time_ns" in event:
                        raise ValueError("non-executed case contains a time")
                    rows.append(row)
                elif kind == "end":
                    row = pending[event["id"]]
                    end = event["time_ns"]
                    code = event["returncode"]
                    error = event.get("error", "")
                    if type(end) is not int or end < row["start_ns"]:
                        raise ValueError("invalid end time")
                    if type(code) is not int or not isinstance(error, str):
                        raise ValueError("invalid outcome")
                    row.update(end_ns=end, duration_s=f'{(end - row["start_ns"]) / 1e9:.9f}',
                               status="failed" if code or error else "passed",
                               error=error or (f"exit {code}" if code else ""))
                    del pending[event["id"]]
                else:
                    raise ValueError(f"unknown event {kind!r}")
            except (ValueError, KeyError, TypeError) as error:
                problems.append(f"{path}:{number}: invalid record ({error})")
    if run_seen and run_end is None:
        problems.append("Run did not finish; the log may be incomplete.")
    elif run_end:
        problems.append(f"go.sh exited with status {run_end}.")
    if not rows:
        problems.append("No tests were recorded; this is not a complete test plan/result.")
    return rows, problems


def selector_lines(rows):
    cases = {}
    for row in rows:
        identity = case_identity(row)
        location = f"{Path(row['file']).name}:{row['line']}"
        cases.setdefault(identity, location)  # Identical repeats use the same choice.
    return ["run\t" + "\t".join(selector_escape(field) for field in identity)
            + "\t# " + selector_escape(location) + "\n"
            for identity, location in cases.items()]


def report(path, as_selector=False):
    rows, problems = read_run(path)
    if as_selector:
        sys.stdout.writelines(selector_lines(rows))
    else:
        fields = ("project", "variant", "run", "file", "line", "name", "params", "status",
                  "start_ns", "end_ns", "duration_s", "error")
        writer = csv.DictWriter(sys.stdout, fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    counts = {state: sum(row["status"] == state for row in rows)
              for state in ("planned", "passed", "failed", "skipped", "skipped_missing")}
    print(", ".join(f"{count} {state}" for state, count in counts.items()), file=sys.stderr)
    for problem in problems:
        print(problem, file=sys.stderr)
    return int(bool(problems or counts["failed"]))


def stop(process):
    # Let Docker clients forward termination before killing remaining children.
    try:
        os.killpg(process.pid, signal.SIGTERM)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            process.poll()
            os.killpg(process.pid, 0)
            time.sleep(.05)
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


def run():
    plan = planning()  # Validate mode/selector before go.sh can reset anything.
    selector_source = os.environ.pop("BENCHMARK_SELECTOR", "")
    selector_text = None
    if selector_source and not plan:
        selector_source = str(Path(selector_source).resolve())
        selector_text = Path(selector_source).read_text(encoding="utf-8")
        parse_selector(selector_text)
    directory = ROOT / "logs/full_runs"
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix=time.strftime("%Y%m%d-%H%M%S-"),
                                     suffix=".jsonl", dir=directory, delete=False) as out:
        path = Path(out.name)
    os.environ["BENCHMARK_LOG_FILE"] = str(path)
    if selector_text is not None:
        # Freeze the edited choices outside build/, which go.sh can reset.
        snapshot = path.with_suffix(".selector.txt")
        snapshot.write_text(selector_text, encoding="utf-8")
        os.environ["BENCHMARK_SELECTOR"] = str(snapshot)
    print(f"Full run log: {path}", file=sys.stderr, flush=True)
    emit(dict(event="run_start", mode="plan" if plan else "run", command=["sh", "go.sh"],
              selector=os.environ.get("BENCHMARK_SELECTOR", ""),
              selector_source=selector_source if not plan else ""))
    code = 1
    try:
        with subprocess.Popen(["sh", "go.sh"], cwd=ROOT, stdout=sys.stderr,
                              start_new_session=True) as process:
            try:
                code = process.wait()
            except KeyboardInterrupt:
                stop(process)
                code = 130
    finally:
        emit(dict(event="run_end", returncode=code))
    return max(report(path, as_selector=plan), int(code != 0))


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--cases":
        return report(sys.argv[2], as_selector=True)
    if sys.argv[1:] in (["--help"], ["-h"]):
        print(__doc__)
        return 0
    if len(sys.argv) == 1:
        return run()
    if len(sys.argv) == 2 and not sys.argv[1].startswith("-"):
        return report(sys.argv[1])
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
