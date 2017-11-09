cmake_minimum_required(VERSION 3.1)

PROJECT(ncbi_applog VERSION 1.2.4)

INSTALL(PROGRAMS app/ncbi_applog DESTINATION bin)
INSTALL(PROGRAMS app/ncbi_applog.cgi DESTINATION bin)

INSTALL(FILES libclog.a DESTINATION lib)

SET(CPACK_GENERATOR "ZIP")
INCLUDE(CPack)

