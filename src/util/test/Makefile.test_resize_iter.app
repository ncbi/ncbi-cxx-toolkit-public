# $Id$
# Author:  Aaron Ucko (ucko@ncbi.nlm.nih.gov)

# Build resizing iterator test application "test_resize_iter"
#################################

APP = test_resize_iter
SRC = test_resize_iter
LIB = xutil xncbi

CHECK_CMD = test_resize_iter "test" /CHECK_NAME=test_resize_iter
