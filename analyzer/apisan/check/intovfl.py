# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
from enum import Enum
from .checker import Checker, Context, BugReport
from ..parse.explorer import is_call
from ..parse.symbol import ConcreteIntSymbol, BinaryOperatorSymbol

# state
class IntOvflChkType(Enum):
    Wrong = 1
    Missing = 2
    Correct = 3
    Undefined = 4

# constant
INT_LIMIT = (0, 2**32 - 1)

def check_integer_overflow(arg, cmgr, limit=INT_LIMIT):
    # XXX: dirty..
    if isinstance(arg, BinaryOperatorSymbol):
        lhs, rhs, op = arg.lhs, arg.rhs, arg.op
        if isinstance(rhs, ConcreteIntSymbol):
            # change order
            rhs, lhs = lhs, rhs

        if isinstance(lhs, ConcreteIntSymbol):
            # if two symbols are not integer, cannot reason
            if op == "+":
                limit = (limit[0] - lhs.value, limit[1] - lhs.value)
            elif op == "*" and lhs.value != 0:
                limit = (limit[0] / lhs.value, limit[1] / lhs.value)
            else:
                # only handling two operator *, +
                return IntOvflChkType.Undefined
            return check_integer_overflow(rhs, cmgr, limit)
        else:
            # sym sym case -> we believe false postivie > false negaitve
            return IntOvflChkType.Missing
    else:
        constraints = cmgr.get(arg)
        if constraints:
            # if constraints == 2 --> no overflow check
            if len(constraints) >= 2:
                return IntOvflChkType.Undefined
            for constraint in constraints:
                lower, upper = constraint
                # if it is out of bound, then printing as bug
                if not (lower >= limit[0] and upper <= limit[1]):
                    return IntOvflChkType.Wrong
            # if there is constraint which passes the check,
            # then it is well checked
            return IntOvflChkType.Correct
    # INT_LIMIT means no constant binary operator
    if limit == INT_LIMIT:
        return IntOvflChkType.Undefined
    else:
        # missing check
        return IntOvflChkType.Missing

def count_corrects(value):
    count = 0
    for ctx, codes in value.items():
        if ctx == IntOvflChkType.Correct:
            count += len(codes)
    return count

class IntOvflContext(Context):
    def get_bugs(self):
        added = set()
        bugs = []
        for key, value in self.ctx_uses.items():
            total = self.total_uses[key]
            correct = count_corrects(value)
            if not correct:
                continue
            score = float(correct) / len(total)
            for ctx, codes in value.items():
                if ctx == IntOvflChkType.Correct:
                    continue
                for bug in codes:
                    br = BugReport(score, bug, key, ctx)
                    added.add(bug)
                    bugs.append(br)
        return bugs

class IntOvflChecker(Checker):
    # step
    # 1. get function call with whose argument has binary operator
    # 2. if binary operator is consisted of a symbol + multiple constants
    #       then check with this rule
    #       if x + c -> x >= 0 && x < UINT_MAX - c
    #       if x * c -> x < UINT_MAX / c
    def _process_path(self, path):
        cmgr = path[-1].cmgr
        for i, node in enumerate(path):
            if is_call(node):
                call = node.event.call
                code = node.event.code

                for j, arg in enumerate(call.args):
                    if isinstance(arg, BinaryOperatorSymbol):
                        ret = check_integer_overflow(arg, cmgr)
                        if ret != IntOvflChkType.Undefined:
                            self.context.add((call.name, j), ret, code)

    def _initialize_process(self):
        self.context = IntOvflContext()

    def _finalize_process(self):
        return self.context

    def merge(self, ctxs):
        if not ctxs:
            return None
        ctx = ctxs[0]
        for i in range(1, len(ctxs)):
            ctx.merge(ctxs[i])
        return self.rank(ctx.get_bugs())

    def rank(self, reports):
        for report in reports:
            if report.ctx == IntOvflChkType.Wrong:
                report.score += 0.3
        return sorted(reports, key=lambda k: k.score, reverse=True)
