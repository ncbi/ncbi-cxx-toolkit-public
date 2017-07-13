# $Id$
# Barebones YAML dumper for run_cd_reporter.py(.in).
import datetime
import math
import numbers
import re

yes_pattern   = r'^(?:[Yy]|[Yy]es|YES|[Tt]rue|TRUE|[^Oo]n|ON)$'
no_pattern    = r'^(?:[Nn]o?|NO|[Ff]alse|FALSE|[Oo]ff|OFF)$'
null_pattern  = r'^(?:[Nn]ull|NULL|\~|)$'
float_pattern = r'^[-+]?(?:\d[0-9_]*(?:\.[0-9_]*)?|\.[0-9_]*)(?:[Ee][+-]?\d+)?$'
flt60_pattern = r'^[-+]?\d[0-9_]*(?::[0-5]?\d)+\.[0-9_]*$'
inf_pattern   = r'^[-+]?\.(?:[Ii]nf|INF)$'
nan_pattern   = r'^\.(?:nan|NaN|NAN)$'
int_pattern   = r'^(?:0|[-+]?[1-9][0-9_]+)$'
int2_pattern  = r'^[-+]?0b[01_]+$'
int8_pattern  = r'^[-+]?0[0-7]+$'
int16_pattern = r'^[-+]?0x[0-9a-fA-F_]+$'
int60_pattern = r'^[-+]?[1-9][0-9_]*(?::[0-5]?\d)+$'
time_pattern  = (r'^\d{4}-(?:\d\d-\d\d|\d\d?-\d\d?(?:[Tt]|\s+)\d\d?:\d\d?:\d\d?'
                 + r'(?:\.\d+)?(?:\s*(?:Z|[-+]\d\d?(?::\d\d)?))?)$')
special_patterns = (r'[^$()+./0-9;A-Z^_a-z]',
                    r'^\.\.\.$',
                    yes_pattern,
                    no_pattern,
                    null_pattern,
                    float_pattern,
                    flt60_pattern,
                    inf_pattern,
                    nan_pattern,
                    int_pattern,
                    int2_pattern,
                    int8_pattern,
                    int16_pattern,
                    int60_pattern,
                    time_pattern)
                    
special_re = re.compile('|'.join(special_patterns))

class Dumper(object):
    def __init__(self, stream, indent = ''):
        self.stream = stream
        self.indent = indent

    def is_scalar(self, x):
        return (isinstance(x, bool) or isinstance(x, str)
                or isinstance(x, numbers.Real) or isinstance(x, datetime.date)
                or len(x) == 0)
        
    def dump(self, x):
        if x is None:
            self.stream.write('~')
        if isinstance(x, bool):
            if x:
                self.stream.write('y')
            else:
                self.stream.write('n')
        elif isinstance(x, str):
            if special_re.search(x) is None:
                self.stream.write(x)
            else:
                self.stream.write('"' + re.escape(x) + '"')
        elif isinstance(x, numbers.Real):
            if math.isinf(x):
                if x < 0:
                    self.stream.write('-')
                self.stream.write('.inf')
            elif math.isnan(x):
                self.stream.write('.nan')
            else:
                self.stream.write(repr(x))
        elif isinstance(x, datetime.date):
            self.stream.write(x.isoformat())
        elif len(x) == 0:
            if isinstance(x, dict):
                self.stream.write('{}')
            else: # list or tuple
                self.stream.write('[]')
        else:
            d = Dumper(self.stream, self.indent + '  ')
            if isinstance(x, dict):
                for k, v in x.items():
                    self.stream.write(self.indent)
                    d.dump(k)
                    self.stream.write(':')
                    if self.is_scalar(v):
                        self.stream.write(' ')
                        d.dump(v)
                        self.stream.write("\n")
                    else:
                        self.stream.write("\n")
                        d.dump(v)
            elif isinstance(x, list) or isinstance(x, tuple):
                for v in x:
                    self.stream.write(self.indent + '-')
                    if self.is_scalar(v):
                        self.stream.write(' ')
                        d.dump(v)
                        self.stream.write("\n")
                    else:
                        self.stream.write("\n")
                        d.dump(v)
            else:
                raise TypeError(x)

def dump(x, stream, Dumper=Dumper):
    d = Dumper(stream)
    d.dump(x)
    if d.is_scalar(x):
        stream.write("\n")
