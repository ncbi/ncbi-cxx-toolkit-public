#!/usr/bin/env python

# $Id$
#
# Generate DECLARE_COBJECT macro calls for SWIG for all COjects
# in a module for which the .py has already been generated.
# This also requires .py files for any modules that are
# %import'ed by this SWIG module (or at least that which
# contains CObject and any that contain base classes
# of the CObjects in this module).  These must
# be given on the command line.
#
# Author: Josh Cherry

import sys, os

if len(sys.argv) < 2:
    sys.stderr.write('usage:  %s module.py [imported_module.py ...]\n' % \
                     sys.argv[0])
    sys.exit(1)

import parse_py
modules = {}
for i in range(1, len(sys.argv)):
    s = open(sys.argv[i]).read()
    classes = parse_py.parse(s)
    module_name = os.path.splitext(os.path.split(sys.argv[i])[1])[0]
    if i == 1:
        main_module_name = module_name
    modules[module_name] = classes


outf = open('%s_cobjects.i' % main_module_name, 'w')


# Figure out whether module_name.cname is a CObject
# by walking up the inheritance hierarchy recursively, perhaps
# across modules.
def IsCObject(module_name, cname, modules):
    if cname == 'CObject':
        return True
    if cname == 'object':
        # the Python hierarchy is rooted at 'object'
        return False
    for base in modules[module_name][cname].bases:
        split = base.split('.')
        if len(split) == 1:
            base_cname = split[0]
            base_module_name = module_name
        elif len(split) == 2:
            base_cname = split[1]
            base_module_name = split[0]
        else:
            raise RuntimeError, 'too many fields in %s' % base
        if not base_module_name in modules or \
               not base_cname in modules[base_module_name]:
            if not base_cname == 'object':
                sys.stderr.write('Warning: base class %s of %s.%s not found\n' \
                                 % (base, module_name, cname))
        else:
            if IsCObject(base_module_name, base_cname, modules):
                return True
    return False

for cname in modules[main_module_name]:
    if IsCObject(main_module_name, cname, modules):
        outf.write('DECLARE_COBJECT(%s, %s);\n' \
                   % (cname, modules[main_module_name][cname].qual_name))

outf.close()
