# $Id$

function(check_definition)
  set(SCRIPT_NAME "${NCBI_TREE_ROOT}/${NCBI_DIRNAME_COMMON_SCRIPTS}/check/check_make_unix.sh")
  set(WORKDIR ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_BUILD})
  get_filename_component(_build ${NCBI_BUILD_ROOT} NAME)

  add_custom_target(check
    COMMAND ${SCRIPT_NAME} check.sh.list ${_build} ${WORKDIR} ${NCBI_TREE_ROOT} ${WORKDIR} check.sh
    COMMAND ${CMAKE_COMMAND} -E echo "To run tests: cd ${WORKDIR}\; ./check.sh run"
    DEPENDS ${SCRIPT_NAME}
    SOURCES ${SCRIPT_NAME}
    WORKING_DIRECTORY ${WORKDIR}
  )
endfunction()

NCBI_begin_custom_target(check)
  NCBI_requires(Linux)
  NCBI_custom_target_definition(check_definition)
NCBI_end_custom_target()
