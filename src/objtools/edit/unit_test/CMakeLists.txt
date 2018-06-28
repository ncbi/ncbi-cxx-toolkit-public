#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(
  unit_test_autodef unit_test_entry_edit unit_test_field_handler
  unit_test_cds_fix unit_test_loc_edit unit_test_mail_report
  unit_test_source_edit unit_test_rna_edit unit_test_parse_text
  unit_test_pub_edit unit_test_gap_trim
)


if(OFF)
# Include projects from this directory
include(CMakeLists.unit_test_autodef.app.txt)
include(CMakeLists.unit_test_entry_edit.app.txt)
include(CMakeLists.unit_test_field_handler.app.txt)
include(CMakeLists.unit_test_cds_fix.app.txt)
include(CMakeLists.unit_test_loc_edit.app.txt)
include(CMakeLists.unit_test_mail_report.app.txt)
include(CMakeLists.unit_test_source_edit.app.txt)
include(CMakeLists.unit_test_rna_edit.app.txt)
include(CMakeLists.unit_test_parse_text.app.txt)
include(CMakeLists.unit_test_pub_edit.app.txt)
endif()
