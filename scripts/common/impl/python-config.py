#!/usr/bin/env python
# $Id$

import os
try:
    import sysconfig
except ImportError:
    from distutils import sysconfig
import sys

def lookup(want):
    if want == 'VERSION':
        return sysconfig.get_config_var('VERSION')
    elif want == 'INCLUDE':
        if os.name == "posix":
            tail = os.path.join('include', 'python' + lookup('VERSION'))
        else:
            tail = 'include'
        return ' '.join(['-I' + os.path.join(sysconfig.get_config_var(x), tail)
                         for x in ('exec_prefix', 'prefix')])
    elif want == 'LIBPATH':
        return ' '.join(sysconfig.get_config_vars('LIBDIR', 'LIBPL'))
    elif want == 'DEPS':
        return ' '.join(sysconfig.get_config_vars('LIBS', 'SYSLIBS'))
    elif want == 'LDVERSION':
        return (sysconfig.get_config_var('LDVERSION')
                or sysconfig.get_config_var('VERSION'))
    elif want == 'LIBS':
        return '-lpython' + lookup('LDVERSION') + ' ' + lookup('DEPS')
    else:
        raise RuntimeError('Unsupported mode ' + want)

print(lookup(sys.argv[1].lstrip('-').upper()))
