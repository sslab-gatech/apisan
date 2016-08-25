#!/usr/bin/env python3
import argparse
import os
from apisan.check import CHECKERS
from apisan.parse.explorer import Explorer

TOP = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../../")
SCAN_BUILD = os.path.join(TOP, "./llvm/tools/clang/tools/scan-build/scan-build")
CLANG_BIN = os.path.join(TOP, "./bin/llvm/bin/clang")
SYM_EXEC_EXTRACTOR = "alpha.unix.SymExecExtract"

DISABLED_CHECKERS = [
    "core.CallAndMessage",
    "core.DivideZero",
    "core.DynamicTypePropagation",
    "core.NonNullParamChecker",
    "core.NullDereference",
    "core.StackAddressEscape",
    "core.UndefinedBinaryOperatorResult",
    "core.VLASize",
    "core.builtin.BuiltinFunctions",
    "core.builtin.NoReturnFunctions",
    "core.uninitialized.ArraySubscript",
    "core.uninitialized.Assign",
    "core.uninitialized.Branch",
    "core.uninitialized.CapturedBlockVariable",
    "core.uninitialized.UndefReturn",
    "cplusplus.NewDelete",
    "deadcode.DeadStores",
    "security.insecureAPI.UncheckedReturn",
    "security.insecureAPI.getpw",
    "security.insecureAPI.gets",
    "security.insecureAPI.mkstemp",
    "security.insecureAPI.mktemp",
    "security.insecureAPI.vfork",
    "unix.API",
    "unix.Malloc",
    "unix.MallocSizeof",
    "unix.MismatchedDeallocator",
    "unix.cstring.BadSizeArg",
    "unix.cstring.NullArg"
]

CONFIGS = [
    "ipa=basic-inlining",
    # "ipa-always-inline-size=3",  # default: 3     # number of basic block
    # "max-inlinable-size=4",     # default: 4, 50 # number of basic block
    # "max-times-inline-large=32", # default: 32    # number of functions
]

def print_bugs(bugs):
    if bugs:
        print("=" * 30 + " POTENTIAL BUGS " + "=" * 30)
        for bug in bugs:
            print(bug)

def get_command():
    cmds = [SCAN_BUILD]
    for checker in DISABLED_CHECKERS:
        cmds += ["-disable-checker", checker]
    for config in CONFIGS:
        cmds += ["-analyzer-config", config]
    cmds += [
        "--use-analyzer", CLANG_BIN,
        "-enable-checker", SYM_EXEC_EXTRACTOR,
    ]
    return cmds

def add_build_command(subparsers):
    parser = subparsers.add_parser("build", help="make a symbolic context database")
    parser.add_argument("cmds", nargs="+")

def add_check_command(subparsers):
    parser = subparsers.add_parser("check", help="check a API misuse")
    parser.add_argument("--checker", choices=CHECKERS.keys(), required=True)
    parser.add_argument("--db", default=None)

def parse_args():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="cmd")
    subparsers.required = True
    add_build_command(subparsers)
    add_check_command(subparsers)
    return parser.parse_args()

def handle_build(args):
    cmds = get_command()
    cmds += args.cmds
    os.spawnv(os.P_WAIT, cmds[0], cmds)

def handle_check(args):
    if args.db is None:
        args.db = os.path.join(os.getcwd(), "as-out")
    chk = CHECKERS[args.checker]()
    exp = Explorer(chk)
    bugs = exp.explore_parallel(args.db)
    print_bugs(bugs)

def main():
    args = parse_args()
    globals()["handle_%s" % args.cmd](args)

if __name__ == "__main__":
    main()
