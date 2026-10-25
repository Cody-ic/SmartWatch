"""Reproducible teaching pipeline; --demo creates synthetic data, not watch measurements.

CSV columns: person,session,split,time_ms,label,ax,ay,az,gx,gy,gz.
One person belongs to one split. Input must already use consistent physical units
at 100 Hz. Use a new output folder for each experiment.
"""
import argparse
import csv
import json
from pathlib import Path

import numpy as np
import torch
from torch import nn

CHANNELS = ["ax", "ay", "az", "gx", "gy", "gz"]
WINDOW = 512
STRIDE = 256


def make_demo_csv(path):
    """Artificial class-dependent signals only demonstrate the software workflow."""
    rng = np.random.default_rng(42)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["person", "session", "split", "time_ms", "label", *CHANNELS])
        for person, split in [("p1", "train"), ("p2", "train"),
                              ("p3", "valid"), ("p4", "test")]:
            for label in range(3):
                for i in range(1024):
                    signal = np.sin(2 * np.pi * (label + 1) * i / 100)
                    values = label * 1.5 + signal * np.linspace(.1, .6, 6)
                    values += rng.normal(0, .05, 6)
                    writer.writerow([person, f"s{label}", split, i * 10, label, *values])


def load_windows(path):
    groups, person_split = {}, {}
    with path.open(encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            person, session, split = row["person"], row["session"], row["split"]
            if split not in ("train", "valid", "test"):
                raise ValueError("split must be train, valid or test")
            if person in person_split and person_split[person] != split:
                raise ValueError("one person must not cross splits")
            person_split[person] = split
            values = np.asarray([float(row[c]) for c in CHANNELS], dtype=np.float32)
            if not np.isfinite(values).all():
                raise ValueError("non-finite sensor value")
            groups.setdefault((person, session, split), []).append(
                (float(row["time_ms"]), int(row["label"]), values))
    result = {s: [[], []] for s in ("train", "valid", "test")}
    for (_, _, split), rows in groups.items():
        # Preserve file order: silently sorting could hide capture/order mistakes.
        times = np.array([r[0] for r in rows])
        if not np.isfinite(times).all() or np.any(np.diff(times) <= 0):
            raise ValueError("session timestamps must be finite and increase")
        for start in range(0, len(rows) - WINDOW + 1, STRIDE):
            block = rows[start:start + WINDOW]
            if len({r[1] for r in block}) != 1:
                continue  # Omit transitions instead of inventing one label.
            if not np.all(np.abs(np.diff(times[start:start + WINDOW]) - 10) <= 1):
                continue  # A missing/discontinuous window is not filled with old data.
            result[split][0].append(np.stack([r[2] for r in block]).T)
            result[split][1].append(block[0][1])
    for split, (x, y) in result.items():
        if not x:
            raise ValueError(f"no valid 512-point windows in {split}")
        result[split] = (np.stack(x).astype(np.float32), np.array(y, dtype=np.int64))
    return result


def make_model(classes, kind):
    if kind == "cnn":
        return nn.Sequential(nn.Conv1d(6, 8, 5, padding=2), nn.ReLU(),
                             nn.AdaptiveAvgPool1d(1), nn.Flatten(), nn.Linear(8, classes))
    return nn.Sequential(nn.Flatten(), nn.Linear(6 * WINDOW, 16),
                         nn.ReLU(), nn.Linear(16, classes))


def evaluate(model, x, y, classes):
    model.eval()
    with torch.no_grad():
        predicted = model(torch.from_numpy(x)).argmax(dim=1).numpy()
    matrix = np.zeros((classes, classes), dtype=np.int64)
    np.add.at(matrix, (y, predicted), 1)
    return dict(accuracy=float((predicted == y).mean()), confusion=matrix.tolist())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=Path)
    parser.add_argument("--demo", action="store_true")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--epochs", type=int, default=8)
    parser.add_argument("--network", choices=["mlp", "cnn"], default="mlp")
    parser.add_argument("--export", action="store_true")
    args = parser.parse_args()
    if args.epochs < 1 or bool(args.csv) == args.demo:
        parser.error("use exactly one of --csv / --demo, with positive epochs")
    args.out.mkdir(parents=True, exist_ok=True)
    if args.demo:
        args.csv = args.out / "synthetic_demo.csv"
        make_demo_csv(args.csv)
        print("SYNTHETIC DEMO: scores do not measure real activity recognition.")
    torch.manual_seed(42)
    torch.set_num_threads(1)
    data = load_windows(args.csv)
    train_x, train_y = data["train"]
    labels = sorted(set(train_y.tolist()))
    if len(labels) < 2 or labels != list(range(len(labels))):
        raise ValueError("training labels must be contiguous integers starting at zero")
    classes = len(labels)
    for _, y in data.values():
        if np.any(y < 0) or np.any(y >= classes):
            raise ValueError("unknown label in validation/test")
    mean = train_x.mean(axis=(0, 2), keepdims=True)
    std = train_x.std(axis=(0, 2), keepdims=True)
    std = np.maximum(std, 1e-6)
    normalized = {s: ((x - mean) / std, y) for s, (x, y) in data.items()}
    train_x, train_y = normalized["train"]
    model = make_model(classes, args.network)
    optimizer = torch.optim.Adam(model.parameters(), lr=.003)
    criterion = nn.CrossEntropyLoss()
    x, y = torch.from_numpy(train_x), torch.from_numpy(train_y)
    for epoch in range(args.epochs):
        model.train()
        order = torch.randperm(len(x))
        for indices in order.split(8):
            logits = model(x[indices])
            loss = criterion(logits, y[indices])
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
        valid = evaluate(model, *normalized["valid"], classes)
        print(f"epoch {epoch + 1}: loss={loss.item():.4f}, valid={valid['accuracy']:.3f}")
    model.eval()
    metadata = dict(channels=CHANNELS, window=WINDOW, stride=STRIDE, sample_rate_hz=100,
                    layout="batch,channel,time", units=["g", "g", "g", "deg/s", "deg/s", "deg/s"],
                    classes=classes, network=args.network,
                    mean=mean.reshape(-1).tolist(), std=std.reshape(-1).tolist(),
                    synthetic_demo=args.demo, seed=42)
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    metrics = {s: evaluate(model, *normalized[s], classes) for s in ("valid", "test")}
    (args.out / "metrics.json").write_text(json.dumps(metrics, indent=2), encoding="utf-8")
    torch.save(model.state_dict(), args.out / "weights.pt")
    np.savez(args.out / "calibration.npz", x=train_x[:16])
    np.savez(args.out / "held_out.npz", x=normalized["test"][0], y=normalized["test"][1])
    print(json.dumps(metrics, indent=2))
    if args.export:
        torch.onnx.export(model, torch.zeros(1, 6, WINDOW), args.out / "model.onnx",
                          input_names=["window"], output_names=["logits"],
                          opset_version=17, dynamo=False)
        print("Exported float ONNX. Check metadata before deployment.")


if __name__ == "__main__":
    main()
