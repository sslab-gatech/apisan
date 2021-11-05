# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
import copy
from .checker import Checker, Context, BugReport
from ..lib import rank_utils, config
from ..parse.explorer import is_call
from ..parse.symbol import IDSymbol

class RetValContext(Context):
    def get_bugs(self):
        bugs = []
        for key, value in self.ctx_uses.items():
            total = self.total_uses[key]
            diff = copy.copy(total)
            scores = {}
            for ctx, codes in value.items():
                score = len(codes) / len(total)
                if score >= config.THRESHOLD and score != 1:
                    diff = diff - codes
                    for bug in diff:
                        scores[bug] = score

            if len(diff) != len(total):
                added = set()
                for bug in diff:
                    if bug in added:
                        continue
                    added.add(bug)
                    br = BugReport(scores[bug], bug, key, ctx)
                    bugs.append(br)
        return bugs


class RetValChecker(Checker):
    def _initialize_process(self):
        self.context = RetValContext()

    def _process_path(self, path):
        # get latest manager
        cmgr = path[-1].cmgr
        for i, node in enumerate(path):
            if is_call(node):
                call = node.event.call
                code = node.event.code
                constraint = cmgr.get(call)
                # want to use as dictionary key
                if constraint is None:
                    # heuristic handling for wrapper function
                    if i == len(path) - 2:
                        continue
                else:
                    constraint = tuple(constraint)
                self.context.add(call.name, constraint, code)

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
            key = report.key
            if isinstance(key, IDSymbol):
                if rank_utils.is_alloc(key.id):
                    report.score += 0.3 # XXX: change score?
        return sorted(reports, key=lambda k: k.score, reverse=True)
