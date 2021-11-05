# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
from .checker import Checker, Context, BugReport
from ..parse.explorer import is_call
from ..lib import config
from ..parse.symbol import CallSymbol, IDSymbol

def extract_nodes(arg):
    s = set()
    stack = [arg]
    while stack:
        arg = stack.pop()
        for child in arg.children:
            stack.append(child)
        if isinstance(arg, IDSymbol):
            s.add(arg)
    return s

def check_related(arg1, arg2):
    if not (isinstance(arg1, CallSymbol) or
            isinstance(arg2, CallSymbol)):
        return False

    nodes1 = extract_nodes(arg1)
    nodes2 = extract_nodes(arg2)
    return bool(nodes1 & nodes2)

class ArgContext(Context):
    def get_bugs(self):
        added = set()
        bugs = []
        for key, value in self.ctx_uses.items():
            total = self.total_uses[key]
            related = len(value[True])
            score = related / len(total)
            codes = value[False]
            if score >= config.THRESHOLD and score != 1:
                for bug in codes:
                    br = BugReport(score, bug, key, False)
                    added.add(bug)
                    bugs.append(br)
        return bugs

class ArgChecker(Checker):
    def _initialize_process(self):
        self.context = ArgContext()

    def _process_path(self, path):
        for i, node in enumerate(path):
            if is_call(node):
                call = node.event.call
                code = node.event.code
                for i, arg1 in enumerate(call.args):
                    for j in range(i + 1, len(call.args)):
                        arg2 = call.args[j]
                        related = check_related(arg1, arg2)
                        self.context.add((call.name, i, j), related, code)

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
        return sorted(reports, key=lambda k: k.score, reverse=True)
