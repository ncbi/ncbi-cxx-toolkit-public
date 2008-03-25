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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of Seq_feat
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include "validatorp.hpp"
#include "utilities.hpp"

#include <serial/serialbase.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Trna_ext.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_set.hpp>

#include <objects/general/Dbtag.hpp>

#include <util/static_set.hpp>

#include <algorithm>
#include <string>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// =============================================================================
//                                     Public
// =============================================================================


CValidError_feat::CValidError_feat(CValidError_imp& imp) :
    CValidError_base(imp),
    m_NumGenes(0),
    m_NumGeneXrefs(0)
{
}


CValidError_feat::~CValidError_feat(void)
{
}


void CValidError_feat::ValidateSeqFeat(const CSeq_feat& feat)
{
    if ( !feat.CanGetLocation() ) {
        PostErr(eDiag_Critical, eErr_SEQ_FEAT_MissingLocation,
            "The feature is missing a location", feat);
        return;
    }
    _ASSERT(feat.CanGetLocation());

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    m_Imp.ValidateSeqLoc(feat.GetLocation(), bsh, "Location", feat);
    x_ValidateSeqFeatLoc(feat);

    if ( feat.CanGetProduct() ) {
        ValidateSeqFeatProduct(feat.GetProduct(), feat);
    }
    
    ValidateFeatPartialness(feat);
    
    ValidateExcept(feat);
    
    ValidateSeqFeatData(feat.GetData(), feat);
    
    if (feat.CanGetDbxref ()) {
        m_Imp.ValidateDbxref (feat.GetDbxref (), feat);
    }
    
    if ( feat.CanGetComment() ) {
        ValidateFeatComment(feat.GetComment(), feat);
    }

    if ( feat.CanGetCit() ) {
        ValidateFeatCit(feat.GetCit(), feat);
    }

    const CGene_ref* gene_xref = feat.GetGeneXref();
    if ( gene_xref != 0  &&  !gene_xref->IsSuppressed() ) {
        ++m_NumGeneXrefs;
    }
}


// =============================================================================
//                                     Private
// =============================================================================


// static member initializations
const string s_PlastidTxt[] = {
  "",
  "",
  "chloroplast",
  "chromoplast",
  "",
  "",
  "plastid",
  "",
  "",
  "",
  "",
  "",
  "cyanelle",
  "",
  "",
  "",
  "apicoplast",
  "leucoplast",
  "proplastid",
  ""
};


static string s_LegalRepeatTypes[] = {
  "tandem", "inverted", "flanking", "terminal",
  "direct", "dispersed", "other"
};


static string s_LegalConsSpliceStrings[] = {
  "(5'site:YES, 3'site:YES)",
  "(5'site:YES, 3'site:NO)",
  "(5'site:YES, 3'site:ABSENT)",
  "(5'site:NO, 3'site:YES)",
  "(5'site:NO, 3'site:NO)",
  "(5'site:NO, 3'site:ABSENT)",
  "(5'site:ABSENT, 3'site:YES)",
  "(5'site:ABSENT, 3'site:NO)",
  "(5'site:ABSENT, 3'site:ABSENT)"
};


static bool s_IsLocRefSeqMrna(const CSeq_loc& loc, CScope& scope)
{
    CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
    if ( bsh ) {
        ITERATE (CBioseq::TId, it, bsh.GetBioseqCore()->GetId()) {
            if ( (*it)->IdentifyAccession() == CSeq_id::eAcc_refseq_mrna ) {
                return true;
            }
        }
    }

    return false;
}


static bool s_IsLocGEDL(const CSeq_loc& loc, CScope& scope)
{
    CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
    if ( bsh ) {
        ITERATE (CBioseq::TId, it, bsh.GetBioseqCore()->GetId()) {
            CSeq_id::EAccessionInfo acc_info = (*it)->IdentifyAccession();
            if ( acc_info == CSeq_id::eAcc_gb_embl_ddbj  ||
                 acc_info == CSeq_id::eAcc_local ) {
                return true;
            }
        }
    }

    return false;
}


// private member functions:

void CValidError_feat::ValidateSeqFeatData
(const CSeqFeatData& data,
 const CSeq_feat& feat)
{
    switch ( data.Which () ) {
    case CSeqFeatData::e_Gene:
        // Validate CGene_ref
        ValidateGene(data.GetGene (), feat);
        break;
    case CSeqFeatData::e_Cdregion:
        // Validate CCdregion
        ValidateCdregion(data.GetCdregion (), feat);
        break;
    case CSeqFeatData::e_Prot:
        // Validate CProt_ref
        ValidateProt(data.GetProt (), feat);
        break;
    case CSeqFeatData::e_Rna:
        // Validate CRNA_ref
        ValidateRna(data.GetRna (), feat);
        break;
    case CSeqFeatData::e_Pub:
        // Validate CPubdesc
        m_Imp.ValidatePubdesc(data.GetPub (), feat);
        break;
    case CSeqFeatData::e_Imp:
        // Validate CPubdesc
        ValidateImp(data.GetImp (), feat);
        break;
    case CSeqFeatData::e_Biosrc:
        // Validate CBioSource
        ValidateFeatBioSource(data.GetBiosrc(), feat);
        break;

    case CSeqFeatData::e_Org:
    case CSeqFeatData::e_Region:
    case CSeqFeatData::e_Seq:
    case CSeqFeatData::e_Comment:
    case CSeqFeatData::e_Bond:
    case CSeqFeatData::e_Site:
    case CSeqFeatData::e_Rsite:
    case CSeqFeatData::e_User:
    case CSeqFeatData::e_Txinit:
    case CSeqFeatData::e_Num:
    case CSeqFeatData::e_Psec_str:
    case CSeqFeatData::e_Non_std_residue:
    case CSeqFeatData::e_Het:
        break;

    default:
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidType,
            "Invalid SeqFeat type [" + CSeqFeatData::SelectionName(data.Which()) + "]",
            feat);
        break;
    }

    if ( !data.IsGene() ) {
        ValidateGeneXRef(feat);
    } else {
        ValidateOperon(feat);
    }
    
    if ( !data.IsBond() ) {
        ValidateBondLocs(feat);
    }
}


void CValidError_feat::ValidateSeqFeatProduct
(const CSeq_loc& prod, const CSeq_feat& feat)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetProduct());
    m_Imp.ValidateSeqLoc(feat.GetProduct(), bsh, "Product", feat);
    
    if ( IsOneBioseq(prod, m_Scope) ) {
        const CSeq_id& sid = GetId(prod, m_Scope);
    
        switch ( sid.Which() ) {
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            {
                const CTextseq_id* tsid = sid.GetTextseq_Id();
                if ( tsid != NULL ) {
                    if ( !tsid->CanGetAccession()  &&  tsid->CanGetName() ) {
                        if ( m_Imp.IsNucAcc(tsid->GetName()) ) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                                "Feature product should not use "
                                "Textseq-id 'name' slot", feat);
                        }
                    }
                }
            }
            break;
            
        default:
            break;
        }
    }
}


bool CValidError_feat::IsPlastid(int genome)
{
    if ( genome == CBioSource::eGenome_chloroplast  ||
         genome == CBioSource::eGenome_chromoplast  ||
         genome == CBioSource::eGenome_plastid      ||
         genome == CBioSource::eGenome_cyanelle     ||
         genome == CBioSource::eGenome_apicoplast   ||
         genome == CBioSource::eGenome_leucoplast   ||
         genome == CBioSource::eGenome_proplastid  ) { 
        return true;
    }

    return false;
}


bool CValidError_feat::IsOverlappingGenePseudo(const CSeq_feat& feat)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( grp  ) {
        return (grp->CanGetPseudo()  &&  grp->GetPseudo());
    }

    // !!! DEBUG {
    // For testing purposes. Remove when test is done.
    if ( m_Imp.AvoidPerfBottlenecks() ) {
        return false;
    }
    // }

    // check overlapping gene
    CConstRef<CSeq_feat> overlap = 
        GetOverlappingGene(feat.GetLocation(), *m_Scope);
    if ( overlap ) {
        if ( (overlap->CanGetPseudo()  &&  overlap->GetPseudo())  ||
             (overlap->GetData().GetGene().CanGetPseudo()  &&
              overlap->GetData().GetGene().GetPseudo()) ) {
            return true;
        }
    }

    return false;
}


bool CValidError_feat::SuppressCheck(const string& except_text)
{
    static string exceptions[] = {
        "ribosomal slippage",
        "artificial frameshift",
        "nonconsensus splice site"
    };

    for ( size_t i = 0; i < sizeof(exceptions) / sizeof(string); ++i ) {
    if ( NStr::FindNoCase(except_text, exceptions[i] ) != string::npos )
         return true;
    }
    return false;
}





unsigned char CValidError_feat::Residue(unsigned char res)
{
    return res == 255 ? '?' : res;
}


int CValidError_feat::CheckForRaggedEnd
(const CSeq_loc& loc, 
 const CCdregion& cdregion)
{
    size_t len = GetLength(loc, m_Scope);
    if ( cdregion.GetFrame() > CCdregion::eFrame_one ) {
        len -= cdregion.GetFrame() - 1;
    }

    int ragged = len % 3;
    if ( ragged > 0 ) {
        len = GetLength(loc, m_Scope);
        size_t last_pos = 0;

        CSeq_loc::TRange range = CSeq_loc::TRange::GetEmpty();
        ITERATE( CCdregion::TCode_break, cbr, cdregion.GetCode_break() ) {
            SRelLoc rl(loc, (*cbr)->GetLoc(), m_Scope);
            ITERATE (SRelLoc::TRanges, rit, rl.m_Ranges) {
                if ((*rit)->GetTo() > last_pos) {
                    last_pos = (*rit)->GetTo();
                }
            }
        }

        // allowing a partial codon at the end
        TSeqPos codon_length = range.GetLength();
        if ( (codon_length == 0 || codon_length == 1)  && 
            last_pos == len - 1 ) {
            ragged = 0;
        }
    }
    return ragged;
}


string CValidError_feat::MapToNTCoords
(const CSeq_feat& feat,
 const CSeq_loc& product,
 TSeqPos pos)
{
    string result;

    CSeq_point pnt;
    pnt.SetPoint(pos);
    pnt.SetStrand( GetStrand(product, m_Scope) );

    try {
        pnt.SetId().Assign(GetId(product, m_Scope));
    } catch (CObjmgrUtilException) {}

    CSeq_loc tmp;
    tmp.SetPnt(pnt);
    CRef<CSeq_loc> loc = ProductToSource(feat, tmp, 0, m_Scope);
    
    loc->GetLabel(&result);

    return result;
}


void CValidError_feat::ValidateFeatPartialness(const CSeq_feat& feat)
{
    static const string parterr[2] = { "PartialProduct", "PartialLocation" };
    static const string parterrs[4] = {
        "Start does not include first/last residue of sequence",
        "Stop does not include first/last residue of sequence",
        "Internal partial intervals do not include first/last residue of sequence",
        "Improper use of partial (greater than or less than)"
    };

    unsigned int  partial_prod = eSeqlocPartial_Complete, 
                  partial_loc  = eSeqlocPartial_Complete;

    bool is_partial = feat.CanGetPartial()  &&  feat.GetPartial();
    partial_loc  = SeqLocPartialCheck(feat.GetLocation(), m_Scope );
    if (feat.CanGetProduct ()) {
        partial_prod = SeqLocPartialCheck(feat.GetProduct (), m_Scope );
    }
    
    if ( (partial_loc  != eSeqlocPartial_Complete)  ||
         (partial_prod != eSeqlocPartial_Complete)  ||   
         is_partial ) {

        // a feature on a partial sequence should be partial -- it often isn't
        if ( !is_partial &&
            partial_loc != eSeqlocPartial_Complete  &&
            feat.GetLocation ().IsWhole() ) {
            PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                "On partial Bioseq, SeqFeat.partial should be TRUE", feat);
        }
        // a partial feature, with complete location, but partial product
        else if ( is_partial                        &&
            partial_loc == eSeqlocPartial_Complete  &&
            feat.CanGetProduct ()                   &&
            feat.GetProduct ().IsWhole()            &&
            partial_prod != eSeqlocPartial_Complete ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "When SeqFeat.product is a partial Bioseq, SeqFeat.location "
                "should also be partial", feat);
        }
        // gene on segmented set is now 'order', should also be partial
        else if ( feat.GetData ().IsGene ()  &&
            !is_partial                      &&
            partial_loc == eSeqlocPartial_Internal ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "Gene of 'order' with otherwise complete location should "
                "have partial flag set", feat);
        }
        // inconsistent combination of partial/complete product,location,partial flag - part 1
        else if (partial_prod == eSeqlocPartial_Complete  &&  feat.CanGetProduct()) {
            // if not local bioseq product, lower severity
            EDiagSev sev = eDiag_Warning;
            if ( IsOneBioseq(feat.GetProduct(), m_Scope) ) {
                const CSeq_id& prod_id = GetId(feat.GetProduct(), m_Scope);
                CBioseq_Handle prod =
                    m_Scope->GetBioseqHandleFromTSE(prod_id, m_Imp.GetTSE());
                if ( !prod ) {
                    sev = eDiag_Info;
                }
            }
                        
            string str("Inconsistent: Product= complete, Location= ");
            str += (partial_loc != eSeqlocPartial_Complete) ? "partial, " : "complete, ";
            str += "Feature.partial= ";
            str += is_partial ? "TRUE" : "FALSE";
            PostErr(sev, eErr_SEQ_FEAT_PartialsInconsistent, str, feat);
        }
        // inconsistent combination of partial/complete product,location,partial flag - part 2
        else if ( partial_loc == eSeqlocPartial_Complete  ||  !is_partial ) {
            string str("Inconsistent: ");
            if ( feat.CanGetProduct() ) {
                str += "Product= ";
                str += (partial_prod != eSeqlocPartial_Complete) ? "partial, " : "complete, ";
            }
            str += "Location= ";
            str += (partial_loc != eSeqlocPartial_Complete) ? "partial, " : "complete, ";
            str += "Feature.partial= ";
            str += is_partial ? "TRUE" : "FALSE";
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialsInconsistent, str, feat);
        }
        // 5' or 3' partial location giving unclassified partial product
        else if ( (((partial_loc & eSeqlocPartial_Start) != 0)  ||
                   ((partial_loc & eSeqlocPartial_Stop) != 0))  &&
                  ((partial_prod & eSeqlocPartial_Other) != 0)  &&
                  is_partial ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "5' or 3' partial location should not have unclassified"
                " partial in product molinfo descriptor", feat);
        }
        
        // may have other error bits set as well 
        unsigned int partials[2] = { partial_prod, partial_loc };
        for ( int i = 0; i < 2; ++i ) {
            unsigned int errtype = eSeqlocPartial_Nostart;

            for ( int j = 0; j < 4; ++j ) {
                if (partials[i] & errtype) {
                    if ( i == 1  &&  j < 2  &&  IsCDDFeat(feat) ) {
                        // supress warning
                    } else if ( i == 1  &&  j < 2  &&
                        IsPartialAtSpliceSite(feat.GetLocation(), errtype) ) {
                        PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + ": " + parterrs[j] + 
                            " (but is at consensus splice site)", feat);
                    } else if ( i == 1  &&  j < 2  &&
                        (feat.GetData().Which() == CSeqFeatData::e_Gene  ||
                        feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) &&
                         IsSameAsCDS(feat) ) {
                        PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + ": " + parterrs[j], feat);
                    } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion && j == 0) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + 
                            ": 5' partial is not at start AND is not at consensus splice site",
                            feat); 
                    } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion && j == 1) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + 
                            ": 3' partial is not at stop AND is not at consensus splice site",
                            feat);
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + ": " + parterrs[j], feat);
                    }
                }
                errtype <<= 1;
            }
        }
    }
}


void CValidError_feat::ValidateGene(const CGene_ref& gene, const CSeq_feat& feat)
{
    ++m_NumGenes;

    if ( (! gene.IsSetLocus()      ||  gene.GetLocus().empty())   &&
         (! gene.IsSetAllele()     ||  gene.GetAllele().empty())  &&
         (! gene.IsSetDesc()       ||  gene.GetDesc().empty())    &&
         (! gene.IsSetMaploc()     ||  gene.GetMaploc().empty())  &&
         (! gene.IsSetDb()         ||  gene.GetDb().empty())      &&
         (! gene.IsSetSyn()        ||  gene.GetSyn().empty())     &&
         (! gene.IsSetLocus_tag()  ||  gene.GetLocus_tag().empty()) ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneRefHasNoData,
                "There is a gene feature where all fields are empty", feat);
    }
    if ( gene.CanGetLocus()  &&  !gene.GetLocus().empty() ) {
        ITERATE (string, it, gene.GetLocus() ) {
            if ( isspace((unsigned char)(*it)) != 0 ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_LocusTagProblem,
                    "Gene locus_tag '" + gene.GetLocus() + 
                    "' should be a single word without any spaces", feat);
                break;
            }
        }         

    }
    if ( gene.CanGetDb () ) {
        m_Imp.ValidateDbxref(gene.GetDb(), feat);
    }
}


void CValidError_feat::ValidateCdregion (
    const CCdregion& cdregion, 
    const CSeq_feat& feat
) 
{
    bool pseudo = (feat.CanGetPseudo()  &&  feat.GetPseudo())  ||
        IsOverlappingGenePseudo(feat);
    
    bool transl_except = false;

    ITERATE (CSeq_feat::TQual, it, feat.GetQual()) {
        const CGb_qual& qual = **it;
        if ( qual.CanGetQual() ) {
            const string& key = qual.GetQual();
            if ( NStr::EqualNocase(key, "pseudo") ) {
                pseudo = true;
            } else if ( NStr::EqualNocase(key, "exception") ) {
                if ( !feat.CanGetExcept()  ||  !feat.GetExcept() ) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptInconsistent,
                        "Exception flag should be set in coding region", feat);
                }
            } else if ( NStr::EqualNocase(key, "transl_except") ) {
                transl_except = true;
            } else if ( NStr::EqualNocase(key, "codon") ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                    "Use the proper genetic code, if available, "
                    "or set transl_excepts on specific codons", feat);
            } else if ( NStr::EqualNocase(key, "protein_id") ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                    "protein_id should not be a gbqual on a CDS feature", feat);
            } else if ( NStr::EqualNocase(key, "transcript_id") ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                    "transcript_id should not be a gbqual on a CDS feature", feat);
            }
        }
    }
    if ( transl_except  &&  feat.CanGetExcept_text()  &&
         NStr::FindNoCase(feat.GetExcept_text(), "RNA editing") != NPOS ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExceptAndRnaEditing,
            "CDS has both RNA editing /exception and /transl_except qualifiers", feat);
    }
    
    bool conflict = cdregion.CanGetConflict()  &&  cdregion.GetConflict();
    if ( !pseudo  &&  !conflict ) {
        ValidateCdTrans(feat);
        ValidateSplice(feat, false);
    } else if ( conflict ) {
        ValidateCdConflict(cdregion, feat);
    }
    ValidateCdsProductId(feat);

    x_ValidateCdregionCodebreak(cdregion, feat);

    if ( cdregion.CanGetOrf()  &&  cdregion.GetOrf ()  &&
         feat.CanGetProduct () ) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_OrfCdsHasProduct,
            "An ORF coding region should not have a product", feat);
    }

    if ( pseudo && feat.CanGetProduct () ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PseudoCdsHasProduct,
            "A pseudo coding region should not have a product", feat);
    }
    
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    if ( bsh ) {
        CSeqdesc_CI diter (bsh, CSeqdesc::e_Source);
        if ( diter ) {
            const CBioSource& src = diter->GetSource();
            int biopgencode = src.GetGenCode();
            
            if ( cdregion.CanGetCode() ) {
                int cdsgencode = cdregion.GetCode().GetId();
                if ( biopgencode != cdsgencode ) {
                    int genome = 0;
                    
                    if ( src.CanGetGenome() ) {
                        genome = src.GetGenome();
                    }
                    
                    if ( IsPlastid(genome) ) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_GenCodeMismatch,
                            "Genetic code conflict between CDS (code " +
                            NStr::IntToString (cdsgencode) +
                            ") and BioSource.genome biological context (" +
                            s_PlastidTxt[genome] + ") (uses code 11)", feat);
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_GenCodeMismatch,
                            "Genetic code conflict between CDS (code " +
                            NStr::IntToString(cdsgencode) +
                            ") and BioSource (code " +
                            NStr::IntToString(biopgencode) + ")", feat);
                    }
                }
            }
        }
    }

    ValidateBothStrands(feat);
    ValidateBadGeneOverlap(feat);
    ValidateBadMRNAOverlap(feat);
    ValidateCommonCDSProduct(feat);
    ValidateCDSPartial(feat);
}


void CValidError_feat::x_ValidateCdregionCodebreak
(const CCdregion& cds,
 const CSeq_feat& feat)
{
    const CSeq_loc& feat_loc = feat.GetLocation();
    const CCode_break* prev_cbr = 0;

    ITERATE (CCdregion::TCode_break, it, cds.GetCode_break()) {
        const CCode_break& cbr = **it;
        const CSeq_loc& cbr_loc = cbr.GetLoc();
        ECompare comp = Compare(cbr_loc, feat_loc, m_Scope);
        if ( (comp != eContained) && (comp != eSame)) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_Range, 
                "Code-break location not in coding region", feat);
        }
        if ( prev_cbr != 0 ) {
            if ( Compare(cbr_loc, prev_cbr->GetLoc(), m_Scope) == eSame ) {
                string msg = "Multiple code-breaks at same location";
                string str;
                cbr_loc.GetLabel(&str);
                if ( !str.empty() ) {
                    msg += "[" + str + "]";
                }
                PostErr(eDiag_Error, eErr_SEQ_FEAT_DuplicateTranslExcept,
                   msg, feat);
            }
        }
        prev_cbr = &cbr;
    }
}


// non-pseudo CDS must have product
void CValidError_feat::ValidateCdsProductId(const CSeq_feat& feat)
{
    // bail if product exists
    if ( feat.CanGetProduct() ) {
        return;
    }
    // bail if pseudo
    bool pseudo = (feat.CanGetPseudo()  &&  feat.GetPseudo())  ||
        IsOverlappingGenePseudo(feat);
    if ( pseudo ) {
        return;
    }
    // bail if location has just stop
    if ( feat.CanGetLocation() ) {
        const CSeq_loc& loc = feat.GetLocation();
        if ( loc.IsPartialStart(eExtreme_Biological)  &&  !loc.IsPartialStop(eExtreme_Biological) ) {
            if ( GetLength(loc, m_Scope) <= 5 ) {
                return;
            }
        }
    }
    // supress in case of the appropriate exception
    if ( feat.CanGetExcept()  &&  feat.CanGetExcept_text()  &&
         !IsBlankString(feat.GetExcept_text()) ) {
        if ( NStr::Find(feat.GetExcept_text(),
            "rearrangement required for product") != NPOS ) {
           return;
        }
    }
    
    // non-pseudo CDS must have /product
    PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingCDSproduct,
        "Expected CDS product absent", feat);
}


void CValidError_feat::ValidateCdConflict
(const CCdregion& cdregion,
 const CSeq_feat& feat)
{
    CBioseq_Handle nuc  = m_Scope->GetBioseqHandle(feat.GetLocation());
    CBioseq_Handle prot = m_Scope->GetBioseqHandle(feat.GetProduct());
    
    // translate the coding region
    string transl_prot;
    try {
        CCdregion_translate::TranslateCdregion(
            transl_prot, 
            nuc, 
            feat.GetLocation(), 
            cdregion,
            false,   // do not include stop codons
            false);  // do not remove trailing X/B/Z
    } catch ( const runtime_error& ) {
    }

    CSeqVector prot_vec = prot.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    string prot_seq;
    prot_vec.GetSeqData(0, prot_vec.size(), prot_seq);

    if ( transl_prot.empty()  ||  prot_seq.empty()  ||  transl_prot == prot_seq ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_BadConflictFlag,
            "Coding region conflict flag should not be set", feat);
    } else {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ConflictFlagSet,
            "Coding region conflict flag is set", feat);
    }
}


void CValidError_feat::ValidateSplice(const CSeq_feat& feat, bool check_all)
{
    // !!! suppress if NCBISubValidate
    //if (GetAppProperty ("NcbiSubutilValidation") != NULL)
    //    return;

    bool report_errors = true, has_errors = false;

    // specific biological exceptions suppress check
    if ( feat.CanGetExcept()  &&  feat.GetExcept()  &&
         feat.CanGetExcept_text() ) {
        if ( SuppressCheck(feat.GetExcept_text()) ) {
            report_errors = false;
        }
    }
        
    size_t num_of_parts = 0;
    ENa_strand  strand = eNa_strand_unknown;

    // !!! The C version treated seq_loc equiv as one whereas the iterator
    // treats it as many. 
    const CSeq_loc& location = feat.GetLocation ();

    for (CSeq_loc_CI citer(location); citer; ++citer) {
        ++num_of_parts;
        if ( num_of_parts == 1 ) {  // first part
            strand = citer.GetStrand();
        } else {
            if ( strand != citer.GetStrand() ) {
                return;         //bail on mixed strand
            }
        }
    }

    if ( num_of_parts == 0 ) {
        return;
    }
    if ( !check_all  &&  num_of_parts == 1 ) {
        return;
    }
    
    bool partial_first = location.IsPartialStart(eExtreme_Biological);
    bool partial_last = location.IsPartialStop(eExtreme_Biological);


    size_t counter = 0;
    const CSeq_id* last_id = 0;
    CBioseq_Handle bsh;
    size_t seq_len = 0;
    CSeqVector seq_vec;
    string label;

    for (CSeq_loc_CI citer(location); citer; ++citer) {
        ++counter;

        const CSeq_id& seq_id = citer.GetSeq_id();
        if ( last_id == 0  ||  !last_id->Match(seq_id) ) {
            bsh = m_Scope->GetBioseqHandle(seq_id);
            if ( !bsh ) {
                break;
            }
            seq_vec = 
                bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,
                                 citer.GetStrand());
            seq_len = seq_vec.size();

            // get the label. if m_SuppressContext flag in true, 
            // get the worst label.
            const CBioseq& bsq = *bsh.GetCompleteBioseq();
            bsq.GetLabel(&label, CBioseq::eContent, m_Imp.IsSuppressContext());

            last_id = &seq_id;
        }

        TSeqPos acceptor = citer.GetRange().GetFrom();
        TSeqPos donor = citer.GetRange().GetTo();

        TSeqPos start = acceptor;
        TSeqPos stop = donor;

        if ( citer.GetStrand() == eNa_strand_minus ) {
            swap(acceptor, donor);
            stop = seq_len - donor - 1;
            start = seq_len - acceptor - 1;
        }

        // set severity level
        // genomic product set or NT_ contig always relaxes to SEV_WARNING
        EDiagSev sev = eDiag_Warning;
        if ( m_Imp.IsSpliceErr()                   &&
             !(m_Imp.IsGPS() || m_Imp.IsRefSeq())  &&
             !check_all ) {
            sev = eDiag_Error;
        }

        // check donor on all but last exon and on sequence
        if ( ((check_all && !partial_last)  ||  counter < num_of_parts)  &&
             (stop < seq_len - 2) ) {
            try {
                CSeqVector::TResidue res1 = seq_vec[stop + 1];    
                CSeqVector::TResidue res2 = seq_vec[stop + 2];

                if ( IsResidue(res1)  &&  IsResidue(res2) ) {
                    if ( (res1 != 'G')  || (res2 != 'T' ) ) {
                        has_errors = true;
                        string msg;
                        if ( (res1 == 'G')  && (res2 == 'C' ) ) { // GC minor splice site
                            sev = eDiag_Info;
                            msg = "Rare splice donor consensus (GC) found instead of "
                                "(GT) after exon ending at position " +
                                NStr::IntToString(donor + 1) + " of " + label;
                        } else {
                            msg = "Splice donor consensus (GT) not found after exon"
                                " ending at position " + 
                                NStr::IntToString(donor + 1) + " of " + label;
                        }
                        if (report_errors) {
                            PostErr(sev, eErr_SEQ_FEAT_NotSpliceConsensus, msg, feat);
                        }
                    }
                }
            } catch ( exception& ) {
                break;
            }
        }

        if ( ((check_all && !partial_first)  ||  counter != 1)  &&
             (start > 1) ) {
            try {
                CSeqVector::TResidue res1 = seq_vec[start - 2];
                CSeqVector::TResidue res2 = seq_vec[start - 1];
                
                if ( IsResidue(res1)  &&  IsResidue(res2) ) {
                    if ( (res1 != 'A')  ||  (res2 != 'G') ) {
                        has_errors = true;
                        if (report_errors) {
                            PostErr(sev, eErr_SEQ_FEAT_NotSpliceConsensus,
                                "Splice acceptor consensus (AG) not found before "
                                "exon starting at position " + 
                                NStr::IntToString(acceptor + 1) + " of " + label, feat);
                        }
                    }
                }
            } catch ( exception& ) {
            }
        }
    } // end of for loop

    if (!report_errors  &&  !has_errors) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
            "feature has exception but passes splice site test", feat);
    }
}


void CValidError_feat::ValidateProt(const CProt_ref& prot, const CSerialObject& obj) 
{
    CProt_ref::EProcessed processed = CProt_ref::eProcessed_not_set;

    if ( prot.CanGetProcessed() ) {
        processed = prot.GetProcessed();
    }

    bool empty = true;
    if ( processed != CProt_ref::eProcessed_signal_peptide  &&
         processed != CProt_ref::eProcessed_transit_peptide ) {
        if ( prot.CanGetName()  &&
            (!prot.GetName().empty()  ||  !prot.GetName().front().empty()) ) {
            empty = false;
        }
        if ( prot.CanGetDesc()  &&  !prot.GetDesc().empty() ) {
            empty = false;
        }
        if ( prot.CanGetEc()  &&  !prot.GetEc().empty() ) {
            empty = false;
        }
        if ( prot.CanGetActivity()  &&  !prot.GetActivity().empty() ) {
            empty = false;
        }
        if ( prot.CanGetDb()  &&  !prot.GetDb().empty() ) {
            empty = false;
        }

        if ( empty ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProtRefHasNoData,
                "There is a protein feature where all fields are empty", obj);
        }
    }
    if ( prot.CanGetDb () ) {
        m_Imp.ValidateDbxref(prot.GetDb(), obj);
    }
    if ( prot.CanGetDesc()  &&  !prot.GetDesc().empty()  &&
         prot.GetName().empty() ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
            "Protein feature has description but no name", obj);
    }
}


void CValidError_feat::ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat) 
{
    const CRNA_ref::EType& rna_type = rna.GetType ();

    if ( rna_type == CRNA_ref::eType_mRNA ) {
        bool pseudo = (feat.CanGetPseudo()  &&  feat.GetPseudo())  ||  IsOverlappingGenePseudo(feat);
        
        if ( !pseudo ) {
            ValidateMrnaTrans(feat);      /* transcription check */
            ValidateSplice(feat, false);
        }
        ValidateBothStrands(feat);
        ValidateBadGeneOverlap(feat);
        ValidateCommonMRNAProduct(feat);
    }

    if ( rna.CanGetExt()  &&
         rna.GetExt().Which() == CRNA_ref::C_Ext::e_TRNA ) {
        const CTrna_ext& trna = rna.GetExt ().GetTRNA ();
        if ( trna.CanGetAnticodon () ) {
            const CSeq_loc& anticodon = trna.GetAnticodon();
            size_t anticodon_len = 0;
            bool bad_anticodon = false;
            for ( CSeq_loc_CI it(anticodon); it; ++it ) {
                anticodon_len += GetLength(anticodon, m_Scope);
                ECompare comp = sequence::Compare(anticodon,
                                                  feat.GetLocation(),
                                                  m_Scope);
                if ( comp != eContained  &&  comp != eSame ) {
                    bad_anticodon = true;
                }
            }
            if ( bad_anticodon ) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_Range,
                    "Anticodon location not in tRNA", feat);
            }
            if ( anticodon_len != 3 ) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_Range,
                    "Anticodon is not 3 bases in length", feat);
            }
            ValidateAnticodon(anticodon, feat);
        }
        ValidateTrnaCodons(trna, feat);
    }

    if ( rna_type == CRNA_ref::eType_tRNA ) {
        ITERATE ( CSeq_feat::TQual, gbqual, feat.GetQual () ) {
            if ( NStr::CompareNocase((**gbqual).GetVal (), "anticodon") == 0 ) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Unparsed anticodon qualifier in tRNA", feat);
                break;
            }
        }
        /* tRNA with string extension */
        if ( rna.CanGetExt()  &&  
             rna.GetExt().Which () == CRNA_ref::C_Ext::e_Name ) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                "Unparsed product qualifier in tRNA", feat);
        }
    }

    if ( rna_type == CRNA_ref::eType_unknown ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_RNAtype0,
            "RNA type 0 (unknown) not supported", feat);
    }

    if ( feat.CanGetProduct() ) {
        ValidateRnaProductType(rna, feat);
    }
}


void CValidError_feat::ValidateAnticodon(const CSeq_loc& anticodon, const CSeq_feat& feat)
{
    bool ordered = true;
    bool adjacent = false;
    bool unmarked_strand = false;
    bool mixed_strand = false;

    CSeq_loc_CI prev;
    for (CSeq_loc_CI curr(anticodon); curr; ++curr) {
        if ( !curr.GetSeq_loc().IsInt()  &&  !curr.GetSeq_loc().IsPnt() ) {
            continue;
        }
        
        if ( prev  &&  curr  &&
             IsSameBioseq(curr.GetSeq_id(), prev.GetSeq_id(), m_Scope) ) {
            CSeq_loc_CI::TRange prev_range = prev.GetRange();
            CSeq_loc_CI::TRange curr_range = curr.GetRange();
            if ( ordered ) {
                if ( curr.GetStrand() == eNa_strand_minus ) {
                    if (prev_range.GetTo() < curr_range.GetTo()) {
                        ordered = false;
                    }
                    if (curr_range.GetTo() + 1 == prev_range.GetFrom()) {
                        adjacent = true;
                    }
                } else {
                    if (prev_range.GetTo() > curr_range.GetTo()) {
                        ordered = false;
                    }
                    if (prev_range.GetTo() + 1 == curr_range.GetFrom()) {
                        adjacent = true;
                    }
                }
            }
            ENa_strand curr_strand = curr.GetStrand();
            ENa_strand prev_strand = prev.GetStrand();
            if ( curr_range == prev_range  &&  curr_strand == prev_strand ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_DuplicateInterval,
                    "Duplicate anticodon exons in location", feat);
            }
            if ( curr_strand != prev_strand ) {
                if (curr_strand == eNa_strand_plus  &&  prev_strand == eNa_strand_unknown) {
                    unmarked_strand = true;
                } else if (curr_strand == eNa_strand_unknown  &&  prev_strand == eNa_strand_plus) {
                    unmarked_strand = true;
                } else {
                    mixed_strand = true;
                }
            }
        }
        prev = curr;
    }
    if (adjacent) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_AbuttingIntervals,
            "Adjacent intervals in Anticodon", feat);
    }

    // trans splicing exception turns off both mixed_strand and out_of_order messages
    bool trans_splice = false;
    if (feat.CanGetExcept()  &&  feat.GetExcept()  && feat.CanGetExcept_text()) {
        if (NStr::FindNoCase(feat.GetExcept_text(), "trans-splicing") != NPOS) {
            trans_splice = true;
        }
    }
    if (!trans_splice) {
        if (mixed_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                "Mixed strands in Anticodon", feat);
        }
        if (unmarked_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                "Mixed plus and unknown strands in Anticodon", feat);
        }
        if (!ordered) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqLocOrder,
                "Intervals out of order in Anticodon", feat);
        }
    }
}


void CValidError_feat::ValidateRnaProductType
(const CRNA_ref& rna,
 const CSeq_feat& feat)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( !bsh ) {
        return;
    }
    CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
    if ( !di ) {
        return;
    }
    const CMolInfo& mol_info = di->GetMolinfo();
    if ( !mol_info.CanGetBiomol() ) {
        return;
    }
    int biomol = mol_info.GetBiomol();
    
    switch ( rna.GetType() ) {

    case CRNA_ref::eType_mRNA:
        if ( biomol == CMolInfo::eBiomol_mRNA ) {
            return;
        }        
        break;

    case CRNA_ref::eType_tRNA:
        if ( biomol == CMolInfo::eBiomol_tRNA ) {
            return;
        }
        break;

    case CRNA_ref::eType_rRNA:
        if ( biomol == CMolInfo::eBiomol_rRNA ) {
            return;
        }
        break;

    default:
        return;
    }

    PostErr(eDiag_Error, eErr_SEQ_FEAT_RnaProductMismatch,
        "Type of RNA does not match MolInfo of product Bioseq", feat);
}


int s_LegalNcbieaaValues[] = { 42, 65, 66, 67, 68, 69, 70, 71, 72, 73,
                               75, 76, 77, 78, 80, 81, 82, 83, 84, 85,
                               86, 87, 88, 89, 90 };

void CValidError_feat::ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat)
{
    if (!trna.IsSetAa()) {
        return;
    }

    // Make sure AA coding is ncbieaa.
    if ( trna.GetAa().Which() != CTrna_ext::C_Aa::e_Ncbieaa ) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_TrnaCodonWrong,
            "tRNA AA coding does not match ncbieaa", feat);
        return;
    }

    unsigned char aa = trna.GetAa().GetNcbieaa();

    // make sure the amino acid is valid
    bool found = false;
    for ( int i = 0; i < 25; ++i ) {
        if ( aa == s_LegalNcbieaaValues[i] ) {
            found = true;
            break;
        }
    }
    if ( !found ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_BadTrnaAA, 
            "Invalid tRNA amino acid (" + NStr::IntToString(aa) + ")", feat);
        return;
    }

    // Retrive the Genetic code id for the tRNA
    int gcode = 0;
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    if ( !bsh ) {
        gcode = 1;
    } else {
        // need only the closest biosoure.
        CSeqdesc_CI diter(bsh, CSeqdesc::e_Source);
        if ( diter ) {
            gcode = diter->GetSource().GetGenCode();
        }
    }
    
    const string& ncbieaa = CGen_code_table::GetNcbieaa(gcode);
    if ( ncbieaa.length() != 64 ) {
        return;
    }
    
    EDiagSev sev = (aa == 'U') ? eDiag_Warning : eDiag_Error;

    bool modified_codon_recognition = false;
    if ( feat.CanGetExcept_text()  &&
         NStr::FindNoCase(feat.GetExcept_text(), "modified codon recognition") != NPOS ) {
        modified_codon_recognition = true;
    }

    ITERATE( CTrna_ext::TCodon, iter, trna.GetCodon() ) {
        // test that codon value is in range 0 - 63
        if ( *iter < 0  ||  *iter > 63 ) {
            PostErr(sev, eErr_SEQ_FEAT_TrnaCodonWrong,
                "tRNA codon value (" + NStr::IntToString(*iter) + 
                ") out of range", feat);
            continue;
        }

        if ( !modified_codon_recognition ) {
            unsigned char taa = ncbieaa[*iter];
            if ( taa != aa ) {
                if ( (aa == 'U')  &&  (taa == '*')  &&  (*iter == 14) ) {
                    // selenocysteine normally uses TGA (14), so ignore without requiring exception in record
                    // TAG (11) is used for pyrrolysine in archaebacteria
                    // TAA (10) is not yet known to be used for an exceptional amino acid
                } else {
                    PostErr(sev, eErr_SEQ_FEAT_TrnaCodonWrong,
                        "tRNA codon does not match genetic code", feat);
                }
            }
        }
    }
}


void CValidError_feat::ValidateImp(const CImp_feat& imp, const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();
    const string& key = imp.GetKey();

    switch ( subtype ) {
    case CSeqFeatData::eSubtype_exon:
        if ( m_Imp.IsValidateExons() ) {
            ValidateSplice(feat, true);
        }
        break;

    case CSeqFeatData::eSubtype_bad:
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey, 
            "Unknown feature key " + key, feat);
        break;

    case CSeqFeatData::eSubtype_virion:
    case CSeqFeatData::eSubtype_mutation:
    case CSeqFeatData::eSubtype_allele:
        PostErr(eDiag_Error, eErr_SEQ_FEAT_UnknownImpFeatKey,
            "Feature key " + key + " is no longer legal", feat);
        break;

    case CSeqFeatData::eSubtype_polyA_site:
        {
            CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
            if ( range.GetFrom() != range.GetTo() ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_PolyAsiteNotPoint,
                    "PolyA_site should be a single point", feat);
            }
        }
        break;

    case CSeqFeatData::eSubtype_mat_peptide:
    case CSeqFeatData::eSubtype_sig_peptide:
    case CSeqFeatData::eSubtype_transit_peptide:
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidForType,
            "Peptide processing feature should be converted to the "
            "appropriate protein feature subtype", feat);
        ValidatePeptideOnCodonBoundry(feat, key);
        break;
        
    case CSeqFeatData::eSubtype_mRNA:
    case CSeqFeatData::eSubtype_tRNA:
    case CSeqFeatData::eSubtype_rRNA:
    case CSeqFeatData::eSubtype_snRNA:
    case CSeqFeatData::eSubtype_scRNA:
    case CSeqFeatData::eSubtype_snoRNA:
    case CSeqFeatData::eSubtype_misc_RNA:
    case CSeqFeatData::eSubtype_precursor_RNA:
    // !!! what about other RNA types (preRNA, otherRNA)?
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
              "RNA feature should be converted to the appropriate RNA feature "
              "subtype, location should be converted manually", feat);
        break;

    case CSeqFeatData::eSubtype_Imp_CDS:
        {
            // impfeat CDS must be pseudo; fail if not
            bool pseudo = (feat.CanGetPseudo()  &&  feat.GetPseudo())  ||
                          IsOverlappingGenePseudo(feat);
            if ( !pseudo ) {
                PostErr(eDiag_Info, eErr_SEQ_FEAT_ImpCDSnotPseudo,
                    "ImpFeat CDS should be pseudo", feat);
            }

            ITERATE( CSeq_feat::TQual, gbqual, feat.GetQual() ) {
                if ( NStr::CompareNocase( (*gbqual)->GetQual(), "translation") == 0 ) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpCDShasTranslation,
                        "ImpFeat CDS with /translation found", feat);
                }
            }
        }
        break;

    default:
        break;
    }// end of switch statement  
    
    // validate the feature's location
    if ( imp.CanGetLoc() ) {
        const string& imp_loc = imp.GetLoc();
        if ( imp_loc.find("one-of") != string::npos ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpFeatBadLoc,
                "ImpFeat loc " + imp_loc + 
                " has obsolete 'one-of' text for feature " + key, feat);
        } else if ( feat.GetLocation().IsInt() ) {
            const CSeq_interval& seq_int = feat.GetLocation().GetInt();
            string temp_loc = NStr::IntToString(seq_int.GetFrom() + 1) +
                ".." + NStr::IntToString(seq_int.GetTo() + 1);
            if ( imp_loc != temp_loc ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpFeatBadLoc,
                    "ImpFeat loc " + imp_loc + " does not equal feature location " +
                    temp_loc + "for feature " + key, feat);
            }
        }
    }

    if ( feat.CanGetQual() ) {
        ValidateImpGbquals(imp, feat);
    }
    
    // Make sure a feature has its mandatory qualifiers
    bool found = false;
    switch (subtype) {
    case CSeqFeatData::eSubtype_conflict:
    case CSeqFeatData::eSubtype_old_sequence:
        if (!feat.IsSetCit()  &&  !NStr::IsBlank(feat.GetNamedQual("compare"))) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingQualOnImpFeat,
                "Feature " + key + " requires either /compare or /citation (or both)",
                feat);
        }
        break;
    default:
        ITERATE (CFeatQualAssoc::TGBQualTypeVec, required, CFeatQualAssoc::GetMandatoryGbquals(subtype)) {
            found = false;
            ITERATE(CSeq_feat::TQual, qual, feat.GetQual()) {
                if (CGbqualType::GetType(**qual) == *required) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingQualOnImpFeat,
                    "Missing qualifier " + CGbqualType::GetString(*required) +
                    " for feature " + key, feat);
            }
        }
        break;
    }
}


void CValidError_feat::ValidateImpGbquals
(const CImp_feat& imp,
 const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype ftype = feat.GetData().GetSubtype();
    const string& key = imp.GetKey();

    ITERATE( CSeq_feat::TQual, qual, feat.GetQual() ) {
        const string& qual_str = (*qual)->GetQual();

        if ( qual_str == "gsdb_id" ) {
            continue;
        }

        CGbqualType::EType gbqual = CGbqualType::GetType(qual_str);
        
        if ( gbqual == CGbqualType::e_Bad ) {
            if ( !qual_str.empty() ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                    "Unknown qualifier " + qual_str, feat);
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                    "Empty qualifier", feat);
            }
        } else {
            if ( !CFeatQualAssoc::IsLegalGbqual(ftype, gbqual) ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                    "Wrong qualifier " + qual_str + " for feature " + 
                    key, feat);
            }
            
            string val = (*qual)->GetVal();
            
            bool error = false;
            switch ( gbqual ) {
            case CGbqualType::e_Rpt_type:
                {{
                    for ( size_t i = 0; 
                    i < sizeof(s_LegalRepeatTypes) / sizeof(string); 
                    ++i ) {
                        if ( val.find(s_LegalRepeatTypes[i]) != string::npos ) {
                            bool left = false, right = false;
                            if ( i > 0 ) {
                                left = val[i-1] == ','  ||  val[i-1] == '(';
                            }
                            if ( i < val.length() - 1 ) {
                                right = val[i+1] == ','  ||  val[i+1] == ')';
                            }
                            if ( left  &&  right ) {
                                error = true;
                            }
                            break;
                        }
                    }
                }}
                break;
                
            case CGbqualType::e_Rpt_unit:
                {{
                    bool found = false, multiple_rpt_unit = true;
                    ITERATE(string, it, val) {
                        if ( *it <= ' ' ) {
                            found = true;
                        } else if ( *it == '('  ||  *it == ')'  ||
                            *it == ','  ||  *it == '.'  ||
                            isdigit((unsigned char)(*it)) ) {
                        } else {
                            multiple_rpt_unit = false;
                        }
                    }
                    /*
                    if ( found || 
                    (!multiple_rpt_unit && val.length() > 48) ) {
                    error = true;
                    }
                    */
                    if ( NStr::CompareNocase(key, "repeat_region") == 0  &&
                        !multiple_rpt_unit  &&
                        val.length() == GetLength(feat.GetLocation(), m_Scope) ) {
                        bool just_nuc_letters = true;
                        static const string nuc_letters = "ACGTNacgtn";
                        
                        ITERATE(string, it, val) {
                            if ( nuc_letters.find(*it) == NPOS ) {
                                just_nuc_letters = false;
                                break;
                            }
                        }
                        
                        if ( just_nuc_letters ) {
                            CSeqVector vec = GetSequenceFromFeature(feat, *m_Scope);
                            if ( !vec.empty() ) {
                                string vec_data;
                                vec.GetSeqData(0, vec.size(), vec_data);
                                if ( NStr::CompareNocase(val, vec_data) != 0 ) {
                                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                                        "repeat_region /rpt_unit and underlying"
                                        "sequence do not match", feat);
                                }
                            }
                            
                            
                        }
                    }
                }}
                break;
                
            case CGbqualType::e_Label:
                {{
                    bool only_digits = true,
                        has_spaces = false;
                    
                    ITERATE(string, it, val) {
                        if ( isspace((unsigned char)(*it)) ) {
                            has_spaces = true;
                        }
                        if ( !isdigit((unsigned char)(*it)) ) {
                            only_digits = false;
                        }
                    }
                    error = only_digits  ||  has_spaces;
                }}
                break;
                
            case CGbqualType::e_Cons_splice:
                {{
                    error = true;
                    for (size_t i = 0; 
                         i < sizeof(s_LegalConsSpliceStrings) / sizeof(string);
                         ++i) {
                        if ( NStr::CompareNocase(val, s_LegalConsSpliceStrings[i]) == 0 ) {
                            error = false;
                            break;
                        }
                    }
                }}
                break;

	        default:
	            break;
            } // end of switch statement
            if ( error ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    val + " is not a legal value for qualifier " + qual_str, feat);
            }
        }
    }  // end of ITERATE 
}


void CValidError_feat::ValidatePeptideOnCodonBoundry
(const CSeq_feat& feat, 
 const string& key)
{
    const CSeq_loc& loc = feat.GetLocation();

    // !!! DEBUG {
    if( m_Imp.AvoidPerfBottlenecks() ) {
        return;
    } 
    // } DEBUG

    CConstRef<CSeq_feat> cds = GetOverlappingCDS(loc, *m_Scope);
    if ( !cds ) {
        return;
    }
    const CCdregion& cdr = cds->GetData().GetCdregion();

    CSeq_loc_CI first, last;
    for ( CSeq_loc_CI sl_iter(loc); sl_iter; ++sl_iter ) {
        if ( !first ) {
            first = sl_iter;
        }
        last = sl_iter;
    }
        
    if ( !first  ||  !last ) {
        return;
    }

    TSeqPos pos1 = LocationOffset(loc, first.GetSeq_loc(), eOffset_FromStart);
    TSeqPos pos2 = LocationOffset(loc, last.GetSeq_loc(), eOffset_FromEnd);
    TSeqPos mod1 = (pos1 - cdr.GetFrame()) %3;
    TSeqPos mod2 = (pos2 - cdr.GetFrame()) %3;

    if ( loc.IsPartialStart(eExtreme_Biological) ) {
        mod1 = 0;
    }
    if ( loc.IsPartialStop(eExtreme_Biological) ) {
        mod2 = 2;
    }

    if ( (mod1 != 0)  &&  (mod2 != 2) ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
            "Start and stop of " + key + " are out of frame with CDS codons",
            feat);
    } else if (mod1 != 0) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame, 
            "Start of " + key + " is out of frame with CDS codons", feat);
    } else if (mod2 != 2) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
            "Stop of " + key + " is out of frame with CDS codons", feat);
    }
}


static const string sc_BypassMrnaTransCheckText[] = {
    "RNA editing",
    "artificial frameshift",
    "mismatches in transcription"
    "reasons given in citation",
    "unclassified transcription discrepancy",    
};
DEFINE_STATIC_ARRAY_MAP(CStaticArraySet<string>, sc_BypassMrnaTransCheck, sc_BypassMrnaTransCheckText);


void CValidError_feat::ValidateMrnaTrans(const CSeq_feat& feat)
{
    bool has_errors = false, unclassified_except = false,
        mismatch_except = false, report_errors = true;
    string farstr;

    if (feat.CanGetPseudo()  &&  feat.GetPseudo()) {
        return;
    }
    if (!feat.CanGetProduct()) {
        return;
    }

    if (feat.CanGetExcept()  &&  feat.GetExcept()  &&
        feat.CanGetExcept_text()) {
        const string& except_text = feat.GetExcept_text();
        ITERATE (CStaticArraySet<string>, it, sc_BypassMrnaTransCheck) {
            if (NStr::FindNoCase(except_text, *it) != NPOS) {
                report_errors = false;  // biological exception
            }
            if (NStr::FindNoCase(except_text, "unclassified transcription discrepancy") != NPOS) {
                unclassified_except = true;
            }
            if (NStr::FindNoCase(except_text, "mismatches in transcription") != NPOS) {
                mismatch_except = true;
                report_errors = true;
            }
        }
    }

    CConstRef<CSeq_id> product_id;
    try {
        product_id.Reset(&GetId(feat.GetProduct(), m_Scope));
    } catch (CException&) {
    }
    if (!product_id) {
        return;
    }
    
    CBioseq_Handle nuc = m_Scope->GetBioseqHandle(feat.GetLocation());
    if (!nuc) {
        has_errors = true;
        if (report_errors  ||  unclassified_except) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_MrnaTransFail,
                "Unable to transcribe mRNA", feat);
        };
    }
    if (nuc) {
        EDiagSev sev = eDiag_Error;
        CBioseq_Handle rna = m_Scope->GetBioseqHandleFromTSE(*product_id, nuc);
        if (!rna) {
            // if not local bioseq product, lower severity (with the exception of Refseq)
            sev = m_Imp.IsRefSeq() ? eDiag_Error : eDiag_Warning;
            if (m_Imp.IsFarFetchMRNAproducts()) {
                rna = m_Scope->GetBioseqHandle(feat.GetProduct());
            }
            if (!rna) {
                return;
            }
            farstr = "(far)";
        }
        _ASSERT(nuc  &&  rna);
    
        CSeqVector nuc_vec(feat.GetLocation(), *m_Scope,
                           CBioseq_Handle::eCoding_Iupac);
        CSeqVector rna_vec(feat.GetProduct(), *m_Scope,
                           CBioseq_Handle::eCoding_Iupac);

        size_t nuc_len = nuc_vec.size();
        size_t rna_len = rna_vec.size();

        if (nuc_len != rna_len) {
            has_errors = true;
            if (nuc_len < rna_len) {
                size_t count_a = 0, count_no_a = 0;
                // count 'A's in the tail
                for (CSeqVector_CI iter(rna_vec, nuc_len); iter; ++iter) {
                    if ((*iter == 'A')  ||  (*iter == 'a')) {
                        ++count_a;
                    } else {
                        ++count_no_a;
                    }
                }
                if (count_a < (19 * count_no_a)) { // less then 5%
                    if (report_errors) {
                        PostErr(sev, eErr_SEQ_FEAT_TranscriptLen,
                            "Transcript length [" + NStr::IntToString(nuc_len) + 
                            "] less than " + farstr + "product length [" +
                            NStr::IntToString(rna_len) + "], and tail < 95% polyA",
                            feat);
                    }
                } else {
                    if (report_errors) {
                        PostErr(eDiag_Info, eErr_SEQ_FEAT_TranscriptLen,
                            "Transcript length [" + NStr::IntToString(nuc_len) + 
                            "] less than " + farstr + "product length [" +
                            NStr::IntToString(rna_len) + "], but tail >= 95% polyA",
                            feat);
                    }
                }            
            } else {
                if (report_errors) {
                    PostErr(sev, eErr_SEQ_FEAT_TranscriptLen,
                        "Transcript length [" + NStr::IntToString(nuc_vec.size()) + "] " +
                        "greater than " + farstr +"product length [" + 
                        NStr::IntToString(rna_vec.size()) + "]", feat);
                }
            }
            // allow base-by-base comparison on common length
            rna_len = nuc_len = min(nuc_len, rna_len);
        }
        _ASSERT(nuc_len == rna_len);

        if (nuc_len > 0) {
            CSeqVector_CI nuc_ci(nuc_vec);
            CSeqVector_CI rna_ci(rna_vec);
            size_t mismatches = 0;

            // compare content of common length
            while ((nuc_ci  &&  rna_ci)  &&  (nuc_ci.GetPos() < nuc_len)) {
                if (*nuc_ci != *rna_ci) {
                    ++mismatches;
                }
                ++nuc_ci;
                ++rna_ci;
            }
            if (mismatches > 0) {
                has_errors = true;
                if (report_errors  &&  !mismatch_except) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_TranscriptMismatches,
                        "There are " + NStr::IntToString(mismatches) + 
                        " mismatches out of " + NStr::IntToString(nuc_len) +
                        " bases between the transcript and " + farstr + "product sequence",
                        feat);
                }
            }
        }
    }

    if (!report_errors  &&  !has_errors) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
            "mRNA has exception but passes transcription test", feat);
    }
}


void CValidError_feat::ValidateCommonMRNAProduct(const CSeq_feat& feat)
{
    if ( (feat.CanGetPseudo()  &&  feat.GetPseudo())  ||
         IsOverlappingGenePseudo(feat) ) {
        return;
    }

    if ( !feat.CanGetProduct() ) {
        return;
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( !bsh ) {
        const CSeq_id& sid = GetId(feat.GetProduct(), m_Scope);
        if ( sid.IsLocal() ) {
            if ( m_Imp.IsGPS()  ||  m_Imp.IsRefSeq() ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_MissingMRNAproduct,
                    "Product Bioseq of mRNA feature is not "
                    "packaged in the record", feat);
            }
        }
    } else {

        // !!! DEBUG {
        if( m_Imp.AvoidPerfBottlenecks() ) {
            return;
        } 
        // } DEBUG

        CFeat_CI mrna(
            bsh, SAnnotSelector(CSeqFeatData::e_Rna)
            .SetOverlapTotalRange()
            .SetResolveNone()
            .SetByProduct());
        while ( mrna ) {
            if ( &(mrna->GetOriginalFeature()) != &feat ) {
                    PostErr(eDiag_Critical, eErr_SEQ_FEAT_MultipleMRNAproducts,
                        "Same product Bioseq from multiple mRNA features", feat);
                    break;
            }
            ++mrna;
        }
    }
}


void CValidError_feat::ValidateBothStrands(const CSeq_feat& feat)
{
    const CSeq_loc& location = feat.GetLocation ();
    for ( CSeq_loc_CI citer (location); citer; ++citer ) {
        if ( citer.GetStrand () == eNa_strand_both ) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_BothStrands, 
                "mRNA or CDS may not be on both strands", feat);  
            break;
        }
    }
}


// Precondition: feat is a coding region
void CValidError_feat::ValidateCommonCDSProduct
(const CSeq_feat& feat)
{
    if ( (feat.CanGetPseudo()  &&  feat.GetPseudo())  ||
          IsOverlappingGenePseudo(feat) ) {
        return;
    }
    
    if ( !feat.CanGetProduct() ) {
        return;
    }
    
    const CCdregion& cdr = feat.GetData().GetCdregion();
    if ( cdr.CanGetOrf() ) {
        return;
    }

    CBioseq_Handle prod = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( !prod ) {
        const CSeq_id* sid = 0;
        try {
            sid = &(GetId(feat.GetProduct(), m_Scope));
        } catch (const CObjmgrUtilException&) {}

        // okay to have far RefSeq product, but only if genomic product set
        if ( sid == 0  ||  !sid->IsOther() ) {
            if ( m_Imp.IsGPS() ) {
                return;
            }
        }
        // or just a bioseq
        if ( m_Imp.GetTSE().IsSeq() ) {
            return;
        }

        // or in a standalone Seq-annot
        if ( m_Imp.IsStandaloneAnnot() ) {
            return;
        }

        PostErr(eDiag_Warning, eErr_SEQ_FEAT_MultipleCDSproducts,
            "Unable to find product Bioseq from CDS feature", feat);
        return;
    }
    CBioseq_Handle nuc  = m_Scope->GetBioseqHandle(feat.GetLocation());
    if ( nuc ) {
        CSeq_entry_Handle prod_nps = 
            prod.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
        CSeq_entry_Handle nuc_nps = 
            nuc.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);

        if ( (prod_nps != nuc_nps)  &&  (!m_Imp.IsNT())  &&  (!m_Imp.IsGPS()) ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_CDSproductPackagingProblem,
                "Protein product not packaged in nuc-prot set with nucleotide", 
                feat);
        }
    }
    const CSeq_feat* sfp = GetCDSForProduct(prod);
    if ( sfp == 0 ) {
        return;
    }
    
    if ( &feat != sfp ) {
        // if genomic product set, with one cds on contig and one on cdna,
        // do not report.
        if ( m_Imp.IsGPS() ) {
            // feature packaging test will do final contig vs. cdna check
            CBioseq_Handle sfh = m_Scope->GetBioseqHandle(sfp->GetLocation());
            if ( nuc != sfh ) {
                return;
            }
        }
        PostErr(eDiag_Critical, eErr_SEQ_FEAT_MultipleCDSproducts, 
            "Same product Bioseq from multiple CDS features", feat);
    }
}


void CValidError_feat::ValidateCDSPartial(const CSeq_feat& feat)
{
    if ( !feat.CanGetProduct()  ||  !feat.CanGetLocation() ) {
        return;
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( !bsh ) {
        return;
    }

    CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);
    if ( !sd ) {
        return;
    }
    const CMolInfo& molinfo = sd->GetMolinfo();

    const CSeq_loc& loc = feat.GetLocation();
    bool partial5 = loc.IsPartialStart(eExtreme_Biological);
    bool partial3 = loc.IsPartialStop(eExtreme_Biological);

    if ( molinfo.CanGetCompleteness() ) {
        switch ( molinfo.GetCompleteness() ) {
        case CMolInfo::eCompleteness_unknown:
            break;

        case CMolInfo::eCompleteness_complete:
            if ( partial5 || partial3 ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is partial but protein is complete", feat);
            }
            break;

        case CMolInfo::eCompleteness_partial:
            break;

        case CMolInfo::eCompleteness_no_left:
            if ( !partial5 ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 5' complete but protein is NH2 partial", feat);
            }
            if ( partial3 ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 3' partial but protein is NH2 partial", feat);
            }
            break;

        case CMolInfo::eCompleteness_no_right:
            if (! partial3) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 3' complete but protein is CO2 partial", feat);
            }
            if (partial5) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 5' partial but protein is CO2 partial", feat);
            }
            break;

        case CMolInfo::eCompleteness_no_ends:
            if ( partial5 && partial3 ) {
            } else if ( partial5 ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 5' partial but protein has neither end", feat);
            } else if ( partial3 ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 3' partial but protein has neither end", feat);
            } else {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is complete but protein has neither end", feat);
            }
            break;

        case CMolInfo::eCompleteness_has_left:
            break;

        case CMolInfo::eCompleteness_has_right:
            break;

        case CMolInfo::eCompleteness_other:
            break;

        default:
            break;
        }
    }
}


void CValidError_feat::ValidateBadMRNAOverlap(const CSeq_feat& feat)
{
    const CSeq_loc& loc = feat.GetLocation();

    // !!! DEBUG {
    if( m_Imp.AvoidPerfBottlenecks() ) {
        return;
    } 
    // } DEBUG

    CConstRef<CSeq_feat> mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Simple,
        *m_Scope);
    if ( !mrna ) {
        return;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_CheckIntRev,
        *m_Scope);
    if ( mrna ) {
        return;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Interval,
        *m_Scope);
    if ( !mrna ) {
        return;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Subset,
        *m_Scope);

    EDiagSev sev = eDiag_Error;
    if ( m_Imp.IsNC()  ||  m_Imp.IsNT()  ||  
         (feat.CanGetExcept()  &&  feat.GetExcept()) ) {
        sev = eDiag_Warning;
    }
    if ( mrna ) {
        // ribosomal slippage exception suppresses CDSmRNArange warning
        bool supress = false;

        if ( feat.CanGetExcept_text() ) {
            const CSeq_feat::TExcept_text& text = feat.GetExcept_text();
            if ( NStr::FindNoCase(text, "ribosomal slippage") != NPOS ) {
                supress = true;
            }
        }
        if ( !supress ) {
            PostErr(sev, eErr_SEQ_FEAT_CDSmRNArange,
                "mRNA contains CDS but internal intron-exon boundaries "
                "do not match", feat);
        }
    } else {
        PostErr(sev, eErr_SEQ_FEAT_CDSmRNArange,
            "mRNA overlaps or contains CDS but does not completely "
            "contain intervals", feat);
    }
}


void CValidError_feat::ValidateBadGeneOverlap(const CSeq_feat& feat)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( grp != 0 ) {
        return;
    }

    // !!! DEBUG {
    if( m_Imp.AvoidPerfBottlenecks() ) {
        return;
    } 
    // } DEBUG
    
    // look for overlapping genes
    if (GetOverlappingGene(feat.GetLocation(), *m_Scope)) {
        return;
    }

    // look for intersecting genes
    if (!GetBestOverlappingFeat(feat.GetLocation(),
                                  CSeqFeatData::e_Gene,
                                  eOverlap_Simple,
                                  *m_Scope)) {
        return;
    }

    // found an intersecting (but not overlapping) gene
    // set severity level
    EDiagSev sev = (m_Imp.IsNC()  ||  m_Imp.IsNT()) ? eDiag_Warning : eDiag_Error;

    // report error
    if (feat.GetData().IsCdregion()) {
        PostErr(sev, eErr_SEQ_FEAT_CDSgeneRange, 
            "gene overlaps CDS but does not completely contain it", feat);
    } else if (feat.GetData().IsRna()) {
        if (GetOverlappingOperon(feat.GetLocation(), *m_Scope)) {
            return;
        }
        PostErr(sev, eErr_SEQ_FEAT_mRNAgeneRange,
            "gene overlaps mRNA but does not completely contain it", feat);
    }
}


static const string s_LegalExceptionStrings[] = {
    "RNA editing",
    "reasons given in citation",
    "rearrangement required for product",
    "ribosomal slippage",
    "trans-splicing",
    "alternative processing",
    "artificial frameshift",
    "nonconsensus splice site",
    "modified codon recognition",
    "alternative start codon",
    "dicistronic gene",
    "transcribed product replaced",
    "translated product replaced",
    "transcribed pseudogene",
};


static const string s_RefseqExceptionStrings [] = {
    "unclassified transcription discrepancy",
    "unclassified translation discrepancy",
    "mismatches in transcription",
    "mismatches in translation",
    "adjusted for low-quality genome",
};


void CValidError_feat::ValidateExcept(const CSeq_feat& feat)
{
    if ( feat.CanGetExcept() &&  !feat.GetExcept()  &&
         feat.CanGetExcept_text()  &&  !feat.GetExcept_text().empty() ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptInconsistent,
            "Exception text is set, but exception flag is not set", feat);
    }
    if ( feat.CanGetExcept_text()  &&  !feat.GetExcept_text ().empty() ) {
        ValidateExceptText(feat.GetExcept_text(), feat);
    }
}


void CValidError_feat::ValidateExceptText(const string& text, const CSeq_feat& feat)
{
    if ( text.empty() ) return;

    EDiagSev sev = eDiag_Error;
    bool found = false;

    string str;
    string::size_type   begin = 0, end, textlen = text.length();

    const string* except_begin = s_LegalExceptionStrings;
    const string* except_end = 
        &(s_LegalExceptionStrings[sizeof(s_LegalExceptionStrings) / sizeof(string)]);
    const string* refseq_begin = s_RefseqExceptionStrings;
    const string* refseq_end = 
        &(s_RefseqExceptionStrings[sizeof(s_RefseqExceptionStrings) / sizeof(string)]);
    
    while ( begin < textlen ) {
        found = false;
        end = min( text.find_first_of(',', begin), textlen );
        str = NStr::TruncateSpaces( text.substr(begin, end) );
        if ( find(except_begin, except_end, str) != except_end ) {
            found = true;
        }
        if ( !found  &&  (m_Imp.IsGPS() || m_Imp.IsRefSeq()) ) {
            if ( find(refseq_begin, refseq_end, str) != refseq_end ) {
               found = true;
            }
        }
        if ( !found ) {
            if ( m_Imp.IsNC()  ||  m_Imp.IsNT() ) {
                sev = eDiag_Warning;
            }
            PostErr(sev, eErr_SEQ_FEAT_InvalidQualifierValue,
                str + " is not legal exception explanation", feat);
        }
        begin = end + 1;
    }
}




void CValidError_feat::ReportCdTransErrors
(const CSeq_feat& feat,
 bool show_stop,
 bool got_stop, 
 bool no_end,
 int ragged,
 bool report_errors,
 bool& has_errors)
{
    if (show_stop) {
        if (!got_stop  && !no_end) {
            has_errors = true;
            if (report_errors) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_NoStop, 
                    "Missing stop codon", feat);
            }
        } else if (got_stop  &&  no_end) {
            has_errors = true;
            if (report_errors) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "Got stop codon, but 3'end is labeled partial", feat);
            }
        } else if (got_stop  &&  !no_end  &&  ragged) {
            has_errors = true;
            if (report_errors) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_TransLen, 
                    "Coding region extends " + NStr::IntToString(ragged) +
                    " base(s) past stop codon", feat);
            }
        }
    }
}


static const string sc_BypassCdsTransCheckText[] = {
  "RNA editing",
  "artificial frameshift",
  "mismatches in translation",
  "rearrangement required for product",
  "reasons given in citation",
  "unclassified translation discrepancy"  
};
DEFINE_STATIC_ARRAY_MAP(CStaticArraySet<string>, sc_BypassCdsTransCheck, sc_BypassCdsTransCheckText);


static void s_LocIdType(const CSeq_loc& loc, CScope& scope, const CSeq_entry& tse,
                        bool& is_nt, bool& is_ng, bool& is_nc)
{
    is_nt = is_ng = is_nc = false;
    if (!IsOneBioseq(loc, &scope)) {
        return;
    }
    const CSeq_id& id = GetId(loc, &scope);
    CBioseq_Handle bsh = scope.GetBioseqHandleFromTSE(id, tse);
    if (bsh) {
        ITERATE (CBioseq_Handle::TId, it, bsh.GetId()) {
            CSeq_id::EAccessionInfo info = it->GetSeqId()->IdentifyAccession();
            is_nt = (info == CSeq_id::eAcc_refseq_contig);
            is_ng = (info == CSeq_id::eAcc_refseq_genomic);
            is_nc = (info == CSeq_id::eAcc_refseq_chromosome);
        }
    }
}


void CValidError_feat::ValidateCdTrans(const CSeq_feat& feat)
{
    // bail if no product exists or not CDS
    if (!feat.CanGetProduct()  ||  !feat.GetData().IsCdregion()) {
        return;
    }

    bool prot_ok = true;
    int  ragged = 0;
    bool has_errors = false, unclassified_except = false,
        mismatch_except = false, report_errors = true;
    string farstr;
    
    if (feat.CanGetExcept()  &&  feat.GetExcept()  &&
        feat.CanGetExcept_text()) {
        const string& except_text = feat.GetExcept_text();
        ITERATE (CStaticArraySet<string>, it, sc_BypassCdsTransCheck) {
            if (NStr::FindNoCase(except_text, *it) != NPOS) {
                report_errors = false;  // biological exception
            }
            if (NStr::FindNoCase(except_text, "unclassified transcription discrepancy") != NPOS) {
                unclassified_except = true;
            }
            if (NStr::FindNoCase(except_text, "mismatches in transcription") != NPOS) {
                mismatch_except = true;
                report_errors = true;
            }
        }
    }

    // pseudo gene
    ITERATE (CSeq_feat::TQual, it, feat.GetQual()) {
        if ((*it)->IsSetQual()  &&  NStr::EqualNocase((*it)->GetQual(), "pseudo")) {
            return;
        }
    }

    const CCdregion& cdregion = feat.GetData().GetCdregion();

    // check for unparsed transl_except
    bool transl_except = false;
    if (!cdregion.IsSetCode_break()) {
        ITERATE (CSeq_feat::TQual, it, feat.GetQual()) {
            if ((*it)->IsSetQual()  &&  NStr::Equal((*it)->GetQual(), "transl_except")) {
                transl_except = true;
            }
        }
    }
    
    const CSeq_loc& location = feat.GetLocation();
    const CSeq_loc& product = feat.GetProduct();
    
    int gc = 0;
    if ( cdregion.CanGetCode() ) {
        // We assume that the id is set for all Genetic_code
        gc = cdregion.GetCode().GetId();
    }
    string gccode = NStr::IntToString(gc);

    string transl_prot;   // translated protein
    bool alt_start = false;
    try {
        CCdregion_translate::TranslateCdregion(
            transl_prot, 
            feat, 
            *m_Scope,
            true,   // include stop codons
            false,  // do not remove trailing X/B/Z
            &alt_start);
    } catch (CException&) {
    }
    if (transl_prot.empty()) {
        if (report_errors) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_CdTransFail, 
                "Unable to translate", feat);
        }
    }

    // check alternative start codon
    if (alt_start  &&  gc == 1) {
        EDiagSev sev = eDiag_Warning;
        if (s_IsLocRefSeqMrna(feat.GetLocation(), *m_Scope)) {
            sev = eDiag_Error;
        } else if (s_IsLocGEDL(feat.GetLocation(), *m_Scope)) {
            sev = eDiag_Info;
        }
        if (sev != eDiag_Info  &&
            feat.CanGetExcept()  &&  feat.GetExcept()  &&
            feat.CanGetExcept_text()  &&  !feat.GetExcept_text().empty()) {
            if (feat.GetExcept_text().find("alternative start codon") != NPOS) {
                sev = eDiag_Info;
            }
        }
        if (sev != eDiag_Info) {
            has_errors = true;
            if (report_errors) {
                PostErr(sev, eErr_SEQ_FEAT_AltStartCodon,
                    "Alternative start codon used", feat);
            }
        }
    }

    bool no_end = false;
    unsigned int part_loc = SeqLocPartialCheck(location, m_Scope);
    unsigned int part_prod = SeqLocPartialCheck(product, m_Scope);
    if ( (part_loc & eSeqlocPartial_Stop)  ||
        (part_prod & eSeqlocPartial_Stop) ) {
        no_end = true;
    } else {    
        // complete stop, so check for ragged end
        ragged = CheckForRaggedEnd(location, cdregion);
    }
    
    // check for code break not on a codon
    has_errors = x_ValidateCodeBreakNotOnCodon(feat, location, cdregion, report_errors);
    
    if (cdregion.GetFrame() > CCdregion::eFrame_one) {
        has_errors = true;
        EDiagSev sev = s_IsLocRefSeqMrna(feat.GetLocation(), *m_Scope) ?
            eDiag_Error : eDiag_Warning;
        if (!(part_loc & eSeqlocPartial_Start)) {
            if (report_errors) {
                PostErr(sev, eErr_SEQ_FEAT_PartialProblem, 
                    "Suspicious CDS location - frame > 1 but not 5' partial",
                    feat);
            }
        } else if ((part_loc & eSeqlocPartial_Nostart)  &&
            !IsPartialAtSpliceSite(location, eSeqlocPartial_Nostart)) {
            if (report_errors) {
                PostErr(sev, eErr_SEQ_FEAT_PartialProblem, 
                    "Suspicious CDS location - frame > 1 and not at consensus splice site",
                    feat);
            }
        }
    }
    
    bool no_beg = 
        (part_loc & eSeqlocPartial_Start)  ||  (part_prod & eSeqlocPartial_Start);

    // count internal stops
    size_t internal_stop_count = 0;
    bool got_stop = false;
    if (!transl_prot.empty()) {
        bool got_dash = (transl_prot[0] == '-');
        got_stop = (transl_prot[transl_prot.length() - 1] == '*');
    
        ITERATE(string, it, transl_prot) {
            if ( *it == '*' ) {
                ++internal_stop_count;
            }
        }
        if (got_stop) {
            --internal_stop_count;
        }
    
        if (internal_stop_count > 0) {
            has_errors = true;
            if (got_dash) {
                if (report_errors  ||  unclassified_except) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                        "Illegal start codon and " + 
                        NStr::IntToString(internal_stop_count) +
                        " internal stops. Probably wrong genetic code [" +
                        gccode + "]", feat);
                }
            } else {
                if (report_errors  ||  unclassified_except) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_InternalStop, 
                        NStr::IntToString(internal_stop_count) + 
                        " internal stops. Genetic code [" + gccode + "]", feat);
                }
            }
            prot_ok = false;
            if (internal_stop_count > 5) {
                return;
            }
        } else if (got_dash) {
            has_errors = true;
            if (report_errors) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon, 
                    "Illegal start codon used. Wrong genetic code [" +
                    gccode + "] or protein should be partial", feat);
            }
        }
    }
    
    bool show_stop = true;

    const CSeq_id* protid = 0;
    try {
        protid = &GetId(product, m_Scope);
    } catch (CException&) {}
    CBioseq_Handle prot_handle;
    if (protid != NULL) {
        prot_handle = m_Scope->GetBioseqHandleFromTSE(*protid, m_Imp.GetTSE());
        if (!prot_handle  &&  m_Imp.IsFarFetchCDSproducts()) {
            prot_handle = m_Scope->GetBioseqHandle(*protid);
        }
    }

    if (!prot_handle) {
        if  (!m_Imp.IsStandaloneAnnot()) {
            if (transl_prot.length() > 6) {
                bool is_nt, is_ng, is_nc;
                s_LocIdType(location, *m_Scope, m_Imp.GetTSE(), is_nt, is_ng, is_nc);
                if (!(is_nt || is_ng)) {
                    EDiagSev sev = eDiag_Error;
                    if (IsDeltaOrFarSeg(location, m_Scope)) {
                        sev = eDiag_Warning;
                    }
                    if (is_nc) {
                        sev = eDiag_Warning;
                        if ( m_Imp.GetTSE().IsSeq()) {
                            sev = eDiag_Info;
                        }
                    }
                    if (sev != eDiag_Info) {
                        has_errors = true;
                        if (report_errors) {
                            PostErr(sev, eErr_SEQ_FEAT_NoProtein, 
                                "No protein Bioseq given", feat);
                        }
                    }
                }
            }
            ReportCdTransErrors(feat, show_stop, got_stop, no_end, ragged,
                report_errors, has_errors);
        }
        return;
    }

    CSeqVector prot_vec = prot_handle.GetSeqVector();
    prot_vec.SetCoding(CSeq_data::e_Ncbieaa);
    size_t prot_len = prot_vec.size(); 
    size_t len = transl_prot.length();

    if (got_stop  &&  (len == prot_len + 1)) { // ok, got stop
        --len;
    }

    // ignore terminal 'X' from partial last codon if present
    while (prot_len > 0) {
        if (prot_vec[prot_len - 1] == 'X') {  //remove terminal X
            --prot_len;
        } else {
            break;
        }
    }
    
    while (len > 0) {
        if (transl_prot[len - 1] == 'X') {  //remove terminal X
            --len;
        } else {
            break;
        }
    }

    vector<TSeqPos> mismatches;
    if (len == prot_len)  {                // could be identical
        for (TSeqPos i = 0; i < len; ++i) {
            if (transl_prot[i] != prot_vec[i]) {
                has_errors = true;
                mismatches.push_back(i);
                prot_ok = false;
            }
        }
    } else {
        has_errors = true;
        if (report_errors) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_TransLen,
                "Given protein length [" + NStr::IntToString(prot_len) + 
                "] does not match " + farstr + "translation length [" + 
                NStr::IntToString(len) + "]", feat);
        }
    }
    
    // Mismatch on first residue
    string msg;
    if (!mismatches.empty()  &&  mismatches.front() == 0) {
        if (feat.GetPartial() && (!no_beg) && (!no_end)) {
            if (report_errors) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem, 
                    "Start of location should probably be partial",
                    feat);
            }
        } else if (transl_prot[mismatches.front()] == '-') {
            if (report_errors) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                    "Illegal start codon used. Wrong genetic code [" +
                    gccode + "] or protein should be partial", feat);
            }
        }
    }

    char prot_res, transl_res;
    string nuclocstr;
    if (mismatches.size() > 10) {
        // report total number of mismatches and the details of the 
        // first and last.
        nuclocstr = MapToNTCoords(feat, product, mismatches.front());
        prot_res = prot_vec[mismatches.front()];
        transl_res = Residue(transl_prot[mismatches.front()]);
        msg = 
            NStr::IntToString(mismatches.size()) + " mismatches found. " +
            "First mismatch at " + NStr::IntToString(mismatches.front() + 1) +
            ", residue in protein [" + prot_res + "]" +
            " != translation [" + transl_res + "]";
        if (!nuclocstr.empty()) {
            msg += " at " + nuclocstr;
        }
        nuclocstr = MapToNTCoords(feat, product, mismatches.back());
        prot_res = prot_vec[mismatches.back()];
        transl_res = Residue(transl_prot[mismatches.back()]);
        msg += 
            ". Last mismatch at " + NStr::IntToString(mismatches.back() + 1) +
            ", residue in protein [" + prot_res + "]" +
            " != translation [" + transl_res + "]";
        if (!nuclocstr.empty()) {
            msg += " at " + nuclocstr;
        }
        msg += ". Genetic code [" + gccode + "]";
        if (report_errors  &&  !mismatch_except) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA, msg, feat);
        }
    } else {
        // report individual mismatches
        for (size_t i = 0; i < mismatches.size(); ++i) {
            nuclocstr = MapToNTCoords(feat, product, mismatches[i]);
            prot_res = prot_vec[mismatches[i]];
            transl_res = Residue(transl_prot[mismatches[i]]);
            msg += farstr + "Residue " + NStr::IntToString(mismatches[i] + 1) +
                " in protein [" + prot_res + "]" +
                " != translation [" + transl_res + "]";
            if (!nuclocstr.empty()) {
                msg += " at " + nuclocstr;
            }
            if (report_errors  &&  !mismatch_except) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA, msg, feat);
            }
        }
    }

    if (feat.CanGetPartial()  &&  feat.GetPartial()  &&
        mismatches.empty()) {
        if (!no_beg  && !no_end) {
            if (report_errors) {
                if (!got_stop) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem, 
                        "End of location should probably be partial", feat);
                } else {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                        "This SeqFeat should not be partial", feat);
                }
            }
            show_stop = false;
        }
    }

    ReportCdTransErrors(feat, show_stop, got_stop, no_end, ragged, 
        report_errors, has_errors);

    if (!prot_ok) {
        if (transl_except) {
            if (report_errors) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExcept,
                    "Unparsed transl_except qual. Skipped", feat);
            }
        }
    }

    if (!report_errors  &&  has_errors) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
            "CDS has exception but passes translation test", feat);
    }
}


bool CValidError_feat::x_ValidateCodeBreakNotOnCodon
(const CSeq_feat& feat,
 const CSeq_loc& loc, 
 const CCdregion& cdregion,
 bool report_errors)
{
    TSeqPos len = GetLength(loc, m_Scope);
    bool has_errors = false;

    ITERATE( CCdregion::TCode_break, cbr, cdregion.GetCode_break() ) {
        size_t codon_length = GetLength((*cbr)->GetLoc(), m_Scope);
        TSeqPos from = LocationOffset(loc, (*cbr)->GetLoc(), 
            eOffset_FromStart, m_Scope);
        TSeqPos to = from + codon_length - 1;
        
        // check for code break not on a codon
        if (codon_length == 3  ||
            ((codon_length == 1  ||  codon_length == 2)  &&  to == len - 1)) {
            size_t start_pos;
            switch (cdregion.GetFrame()) {
            case CCdregion::eFrame_two:
                start_pos = 1;
                break;
            case CCdregion::eFrame_three:
                start_pos = 2;
                break;
            default:
                start_pos = 0;
                break;
            }
            if ((from % 3) != start_pos) {
                if (report_errors) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExceptPhase,
                        "transl_except qual out of frame.", feat);
                }
                has_errors = true;
            }
        }
    }
    return has_errors;
}


// Check for redundant gene Xref
void CValidError_feat::ValidateGeneXRef(const CSeq_feat& feat)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( !grp  ||  grp->IsSuppressed() ) {
        return;
    }

    // !!! DEBUG {
    if( m_Imp.AvoidPerfBottlenecks() ) {
        return;
    } 
    // } DEBUG
    
    CConstRef<CSeq_feat> overlap = 
        GetOverlappingGene(feat.GetLocation(), *m_Scope);
    if ( !overlap ) {
        return;
    }
    
    const CGene_ref& gene = overlap->GetData().GetGene();
    if ( !gene.IsSuppressed() ) {
        if ( gene.CanGetAllele()  && !gene.GetAllele().empty() ) {
            const string& allele = gene.GetAllele();

            ITERATE(CSeq_feat::TQual, qual_iter, feat.GetQual()) {
                const CGb_qual& qual = **qual_iter;
                if ( qual.CanGetQual()  &&
                     NStr::Compare(qual.GetQual(), "allele") == 0 ) {
                    if ( qual.CanGetVal()  &&
                         NStr::CompareNocase(qual.GetVal(), allele) == 0 ) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                            "Redundant allele qualifier (" + allele + 
                            ") on gene and feature", feat);
                    } else if (feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_variation) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                            "Mismatched allele qualifier on gene (" + allele + 
                            ") and feature (" + qual.GetVal() +")", feat);
                    }
                }
            }
        }
    }

    string label, gene_label;
    grp->GetLabel(&label);
    gene.GetLabel(&gene_label);
    
    if ( NStr::CompareNocase(label, gene_label) == 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryGeneXref,
            "Unnecessary gene cross-reference " + label, feat);
    }
}


void CValidError_feat::ValidateOperon(const CSeq_feat& gene)
{
    CConstRef<CSeq_feat> operon = 
        GetOverlappingOperon(gene.GetLocation(), *m_Scope);
    if ( !operon  ||  !operon->CanGetQual() ) {
        return;
    }

    string label;
    feature::GetLabel(gene, &label, feature::eContent, m_Scope);
    if ( label.empty() ) {
        return;
    }

    ITERATE(CSeq_feat::TQual, qual_iter, gene.GetQual()) {
        const CGb_qual& qual = **qual_iter;
        if( qual.CanGetQual()  &&  qual.CanGetVal() ) {
            if ( NStr::Compare(qual.GetQual(), "operon") == 0  &&
                 NStr::CompareNocase(qual.GetVal(), label) ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Operon is same as gene - " + label, gene);
            }
        }
    }
}


bool CValidError_feat::IsPartialAtSpliceSite
(const CSeq_loc& loc,
 unsigned int tag)
{
    if ( tag != eSeqlocPartial_Nostart && tag != eSeqlocPartial_Nostop ) {
        return false;
    }

    CSeq_loc_CI first, last;
    for ( CSeq_loc_CI sl_iter(loc); sl_iter; ++sl_iter ) { // EQUIV_IS_ONE not supported
        if ( !first ) {
            first = sl_iter;
        }
        last = sl_iter;
    }

    if ( first.GetStrand() != last.GetStrand() ) {
        return false;
    }
    CSeq_loc_CI temp = (tag == eSeqlocPartial_Nostart) ? first : last;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(temp.GetSeq_id());
    if ( !bsh ) {
        return false;
    }
    
    TSeqPos acceptor = temp.GetRange().GetFrom();
    TSeqPos donor = temp.GetRange().GetTo();
    TSeqPos start = acceptor;
    TSeqPos stop = donor;

    CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,
        temp.GetStrand());
    TSeqPos len = vec.size();

    if ( temp.GetStrand() == eNa_strand_minus ) {
        swap(acceptor, donor);
        stop = len - donor - 1;
        start = len - acceptor - 1;
    }

    bool result = false;

    
    if ( (tag == eSeqlocPartial_Nostop)  &&  (stop < len - 2) ) {
        try {
            CSeqVector::TResidue res1 = vec[stop + 1];
            CSeqVector::TResidue res2 = vec[stop + 2];

            if ( IsResidue(res1)  &&  IsResidue(res2) ) {
                if ( (res1 == 'G'  &&  res2 == 'T')  || 
                    (res1 == 'G'  &&  res2 == 'C') ) {
                    result = true;
                }
            }
        } catch ( exception& ) {
            return false;
        }
    } else if ( (tag == eSeqlocPartial_Nostart)  &&  (start > 1) ) {
        try {
            CSeqVector::TResidue res1 = vec[start - 2];    
            CSeqVector::TResidue res2 = vec[start - 1];
        
            if ( IsResidue(res1)  &&  IsResidue(res2) ) {
                if ( (res1 == 'A')  &&  (res2 == 'G') ) { 
                    result = true;
                }
            }
        } catch ( exception& ) {
            return false;
        }
    }

    return result;    
}


void CValidError_feat::ValidateFeatCit
(const CPub_set& cit,
 const CSeq_feat& feat)
{
    if ( !feat.CanGetCit() ) {
        return;
    }

    if ( cit.IsPub() ) {
        ITERATE( CPub_set::TPub, pi, cit.GetPub() ) {
            if ( (*pi)->IsEquiv() ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryCitPubEquiv,
                    "Citation on feature has unexpected internal Pub-equiv",
                    feat);
                return;
            }
        }
    }
}

void CValidError_feat::ValidateFeatComment
(const string& comment,
 const CSeq_feat& feat)
{
    if ( m_Imp.IsSerialNumberInComment(comment) ) {
        PostErr(eDiag_Info, eErr_SEQ_FEAT_SerialInComment,
            "Feature comment may refer to reference by serial number - "
            "attach reference specific comments to the reference "
            "REMARK instead.", feat);
    }
}


void CValidError_feat::ValidateFeatBioSource
(const CBioSource& bsrc,
 const CSeq_feat& feat)
{
    if ( bsrc.CanGetIs_focus() ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_FocusOnBioSourceFeature,
            "Focus must be on BioSource descriptor, not BioSource feature.",
            feat);
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    if ( !bsh ) {
        return;
    }
    CSeqdesc_CI dbsrc_i(bsh, CSeqdesc::e_Source);
    if ( !dbsrc_i ) {
        return;
    }
    
    const COrg_ref& org = bsrc.GetOrg();           
    const CBioSource& dbsrc = dbsrc_i->GetSource();
    const COrg_ref& dorg = dbsrc.GetOrg(); 

    if ( org.CanGetTaxname()  &&  !org.GetTaxname().empty()  &&
            dorg.CanGetTaxname() ) {
        string taxname = org.GetTaxname();
        string dtaxname = dorg.GetTaxname();
        if ( NStr::CompareNocase(taxname, dtaxname) != 0 ) {
            if ( !dbsrc.CanGetIs_focus()  &&  !m_Imp.IsTransgenic(dbsrc) ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_BioSourceNeedsFocus,
                    "BioSource descriptor must have focus or transgenic "
                    "when BioSource feature with different taxname is "
                    "present.", feat);
            }
        }
    }
    
    ITERATE (CSeq_feat::TDbxref, it, feat.GetDbxref()) {
        if ((*it)->GetType() == CDbtag::eDbtagType_taxon) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_TaxonDbxrefOnFeature,
                "BioSource feature has taxon xref in common feature db_xref list",
                feat);
        }
    }

    m_Imp.ValidateBioSource(bsrc, feat);
}


void CValidError_feat::ValidateBondLocs(const CSeq_feat& feat)
{
    _ASSERT( ! feat.GetData().IsBond() );
    
    // Bond Seq_locs only allowed on Bond features.
    CSeq_loc_CI loc_ci(feat.GetLocation());
    for (; loc_ci; ++loc_ci) {
        if (loc_ci.GetSeq_loc().IsBond()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ImproperBondLocation,
                "Bond location should only be on bond features", feat);
            break;
        }
    }
}

// REQUIRES: feature is either Gene or mRNA
bool CValidError_feat::IsSameAsCDS(const CSeq_feat& feat)
{
    EOverlapType overlap_type;
    if ( feat.GetData().IsGene() ) {
        overlap_type = eOverlap_Simple;
    } else if ( feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA ) {
        overlap_type = eOverlap_CheckIntervals;
    } else {
        return false;
    }

    CConstRef<CSeq_feat> cds = GetBestOverlappingFeat(
            feat.GetLocation(),
            CSeqFeatData::e_Cdregion,
            overlap_type,
            *m_Scope);
        if ( cds ) {
            if ( TestForOverlap(
                    cds->GetLocation(),
                    feat.GetLocation(),
                    eOverlap_Simple) == 0 ) {
                return true;
            }
        }
    return false;
}


bool CValidError_feat::IsCDDFeat(const CSeq_feat& feat) const
{
    if ( feat.GetData().IsRegion() ) {
        if ( feat.CanGetDbxref() ) {
            ITERATE(CSeq_feat::TDbxref, db, feat.GetDbxref()) {
                if ( (*db)->CanGetDb()  &&
                    NStr::Compare((*db)->GetDb(), "CDD") == 0 ) {
                    return true;
                }
            }
        }
    }
    return false;
}

void CValidError_feat::x_ValidateSeqFeatLoc(const CSeq_feat& feat)
{
    // check for location on multiple near non-part bioseqs
    //CSeq_entry_Handle tse = m_Scope->GetSeq_entryHandle(m_Imp.GetTSE());
    CSeq_entry_Handle tse = m_Imp.GetTSEH();
    const CSeq_loc& loc = feat.GetLocation();
    if (IsOneBioseq(loc, m_Scope)) {
        return;
    }

    CBioseq_Handle bsh;
    for (CSeq_loc_CI it(loc) ;it; ++it) {
        CBioseq_Handle seq = m_Scope->GetBioseqHandleFromTSE(it.GetSeq_id(), tse);
        if (seq  &&  !seq.GetExactComplexityLevel(CBioseq_set::eClass_parts)) {
            if (bsh  &&  seq != bsh) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_MultipleBioseqs,
                    "Feature location refers to multiple near bioseqs.", feat);
            } else {
                bsh = seq;
            }
        }
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
