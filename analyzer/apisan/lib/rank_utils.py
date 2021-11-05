# SPDX-License-Identifier: MIT
#!/usr/bin/env python3

ALLOC_KEYWORD = ["alloc", "_new", "clone", "create", "dup"]
DEALLOC_KEYWORD = ["free", "release"]
LOCK_KEYWORD = ["_lock"]
UNLOCK_KEYWORD = ["_unlock"]
PRINT_KEYWORD = ["print"]

def has_keyword(name, keywords):
    for keyword in keywords:
        if keyword in name:
            return True

def is_alloc(name):
    return has_keyword(name, ALLOC_KEYWORD)

def is_lock(name):
    return has_keyword(name, LOCK_KEYWORD)

def is_unlock(name):
    return has_keyword(name, UNLOCK_KEYWORD)

def is_dealloc(name):
    return has_keyword(name, DEALLOC_KEYWORD)

def is_print(name):
    return has_keyword(name, PRINT_KEYWORD)
