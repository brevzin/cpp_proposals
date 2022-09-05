import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('--number', required=True)
parser.add_argument('--name', required=True)
args = parser.parse_args()

name = args.name.lower().split()

folder = f"{args.number}_{'_'.join(name)}"
os.mkdir(folder)

md_file = f"{'-'.join(name)}.md"

with open(f"{folder}/{md_file}", "w") as f:
    f.write(f"""---
title: ""
document: P{args.number}R0
date: today
audience: ???
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction
""")

with open(f"{folder}/Makefile", "w") as f:
    f.write(f"p{args.number}r0.html : {md_file}\n")
    f.write("include ../md/mpark-wg21.mk\n")

print(f"Created {folder}")
