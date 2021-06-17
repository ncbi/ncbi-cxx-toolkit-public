#!/bin/sh
CONTENT_TYPE='multipart/form-data; boundary==-=-='
CONTENT_LENGTH=`wc -c < $1 | tr -d ' '`
#CONTENT_LENGTH=-1
REQUEST_METHOD=POST
export CONTENT_TYPE CONTENT_LENGTH REQUEST_METHOD
exec $CHECK_EXEC_STDIN test_cgi_entry_reader < $1
