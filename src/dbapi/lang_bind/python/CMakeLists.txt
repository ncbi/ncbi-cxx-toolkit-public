add_library(python_ncbi_dbapi python_ncbi_dbapi)

include_directories(SYSTEM ${PYTHON_INCLUDE})

#LDFLAGS  = $(ORIG_LDFLAGS) $(RUNPATH_ORIGIN)/python_ncbi_dbapi/$(NCBI_PACKAGE_VERSION))

target_link_libraries(python_ncbi_dbapi
    dbapi xconnext xutil
    ${PYTHON_LIBS}
)

#KEEP_CORELIB = yes

add_subdirectory(tests)
