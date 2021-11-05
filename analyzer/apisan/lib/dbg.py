# SPDX-License-Identifier: MIT
import os
import sys

#
# usage:
#
#    import dbg
#
#    dbg.test("#B<red text#>")
#    dbg.info("this is info")
#    dbg.error("this is #R<error#>")
#

#    dbg.save_to_file(path, color=T/F)
#    dbg.set_queit(tags)
#    dbg.set_header()

#
# <<func>>   : function name
# <<line>>   : line number
# <<file>>   : file name
# <<tag>>    : tag name
# #B<        : blue
# #R<        : red
# #G<        : green
# #Y<        : yellow
# #C<        : cyan
# #>         : end mark
#

_header = "'[#R<%-10s#>|#B<%-20s#>] ' % (<<tag>>,<<func>>)"

# reference color
_BLACK, _RED, _GREEN, _YELLOW, _BLUE, _MAGENTA, _CYAN, _WHITE = range(8)

def _currentframe() :
    try :
        raise Exception
    except :
        return sys.exc_info()[2].tb_frame.f_back

def _formatting(msg, tag, rv) :
    h = msg
    h = h.replace("<<tag>>", repr(str(tag)))
    h = h.replace("<<func>>", repr(rv[2]))
    h = h.replace("<<line>>", repr(rv[1]))
    h = h.replace("<<file>>", repr(rv[0]))
    return _coloring(eval(h))

def _coloring(msg) :
    h = msg
    h = h.replace("#B<", "\033[3%dm" % _BLUE)
    h = h.replace("#G<", "\033[3%dm" % _GREEN)
    h = h.replace("#R<", "\033[3%dm" % _RED)
    h = h.replace("#Y<", "\033[3%dm" % _YELLOW)
    h = h.replace("#C<", "\033[3%dm" % _CYAN)
    h = h.replace("#>" , "\033[m")
    return h

# when ignored, don't stringify the objects
def _dbg(tag, fmt, *msglist):
    f = _currentframe()

    # use frame of caller's caller, as this is called via wrapper
    # static methods of the dbg class
    if f is not None:
        f = f.f_back.f_back

    # look up frames
    rv = "(unknown file)", 0, "(unknown function)"
    while hasattr(f, "f_code"):
        co = f.f_code
        filename = os.path.normcase(co.co_filename)
        if filename in [__file__, "<string>"]:
            f = f.f_back
            continue
        rv = (filename, f.f_lineno, co.co_name)
        break

    # convert to str
    msg = fmt % msglist
    sys.stderr.write(("%s %s\n" % (_formatting(_header, tag, rv),
                                   _coloring(msg))))

global settings
settings = None

def _stop():
    import pdb
    pdb.Pdb().set_trace(sys._getframe().f_back)

def _quiet(tags):
    global settings
    settings = tags

# attribute factories
class Wrapper(object):
    def __init__(self, wrapped):
        self.wrapped = wrapped
    def __getattr__(self, tag):
        global settings
        if tag == "stop":
            return _stop
        if tag == "quiet":
            return _quiet
        if settings is None or not tag in settings:
            return lambda *fmt: _dbg(tag, *fmt)
        return lambda *fmt: None

sys.modules[__name__] = Wrapper(sys.modules[__name__])
