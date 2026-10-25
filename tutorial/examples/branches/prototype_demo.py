"""Two-dimensional prototype arithmetic with independent query samples."""
import numpy as np


def normalize(value):
    value = np.asarray(value, dtype=np.float64)
    length = np.linalg.norm(value)
    if not np.isfinite(value).all() or length < 1e-8:
        raise ValueError("a finite nonzero vector is required")
    return value / length


def prototype(samples):
    values = np.asarray(samples, dtype=np.float64)
    if values.ndim != 2 or len(values) == 0 or not np.isfinite(values).all():
        raise ValueError("need at least one valid labeled sample")
    return normalize(values.mean(axis=0))


def main():
    walking = prototype([[.9, .1], [.8, .2]])
    running = prototype([[.1, .9], [.2, .8]])
    query = normalize([.7, .3])
    scores = np.stack([walking, running]) @ query
    print("centers:",walking,running)
    print("similarity:",scores,"class:",int(scores.argmax()))
    print("Scores are direction similarities, not calibrated probabilities.")


if __name__ == "__main__":
    main()
