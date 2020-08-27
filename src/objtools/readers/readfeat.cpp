 /*
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
 * Author:  Jonathan Kans, Michael Kornbluh
 *
 * File Description:
 *   Feature table reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>

#include <util/static_map.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/seq_loc_from_string.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/RNA_qual_set.hpp>
#include <objects/seqfeat/RNA_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/table_filter.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>
#include <unordered_set>

#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/read_util.hpp>
#include "best_feat_finder.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_Feature


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::



namespace {
    static const char * const kCdsFeatName = "CDS";
    // priorities, inherited from C toolkit
    static Uchar std_order[CSeq_id::e_MaxChoice] = {
        83,  /* 0 = not set */
        80,  /* 1 = local Object-id */
        70,  /* 2 = gibbsq */
        70,  /* 3 = gibbmt */
        70,  /* 4 = giim Giimport-id */
        60,  /* 5 = genbank */
        60,  /* 6 = embl */
        60,  /* 7 = pir */
        60,  /* 8 = swissprot */
        81,  /* 9 = patent */
        65,  /* 10 = other TextSeqId */
        80,  /* 11 = general Dbtag */
        82,  /* 12 = gi */
        60,  /* 13 = ddbj */
        60,  /* 14 = prf */
        60,  /* 15 = pdb */
        60,  /* 16 = tpg */
        60,  /* 17 = tpe */
        60,  /* 18 = tpd */
        68,  /* 19 = gpp */
        69   /* 20 = nat */
    };

CRef<CSeq_id> GetBestId(const CBioseq::TId& ids)
{
    if (ids.size() == 1)
        return ids.front();

    CRef<CSeq_id> id;
    if (!ids.empty())
    {
        Uchar best_weight = UCHAR_MAX;
        ITERATE(CBioseq::TId, it, ids)
        {
            Uchar new_weight = std_order[(*it)->Which()];
            if (new_weight < best_weight)
            {
                id = *it;
                best_weight = new_weight;
            }
        };
    }

    return id;
}


map<char, list<char>> s_IUPACmap 
{
    {'A', list<char>({'A'})},
    {'G', list<char>({'G'})},
    {'C', list<char>({'C'})},
    {'T', list<char>({'T'})},
    {'U', list<char>({'U'})},
    {'M', list<char>({'A', 'C'})},
    {'R', list<char>({'A', 'G'})},
    {'W', list<char>({'A', 'T'})},
    {'S', list<char>({'C', 'G'})},
    {'Y', list<char>({'C', 'T'})},
    {'K', list<char>({'G', 'T'})},
    {'V', list<char>({'A', 'C', 'G'})},
    {'H', list<char>({'A', 'C', 'T'})},
    {'D', list<char>({'A', 'G', 'T'})},
    {'B', list<char>({'C', 'G', 'T'})},
    {'N', list<char>({'A', 'C', 'G', 'T'})}
};

}


class /* NCBI_XOBJREAD_EXPORT */ CFeatureTableReader_Imp
{
public:
    enum EQual {
        eQual_allele,
        eQual_anticodon,
        eQual_bac_ends,
        eQual_bond_type,
        eQual_bound_moiety,
        eQual_chrcnt,
        eQual_citation,
        eQual_clone,
        eQual_clone_id,
        eQual_codon_recognized,
        eQual_codon_start,
        eQual_compare,
        eQual_cons_splice,
        eQual_ctgcnt,
        eQual_cyt_map,
        eQual_db_xref,
        eQual_direction,
        eQual_EC_number,
        eQual_estimated_length,
        eQual_evidence,
        eQual_exception,
        eQual_experiment,
        eQual_frequency,
        eQual_function,
        eQual_gap_type,
        eQual_gen_map,
        eQual_gene,
        eQual_gene_desc,
        eQual_gene_syn,
        eQual_go_component,
        eQual_go_function,
        eQual_go_process,
        eQual_heterogen,
        eQual_inference,
        eQual_insertion_seq,
        eQual_label,
        eQual_linkage_evidence,
        eQual_loccnt,
        eQual_locus_tag,
        eQual_macronuclear,
        eQual_map,
        eQual_MEDLINE,
        eQual_method,
        eQual_mobile_element_type,
        eQual_mod_base,
        eQual_muid,
        eQual_ncRNA_class,
        eQual_nomenclature,
        eQual_note,
        eQual_number,
        eQual_old_locus_tag,
        eQual_operon,
        eQual_organism,
        eQual_partial,
        eQual_PCR_conditions,
        eQual_phenotype,
        eQual_pmid,
        eQual_product,
        eQual_prot_desc,
        eQual_prot_note,
        eQual_protein_id,
        eQual_pseudo,
        eQual_pseudogene,
        eQual_PubMed,
        eQual_rad_map,
        eQual_region_name,
        eQual_regulatory_class,
        eQual_replace,
        eQual_ribosomal_slippage,
        eQual_rpt_family,
        eQual_rpt_type,
        eQual_rpt_unit,
        eQual_rpt_unit_range,
        eQual_rpt_unit_seq,
        eQual_satellite,
        eQual_sec_str_type,
        eQual_secondary_accession,
        eQual_sequence,
        eQual_site_type,
        eQual_snp_class,
        eQual_snp_gtype,
        eQual_snp_het,
        eQual_snp_het_se,
        eQual_snp_linkout,
        eQual_snp_maxrate,
        eQual_snp_valid,
        eQual_standard_name,
        eQual_STS,
        eQual_sts_aliases,
        eQual_sts_dsegs,
        eQual_tag_peptide,
        eQual_trans_splicing,
        eQual_transcript_id,
        eQual_transcription,
        eQual_transl_except,
        eQual_transl_table,
        eQual_translation,
        eQual_transposon,
        eQual_usedin,
        eQual_weight
    };

    enum EOrgRef {
        eOrgRef_organism,
        eOrgRef_organelle,
        eOrgRef_div,
        eOrgRef_lineage,
        eOrgRef_gcode,
        eOrgRef_mgcode
    };

    using TFlags = CFeature_table_reader::TFlags;
    using TFtable = CSeq_annot::C_Data::TFtable;

    // constructor
    CFeatureTableReader_Imp(ILineReader* reader, unsigned int line_num, ILineErrorListener* pMessageListener);
    // destructor
    ~CFeatureTableReader_Imp(void);

    // read 5-column feature table and return Seq-annot
    CRef<CSeq_annot> ReadSequinFeatureTable (const CTempString& seqid,
                                             const CTempString& annotname,
                                             const TFlags flags, 
                                             ITableFilter *filter);

    // create single feature from key
    CRef<CSeq_feat> CreateSeqFeat (const string& feat,
                                   CSeq_loc& location,
                                   const TFlags flags, 
                                   const string &seq_id,
                                   ITableFilter *filter);

    // add single qualifier to feature
    void AddFeatQual (CRef<CSeq_feat> sfp,
                      const string& feat_name,
                      const string& qual,
                      const string& val,
                      const TFlags flags,
                      const string &seq_id );

    static bool ParseInitialFeatureLine (
        const CTempString& line_arg,
        CTempStringEx& out_seqid,
        CTempStringEx& out_annotname );

    static void PutProgress(const CTempString& seq_id,
        const unsigned int line_number,
        ILineErrorListener* pListener);

    ILineReader* const GetLineReaderPtr(void)  {
        return m_reader;
    }

    ILineErrorListener* const GetErrorListenerPtr(void) {
        return m_pMessageListener;
    }

private:

    // Prohibit copy constructor and assignment operator
    CFeatureTableReader_Imp(const CFeatureTableReader_Imp& value);
    CFeatureTableReader_Imp& operator=(const CFeatureTableReader_Imp& value);

    void x_InitId(const CTempString& seq_id, const TFlags flags);
    // returns true if parsed (otherwise, out_offset is left unchanged)
    bool x_TryToParseOffset(const CTempString & sLine, Int4 & out_offset );


    struct SFeatLocInfo {
        Int4 start_pos;
        Int4 stop_pos;
        bool is_5p_partial;
        bool is_3p_partial;
        bool is_point;
        bool is_minus_strand;
    };


    bool x_ParseFeatureTableLine(
            const CTempString& line,
            SFeatLocInfo& loc_info,
            string& feat,
            string& qual,
            string& val,
            Int4 offset); 


    bool x_IsWebComment(CTempString line);

    bool x_AddIntervalToFeature (
            CTempString strFeatureName, 
            CRef<CSeq_feat>& sfp,
            const SFeatLocInfo& loc_info);

    bool x_AddQualifierToFeature (CRef<CSeq_feat> sfp,
        const string &feat_name,
        const string& qual, const string& val,
        const TFlags flags);

    void x_ProcessQualifier(const string& qual_name,
                            const string& qual_val,
                            const string& feat_name,
                            CRef<CSeq_feat> feat,
                            TFlags flags);

    bool x_AddQualifierToGene     (CSeqFeatData& sfdata,
                                   EQual qtype, const string& val);
    bool x_AddQualifierToCdregion (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                   EQual qtype, const string& val);
    bool x_AddQualifierToRna      (CRef<CSeq_feat> sfp,
                                   EQual qtype, const string& val);
    bool x_AddQualifierToImp      (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                   EQual qtype, const string& qual, const string& val);
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   const string &feat_name,
                                   EOrgRef rtype, const string& val);
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   CSubSource::ESubtype stype, const string& val);
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   COrgMod::ESubtype mtype, const string& val);

    bool x_AddNoteToFeature(CRef<CSeq_feat> sfp, const string& note);

    bool x_AddNoteToFeature(CRef<CSeq_feat> sfp,
            const string& feat_name,
            const string& qual,
            const string& val);

    bool x_AddGBQualToFeature    (CRef<CSeq_feat> sfp,
                                  const string& qual, const string& val);

    bool x_AddCodons(const string& val, CTrna_ext& trna_ext) const;

    typedef CConstRef<CSeq_feat> TFeatConstRef;
    struct SFeatAndLineNum {
        SFeatAndLineNum(
            TFeatConstRef pFeat,
            TSeqPos       uLineNum ) :
        m_pFeat(pFeat), m_uLineNum(uLineNum) { 
            _ASSERT(pFeat);
        }

        bool operator==(const SFeatAndLineNum & rhs) const { 
            return Compare(rhs) == 0; }
        bool operator!=(const SFeatAndLineNum & rhs) const {
            return Compare(rhs) != 0; }
        bool operator<(const SFeatAndLineNum & rhs) const {
            return Compare(rhs) < 0; }

        int Compare(const SFeatAndLineNum & rhs) const {
            if( m_uLineNum != rhs.m_uLineNum ) {
                return ( m_uLineNum < rhs.m_uLineNum ? -1 : 1 );
            }
            return (m_pFeat.GetPointerOrNull() < rhs.m_pFeat.GetPointerOrNull() ? -1 : 1 );
        }

        TFeatConstRef m_pFeat; // must be non-NULL
        TSeqPos       m_uLineNum; // the line where this feature was created (or zero if programmatically created)
    };
    typedef multimap<CSeqFeatData::E_Choice, SFeatAndLineNum> TChoiceToFeatMap;
    void x_CreateGenesFromCDSs(
        CRef<CSeq_annot> sap,
        TChoiceToFeatMap & choiceToFeatMap, // an input param, but might get more items added
        const TFlags flags);

    bool x_StringIsJustQuotes (const string& str);

    string x_TrnaToAaString(const string& val);

    bool x_ParseTrnaExtString(CTrna_ext & ext_trna, const string & str);
    SIZE_TYPE x_MatchingParenPos( const string &str, SIZE_TYPE open_paren_pos );

    long x_StringToLongNoThrow (
        CTempString strToConvert,
        CTempString strFeatureName,
        CTempString strQualifierName,
        // user can override the default problem types that are set on error
        ILineError::EProblem eProblem = ILineError::eProblem_Unset
    );

    bool x_SetupSeqFeat (CRef<CSeq_feat> sfp, const string& feat,
                         const TFlags flags, 
                         ITableFilter *filter);

    void  x_ProcessMsg (
        ILineError::EProblem eProblem,
        EDiagSev eSeverity,
        const std::string & strFeatureName = kEmptyStr,
        const std::string & strQualifierName = kEmptyStr,
        const std::string & strQualifierValue = kEmptyStr,
        const std::string & strErrorMessage = kEmptyStr,
        const ILineError::TVecOfLines & vecOfOtherLines =
            ILineError::TVecOfLines() );

    void  x_ProcessMsg(
        int line_num,
        ILineError::EProblem eProblem,
        EDiagSev eSeverity,
        const std::string & strFeatureName = kEmptyStr,
        const std::string & strQualifierName = kEmptyStr,
        const std::string & strQualifierValue = kEmptyStr,
        const std::string & strErrorMessage = kEmptyStr,
        const ILineError::TVecOfLines & vecOfOtherLines =
        ILineError::TVecOfLines());

    void x_TokenizeStrict( const CTempString &line, vector<string> &out_tokens );
    void x_TokenizeLenient( const CTempString &line, vector<string> &out_tokens );
    void x_FinishFeature(CRef<CSeq_feat>& feat, TFtable& ftable);
    void x_ResetFeat(CRef<CSeq_feat>& feat, bool & curr_feat_intervals_done);
    void x_UpdatePointStrand(CSeq_feat& feat, CSeq_interval::TStrand strand) const;
    void x_GetPointStrand(const CSeq_feat& feat, CSeq_interval::TStrand& strand) const;

    bool m_need_check_strand;
    string m_real_seqid;
    CRef<CSeq_id> m_seq_id;
    ILineReader* m_reader;
    unsigned int m_LineNumber;
    ILineErrorListener* m_pMessageListener;
    unordered_set<string> m_ProcessedTranscriptIds;
    unordered_set<string> m_ProcessedProteinIds;
};


typedef SStaticPair<const char *, CFeatureTableReader_Imp::EQual> TQualKey;

static const TQualKey qual_key_to_subtype [] = {
    {  "EC_number",            CFeatureTableReader_Imp::eQual_EC_number             },
    {  "PCR_conditions",       CFeatureTableReader_Imp::eQual_PCR_conditions        },
    {  "PubMed",               CFeatureTableReader_Imp::eQual_PubMed                },
    {  "STS",                  CFeatureTableReader_Imp::eQual_STS                   },
    {  "allele",               CFeatureTableReader_Imp::eQual_allele                },
    {  "anticodon",            CFeatureTableReader_Imp::eQual_anticodon             },
    {  "bac_ends",             CFeatureTableReader_Imp::eQual_bac_ends              },
    {  "bond_type",            CFeatureTableReader_Imp::eQual_bond_type             },
    {  "bound_moiety",         CFeatureTableReader_Imp::eQual_bound_moiety          },
    {  "chrcnt",               CFeatureTableReader_Imp::eQual_chrcnt                },
    {  "citation",             CFeatureTableReader_Imp::eQual_citation              },
    {  "clone",                CFeatureTableReader_Imp::eQual_clone                 },
    {  "clone_id",             CFeatureTableReader_Imp::eQual_clone_id              },
    {  "codon_recognized",     CFeatureTableReader_Imp::eQual_codon_recognized      },
    {  "codon_start",          CFeatureTableReader_Imp::eQual_codon_start           },
    {  "codons_recognized",    CFeatureTableReader_Imp::eQual_codon_recognized      },
    {  "compare",              CFeatureTableReader_Imp::eQual_compare               },
    {  "cons_splice",          CFeatureTableReader_Imp::eQual_cons_splice           },
    {  "ctgcnt",               CFeatureTableReader_Imp::eQual_ctgcnt                },
    {  "cyt_map",              CFeatureTableReader_Imp::eQual_cyt_map               },
    {  "db_xref",              CFeatureTableReader_Imp::eQual_db_xref               },
    {  "direction",            CFeatureTableReader_Imp::eQual_direction             },
    {  "estimated_length",     CFeatureTableReader_Imp::eQual_estimated_length      },
    {  "evidence",             CFeatureTableReader_Imp::eQual_evidence              },
    {  "exception",            CFeatureTableReader_Imp::eQual_exception             },
    {  "experiment",           CFeatureTableReader_Imp::eQual_experiment            },
    {  "frequency",            CFeatureTableReader_Imp::eQual_frequency             },
    {  "function",             CFeatureTableReader_Imp::eQual_function              },
    {  "gap_type",             CFeatureTableReader_Imp::eQual_gap_type              },
    {  "gen_map",              CFeatureTableReader_Imp::eQual_gen_map               },
    {  "gene",                 CFeatureTableReader_Imp::eQual_gene                  },
    {  "gene_desc",            CFeatureTableReader_Imp::eQual_gene_desc             },
    {  "gene_syn",             CFeatureTableReader_Imp::eQual_gene_syn              },
    {  "gene_synonym",         CFeatureTableReader_Imp::eQual_gene_syn              },
    {  "go_component",         CFeatureTableReader_Imp::eQual_go_component          },
    {  "go_function",          CFeatureTableReader_Imp::eQual_go_function           },
    {  "go_process",           CFeatureTableReader_Imp::eQual_go_process            },
    {  "heterogen",            CFeatureTableReader_Imp::eQual_heterogen             },
    {  "inference",            CFeatureTableReader_Imp::eQual_inference             },
    {  "insertion_seq",        CFeatureTableReader_Imp::eQual_insertion_seq         },
    {  "label",                CFeatureTableReader_Imp::eQual_label                 },
    {  "linkage_evidence",     CFeatureTableReader_Imp::eQual_linkage_evidence      },
    {  "loccnt",               CFeatureTableReader_Imp::eQual_loccnt                },
    {  "locus_tag",            CFeatureTableReader_Imp::eQual_locus_tag             },
    {  "macronuclear",         CFeatureTableReader_Imp::eQual_macronuclear          },
    {  "map",                  CFeatureTableReader_Imp::eQual_map                   },
    {  "method",               CFeatureTableReader_Imp::eQual_method                },
    {  "mobile_element_type",  CFeatureTableReader_Imp::eQual_mobile_element_type   },
    {  "mod_base",             CFeatureTableReader_Imp::eQual_mod_base              },
    {  "ncRNA_class",          CFeatureTableReader_Imp::eQual_ncRNA_class           },
    {  "nomenclature",         CFeatureTableReader_Imp::eQual_nomenclature          },
    {  "note",                 CFeatureTableReader_Imp::eQual_note                  },
    {  "number",               CFeatureTableReader_Imp::eQual_number                },
    {  "old_locus_tag",        CFeatureTableReader_Imp::eQual_old_locus_tag         },
    {  "operon",               CFeatureTableReader_Imp::eQual_operon                },
    {  "organism",             CFeatureTableReader_Imp::eQual_organism              },
    {  "partial",              CFeatureTableReader_Imp::eQual_partial               },
    {  "phenotype",            CFeatureTableReader_Imp::eQual_phenotype             },
    {  "product",              CFeatureTableReader_Imp::eQual_product               },
    {  "prot_desc",            CFeatureTableReader_Imp::eQual_prot_desc             },
    {  "prot_note",            CFeatureTableReader_Imp::eQual_prot_note             },
    {  "protein_id",           CFeatureTableReader_Imp::eQual_protein_id            },
    {  "pseudo",               CFeatureTableReader_Imp::eQual_pseudo                },
    {  "pseudogene",           CFeatureTableReader_Imp::eQual_pseudogene            },
    {  "rad_map",              CFeatureTableReader_Imp::eQual_rad_map               },
    {  "region_name",          CFeatureTableReader_Imp::eQual_region_name           },
    {  "regulatory_class",     CFeatureTableReader_Imp::eQual_regulatory_class      },
    {  "replace",              CFeatureTableReader_Imp::eQual_replace               },
    {  "ribosomal_slippage",   CFeatureTableReader_Imp::eQual_ribosomal_slippage    },
    {  "rpt_family",           CFeatureTableReader_Imp::eQual_rpt_family            },
    {  "rpt_type",             CFeatureTableReader_Imp::eQual_rpt_type              },
    {  "rpt_unit",             CFeatureTableReader_Imp::eQual_rpt_unit              },
    {  "rpt_unit_range",       CFeatureTableReader_Imp::eQual_rpt_unit_range        },
    {  "rpt_unit_seq",         CFeatureTableReader_Imp::eQual_rpt_unit_seq          },
    {  "satellite",            CFeatureTableReader_Imp::eQual_satellite             },
    {  "sec_str_type",         CFeatureTableReader_Imp::eQual_sec_str_type          },
    {  "secondary_accession",  CFeatureTableReader_Imp::eQual_secondary_accession   },
    {  "secondary_accessions", CFeatureTableReader_Imp::eQual_secondary_accession   },
    {  "sequence",             CFeatureTableReader_Imp::eQual_sequence              },
    {  "site_type",            CFeatureTableReader_Imp::eQual_site_type             },
    {  "snp_class",            CFeatureTableReader_Imp::eQual_snp_class             },
    {  "snp_gtype",            CFeatureTableReader_Imp::eQual_snp_gtype             },
    {  "snp_het",              CFeatureTableReader_Imp::eQual_snp_het               },
    {  "snp_het_se",           CFeatureTableReader_Imp::eQual_snp_het_se            },
    {  "snp_linkout",          CFeatureTableReader_Imp::eQual_snp_linkout           },
    {  "snp_maxrate",          CFeatureTableReader_Imp::eQual_snp_maxrate           },
    {  "snp_valid",            CFeatureTableReader_Imp::eQual_snp_valid             },
    {  "standard_name",        CFeatureTableReader_Imp::eQual_standard_name         },
    {  "sts_aliases",          CFeatureTableReader_Imp::eQual_sts_aliases           },
    {  "sts_dsegs",            CFeatureTableReader_Imp::eQual_sts_dsegs             },
    {  "tag_peptide",          CFeatureTableReader_Imp::eQual_tag_peptide           },
    {  "trans_splicing",       CFeatureTableReader_Imp::eQual_trans_splicing        },
    {  "transcript_id",        CFeatureTableReader_Imp::eQual_transcript_id         },
    {  "transcription",        CFeatureTableReader_Imp::eQual_transcription         },
    {  "transl_except",        CFeatureTableReader_Imp::eQual_transl_except         },
    {  "transl_table",         CFeatureTableReader_Imp::eQual_transl_table          },
    {  "translation",          CFeatureTableReader_Imp::eQual_translation           },
    {  "transposon",           CFeatureTableReader_Imp::eQual_transposon            },
    {  "usedin",               CFeatureTableReader_Imp::eQual_usedin                },
    {  "weight",               CFeatureTableReader_Imp::eQual_weight                }
};

typedef CStaticPairArrayMap <const char*, CFeatureTableReader_Imp::EQual, PCase_CStr> TQualMap;
DEFINE_STATIC_ARRAY_MAP(TQualMap, sm_QualKeys, qual_key_to_subtype);


typedef SStaticPair<const char *, CFeatureTableReader_Imp::EOrgRef> TOrgRefKey;

static const TOrgRefKey orgref_key_to_subtype [] = {
    {  "div",        CFeatureTableReader_Imp::eOrgRef_div        },
    {  "gcode",      CFeatureTableReader_Imp::eOrgRef_gcode      },
    {  "lineage",    CFeatureTableReader_Imp::eOrgRef_lineage    },
    {  "mgcode",     CFeatureTableReader_Imp::eOrgRef_mgcode     },
    {  "organelle",  CFeatureTableReader_Imp::eOrgRef_organelle  },
    {  "organism",   CFeatureTableReader_Imp::eOrgRef_organism   }
};

typedef CStaticPairArrayMap <const char*, CFeatureTableReader_Imp::EOrgRef, PCase_CStr> TOrgRefMap;
DEFINE_STATIC_ARRAY_MAP(TOrgRefMap, sm_OrgRefKeys, orgref_key_to_subtype);


typedef SStaticPair<const char *, CBioSource::EGenome> TGenomeKey;

static const TGenomeKey genome_key_to_subtype [] = {
    {  "apicoplast",                CBioSource::eGenome_apicoplast        },
    {  "chloroplast",               CBioSource::eGenome_chloroplast       },
    {  "chromatophore",             CBioSource::eGenome_chromatophore     },
    {  "chromoplast",               CBioSource::eGenome_chromoplast       },
    {  "chromosome",                CBioSource::eGenome_chromosome        },
    {  "cyanelle",                  CBioSource::eGenome_cyanelle          },
    {  "endogenous_virus",          CBioSource::eGenome_endogenous_virus  },
    {  "extrachrom",                CBioSource::eGenome_extrachrom        },
    {  "genomic",                   CBioSource::eGenome_genomic           },
    {  "hydrogenosome",             CBioSource::eGenome_hydrogenosome     },
    {  "insertion_seq",             CBioSource::eGenome_insertion_seq     },
    {  "kinetoplast",               CBioSource::eGenome_kinetoplast       },
    {  "leucoplast",                CBioSource::eGenome_leucoplast        },
    {  "macronuclear",              CBioSource::eGenome_macronuclear      },
    {  "mitochondrion",             CBioSource::eGenome_mitochondrion     },
    {  "mitochondrion:kinetoplast", CBioSource::eGenome_kinetoplast       },
    {  "nucleomorph",               CBioSource::eGenome_nucleomorph       },
    {  "plasmid",                   CBioSource::eGenome_plasmid           },
    {  "plastid",                   CBioSource::eGenome_plastid           },
    {  "plastid:apicoplast",        CBioSource::eGenome_apicoplast        },
    {  "plastid:chloroplast",       CBioSource::eGenome_chloroplast       },
    {  "plastid:chromoplast",       CBioSource::eGenome_chromoplast       },
    {  "plastid:cyanelle",          CBioSource::eGenome_cyanelle          },
    {  "plastid:leucoplast",        CBioSource::eGenome_leucoplast        },
    {  "plastid:proplastid",        CBioSource::eGenome_proplastid        },
    {  "proplastid",                CBioSource::eGenome_proplastid        },
    {  "proviral",                  CBioSource::eGenome_proviral          },
    {  "transposon",                CBioSource::eGenome_transposon        },
    {  "unknown",                   CBioSource::eGenome_unknown           },
    {  "virion",                    CBioSource::eGenome_virion            }
};

typedef CStaticPairArrayMap <const char*, CBioSource::EGenome, PCase_CStr> TGenomeMap;
DEFINE_STATIC_ARRAY_MAP(TGenomeMap, sm_GenomeKeys, genome_key_to_subtype);


typedef SStaticPair<const char *, CSubSource::ESubtype> TSubSrcKey;

static const TSubSrcKey subsrc_key_to_subtype [] = {
    {  "altitude",             CSubSource::eSubtype_altitude               },
    {  "cell_line",            CSubSource::eSubtype_cell_line              },
    {  "cell_type",            CSubSource::eSubtype_cell_type              },
    {  "chromosome",           CSubSource::eSubtype_chromosome             },
    {  "clone",                CSubSource::eSubtype_clone                  },
    {  "clone_lib",            CSubSource::eSubtype_clone_lib              },
    {  "collected_by",         CSubSource::eSubtype_collected_by           },
    {  "collection_date",      CSubSource::eSubtype_collection_date        },
    {  "country",              CSubSource::eSubtype_country                },
    {  "dev_stage",            CSubSource::eSubtype_dev_stage              },
    {  "endogenous_virus",     CSubSource::eSubtype_endogenous_virus_name  },
    {  "environmental_sample", CSubSource::eSubtype_environmental_sample   },
    {  "frequency",            CSubSource::eSubtype_frequency              },
    {  "fwd_primer_name",      CSubSource::eSubtype_fwd_primer_name        },
    {  "fwd_primer_seq",       CSubSource::eSubtype_fwd_primer_seq         },
    {  "genotype",             CSubSource::eSubtype_genotype               },
    {  "germline",             CSubSource::eSubtype_germline               },
    {  "haplotype",            CSubSource::eSubtype_haplotype              },
    {  "identified_by",        CSubSource::eSubtype_identified_by          },
    {  "insertion_seq",        CSubSource::eSubtype_insertion_seq_name     },
    {  "isolation_source",     CSubSource::eSubtype_isolation_source       },
    {  "lab_host",             CSubSource::eSubtype_lab_host               },
    {  "lat_lon",              CSubSource::eSubtype_lat_lon                },
    {  "map",                  CSubSource::eSubtype_map                    },
    {  "metagenomic",          CSubSource::eSubtype_metagenomic            },
    {  "plasmid",              CSubSource::eSubtype_plasmid_name           },
    {  "plastid",              CSubSource::eSubtype_plastid_name           },
    {  "pop_variant",          CSubSource::eSubtype_pop_variant            },
    {  "rearranged",           CSubSource::eSubtype_rearranged             },
    {  "rev_primer_name",      CSubSource::eSubtype_rev_primer_name        },
    {  "rev_primer_seq",       CSubSource::eSubtype_rev_primer_seq         },
    {  "segment",              CSubSource::eSubtype_segment                },
    {  "sex",                  CSubSource::eSubtype_sex                    },
    {  "subclone",             CSubSource::eSubtype_subclone               },
    {  "tissue_lib ",          CSubSource::eSubtype_tissue_lib             },
    {  "tissue_type",          CSubSource::eSubtype_tissue_type            },
    {  "transgenic",           CSubSource::eSubtype_transgenic             },
    {  "transposon",           CSubSource::eSubtype_transposon_name        }
};

typedef CStaticPairArrayMap <const char*, CSubSource::ESubtype, PCase_CStr> TSubSrcMap;
DEFINE_STATIC_ARRAY_MAP(TSubSrcMap, sm_SubSrcKeys, subsrc_key_to_subtype);

// case-insensitive version of sm_SubSrcKeys
typedef CStaticPairArrayMap <const char*, CSubSource::ESubtype, PNocase_CStr> TSubSrcNoCaseMap;
DEFINE_STATIC_ARRAY_MAP(
    TSubSrcNoCaseMap, sm_SubSrcNoCaseKeys, subsrc_key_to_subtype);

typedef SStaticPair<const char *, COrgMod::ESubtype> TOrgModKey;

static const TOrgModKey orgmod_key_to_subtype [] = {
    {  "acronym",            COrgMod::eSubtype_acronym             },
    {  "anamorph",           COrgMod::eSubtype_anamorph            },
    {  "authority",          COrgMod::eSubtype_authority           },
    {  "bio_material",       COrgMod::eSubtype_bio_material        },
    {  "biotype",            COrgMod::eSubtype_biotype             },
    {  "biovar",             COrgMod::eSubtype_biovar              },
    {  "breed",              COrgMod::eSubtype_breed               },
    {  "chemovar",           COrgMod::eSubtype_chemovar            },
    {  "common",             COrgMod::eSubtype_common              },
    {  "cultivar",           COrgMod::eSubtype_cultivar            },
    {  "culture_collection", COrgMod::eSubtype_culture_collection  },
    {  "dosage",             COrgMod::eSubtype_dosage              },
    {  "ecotype",            COrgMod::eSubtype_ecotype             },
    {  "forma",              COrgMod::eSubtype_forma               },
    {  "forma_specialis",    COrgMod::eSubtype_forma_specialis     },
    {  "gb_acronym",         COrgMod::eSubtype_gb_acronym          },
    {  "gb_anamorph",        COrgMod::eSubtype_gb_anamorph         },
    {  "gb_synonym",         COrgMod::eSubtype_gb_synonym          },
    {  "group",              COrgMod::eSubtype_group               },
    {  "isolate",            COrgMod::eSubtype_isolate             },
    {  "metagenome_source",  COrgMod::eSubtype_metagenome_source   },
    {  "nat_host",           COrgMod::eSubtype_nat_host            },
    {  "natural_host",       COrgMod::eSubtype_nat_host            },
    {  "old_lineage",        COrgMod::eSubtype_old_lineage         },
    {  "old_name",           COrgMod::eSubtype_old_name            },
    {  "pathovar",           COrgMod::eSubtype_pathovar            },
    {  "serogroup",          COrgMod::eSubtype_serogroup           },
    {  "serotype",           COrgMod::eSubtype_serotype            },
    {  "serovar",            COrgMod::eSubtype_serovar             },
    {  "spec_host",          COrgMod::eSubtype_nat_host            },
    {  "specific_host",      COrgMod::eSubtype_nat_host            },
    {  "specimen_voucher",   COrgMod::eSubtype_specimen_voucher    },
    {  "strain",             COrgMod::eSubtype_strain              },
    {  "sub_species",        COrgMod::eSubtype_sub_species         },
    {  "subgroup",           COrgMod::eSubtype_subgroup            },
    {  "substrain",          COrgMod::eSubtype_substrain           },
    {  "subtype",            COrgMod::eSubtype_subtype             },
    {  "synonym",            COrgMod::eSubtype_synonym             },
    {  "teleomorph",         COrgMod::eSubtype_teleomorph          },
    {  "type",               COrgMod::eSubtype_type                },
    {  "type_material",      COrgMod::eSubtype_type_material       },
    {  "variety",            COrgMod::eSubtype_variety             }
};

typedef CStaticPairArrayMap <const char*, COrgMod::ESubtype, PCase_CStr> TOrgModMap;
DEFINE_STATIC_ARRAY_MAP(TOrgModMap, sm_OrgModKeys, orgmod_key_to_subtype);

static const map<const char*, int, PNocase_CStr> sm_TrnaKeys 
{
    {  "Ala",            'A'  },
    {  "Alanine",        'A'  },
    {  "Arg",            'R'  },
    {  "Arginine",       'R'  },
    {  "Asn",            'N'  },
    {  "Asp",            'D'  },
    {  "Asp or Asn",     'B'  },
    {  "Asparagine",     'N'  },
    {  "Aspartate",      'D'  },
    {  "Aspartic Acid",  'D'  },
    {  "Asx",            'B'  },
    {  "Cys",            'C'  },
    {  "Cysteine",       'C'  },
    {  "Gln",            'Q'  },
    {  "Glu",            'E'  },
    {  "Glu or Gln",     'Z'  },
    {  "Glutamate",      'E'  },
    {  "Glutamic Acid",  'E'  },
    {  "Glutamine",      'Q'  },
    {  "Glx",            'Z'  },
    {  "Gly",            'G'  },
    {  "Glycine",        'G'  },
    {  "His",            'H'  },
    {  "Histidine",      'H'  },
    {  "Ile",            'I'  },
    {  "Ile2",           'I'  },
    {  "Isoleucine",     'I'  },
    {  "Leu",            'L'  },
    {  "Leu or Ile",     'J'  },
    {  "Leucine",        'L'  },
    {  "Lys",            'K'  },
    {  "Lysine",         'K'  },
    {  "Met",            'M'  },
    {  "Methionine",     'M'  },
    {  "OTHER",          'X'  },
    {  "Phe",            'F'  },
    {  "Phenylalanine",  'F'  },
    {  "Pro",            'P'  },
    {  "Proline",        'P'  },
    {  "Pyl",            'O'  },
    {  "Pyrrolysine",    'O'  },
    {  "Sec",            'U'  },
    {  "Selenocysteine", 'U'  },
    {  "Ser",            'S'  },
    {  "Serine",         'S'  },
    {  "TERM",           '*'  },
    {  "Ter",            '*'  },
    {  "Termination",    '*'  },
    {  "Thr",            'T'  },
    {  "Threonine",      'T'  },
    {  "Trp",            'W'  },
    {  "Tryptophan",     'W'  },
    {  "Tyr",            'Y'  },
    {  "Tyrosine",       'Y'  },
    {  "Val",            'V'  },
    {  "Valine",         'V'  },
    {  "Xle",            'J'  },
    {  "Xxx",            'X'  },
    {  "Undet",          'X'  },
    {  "fMet",           'M'  },
    {  "iMet",           'M'  }
};


static 
set<const char*, PCase_CStr> 
sc_SingleKeys {
    "environmental_sample", 
    "germline",
    "metagenomic",
    "partial",
    "pseudo",
    "rearranged",
    "ribosomal_slippage",
    "trans_splicing",
    "transgenic",
    "replace" // RW-882
};

// constructor
CFeatureTableReader_Imp::CFeatureTableReader_Imp(ILineReader* reader, unsigned int line_num, ILineErrorListener* pMessageListener)
    : m_reader(reader), m_LineNumber(line_num), m_pMessageListener(pMessageListener)
{
}

// destructor
CFeatureTableReader_Imp::~CFeatureTableReader_Imp(void)
{
}

bool CFeatureTableReader_Imp::x_TryToParseOffset(
    const CTempString & sLine, Int4 & out_offset )
{
    // offset strings are of the form [offset=SOME_NUMBER], but here we try 
    // to be as forgiving of whitespace as possible.

    CTempString sKey;
    CTempString sValue;
    if( ! NStr::SplitInTwo(sLine, "=", sKey, sValue) ) {
        // "=" not found
        return false;
    }


    
    // check key
    NStr::TruncateSpacesInPlace(sKey);
    if( NStr::StartsWith(sKey, "[") ) {
        sKey = sKey.substr(1); // remove initial "["
    }
    NStr::TruncateSpacesInPlace(sKey, NStr::eTrunc_Begin);
    if( ! NStr::EqualNocase(sKey, "offset") ) {
        // key is not offset
        return false;
    }

    // check value
    NStr::TruncateSpacesInPlace(sValue);
    if( ! NStr::EndsWith(sValue, "]") ) {
        // no closing bracket
        return false;
    }
    // remove closing bracket
    sValue = sValue.substr(0, (sValue.length() - 1) );
    NStr::TruncateSpacesInPlace(sValue, NStr::eTrunc_End);
    // is it a number?
    try {
        Int4 new_offset = NStr::StringToInt(sValue);
    //    if( new_offset < 0 ) {
    //        return false;
    //    }
        out_offset = new_offset;
        return true;
    } catch ( CStringException & ) {
        return false;
    }
}

bool CFeatureTableReader_Imp::x_ParseFeatureTableLine (
    const CTempString& line,
    SFeatLocInfo& loc_info,
    string& featP,
    string& qualP,
    string& valP,
    Int4 offset
)

{
    SIZE_TYPE      numtkns;
    bool           isminus = false;
    bool           ispoint = false;
    size_t         len;
    bool           partial5 = false;
    bool           partial3 = false;
    Int4           startv = -1;
    Int4           stopv = -1;
    Int4           swp;
    string         start, stop, feat, qual, val, stnd;
    vector<string> tkns;


    if (line.empty ()) return false;

    /* offset and other instructions encoded in brackets */
    if (NStr::StartsWith (line, '[')) return false;

    tkns.clear ();
    x_TokenizeLenient(line, tkns);
    numtkns = tkns.size ();

    if (numtkns > 0) {
        start = NStr::TruncateSpaces(tkns[0]);
    }
    if (numtkns > 1) {
        stop = NStr::TruncateSpaces(tkns[1]);
    }
    if (numtkns > 2) {
        feat = NStr::TruncateSpaces(tkns[2]);
    }
    if (numtkns > 3) {
        qual = NStr::TruncateSpaces(tkns[3]);
    }
    if (numtkns > 4) {
        val = NStr::TruncateSpaces(tkns[4]);
        // trim enclosing double-quotes
        if( val.length() >= 2 && val[0] == '"' && val[val.length()-1] == '"' ) {
            val = val.substr(1, val.length() - 2);
        }
    }
    if (numtkns > 5) {
        stnd = NStr::TruncateSpaces(tkns[5]);
    }

    bool has_start = false;
    if (! start.empty ()) {
        if (start [0] == '<') {
            partial5 = true;
            start.erase (0, 1);
        }
        len = start.length ();
        if (len > 1 && start [len - 1] == '^') {
          ispoint = true;
          start [len - 1] = '\0';
        }
        startv = x_StringToLongNoThrow(start, feat, qual,
            ILineError::eProblem_BadFeatureInterval);
        has_start = true;
    }

    bool has_stop = false;
    if (! stop.empty ()) {
        if (stop [0] == '>') {
            partial3 = true;
            stop.erase (0, 1);
        }
        stopv = x_StringToLongNoThrow (stop, feat, qual,
            ILineError::eProblem_BadFeatureInterval);
        has_stop = true;
    }
    
    if ( startv <= 0 || stopv <= 0 ) {
        startv = -1;
        stopv = -1;
    } else {
        startv--;
        stopv--;
        if (! stnd.empty ()) {
            if (stnd == "minus" || stnd == "-" || stnd == "complement") {
                if (start < stop) {
                    swp = startv;
                    startv = stopv;
                    stopv = swp;
                }
                isminus = true;
            }
        }
    }

    if (startv >= 0) {
        startv += offset;
    }
    if (stopv >= 0) {
        stopv += offset;
    }
    
    if ((has_start && startv < 0) || (has_stop && stopv < 0)) {
        x_ProcessMsg( 
            ILineError::eProblem_FeatureBadStartAndOrStop,
            eDiag_Error,
            feat);
    }

    loc_info.start_pos = ( startv < 0 ? -1 : startv);
    loc_info.stop_pos = ( stopv < 0 ? -1 : stopv);
    
    
    loc_info.is_5p_partial = partial5;
    loc_info.is_3p_partial = partial3;
    loc_info.is_point = ispoint;
    loc_info.is_minus_strand = isminus;
    featP = feat;
    qualP = qual;
    valP = val;

    return true;
}

/*
bool CFeatureTableReader_Imp::x_ParseFeatureTableLine (
    const CTempString& line,
    Int4* startP,
    Int4* stopP,
    bool* partial5P,
    bool* partial3P,
    bool* ispointP,
    bool* isminusP,
    string& featP,
    string& qualP,
    string& valP,
    Int4 offset
)

{
    SIZE_TYPE      numtkns;
    bool           isminus = false;
    bool           ispoint = false;
    size_t         len;
    bool           partial5 = false;
    bool           partial3 = false;
    Int4           startv = -1;
    Int4           stopv = -1;
    Int4           swp;
    string         start, stop, feat, qual, val, stnd;
    vector<string> tkns;


    if (line.empty ()) return false;

    if (NStr::StartsWith (line, '[')) return false;

    tkns.clear ();
    x_TokenizeLenient(line, tkns);
    numtkns = tkns.size ();

    if (numtkns > 0) {
        start = NStr::TruncateSpaces(tkns[0]);
    }
    if (numtkns > 1) {
        stop = NStr::TruncateSpaces(tkns[1]);
    }
    if (numtkns > 2) {
        feat = NStr::TruncateSpaces(tkns[2]);
    }
    if (numtkns > 3) {
        qual = NStr::TruncateSpaces(tkns[3]);
    }
    if (numtkns > 4) {
        val = NStr::TruncateSpaces(tkns[4]);
        // trim enclosing double-quotes
        if( val.length() >= 2 && val[0] == '"' && val[val.length()-1] == '"' ) {
            val = val.substr(1, val.length() - 2);
        }
    }
    if (numtkns > 5) {
        stnd = NStr::TruncateSpaces(tkns[5]);
    }

    bool has_start = false;
    if (! start.empty ()) {
        if (start [0] == '<') {
            partial5 = true;
            start.erase (0, 1);
        }
        len = start.length ();
        if (len > 1 && start [len - 1] == '^') {
          ispoint = true;
          start [len - 1] = '\0';
        }
        startv = x_StringToLongNoThrow(start, feat, qual,
            ILineError::eProblem_BadFeatureInterval);
        has_start = true;
    }

    bool has_stop = false;
    if (! stop.empty ()) {
        if (stop [0] == '>') {
            partial3 = true;
            stop.erase (0, 1);
        }
        stopv = x_StringToLongNoThrow (stop, feat, qual,
            ILineError::eProblem_BadFeatureInterval);
        has_stop = true;
    }
    
    if ( startv <= 0 || stopv <= 0 ) {
        startv = -1;
        stopv = -1;
    } else {
        startv--;
        stopv--;
        if (! stnd.empty ()) {
            if (stnd == "minus" || stnd == "-" || stnd == "complement") {
                if (start < stop) {
                    swp = startv;
                    startv = stopv;
                    stopv = swp;
                }
                isminus = true;
            }
        }
    }

    if (startv >= 0) {
        startv += offset;
    }
    if (stopv >= 0) {
        stopv += offset;
    }
    
    if ((has_start && startv < 0) || (has_stop && stopv < 0)) {
        x_ProcessMsg( 
            ILineError::eProblem_FeatureBadStartAndOrStop,
            eDiag_Error,
            feat);
    }

    *startP = ( startv < 0 ? -1 : startv);
    *stopP = ( stopv < 0 ? -1 : stopv);
    
    
    *partial5P = partial5;
    *partial3P = partial3;
    *ispointP = ispoint;
    *isminusP = isminus;
    featP = feat;
    qualP = qual;
    valP = val;

    return true;
}
*/

void CFeatureTableReader_Imp::x_TokenizeStrict( 
    const CTempString &line,
    vector<string> &out_tokens )
{
    out_tokens.clear();

    // each token has spaces before it and a tab or end-of-line after it
    string::size_type startPosOfNextRoundOfTokenization = 0;
    while ( startPosOfNextRoundOfTokenization < line.size() ) {
        auto posAfterSpaces = line.find_first_not_of( " ", startPosOfNextRoundOfTokenization );
        if( posAfterSpaces == string::npos ) {
            return;
        }

        string::size_type posOfTab = line.find( '\t', posAfterSpaces );
        if( posOfTab == string::npos ) {
            posOfTab = line.length();
        }

        // The next token is between the spaces and the tab (or end of string)
        out_tokens.push_back(kEmptyStr);
        string &new_token = out_tokens.back();
        copy( line.begin() + posAfterSpaces, line.begin() + posOfTab, back_inserter(new_token) );
        NStr::TruncateSpacesInPlace( new_token );

        startPosOfNextRoundOfTokenization = ( posOfTab + 1 );
    }
}

// since some compilers won't let me use isspace for find_if
class CIsSpace {
public:
    bool operator()( char c ) { return isspace(c); }
};

class CIsNotSpace {
public:
    bool operator()( char c ) { return ! isspace(c); }
};

void CFeatureTableReader_Imp::x_TokenizeLenient( 
    const CTempString &line,
    vector<string> &out_tokens )
{
    out_tokens.clear();

    if( line.empty() ) {
        return;
    }

    // if it starts with whitespace, it must be a qual line, else it's a feature line
    if( isspace(line[0]) ) {
        // In regex form, we're doing something like this:
        // \s+(\S+)(\s+(\S.*))?
        // Where the first is the qual, and the rest is the val
        auto start_of_qual = find_if( line.begin(), line.end(), CIsNotSpace() );
        if( start_of_qual == line.end() ) {
            return;
        }
        auto start_of_whitespace_after_qual = find_if( start_of_qual, line.end(), CIsSpace() );
        auto start_of_val = find_if( start_of_whitespace_after_qual, line.end(), CIsNotSpace() );

        // first 3 are empty
        out_tokens.push_back(kEmptyStr);
        out_tokens.push_back(kEmptyStr);
        out_tokens.push_back(kEmptyStr);

        // then qual
        out_tokens.push_back(kEmptyStr);
        string &qual = out_tokens.back();
        copy( start_of_qual, start_of_whitespace_after_qual, back_inserter(qual) );

        // then val
        if( start_of_val != line.end() ) {
            out_tokens.push_back(kEmptyStr);
            string &val = out_tokens.back();
            copy( start_of_val, line.end(), back_inserter(val) );
            NStr::TruncateSpacesInPlace( val );
        }

    } else {
        // parse a feature line

        // Since we're being lenient, we consider it to be 3 ( or 6 ) parts separated by whitespace
        auto first_column_start = line.begin();
        auto first_whitespace = find_if( first_column_start, line.end(), CIsSpace() );
        auto second_column_start = find_if( first_whitespace, line.end(), CIsNotSpace() );
        auto second_whitespace = find_if( second_column_start, line.end(), CIsSpace() );
        auto third_column_start = find_if( second_whitespace, line.end(), CIsNotSpace() );
        auto third_whitespace = find_if( third_column_start, line.end(), CIsSpace() );
        // columns 4 and 5 are unused on feature lines
        auto sixth_column_start = find_if( third_whitespace, line.end(), CIsNotSpace() );
        auto sixth_whitespace = find_if( sixth_column_start, line.end(), CIsSpace() );

        out_tokens.push_back(kEmptyStr);
        string &first = out_tokens.back();
        copy( first_column_start, first_whitespace, back_inserter(first) );

        out_tokens.push_back(kEmptyStr);
        string &second = out_tokens.back();
        copy( second_column_start, second_whitespace, back_inserter(second) );

        out_tokens.push_back(kEmptyStr);
        string &third = out_tokens.back();
        copy( third_column_start, third_whitespace, back_inserter(third) );

        if( sixth_column_start != line.end() ) {
            // columns 4 and 5 are unused
            out_tokens.push_back(kEmptyStr);
            out_tokens.push_back(kEmptyStr);

            out_tokens.push_back(kEmptyStr);
            string &sixth = out_tokens.back();
            copy( sixth_column_start, sixth_whitespace, back_inserter(sixth) );
        }
    }
}


bool CFeatureTableReader_Imp::x_AddQualifierToGene (
    CSeqFeatData& sfdata,
    EQual qtype,
    const string& val
)

{
    CGene_ref& grp = sfdata.SetGene ();
    switch (qtype) {
        case eQual_gene:
            grp.SetLocus (val);
            return true;
        case eQual_allele:
            grp.SetAllele (val);
            return true;
        case eQual_gene_desc:
            grp.SetDesc (val);
            return true;
        case eQual_gene_syn:
            {
                CGene_ref::TSyn& syn = grp.SetSyn ();
                syn.push_back (val);
                return true;
            }
        case eQual_map:
            grp.SetMaploc (val);
            return true;
        case eQual_locus_tag:
            grp.SetLocus_tag (val);
            return true;
        case eQual_nomenclature:
            /* !!! need to implement !!! */
            return true;
        default:
            break;
    }
    return false;
}


bool CFeatureTableReader_Imp::x_AddQualifierToCdregion (
    CRef<CSeq_feat> sfp,
    CSeqFeatData& sfdata,
    EQual qtype, const string& val
)

{
    CCdregion& crp = sfdata.SetCdregion ();
    switch (qtype) {
        case eQual_codon_start:
            {
                int frame = x_StringToLongNoThrow (val, kCdsFeatName, "codon_start");
                switch (frame) {
                    case 0:
                        crp.SetFrame (CCdregion::eFrame_not_set);
                        break;
                    case 1:
                        crp.SetFrame (CCdregion::eFrame_one);
                        break;
                    case 2:
                        crp.SetFrame (CCdregion::eFrame_two);
                        break;
                    case 3:
                        crp.SetFrame (CCdregion::eFrame_three);
                        break;
                    default:
                        break;
                }
                return true;
            }
        case eQual_EC_number:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                CProt_ref::TEc& ec = prp.SetEc ();
                ec.push_back (val);
                return true;
            }
        case eQual_function:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                CProt_ref::TActivity& fun = prp.SetActivity ();
                fun.push_back (val);
                return true;
            }
        case eQual_product:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                CProt_ref::TName& prod = prp.SetName ();
                prod.push_back (val);
                return true;
            }
        case eQual_prot_desc:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                prp.SetDesc (val);
                return true;
            }
        case eQual_prot_note:
            return x_AddGBQualToFeature(sfp, "prot_note", val);
        case eQual_transl_except:
            // add as GBQual, let cleanup convert to code_break
            return x_AddGBQualToFeature(sfp, "transl_except", val);
        case eQual_translation:
            // we should accept, but ignore this qual on CDSs.
            // so, do nothing but return success
            return true;
        case eQual_transl_table:
            // set genetic code directly, or add qualifier and let cleanup convert?
            try {
                int num = NStr::StringToLong(val);
                CGen_code_table::GetTransTable(num); // throws if bad num
                CRef<CGenetic_code::C_E> code(new CGenetic_code::C_E());
                code->SetId(num);
                crp.SetCode().Set().push_back(code);
                return true;
            } catch( CStringException ) {
                // if val is not a number, add qualifier directly and 
                // let cleanup convert?
                return x_AddGBQualToFeature(sfp, "transl_table", val);
            } catch( ... ) {
                // invalid genome code table so don't even try to make 
                // the transl_table qual
                x_ProcessMsg(
                    ILineError::eProblem_QualifierBadValue, eDiag_Error,
                    kCdsFeatName, "transl_table", val);
                return true;
            }
            break;
            
        default:
            break;
    }
    return false;
}


bool CFeatureTableReader_Imp::x_StringIsJustQuotes (
    const string& str
)

{
    ITERATE (string, it, str) {
      char ch = *it;
      if (ch > ' ' && ch != '"' && ch != '\'') return false;
    }

    return true;
}

static bool
s_LineIndicatesOrder( const CTempString & line )
{
    // basically, this is true if the line starts with "order" (whitespaces disregarded)

    const static char* kOrder = "ORDER";

    // find first non-whitespace character
    string::size_type pos = 0;
    for( ; pos < line.length() && isspace(line[pos]); ++pos) {
        // nothing to do here
    }

    // line is all whitespace
    if( pos >= line.length() ) {
        return false;
    }

    // check if starts with "order" after whitespace
    return ( 0 == NStr::CompareNocase( line, pos, strlen(kOrder), kOrder ) );
}

// Turns a "join" location into an "order" by putting nulls between it
// Returns an unset CRef if the loc doesn't need nulls (e.g. if it's just an interval)
static CRef<CSeq_loc>
s_LocationJoinToOrder( const CSeq_loc & loc )
{
    // create result we're returning
    CRef<CSeq_loc> result( new CSeq_loc );
    CSeq_loc_mix::Tdata & mix_pieces  = result->SetMix().Set();

    // keep this around for whenever we need a "null" piece
    CRef<CSeq_loc> loc_piece_null( new CSeq_loc );
    loc_piece_null->SetNull();

    // push pieces of source, with NULLs between
    CSeq_loc_CI loc_iter( loc );
    for( ; loc_iter; ++loc_iter ) {
        if( ! mix_pieces.empty() ) {
            mix_pieces.push_back( loc_piece_null );
        }
        CRef<CSeq_loc> new_piece( new CSeq_loc );
        new_piece->Assign( loc_iter.GetEmbeddingSeq_loc() );
        mix_pieces.push_back( new_piece );
    }

    // Only wrap in "mix" if there was more than one piece
    if( mix_pieces.size() > 1 ) {
        return result;
    } else {
        return CRef<CSeq_loc>();
    }
}


string CFeatureTableReader_Imp::x_TrnaToAaString(
    const string& val
)
{
    CTempString value(val);

    if (NStr::StartsWith(value, "tRNA-")) {
        value.assign(value, strlen("tRNA-"), CTempString::npos);
    }
    
    CTempString::size_type pos = value.find_first_of("-,;:()=\'_~");
    if (pos != CTempString::npos) {
        value.erase(pos);
        NStr::TruncateSpacesInPlace(value);
    }

    return string(value);
}


bool
CFeatureTableReader_Imp::x_ParseTrnaExtString(CTrna_ext & ext_trna, const string & str)
{
    if (NStr::IsBlank (str)) return false;

    string normalized_string = str;
    normalized_string.erase(
            remove_if(begin(normalized_string), 
                      end(normalized_string), 
                      [](char c) { return isspace(c);}),
            end(normalized_string));

    if ( NStr::StartsWith(normalized_string, "(pos:") ) {
        // find position of closing paren
        string::size_type pos_end = x_MatchingParenPos( normalized_string, 0 );
        if (pos_end != string::npos) {
            string pos_str = normalized_string.substr (5, pos_end - 5);
            string::size_type aa_start = NStr::FindNoCase(pos_str, "aa:");
            if (aa_start != string::npos) {
                auto seq_start = NStr::FindNoCase(pos_str, ",seq:");
                if (seq_start != string::npos &&
                    seq_start < aa_start+3) {
                    return false;
                }
                
                size_t aa_length = (seq_start == NPOS) ?
                                pos_str.size() - (aa_start+3) :
                                seq_start - (aa_start+3);   

                string abbrev = pos_str.substr (aa_start + 3, aa_length);
                //TTrnaMap::const_iterator 
                auto t_iter = sm_TrnaKeys.find (abbrev.c_str ());
                if (t_iter == sm_TrnaKeys.end ()) {
                    // unable to parse
                    return false;
                }
                CRef<CTrna_ext::TAa> aa(new CTrna_ext::TAa);
                aa->SetNcbieaa (t_iter->second);
                ext_trna.SetAa(*aa);
                pos_str = pos_str.substr (0, aa_start);
                NStr::TruncateSpacesInPlace (pos_str);
                if (NStr::EndsWith (pos_str, ",")) {
                    pos_str = pos_str.substr (0, pos_str.length() - 1);
                }
            }
            CGetSeqLocFromStringHelper helper;
            CRef<CSeq_loc> anticodon = GetSeqLocFromString (pos_str, m_seq_id, & helper);
            if (anticodon == NULL) {
                ext_trna.ResetAa();
                return false;
            } else {
                switch( anticodon->GetStrand() ) {
                case eNa_strand_unknown:
                case eNa_strand_plus:
                case eNa_strand_minus:
                    ext_trna.SetAnticodon(*anticodon);
                    return true;
                default:
                    ext_trna.ResetAa();
                    return false;
                }
            }
        }
    }

    return false;
}


SIZE_TYPE CFeatureTableReader_Imp::x_MatchingParenPos( 
    const string &str, SIZE_TYPE open_paren_pos )
{
    _ASSERT( str[open_paren_pos] == '(' );
    _ASSERT( open_paren_pos < str.length() );

    // nesting level. start at 1 since we know there's an open paren
    int level = 1;

    SIZE_TYPE pos = open_paren_pos + 1;
    for( ; pos < str.length(); ++pos ) {
        switch( str[pos] ) {
            case '(':
                // nesting deeper
                ++level;
                break;
            case ')':
                // closed a level of nesting
                --level;
                if( 0 == level ) {
                    // reached the top: we're closing the initial paren,
                    // so we return our position
                    return pos;
                }
                break;
            default:
                // ignore other characters.
                // maybe in the future we'll handle ignoring parens in quotes or
                // things like that.
                break;
        }
    }
    return NPOS;
}

long CFeatureTableReader_Imp::x_StringToLongNoThrow (
    CTempString strToConvert,
    CTempString strFeatureName,
    CTempString strQualifierName,
    ILineError::EProblem eProblem
)
{
    try {
        return NStr::StringToLong(strToConvert);
    } catch( ... ) {
        // See if we start with a number, but there's extra junk after it, try again
        if( ! strToConvert.empty() && isdigit(strToConvert[0]) ) {
            try {
                long result = NStr::StringToLong(strToConvert, NStr::fAllowTrailingSymbols);

                ILineError::EProblem problem = 
                    ILineError::eProblem_NumericQualifierValueHasExtraTrailingCharacters;
                if( eProblem != ILineError::eProblem_Unset ) {
                    problem = eProblem;
                }

                x_ProcessMsg( 
                    problem,
                    eDiag_Warning,
                    strFeatureName, strQualifierName, strToConvert );
                return result;
            } catch( ... ) { } // fall-thru to usual handling
        }

        ILineError::EProblem problem = 
            ILineError::eProblem_NumericQualifierValueIsNotANumber;
        if( eProblem != ILineError::eProblem_Unset ) {
            problem = eProblem;
        }

        x_ProcessMsg(
            problem,
            eDiag_Warning,
            strFeatureName, strQualifierName, strToConvert );
        // we have no idea, so just return zero
        return 0;
    }
}


bool CFeatureTableReader_Imp::x_AddQualifierToRna (
    CRef<CSeq_feat> sfp,
    EQual qtype,
    const string& val
)
{
    CSeqFeatData& sfdata = sfp->SetData();
    CRNA_ref& rrp = sfdata.SetRna ();
    CRNA_ref::EType rnatyp = rrp.GetType ();
    switch (rnatyp) {
        case CRNA_ref::eType_premsg:
        case CRNA_ref::eType_mRNA:
        case CRNA_ref::eType_rRNA:
            switch (qtype) {
                case eQual_product:
                    {
                        CRNA_ref::TExt& tex = rrp.SetExt ();
                        CRNA_ref::C_Ext::E_Choice exttype = tex.Which ();
                        if (exttype == CRNA_ref::C_Ext::e_TRNA) return false;
                        tex.SetName (val);
                        return true;
                    }
                default:
                    break;
            }
            break;
        case CRNA_ref::eType_ncRNA:
            switch (qtype) {
                case eQual_product:
                    rrp.SetExt().SetGen().SetProduct(val);
                    return true;
                    break;
                case eQual_ncRNA_class:
                    rrp.SetExt().SetGen().SetClass(val);
                    return true;
                    break;
                default:
                    break;
            }
            break;
        case CRNA_ref::eType_tmRNA:
            switch (qtype) {
                case eQual_product:
                    rrp.SetExt().SetGen().SetProduct(val);
                    return true;
                case eQual_tag_peptide:
                  {
                    CRef<CRNA_qual> q(new CRNA_qual());
                    q->SetQual("tag_peptide");
                    q->SetVal(val);
                    rrp.SetExt().SetGen().SetQuals().Set().push_back(q);
                    return true;
                  }
                  break;
                default:
                    break;
            }
            break;
        case CRNA_ref::eType_snRNA:
        case CRNA_ref::eType_scRNA:
        case CRNA_ref::eType_snoRNA:
        case CRNA_ref::eType_other:
            return false;
        case CRNA_ref::eType_tRNA:
            switch (qtype) {
                case eQual_product: {
                        if (rrp.IsSetExt() && rrp.GetExt().Which() == CRNA_ref::C_Ext::e_Name) 
                            return false;

                
                        const string& aa_string = x_TrnaToAaString(val);
                        const auto aaval_it = sm_TrnaKeys.find(aa_string.c_str());

                        if (aaval_it != sm_TrnaKeys.end()) {
                            CRNA_ref::TExt& tex = rrp.SetExt ();
                            CTrna_ext& trx = tex.SetTRNA();
                            CTrna_ext::TAa& taa = trx.SetAa();
                            taa.SetNcbieaa(aaval_it->second);
                            if (aa_string == "fMet" ||
                                aa_string == "iMet" ||
                                aa_string == "Ile2") {
                               x_AddGBQualToFeature(sfp, "product", val); 
                            }
                        }
                        else {
                            x_ProcessMsg(
                                ILineError::eProblem_QualifierBadValue, eDiag_Warning,
                                "tRNA", "product", val);
                        }
                        return true;
                    }
                    break;
                case eQual_anticodon:
                    {
                        CRNA_ref::TExt& tex = rrp.SetExt ();
                        CRNA_ref::C_Ext::TTRNA & ext_trna = tex.SetTRNA();
                        if( ! x_ParseTrnaExtString(ext_trna, val) ) {
                            x_ProcessMsg(
                                ILineError::eProblem_QualifierBadValue, eDiag_Error,
                                "tRNA", "anticodon", val );
                        }
                        return true;
                    }
                    break;
                case eQual_codon_recognized: 
                    {
                        //const auto codon_index = CGen_code_table::CodonToIndex(val);
                        //if (codon_index >= 0) {
                            CRNA_ref::TExt& tex = rrp.SetExt ();
                            CRNA_ref::C_Ext::TTRNA & ext_trna = tex.SetTRNA();
                            if (!x_AddCodons(val, ext_trna)) {
                                return false;
                            }
                        //}
                        return true;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return false;
}


bool CFeatureTableReader_Imp::x_AddCodons(
        const string& val,
        CTrna_ext& trna_ext
    ) const
{
    if (val.size() != 3) {
        return false;
    }

    set<int> codons;
    try {
        for (char char1 : s_IUPACmap.at(val[0])) {
            for (char char2 : s_IUPACmap.at(val[1])) {
                for (char char3 : s_IUPACmap.at(val[2])) {
                    const auto codon_index = CGen_code_table::CodonToIndex(char1, char2, char3);
                    codons.insert(codon_index);
                }
            }
        }

        if (!codons.empty()) {
            trna_ext.SetAa().SetNcbieaa();
            for (const auto codon_index : codons) {
                trna_ext.SetCodon().push_back(codon_index);
            }
        }
        return true;
    }
    catch(...) {}

    return false;
}


bool CFeatureTableReader_Imp::x_AddQualifierToImp (
    CRef<CSeq_feat> sfp,
    CSeqFeatData& sfdata,
    EQual qtype,
    const string& qual,
    const string& val
)

{
    const char *str = NULL;

    CSeqFeatData::ESubtype subtype = sfdata.GetSubtype ();

    // used if-statement because CSeqFeatData::IsRegulatory won't work in a
    // switch statement.
    if( (subtype == CSeqFeatData::eSubtype_regulatory) ||
        CSeqFeatData::IsRegulatory(subtype) )
    {
        if (qtype == eQual_regulatory_class) {
            if (val != "other") { // RW-374 "other" is a special case 

                const vector<string>& allowed_values = 
                    CSeqFeatData::GetRegulatoryClassList();
                if (find(allowed_values.cbegin(), allowed_values.cend(), val) 
                    == allowed_values.cend()) {
                    return false;
                }

/*
                const CSeqFeatData::ESubtype regulatory_class_subtype =
                    CSeqFeatData::GetRegulatoryClass(val);
                if( regulatory_class_subtype == CSeqFeatData::eSubtype_bad ) {
                    // msg will be sent in caller x_AddQualifierToFeature
                    return false; 
                } 
                */
            }
            // okay 
            // (Note that at this time we don't validate
            // if the regulatory_class actually matches the
            // subtype)
            x_AddGBQualToFeature(sfp, qual, val);
            return true;
        }
    }

    switch (subtype) {
        case CSeqFeatData::eSubtype_variation:
            {
                switch (qtype) {
                    case eQual_chrcnt:
                    case eQual_ctgcnt:
                    case eQual_loccnt:
                    case eQual_snp_class:
                    case eQual_snp_gtype:
                    case eQual_snp_het:
                    case eQual_snp_het_se:
                    case eQual_snp_linkout:
                    case eQual_snp_maxrate:
                    case eQual_snp_valid:
                    case eQual_weight:
                        str = "dbSnpSynonymyData";
                        break;
                    default:
                        break;
                }
            }
            break;
        case CSeqFeatData::eSubtype_STS:
            {
                switch (qtype) {
                    case eQual_sts_aliases:
                    case eQual_sts_dsegs:
                    case eQual_weight:
                        str = "stsUserObject";
                        break;
                    default:
                        break;
                }
            }
            break;
        case CSeqFeatData::eSubtype_misc_feature:
            {
                switch (qtype) {
                    case eQual_bac_ends:
                    case eQual_clone_id:
                    case eQual_method:
                    case eQual_sequence:
                    case eQual_STS:
                    case eQual_weight:
                        str = "cloneUserObject";
                        break;
                    default:
                        break;
                }
            }
            break;
        default:
            break;
    }

    if( NULL != str ) {
        CSeq_feat::TExt& ext = sfp->SetExt ();
        CObject_id& obj = ext.SetType ();
        if ((! obj.IsStr ()) || obj.GetStr ().empty ()) {
            obj.SetStr ();
        }
        ext.AddField (qual, val, CUser_object::eParse_Number);
        return true;
    }

    return false;
}


bool CFeatureTableReader_Imp::x_AddQualifierToBioSrc (
    CSeqFeatData& sfdata,
    const string &feat_name,
    EOrgRef rtype,
    const string& val
)
{
    CBioSource& bsp = sfdata.SetBiosrc ();

    switch (rtype) {
        case eOrgRef_organism:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                orp.SetTaxname (val);
                return true;
            }
        case eOrgRef_organelle:
            {
                TGenomeMap::const_iterator g_iter = sm_GenomeKeys.find (val.c_str ());
                if (g_iter != sm_GenomeKeys.end ()) {
                    CBioSource::EGenome gtype = g_iter->second;
                    bsp.SetGenome (gtype);
                } else {
                    x_ProcessMsg(
                        ILineError::eProblem_QualifierBadValue, eDiag_Error,
                        feat_name, "organelle", val );
                }
                return true;
            }
        case eOrgRef_div:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                onp.SetDiv (val);
                return true;
            }
        case eOrgRef_lineage:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                onp.SetLineage (val);
                return true;
            }
        case eOrgRef_gcode:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                int code = x_StringToLongNoThrow (val, feat_name, "gcode");
                onp.SetGcode (code);
                return true;
            }
        case eOrgRef_mgcode:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                int code = x_StringToLongNoThrow (val, feat_name, "mgcode");
                onp.SetMgcode (code);
                return true;
            }
        default:
            break;
    }
    return false;
}


bool CFeatureTableReader_Imp::x_AddQualifierToBioSrc (
    CSeqFeatData& sfdata,
    CSubSource::ESubtype stype,
    const string& val
)

{
    CBioSource& bsp = sfdata.SetBiosrc ();
    CBioSource::TSubtype& slist = bsp.SetSubtype ();
    CRef<CSubSource> ssp (new CSubSource);
    ssp->SetSubtype (stype);
    ssp->SetName (val);
    slist.push_back (ssp);
    return true;
}


bool CFeatureTableReader_Imp::x_AddQualifierToBioSrc (
    CSeqFeatData& sfdata,
    COrgMod::ESubtype mtype,
    const string& val
)

{
    CBioSource& bsp = sfdata.SetBiosrc ();
    CBioSource::TOrg& orp = bsp.SetOrg ();
    COrg_ref::TOrgname& onp = orp.SetOrgname ();
    COrgName::TMod& mlist = onp.SetMod ();
    CRef<COrgMod> omp (new COrgMod);
    omp->SetSubtype (mtype);
    omp->SetSubname (val);
    mlist.push_back (omp);
    return true;
}


bool CFeatureTableReader_Imp::x_AddGBQualToFeature (
    CRef<CSeq_feat> sfp,
    const string& qual,
    const string& val
)

{
    if (qual.empty ()) return false;

    // need this pointer because references can't be repointed
    CTempString normalized_qual = qual;

    // normalize qual if needed, especially regarding case, and
    // use as-is if no normalization applies
    auto qual_type = CSeqFeatData::GetQualifierType(qual);
    if( qual_type != CSeqFeatData::eQual_bad ) {
        // swap is constant time
        CTempString potential_normalized_qual = CSeqFeatData::GetQualifierAsString(qual_type);
        if( ! potential_normalized_qual.empty() ) {
            normalized_qual = potential_normalized_qual;
        }
    }

    auto& qlist = sfp->SetQual ();
    CRef<CGb_qual> gbq (new CGb_qual);
    gbq->SetQual() = normalized_qual;
    if (x_StringIsJustQuotes (val)) {
        gbq->SetVal() = kEmptyStr;
    } else {
        gbq->SetVal() = val;
    }
    qlist.push_back (gbq);

    return true;
}


void CFeatureTableReader_Imp::x_CreateGenesFromCDSs(
    CRef<CSeq_annot> sap,
    TChoiceToFeatMap & choiceToFeatMap,
    const TFlags flags)
{
    // load cds_equal_range to hold the CDSs
    typedef TChoiceToFeatMap::iterator TChoiceCI;
    typedef pair<TChoiceCI, TChoiceCI> TChoiceEqualRange;
    TChoiceEqualRange cds_equal_range = 
        choiceToFeatMap.equal_range(CSeqFeatData::e_Cdregion);
    if( cds_equal_range.first == cds_equal_range.second )
    {
        // nothing to do if there are no CDSs
        return;
    }

    // load mappings from locus or locus-tag to gene
    typedef multimap<string, SFeatAndLineNum> TStringToGeneAndLineMap;
    TStringToGeneAndLineMap locusToGeneAndLineMap;
    TStringToGeneAndLineMap locusTagToGeneAndLineMap;
    const TChoiceEqualRange gene_equal_range =
        choiceToFeatMap.equal_range(CSeqFeatData::e_Gene);
    for( TChoiceCI gene_choice_ci = gene_equal_range.first; 
        gene_choice_ci != gene_equal_range.second;
        ++gene_choice_ci ) 
    {
        SFeatAndLineNum gene_feat_ref_and_line = gene_choice_ci->second;
        const CGene_ref & gene_ref = gene_feat_ref_and_line.m_pFeat->GetData().GetGene();
        if( ! RAW_FIELD_IS_EMPTY_OR_UNSET(gene_ref, Locus) ) {
            locusToGeneAndLineMap.insert(
                TStringToGeneAndLineMap::value_type(
                    gene_ref.GetLocus(), gene_feat_ref_and_line));
        }
        if( ! RAW_FIELD_IS_EMPTY_OR_UNSET(gene_ref, Locus_tag) ) {
            locusTagToGeneAndLineMap.insert(
                TStringToGeneAndLineMap::value_type(
                    gene_ref.GetLocus_tag(), gene_feat_ref_and_line));
        }
    }

    // for each CDS, check for gene conflicts or create genes,
    // depending on various flags
    for( TChoiceCI cds_choice_ci = cds_equal_range.first;
        cds_choice_ci != cds_equal_range.second ; ++cds_choice_ci) 
    {
        TFeatConstRef cds_feat_ref = cds_choice_ci->second.m_pFeat;
        const TSeqPos cds_line_num = cds_choice_ci->second.m_uLineNum;

        const CSeq_loc & cds_loc = cds_feat_ref->GetLocation();

        const CGene_ref * pGeneXrefOnCDS = cds_feat_ref->GetGeneXref();
        if( ! pGeneXrefOnCDS ) {
            // no xref, so can't do anything for this CDS
            // (this is NOT an error)
            continue;
        }

        // get all the already-existing genes that
        // this CDS xrefs.  It should be somewhat uncommon for there
        // to be more than one matching gene.
        set<SFeatAndLineNum> matchingGenes;

        const string locus = 
            pGeneXrefOnCDS->IsSetLocus() ? 
            pGeneXrefOnCDS->GetLocus() :
            "";

        const string locus_tag = 
            pGeneXrefOnCDS->IsSetLocus_tag() ?
            pGeneXrefOnCDS->GetLocus_tag() :
            "";


        {{
            // all the code in this scope is all just for setting up matchingGenes

            typedef TStringToGeneAndLineMap::iterator TStrToGeneCI;
            typedef pair<TStrToGeneCI, TStrToGeneCI> TStrToGeneEqualRange;
            set<SFeatAndLineNum> locusGeneMatches;
            // add the locus matches (if any) to genesAlreadyCreated
            if( !NStr::IsBlank(locus) ) {
                TStrToGeneEqualRange locus_equal_range =
                    locusToGeneAndLineMap.equal_range(locus);
                for( TStrToGeneCI locus_gene_ci = locus_equal_range.first;
                    locus_gene_ci != locus_equal_range.second;
                    ++locus_gene_ci  ) 
                {
                    if (!NStr::IsBlank(locus_tag)) {
                        auto gene_feat = locus_gene_ci->second.m_pFeat;
                        if (gene_feat->GetData().GetGene().IsSetLocus_tag() &&
                            gene_feat->GetData().GetGene().GetLocus_tag() != locus_tag) {
                            continue;
                        }
                    }
                    locusGeneMatches.insert(locus_gene_ci->second);
                }
            }
            // remove any that don't also match the locus-tag (if any)
            set<SFeatAndLineNum> locusTagGeneMatches;
            if( !NStr::IsBlank(locus_tag) ) {
                TStrToGeneEqualRange locus_tag_equal_range =
                    locusTagToGeneAndLineMap.equal_range(locus_tag);
                for( TStrToGeneCI locus_tag_gene_ci = locus_tag_equal_range.first;
                     locus_tag_gene_ci != locus_tag_equal_range.second;
                     ++locus_tag_gene_ci )
                {
                    if (!NStr::IsBlank(locus)) {
                        auto gene_feat = locus_tag_gene_ci->second.m_pFeat;
                        if (gene_feat->GetData().GetGene().IsSetLocus() &&
                            gene_feat->GetData().GetGene().GetLocus() != locus) {
                            continue;
                        }
                    }
                    locusTagGeneMatches.insert(locus_tag_gene_ci->second);
                }
            }
            // analyze locusGeneMatches and locusTagGeneMatches to find matchingGenes.
            if( locusGeneMatches.empty() ) {
                // swap is faster than assignment
                matchingGenes.swap(locusTagGeneMatches);
            } else if( locusTagGeneMatches.empty() ) {
                // swap is faster than assignment
                matchingGenes.swap(locusGeneMatches);
            } else {
                // get only the genes that match both (that is, the intersection)
                set_intersection(
                    locusGeneMatches.begin(), locusGeneMatches.end(),
                    locusTagGeneMatches.begin(), locusTagGeneMatches.end(),
                    inserter(matchingGenes, matchingGenes.begin()));
            }
        }}

        // if requested, check that the genes really do contain the CDS
        // (also check if we're trying to create a gene that already exists)
        
            ITERATE(set<SFeatAndLineNum>, gene_feat_and_line_ci, matchingGenes) {
                const CSeq_loc & gene_loc = gene_feat_and_line_ci->m_pFeat->GetLocation();
                const TSeqPos gene_line_num = gene_feat_and_line_ci->m_uLineNum;

                if ((flags & CFeature_table_reader::fCDSsMustBeInTheirGenes) != 0) {

                    // CDS's loc minus gene's loc should be an empty location
                    // because the CDS should be entirely on the gene
                    CRef<CSeq_loc> pCdsMinusGeneLoc = cds_loc.Subtract(
                        gene_loc, CSeq_loc::fSortAndMerge_All, NULL, NULL);
                    if( pCdsMinusGeneLoc &&
                        ! pCdsMinusGeneLoc->IsNull() &&
                        ! pCdsMinusGeneLoc->IsEmpty() )
                    {
                        ILineError::TVecOfLines gene_lines;
                        if( gene_line_num > 0 ) {
                            gene_lines.push_back(gene_line_num);
                        }
                        x_ProcessMsg( 
                            cds_line_num,
                            ILineError::eProblem_FeatMustBeInXrefdGene, eDiag_Error,
                            kCdsFeatName, 
                            kEmptyStr, kEmptyStr, kEmptyStr,
                            gene_lines );
                    }
                }
            }

        // if requested, create genes for the CDS if there isn't already one
        // (it is NOT an error if the gene is already created)
        if ( (flags & CFeature_table_reader::fCreateGenesFromCDSs) != 0 && 
            matchingGenes.empty() )
        {
            // create the gene
            CRef<CSeq_feat> pNewGene( new CSeq_feat );
            pNewGene->SetData().SetGene().Assign( *pGeneXrefOnCDS );
            if( FIELD_EQUALS(*cds_feat_ref, Partial, true) ) pNewGene->SetPartial(true);
            pNewGene->SetLocation().Assign( cds_feat_ref->GetLocation() );

            // add gene the annot
            _ASSERT( sap->IsFtable() );
            TFtable & the_ftable = sap->SetData().SetFtable();
            the_ftable.push_back(pNewGene);

            // add it to our local information for later CDSs
            SFeatAndLineNum  gene_feat_and_line(pNewGene, 0);
            choiceToFeatMap.insert(
                TChoiceToFeatMap::value_type(
                    pNewGene->GetData().Which(), gene_feat_and_line ) );
            if( ! RAW_FIELD_IS_EMPTY_OR_UNSET(*pGeneXrefOnCDS, Locus) ) {
                locusToGeneAndLineMap.insert(
                    TStringToGeneAndLineMap::value_type(
                        pGeneXrefOnCDS->GetLocus(), gene_feat_and_line));
            }
            if( ! RAW_FIELD_IS_EMPTY_OR_UNSET(*pGeneXrefOnCDS, Locus_tag) ) {
                locusTagToGeneAndLineMap.insert(
                    TStringToGeneAndLineMap::value_type(
                        pGeneXrefOnCDS->GetLocus_tag(), gene_feat_and_line));
            }
        }
    } // end of iteration through the CDS's
}

static const string s_QualsWithCaps[] = {
  "EC_number",
  "PCR_conditions",
  "PubMed",
  "STS",
  "ncRNA_class"
};

static const int s_NumQualsWithCaps = sizeof (s_QualsWithCaps) / sizeof (string);

static string s_FixQualCapitalization (const string& qual)
{
    string lqual = qual;
    lqual = NStr::ToLower(lqual);
    for (int j = 0; j < s_NumQualsWithCaps; j++) {    
        if (NStr::EqualNocase(lqual, s_QualsWithCaps[j])) {
            lqual = s_QualsWithCaps[j];
            break;
        }
    }
    return lqual;
}


bool CFeatureTableReader_Imp::x_AddNoteToFeature(
        CRef<CSeq_feat> sfp,
        const string& note)
{
    if (sfp.IsNull()) {
        return false;
    }

    if (NStr::IsBlank(note)) { // Nothing to do
        return true;
    }

    string comment = (sfp->CanGetComment()) ? 
        sfp->GetComment() + "; " + note :
        note;
        sfp->SetComment(comment);
    return true;
}


bool CFeatureTableReader_Imp::x_AddNoteToFeature(
    CRef<CSeq_feat> sfp,
    const string& feat_name,
    const string& qual,
    const string& val) {

    if (!x_AddNoteToFeature(sfp, val)) {
        return false;
    }
    // Else convert qualifier to note and issue warning
    if (qual != "note") {
        string error_message = 
            qual + " is not a valid qualifier for this feature. Converting to note."; 
        x_ProcessMsg(                        
        ILineError::eProblem_InvalidQualifier, eDiag_Warning,
        feat_name, qual, kEmptyStr, error_message);
    }
    return true;
}

bool CFeatureTableReader_Imp::x_AddQualifierToFeature (
    CRef<CSeq_feat> sfp,
    const string &feat_name,
    const string& qual,
    const string& val,
    const TFlags flags
)

{
    CSeqFeatData&          sfdata = sfp->SetData ();
    CSeqFeatData::E_Choice featType = sfdata.Which ();

    const CSeqFeatData::EQualifier qual_type =
        CSeqFeatData::GetQualifierType(qual);
    if( (flags & CFeature_table_reader::fReportDiscouragedKey) != 0 ) {
        if( CSeqFeatData::IsDiscouragedQual(qual_type) ) {
            x_ProcessMsg(
                ILineError::eProblem_DiscouragedQualifierName,
                eDiag_Warning, feat_name, qual);
        }
    }

    if (featType == CSeqFeatData::e_Biosrc) {

        TOrgRefMap::const_iterator o_iter = sm_OrgRefKeys.find (qual.c_str ());
        if (o_iter != sm_OrgRefKeys.end ()) {
            EOrgRef rtype = o_iter->second;
            if (x_AddQualifierToBioSrc (sfdata, feat_name, rtype, val)) return true;
        } else {

            TSubSrcMap::const_iterator s_iter = sm_SubSrcKeys.find (qual.c_str ());
            if (s_iter != sm_SubSrcKeys.end ()) {

                CSubSource::ESubtype stype = s_iter->second;
                if (x_AddQualifierToBioSrc (sfdata, stype, val)) return true;

            } else {

                TOrgModMap::const_iterator m_iter = sm_OrgModKeys.find (qual.c_str ());
                if (m_iter != sm_OrgModKeys.end ()) {

                    COrgMod::ESubtype  mtype = m_iter->second;
                    if (x_AddQualifierToBioSrc (sfdata, mtype, val)) return true;
                }
            }
        }
        return false;
    } 


    // else type != CSeqFeatData::e_Biosrc
    string lqual = s_FixQualCapitalization(qual);
    TQualMap::const_iterator q_iter = sm_QualKeys.find (lqual.c_str ());
    if (q_iter != sm_QualKeys.end ()) {
        EQual qtype = q_iter->second;
        switch (featType) {
            case CSeqFeatData::e_Gene:
                if (x_AddQualifierToGene (sfdata, qtype, val)) return true;
                break;
            case CSeqFeatData::e_Cdregion:
                if (x_AddQualifierToCdregion (sfp, sfdata, qtype, val)) return true;
                break;
            case CSeqFeatData::e_Rna:
                if (x_AddQualifierToRna (sfp, qtype, val)) return true;
                break;
            case CSeqFeatData::e_Imp:
                if (x_AddQualifierToImp (sfp, sfdata, qtype, qual, val)) return true;
                break;
            case CSeqFeatData::e_Region:
                if (qtype == eQual_region_name) {
                    sfdata.SetRegion (val);
                    return true;
                }
                break;
            case CSeqFeatData::e_Bond:
                if (qtype == eQual_bond_type) {
                    CSeqFeatData::EBond btyp = CSeqFeatData::eBond_other;
                    if (CSeqFeatData::GetBondList()->IsBondName(val.c_str(), btyp)) {
                        sfdata.SetBond (btyp);
                        return true;
                    }
                }
                break;
            case CSeqFeatData::e_Site:
                if (qtype == eQual_site_type) {
                    CSeqFeatData::ESite styp = CSeqFeatData::eSite_other;
                    if (CSeqFeatData::GetSiteList()->IsSiteName( val.c_str(), styp)) {
                        sfdata.SetSite (styp);
                        return true;
                    }
                }
                break;
            case CSeqFeatData::e_Pub:
                if( qtype == eQual_PubMed ) {
                    CRef<CPub> new_pub( new CPub );
                    new_pub->SetPmid( CPubMedId( ENTREZ_ID_FROM(long, x_StringToLongNoThrow(val, feat_name, qual)) ) );
                    sfdata.SetPub().SetPub().Set().push_back( new_pub );
                    return true;
                }
                break;
            case CSeqFeatData::e_Prot:
                switch( qtype ) {
                case eQual_product:
                    sfdata.SetProt().SetName().push_back( val );
                    return true;
                case eQual_function:
                    sfdata.SetProt().SetActivity().push_back( val );
                    return true;
                case eQual_EC_number:
                    sfdata.SetProt().SetEc().push_back( val );
                    return true;
                default:
                    break;
                }
                break;
            default:
                break;
            }

        switch (qtype) {
            case eQual_pseudo:
                sfp->SetPseudo (true);
                return true;
            case eQual_partial:
                sfp->SetPartial (true);
                return true;
            case eQual_exception:
                sfp->SetExcept (true);
                sfp->SetExcept_text (val);
                return true;
            case eQual_ribosomal_slippage:
                sfp->SetExcept (true);
                sfp->SetExcept_text (qual);
                return true;
            case eQual_trans_splicing:
                sfp->SetExcept (true);
                sfp->SetExcept_text (qual);
                return true;
            case eQual_evidence:
                if (val == "experimental") {
                    sfp->SetExp_ev (CSeq_feat::eExp_ev_experimental);
                } else if (val == "not_experimental" || val == "non_experimental" ||
                           val == "not-experimental" || val == "non-experimental") {
                    sfp->SetExp_ev (CSeq_feat::eExp_ev_not_experimental);
                }
                return true;
            case eQual_note:
                    return x_AddNoteToFeature(sfp, val);
            case eQual_inference:
                {
                    string prefix, remainder;
                    CInferencePrefixList::GetPrefixAndRemainder(val, prefix, remainder);
                    if (!NStr::IsBlank(prefix)) {
                        x_AddGBQualToFeature(sfp, qual, val);
                    }
                    else {
                        x_ProcessMsg(
                            ILineError::eProblem_QualifierBadValue, eDiag_Error,
                            feat_name, qual, val);
                    }
                    return true;
                }
            case eQual_replace:
                {
                    string val_copy = val;
                    NStr::ToLower( val_copy );
                    x_AddGBQualToFeature (sfp, qual, val_copy );
                    return true;
                }
            case eQual_allele:
            case eQual_bound_moiety:
            case eQual_clone:
            case eQual_compare:
            case eQual_cons_splice:
            case eQual_direction:
            case eQual_EC_number:
            case eQual_estimated_length:
            case eQual_experiment:
            case eQual_frequency:
            case eQual_function:
            case eQual_gap_type:
            case eQual_insertion_seq:
            case eQual_label:
            case eQual_linkage_evidence:
            case eQual_map:
            case eQual_ncRNA_class:
            case eQual_number:
            case eQual_old_locus_tag:
            case eQual_operon:
            case eQual_organism:
            case eQual_PCR_conditions:
            case eQual_phenotype:
            case eQual_product:
            case eQual_pseudogene:
            case eQual_satellite:
            case eQual_rpt_family:
            case eQual_rpt_type:
            case eQual_rpt_unit:
            case eQual_rpt_unit_range:
            case eQual_rpt_unit_seq:
            case eQual_standard_name:
            case eQual_tag_peptide:
            case eQual_transposon:
            case eQual_usedin:
            case eQual_cyt_map:
            case eQual_gen_map:
            case eQual_rad_map:
            case eQual_mobile_element_type:
                {
                    x_AddGBQualToFeature (sfp, qual, val);
                    return true;
                }
            case eQual_gene:
                {
                    if (CSeqFeatData::CanHaveGene(sfdata.GetSubtype())) {
                        CGene_ref& grp = sfp->SetGeneXref ();
                        if (val != "-") {
                            grp.SetLocus (val);
                        }
                        return true;
                    }
                    // else:
                    return x_AddNoteToFeature(sfp, feat_name, qual, val);
                }
            case eQual_gene_desc:
                {
                    if (CSeqFeatData::CanHaveGene(sfdata.GetSubtype())) {
                        CGene_ref& grp = sfp->SetGeneXref ();
                        grp.SetDesc (val);
                        return true;
                    }
                    // else:
                    return x_AddNoteToFeature(sfp, feat_name, qual, val);
                }
            case eQual_gene_syn:
                {
                    if (CSeqFeatData::CanHaveGene(sfdata.GetSubtype())) {
                        CGene_ref& grp = sfp->SetGeneXref ();
                        CGene_ref::TSyn& syn = grp.SetSyn ();
                        syn.push_back (val);
                        return true;
                    }
                    // else:
                    return x_AddNoteToFeature(sfp, feat_name, qual, val);
                }
            case eQual_locus_tag:
                {
                    if (CSeqFeatData::CanHaveGene(sfdata.GetSubtype())) {
                        CGene_ref& grp = sfp->SetGeneXref ();
                        grp.SetLocus_tag (val);
                        return true;
                    } 
                    // else:
                    return x_AddNoteToFeature(sfp, feat_name, qual, val);
                }
            case eQual_db_xref:
                {
                    CTempString db, tag;
                    if (NStr::SplitInTwo (val, ":", db, tag)) {
                        CSeq_feat::TDbxref& dblist = sfp->SetDbxref ();
                        CRef<CDbtag> dbt (new CDbtag);
                        dbt->SetDb (db);
                        CRef<CObject_id> oid (new CObject_id);
                        static const char* digits = "0123456789";
                        if (tag.find_first_not_of(digits) == string::npos && !NStr::IsBlank(tag))
                            oid->SetId(NStr::StringToLong(tag));
                        else
                            oid->SetStr(tag);
                        dbt->SetTag (*oid);
                        dblist.push_back (dbt);
                        return true;
                    }
                    return true;
                }
            case eQual_nomenclature:
                {
                    /* !!! need to implement !!! */
                    return true;
                }
            case eQual_go_component:
            case eQual_go_function:
            case eQual_go_process:
                if (featType == CSeqFeatData::e_Gene || 
                    featType == CSeqFeatData::e_Cdregion || 
                    featType == CSeqFeatData::e_Rna) {
                    try {
                        CReadUtil::AddGeneOntologyTerm(*sfp, qual, val);
                    }
                    catch( ILineError& err) {
                        x_ProcessMsg(
                            err.Problem(), 
                            err.Severity(), 
                            feat_name, qual, val, 
                            err.ErrorMessage());
                    }
                    //rw-621: throw out the faulty qualifier but retain the rest of the feature.
                    return true;
                }
                return false;
            case eQual_transcript_id:
                {
                    if (featType == CSeqFeatData::e_Rna &&
                        sfdata.GetRna().GetType() == CRNA_ref::eType_mRNA) {
                        CBioseq::TId ids;
                        try {
                            CSeq_id::ParseIDs(ids, val, 
                                CSeq_id::fParse_ValidLocal 
                            |   CSeq_id::fParse_PartialOK);
                        }
                        catch (CSeqIdException& e) 
                        {
                            x_ProcessMsg(
                                ILineError::eProblem_QualifierBadValue, eDiag_Error,
                                feat_name, qual, val,
                                "Invalid transcript_id  : " + val);
                            return true;
                        }

                        for (const auto& id : ids) {
                            auto id_string = id->GetSeqIdString(true);
                            auto res = m_ProcessedTranscriptIds.insert(id_string);
                            if (res.second == false) { // Insertion failed because Seq-id already encountered
                                x_ProcessMsg(
                                    ILineError::eProblem_DuplicateIDs, eDiag_Error, 
                                    feat_name, qual, val, 
                                    "Transcript ID " + id_string + " appears on multiple mRNA features"
                                );
                            }
                        }
                    }
                    x_AddGBQualToFeature(sfp, qual, val);
                    return true;
                }
            case eQual_protein_id:
                // see SQD-1535 and SQD-3496
                if (featType == CSeqFeatData::e_Cdregion ||
                    (featType == CSeqFeatData::e_Rna &&
                    sfdata.GetRna().GetType() == CRNA_ref::eType_mRNA) ||
                    (featType == CSeqFeatData::e_Prot &&
                     sfdata.GetProt().IsSetProcessed() &&
                     sfdata.GetProt().GetProcessed() == CProt_ref::eProcessed_mature))
                {
                    CBioseq::TId ids;
                    try {
                        CSeq_id::ParseIDs(ids, val,                                
                                CSeq_id::fParse_ValidLocal |
                                CSeq_id::fParse_PartialOK);
                    }
                    catch (CSeqIdException& e) 
                    {
                        x_ProcessMsg(
                                ILineError::eProblem_QualifierBadValue, eDiag_Error,
                                feat_name, qual, val,
                                "Invalid protein_id  : " + val);
                        return true;
                    }
                    
                    if (featType == CSeqFeatData::e_Cdregion) {
                        for (const auto& id : ids) {
                            auto id_string = id->GetSeqIdString(true);
                            auto res = m_ProcessedProteinIds.insert(id_string);
                            if (res.second == false) { // Insertion failed because Seq-id already encountered
                                x_ProcessMsg(
                                    ILineError::eProblem_DuplicateIDs, eDiag_Error, 
                                    feat_name, qual, val, 
                                    "Protein ID " + id_string + " appears on multiple CDS features"
                                );
                            }
                        }
                    }
                        
                    if (featType != CSeqFeatData::e_Rna) { // mRNA only has a protein_id qualifier
                        auto pBestId = GetBestId(ids);
                        if (pBestId) {
                            sfp->SetProduct().SetWhole(*pBestId);
                        }
                    }
                }

                if (featType != CSeqFeatData::e_Prot) { // Mat-peptide has an instantiated product, but no qualifier
                    x_AddGBQualToFeature(sfp, qual, val);
                }
                return true;
            case eQual_regulatory_class:
                // This should've been handled up in x_AddQualifierToImp
                // so it's always a bad value to be here
                x_ProcessMsg(                        
                    ILineError::eProblem_QualifierBadValue, eDiag_Error,
                    feat_name, qual, val );
                return true;
            default:
                break;
        }
    }
    return false;
}

bool CFeatureTableReader_Imp::x_IsWebComment(CTempString line)
{
    // This function is testing for a match against the following regular
    // expression, but we avoid actual regexps for max speed:
    // "^(===================================================================| INFO:| WARNING:| ERROR:).*"

    // (that magic number is the size of the smallest possible match)
    if( line.length() < 6 ) { 
        return false;
    }

    if( line[0] == '=' ) {
        static const CTempString kAllEqualsMatch =
            "===================================================================";
        if( NStr::StartsWith(line, kAllEqualsMatch) ) {
            return true;
        }
    } else if( line[0] == ' ') {
        switch(line[1]) {
        case 'I':
            {
                static const CTempString kInfo = " INFO:";
                if( NStr::StartsWith(line, kInfo) ) {
                    return true;
                }
            }
            break;
        case 'W':
            {
                static const CTempString kWarning = " WARNING:";
                if( NStr::StartsWith(line, kWarning) ) {
                    return true;
                }
            }
            break;
        case 'E':
            {
                static const CTempString kError = " ERROR:";
                if( NStr::StartsWith(line, kError) ) {
                    return true;
                }
            }
            break;
        default:
            // no match
            break;
        }
    }

    // no match
    return false;
}

bool CFeatureTableReader_Imp::x_AddIntervalToFeature(
    CTempString strFeatureName,
    CRef<CSeq_feat>& sfp,
    const SFeatLocInfo& loc_info
)

{

    auto start = loc_info.start_pos;
    auto stop = loc_info.stop_pos;

    const Int4 orig_start = start;
    CSeq_interval::TStrand strand = eNa_strand_plus;

    if (start > stop) {
        swap(start, stop);
        strand = eNa_strand_minus;
    }
    if (loc_info.is_minus_strand) {
        strand = eNa_strand_minus;
    }

    // construct loc, which will be added to the mix
    CSeq_loc_mix::Tdata & mix_set = sfp->SetLocation().SetMix();
    CRef<CSeq_loc> loc(new CSeq_loc);
    if (loc_info.is_point || start == stop ) {
        // a point of some kind
        if (mix_set.empty())
           m_need_check_strand = true;
        else
           x_GetPointStrand(*sfp, strand);

        // note usage of orig_start instead of start 
        // because we want the first part of the point
        // specified in the file, not the smallest because SetRightOf
        // works differently for plus vs. minus strand
        CRef<CSeq_point> pPoint(
            new CSeq_point(*m_seq_id, orig_start, strand) );
        if( loc_info.is_point ) {
            // between two bases
            pPoint->SetRightOf (true);
            // warning if stop is not start plus one
            if( stop != (start+1) ) {
                x_ProcessMsg( 
                    ILineError::eProblem_BadFeatureInterval, eDiag_Warning,
                    strFeatureName );
            }
        } else {
            // just a point. do nothing
        }

        if (loc_info.is_5p_partial) {
            pPoint->SetPartialStart (true, eExtreme_Biological);
        }
        if (loc_info.is_3p_partial) {
            pPoint->SetPartialStop (true, eExtreme_Biological);
        }

        loc->SetPnt( *pPoint );
    } else {
        // interval
        CRef<CSeq_interval> pIval( new CSeq_interval(*m_seq_id, start, stop, strand) );
        if (loc_info.is_5p_partial) {
            pIval->SetPartialStart (true, eExtreme_Biological);
        }
        if (loc_info.is_3p_partial) {
            pIval->SetPartialStop (true, eExtreme_Biological);
        }
        loc->SetInt(*pIval);
        if (m_need_check_strand)
        {
            x_UpdatePointStrand(*sfp, strand);
            m_need_check_strand = false;
        }
    }

    // check for internal partials
    if( ! mix_set.empty() ) {
        const CSeq_loc & last_loc = *mix_set.back();
        if( last_loc.IsPartialStop(eExtreme_Biological) ||
            loc->IsPartialStart(eExtreme_Biological) ) 
        {
            // internal partials
            x_ProcessMsg(ILineError::eProblem_InternalPartialsInFeatLocation,
                eDiag_Warning, strFeatureName );
        }
    }

    mix_set.push_back(loc);


    if (loc_info.is_5p_partial || loc_info.is_3p_partial) {
        sfp->SetPartial (true);
    }

    return true;
}



bool CFeatureTableReader_Imp::x_SetupSeqFeat (
    CRef<CSeq_feat> sfp,
    const string& feat,
    const TFlags flags,
    ITableFilter *filter
)

{
    if (feat.empty ()) return false;

    // check filter, if any
    if( NULL != filter ) {
        ITableFilter::EAction action = filter->GetFeatAction(feat);
        if( action != ITableFilter::eAction_Okay ) {
            x_ProcessMsg( 
                ILineError::eProblem_FeatureNameNotAllowed,
                eDiag_Warning, feat );
            if( action == ITableFilter::eAction_Disallowed ) {
                return false;
            }
        }
    }

    CSeqFeatData::ESubtype sbtyp = CSeqFeatData::SubtypeNameToValue(feat);
    if (sbtyp != CSeqFeatData::eSubtype_bad) {

        // populate *sfp here...

        CSeqFeatData::E_Choice typ = CSeqFeatData::GetTypeFromSubtype (sbtyp);
        sfp->SetData ().Select (typ);
        CSeqFeatData& sfdata = sfp->SetData ();

        if (typ == CSeqFeatData::e_Rna) {
            CRNA_ref& rrp = sfdata.SetRna ();
            CRNA_ref::EType rnatyp = CRNA_ref::eType_unknown;
            switch (sbtyp) {
            case CSeqFeatData::eSubtype_preRNA :
                rnatyp = CRNA_ref::eType_premsg;
                break;
            case CSeqFeatData::eSubtype_mRNA :
                rnatyp = CRNA_ref::eType_mRNA;
                break;
            case CSeqFeatData::eSubtype_tRNA :
                rnatyp = CRNA_ref::eType_tRNA;
                break;
            case CSeqFeatData::eSubtype_rRNA :
                rnatyp = CRNA_ref::eType_rRNA;
                break;
            case CSeqFeatData::eSubtype_snRNA :
                rnatyp = CRNA_ref::eType_ncRNA;
                rrp.SetExt().SetGen().SetClass("snRNA");
                break;
            case CSeqFeatData::eSubtype_scRNA :
                rnatyp = CRNA_ref::eType_ncRNA;
                rrp.SetExt().SetGen().SetClass("scRNA");
                break;
            case CSeqFeatData::eSubtype_snoRNA :
                rnatyp = CRNA_ref::eType_ncRNA;
                rrp.SetExt().SetGen().SetClass("snoRNA");
                break;
            case CSeqFeatData::eSubtype_ncRNA :
                rnatyp = CRNA_ref::eType_ncRNA;
                rrp.SetExt().SetGen();
                break;
            case CSeqFeatData::eSubtype_tmRNA :
                rnatyp = CRNA_ref::eType_tmRNA;
                rrp.SetExt().SetGen();
                break;
            case CSeqFeatData::eSubtype_otherRNA :
                rrp.SetExt().SetName("misc_RNA");
                rnatyp = CRNA_ref::eType_other;
                break;
            default :
                break;
            }
            rrp.SetType (rnatyp);

        } else if (typ == CSeqFeatData::e_Imp) {
            CImp_feat_Base& imp = sfdata.SetImp ();
            imp.SetKey (feat);

        } else if (typ == CSeqFeatData::e_Bond) {
            sfdata.SetBond (CSeqFeatData::eBond_other);

        } else if (typ == CSeqFeatData::e_Site) {
            sfdata.SetSite (CSeqFeatData::eSite_other);
        } else if (typ == CSeqFeatData::e_Prot ) {
            CProt_ref &prot_ref = sfdata.SetProt();
            switch (sbtyp) {
                default:
                    break;
                case CSeqFeatData::eSubtype_mat_peptide_aa:
                    prot_ref.SetProcessed(CProt_ref::eProcessed_mature);
                    break;
                case CSeqFeatData::eSubtype_sig_peptide_aa:
                    prot_ref.SetProcessed(CProt_ref::eProcessed_signal_peptide);
                    break;
                case CSeqFeatData::eSubtype_preprotein:
                    prot_ref.SetProcessed(CProt_ref::eProcessed_preprotein);
                    break;
                case CSeqFeatData::eSubtype_transit_peptide_aa:
                    prot_ref.SetProcessed(CProt_ref::eProcessed_transit_peptide);
                    break;
                case CSeqFeatData::eSubtype_propeptide_aa:
                    prot_ref.SetProcessed(CProt_ref::eProcessed_propeptide);
                    break;
            }
        }

        // check for discouraged feature name
        if( (flags & CFeature_table_reader::fReportDiscouragedKey) != 0 ) {
            if( CSeqFeatData::IsDiscouragedSubtype(sbtyp) ) {
                x_ProcessMsg(
                    ILineError::eProblem_DiscouragedFeatureName,
                    eDiag_Warning, feat);
            }
        }

        return true;
    }

    // unrecognized feature key

    if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
        x_ProcessMsg(ILineError::eProblem_UnrecognizedFeatureName, eDiag_Warning, feat );
    }

    if ((flags & CFeature_table_reader::fTranslateBadKey) != 0) {

        sfp->SetData ().Select (CSeqFeatData::e_Imp);
        CSeqFeatData& sfdata = sfp->SetData ();
        CImp_feat_Base& imp = sfdata.SetImp ();
        imp.SetKey ("misc_feature");
        x_AddQualifierToFeature (sfp, kEmptyStr, "standard_name", feat, flags);

        return true;

    } else if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {

        sfp->SetData ().Select (CSeqFeatData::e_Imp);
        CSeqFeatData& sfdata = sfp->SetData ();
        CImp_feat_Base& imp = sfdata.SetImp ();
        imp.SetKey (feat);

        return true;
    }

    return false;
}

void CFeatureTableReader_Imp::x_ProcessMsg(
    ILineError::EProblem eProblem,
    EDiagSev eSeverity,
    const string& strFeatureName,
    const string& strQualifierName,
    const string& strQualifierValue,
    const string& strErrorMessage,
    const ILineError::TVecOfLines & vecOfOtherLines)
{
    x_ProcessMsg(m_reader ? m_reader->GetLineNumber() : m_LineNumber,
        eProblem, 
        eSeverity, 
        strFeatureName, 
        strQualifierName, 
        strQualifierValue, 
        strErrorMessage,
        vecOfOtherLines);
}


void CFeatureTableReader_Imp::x_ProcessMsg(
    int line_num,
    ILineError::EProblem eProblem,
    EDiagSev eSeverity,
    const string & strFeatureName,
    const string & strQualifierName,
    const string & strQualifierValue,
    const string& strErrorMessage,
    const ILineError::TVecOfLines & vecOfOtherLines )
{

    if (!m_pMessageListener) {
        return;
    }

    AutoPtr<CObjReaderLineException> pErr ( 
        CObjReaderLineException::Create(
        eSeverity, line_num, strErrorMessage, eProblem, m_real_seqid, strFeatureName,
        strQualifierName, strQualifierValue));
    ITERATE( ILineError::TVecOfLines, line_it, vecOfOtherLines ) {
        pErr->AddOtherLine(*line_it);
    }

    if (!m_pMessageListener->PutError(*pErr)) {
        pErr->Throw();
    }
}


void CFeatureTableReader_Imp::PutProgress(
    const CTempString& seq_id,
    const unsigned int line_number,
    ILineErrorListener* pListener) 
{
    if (!pListener) {
        return;
    }

    string msg = "Seq-id " + seq_id + ", line " + NStr::IntToString(line_number);
    pListener->PutProgress(msg); 
}


// helper for CFeatureTableReader_Imp::ReadSequinFeatureTable,
// just so we don't forget a step when we reset the feature
// 
void CFeatureTableReader_Imp::x_ResetFeat(CRef<CSeq_feat> & sfp, bool & curr_feat_intervals_done)
{
    m_need_check_strand = false;
    sfp.Reset(new CSeq_feat);
    //sfp->ResetLocation();
    curr_feat_intervals_done = false;
}

void CFeatureTableReader_Imp::x_GetPointStrand(const CSeq_feat& feat, CSeq_interval::TStrand& strand) const
{
    if (feat.IsSetLocation() && feat.GetLocation().IsMix())
    {
        const CSeq_loc& last = *feat.GetLocation().GetMix().Get().back();
        if (last.IsInt() && last.GetInt().IsSetStrand())
        {
            strand = last.GetInt().GetStrand();
        }
        else
        if (last.IsPnt() && last.GetPnt().IsSetStrand())
        {
            strand = last.GetPnt().GetStrand();
        }
    }
}

void CFeatureTableReader_Imp::x_UpdatePointStrand(CSeq_feat& feat, CSeq_interval::TStrand strand) const
{
    if (feat.IsSetLocation() && feat.GetLocation().IsMix())
    {

        for (auto pSeqLoc : feat.SetLocation().SetMix().Set()) {
            if (pSeqLoc->IsPnt()) {
                auto& seq_point = pSeqLoc->SetPnt();
                const auto old_strand = 
                    seq_point.IsSetStrand() ? 
                    seq_point.GetStrand() :
                    eNa_strand_plus;

                    seq_point.SetStrand(strand);
                    if (old_strand != strand) {
                        const bool is_5p_partial = seq_point.IsPartialStop(eExtreme_Biological);
                        const bool is_3p_partial = seq_point.IsPartialStart(eExtreme_Biological);
                        seq_point.SetPartialStart(is_5p_partial, eExtreme_Biological);
                        seq_point.SetPartialStop(is_3p_partial, eExtreme_Biological);  
                    }
            }
        }
    }    
}


void CFeatureTableReader_Imp::x_FinishFeature(CRef<CSeq_feat>& feat, 
                                              TFtable& ftable)
{
    if ( !feat || 
         feat.Empty() ||
         !feat->IsSetData() ||
         (feat->GetData().Which() == CSeqFeatData::e_not_set) )
    {
        return;
    }

    // Check for missing publication - RW-626
    if (feat->GetData().GetSubtype() == CSeqFeatData::eSubtype_pub &&
        (!feat->SetData().SetPub().IsSetPub() ||
          feat->SetData().SetPub().GetPub().Get().empty())) {
        const int line_number = m_reader->AtEOF() ?
                                m_reader->GetLineNumber() :
                                m_reader->GetLineNumber()-1;

        string msg = "Reference feature is empty. Skipping feature.";

        x_ProcessMsg(line_number,
                     ILineError::eProblem_IncompleteFeature,
                     eDiag_Warning, 
                     "Reference",
                     kEmptyStr,
                     kEmptyStr,
                     msg);
            return;
    }
        
    if (feat->IsSetLocation() && feat->GetLocation().IsMix())
    {

        if (feat->GetLocation().GetMix().Get().empty()) {
            // turn empty seqlocmix into a null seq-loc
            feat->SetLocation().SetNull();
        }
        else 
        if (feat->GetLocation().GetMix().Get().size() == 1) {
            // demote 1-part seqlocmixes to seq-loc with just that part
            CRef<CSeq_loc> keep_loc = *feat->SetLocation().SetMix().Set().begin();
            feat->SetLocation(*keep_loc);
        }
    }
    ftable.push_back(feat);
}



void CFeatureTableReader_Imp::x_ProcessQualifier(const string& qual_name,
                                                 const string& qual_val,
                                                 const string& feat_name,
                                                 CRef<CSeq_feat> feat, 
                                                 TFlags flags) 
{
    if (NStr::IsBlank(qual_name)) {
        return;
    }

    if (!feat) {
        if ( flags & CFeature_table_reader::fReportBadKey ) {
            x_ProcessMsg(ILineError::eProblem_QualifierWithoutFeature, 
                        eDiag_Warning, kEmptyStr, qual_name, qual_val);
        }
        return;
    }

    if (NStr::IsBlank(qual_val)) {
        if (sc_SingleKeys.find(qual_name.c_str()) != sc_SingleKeys.end()) {
            x_AddQualifierToFeature(feat, feat_name, qual_name, qual_val, flags);
        }
        else {
            x_ProcessMsg(ILineError::eProblem_QualifierBadValue, 
                         eDiag_Warning, feat_name, qual_name);
        }
        return;
    }

    // else qual_name and qual_val are not blank
    if (!x_AddQualifierToFeature(feat, feat_name, qual_name, qual_val, flags)) {
        if (flags & CFeature_table_reader::fReportBadKey) {
            x_ProcessMsg(ILineError::eProblem_UnrecognizedQualifierName,
                         eDiag_Warning, feat_name, qual_name, qual_val);
        }

        if (flags & CFeature_table_reader::fKeepBadKey) {
            x_AddGBQualToFeature(feat, qual_name, qual_val);
        }
    }
}



CRef<CSeq_annot> CFeatureTableReader_Imp::ReadSequinFeatureTable (
    const CTempString& in_seqid,
    const CTempString& in_annotname,
    const TFlags flags,
    ITableFilter *filter
)
{
    string feat, qual, qual_value;
    string curr_feat_name;
   // Int4 start, stop;
    //bool partial5, partial3, ispoint, isminus, 
         
    bool ignore_until_next_feature_key = false;
    Int4 offset = 0;
    SFeatLocInfo loc_info;

    CRef<CSeq_annot> sap(new CSeq_annot);

    TFtable& ftable = sap->SetData().SetFtable();
    const bool bIgnoreWebComments = 
        ( (flags & CFeature_table_reader::fIgnoreWebComments) != 0 );

    // if sequence ID is a list, use just one sequence ID string    
    x_InitId(in_seqid, flags);

    // Use this to efficiently find the best CDS for a prot feature
    // (only add CDS's for it to work right)
    CBestFeatFinder best_CDS_finder;

    // map feature types to features
    TChoiceToFeatMap choiceToFeatMap;

    CRef<CSeq_feat> sfp;
    // This is true once this feature should not
    // have any more intervals.
    // This allows us to catch errors like the following:
    //
    //
    //>Feature lcl|Seq1
    //1	1008	CDS
    //			gene    THE_GENE_NAME
    //50	200
    //			product THE_GENE_PRODUCT
    bool curr_feat_intervals_done = false;

    if (! in_annotname.empty ()) {
      CAnnot_descr& descr = sap->SetDesc ();
      CRef<CAnnotdesc> annot(new CAnnotdesc);
      annot->SetName (in_annotname);
      descr.Set().push_back (annot);
    }

    while ( !m_reader->AtEOF() ) {

        CTempString line = *++(*m_reader);

        if( m_reader->GetLineNumber() % 10000 == 0 &&
            m_reader->GetLineNumber() > 0 )
        {
            PutProgress(m_real_seqid, m_reader->GetLineNumber(), m_pMessageListener);
        }

        // skip empty lines.
        // if requested, also skip webcomment lines
        if( line.empty () || (bIgnoreWebComments && x_IsWebComment(line) ) ) {
            continue;
        }

        // if next line is a new feature table, return current sap
        CTempStringEx dummy1, dummy2;
        if( ParseInitialFeatureLine(line, dummy1, dummy2) ) {
            m_reader->UngetLine(); // we'll get this feature line the next time around
            break;
        } 

        if (line [0] == '[') {

            // try to parse it as an offset
            if( x_TryToParseOffset(line, offset) ) {
                // okay, known command
            } else {
                // warn for unknown square-bracket commands
                x_ProcessMsg( 
                    ILineError::eProblem_UnrecognizedSquareBracketCommand,
                    eDiag_Warning);
            }

        } else if ( s_LineIndicatesOrder(line) ) {

            // put nulls between feature intervals
            CRef<CSeq_loc> loc_with_nulls = s_LocationJoinToOrder( sfp->GetLocation() );
            // loc_with_nulls is unset if no change was needed
            if( loc_with_nulls ) {
                sfp->SetLocation( *loc_with_nulls );
            }

        } else if (x_ParseFeatureTableLine (line, loc_info, feat, qual, qual_value, offset)) {
      //  } else if (x_ParseFeatureTableLine (line, &start, &stop, &partial5, &partial3,
       //                                     &ispoint, &isminus, feat, qual, qual_value, offset)) {
/*
            SFeatLocInfo loc_info;
            loc_info.start_pos = start;
            loc_info.stop_pos = stop;
            loc_info.is_5p_partial = partial5;
            loc_info.is_3p_partial = partial3;
            loc_info.is_point = ispoint;
            loc_info.is_minus_strand = isminus;
            */
            // process line in feature table

            replace( qual_value.begin(), qual_value.end(), '\"', '\'' );

            if ((! feat.empty ()) && loc_info.start_pos >= 0 && loc_info.stop_pos >= 0) {

                // process start - stop - feature line

                x_FinishFeature(sfp, ftable);
                x_ResetFeat( sfp, curr_feat_intervals_done );

                if (x_SetupSeqFeat (sfp, feat, flags, filter)) {

                    // figure out type of feat, and store in map for later use
                    CSeqFeatData::E_Choice eChoice = CSeqFeatData::e_not_set;
                    if( sfp->CanGetData() ) {
                        eChoice = sfp->GetData().Which();
                    }
                    choiceToFeatMap.insert(
                        TChoiceToFeatMap::value_type(
                        eChoice, 
                        SFeatAndLineNum(sfp, m_reader->GetLineNumber())));

                    // if new feature is a CDS, remember it for later lookups
                    if( eChoice == CSeqFeatData::e_Cdregion ) {
                        best_CDS_finder.AddFeat( *sfp );
                    }
                    
                    // and add first interval
                    x_AddIntervalToFeature (curr_feat_name, sfp, loc_info);

                  //  x_AddIntervalToFeature (curr_feat_name, sfp,
                   //     start, stop, partial5, partial3, ispoint, isminus);

                    ignore_until_next_feature_key = false;

                    curr_feat_name = feat;

                } else {

                    // bad feature, set ignore flag

                    ignore_until_next_feature_key = true;
                }

            } else if (ignore_until_next_feature_key) {

                // bad feature was found before, so ignore 
                // qualifiers until next feature key

            } 
            else 
            if (loc_info.start_pos >= 0 && 
                loc_info.stop_pos >= 0 && 
                feat.empty () && 
                qual.empty () && 
                qual_value.empty ()) {

                if( curr_feat_intervals_done ) {
                    // the feat intervals were done, so it's an error for there to be more intervals
                    x_ProcessMsg(ILineError::eProblem_NoFeatureProvidedOnIntervals, eDiag_Error);
                    // this feature is in bad shape, so we ignore the rest of it
                    ignore_until_next_feature_key = true;
                    x_ResetFeat(sfp, curr_feat_intervals_done);
                } else if (sfp  &&  sfp->IsSetLocation()  &&  sfp->GetLocation().IsMix()) {
                    // process start - stop multiple interval line
                    x_AddIntervalToFeature (curr_feat_name, sfp, loc_info);
                                           // start, stop, partial5, partial3, ispoint, isminus);
                } else {
                    if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                        x_ProcessMsg(ILineError::eProblem_NoFeatureProvidedOnIntervals,
                            eDiag_Warning);
                    }
                }

            } else if (!NStr::IsBlank(qual)) {
              curr_feat_intervals_done = true;
              x_ProcessQualifier(qual, qual_value, curr_feat_name, sfp, flags);
            }   
            else if (!feat.empty()) {
                
                // unrecognized location

                // there should no more ranges for this feature
                // (although there still can be ranges for quals, of course).
                curr_feat_intervals_done = true;

                if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                    x_ProcessMsg( 
                        ILineError::eProblem_FeatureBadStartAndOrStop, eDiag_Warning,
                        feat );
                }
            }
        }
    }

    // make sure last feature is finished
    x_FinishFeature(sfp, ftable);
    x_ResetFeat( sfp, curr_feat_intervals_done );

    if ((flags & CFeature_table_reader::fCreateGenesFromCDSs) != 0 ||
        (flags & CFeature_table_reader::fCDSsMustBeInTheirGenes) != 0 ) 
    {
        x_CreateGenesFromCDSs(sap, choiceToFeatMap, flags);
    }

    if (sap->GetData().GetFtable().empty()) { 
        // An empty feature table implies bad input
        x_ProcessMsg(ILineError::eProblem_GeneralParsingError, eDiag_Error);
    }

    return sap;
}


CRef<CSeq_feat> CFeatureTableReader_Imp::CreateSeqFeat (
    const string& feat,
    CSeq_loc& location,
    const TFlags flags,
    const string &seq_id,
    ITableFilter *filter
)

{
    CRef<CSeq_feat> sfp (new CSeq_feat);

    sfp->ResetLocation ();

    if ( ! x_SetupSeqFeat (sfp, feat, flags, filter) ) {

        // bad feature, make dummy
        sfp->SetData ().Select (CSeqFeatData::e_not_set);
    }
    sfp->SetLocation (location);

    return sfp;
}

void CFeatureTableReader_Imp::x_InitId(const CTempString& seq_id, const TFlags flags)
{
    if (!NStr::IsBlank(seq_id)) {
        CBioseq::TId ids;
        CSeq_id::ParseIDs(ids, seq_id, 
            (flags & CFeature_table_reader::fAllIdsAsLocal) ? CSeq_id::fParse_AnyLocal : CSeq_id::fParse_Default);

        m_seq_id.Reset();
        if (flags & CFeature_table_reader::fPreferGenbankId)
        {
            for (auto id : ids)
            {
                if (id->IsGenbank())
                    m_seq_id = id;
            }
        };

        if (m_seq_id.Empty())
            m_seq_id = ids.front();

        m_real_seqid.clear();
        m_seq_id->GetLabel(&m_real_seqid, CSeq_id::eFasta);
    }
}

void CFeatureTableReader_Imp::AddFeatQual (
    CRef<CSeq_feat> sfp,
    const string& feat_name,
    const string& qual,
    const string& val,
    const TFlags flags,
    const string &seq_id1 )

{
    x_InitId(seq_id1, flags);

    if (NStr::IsBlank(qual)) {
        return;
    }

    if (!val.empty ()) { // Should probably use NStr::IsBlank()
        if (! x_AddQualifierToFeature (sfp, feat_name, qual, val, flags)) {
            // unrecognized qualifier key
            if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                ERR_POST_X (5, Warning << "Unrecognized qualifier '" << qual << "'");
            }
            if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {
                x_AddGBQualToFeature (sfp, qual, val);
            }
        }
    } 
    else { // empty val
        // check for the few qualifiers that do not need a value
        auto s_iter = sc_SingleKeys.find (qual.c_str ());
        if (s_iter != sc_SingleKeys.end ()) {
            x_AddQualifierToFeature (sfp, feat_name, qual, val, flags);
        }
    }
}

// static
bool CFeatureTableReader_Imp::ParseInitialFeatureLine (
    const CTempString& line_arg,
    CTempStringEx& out_seqid,
    CTempStringEx& out_annotname )
{
    out_seqid.clear();
    out_annotname.clear();

    // copy the line_arg because we can't edit line_arg itself
    CTempString line = line_arg;

    // handle ">"
    NStr::TruncateSpacesInPlace(line);
    if( ! NStr::StartsWith(line, ">") ) {
        return false;
    }
    line = line.substr(1); // remove '>'

    // handle "Feature"
    NStr::TruncateSpacesInPlace(line, NStr::eTrunc_Begin);
    const CTempString kFeatureStr("Feature");
    if( ! NStr::StartsWith(line, kFeatureStr, NStr::eNocase) ) {
        return false;
    }
    line = line.substr( kFeatureStr.length() ); // remove "Feature"

    // throw out any non-space characters at the beginning,
    // so we can, for example, handle ">Features" (note the "s")
    while( !line.empty() && !isspace(line[0])  ) {
        line = line.substr(1);
    }

    // extract seqid and annotname
    NStr::TruncateSpacesInPlace(line, NStr::eTrunc_Begin);
    NStr::SplitInTwo(line, " \t", out_seqid, out_annotname, NStr::fSplit_Tokenize);

    return true;
}


// public access functions

CFeature_table_reader::CFeature_table_reader(
    TReaderFlags fReaderFlags)
    : CReaderBase(fReaderFlags)
{
}

CFeature_table_reader::CFeature_table_reader(
    ILineReader& lr,
    ILineErrorListener* pErrors) : 
    CReaderBase(0), 
    m_pImpl(new CFeatureTableReader_Imp(&lr, 0, pErrors))
    {}

CRef<CSerialObject> 
CFeature_table_reader::ReadObject(
    ILineReader &lr, ILineErrorListener *pMessageListener)
{
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pMessageListener ).ReleaseOrNull() );
    return object;
}


CRef<CSeq_annot>
CFeature_table_reader::ReadSeqAnnot(
    ILineReader &lr, ILineErrorListener *pMessageListener)
{
    return ReadSequinFeatureTable(lr, 0, pMessageListener);
}


CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    CNcbiIstream& ifs,
    const string& seqid,
    const string& annotname,
    const TFlags flags,
    ILineErrorListener* pMessageListener,
    ITableFilter *filter
)
{
    CStreamLineReader reader(ifs);
    return ReadSequinFeatureTable(reader, seqid, annotname, flags, pMessageListener, filter);
}

CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    ILineReader& reader,
    const string& seqid,
    const string& annotname,
    const TFlags flags,
    ILineErrorListener* pMessageListener,
    ITableFilter *filter
)
{
    // just read features from 5-column table
    CFeatureTableReader_Imp impl(&reader, 0, pMessageListener);
    return impl.ReadSequinFeatureTable(seqid, annotname, flags, filter);
}

CRef<CSeq_annot> CFeature_table_reader::x_ReadFeatureTable(
    CFeatureTableReader_Imp& reader,
    const CTempString& seqid,
    const CTempString& annot_name,
    TFlags flags,
    ITableFilter* filter) {
    return reader.ReadSequinFeatureTable(seqid, annot_name, flags, filter);
}


CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    CNcbiIstream& ifs,
    const TFlags flags,
    ILineErrorListener* pMessageListener,
    ITableFilter *filter
)
{
    CStreamLineReader reader(ifs);
    return ReadSequinFeatureTable(reader, flags, pMessageListener, filter);
}


CRef<CSeq_annot> CFeature_table_reader::x_ReadFeatureTable(
        CFeatureTableReader_Imp& reader,
        const TFlags flags,
        ITableFilter* filter,
        const string& seqid_prefix)
{
    auto pLineReader = reader.GetLineReaderPtr();
    if (!pLineReader) {
        return CRef<CSeq_annot>();
    }


    CTempStringEx orig_seqid, annotname;
    // first look for >Feature line, extract seqid and optional annotname
    while (orig_seqid.empty () && !pLineReader->AtEOF() ) {
        CTempString line = *++(*pLineReader);
        if( ParseInitialFeatureLine(line, orig_seqid, annotname) ) {
            CFeatureTableReader_Imp::PutProgress(orig_seqid, 
                                                 pLineReader->GetLineNumber(), 
                                                 reader.GetErrorListenerPtr());
        }
    }

    string temp_seqid;
    if (seqid_prefix.empty()) {
        //seqid = orig_seqid;
    } else {
        if (orig_seqid.find('|') == string::npos)
            temp_seqid = seqid_prefix + orig_seqid;
        else
        if (NStr::StartsWith(orig_seqid, "lcl|"))
        {
            temp_seqid = seqid_prefix + orig_seqid.substr(4);
        }
        orig_seqid = temp_seqid;
    }
    return x_ReadFeatureTable(reader, orig_seqid, annotname, flags, filter);
}


CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    ILineReader& reader,
    const TFlags flags,
    ILineErrorListener* pMessageListener,
    ITableFilter* pFilter,
    const string& seqid_prefix
)
{
    CFeatureTableReader_Imp ftable_reader(&reader, 0, pMessageListener);
    return x_ReadFeatureTable(ftable_reader, flags, pFilter, seqid_prefix);
}


CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable(
    const TFlags flags,
    ITableFilter* pFilter,
    const string& seqid_prefix
)
{
    return x_ReadFeatureTable(*m_pImpl, flags, pFilter, seqid_prefix);
}


void CFeature_table_reader::ReadSequinFeatureTables(
    CNcbiIstream& ifs,
    CSeq_entry& entry,
    const TFlags flags,
    ILineErrorListener* pMessageListener,
    ITableFilter *filter
)
{
    CStreamLineReader reader(ifs);
    return ReadSequinFeatureTables(reader, entry, flags, pMessageListener, filter);
}

struct SCSeqidCompare
{
  inline
  bool operator()(const CSeq_id* left, const CSeq_id* right) const
  { 
     return *left < *right;
  };
};

void CFeature_table_reader::ReadSequinFeatureTables(
    ILineReader& reader,
    CSeq_entry& entry,
    const TFlags flags,
    ILineErrorListener* pMessageListener,
    ITableFilter *filter
)
{
    // let's use map to speedup matching on very large files, see SQD-1847
    map<const CSeq_id*, CRef<CBioseq>, SCSeqidCompare> seq_map;

    for (CTypeIterator<CBioseq> seqit(entry);  seqit;  ++seqit) {
        ITERATE (CBioseq::TId, seq_id, seqit->GetId()) {
            seq_map[seq_id->GetPointer()].Reset(&*seqit);
        }
    }

    CFeatureTableReader_Imp ftable_reader(&reader, 0, pMessageListener);
    while ( !reader.AtEOF() ) {
        auto annot =  x_ReadFeatureTable(ftable_reader, flags, filter);
        //CRef<CSeq_annot> annot = ReadSequinFeatureTable(reader, flags, pMessageListener, filter);
        if (entry.IsSeq()) { // only one place to go
            entry.SetSeq().SetAnnot().push_back(annot);
            continue;
        }
        _ASSERT(annot->GetData().IsFtable());
        if (annot->GetData().GetFtable().empty()) {
            continue;
        }
        // otherwise, take the first feature, which should be representative
        const CSeq_feat& feat    = *annot->GetData().GetFtable().front();
        const CSeq_id*   feat_id = feat.GetLocation().GetId();
        CBioseq*         seq     = NULL;
        _ASSERT(feat_id); // we expect a uniform sequence ID
        seq = seq_map[feat_id].GetPointer();
        if (seq) { // found a match
            seq->SetAnnot().push_back(annot);
        } else { // just package on the set
            ERR_POST_X(6, Warning
                       << "ReadSequinFeatureTables: unable to find match for "
                       << feat_id->AsFastaString());
            entry.SetSet().SetAnnot().push_back(annot);
        }
    }
}


CRef<CSeq_feat> CFeature_table_reader::CreateSeqFeat (
    const string& feat,
    CSeq_loc& location,
    const TFlags flags,
    ILineErrorListener* pMessageListener,
    unsigned int line_number,
    string *seq_id,
    ITableFilter *filter
)
{
    CFeatureTableReader_Imp impl(nullptr, line_number, pMessageListener);
    return impl.CreateSeqFeat (feat, location, flags, (seq_id ? *seq_id : string() ), filter);
}


void CFeature_table_reader::AddFeatQual (
    CRef<CSeq_feat> sfp,
    const string& feat_name,
    const string& qual,
    const string& val,
    const CFeature_table_reader::TFlags flags,
    ILineErrorListener* pMessageListener,
    int line_number, 	 
    const string &seq_id 
)

{
    CFeatureTableReader_Imp impl(nullptr, line_number, pMessageListener);
    impl.AddFeatQual (sfp, feat_name, qual, val, flags, seq_id) ;
}

bool
CFeature_table_reader::ParseInitialFeatureLine (
    const CTempString& line_arg,
    CTempStringEx& out_seqid,
    CTempStringEx& out_annotname )
{
     return CFeatureTableReader_Imp::ParseInitialFeatureLine(line_arg, out_seqid, out_annotname);
}


CFeature_table_reader::~CFeature_table_reader() {}

END_objects_SCOPE
END_NCBI_SCOPE
