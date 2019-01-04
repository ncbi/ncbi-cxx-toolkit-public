cmake_minimum_required(VERSION 3.1)

PROJECT(pubseq_gateway VERSION 0.0.0)

INSTALL(PROGRAMS pubseq_gateway DESTINATION bin)

INSTALL(FILES pubseq_gateway.ini DESTINATION bin)

SET(CPACK_GENERATOR "ZIP")

INCLUDE(CPack)

