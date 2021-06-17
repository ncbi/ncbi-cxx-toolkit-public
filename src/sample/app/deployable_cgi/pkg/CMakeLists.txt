cmake_minimum_required(VERSION 3.1)

PROJECT(deployable_cgi VERSION 1.2.4)

INSTALL(PROGRAMS deployable_cgi.cgi DESTINATION bin)
INSTALL(FILES deployable_cgi.html DESTINATION share)
INSTALL(FILES deployable_cgi_env.html DESTINATION share)
INSTALL(FILES pkg/Procfile DESTINATION .)
INSTALL(FILES pkg/.env DESTINATION etc)
INSTALL(FILES pkg/.keys DESTINATION etc)
INSTALL(FILES deployable_cgi.ini DESTINATION bin)

SET(CPACK_GENERATOR "ZIP")

INCLUDE(CPack)

