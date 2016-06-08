#!/usr/bin/env python
# $Id$

from distutils import sysconfig
import sys

def lookup(want):
    if want == 'VERSION':
        return sysconfig.get_config_var('VERSION')
    elif want == 'INCLUDE':
        return ('-I%s -I%s' % (sysconfig.get_python_inc(),
                               sysconfig.get_python_inc(True)))
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
