# $Id$

APP = test_ncbiargs
SRC = test_ncbiargs
LIB = xncbi

CHECK_CMD = test_ncbiargs -k a -f1 -ko true foo - B False t f
CHECK_CMD = test_ncbiargs -ko false -k 100 bar
CHECK_CMD = test_ncbiargs -k a -f1 -ko true -kd8 123456789020 foo - B False t f
