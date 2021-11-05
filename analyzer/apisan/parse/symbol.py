# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
from enum import Enum

class SymbolKind(Enum):
    ConcreteInt = 1
    Call = 2
    StringLiteral = 3
    BinaryOperator = 4
    Field = 5
    Array = 6
    Constraint = 7
    ID = 8
    Unknown = 9

class Symbol:
    # base class
    def __init__(self, kind):
        self.kind = kind

    def __hash__(self):
        return hash(repr(self))

    def __eq__(self, other):
        return hash(self) == hash(other)

    @property
    def children(self):
        return []

class ConcreteIntSymbol(Symbol):
    def __init__(self, value):
        super().__init__(SymbolKind.ConcreteInt)
        assert isinstance(value, int)
        self.value = value

    def __repr__(self):
        return "%d" % self.value

class CallSymbol(Symbol):
    def __init__(self, name, args):
        super().__init__(SymbolKind.Call)
        self.name = name
        self.args = args

    def __repr__(self):
        return "%s(%s)" % (self.name, self.args)

    @property
    def children(self):
        return self.args
        #return [self.name] + self.args

class StringLiteralSymbol(Symbol):
    def __init__(self, string):
        super().__init__(SymbolKind.StringLiteral)
        self.string = string

    def __repr__(self):
        return "\"%s\"" % self.string

class BinaryOperatorSymbol(Symbol):
    def __init__(self, lhs, op, rhs):
        super().__init__(SymbolKind.BinaryOperator)
        self.lhs = lhs
        self.op = op
        self.rhs = rhs

    def __repr__(self):
        try:
            return "%s %s %s" % (self.lhs, self.op, self.rhs)
        except:
            # fix bug
            return "0 == 0"

    @property
    def children(self):
        return [self.lhs, self.rhs]

class FieldSymbol(Symbol):
    def __init__(self, base, member):
        super().__init__(SymbolKind.Field)
        self.base = base
        self.member = member

    def __repr__(self):
        return "%s->%s" % (self.base, self.member)

    @property
    def children(self):
        return [self.base]

class ArraySymbol(Symbol):
    def __init__(self, base, index):
        super().__init__(SymbolKind.Array)
        self.base = base
        self.index = index

    def __repr__(self):
        return "%s[%s]" % (self.base, self.index)

    @property
    def children(self):
        return [self.base]

class ConstraintSymbol(Symbol):
    def __init__(self, symbol, constraints):
        super().__init__(SymbolKind.Constraint)
        self.constraints = constraints
        self.symbol = symbol

    def __repr__(self):
        return "Const(%s, %s)" % (self.symbol, self.constraints)

    @property
    def children(self):
        return [self.symbol]

class IDSymbol(Symbol):
    def __init__(self, id):
        super().__init__(SymbolKind.ID)
        self.id = id

    def __repr__(self):
        return "%s" % (self.id)


class UnknownSymbol(Symbol):
    def __init__(self):
        super().__init__(SymbolKind.Unknown)
