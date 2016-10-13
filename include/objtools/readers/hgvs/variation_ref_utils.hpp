#ifndef VARIATION_REF_UTILS_HPP
#define VARIATION_REF_UTILS_HPP

#include <objects/seqfeat/Variation_ref.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);
BEGIN_SCOPE(NHgvsTestUtils)

CRef<CVariation_ref> g_CreateSNV(const CSeq_data& nucleotide, 
                                 CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                 CRef<CDelta_item> offset=null);

CRef<CVariation_ref> g_CreateMNP(const CSeq_data& nucleotide,
                                 TSeqPos length,
                                 CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown, 
                                 CRef<CDelta_item> offset=null);

CRef<CVariation_ref> g_CreateMissense(const CSeq_data& amino_acid,
                                      CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown);


CRef<CVariation_ref> g_CreateDeletion(CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                      CRef<CDelta_item> start_offset=null,
                                      CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateDuplication(CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                         CRef<CDelta_item> start_offset=null, 
                                         CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateIdentity(CRef<CSeq_literal> seq_literal,
                                      CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                      CRef<CDelta_item> start_offset=null,
                                      CRef<CDelta_item> stop_offset=null,
                                      bool enforce_assert=false);

CRef<CVariation_ref> g_CreateVarref(CRef<CSeq_literal> seq_literal,
                                    CVariation_inst::EType type,
                                    CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                    CRef<CDelta_item> start_offset=null,
                                    CRef<CDelta_item> stop_offset=null,
                                    bool enforce_assert=false);

CRef<CVariation_ref> g_CreateDelins(CSeq_literal& insertion, 
                                    CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                    CRef<CDelta_item> start_offset=null,
                                    CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateInsertion(CSeq_literal& insertion, 
                                       CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                       CRef<CDelta_item> start_offset=null,
                                       CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateInversion(CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                       CRef<CDelta_item> start_offset=null,
                                       CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateMicrosatellite(CRef<CDelta_item> repeat_info,
                                            CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                            CRef<CDelta_item> start_offset=null,
                                            CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateConversion(const CSeq_loc& interval, 
                                        CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown,
                                        CRef<CDelta_item> start_offset=null,
                                        CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateFrameshift(CVariation_ref::EMethod_E method = CVariation_ref::eMethod_E_unknown);


END_SCOPE(NHgvsTestUtils)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
