# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
from collections import defaultdict

# can we define recursively?
def create_store_level1():
    return defaultdict(set)

def _merge(merge, target, level):
    if level == 1:
        for key, value in target.items():
            merge[key] = merge[key].union(value)
    else:
        for key, value in target.items():
            _merge(merge[key], value, level - 1)

class Store():
    def __init__(self, level=1):
        self.level = level
        if level == 1:
            self.store = defaultdict(set)
        elif level == 2:
            self.store = defaultdict(create_store_level1)
        else:
            raise ValueError("Too big level")

    def __getitem__(self, name):
        return self.store[name]

    def __setitem__(self, key, value):
        self.store[key] = value

    def merge(self, other):
        if self.level != other.level:
            raise ValueError("To merge, level needs to be same")

        _merge(self, other, self.level)

    # iterator
    def __iter__(self):
        return iter(self.store)

    def items(self):
        return self.store.items()

    # for debugging
    def __str__(self):
        return repr(self)

    def __repr__(self):
        return repr(self.store)

    def __eq__(self, other):
        return self.store == other.store
