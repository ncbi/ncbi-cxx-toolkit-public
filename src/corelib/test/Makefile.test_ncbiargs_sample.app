# $Id$

APP = test_ncbiargs_sample
SRC = test_ncbiargs_sample
LIB = xncbi

CHECK_CMD = test_ncbiargs -k a -f1 -ko true foo - B False t f
CHECK_CMD = test_ncbiargs -ko false -k 100 bar
