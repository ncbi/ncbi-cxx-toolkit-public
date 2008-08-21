# $Id$

APP = test_ncbiargs_sample
SRC = test_ncbiargs_sample
LIB = xncbi

CHECK_CMD = test_ncbiargs_sample -k a -f1 -ko true foo - B False t f /CHECK_NAME=test_ncbiargs_sample1
CHECK_CMD = test_ncbiargs_sample -ko false -k 100 bar /CHECK_NAME=test_ncbiargs_sample2
CHECK_CMD = test_ncbiargs_sample -h /CHECK_NAME=test_ncbiargs_sample3
CHECK_CMD = test_ncbiargs_sample -help /CHECK_NAME=test_ncbiargs_sample4
