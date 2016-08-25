#!/usr/bin/env python3
from enum import Enum
from .symbol import CallSymbol
from .sparser import SParser
from ..lib import dbg

gid = 0

class EventKind(Enum):
    Call = "@LOG_CALL"
    Location = "@LOG_LOCATION"
    EOP = "@LOG_EOP"
    Assume = "@LOG_ASSUME"

class Event(object):
    def __init__(self):
        global gid
        self.id = gid
        gid += 1

    def __hash__(self):
        return self.id

    def _parse_symbol(self, string):
        try:
            parser = SParser()
            sym = parser.parse(string)
            return sym
        except Exception as e:
            #dbg.debug('Exception when parsing : %s' % e)
            return


class CallEvent(Event):
    def __init__(self, event):
        super().__init__()
        self.kind = EventKind.Call

        for child in event:
            if child.tag == "KIND":
                assert child.text == self.kind.value
            elif child.tag == "CALL":
                self.call = self._parse_call(child.text)
            elif child.tag == "CODE":
                self.code = child.text
            else:
                raise ValueError("Unknown tag for CallEvent")

    def _parse_call(self, text):
        sym = self._parse_symbol(text)
        if isinstance(sym, CallSymbol):
            return sym

class LocationEvent(Event):
    def __init__(self, event):
        super().__init__()
        self.kind = EventKind.Location

        for child in event:
            if child.tag == "KIND":
                assert child.text == self.kind.value
            elif child.tag == "LOC":
                self.loc = self._parse_symbol(child.text)
            elif child.tag == "TYPE":
                self.type = child.text
            elif child.tag == "CODE":
                self.code = child.text
            else:
                raise ValueError("Unknown tag for LocationEvent")

    def is_store(self):
        return self.type == "STORE"

class EOPEvent(Event):
    def __init__(self, event):
        super().__init__()
        self.kind = EventKind.EOP

        for child in event:
            if child.tag == "KIND":
                assert child.text == self.kind.value
            else:
                raise ValueError("Unknown tag for EOPEvent")

class AssumeEvent(Event):
    def __init__(self, event):
        super().__init__()
        self.kind = EventKind.Assume

        for child in event:
            if child.tag == "KIND":
                assert child.text == self.kind.value
            elif child.tag == "COND":
                self.cond = self.parse_cond(child.text)
            else:
                raise ValueError("Unknown tag for AssumeEvent")

    def parse_cond(self, cond):
        # XXX: symbol can be UnknownSymbol when parsing failed
        sym = self._parse_symbol(cond)
        return sym
