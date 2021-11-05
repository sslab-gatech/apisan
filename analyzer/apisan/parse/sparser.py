# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
import os
import sys
from ply import yacc
from ply.lex import TOKEN
from .slexer import SLexer
from ..lib import dbg
from .symbol import (
    BinaryOperatorSymbol, ConstraintSymbol, FieldSymbol, ArraySymbol,
    CallSymbol, IDSymbol, ConcreteIntSymbol, StringLiteralSymbol
)

# for LALR table reuse
ROOT = os.path.dirname(__file__)
sys.path.append(ROOT)

class SParser(object):
    # Precedence rules for the arithmetic operators
    precedence = (
            ('left', 'LOR'),
            ('left', 'LAND'),
            ('left', 'OR'),
            ('left', 'XOR'),
            ('left', 'AND'),
            ('left', 'EQ', 'NE'),
            ('left', 'GT', 'GE', 'LT', 'LE'),
            ('left', 'RSHIFT', 'LSHIFT'),
            ('left', 'PLUS', 'MINUS'),
            ('left', 'TIMES', 'DIVIDE', 'MOD')
    )

    def __init__(self, **kwargs):
        self.slex = SLexer()
        self.slex.build()
        self.tokens = self.slex.tokens
        self.yacc = yacc.yacc(module=self)

    def p_expression_1(self, p):
        ''' expression : binary_expression '''
        p[0] = p[1]

    def p_binary_expression_1(self, p):
        ''' binary_expression   : cast_expression '''
        p[0] = p[1]

    def p_binary_expression_2(self, p):
        ''' binary_expression   : binary_expression TIMES binary_expression
                                | binary_expression DIVIDE binary_expression
                                | binary_expression MOD binary_expression
                                | binary_expression PLUS binary_expression
                                | binary_expression MINUS binary_expression
                                | binary_expression RSHIFT binary_expression
                                | binary_expression LSHIFT binary_expression
                                | binary_expression LT binary_expression
                                | binary_expression LE binary_expression
                                | binary_expression GE binary_expression
                                | binary_expression GT binary_expression
                                | binary_expression EQ binary_expression
                                | binary_expression NE binary_expression
                                | binary_expression AND binary_expression
                                | binary_expression OR binary_expression
                                | binary_expression XOR binary_expression
                                | binary_expression LAND binary_expression
                                | binary_expression LOR binary_expression
        '''
        p[0] = BinaryOperatorSymbol(p[1], p[2], p[3])

    def p_binary_expression_3(self, p):
        # expr CONSTRAINT_OP constraints
        ''' expression  : expression CONSTRAINT_OP LBRACE constraint_list RBRACE '''
        p[0] = ConstraintSymbol(p[1], p[4])

    def p_constraint(self, p):
        ''' constraint  : LBRACKET concrete_integer_expression COMMA concrete_integer_expression RBRACKET '''
        p[0] = (p[2], p[4])

    def p_constraint_list(self, p):
        ''' constraint_list : constraint_list COMMA constraint
                            | constraint '''
        if len(p) == 2:
            p[0] = [p[1]]
        else:
            p[0] = p[1]
            p[1].append(p[3])

    def p_cast_expression_1(self, p):
        """ cast_expression : unary_expression """
        p[0] = p[1]

    def p_unary_expression_1(self, p):
        """ unary_expression    : postfix_expression """
        p[0] = p[1]

    def p_unary_expression_2(self, p):
        """ unary_expression    : AND postfix_expression """
        # XXX : needs to handle & operator
        p[0] = p[2]

    def p_postfix_expression_1(self, p):
        ''' postfix_expression : primary_expression '''
        p[0] = p[1]

    def p_postfix_expression_2(self, p):
        ''' postfix_expression : postfix_expression ARROW ID'''
        p[0] = FieldSymbol(p[1], p[3])

    def p_postfix_expression3(self, p):
        ''' postfix_expression : postfix_expression LBRACKET expression RBRACKET '''
        p[0] = ArraySymbol(p[1], p[3])

    def p_postfix_expression4(self, p):
        ''' postfix_expression : postfix_expression LPAREN argument_list RPAREN '''
        p[0] = CallSymbol(p[1], p[3])

    def p_primary_expression_1(self, p):
        ''' primary_expression : ID '''
        p[0] = IDSymbol(p[1])

    def p_primary_expression_2(self, p):
        ''' primary_expression : concrete_integer_expression '''
        p[0] = ConcreteIntSymbol(p[1])

    def p_primary_expression_3(self, p):
        '''primary_expression : LPAREN expression RPAREN'''
        p[0] = p[2]

    def p_primary_expression_4(self, p):
        ''' primary_expression : STRING_LITERAL '''
        p[0] = StringLiteralSymbol(p[1])

    def p_concrete_integer(self, p):
        ''' concrete_integer_expression : INT_CONST_DEC
                                        | MINUS INT_CONST_DEC '''
        if len(p) == 3:
            p[0] = -int(p[2])
        else:
            p[0] = int(p[1])

    def p_argument_list(self, p):
        ''' argument_list :
                            | expression
                            | argument_list COMMA expression '''
        if len(p) == 1:
            p[0] = []
        elif len(p) == 2:
            p[0] = [p[1]]
        else:
            p[0] = p[1]
            p[1].append(p[3])

    def parse(self, text):
        self.last_text = text
        return self.yacc.parse(input = text,
                                lexer = self.slex)

    def p_error(self, p):
        #dbg.debug('Illegal token %s' % repr(p))
        #dbg.debug('Text : %s' % self.last_text)
        return

if __name__ == '__main__':
    parser = SParser()
    tests = ["\"String Literal\\n\"",
                "malloc(256)@={ [0, 0] }",
                "malloc(256)@={ [0, 0], [2, 18446744073709551615] }"]

    for test in tests:
        print(parse_symbol(test))

