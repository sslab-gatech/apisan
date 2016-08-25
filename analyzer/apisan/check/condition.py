#!/usr/bin/env python3
from .checker import Checker, Context
from ..lib import utils, rank_utils
from ..lib.rank_utils import (
    is_alloc, is_dealloc, is_lock, is_unlock
)
from ..parse.explorer import is_call
from ..parse.symbol import IDSymbol

class CondChecker(Checker):
    def _initialize_process(self):
        self.context = Context()

    def _process_path(self, path):
        # get latest manager
        cmgr = path[-1].cmgr
        for i, node in enumerate(path):
            if is_call(node):
                call = node.event.call
                code = node.event.code
                cstr = cmgr.get(call, True)
                # want to use as dictionary key
                constraints = set()
                for j in range(0, len(path)):
                    if i == j: # skip myself
                        continue
                    ff_node = path[j]
                    if is_call(ff_node):
                        ff_call = ff_node.event.call
                        other_cstr = cmgr.get(ff_call, True)
                        self.context.add((call.name, cstr), (ff_call.name, other_cstr), code)

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
        return sorted(reports, key=lambda k:k.score, reverse=True)
