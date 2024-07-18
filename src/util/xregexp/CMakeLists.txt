# $Id$

NCBI_project_tags(core)
# NCBI_optional_components(PCRE PCRE2)
if(NCBI_COMPONENT_PCRE2_FOUND)
  NCBI_REQUIRES(PCRE2)
else()
  NCBI_REQUIRES(PCRE)
endif()
NCBI_add_library(xregexp xregexp_template_tester)

