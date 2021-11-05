# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
import os

TOP = os.path.dirname(os.path.realpath(__file__))

def get_data_dir(name):
    path = os.path.join(TOP, "data", name)
    assert os.path.exists(path)
    return path
