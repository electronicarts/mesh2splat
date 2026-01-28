#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

LINE_RE = re.compile(
    r"\\[mesh2splat\\] ConversionPass: candidates=(\\d+) maxSplats=(\\d+) kept=(\\d+) overflow=([0-9eE.+-]+) "
    r"sampleProb=([0-9eE.+-]+) res=(\\d+) strategy=([^ ]+)"
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Extract conversion candidate/maxSplats/sampleProb from mesh2splat logs."
    )
    parser.add_argument("log", type=Path, help="Path to a mesh2splat log file")
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="ignore")
    matches = LINE_RE.findall(text)
    if not matches:
        print("No ConversionPass stats found.")
        return 1

    for i, (candidates, max_splats, kept, overflow, sample_prob, res, strategy) in enumerate(matches, 1):
        print(
            f"Run {i}: candidates={candidates} maxSplats={max_splats} kept={kept} "
            f"overflow={overflow} sampleProb={sample_prob} res={res} strategy={strategy}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
