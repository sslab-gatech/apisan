# SPDX-License-Identifier: MIT
from apisan.parse.explorer import Explorer
from apisan.check.argument import ArgChecker
from apisan.check.causality import CausalityChecker
from apisan.check.condition import CondChecker
from apisan.check.echo import EchoChecker
from apisan.check.fsb import FSBChecker
from apisan.check.intovfl import IntOvflChecker
from apisan.check.retval import RetValChecker

CHECKERS = {
    "rvchk": RetValChecker,
    "cpair": CausalityChecker,
    "cond": CondChecker,
    "fsb": FSBChecker,
    "args": ArgChecker,
    "intovfl": IntOvflChecker
}
