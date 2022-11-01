#!/bin/bash
DIR=$(dirname $(readlink -f $0))

~/local/usr/bin/lcov -d ${DIR}/../../ipg -c -o code.coverage

~/local/usr/bin/lcov -e code.coverage '*pubseq_gateway/*ipg/*' -o extracted_coverage.info
~/local/usr/bin/genhtml extracted_coverage.info --ignore-errors source -t "pubseq_gateway/ipg " --num-spaces 4 --legend --demangle-cpp -p $(readlink -e ../../../../../..) -o code_coverage/

cp extracted_coverage.info code_coverage/
rsync -azv --delete-after ${DIR}/code_coverage/ ~/psg_ipg_code_coverage/
