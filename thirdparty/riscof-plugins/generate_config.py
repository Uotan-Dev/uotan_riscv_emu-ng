#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

dutname = "uemu"
refname = "spike_simple"

script_dir = os.path.abspath(os.path.dirname(__file__))
out_dir = os.getcwd()
out_file = os.path.join(out_dir, "config.ini")

config_temp = """[RISCOF]
ReferencePlugin={0}
ReferencePluginPath={1}
DUTPlugin={2}
DUTPluginPath={3}

[{2}]
pluginpath={3}
ispec={3}/{2}_isa.yaml
pspec={3}/{2}_platform.yaml
target_run=1

[{0}]
pluginpath={1}
ispec={1}/{0}_isa.yaml
pspec={1}/{0}_platform.yaml
target_run=1
"""


def make_abs(p):
    return os.path.abspath(os.path.expanduser(p))


def main():
    dutpath = make_abs(os.path.join(script_dir, dutname))
    refpath = make_abs(os.path.join(script_dir, refname))

    content = config_temp.format(refname, refpath, dutname, dutpath, dutpath)

    try:
        with open(out_file, "w", encoding="utf-8") as f:
            f.write(content)
    except Exception as e:
        print(f"Error: failed to write {out_file}: {e}", file=sys.stderr)
        sys.exit(1)

    print("----- config.ini begin -----")
    print(content.rstrip())
    print("----- config.ini end -----\n")


if __name__ == "__main__":
    main()
