#!/bin/bash

echo "const char* sc_BuiltinConfig = \"\\"
cat idmapper_builtin_config.ini |
sed -e 's/$/ \\n\\/'
echo "\";"

