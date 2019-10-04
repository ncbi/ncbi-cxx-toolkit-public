/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Justin Foley
 *
 */
    
#ifndef _MOD_TO_ENUM_HPP_
#define _MOD_TO_ENUM_HPP_

#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seq/MolInfo.hpp>
#include <unordered_map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

template<typename TEnum> 
using TStringToEnumMap = unordered_map<string,TEnum>;

string g_GetNormalizedModVal(const string& unnormalized);


TStringToEnumMap<COrgMod::ESubtype>
g_InitModNameOrgSubtypeMap(void);


TStringToEnumMap<CSubSource::ESubtype>
g_InitModNameSubSrcSubtypeMap(void);


TStringToEnumMap<CBioSource::EGenome>
g_InitModNameGenomeMap(void);


TStringToEnumMap<CBioSource::EOrigin>
g_InitModNameOriginMap(void);


static const TStringToEnumMap<CSeq_inst::EStrand> 
s_StrandStringToEnum = {{"single", CSeq_inst::eStrand_ss},
                        {"double", CSeq_inst::eStrand_ds},
                        {"mixed", CSeq_inst::eStrand_mixed},
                        {"other", CSeq_inst::eStrand_other}};


static const TStringToEnumMap<CSeq_inst::EMol>
s_MolStringToEnum = {{"dna", CSeq_inst::eMol_dna},
                     {"rna", CSeq_inst::eMol_rna},
                     {"aa", CSeq_inst::eMol_aa},
                     {"na", CSeq_inst::eMol_na},
                    {"other", CSeq_inst::eMol_other}};


static const TStringToEnumMap<CSeq_inst::ETopology> 
s_TopologyStringToEnum = {{"linear", CSeq_inst::eTopology_linear},
                          {"circular", CSeq_inst::eTopology_circular},
                          {"tandem", CSeq_inst::eTopology_tandem},
                          {"other", CSeq_inst::eTopology_other}};


static const 
TStringToEnumMap<CMolInfo::TBiomol> s_BiomolStringToEnum
= { {"crna",                    CMolInfo::eBiomol_cRNA },   
    {"dna",                     CMolInfo::eBiomol_genomic},   
    {"genomic",                 CMolInfo::eBiomol_genomic},   
    {"genomicdna",              CMolInfo::eBiomol_genomic},   
    {"genomicrna",              CMolInfo::eBiomol_genomic},   
    {"mrna",                    CMolInfo::eBiomol_mRNA},   
    {"ncrna",                   CMolInfo::eBiomol_ncRNA},
    {"noncodingrna",            CMolInfo::eBiomol_ncRNA},   
    {"othergenetic",            CMolInfo::eBiomol_other_genetic}, 
    {"precursorrna",            CMolInfo::eBiomol_pre_RNA},   
    {"ribosomalrna",            CMolInfo::eBiomol_rRNA},   
    {"rrna",                    CMolInfo::eBiomol_rRNA},   
    {"transcribedrna",          CMolInfo::eBiomol_transcribed_RNA},   
    {"transfermessengerrna",    CMolInfo::eBiomol_tmRNA},   
    {"tmrna",                   CMolInfo::eBiomol_tmRNA},   
    {"transferrna",             CMolInfo::eBiomol_tRNA},   
    {"trna",                    CMolInfo::eBiomol_tRNA},   
};


static const 
unordered_map<CMolInfo::TBiomol, CSeq_inst::EMol> s_BiomolEnumToMolEnum
= { { CMolInfo::eBiomol_genomic, CSeq_inst::eMol_dna},

    { CMolInfo::eBiomol_pre_RNA,  CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_mRNA,  CSeq_inst::eMol_rna },
    { CMolInfo::eBiomol_rRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_tRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_snRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_scRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_genomic_mRNA, CSeq_inst::eMol_rna },
    { CMolInfo::eBiomol_cRNA, CSeq_inst::eMol_rna },
    { CMolInfo::eBiomol_snoRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_transcribed_RNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_ncRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_tmRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_peptide, CSeq_inst::eMol_aa},
    { CMolInfo::eBiomol_other_genetic, CSeq_inst::eMol_other},
    { CMolInfo::eBiomol_other, CSeq_inst::eMol_other}
};


static const 
TStringToEnumMap<CMolInfo::TTech>
s_TechStringToEnum = {
    { "?",                  CMolInfo::eTech_unknown },
    { "barcode",            CMolInfo::eTech_barcode },
    { "both",               CMolInfo::eTech_both },
    { "compositewgshtgs",   CMolInfo::eTech_composite_wgs_htgs },
    { "concepttrans",       CMolInfo::eTech_concept_trans },
    { "concepttransa",      CMolInfo::eTech_concept_trans_a },
    { "derived",            CMolInfo::eTech_derived },
    { "est",                CMolInfo::eTech_est },
    { "flicdna",            CMolInfo::eTech_fli_cdna },
    { "geneticmap",         CMolInfo::eTech_genemap },
    { "htc",                CMolInfo::eTech_htc },
    { "htgs0",              CMolInfo::eTech_htgs_0 },
    { "htgs1",              CMolInfo::eTech_htgs_1 },
    { "htgs2",              CMolInfo::eTech_htgs_2 },
    { "htgs3",              CMolInfo::eTech_htgs_3 },
    { "physicalmap",        CMolInfo::eTech_physmap },
    { "seqpept",            CMolInfo::eTech_seq_pept },
    { "seqpepthomol",       CMolInfo::eTech_seq_pept_homol },
    { "seqpeptoverlap",     CMolInfo::eTech_seq_pept_overlap },
    { "standard",           CMolInfo::eTech_standard },
    { "sts",                CMolInfo::eTech_sts },
    { "survey",             CMolInfo::eTech_survey },
    { "targeted",           CMolInfo::eTech_targeted },
    { "tsa",                CMolInfo::eTech_tsa },
    { "wgs",                CMolInfo::eTech_wgs }
};


static const 
unordered_map<string, CMolInfo::TCompleteness> 
s_CompletenessStringToEnum = {
    { "complete",  CMolInfo::eCompleteness_complete  },
    { "hasleft",   CMolInfo::eCompleteness_has_left  },
    { "hasright",  CMolInfo::eCompleteness_has_right  },
    { "noends",    CMolInfo::eCompleteness_no_ends  },
    { "noleft",    CMolInfo::eCompleteness_no_left  },
    { "noright",   CMolInfo::eCompleteness_no_right  },
    { "partial",   CMolInfo::eCompleteness_partial  }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _MOD_TO_ENUM_HPP_
