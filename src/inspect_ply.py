import argparse
from pathlib import Path
import numpy as np

TYPEMAP = {
    "float": np.float32, "float32": np.float32,
    "double": np.float64, "float64": np.float64,
    "uchar": np.uint8, "uint8": np.uint8,
    "char": np.int8, "int8": np.int8,
    "ushort": np.uint16, "uint16": np.uint16,
    "short": np.int16, "int16": np.int16,
    "uint": np.uint32, "uint32": np.uint32,
    "int": np.int32, "int32": np.int32,
}

def read_ply_header(path: Path):
    print(f"[debug] opening: {path}", flush=True)
    with open(path, "rb") as f:
        header = b""
        while True:
            line = f.readline()
            if not line:  # EOF guard prevents infinite loop
                raise ValueError("EOF before 'end_header' (invalid/corrupted PLY or wrong file).")
            header += line
            if line.strip() == b"end_header":
                break

    text = header.decode("ascii", errors="ignore").splitlines()
    print(f"[debug] header lines: {len(text)} bytes: {len(header)}", flush=True)

    fmt = None
    vcount = None
    props = []
    in_vertex = False

    for ln in text:
        if ln.startswith("format"):
            fmt = ln.split()[1]
        if ln.startswith("element vertex"):
            vcount = int(ln.split()[2])
            in_vertex = True
        elif ln.startswith("element") and not ln.startswith("element vertex"):
            in_vertex = False
        if in_vertex and ln.startswith("property"):
            parts = ln.split()
            # ignore list properties if any
            if len(parts) >= 3 and parts[1] != "list":
                _, typ, name = parts[:3]
                props.append((typ, name))

    return fmt, vcount, props, len(header)

def inspect_ply(path: Path):
    fmt, n, props, header_len = read_ply_header(path)

    print(f"[debug] format={fmt} vertices={n} props={len(props)} header_len={header_len}", flush=True)

    if fmt != "binary_little_endian":
        print(path, "not binary_little_endian:", fmt, flush=True)
        return

    unknown = [typ for typ, _ in props if typ not in TYPEMAP]
    if unknown:
        print(path, "unsupported types:", sorted(set(unknown)), flush=True)
        return

    dtype = [(name, TYPEMAP[typ]) for typ, name in props]
    data = np.fromfile(path, dtype=np.dtype(dtype), offset=header_len, count=n)

    for k in ["x", "y", "z"]:
        if k not in data.dtype.names:
            print(path, "missing", k, flush=True)
            return

    x = data["x"].astype(np.float64)
    y = data["y"].astype(np.float64)
    z = data["z"].astype(np.float64)

    fin = np.isfinite(x) & np.isfinite(y) & np.isfinite(z)

    print("\n", path, flush=True)
    print(" vertices:", n, "pos finite ratio:", float(fin.mean()), flush=True)
    if fin.any():
        print(" pos min:", (float(x[fin].min()), float(y[fin].min()), float(z[fin].min())), flush=True)
        print(" pos max:", (float(x[fin].max()), float(y[fin].max()), float(z[fin].max())), flush=True)
        print(" pos std:", (float(x[fin].std()), float(y[fin].std()), float(z[fin].std())), flush=True)

def main():
    print("[debug] script started", flush=True)
    parser = argparse.ArgumentParser()
    parser.add_argument("ply", nargs="+", help="Path(s) to .ply file(s)")
    args = parser.parse_args()

    print("[debug] args:", args.ply, flush=True)

    for p in args.ply:
        path = Path(p)
        if not path.exists():
            print(f"[error] not found: {path.resolve()}", flush=True)
            continue
        inspect_ply(path)

if __name__ == "__main__":
    main()
