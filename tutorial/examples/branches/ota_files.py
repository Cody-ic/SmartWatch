"""Process-restart OTA model using ordinary files, never a device or Flash.

SHA-256 checks content integrity only. This exercise does not implement image
signatures, real boot jumps, hardware power-fail atomicity, or an OTA bootloader.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path

MAX_IMAGE = 1024 * 1024
TARGET = "stm32u575-file-demo"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def save(root, state):
    temporary = root / "state.tmp"
    with temporary.open("w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temporary, root / "state.json")


def load(root):
    return json.loads((root / "state.json").read_text(encoding="utf-8"))


def valid_slot(root, state, slot):
    record = state["valid"].get(slot)
    path = root / (slot + ".bin")
    return bool(record and path.is_file() and
                digest(path.read_bytes()) == record["sha256"])


def initialize(root):
    root.mkdir(parents=True, exist_ok=True)
    if (root / "state.json").exists():
        raise ValueError("already initialized; choose another practice folder")
    old = b"old-working-application-file"
    (root / "slot_a.bin").write_bytes(old)
    state = dict(active="slot_a", confirmed="slot_a", pending=None,
                 trial=False, receiving=None,
                 valid={"slot_a":dict(sha256=digest(old), version=1)})
    save(root, state)
    return state


def begin(root, image, target, version):
    state = load(root)
    if state["trial"] or state["pending"]:
        raise ValueError("finish the existing trial before another update")
    data = image.read_bytes()
    if target != TARGET or not 0 < len(data) <= MAX_IMAGE:
        raise ValueError("wrong target or invalid image size")
    if version <= state["valid"][state["confirmed"]]["version"]:
        raise ValueError("demo requires a newer version")
    slot = "slot_b" if state["confirmed"] == "slot_a" else "slot_a"
    state["valid"].pop(slot, None)
    state["receiving"] = dict(slot=slot, size=len(data), sha256=digest(data),
                              written=0, version=version, target=target)
    save(root, state)  # Invalidate the candidate before reusing its file.
    (root / (slot + ".bin")).write_bytes(b"")
    return state


def chunk(root, image, offset, length):
    state = load(root)
    receive = state["receiving"]
    if not receive:
        raise ValueError("no transfer in progress")
    whole = image.read_bytes()
    if digest(whole) != receive["sha256"]:
        raise ValueError("source changed since begin")
    if offset < 0 or length <= 0 or offset > receive["written"]:
        raise ValueError("invalid or out-of-order chunk")
    data = whole[offset:offset + length]
    if not data or offset + len(data) > receive["size"]:
        raise ValueError("chunk outside image")
    path = root / (receive["slot"] + ".bin")
    if offset < receive["written"]:
        if offset + len(data) > receive["written"]:
            raise ValueError("overlapping partial retry is not supported")
        with path.open("rb") as f:
            f.seek(offset)
            if f.read(len(data)) != data:
                raise ValueError("retry differs from saved content")
        return state
    # Rewriting the same offset also tolerates file data written just before a
    # simulated process stop whose progress record was not yet saved.
    with path.open("r+b") as f:
        f.seek(offset)
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    receive["written"] = offset + len(data)
    save(root, state)
    return state


def finish(root):
    state = load(root)
    receive = state["receiving"]
    if not receive or receive["written"] != receive["size"]:
        raise ValueError("incomplete transfer")
    data = (root / (receive["slot"] + ".bin")).read_bytes()
    if len(data) != receive["size"] or digest(data) != receive["sha256"]:
        raise ValueError("candidate content check failed")
    slot = receive["slot"]
    state["valid"][slot] = dict(sha256=receive["sha256"], version=receive["version"])
    state["pending"] = slot
    state["receiving"] = None
    save(root, state)
    return state


def boot(root):
    state = load(root)
    if state["trial"]:
        if not valid_slot(root, state, state["confirmed"]):
            raise ValueError("no valid confirmed file; recovery required")
        state["active"] = state["confirmed"]
        state["trial"] = False
        state["pending"] = None
        state["reason"] = "trial was not confirmed; reverted"
    elif state["pending"]:
        if valid_slot(root, state, state["pending"]):
            state["active"] = state["pending"]
            state["pending"] = None
            state["trial"] = True
            state["reason"] = "start candidate trial"
        else:
            state["pending"] = None
            state["active"] = state["confirmed"]
            state["reason"] = "candidate invalid; keep confirmed file"
    else:
        state["active"] = state["confirmed"]
        state["reason"] = "start confirmed file"
    if not valid_slot(root, state, state["active"]):
        raise ValueError("active file invalid; recovery required")
    save(root, state)
    return state


def confirm(root):
    state = load(root)
    if not state["trial"] or not valid_slot(root, state, state["active"]):
        raise ValueError("no valid trial to confirm")
    state["confirmed"] = state["active"]
    state["trial"] = False
    save(root, state)
    return state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("init")
    start = commands.add_parser("begin")
    start.add_argument("image", type=Path)
    start.add_argument("--target", default=TARGET)
    start.add_argument("--version", type=int, default=2)
    part = commands.add_parser("chunk")
    part.add_argument("image", type=Path)
    part.add_argument("--offset", type=int, required=True)
    part.add_argument("--length", type=int, required=True)
    for command in ("finish", "boot", "confirm", "status"):
        commands.add_parser(command)
    args = parser.parse_args()
    try:
        if args.command == "init": state = initialize(args.root)
        elif args.command == "begin": state = begin(args.root, args.image, args.target, args.version)
        elif args.command == "chunk": state = chunk(args.root, args.image, args.offset, args.length)
        elif args.command == "finish": state = finish(args.root)
        elif args.command == "boot": state = boot(args.root)
        elif args.command == "confirm": state = confirm(args.root)
        else: state = load(args.root)
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, f"{error}\n")
    print(json.dumps(state, indent=2))


if __name__ == "__main__":
    main()
