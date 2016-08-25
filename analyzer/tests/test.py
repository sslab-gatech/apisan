#!/usr/bin/env python3
import unittest
import config
from apisan.lib import dbg
from apisan.parse.explorer import Explorer
from apisan.check.argument import ArgChecker
from apisan.check.causality import CausalityChecker
from apisan.check.condition import CondChecker
from apisan.check.echo import EchoChecker
from apisan.check.fsb import FSBChecker
from apisan.check.intovfl import IntOvflChecker
from apisan.check.retval import RetValChecker

class TestApiSan(unittest.TestCase):
    def test_retval(self):
        chk = RetValChecker()
        exp = Explorer(chk)
        bugs = exp.explore_parallel(config.get_data_dir("return-value"))
        assert(len(bugs) == 1)

    def test_memleak(self):
        chk = CausalityChecker()
        exp = Explorer(chk)
        bugs = exp.explore_parallel(config.get_data_dir("memory-leak"))
        assert(len(bugs) == 1)

    def test_missing_unlock(self):
        chk = CausalityChecker()
        exp = Explorer(chk)
        bugs = exp.explore_parallel(config.get_data_dir("missing-unlock"))
        assert(len(bugs) == 1)

    def test_SSL(self):
        chk = CondChecker()
        exp = Explorer(chk)
        bugs = exp.explore_parallel(config.get_data_dir("SSL"))
        assert(len(bugs) == 2) # (X, Y), (Y, X)

    def test_intovfl(self):
        chk = IntOvflChecker()
        exp = Explorer(chk)
        bugs = exp.explore_parallel(config.get_data_dir("integer-overflow"))
        assert(len(bugs) == 1)

    def test_FSB(self):
        chk = FSBChecker()
        exp = Explorer(chk)
        bugs = exp.explore_parallel(config.get_data_dir("format-string-bug"))
        assert(len(bugs) == 1)

    def test_arg(self):
        chk = ArgChecker()
        exp = Explorer(chk)
        bugs = exp.explore_parallel(config.get_data_dir("argument"))
        assert(len(bugs) == 1)

if __name__ == "__main__":
    unittest.main()
