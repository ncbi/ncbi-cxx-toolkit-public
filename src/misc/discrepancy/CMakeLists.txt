# $Id$

set(_discrepancy_modules
    biosource_tests
    discrepancy_case
    division_code_conflicts
    feature_per_bioseq
    feature_tests
    gene_names
    overlapping_features
    pub_tests
    rna_names
    seqdesc_per_bioseq
    sequence_tests
    sesame_street
    #spell_check #disabled till better times...
    suspect_product_names
    transl_note
    transl_too_long
    deprecated_quals
)


NCBI_add_library(discrepancy)
NCBI_add_target(discrepancy.testnames)
NCBI_add_target(discrepancy.fsm)
NCBI_add_subdirectory(unit_test)
