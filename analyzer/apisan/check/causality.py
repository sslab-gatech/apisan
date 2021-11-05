# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
from .checker import Checker, Context
from ..lib.rank_utils import (
    is_alloc, is_dealloc, is_lock, is_unlock
)
from ..parse.explorer import is_call
from ..parse.symbol import IDSymbol

class CausalityContext(Context):
    def __init__(self):
        super().__init__()
        self.entries = {}

    def add_or_intersect(self, key, values, code):
        entry = (key, code)
        if entry in self.entries:
            self.entries[entry] &= values
        else:
            self.entries[entry] = values

    def add_all(self):
        for entry, values in self.entries.items():
            key, code = entry
            for value in values:
                self.add(key, value, code)
            self.add(key, None, code)

class CausalityChecker(Checker):
    def _initialize_process(self):
        self.context = CausalityContext()

    def _process_path(self, path):
        # get latest manager
        cmgr = path[-1].cmgr
        for i, node in enumerate(path):
            if is_call(node):
                call = node.event.call
                code = node.event.code
                constraint = cmgr.get(call)
                # want to use as dictionary key
                if constraint is not None:
                    constraint = tuple(constraint)

                calls = set()
                for j in range(i + 1, len(path)):
                    ff_node = path[j]
                    if (is_call(ff_node)
                            and ff_node.event.call.name != call.name):
                        calls.add(ff_node.event.call.name)
                self.context.add_or_intersect((call.name, constraint), calls, code)

    def _finalize_process(self):
        self.context.add_all()
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
            func = report.key[0]
            if not isinstance(func, IDSymbol):
                continue
            ctx = report.ctx
            if not isinstance(ctx, IDSymbol):
                continue

            func_name = func.id
            ctx_name = ctx.id

            if is_alloc(func_name) and is_dealloc(ctx_name):
                report.score += 0.5
            elif is_lock(func_name) and is_unlock(ctx_name):
                report.score += 0.5
            elif is_dealloc(ctx_name):
                report.score += 0.3
        return sorted(reports, key=lambda k: k.score, reverse=True)
