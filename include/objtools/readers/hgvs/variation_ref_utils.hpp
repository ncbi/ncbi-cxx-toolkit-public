#ifndef VARIATION_REF_UTILS_HPP
#define VARIATION_REF_UTILS_HPP

#include <objects/seqfeat/Variation_ref.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(NHgvsTestUtils)

CRef<CVariation_ref> g_CreateSNV(const CSeq_data& nucleotide, 
                                 CRef<CDelta_item> offset=null);

CRef<CVariation_ref> g_CreateMNP(const CSeq_data& nucleotide,
                                 TSeqPos length,
                                 CRef<CDelta_item> offset=null);

CRef<CVariation_ref> g_CreateMissense(const CSeq_data& amino_acid);


CRef<CVariation_ref> g_CreateDeletion(CRef<CDelta_item> start_offset=null,
                                      CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateDuplication(CRef<CDelta_item> start_offset=null, 
                                         CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateIdentity(CRef<CSeq_literal> seq_literal,
                                      CRef<CDelta_item> start_offset=null,
                                      CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateDelins(CSeq_literal& insertion, 
                                    CRef<CDelta_item> start_offset=null,
                                    CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateInsertion(CSeq_literal& insertion, 
                                       CRef<CDelta_item> start_offset=null,
                                       CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateInversion(CRef<CDelta_item> start_offset=null,
                                       CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateMicrosatellite(CRef<CDelta_item> repeat_info,
                                            CRef<CDelta_item> start_offset=null,
                                            CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateConversion(const CSeq_loc& interval, 
                                        CRef<CDelta_item> start_offset=null,
                                        CRef<CDelta_item> stop_offset=null);


CRef<CVariation_ref> g_CreateFrameshift(void);

END_SCOPE(NHgvsTestUtils)
END_NCBI_SCOPE

#endif
