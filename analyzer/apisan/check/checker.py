# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
from ..parse.explorer import is_eop
from ..lib import config
from ..lib.store import Store

class BugReport():
    def __init__(self, score, code, key, ctx):
        self.key = key
        self.ctx = ctx
        self.score = score
        self.code = code

    def __repr__(self):
        return "BugReport(score=%.02f, code=%s, key=%s, ctx=%s" % (
            self.score, self.code, self.key, self.ctx
        )

class Context():
    def __init__(self):
        self.total_uses = Store(level=1)
        self.ctx_uses = Store(level=2)

    def add(self, key, value, code):
        if value is not None:
            self.ctx_uses[key][value].add(code)
        self.total_uses[key].add(code)

    def merge(self, other):
        self.total_uses.merge(other.total_uses)
        self.ctx_uses.merge(other.ctx_uses)

    def get_bugs(self):
        added = set()
        bugs = []
        for key, value in self.ctx_uses.items():
            total = self.total_uses[key]
            for ctx, codes in value.items():
                score = len(codes) / len(total)
                if score >= config.THRESHOLD and score != 1:
                    diff = total - codes
                    for bug in diff:
                        br = BugReport(score, bug, key, ctx)
                        added.add(bug)
                        bugs.append(br)
        return bugs

class Checker(object):
    def _initialize_process(self):
        # optional
        pass

    def _finalize_process(self):
        raise NotImplementedError

    def _process_path(self, path):
        raise NotImplementedError

    def process(self, tree):
        self._initialize_process()
        self._do_dfs(tree)
        return self._finalize_process()

    def _do_dfs(self, tree):
        count = 0
        indices = [0]
        nodes = [tree.root]

        while nodes:
            index = indices.pop()
            node = nodes.pop()
            if is_eop(node):
                nodes.append(node)
                # delayed visiting for truncated paths
                self._process_path(nodes)
                nodes.pop()
                count += 1
            else:
                if len(node.children) > index:
                    indices.append(index + 1)
                    nodes.append(node)

                    child = node.children[index]
                    child.visited = False
                    indices.append(0)
                    nodes.append(child)
