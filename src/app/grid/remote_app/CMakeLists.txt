#############################################################################
# $Id$
#############################################################################

NCBI_add_app(remote_app ns_submit_remote_job ns_remote_job_control remote_cgi)

if (OFF)
# Include projects from this directory
include(CMakeLists.remote_app.app.txt)
include(CMakeLists.remote_cgi.app.txt)
endif()
