#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(demo)
NCBI_add_app(cpgdemo regexplocdemo xcompareannotsdemo demo_gene_model adapter_search)

# Include projects from this directory
#include(CMakeLists.cpgdemo.app.txt)
#include(CMakeLists.regexplocdemo.app.txt)
#include(CMakeLists.xcompareannotsdemo.app.txt)
#include(CMakeLists.demo_gene_model.app.txt)
#include(CMakeLists.adapter_search.app.txt)

