"""Small, dependency-free Python introduction. Run with Python 3."""
import csv
from io import StringIO


def average(values):
    return sum(values) / len(values)


class Measurement:
    def __init__(self, millivolts):
        self.millivolts = millivolts

    def describe(self):
        return f"{self.millivolts} mV"


def main():
    readings = [3900, 4000, 4100]
    first_two = readings[:2]
    names = {0: "still", 1: "moving"}
    for index, value in enumerate(readings):
        print(index, value)
    print(average(first_two), names[1])
    rows = list(csv.DictReader(StringIO("time_ms,ax\n0,0.1\n10,0.2\n")))
    print([float(row["ax"]) for row in rows])
    print(Measurement(4000).describe())


if __name__ == "__main__":
    main()
