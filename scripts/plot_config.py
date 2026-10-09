from pathlib import Path
import json
import re

REPO_ROOT = Path(__file__).resolve().parents[1]
RESULTS_ROOT = REPO_ROOT / "results"

RESULT_PREFIXES = [
  "olabs_oram",
  "signal_icelake",
  "signal_jasmine",
  "h2o2_oram",
  "mc_oblivious",
  "meta_oram",
  "olabs_rostl",
  "sonic",
]

# Git commits (40 hex digits) or pinned release archive SHA-256s (64).
CANONICAL_RESULT_RE = re.compile(
  r"^(?P<prefix>[a-z0-9_]+)_(?P<timestamp>\d+)_([0-9a-f]{40}|[0-9a-f]{64})(?P<swap>_SWAP[0-9A-Za-z]+)?$"
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
        if not isinstance(pt, dict):
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

  # A result file is a run snapshot, not a source of individual missing cases.
  # Selecting older files for discontinued implementation names also imported
  # their overlapping rows, silently averaging old and new measurements.
  selected = []
  for prefix in RESULT_PREFIXES:
    candidates = []
    for path in RESULTS_ROOT.glob(f"{prefix}_*"):
      match = CANONICAL_RESULT_RE.fullmatch(path.name)
      if not path.is_file() or not match or match.group("prefix") != prefix:
        continue
      # Memory-limited runs are different experiments. Select them explicitly
      # in `files` rather than mixing them into the default comparison.
      if match.group("swap"):
        continue
      candidates.append((int(match.group("timestamp")), path))

    for _, path in sorted(candidates, reverse=True):
      if _load_file_signature(path):
        selected.append(path)
        break

  return [f"./results/{path.name}" for path in sorted(selected)]


files = _discover_files()
