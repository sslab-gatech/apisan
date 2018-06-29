#!/usr/bin/env python3
from .lex import lex, TOKEN
import logging

logger = logging.getLogger('SLexer')

class SLexer(object):
    keywords = ()

    keyword_map = {}
    for keyword in keywords:
        keyword_map[keyword.lower()] = keyword

    tokens = keywords + (
        'ID',

        # Operators
        'PLUS', 'MINUS', 'TIMES', 'DIVIDE', 'MOD',
        'OR', 'AND', 'NOT', 'XOR', 'LSHIFT', 'RSHIFT',
        'LOR', 'LAND', 'LNOT',
        'LT', 'LE', 'GT', 'GE', 'EQ', 'NE',

        # Delimeters
        'LPAREN', 'RPAREN',         # ( )
        'LBRACKET', 'RBRACKET',
        'LBRACE', 'RBRACE',
        'COLON',                    # :
        'COMMA',                    # ,

        # constants
        'INT_CONST_DEC',

        # String literals
        'STRING_LITERAL',

        # pointer,
        'ARROW',

        # constraint operator
        'CONSTRAINT_OP'
    )

    identifier = r'[a-zA-Z_$][0-9a-zA-Z_$]*'

    # Operators
    t_PLUS              = r'\+'
    t_MINUS             = r'-'
    t_TIMES             = r'\*'
    t_DIVIDE            = r'/'
    t_MOD               = r'%'
    t_OR                = r'\|'
    t_AND               = r'&'
    t_NOT               = r'~'
    t_XOR               = r'\^'
    t_LSHIFT            = r'<<'
    t_RSHIFT            = r'>>'
    t_LOR               = r'\|\|'
    t_LAND              = r'&&'
    t_LNOT              = r'!'
    t_LT                = r'<'
    t_GT                = r'>'
    t_LE                = r'<='
    t_GE                = r'>='
    t_EQ                = r'=='
    t_NE                = r'!='

    t_COLON             = r':'

    t_LPAREN            = r'\('
    t_RPAREN            = r'\)'

    t_LBRACKET          = r'\['
    t_RBRACKET          = r'\]'

    t_LBRACE            = r'\{'
    t_RBRACE            = r'\}'

    t_COMMA             = r','
    t_ARROW             = r'->'
    t_CONSTRAINT_OP     = r'@='
    t_ignore            = ' \t'

    # integer constants (K&R2: A.2.5.1)
    integer_suffix_opt = r'(([uU]ll)|([uU]LL)|(ll[uU]?)|(LL[uU]?)|([uU][lL])|([lL][uU]?)|[uU])?'
    decimal_constant = '(0'+integer_suffix_opt+')|([1-9][0-9]*'+integer_suffix_opt+')'

    # character constants (K&R2: A.2.5.2)
    # Note: a-zA-Z and '.-~^_!=&;,' are allowed as escape chars to support #line
    # directives with Windows paths as filenames (..\..\dir\file)
    # For the same reason, decimal_escape allows all digit sequences. We want to
    # parse all correct code, even if it means to sometimes parse incorrect
    # code.
    #
    simple_escape = r"""([a-zA-Z._~!=&\^\-\\?'"])"""
    decimal_escape = r"""(\d+)"""
    hex_escape = r"""(x[0-9a-fA-F]+)"""
    bad_escape = r"""([\\][^a-zA-Z._~^!=&\^\-\\?'"x0-7])"""

    escape_sequence = r"""(\\("""+simple_escape+'|'+decimal_escape+'|'+hex_escape+'))'
    cconst_char = r"""([^'\\\n]|"""+escape_sequence+')'
    char_const = "'"+cconst_char+"'"
    wchar_const = 'L'+char_const
    unmatched_quote = "('"+cconst_char+"*\\n)|('"+cconst_char+"*$)"
    bad_char_const = r"""('"""+cconst_char+"""[^'\n]+')|('')|('"""+bad_escape+r"""[^'\n]*')"""

    # string literals (K&R2: A.2.6)
    string_char = r"""([^"\\\n]|"""+escape_sequence+')'
    string_literal = '"'+string_char+'*"'

    t_STRING_LITERAL = string_literal

    def build(self, **kwargs):
        self.lexer = lex.lex(object=self, **kwargs)

    def input(self, text):
        self.text = text
        self.lexer.input(text)

    def token(self):
        self.last_token = self.lexer.token()
        return self.last_token

    @TOKEN(identifier)
    def t_ID(self, t):
        t.type = self.keyword_map.get(t.value, "ID")
        return t

    @TOKEN(decimal_constant)
    def t_INT_CONST_DEC(self, t):
        return t

    def t_error(self, t):
        logger.debug('Illegal character %s' % repr(t.value[0]))
        logger.debug('Text %s' % self.text)


def parse_tokens(input):
    lexer.input(input)
    while True:
        token = lexer.token()
        if not token:
            break
        print(token)

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    lexer = SLexer()
    lexer.build()
    tests = ["malloc(256)"]

    for test in tests:
        parse_tokens(test)
