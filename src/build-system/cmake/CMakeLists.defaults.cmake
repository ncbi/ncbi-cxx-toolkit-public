cmake_minimum_required(VERSION 2.8) 
project(CPP) 


## set(CMAKE_C_COMPILER   /usr/local/gcc/4.8.1/bin/gcc)
## set(CMAKE_CXX_COMPILER /usr/local/gcc/4.8.1/bin/g++)
## 
## set(top_src_dir     ${CMAKE_CURRENT_SOURCE_DIR}/..)
## set(abs_top_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/..)
## set(build_root      ${CMAKE_BINARY_DIR})
## set(builddir        ${CMAKE_BINARY_DIR})
## set(includedir0     ${top_src_dir}/include)
## set(includedir      ${includedir0})
## set(incdir          ${build_root}/inc)
## set(incinternal     ${includedir0}/internal)
## 
## #set(buildconf    GCC401-DebugMT64)
## #set(buildconf0   GCC401-DebugMT)
## 
## #set(CONF_CFLAGS    " -Wall -Wno-format-y2k  -pthread -fPIC  -gdwarf-3")
## #set(CONF_CXXFLAGS  " -Wall -Wno-format-y2k  -pthread -fPIC  -gdwarf-3 -std=gnu++11 -Wno-unused-local-typedefs")
## #set(CONF_CPPFLAGS  )
## set(STD_INCLUDE    " -I${incdir} -I${includedir0} -I${incinternal}")
## set(ORIG_CPPFLAGS  " ${CONF_CXXFLAGS} ${CONF_CPPFLAGS} ${STD_INCLUDE}")
## 
## #set(CMAKE_CXX_FLAGS "${CONF_CXXFLAGS} ${CONF_CPPFLAGS} ${STD_INCLUDE}")
## #set(CMAKE_C_FLAGS   "${CONF_CFLAGS}                    ${STD_INCLUDE}")
## 
## set(NCBI_TOOLS_ROOT $ENV{NCBI})
## 
## #set (ARCHIVE_OUTPUT_DIRECTORY ${NCBI_BUILD_ROOT}/../lib)
## #set (LIBRARY_OUTPUT_DIRECTORY ${NCBI_BUILD_ROOT}/../lib)
## #set (RUNTIME_OUTPUT_DIRECTORY ${NCBI_BUILD_ROOT}/../bin)
## #set (PDB_OUTPUT_DIRECTORY     ${NCBI_BUILD_ROOT}/../bin)
## 
## #set (LIBRARY_OUTPUT_PATH     ${NCBI_BUILD_ROOT}/../lib)
## #set (EXECUTABLE_OUTPUT_PATH  ${NCBI_BUILD_ROOT}/../bin)
## 
## set(LIBRARY_OUTPUT_PATH     ${build_root}/lib)
## set(EXECUTABLE_OUTPUT_PATH  ${build_root}/bin)
## set(CMAKE_INCLUDE_PATH      ${CMAKE_INCLUDE_PATH} ${includedir0})
## 
## set (NCBI_BUILD_BIN ${EXECUTABLE_OUTPUT_PATH})
## 
## #set (NCBI_DATATOOL $NCBI/bin/datatool)
## set (NCBI_DATATOOL /netopt/ncbi_tools64/bin/datatool)
## 
## ENABLE_TESTING()
## 
## include_directories(${incdir} ${includedir0} ${incinternal})
## include(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/CMakeChecks.cmake)


