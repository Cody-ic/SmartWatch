"""Quantize the tutorial ONNX internals with training-only calibration data.
The ONNX boundary remains float; an int8-named file is not proof of a uint8 input.
"""
import argparse
from pathlib import Path
import numpy as np
import onnxruntime as ort
from onnxruntime.quantization import CalibrationDataReader, QuantFormat, QuantType, quantize_static


class Reader(CalibrationDataReader):
    def __init__(self, rows):
        self.rows = iter(rows)

    def get_next(self):
        row = next(self.rows, None)
        return None if row is None else {"window": row[None].astype(np.float32)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()
    calibration = np.load(args.folder / "calibration.npz")["x"]
    target = args.folder / "model_qdq.onnx"
    quantize_static(str(args.folder / "model.onnx"), str(target), Reader(calibration),
                    quant_format=QuantFormat.QDQ, activation_type=QuantType.QUInt8,
                    weight_type=QuantType.QInt8)
    floating = ort.InferenceSession(str(args.folder / "model.onnx"), providers=["CPUExecutionProvider"])
    quantized = ort.InferenceSession(str(target), providers=["CPUExecutionProvider"])
    held_out = np.load(args.folder / "held_out.npz")
    before, after, differences = [], [], []
    for row in held_out["x"]:
        a = floating.run(None, {"window": row[None].astype(np.float32)})[0]
        b = quantized.run(None, {"window": row[None].astype(np.float32)})[0]
        before.append(int(a.argmax()))
        after.append(int(b.argmax()))
        differences.append(float(np.abs(a - b).max()))
    print("boundary:",quantized.get_inputs()[0].type,quantized.get_inputs()[0].shape)
    print("max logit difference:",max(differences))
    print("float test accuracy:",float(np.mean(np.array(before) == held_out["y"])))
    print("QDQ test accuracy:",float(np.mean(np.array(after) == held_out["y"])))
    print("Synthetic input gives software checks, not real watch accuracy.")


if __name__ == "__main__":
    main()
