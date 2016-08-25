#!/usr/bin/env python3
from .checker import Checker

class EchoChecker(Checker):
    def process(self, tree):
        return tree

    def merge(self, processed):
        return []
