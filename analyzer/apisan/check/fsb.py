# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
from .checker import Checker, Context, BugReport
from ..lib import rank_utils, config
from ..parse.explorer import is_call
from ..parse.symbol import IDSymbol, StringLiteralSymbol

FORMAT_STRINGS = ["%d", "%p", "%x", "%s", "%u", "%c"]

def is_format_string(string):
    for fmt in FORMAT_STRINGS:
        if fmt in string:
            return True
    return False

def count_const_strings(value):
    count = 0
    for ctx, codes in value.items():
        if ctx[0]:
            count += len(codes)
    return count

class FSBContext(Context):
    def get_bugs(self):
        added = set()
        bugs = []
        for key, value in self.ctx_uses.items():
            total = self.total_uses[key]
            correct = count_const_strings(value)
            for ctx, codes in value.items():
                if ctx[0]:
                    continue
                score = correct / len(total)
                if score >= config.THRESHOLD and score != 1:
                    for bug in codes:
                        br = BugReport(score, bug, key, ctx)
                        added.add(bug)
                        bugs.append(br)
        return bugs

class FSBChecker(Checker):
    def _initialize_process(self):
        self.context = FSBContext()

    def _process_path(self, path):
        for i, node in enumerate(path):
            if is_call(node):
                call = node.event.call
                code = node.event.code
                for i, arg in enumerate(call.args):
                    key = (call.name, i)
                    value = (False, False)
                    if isinstance(arg, StringLiteralSymbol):
                        if is_format_string(arg.string):
                            value = (True, True)
                        else:
                            value = (True, False)
                    self.context.add(key, value, code)

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
            ctx = report.ctx
            if ctx == (True, True):
                report.score += 0.5
            func = report.key[0]
            if isinstance(func, IDSymbol):
                func_name = func.id
                if rank_utils.is_print(func_name):
                    report.score += 0.3
        return sorted(reports, key=lambda k: k.score, reverse=True)
