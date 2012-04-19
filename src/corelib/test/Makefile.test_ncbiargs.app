# $Id$

APP = test_ncbiargs
SRC = test_ncbiargs
LIB = xncbi

CHECK_CMD = test_ncbiargs -k a -f1 -ko true foo - B False t f /CHECK_NAME=test_ncbiargs1
CHECK_CMD = test_ncbiargs -ko false -k 100 bar /CHECK_NAME=test_ncbiargs2
CHECK_CMD = test_ncbiargs -k a -f1 -ko true -kd8 123456789020 foo - B False t f /CHECK_NAME=test_ncbiargs3
CHECK_CMD = test_ncbiargs --key=123 foo - a -notakey /CHECK_NAME=test_ncbiargs4
CHECK_CMD = test_ncbiargs runtest1 abc-123-def -k kvalue /CHECK_NAME=test_ncbiargs5
CHECK_CMD = test_ncbiargs runtest2 123654 -ka alpha2 -kb true -ki 34 -kd 1.567 /CHECK_NAME=test_ncbiargs6

WATCHERS = gouriano
