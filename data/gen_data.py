#!/usr/bin/env python3
"""Generate test CSV data for the columnar engine."""
import csv
import random
import argparse


def generate(rows: int, out: str) -> None:
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["order_id", "customer_id", "amount", "status", "ts"])
        for i in range(rows):
            w.writerow([
                i,
                random.randint(1, 10000),
                round(random.uniform(1.0, 5000.0), 2),
                random.randint(0, 3),
                1_700_000_000 + i * 60,
            ])


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--rows", type=int, default=1_000_000)
    p.add_argument("--out", default="orders.csv")
    args = p.parse_args()
    generate(args.rows, args.out)
    print(f"Generated {args.rows} rows → {args.out}")
