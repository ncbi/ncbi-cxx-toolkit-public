set(MONGOCXX_INCLUDE  ${NCBI_TOOLS_ROOT}/mongodb-3.0.2/include/mongocxx/v_noabi 
                      ${NCBI_TOOLS_ROOT}/mongodb-3.0.2/include/bsoncxx/v_noabi)
set(MONGOCXX_LIB   -L${NCBI_TOOLS_ROOT}/mongodb-3.0.2/lib -Wl,-rpath,${NCBI_TOOLS_ROOT}/mongodb-3.0.2/lib -lmongocxx -lbsoncxx -lssl -lcrypto -lsasl2)
