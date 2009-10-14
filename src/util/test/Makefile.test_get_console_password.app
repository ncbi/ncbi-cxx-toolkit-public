# $Id$
# Author:  Sergey Satskiy (satskyse@ncbi.nlm.nih.gov)

# Build console password getting test application "test_get_console_password"
#################################

APP = test_get_console_password
SRC = test_get_console_password
LIB = xutil xncbi

# The test should not be run in a batch mode
# CHECK_CMD = test_get_console_password

WATCHERS = satskyse
