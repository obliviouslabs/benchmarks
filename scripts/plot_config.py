from pathlib import Path
import json
import re

REPO_ROOT = Path(__file__).resolve().parents[1]
RESULTS_ROOT = REPO_ROOT / "results"

RESULT_PREFIXES = [
  "olabs_oram",
  "signal_icelake",
  "signal_jasmine",
  "mc_oblivious",
  "meta_oram",
  "olabs_rostl",
]

CANONICAL_RESULT_RE = re.compile(
  r"^(?P<prefix>[a-z0-9_]+)_(?P<timestamp>\d+)_([0-9a-f]{40})(?:_SWAP[0-9A-Za-z]+)?$"
)


def _load_file_signature(file_path: Path):
  impl_type_pairs = set()
  try:
    with open(file_path, "r") as fh:
      for line in fh:
        line = line.strip()
        if not line or line.startswith("#"):
          continue
        try:
          pt = json.loads(line)
        except json.JSONDecodeError:
          continue
        impl = pt.get("implementation")
        benchmark_type = pt.get("benchmark_type")
        if impl is None or benchmark_type is None:
          continue
        impl_type_pairs.add((impl, benchmark_type))
  except FileNotFoundError:
    return set()
  return impl_type_pairs


def _discover_files() -> list[str]:
  if not RESULTS_ROOT.exists():
    return []

  selected = {}
  for prefix in RESULT_PREFIXES:
    candidates = []
    for path in RESULTS_ROOT.glob(f"{prefix}_*"):
      match = CANONICAL_RESULT_RE.fullmatch(path.name)
      if not match:
        continue
      candidates.append((int(match.group("timestamp")), path))

    for timestamp, path in sorted(candidates, key=lambda x: x[0], reverse=True):
      signature = _load_file_signature(path)
      if not signature:
        continue
      for impl_bt in signature:
        if impl_bt in selected:
          continue
        selected[impl_bt] = (timestamp, path)

  unique_paths = {path for _, path in selected.values()}
  return [f"./results/{path.name}" for path in sorted(unique_paths)]


files = _discover_files()
