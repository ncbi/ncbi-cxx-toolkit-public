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
#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>

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
#include <objects/seq/seqport_util.hpp>

#include <objects/seq/seq_id_mapper.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_set.hpp>

#include <objects/general/Dbtag.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objtools/edit/cds_fix.hpp>

#include <util/static_set.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sgml_entity.hpp>

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
    CValidError_base(imp)
{
}


CValidError_feat::~CValidError_feat(void)
{
}


static bool s_IsValidECNumberFormat (const string& str)
{
    char     ch;
    bool     is_ambig;
    int      numdashes;
    int      numdigits;
    int      numperiods;
    const char *ptr;

    if (NStr::IsBlank (str)) {
        return false;
    }

    is_ambig = false;
    numperiods = 0;
    numdigits = 0;
    numdashes = 0;

    ptr = str.c_str();
    ch = *ptr;
    while (ch != '\0') {
        if (isdigit (ch)) {
            numdigits++;
            if (is_ambig) return false;
            ptr++;
            ch = *ptr;
        } else if (ch == '-') {
            numdashes++;
            is_ambig = true;
            ptr++;
            ch = *ptr;
        } else if (ch == 'n') {
            if (numperiods == 3 && numdigits == 0 && isdigit(*(ptr + 1))) {
              // allow/ignore n in first position of fourth number to not mean ambiguous, if followed by digit */
            } else {
              numdashes++;
              is_ambig = true;
            }
            ptr++;
            ch = *ptr;
        } else if (ch == '.') {
            numperiods++;
            if (numdigits > 0 && numdashes > 0) return false;
            if (numdigits == 0 && numdashes == 0) return false;
            if (numdashes > 1) return false;
            numdigits = 0;
            numdashes = 0;
            ptr++;
            ch = *ptr;
        } else {
            ptr++;
            ch = *ptr;
        }
    }

    if (numperiods == 3) {
        if (numdigits > 0 && numdashes > 0) return false;
        if (numdigits > 0 || numdashes == 1) return true;
    }

    return false;
}

const string kInferenceMessage[] = {
  "unknown error",
  "empty inference string",
  "bad inference prefix",
  "bad inference body",
  "single inference field",
  "spaces in inference",
  "possible comment in inference",
  "same species misused",
  "bad inference accession",
  "bad inference accession version",
  "accession.version not public",
  "bad accession type",
};


void CValidError_feat::ValidateSeqFeat(const CSeq_feat& feat)
{
    if ( !feat.IsSetLocation() ) {
        PostErr(eDiag_Critical, eErr_SEQ_FEAT_MissingLocation,
            "The feature is missing a location", feat);
        return;
    }

    if (m_Scope) {
        CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
        m_Imp.ValidateSeqLoc(feat.GetLocation(), bsh, 
                             (feat.GetData().IsGene() || !m_Imp.IsGpipe()),
                             "Location", feat);

        if ( feat.CanGetProduct() ) {
            ValidateSeqFeatProduct(feat.GetProduct(), feat);
            CBioseq_Handle p_bsh = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
            if (p_bsh == bsh) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_SelfReferentialProduct, "Self-referential feature product", feat);
            }
        }
    }
    x_ValidateSeqFeatLoc(feat);
    
    ValidateFeatPartialness(feat);
    
    ValidateExcept(feat);

    if (feat.IsSetXref()) {
        CTSE_Handle tse = m_Imp.GetTSEH().GetTSE_Handle();
        FOR_EACH_SEQFEATXREF_ON_SEQFEAT (it, feat) {
            ValidateSeqFeatXref (**it, feat, tse);
        }
    }


    ValidateSeqFeatData(feat.GetData(), feat);
  
    ValidateBothStrands (feat);
    
    if (feat.CanGetDbxref ()) {
        m_Imp.ValidateDbxref (feat.GetDbxref (), feat);
    }
    
    if ( feat.CanGetComment() ) {
        ValidateFeatComment(feat.GetComment(), feat);
    }

    if ( feat.CanGetCit() ) {
        ValidateFeatCit(feat.GetCit(), feat);
    }

    if ( feat.IsSetPseudo()  &&  feat.GetPseudo() ) {
        m_Imp.IncrementPseudoCount();
    }

    FOR_EACH_GBQUAL_ON_FEATURE (it, feat) {
        if (!(*it)->IsSetQual()) {
            continue;
        }
        /* first check for anything other than replace */
        if (!(*it)->IsSetVal() || NStr::IsBlank ((*it)->GetVal())) {
            if (NStr::EqualNocase ((*it)->GetQual(), "replace")) {
                /* ok for replace */
            } else {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue, 
                         "Qualifier other than replace has just quotation marks", feat);
                if (NStr::EqualNocase ((*it)->GetQual(), "EC_number")) {
                      PostErr (eDiag_Warning, eErr_SEQ_FEAT_EcNumberProblem, "EC number should not be empty", feat);
                }
            }
        } else if (NStr::EqualNocase ((*it)->GetQual(), "EC_number")) {
            if (!s_IsValidECNumberFormat((*it)->GetVal())) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberFormat,
                        (*it)->GetVal() + " is not in proper EC_number format", feat);
            } else {
                  string ec_number = (*it)->GetVal();
                CProt_ref::EECNumberStatus status = CProt_ref::GetECNumberStatus (ec_number);
                  switch (status) {
                      case CProt_ref::eEC_deleted:
                          PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberValue, 
                                   "EC_number " + ec_number + " was deleted",
                                   feat);
                          break;
                      case CProt_ref::eEC_replaced:
                          PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberValue, 
                                   "EC_number " + ec_number + " was replaced",
                                   feat);
                          break;
                      case CProt_ref::eEC_unknown:
                    {
                        size_t pos = NStr::Find (ec_number, "n");
                        if (pos == string::npos || !isdigit (ec_number.c_str()[pos + 1])) {
                                  PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberValue, 
                                             ec_number + " is not a legal value for qualifier EC_number",
                                               feat);
                        } else {
                                  PostErr (eDiag_Info, eErr_SEQ_FEAT_BadEcNumberValue, 
                                             ec_number + " is not a legal preliminary value for qualifier EC_number",
                                               feat);
                        }
                    }
                          break;
                      default:
                          break;
                  }
            }
        } else if (NStr::EqualNocase ((*it)->GetQual(), "inference")) {
            /* TODO: Validate inference */
            string val = "";
            if ((*it)->IsSetVal()) {
                val = (*it)->GetVal();
            }
            EInferenceValidCode rsult = ValidateInference (val, m_Imp.ValidateInferenceAccessions());
            if (rsult > eInferenceValidCode_valid) {
                if (NStr::IsBlank (val)) {
                    val = "?";
                }
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_InvalidInferenceValue,
                         "Inference qualifier problem - " + kInferenceMessage [(int) rsult] + " ("
                         + val + ")", feat);
            }
        } else if (NStr::EqualNocase ((*it)->GetQual(), "pseudogene")) {
            m_Imp.IncrementPseudogeneCount();
        }
        if ((*it)->IsSetVal() && ContainsSgml ((*it)->GetVal())) {
            PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                     "feature qualifier " + (*it)->GetVal() + " has SGML",
                     feat);
        }
    }

    if (feat.IsSetExt()) {
        ValidateExtUserObject (feat.GetExt(), feat);
    }
}


bool CValidError_feat::GetTSACDSOnMinusStrandErrors(const CSeq_feat& feat, const CBioseq& seq)
{
    bool rval = false;
    // check for CDSonMinusStrandTranscribedRNA
    if (feat.IsSetData()
        && feat.GetData().IsCdregion()
        && feat.IsSetLocation()
        && feat.GetLocation().GetStrand() == eNa_strand_minus) {
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
        if ( bsh ) {
            CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
            if (di 
                && di->GetMolinfo().IsSetTech() 
                && di->GetMolinfo().GetTech() == CMolInfo::eTech_tsa
                && di->GetMolinfo().IsSetBiomol()
                && di->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_transcribed_RNA) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSonMinusStrandTranscribedRNA,
                        "Coding region on TSA transcribed RNA should not be on the minus strand", feat);
                rval = true;
            }
        }
    }                
    return rval;
}


void CValidError_feat::ValidateSeqFeatContext(const CSeq_feat& feat, const CBioseq& seq)
{
    CSeqFeatData::E_Choice ftype = feat.GetData().Which();

    if (seq.IsAa()) {
        // protein
        switch (ftype) {
            case CSeqFeatData::e_Cdregion:
            case CSeqFeatData::e_Rna:
            case CSeqFeatData::e_Rsite:
            case CSeqFeatData::e_Txinit:
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                    "Invalid feature for a protein Bioseq.", feat);
                break;
            default:
                break;
        }
    } else {
        // nucleotide
        if (ftype == CSeqFeatData::e_Prot || ftype == CSeqFeatData::e_Psec_str) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                    "Invalid feature for a nucleotide Bioseq.", feat);
        }
        if (feat.IsSetData() && feat.GetData().IsProt() && feat.GetData().GetProt().IsSetProcessed()) {
            CProt_ref::TProcessed processed = feat.GetData().GetProt().GetProcessed();
            if (processed == CProt_ref::eProcessed_mature
                || processed == CProt_ref::eProcessed_signal_peptide
                || processed == CProt_ref::eProcessed_transit_peptide
                || processed == CProt_ref::eProcessed_preprotein) {
                PostErr (m_Imp.IsRefSeq() ? eDiag_Error : eDiag_Warning,
                         eErr_SEQ_FEAT_InvalidForType,
                         "Peptide processing feature should be remapped to the appropriate protein bioseq",
                         feat);
            }
        }
    }

    // check for CDSonMinusStrandTranscribedRNA
    GetTSACDSOnMinusStrandErrors(feat, seq);               
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


static string s_LegalMobileElementStrings[] = {
  "transposon",
  "retrotransposon",
  "integron",
  "insertion sequence",
  "non-LTR retrotransposon",
  "SINE",
  "MITE",
  "LINE",
  "other"
};


// private member functions:

void CValidError_feat::ValidateSeqFeatData
(const CSeqFeatData& data,
 const CSeq_feat& feat)
{
    switch ( data.Which () ) {
    case CSeqFeatData::e_Gene:
        // Validate CGene_ref
        ValidateGene(data.GetGene (), feat);
        ValidateOperon(feat);
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
        // Validate CImp
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
    case CSeqFeatData::e_Clone:
    case CSeqFeatData::e_Variation:
        break;

    default:
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidType,
            "Invalid SeqFeat type [" + NStr::IntToString(data.Which()) + "]",
            feat);
        break;
    }

    if ( !data.IsGene() ) {
        ValidateGeneXRef(feat);

        // check old locus tag on feature and overlapping gene
        string feat_old_locus_tag;
        FOR_EACH_GBQUAL_ON_SEQFEAT (it, feat) {
            if ((*it)->IsSetQual() && NStr::Equal ((*it)->GetQual(), "old_locus_tag")
                && (*it)->IsSetVal() && !NStr::IsBlank((*it)->GetVal())) {
                feat_old_locus_tag = (*it)->GetVal();
                break;
            }
        }
        const CGene_ref* grp = feat.GetGeneXref();
        if ( !grp) {
            // check overlapping gene
            CConstRef<CSeq_feat> overlap = 
                GetOverlappingGene(feat.GetLocation(), *m_Scope);
            if ( overlap ) {
                string gene_old_locus_tag;

                FOR_EACH_GBQUAL_ON_SEQFEAT (it, *overlap) {
                    if ((*it)->IsSetQual() && NStr::Equal ((*it)->GetQual(), "old_locus_tag")
                        && (*it)->IsSetVal() && !NStr::IsBlank((*it)->GetVal())) {
                        gene_old_locus_tag = (*it)->GetVal();
                        break;
                    }
                }
                if (!NStr::IsBlank (gene_old_locus_tag)
                    && !NStr::IsBlank (feat_old_locus_tag)
                    && !NStr::EqualNocase (gene_old_locus_tag, feat_old_locus_tag)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_OldLocusTagMismtach,
                             "Old locus tag on feature (" + feat_old_locus_tag
                             + ") does not match that on gene (" + gene_old_locus_tag + ")",
                             feat);
                }
            }
            if (! NStr::IsBlank (feat_old_locus_tag)) {
                if ( grp == 0 ) {
                    const CSeq_feat* gene = GetOverlappingGene(feat.GetLocation(), *m_Scope);
                    if ( gene != 0 ) {
                        grp = &gene->GetData().GetGene();
                    }
                }
                if (grp == 0 || ! grp->IsSetLocus_tag() || NStr::IsBlank (grp->GetLocus_tag())) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_LocusTagProblem,
                            "old_locus_tag without inherited locus_tag", feat);
                }
            }
        }
    }
    
    if ( !data.IsImp() ) {
        ValidateNonImpFeat (feat);
    }
}


static bool s_SeqLocHasAccession (const CSeq_loc& loc)
{
    bool rval = false;

    string label;
    loc.GetLabel (&label);

    for ( CSeq_loc_CI it(loc); it && !rval; ++it ) {
        switch (it.GetSeq_id().Which()) {
            case CSeq_id::e_Genbank:
                if (it.GetSeq_id().GetGenbank().IsSetAccession()
                    && !NStr::IsBlank (it.GetSeq_id().GetGenbank().GetAccession())) {
                    rval = true;
                }
                break;
            case CSeq_id::e_Embl:
                if (it.GetSeq_id().GetEmbl().IsSetAccession()
                    && !NStr::IsBlank (it.GetSeq_id().GetEmbl().GetAccession())) {
                    rval = true;
                }
                break;
            case CSeq_id::e_Ddbj:
                if (it.GetSeq_id().GetDdbj().IsSetAccession()
                    && !NStr::IsBlank (it.GetSeq_id().GetDdbj().GetAccession())) {
                    rval = true;
                }
                break;
            case CSeq_id::e_Tpg:
                if (it.GetSeq_id().GetTpg().IsSetAccession()
                    && !NStr::IsBlank (it.GetSeq_id().GetTpg().GetAccession())) {
                    rval = true;
                }
                break;
            case CSeq_id::e_Tpe:
                if (it.GetSeq_id().GetTpe().IsSetAccession()
                    && !NStr::IsBlank (it.GetSeq_id().GetTpe().GetAccession())) {
                    rval = true;
                }
                break;
            case CSeq_id::e_Tpd:
                if (it.GetSeq_id().GetTpd().IsSetAccession()
                    && !NStr::IsBlank (it.GetSeq_id().GetTpd().GetAccession())) {
                    rval = true;
                }
                break;
            case CSeq_id::e_Other:
                if (it.GetSeq_id().GetOther().IsSetAccession()
                    && !NStr::IsBlank (it.GetSeq_id().GetOther().GetAccession())) {
                    rval = true;
                }
                break;
            default:
                break;
        }
    }
    return rval;
}


void CValidError_feat::ValidateSeqFeatProduct
(const CSeq_loc& prod, const CSeq_feat& feat)
{
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
                    if (ValidateAccessionString (tsid->GetName(), false) == eAccessionFormat_valid) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                                "Feature product should not put an accession in the Textseq-id 'name' slot", feat);
                    } else {
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
    
    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
    if (bsh) {
        m_Imp.ValidateSeqLoc(feat.GetProduct(), bsh, true, "Product", feat);

        FOR_EACH_SEQID_ON_BIOSEQ (id, *(bsh.GetCompleteBioseq())) {
            switch ( (*id)->Which() ) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                {
                    const CTextseq_id* tsid = (*id)->GetTextseq_Id();
                    if ( tsid != NULL ) {
                        if ( !tsid->IsSetAccession()  &&  tsid->IsSetName() ) {
                            if ( ValidateAccessionString (tsid->GetName(), false) == eAccessionFormat_valid) {
                                PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                                    "Protein bioseq has Textseq-id 'name' that "
                                    "looks like it is derived from a nucleotide "
                                    "accession", feat);
                            } else {
                                PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                                    "Protein bioseq has Textseq-id 'name' and no accession", feat);
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
    // look for accession IDs in location
    if (m_Imp.DoesAnyProductLocHaveGI() && s_SeqLocHasAccession (prod) && !m_Imp.IsGpipe()) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureRefersToAccession, "Feature product refers to accession", feat);
    }
}


static const string kGoTermText = "text string";
static const string kGoTermID = "go id";
static const string kGoTermPubMedID = "pubmed id";
static const string kGoTermRef = "go ref";
static const string kGoTermEvidence = "evidence";

class CGoTermSortStruct
{
public:
    CGoTermSortStruct (string term, string goid, int pmid, string evidence);
    int Compare(const CGoTermSortStruct& g2) const;
    bool Duplicates (const CGoTermSortStruct& g2) const;

    string m_Term;
    string m_Goid;
    int m_Pmid;
    string m_Evidence;
};


CGoTermSortStruct::CGoTermSortStruct (string term, string goid, int pmid, string evidence) {
     m_Term = term;
     m_Goid = goid;
     m_Pmid = pmid;
     m_Evidence = evidence;
}


int CGoTermSortStruct::Compare(const CGoTermSortStruct& g2) const
{
    int compare = NStr::Compare (m_Term, g2.m_Term);
    if (compare == 0) {
        compare = NStr::Compare (m_Goid, g2.m_Goid);
    }
    if (compare == 0) {
        compare = NStr::Compare (m_Evidence, g2.m_Evidence);
    }

    if (m_Pmid == 0) return 1;
    if (g2.m_Pmid == 0) return -1;
    if (m_Pmid > g2.m_Pmid) {
        return 1;
    } else if (m_Pmid < g2.m_Pmid) {
        return -1;
    }

    return 0;
}


bool CGoTermSortStruct::Duplicates (const CGoTermSortStruct& g2) const
{
    bool rval = false;
    if (NStr::EqualNocase(m_Term, g2.m_Term) || NStr::EqualNocase (m_Goid, g2.m_Goid)) {
        if (m_Pmid == g2.m_Pmid && NStr::EqualNocase (m_Evidence, g2.m_Evidence)) {
            rval = true;
        }
    }
    return rval;
}


static bool s_GoTermSortStructCompare (const CGoTermSortStruct& q1, const CGoTermSortStruct& q2)
{
    // is q1 < q2
    return (q1.Compare(q2) < 0);
}


static bool s_GoTermPairCompare (const pair<string, string>& p1, const pair<string, string>& p2)
{
    int compare = NStr::Compare (p1.first, p2.first);
    if (compare == 0) {
        compare = NStr::Compare (p1.second, p2.second);
    }
    return (compare < 0);
}


void CValidError_feat::ValidateGoTerms (CUser_object::TData field_list, const CSeq_feat& feat, vector<pair<string, string> >& id_terms)
{
    vector < CGoTermSortStruct > sorted_list;

    ITERATE (CUser_object::TData, it, field_list) {
        if (!(*it)->IsSetData() || !(*it)->GetData().IsFields()) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat,
                     "Bad GO term format", feat);
            continue;
        }
        CUser_object::TData sublist = (*it)->GetData().GetFields();
        string textstr = "";
        string evidence = "";
        string goid = "";
        int pmid = 0; 
        ITERATE (CUser_object::TData, sub_it, sublist) {
            if (!(*sub_it)->IsSetLabel() || !(*sub_it)->GetLabel().IsStr()) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat,
                         "No label on GO term qualifier field",
                         feat);
                continue;
            } else if (NStr::Equal ((*sub_it)->GetLabel().GetStr(), kGoTermText)) {
                if ((*sub_it)->GetData().IsStr()) {
                    textstr = (*sub_it)->GetData().GetStr();
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat,
                             "Bad data format for GO term qualifier term",
                             feat);
                }
            } else if (NStr::Equal ((*sub_it)->GetLabel().GetStr(), kGoTermID)) {
                if ((*sub_it)->GetData().IsInt()) {
                    goid = NStr::IntToString ((*sub_it)->GetData().GetInt());
                } else if ((*sub_it)->GetData().IsStr()) {
                    goid = (*sub_it)->GetData().GetStr();
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat,
                             "Bad data format for GO term qualifier GO ID",
                             feat);
                }
            } else if (NStr::Equal ((*sub_it)->GetLabel().GetStr(), kGoTermPubMedID)) {
                if ((*sub_it)->GetData().IsInt()) {
                    pmid = (*sub_it)->GetData().GetInt();
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat,
                             "Bad data format for GO term qualifier PMID",
                             feat);
                }
            } else if (NStr::Equal ((*sub_it)->GetLabel().GetStr(), kGoTermEvidence)) {
                if ((*sub_it)->GetData().IsStr()) {
                    evidence = (*sub_it)->GetData().GetStr();
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat,
                             "Bad data format for GO term qualifier evidence",
                             feat);
                }
            } else if (NStr::Equal ((*sub_it)->GetLabel().GetStr(), kGoTermRef)) {
                // recognized term
                
            } else {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat,
                         "Unrecognized label on GO term qualifier field " + (*sub_it)->GetLabel().GetStr(),
                         feat);
            }
        }
        // create sort structure and add to list
        sorted_list.push_back (CGoTermSortStruct(textstr, goid, pmid, evidence));
        // add id/term pair
        pair<string, string> p(goid, textstr);
        id_terms.push_back (p);
    }
    stable_sort (sorted_list.begin(), sorted_list.end(), s_GoTermSortStructCompare);

    vector < CGoTermSortStruct >::iterator it1 = sorted_list.begin();
    if (it1 != sorted_list.end()) {
        if (NStr::IsBlank ((*it1).m_Goid)) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneOntologyTermMissingGOID,
                     "GO term does not have GO identifier", feat);
        }
        vector < CGoTermSortStruct >::iterator it2 = it1;
        ++it2;
        while (it2 != sorted_list.end()) {
            if (NStr::IsBlank ((*it2).m_Goid)) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneOntologyTermMissingGOID,
                         "GO term does not have GO identifier", feat);
            }
            if ((*it2).Duplicates (*it1)) {
                PostErr (eDiag_Info, eErr_SEQ_FEAT_DuplicateGeneOntologyTerm, 
                         "Duplicate GO term on feature", feat);
            }
            it1 = it2;
            ++it2;
        }
    }
}


void CValidError_feat::ValidateExtUserObject (const CUser_object& user_object, const CSeq_feat& feat)
{
    if (user_object.IsSetType() && user_object.GetType().IsStr() 
        && NStr::EqualCase(user_object.GetType().GetStr(), "GeneOntology")
        && user_object.IsSetData()) {
        vector<pair<string, string> > id_terms;
        // iterate through fields
        ITERATE (CUser_object::TData, it, user_object.GetData()) {
            // validate terms if match accepted type
            if (!(*it)->GetData().IsFields()) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat, "Bad data format for GO term", feat);
            } else if (!(*it)->IsSetLabel() || !(*it)->GetLabel().IsStr() || !(*it)->IsSetData()) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat, "Unrecognized GO term label [blank]", feat);
            } else {
                string qualtype = (*it)->GetLabel().GetStr();
                if (NStr::EqualNocase (qualtype, "Process")
                    || NStr::EqualNocase (qualtype, "Component")
                    || NStr::EqualNocase (qualtype, "Function")
                    || NStr::IsBlank (qualtype)) {
                    if ((*it)->IsSetData()
                        && (*it)->GetData().IsFields()) {
                        ValidateGoTerms ((*it)->GetData().GetFields(), feat, id_terms);
                    }
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadGeneOntologyFormat, "Unrecognized GO term label " + qualtype, feat);
                }
            }
        }
        if (id_terms.size() > 1) {
            stable_sort (id_terms.begin(), id_terms.end(), s_GoTermPairCompare);
            vector<pair <string, string> >::iterator id_it1 = id_terms.begin();
            vector<pair <string, string> >::iterator id_it2 = id_it1;
            ++id_it2;
            while (id_it2 != id_terms.end()) {
                if (NStr::Equal((*id_it1).first, (*id_it2).first) && !NStr::Equal ((*id_it1).second, (*id_it2).second)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_InconsistentGeneOntologyTermAndId, "Inconsistent GO terms for GO ID " + (*id_it1).first,
                             feat);
                }
                id_it1 = id_it2;
                id_it2++;
            }
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
         genome == CBioSource::eGenome_proplastid   ||
         genome == CBioSource::eGenome_chromatophore ) { 
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
        FOR_EACH_CODEBREAK_ON_CDREGION (cbr, cdregion) {
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
    
    result = GetValidatorLocationLabel(*loc);

    return result;
}


void CValidError_feat::ValidateFeatPartialness(const CSeq_feat& feat)
{
    unsigned int  partial_prod = eSeqlocPartial_Complete, 
                  partial_loc  = eSeqlocPartial_Complete;

    bool is_partial = feat.IsSetPartial()  &&  feat.GetPartial();
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
            if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                // suppress in gpipe genomic
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                    "When SeqFeat.product is a partial Bioseq, SeqFeat.location "
                    "should also be partial", feat);
            }
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
        else if (partial_prod == eSeqlocPartial_Complete  &&  feat.IsSetProduct()) {
            // if not local bioseq product, lower severity
            EDiagSev sev = eDiag_Warning;
            bool is_far_fail = false;
            if ( IsOneBioseq(feat.GetProduct(), m_Scope) ) {
                const CSeq_id& prod_id = GetId(feat.GetProduct(), m_Scope);
                CBioseq_Handle prod =
                    m_Scope->GetBioseqHandleFromTSE(prod_id, m_Imp.GetTSE());
                if ( !prod ) {
                    sev = eDiag_Info;
                    if (m_Imp.x_IsFarFetchFailure(feat.GetProduct())) {
                        is_far_fail = true;
                    }                        
                }
            }
                        
            string str("Inconsistent: Product= complete, Location= ");
            str += (partial_loc != eSeqlocPartial_Complete) ? "partial, " : "complete, ";
            str += "Feature.partial= ";
            str += is_partial ? "TRUE" : "FALSE";
            if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                // suppress for genomic gpipe
            } else if (is_far_fail) {
                m_Imp.SetFarFetchFailure();
            } else {
                PostErr(sev, eErr_SEQ_FEAT_PartialsInconsistent, str, feat);
            }
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
            if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                // suppress for genomic gpipe
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialsInconsistent, str, feat);
            }
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

        // note - in analogous C Toolkit function there is additional code for ensuring
        // that partial intervals are partial at splice sites, gaps, or the ends of the
        // sequence.  This has been moved to CValidError_bioseq::ValidateFeatPartialInContext.
    }
}


static int s_GetStrictGenCode(const CBioSource& src)
{
    int gencode = 0;

    try {
      CBioSource::TGenome genome = src.IsSetGenome() ? src.GetGenome() : CBioSource::eGenome_unknown;

        if ( src.IsSetOrg()  &&  src.GetOrg().IsSetOrgname() ) {
            const COrgName& orn = src.GetOrg().GetOrgname();

            switch ( genome ) {
                case CBioSource::eGenome_kinetoplast:
                case CBioSource::eGenome_mitochondrion:
                    // bacteria and plant organelle code
                    if (orn.IsSetMgcode()) {
                        gencode = orn.GetMgcode();
                    }
                    break;
                case CBioSource::eGenome_chloroplast:
                case CBioSource::eGenome_chromoplast:
                case CBioSource::eGenome_plastid:
                case CBioSource::eGenome_cyanelle:
                case CBioSource::eGenome_apicoplast:
                case CBioSource::eGenome_leucoplast:
                case CBioSource::eGenome_proplastid:
                    // bacteria and plant plastids are code 11.
                    gencode = 11;
                    break;
                default:
                    if (orn.IsSetGcode()) {
                        gencode = orn.GetGcode();
                    }
                    break;
            }
        }
    } catch (CException ) {
    } catch (std::exception ) {
    }
    return gencode;
}


// to be DirSub, must not have ID of type other, must not have WGS keyword or tech, and
// must not be both complete and bacterial
static bool s_IsLocDirSub (const CSeq_loc& loc, CScope& scope)
{
    if (!loc.GetId()) {
        for ( CSeq_loc_CI si(loc); si; ++si ) {
            if (!s_IsLocDirSub(si.GetEmbeddingSeq_loc(), scope)) {
                return false;
            }
        }
        return true;
    }


    if (loc.GetId()->IsOther()) {
        return false;
    }

    CBioseq_Handle bsh = BioseqHandleFromLocation (&scope, loc);
    if (!bsh) {
        return true;
    }

    bool rval = true;
    FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetCompleteBioseq())) {
        if ((*it)->IsOther()) {
            rval = false;
        }
    }

    if (rval) {
        // look for WGS keyword
        for (CSeqdesc_CI diter (bsh, CSeqdesc::e_Genbank); diter && rval; ++diter) {
            FOR_EACH_KEYWORD_ON_GENBANKBLOCK (it, diter->GetGenbank()) {
                if (NStr::EqualNocase (*it, "WGS")) {
                    rval = false;
                }
            }
        }

        // look for WGS tech and completeness
        bool completeness = false;

        for (CSeqdesc_CI diter (bsh, CSeqdesc::e_Molinfo); diter && rval; ++diter) {
            if (diter->GetMolinfo().IsSetTech() && diter->GetMolinfo().GetTech() == CMolInfo::eTech_wgs) {
                rval = false;
            }
            if (diter->GetMolinfo().IsSetCompleteness() && diter->GetMolinfo().GetCompleteness() == CMolInfo::eCompleteness_complete) {
                completeness = true;
            }
        }

        // look for bacterial
        if (completeness) {
            for (CSeqdesc_CI diter (bsh, CSeqdesc::e_Source); diter && rval; ++diter) {
                if (diter->GetSource().IsSetDivision()
                    && NStr::EqualNocase (diter->GetSource().GetDivision(), "BCT")) {
                    rval = false;
                }
            }
        }
    }
    return rval;
}


static const char* const sc_BadGeneSynText [] = {
  "HLA",
  "alpha",
  "alternative",
  "beta",
  "cellular",
  "cytokine",
  "delta",
  "drosophila",
  "epsilon",
  "gamma",
  "homolog",
  "mouse",
  "orf",
  "partial",
  "plasma",
  "precursor",
  "pseudogene",
  "putative",
  "rearranged",
  "small",
  "trna",
  "unknown",
  "unknown function",
  "unknown protein",
  "unnamed"
};
typedef CStaticArraySet<const char*, PCase_CStr> TBadGeneSynSet;
DEFINE_STATIC_ARRAY_MAP(TBadGeneSynSet, sc_BadGeneSyn, sc_BadGeneSynText);


void CValidError_feat::ValidateGene(const CGene_ref& gene, const CSeq_feat& feat)
{
    m_Imp.IncrementGeneCount();

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
    if ( gene.IsSetLocus() && feat.IsSetComment() && NStr::EqualCase (feat.GetComment(), gene.GetLocus())) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_RedundantFields, 
                 "Comment has same value as gene locus",
                 feat);
    }

    if ( gene.IsSetLocus_tag()  &&  !NStr::IsBlank (gene.GetLocus_tag()) ) {
            const string& locus_tag = gene.GetLocus_tag();

        ITERATE (string, it, locus_tag ) {
            if ( isspace((unsigned char)(*it)) != 0 ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_LocusTagProblem,
                    "Gene locus_tag '" + gene.GetLocus_tag() + 
                    "' should be a single word without any spaces", feat);
                break;
            }
        }

            if (gene.IsSetLocus() && !NStr::IsBlank(gene.GetLocus())
                  && NStr::EqualNocase(locus_tag, gene.GetLocus())) {
                  PostErr (eDiag_Error, eErr_SEQ_FEAT_LocusTagProblem, 
                             "Gene locus and locus_tag '" + gene.GetLocus() + "' match",
                               feat);
            }
        if (feat.IsSetComment() && NStr::EqualCase (feat.GetComment(), locus_tag)) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_RedundantFields, 
                     "Comment has same value as gene locus_tag", feat);
        }
        FOR_EACH_GBQUAL_ON_SEQFEAT (it, feat) {
            if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "old_locus_tag") && (*it)->IsSetVal()) {
                if (NStr::EqualCase((*it)->GetVal(), locus_tag)) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_RedundantFields,
                            "old_locus_tag has same value as gene locus_tag", feat);
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_LocusTagProblem, 
                            "Gene locus_tag and old_locus_tag '" + locus_tag + "' match", feat);
                }
                if (NStr::Find ((*it)->GetVal(), ",") != string::npos) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_LocusTagProblem,
                            "old_locus_tag has comma, may contain multiple values", feat);
                }
            }
        }                        
    } else if (m_Imp.DoesAnyGeneHaveLocusTag() && !s_IsLocDirSub (feat.GetLocation(), *m_Scope)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_MissingGeneLocusTag, 
                 "Missing gene locus tag", feat);
    }

    if ( gene.CanGetDb () ) {
        m_Imp.ValidateDbxref(gene.GetDb(), feat);
    }

    if (m_Imp.IsRefSeq()) {
        FOR_EACH_SYNONYM_ON_GENEREF (it, gene) {
            if (sc_BadGeneSyn.find (it->c_str()) != sc_BadGeneSyn.end()) {
                PostErr (m_Imp.IsGpipe() ? eDiag_Info : eDiag_Warning, eErr_SEQ_FEAT_UndesiredGeneSynonym, 
                         "Uninformative gene synonym '" + *it + "'",
                         feat);
            }
            if (gene.IsSetLocus() && !NStr::IsBlank(gene.GetLocus())
                && NStr::Equal(gene.GetLocus(), *it)) {
                PostErr (m_Imp.IsGpipe() ? eDiag_Info : eDiag_Warning, eErr_SEQ_FEAT_UndesiredGeneSynonym,
                         "gene synonym has same value as gene locus",
                         feat);
            }
        }
    }

    if (gene.IsSetLocus() && gene.IsSetDesc() 
        && NStr::EqualCase (gene.GetLocus(), gene.GetDesc())
        && !m_Imp.IsGpipe()) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UndesiredGeneSynonym,
                 "gene description has same value as gene locus", feat);
    }

    if (!gene.IsSetLocus() && !gene.IsSetDesc() && gene.IsSetSyn() && gene.GetSyn().size() > 0) {
        PostErr (m_Imp.IsGpipe() ? eDiag_Info : eDiag_Warning, eErr_SEQ_FEAT_UndesiredGeneSynonym,
                 "gene synonym without gene locus or description", feat);
    }
                 
    if (gene.IsSetLocus()) {
        ValidateCharactersInField (gene.GetLocus(), "Gene locus", feat);
    }

      // check for SGML
      if (gene.IsSetLocus() && ContainsSgml(gene.GetLocus())) {
          PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                   "gene locus " + gene.GetLocus() + " has SGML", feat);
      }
      if (gene.IsSetLocus_tag() && ContainsSgml(gene.GetLocus_tag())) {
          PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                   "gene locus_tag " + gene.GetLocus_tag() + " has SGML", feat);
      }
      if (gene.IsSetDesc()) {
            string desc = gene.GetDesc();
            if (ContainsSgml(desc)) {
                  PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                           "gene description " + gene.GetDesc() + " has SGML", feat);
            }
            if (NStr::Find(desc, "..") != string::npos) {
                  PostErr (eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature, 
                           "Possible location text (" + desc + ") on gene description", feat);
            }
      }
      FOR_EACH_SYNONYM_ON_GENEREF (it, gene) {
            if (ContainsSgml(*it)) {
                  PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                           "gene synonym " + *it + " has SGML", feat);
            }
      }

}


void CValidError_feat::ValidateCdregion (
    const CCdregion& cdregion, 
    const CSeq_feat& feat
) 
{
    bool feat_is_pseudo = feat.CanGetPseudo()  &&  feat.GetPseudo();
    bool gene_is_pseudo = IsOverlappingGenePseudo(feat);
    bool pseudo = feat_is_pseudo  ||  gene_is_pseudo;

    FOR_EACH_GBQUAL_ON_FEATURE (it, feat) {
        const CGb_qual& qual = **it;
        if ( qual.CanGetQual() ) {
            const string& key = qual.GetQual();
            if ( NStr::EqualNocase(key, "pseudo") ) {
                pseudo = true;
            } else if ( NStr::EqualNocase(key, "exception") ) {
                if ( !feat.IsSetExcept() ) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptInconsistent,
                        "Exception flag should be set in coding region", feat);
                }
            } else if ( NStr::EqualNocase(key, "codon") ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_CodonQualifierUsed,
                    "Use the proper genetic code, if available, "
                    "or set transl_excepts on specific codons", feat);
            } else if ( NStr::EqualNocase(key, "protein_id") ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                    "protein_id should not be a gbqual on a CDS feature", feat);
            } else if ( NStr::EqualNocase(key, "gene_synonym") ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                    "gene_synonym should not be a gbqual on a CDS feature", feat);
            } else if ( NStr::EqualNocase(key, "transcript_id") ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                    "transcript_id should not be a gbqual on a CDS feature", feat);
            } else if ( NStr::EqualNocase(key, "codon_start") ) {
                if (cdregion.IsSetFrame() && cdregion.GetFrame() != CCdregion::eFrame_not_set) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                        "conflicting codon_start values", feat);
                } else {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                        "codon_start value should be 1, 2, or 3", feat);
                }
            }
        }
    }
    if ( cdregion.IsSetCode_break()  &&  feat.IsSetExcept_text()  &&
         NStr::FindNoCase(feat.GetExcept_text(), "RNA editing") != NPOS ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExceptAndRnaEditing,
            "CDS has both RNA editing /exception and /transl_except qualifiers", feat);
    }
    
    bool conflict = cdregion.CanGetConflict()  &&  cdregion.GetConflict();
    if ( !pseudo  &&  !conflict ) {
        ValidateCdTrans(feat);
        ValidateSplice(feat);
    } else if ( conflict ) {
        ValidateCdConflict(cdregion, feat);
    }
    ValidateCdsProductId(feat);

    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());

    if ( cdregion.IsSetOrf()  &&  cdregion.GetOrf ()  &&
         feat.IsSetProduct () ) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_OrfCdsHasProduct,
            "An ORF coding region should not have a product", feat);
    }

    if ( pseudo ) {
        if (feat.IsSetProduct () ) {
            if (feat_is_pseudo) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PseudoCdsHasProduct,
                    "A pseudo coding region should not have a product", feat);
            } else if (gene_is_pseudo) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PseudoCdsViaGeneHasProduct,
                    "A coding region overlapped by a pseudogene should not have a product", feat);
            } else {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PseudoCdsHasProduct,
                    "A pseudo coding region should not have a product", feat);
            }
        }
        bool has_protxref = false;
        FOR_EACH_SEQFEATXREF_ON_SEQFEAT (it, feat) {
            if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                has_protxref = true;
            }
        }
        if (has_protxref) {
            EDiagSev sev = eDiag_Error;
            // determine warning level based on location of feature
            if (bsh) {
                FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetCompleteBioseq())) {
                    if ((*it)->IsEmbl()) {
                        sev = eDiag_Warning;
                    } else if ((*it)->IsOther()) {
                        string acc = (*it)->GetOther().GetAccession();
                        if (NStr::StartsWith(acc, "NT_", NStr::eNocase)
                            || NStr::StartsWith(acc, "NG_", NStr::eNocase)
                            || NStr::StartsWith(acc, "NW_", NStr::eNocase)) {
                            sev = eDiag_Warning;
                        }
                    }
                }
            }
            PostErr (sev, eErr_SEQ_FEAT_PseudoCdsHasProtXref,
                     "A pseudo coding region should not have a protein xref", feat);
        }
    }
    
    if ( bsh ) {
        // debug
        string label;
        (*(bsh.GetCompleteBioseq())).GetLabel(&label, CBioseq::eBoth);

        CSeqdesc_CI diter (bsh, CSeqdesc::e_Source);
        if ( diter ) {
            const CBioSource& src = diter->GetSource();
            int biopgencode = s_GetStrictGenCode(src);
            
            int cdsgencode = 0;

            if ( cdregion.CanGetCode() ) {
                cdsgencode = cdregion.GetCode().GetId();
            }
            if ( biopgencode != cdsgencode 
                 && (!feat.IsSetExcept()
                     || !feat.IsSetExcept_text() 
                     || NStr::Find(feat.GetExcept_text(), "genetic code exception") == string::npos)) {
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

    ValidateBadMRNAOverlap(feat);
    ValidateCommonCDSProduct(feat);
    ValidateCDSPartial(feat);
    if (!feat.IsSetExcept() && DoesCDSHaveShortIntrons(feat)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_ShortIntron,
                 "Introns should be at least 10 nt long", feat);
    }

    // look for EC number in comment
    if (feat.IsSetComment() && HasECnumberPattern(feat.GetComment())) {
        // suppress if protein has EC numbers
        bool suppress = false;
        if (feat.IsSetProduct()) {
            CBioseq_Handle pbsh = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
            if (pbsh) {
                CFeat_CI prot_feat(pbsh, SAnnotSelector(CSeqFeatData::eSubtype_prot));
                if (prot_feat && prot_feat->GetData().GetProt().IsSetEc()) {
                    suppress = true;
                }
            }
        }
        if (!suppress) {
            PostErr (eDiag_Info, eErr_SEQ_FEAT_EcNumberProblem,
                     "Apparent EC number in CDS comment", feat);
        }
    }

}


void CValidError_feat::x_ValidateCdregionCodebreak
(const CCdregion& cds,
 const CSeq_feat& feat)
{
    const CSeq_loc& feat_loc = feat.GetLocation();
    const CCode_break* prev_cbr = 0;

    FOR_EACH_CODEBREAK_ON_CDREGION (it, cds) {
        const CCode_break& cbr = **it;
        const CSeq_loc& cbr_loc = cbr.GetLoc();
        ECompare comp = Compare(cbr_loc, feat_loc, m_Scope);
        if ( ((comp != eContained) && (comp != eSame)) || cbr_loc.IsNull() || cbr_loc.IsEmpty()) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_Range, 
                "Code-break location not in coding region", feat);
        } else if (feat.IsSetProduct()) {
            if (cbr_loc.GetStop(eExtreme_Biological) == feat_loc.GetStop(eExtreme_Biological)) {
                // terminal exception - don't bother checking, can't be mapped
            } else {
                int frame = 0;
                CRef<CSeq_loc> p_loc = SourceToProduct(feat, cbr_loc, fS2P_AllowTer, m_Scope, &frame);
                if (!p_loc || p_loc->IsNull() || frame != 1 ) {
                    PostErr (eDiag_Error, eErr_SEQ_FEAT_Range, 
                        "Code-break location not in coding region - may be frame problem", feat);
                }
            }
        }
        if ( prev_cbr != 0 ) {
            if ( Compare(cbr_loc, prev_cbr->GetLoc(), m_Scope) == eSame ) {
                string msg = "Multiple code-breaks at same location ";
                string str = GetValidatorLocationLabel (cbr_loc);
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
         !NStr::IsBlank(feat.GetExcept_text()) ) {
        if ( NStr::Find(feat.GetExcept_text(),
            "rearrangement required for product") != NPOS ) {
           return;
        }
    }
    
    // non-pseudo CDS must have /product
    PostErr(m_Imp.IsGeneious() ? eDiag_Warning : eDiag_Error, eErr_SEQ_FEAT_MissingCDSproduct,
        "Expected CDS product absent", feat);
}


void CValidError_feat::ValidateCdConflict
(const CCdregion& cdregion,
 const CSeq_feat& feat)
{
    CBioseq_Handle nuc  = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
    CBioseq_Handle prot = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
    
    // translate the coding region
    string transl_prot;
    try {
        CSeqTranslator::Translate(feat, *m_Scope, transl_prot,
                                  false,   // do not include stop codons
                                  false);  // do not remove trailing X/B/Z

    } catch ( const runtime_error& ) {
    }

    CSeqVector prot_vec = prot.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    prot_vec.SetCoding(CSeq_data::e_Ncbieaa);

    string prot_seq;
    prot_vec.GetSeqData(0, prot_vec.size(), prot_seq);

    if ( transl_prot.empty()  ||  prot_seq.empty()  ||  NStr::Equal(transl_prot, prot_seq) ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_BadConflictFlag,
                "Coding region conflict flag should not be set", feat);
    } else {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ConflictFlagSet,
                "Coding region conflict flag is set", feat);
    }
}


static bool s_RareConsensusNotExpected (CBioseq_Handle seq)

{
    bool rval = false;
    CSeqdesc_CI sd(seq, CSeqdesc::e_Source);

    if (sd && sd->GetSource().IsSetOrgname()) {
        if (!sd->GetSource().GetOrgname().IsSetDiv()
            || !NStr::EqualNocase (sd->GetSource().GetOrgname().GetDiv(), "PLN")) {
            rval = true;
        } else if (!sd->GetSource().GetOrgname().IsSetLineage()
            || !NStr::StartsWith(sd->GetSource().GetOrgname().GetLineage(), "Eukaryota; Viridiplantae; ")) {
            rval = true;
        }
    }
    return rval;
}

    

static bool s_ConsistentWithA (Char ch)

{
  return (bool) (strchr ("ANRMWHVD", ch) != NULL);
}

static bool s_ConsistentWithC (Char ch)

{
  return (bool) (strchr ("CNYMSHBV", ch) != NULL);
}

static bool s_ConsistentWithG (Char ch)

{
  return (bool) (strchr ("GNRKSBVD", ch) != NULL);
}

static bool s_ConsistentWithT (Char ch)

{
  return (bool) (strchr ("TNYKWHBD", ch) != NULL);
}


void CValidError_feat::ValidateIntron (const CSeq_feat& feat)
{
    if (IsIntronShort(feat)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_ShortIntron,
                 "Introns should be at least 10 nt long", feat);
    }

    if (feat.IsSetExcept() && feat.IsSetExcept_text()
        && NStr::FindNoCase (feat.GetExcept_text(), "nonconsensus splice site") != string::npos) {
        return;
    }

    const CSeq_loc& loc = feat.GetLocation();

    bool partial5 = loc.IsPartialStart(eExtreme_Biological);
    bool partial3 = loc.IsPartialStop(eExtreme_Biological);
    if (partial5 && partial3) {
        return;
    }

    // suppress if contained by rRNA - different consensus splice site
    TFeatScores scores;
    GetOverlappingFeatures(loc,
                           CSeqFeatData::e_Rna,
                           CSeqFeatData::eSubtype_rRNA,
                           eOverlap_Contained,
                           scores, *m_Scope);
    if (scores.size() > 0) {
        return;
    }

    // suppress if contained by tRNA - different consensus splice site
    scores.clear();
    GetOverlappingFeatures(loc,
                           CSeqFeatData::e_Rna,
                           CSeqFeatData::eSubtype_tRNA,
                           eOverlap_Contained,
                           scores, *m_Scope);
    if (scores.size() > 0) {
        return;
    }

    // skip if more than one bioseq
    if (!IsOneBioseq(loc, m_Scope)) {
        return;
    }

    // skip if organelle
    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, loc);
    if (m_Imp.IsOrganelle(bsh)) {
        return;
    }

    TSeqPos seq_len = bsh.GetBioseqLength();
    string label = GetBioseqIdLabel(*(bsh.GetCompleteBioseq()));
    CSeqVector vec = bsh.GetSeqVector (CBioseq_Handle::eCoding_Iupac);

    ENa_strand strand = loc.GetStrand();

    EDiagSev sev = eDiag_Warning;
    if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
        sev = eDiag_Info;
    }

    if (!partial5) {
        TSeqPos end = loc.GetStart (eExtreme_Biological);
        if (vec.IsInGap (end)) {
            //is ok
        } else if (strand == eNa_strand_minus) {
            if (end > 0 
                && IsResidue (vec[end - 1]) && (s_ConsistentWithA(vec[end - 1]) || s_ConsistentWithG(vec[end - 1]))
                && IsResidue (vec[end]) && s_ConsistentWithC(vec[end])) {
                // matches donor
            } else if (end == seq_len - 1) {
                PostErr (eDiag_Info, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                         "Splice donor consensus (GT) not found at start of terminal intron, position "
                          + NStr::IntToString (end + 1) + " of " + label,
                          feat);
            } else {
                PostErr (sev, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                         "Splice donor consensus (GT) not found at start of intron, position "
                          + NStr::IntToString (end + 1) + " of " + label,
                          feat);
            }
        } else {
            if (end < seq_len - 1
                && IsResidue (vec[end]) && s_ConsistentWithG(vec[end])
                && IsResidue (vec[end + 1]) && (s_ConsistentWithT(vec[end + 1]) || s_ConsistentWithC(vec[end + 1]))) {
                // matches donor
            } else if (end == 0) {
                PostErr (eDiag_Info, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                         "Splice donor consensus (GT) not found at start of terminal intron, position "
                          + NStr::IntToString (end + 1) + " of " + label,
                          feat);
            } else {
                PostErr (sev, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                         "Splice donor consensus (GT) not found at start of intron, position "
                          + NStr::IntToString (end + 1) + " of " + label,
                          feat);
            }
        } 
    }
    if (!partial3) {
        TSeqPos end = loc.GetStop (eExtreme_Biological);
        if (vec.IsInGap (end)) {
            //is ok
        } else if (strand == eNa_strand_minus) {            
            if (end < seq_len - 1
                && IsResidue (vec[end]) && vec[end] == 'C'
                && IsResidue (vec[end + 1]) && vec[end + 1] == 'T') {
                // matches acceptor
            } else if (end == 0) {
                PostErr (eDiag_Info, eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                          "Splice acceptor consensus (AG) not found at end of terminal intron, position "
                          + NStr::IntToString (end + 1) + " of " + label + ", but at end of sequence",
                          feat);
            } else {
                PostErr (sev, eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                         "Splice acceptor consensus (AG) not found at end of intron, position "
                          + NStr::IntToString (end + 1) + " of " + label,
                          feat);
            }
        } else {
            if (end > 0
                && IsResidue (vec[end - 1]) && s_ConsistentWithA(vec[end - 1])
                && IsResidue (vec[end]) && s_ConsistentWithG(vec[end])) {
                // matches acceptor
            } else if (end == seq_len - 1) {
                PostErr (eDiag_Info, eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                          "Splice acceptor consensus (AG) not found at end of terminal intron, position "
                          + NStr::IntToString (end + 1) + " of " + label + ", but at end of sequence",
                          feat);
            } else {
                PostErr (sev, eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                         "Splice acceptor consensus (AG) not found at end of intron, position "
                          + NStr::IntToString (end + 1) + " of " + label,
                          feat);
            }
        }
    }
}


void CValidError_feat::ValidateDonor 
(ENa_strand strand, 
 TSeqPos    stop, 
 CSeqVector vec, 
 TSeqPos    seq_len, 
 bool       rare_consensus_not_expected,
 string     label,
 bool       report_errors,
 bool       relax_to_warning,
 bool&      has_errors,
 const CSeq_feat& feat)
{

    if (strand == eNa_strand_minus) {
        // check donor on minus strand
        if (stop > 1) {
            if (vec.IsInGap(stop - 2) && vec.IsInGap (stop - 1)) {
                // ok, location abuts gap
                // suppress warning about UnnecessaryException
                has_errors = true;
            } else if (vec[stop - 1] > 250 || vec[stop - 2] > 250) {
                has_errors = true;
                if (report_errors) {
                   PostErr (eDiag_Warning, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                             "Bad sequence at splice donor after exon ending at position "
                             + NStr::IntToString (stop + 1) + " of " + label,
                             feat);
                }
            } else if (s_ConsistentWithC(vec[stop - 1]) && s_ConsistentWithA(vec[stop - 2])) {
                // it's ok, it's abutting the reverse complement of GU
            } else if (vec[stop - 1] == 'C' && vec[stop - 2] == 'G') {
                // reverse complement of minor splice site GC
                if (rare_consensus_not_expected) {
                    has_errors = true;
                    if (report_errors) {
                        PostErr (eDiag_Info, eErr_SEQ_FEAT_RareSpliceConsensusDonor,
                                 "Rare splice donor consensus (GC) found instead of (GT) after exon ending at position "
                                 + NStr::IntToString (stop + 1) + " of " + label,
                                 feat);
                    }
                }
            } else {
                has_errors = true;
                if (report_errors) {
                    EDiagSev sev = eDiag_Error;
                    if (m_Imp.IsGpipe() && m_Imp.IsGenomic()) {
                        sev = eDiag_Info;
                    } else if (relax_to_warning) {
                        sev = eDiag_Warning;
                    }
                    PostErr (sev, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                             "Splice donor consensus (GT) not found after exon ending at position "
                             + NStr::IntToString (stop + 1) + " of " + label,
                             feat);
                }
            }
        }
    } else {
        // check for donor on plus strand
        if (stop < seq_len - 2) {
            if (vec.IsInGap(stop + 1) && vec.IsInGap (stop + 2)) {
                // ok, location abuts gap
                // suppress warning about UnnecessaryException
                has_errors = true;
            } else if (vec[stop + 1] > 250 || vec[stop + 2] > 250) {
                has_errors = true;
                if (report_errors) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                             "Bad sequence at splice donor after exon ending at position "
                             + NStr::IntToString (stop + 1) + " of " + label,
                             feat);
                }
            } else if (s_ConsistentWithG(vec[stop + 1]) && s_ConsistentWithT(vec[stop + 2])) {
                // it's ok, it's abutting GT
            } else if (vec[stop + 1] == 'G' && vec[stop + 2] == 'C') {
                // minor splice site GC
                if (rare_consensus_not_expected) {
                    has_errors = true;
                    if (report_errors) {
                        PostErr (eDiag_Info, eErr_SEQ_FEAT_RareSpliceConsensusDonor,
                                 "Rare splice donor consensus (GC) found instead of (GT) after exon ending at position "
                                 + NStr::IntToString (stop + 1) + " of " + label,
                                 feat);
                    }
                }
            } else {
                has_errors = true;
                if (report_errors) {
                    EDiagSev sev = eDiag_Error;
                    if (m_Imp.IsGpipe() && m_Imp.IsGenomic()) {
                        sev = eDiag_Info;
                    } else if (relax_to_warning) {
                        sev = eDiag_Warning;
                    }
                    PostErr (sev, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                             "Splice donor consensus (GT) not found after exon ending at position "
                             + NStr::IntToString (stop + 1) + " of " + label,
                             feat);
                }
            }
        }
    }
}


void CValidError_feat::ValidateAcceptor 
(ENa_strand strand, 
 TSeqPos    start, 
 CSeqVector vec, 
 TSeqPos    seq_len, 
 bool       rare_consensus_not_expected,
 string     label,
 bool       report_errors,
 bool       relax_to_warning,
 bool&      has_errors,
 const CSeq_feat& feat)
{
    if (strand == eNa_strand_minus) {
        // check for acceptor on minus strand
        if (start < seq_len - 2) {
            if (vec.IsInGap(start + 1) && vec.IsInGap (start + 2)) {
                // ok, location abuts gap
                // suppress warning about UnnecessaryException
                has_errors = true;
            } else if (vec[start + 1] > 250 || vec[start + 2] > 250) {
                has_errors = true;
                if (report_errors) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                             "Bad sequence at splice acceptor before exon starting at position "
                             + NStr::IntToString (start + 1) + " of " + label,
                             feat);
                }
            } else if (s_ConsistentWithC(vec[start + 1]) && s_ConsistentWithT(vec[start + 2])) {
                // it's ok, it's abutting the reverse complement of AG
            } else {
                has_errors = true;
                if (report_errors) {
                    EDiagSev sev = eDiag_Error;
                    if (m_Imp.IsGpipe() && m_Imp.IsGenomic()) {
                        sev = eDiag_Info;
                    } else if (relax_to_warning) {
                        sev = eDiag_Warning;
                    }
                    PostErr (sev, eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                             "Splice acceptor consensus (AG) not found before exon starting at position "
                             + NStr::IntToString (start + 1) + " of " + label,
                             feat);
                }
            }
        }
    } else {
        // check for acceptor on plus strand
        if (start > 1) {
            if (vec.IsInGap(start - 2) && vec.IsInGap (start - 1)) {
                // ok, location abuts gap
                // suppress warning about UnnecessaryException
                has_errors = true;
            } else if (vec[start - 2] > 250 || vec[start - 1] > 250) {
                has_errors = true;
                if (report_errors) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                             "Bad sequence at splice acceptor before exon starting at position "
                             + NStr::IntToString (start + 1) + " of " + label,
                             feat);
                }
            } else if (s_ConsistentWithA(vec[start - 2]) && s_ConsistentWithG(vec[start - 1])) {
                // it's ok, it's abutting AG
            } else {
                has_errors = true;
                if (report_errors) {
                    EDiagSev sev = eDiag_Error;
                    if (m_Imp.IsGpipe() && m_Imp.IsGenomic()) {
                        sev = eDiag_Info;
                    } else if (relax_to_warning) {
                        sev = eDiag_Warning;
                    }
                    PostErr (sev, eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                             "Splice acceptor consensus (AG) not found before exon starting at position "
                             + NStr::IntToString (start + 1) + " of " + label,
                             feat);
                }
            }
        }
    }
}


void CValidError_feat::ValidateSplice(const CSeq_feat& feat, bool check_all)
{
    bool report_errors = true, has_errors = false, ribo_slip = false;

    const CSeq_loc& loc = feat.GetLocation();
    
    // skip if organelle
    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, loc);
    if (!bsh || m_Imp.IsOrganelle(bsh)) {
        return;
    }
    
    // suppress for specific biological exceptions
    if (feat.IsSetExcept() && feat.IsSetExcept_text()
        && (NStr::FindNoCase (feat.GetExcept_text(), "low-quality sequence region") != string::npos)) {
        return;
    }
    if (feat.IsSetExcept() && feat.IsSetExcept_text()
        && (NStr::FindNoCase (feat.GetExcept_text(), "ribosomal slippage") != string::npos)) {
        report_errors = false;
        ribo_slip = true;
    }
    if (feat.IsSetExcept() && feat.IsSetExcept_text()
        && (NStr::FindNoCase (feat.GetExcept_text(), "artificial frameshift") != string::npos
            || NStr::FindNoCase (feat.GetExcept_text(), "nonconsensus splice site") != string::npos
            || NStr::FindNoCase (feat.GetExcept_text(), "adjusted for low-quality genome") != string::npos
            || NStr::FindNoCase (feat.GetExcept_text(), "heterogeneous population sequenced") != string::npos
            || NStr::FindNoCase (feat.GetExcept_text(), "low-quality sequence region") != string::npos
            || NStr::FindNoCase (feat.GetExcept_text(), "artificial location") != string::npos)) {
        report_errors = false;
    }

    if (m_Imp.x_IsFarFetchFailure(loc)) {
        m_Imp.SetFarFetchFailure();
        return;
    }


    // look for mixed strands, skip if found
    ENa_strand strand = eNa_strand_unknown;
    bool relax_to_warning = false;

    int num_parts = 0;
    for ( CSeq_loc_CI si(loc); si; ++si ) {
        if (si.IsSetStrand()) {
            ENa_strand tmp = si.GetStrand();
            if (tmp == eNa_strand_plus || tmp == eNa_strand_minus) {
                if (strand == eNa_strand_unknown) {
                    strand = si.GetStrand();
                } else if (strand != tmp) {
                    return;
                }
            }
        }
        CBioseq_Handle bsh_si = BioseqHandleFromLocation(m_Scope, si.GetEmbeddingSeq_loc());
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh_si.GetCompleteBioseq())) {
            if ((*it)->IsOther() && (*it)->GetOther().IsSetAccession()) {
                relax_to_warning = true;
            }
        }
        num_parts++;
    }

    if (!check_all && num_parts < 2) {
        return;
    }

    if (!m_Imp.IsGPS() && !m_Imp.IsRefSeq() && !m_Imp.ReportSpliceAsError()) {
        relax_to_warning = true;
    } 

    // only check for errors if overlapping gene is not pseudo
    if (!IsOverlappingGenePseudo(feat)) {

        // get overlapping mRNA - won't check endpoints if they coincide with
        // endpoints of mRNA
        TSeqPos mrna_start = 0, mrna_stop = 0;
        bool set_mrna_start = false, set_mrna_stop = false;

        CConstRef<CSeq_feat> mrna = GetBestOverlappingFeat(
            loc,
            CSeqFeatData::eSubtype_mRNA,
            eOverlap_Contained,
            *m_Scope);
        if (mrna) {
            if (!mrna->GetLocation().IsPartialStart(eExtreme_Biological)) {
                mrna_start = mrna->GetLocation().GetStart(eExtreme_Biological);
                set_mrna_start = true;
            }
            if (!mrna->GetLocation().IsPartialStop(eExtreme_Biological)) {
                mrna_stop = mrna->GetLocation().GetStop(eExtreme_Biological);
                set_mrna_stop = true;
            }
        }    

        bool ignore_mrna_partial5 = false;
        bool ignore_mrna_partial3 = false;
        if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
            CConstRef<CSeq_feat> cds = GetBestOverlappingFeat(
                loc,
                CSeqFeatData::eSubtype_cdregion,
                eOverlap_Contains,
                *m_Scope);
            if (cds) {
                if (!cds->GetLocation().IsPartialStart(eExtreme_Biological) 
                    && cds->GetLocation().GetStart(eExtreme_Biological) == feat.GetLocation().GetStart(eExtreme_Biological)) {
                    ignore_mrna_partial5 = true;
                }
                if (!cds->GetLocation().IsPartialStop(eExtreme_Biological) 
                    && cds->GetLocation().GetStop(eExtreme_Biological) == feat.GetLocation().GetStop(eExtreme_Biological)) {
                    ignore_mrna_partial3 = true;
                }
            }
        }

        bool rare_consensus_not_expected = s_RareConsensusNotExpected (bsh);
        int part_num = 0;

        for ( CSeq_loc_CI si(loc); si; ++si, ++part_num ) {
            try {
                const CSeq_loc& part = si.GetEmbeddingSeq_loc();
                CSeq_loc::TRange range = si.GetRange();
                CBioseq_Handle bsh_si = BioseqHandleFromLocation(m_Scope, *(si.GetRangeAsSeq_loc()) );
                    
                if (bsh_si) {
                    CSeqVector vec = bsh_si.GetSeqVector (CBioseq_Handle::eCoding_Iupac);

                    string label = GetBioseqIdLabel (*(bsh_si.GetCompleteBioseq()), true);

                    TSeqPos start = part.GetStart(eExtreme_Biological);
                    TSeqPos stop = part.GetStop(eExtreme_Biological);
                    bool first = false;
                    bool last = false;
                    if (si.GetStrand() == eNa_strand_minus) {
                        start = range.GetTo();
                        stop = range.GetFrom();
                    } else {
                        start = range.GetFrom();
                        stop = range.GetTo();
                    }
                    if (part_num == 0) {
                        first = true;
                    }
                    if (part_num == num_parts - 1) {
                        last = true;
                    }

                    bool checkExonDonor = false;
                    bool checkExonAcceptor = false;
                    if (check_all) {
                        checkExonDonor = true;
                        checkExonAcceptor = true;
                        if (mrna) {
                            if (set_mrna_stop && stop == mrna_stop && !mrna->GetLocation().IsPartialStop(eExtreme_Biological)) {
                                checkExonDonor = false;
                            }
                            if (set_mrna_start && mrna && start == mrna_start && !mrna->GetLocation().IsPartialStart(eExtreme_Biological)) {
                                checkExonAcceptor = false;
                            }
                        }
                    }
                        
                    bool last_partial = last && part.IsPartialStop(eExtreme_Biological);
                    bool first_partial = first && part.IsPartialStart(eExtreme_Biological);

                    // check donor
                    if (!last || (checkExonDonor && !last_partial) || (last && last_partial && !ignore_mrna_partial3)) {
                        ValidateDonor (strand, stop, vec, bsh_si.GetInst_Length(), rare_consensus_not_expected,
                                       label, report_errors, relax_to_warning, has_errors, feat);
                    }
                                            
                    // check acceptor
                    if (!first || (checkExonAcceptor && !first_partial) || (first && first_partial && !ignore_mrna_partial5)) {
                        ValidateAcceptor (strand, start, vec, bsh_si.GetInst_Length(), rare_consensus_not_expected,
                                       label, report_errors, relax_to_warning, has_errors, feat);
                    }
                }
                  } catch (CException ) {
                  } catch (std::exception ) {
                // could get errors from CSeqVector
            }
        }
    }                   

    if (!report_errors  &&  !has_errors  &&  !ribo_slip) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
            "feature has exception but passes splice site test", feat);
    }
}


// note - list bad protein names in lower case, as search term is converted to lower case
// before looking for exact match
static const char* const sc_BadProtNameText [] = {
  "'hypothetical protein",
  "alpha",
  "alternative",
  "alternatively spliced",
  "bacteriophage hypothetical protein",
  "beta",
  "cellular",
  "cnserved hypothetical protein",
  "conesrved hypothetical protein",
  "conserevd hypothetical protein",
  "conserved archaeal protein",
  "conserved domain protein",
  "conserved hypohetical protein",
  "conserved hypotehtical protein",
  "conserved hypotheical protein",
  "conserved hypothertical protein",
  "conserved hypothetcial protein",
  "conserved hypothetical",
  "conserved hypothetical exported protein",
  "conserved hypothetical integral membrane protein",
  "conserved hypothetical membrane protein",
  "conserved hypothetical phage protein",
  "conserved hypothetical prophage protein",
  "conserved hypothetical protein",
  "conserved hypothetical protein - phage associated",
  "conserved hypothetical protein fragment 3",
  "conserved hypothetical protein, fragment",
  "conserved hypothetical protein, putative",
  "conserved hypothetical protein, truncated",
  "conserved hypothetical protein, truncation",
  "conserved hypothetical protein.",
  "conserved hypothetical protein; possible membrane protein",
  "conserved hypothetical protein; putative membrane protein",
  "conserved hypothetical proteins",
  "conserved hypothetical protien",
  "conserved hypothetical transmembrane protein",
  "conserved hypotheticcal protein",
  "conserved hypthetical protein",
  "conserved in bacteria",
  "conserved membrane protein",
  "conserved protein",
  "conserved protein of unknown function",
  "conserved protein of unknown function ; putative membrane protein",
  "conserved unknown protein",
  "conservedhypothetical protein",
  "conserverd hypothetical protein",
  "conservered hypothetical protein",
  "consrved hypothetical protein",
  "converved hypothetical protein",
  "cytokine",
  "delta",
  "drosophila",
  "duplicated hypothetical protein",
  "epsilon",
  "gamma",
  "hla",
  "homeodomain",
  "homeodomain protein",
  "homolog",
  "hyopthetical protein",
  "hypotethical",
  "hypotheical protein",
  "hypothertical protein",
  "hypothetcical protein",
  "hypothetical",
  "hypothetical  protein",
  "hypothetical conserved protein",
  "hypothetical exported protein",
  "hypothetical novel protein",
  "hypothetical orf",
  "hypothetical phage protein",
  "hypothetical prophage protein",
  "hypothetical protein",
  "hypothetical protein (fragment)",
  "hypothetical protein (multi-domain)",
  "hypothetical protein (phage associated)",
  "hypothetical protein - phage associated",
  "hypothetical protein fragment",
  "hypothetical protein fragment 1",
  "hypothetical protein predicted by genemark",
  "hypothetical protein predicted by glimmer",
  "hypothetical protein predicted by glimmer/critica",
  "hypothetical protein, conserved",
  "hypothetical protein, phage associated",
  "hypothetical protein, truncated",
  "hypothetical protein-putative conserved hypothetical protein",
  "hypothetical protein.",
  "hypothetical proteins",
  "hypothetical protien",
  "hypothetical transmembrane protein",
  "hypothetoical protein",
  "hypothteical protein",
  "identified by sequence similarity; putative; orf located~using blastx/framed",
  "identified by sequence similarity; putative; orf located~using blastx/glimmer/genemark",
  "ion channel",
  "membrane protein, putative",
  "mouse",
  "narrowly conserved hypothetical protein",
  "novel protein",
  "orf",
  "orf, conserved hypothetical protein",
  "orf, hypothetical",
  "orf, hypothetical protein",
  "orf, hypothetical, fragment",
  "orf, partial conserved hypothetical protein",
  "orf; hypothetical protein",
  "orf; unknown function",
  "partial",
  "partial cds, hypothetical",
  "partially conserved hypothetical protein",
  "phage hypothetical protein",
  "phage-related conserved hypothetical protein",
  "phage-related protein",
  "plasma",
  "possible hypothetical protein",
  "precursor",
  "predicted coding region",
  "predicted protein",
  "predicted protein (pseudogene)",
  "predicted protein family",
  "product uncharacterised protein family",
  "protein family",
  "protein of unknown function",
  "pseudogene",
  "putative",
  "putative conserved protein",
  "putative exported protein",
  "putative hypothetical protein",
  "putative membrane protein",
  "putative orf; unknown function",
  "putative phage protein",
  "putative protein",
  "rearranged",
  "repeats containing protein",
  "reserved",
  "ribosomal protein",
  "similar to",
  "small",
  "small hypothetical protein",
  "transmembrane protein",
  "trna",
  "trp repeat",
  "trp-repeat protein",
  "truncated conserved hypothetical protein",
  "truncated hypothetical protein",
  "uncharacterized conserved membrane protein",
  "uncharacterized conserved protein",
  "uncharacterized conserved secreted protein",
  "uncharacterized protein",
  "uncharacterized protein conserved in archaea",
  "uncharacterized protein conserved in bacteria",
  "unique hypothetical",
  "unique hypothetical protein",
  "unknown",
  "unknown cds",
  "unknown function",
  "unknown gene",
  "unknown protein",
  "unknown, conserved protein",
  "unknown, hypothetical",
  "unknown-related protein",
  "unknown; predicted coding region",
  "unnamed",
  "unnamed protein product",
  "very hypothetical protein"
};
typedef CStaticArraySet<const char*, PCase_CStr> TBadProtNameSet;
DEFINE_STATIC_ARRAY_MAP(TBadProtNameSet, sc_BadProtName, sc_BadProtNameText);

void CValidError_feat::ValidateProt(const CProt_ref& prot, const CSeq_feat& feat) 
{
    CProt_ref::EProcessed processed = CProt_ref::eProcessed_not_set;

    if ( prot.CanGetProcessed() ) {
        processed = prot.GetProcessed();
    }

    bool empty = true;
    if ( processed != CProt_ref::eProcessed_signal_peptide  &&
         processed != CProt_ref::eProcessed_transit_peptide ) {
        if ( prot.IsSetName()  &&
            !prot.GetName().empty()  &&
            !prot.GetName().front().empty() ) {
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
                "There is a protein feature where all fields are empty", feat);
        }
    }

    if (m_Imp.IsRefSeq()) {
        x_ReportUninformativeNames (prot, feat);
    }

    // only look for EC numbers in first protein name
    if (prot.IsSetName() && prot.GetName().size() > 0
        && HasECnumberPattern(prot.GetName().front())) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_EcNumberProblem, 
                     "Apparent EC number in protein title", feat);
    }

    FOR_EACH_NAME_ON_PROTREF (it, prot) {
        if (NStr::EndsWith (*it, "]")) {
            bool report_name = true;
            size_t pos = NStr::Find(*it, "[", 0, string::npos, NStr::eLast);
            if (pos == string::npos) {
                // no disqualifying text
            } else if (it->length() - pos < 5) {
                // no disqualifying text
            } else if (NStr::EqualCase(*it, pos, 4, "[NAD")) {
                report_name = false;
            }
            if (! m_Imp.IsEmbl() && ! m_Imp.IsTPE()) {
                if (report_name) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProteinNameEndsInBracket,
                            "Protein name ends with bracket and may contain organism name",
                            feat);
                }
            }
        }
        if (NStr::StartsWith (*it, "hypothetical protein XP_")) {
            CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
            if (bsh) {
                FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(bsh.GetCompleteBioseq())) {
                    if ((*id_it)->IsOther() 
                        && (*id_it)->GetOther().IsSetAccession()
                        && !NStr::EqualNocase ((*id_it)->GetOther().GetAccession(),
                                               (*it).substr (21))) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_HpotheticalProteinMismatch,
                                "Hypothetical protein reference does not match accession",
                                feat);
                    }
                }
            }
        }
        if (feat.IsSetComment() && NStr::EqualCase (feat.GetComment(), *it)) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_RedundantFields, 
                    "Comment has same value as protein name", feat);
        }

        if (s_StringHasPMID(*it)) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProteinNameHasPMID, 
                    "Protein name has internal PMID", feat);
        }
        if (prot.IsSetEc()
            && (NStr::EqualCase (*it, "Hypothetical protein")
                || NStr::EqualCase (*it, "hypothetical protein")
                || NStr::EqualCase (*it, "Unknown protein")
                || NStr::EqualCase (*it, "unknown protein"))) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadProteinName, 
                     "Unknown or hypothetical protein should not have EC number",
                     feat);
        }

        if (m_Imp.DoRubiscoTest()) {
            if (NStr::FindCase (*it, "ribulose") != string::npos
                && NStr::FindCase (*it, "bisphosphate") != string::npos
                && NStr::FindCase (*it, "methyltransferase") == string::npos
                && NStr::FindCase (*it, "activase") == string::npos) {
                if (NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase/oxygenase")) {
                    // allow standard name without large or small subunit designation - later need kingdom test
                } else if (!NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase/oxygenase large subunit")
                           && !NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase/oxygenase small subunit")) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_RubiscoProblem, 
                             "Nonstandard ribulose bisphosphate protein name", feat);
                }
            }
        }



        ValidateCharactersInField (*it, "Protein name", feat);
        if (ContainsSgml(*it)) {
            PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                     "protein name " + *it + " has SGML",
                     feat);
        }
    }

    if (prot.IsSetDesc() && ContainsSgml(prot.GetDesc())) {
        PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                 "protein description " + prot.GetDesc() + " has SGML",
                 feat);
    }

    if (prot.IsSetDesc() && feat.IsSetComment() 
        && NStr::EqualCase(prot.GetDesc(), feat.GetComment())) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_RedundantFields, 
                 "Comment has same value as protein description",
                 feat);
    }

    if (feat.IsSetComment() && HasECnumberPattern(feat.GetComment())) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_EcNumberProblem, 
                 "Apparent EC number in protein comment", feat);
    }

    if ( prot.CanGetDb () ) {
        m_Imp.ValidateDbxref(prot.GetDb(), feat);
    }
    if ( (!prot.IsSetName() || prot.GetName().empty()) && 
         (!prot.IsSetProcessed() 
          || (prot.GetProcessed() != CProt_ref::eProcessed_signal_peptide
              && prot.GetProcessed() !=  CProt_ref::eProcessed_transit_peptide))) {
        if (prot.IsSetDesc() && !NStr::IsBlank (prot.GetDesc())) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
                "Protein feature has description but no name", feat);
        } else if (prot.IsSetActivity() && !prot.GetActivity().empty()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
                "Protein feature has function but no name", feat);
        } else if (prot.IsSetEc() && !prot.GetEc().empty()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
                "Protein feature has EC number but no name", feat);
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
                "Protein feature has no name", feat);
        }
    }
    
    x_ValidateProtECNumbers (prot, feat);
}


void CValidError_feat::x_ReportUninformativeNames(const CProt_ref& prot, const CSeq_feat& feat) 
{
    FOR_EACH_NAME_ON_PROTREF (it, prot) {
        string search = *it;
        search = NStr::ToLower(search);
        if (sc_BadProtName.find (search.c_str()) != sc_BadProtName.end()
            || NStr::Find(search, "=") != string::npos
            || NStr::Find(search, "~") != string::npos
            || NStr::FindNoCase(search, "uniprot") != string::npos
            || NStr::FindNoCase(search, "uniprotkb") != string::npos
            || NStr::FindNoCase(search, "pmid") != string::npos
            || NStr::FindNoCase(search, "dbxref") != string::npos) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_UndesiredProteinName, 
                     "Uninformative protein name '" + *it + "'",
                     feat);
        }
    }
}


void CValidError_feat::x_ValidateProtECNumbers(const CProt_ref& prot, const CSeq_feat& feat) 
{
    FOR_EACH_ECNUMBER_ON_PROTREF (it, prot) {
        if (NStr::IsBlank (*it)) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_EcNumberProblem, "EC number should not be empty", feat);
        } else if (!s_IsValidECNumberFormat(*it)) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberFormat,
                    (*it) + " is not in proper EC_number format", feat);
        } else {
            const string& ec_number = *it;
            CProt_ref::EECNumberStatus status = CProt_ref::GetECNumberStatus (ec_number);
            switch (status) {
                case CProt_ref::eEC_deleted:
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberValue, 
                             "EC_number " + ec_number + " was deleted",
                             feat);
                    break;
                case CProt_ref::eEC_replaced:
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberValue, 
                             "EC_number " + ec_number + " was transferred and is no longer valid",
                             feat);
                    break;
                case CProt_ref::eEC_unknown:
          {
              size_t pos = NStr::Find (ec_number, "n");
              if (pos == string::npos || !isdigit (ec_number.c_str()[pos + 1])) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberValue, 
                                       ec_number + " is not a legal value for qualifier EC_number",
                                         feat);
              } else {
                            PostErr (eDiag_Info, eErr_SEQ_FEAT_BadEcNumberValue, 
                                       ec_number + " is not a legal preliminary value for qualifier EC_number",
                                         feat);
              }
          }
                    break;
                default:
                    break;
            }
        }
    }

}


static bool s_EqualGene_ref(const CGene_ref& genomic, const CGene_ref& mrna)
{
    bool locus = (!genomic.CanGetLocus()  &&  !mrna.CanGetLocus())  ||
        (genomic.CanGetLocus()  &&  mrna.CanGetLocus()  &&
        genomic.GetLocus() == mrna.GetLocus());
    bool allele = (!genomic.CanGetAllele()  &&  !mrna.CanGetAllele())  ||
        (genomic.CanGetAllele()  &&  mrna.CanGetAllele()  &&
        genomic.GetAllele() == mrna.GetAllele());
    bool desc = (!genomic.CanGetDesc()  &&  !mrna.CanGetDesc())  ||
        (genomic.CanGetDesc()  &&  mrna.CanGetDesc()  &&
        genomic.GetDesc() == mrna.GetDesc());
    bool locus_tag = (!genomic.CanGetLocus_tag()  &&  !mrna.CanGetLocus_tag())  ||
        (genomic.CanGetLocus_tag()  &&  mrna.CanGetLocus_tag()  &&
        genomic.GetLocus_tag() == mrna.GetLocus_tag());

    return locus  &&  allele  &&  desc  && locus_tag;
}


void CValidError_feat::ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat) 
{
    CRNA_ref::EType rna_type = CRNA_ref::eType_unknown;
    if (rna.IsSetType()) {
        rna_type = rna.GetType();
    }

    if ( rna_type != CRNA_ref::eType_tRNA ) {
        if ( rna.CanGetExt() && rna.GetExt().IsTRNA () ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidForType,
                "tRNA data structure on non-tRNA feature", feat);
        }
    }

    /*
    if ( rna_type == CRNA_ref::eType_miscRNA ) {
        if ( rna.CanGetExt() && rna.GetExt().IsGen() ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidForType,
                "RNA-gen data structure on miscRNA feature", feat);
        }
    }
    */

    bool feat_pseudo = feat.IsSetPseudo() && feat.GetPseudo();
    bool gene_pseudo = IsOverlappingGenePseudo(feat);
    bool pseudo = feat_pseudo || gene_pseudo;

    if ( rna_type == CRNA_ref::eType_mRNA ) {        
        if ( !pseudo ) {
            ValidateMrnaTrans(feat);      /* transcription check */
            ValidateSplice(feat);
        }
        ValidateCommonMRNAProduct(feat);

        FOR_EACH_GBQUAL_ON_FEATURE (it, feat) {
            const CGb_qual& qual = **it;
            if ( qual.CanGetQual() ) {
                const string& key = qual.GetQual();
                if ( NStr::EqualNocase(key, "protein_id") ) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                        "protein_id should not be a gbqual on an mRNA feature", feat);
                } else if ( NStr::EqualNocase(key, "transcript_id") ) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                        "transcript_id should not be a gbqual on an mRNA feature", feat);
                }
            }
        }
        
        if (rna.CanGetExt() && rna.GetExt().IsName()) {
            const string& rna_name = rna.GetExt().GetName();
            if (NStr::StartsWith (rna_name, "transfer RNA ") &&
                (! NStr::EqualNocase(rna_name, "transfer RNA nucleotidyltransferase")) &&
                (! NStr::EqualNocase(rna_name, "transfer RNA methyltransferase"))) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_tRNAmRNAmixup,
                    "mRNA feature product indicates it should be a tRNA feature", feat);
            }
            ValidateCharactersInField (rna_name, "mRNA name", feat);
            if (ContainsSgml(rna_name)) {
                PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                    "mRNA name " + rna_name + " has SGML", feat);
            }
        }
    }

    if ( rna.CanGetExt()  &&
         rna.GetExt().Which() == CRNA_ref::C_Ext::e_TRNA ) {
        const CTrna_ext& trna = rna.GetExt ().GetTRNA ();
        if ( trna.CanGetAnticodon () ) {
            const CSeq_loc& anticodon = trna.GetAnticodon();
            size_t anticodon_len = GetLength(anticodon, m_Scope);
            if ( anticodon_len != 3 ) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_Range,
                    "Anticodon is not 3 bases in length", feat);
            }
            ECompare comp = sequence::Compare(anticodon,
                                              feat.GetLocation(),
                                              m_Scope);
            if ( comp != eContained  &&  comp != eSame ) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_Range,
                    "Anticodon location not in tRNA", feat);
            }
            ValidateAnticodon(anticodon, feat);
        }
        ValidateTrnaCodons(trna, feat);
    }

    if ( rna_type == CRNA_ref::eType_tRNA ) {
        FOR_EACH_GBQUAL_ON_FEATURE (gbqual, feat) {
            if ( NStr::CompareNocase((**gbqual).GetQual (), "anticodon") == 0 ) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Unparsed anticodon qualifier in tRNA", feat);
            } else if (NStr::CompareNocase ((**gbqual).GetQual (), "product") == 0 ) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Unparsed product qualifier in tRNA", feat);
            }
        }
        /* tRNA with string extension */
        if ( rna.IsSetExt()  &&  
             rna.GetExt().Which () == CRNA_ref::C_Ext::e_Name ) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                "Unparsed product qualifier in tRNA", feat);
        } else if (!rna.IsSetExt() || rna.GetExt().Which() == CRNA_ref::C_Ext::e_not_set ) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_MissingTrnaAA,
                "Missing encoded amino acid qualifier in tRNA", feat);
        }
    }

    if ( rna_type == CRNA_ref::eType_unknown ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_RNAtype0,
            "RNA type 0 (unknown) not supported", feat);
    }

    if (rna_type == CRNA_ref::eType_rRNA) {
        if (rna.CanGetExt() && rna.GetExt().IsName()) {
            const string& rna_name = rna.GetExt().GetName();
            ValidateCharactersInField (rna_name, "rRNA name", feat);
            if (ContainsSgml(rna_name)) {
                PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                    "rRNA name " + rna_name + " has SGML", feat);
            }
        }
    }

    if (rna_type == CRNA_ref::eType_rRNA
        || rna_type == CRNA_ref::eType_snRNA
        || rna_type == CRNA_ref::eType_scRNA
        || rna_type == CRNA_ref::eType_snoRNA) {
        if (!rna.IsSetExt() || !rna.GetExt().IsName() || NStr::IsBlank(rna.GetExt().GetName())) {
            if (!pseudo) {
                string rna_typename = CRNA_ref::GetRnaTypeName(rna_type);
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue, 
                         rna_typename + " has no name", feat);
            }
        }
    }

    if ( feat.IsSetProduct() ) {
        ValidateRnaProductType(rna, feat);

        if (!feat.IsSetExcept_text() 
            || NStr::FindNoCase (feat.GetExcept_text(), "transcribed pseudogene") == string::npos) {
            if (feat_pseudo) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_PseudoRnaHasProduct,
                         "A pseudo RNA should not have a product", feat);
            } else if (gene_pseudo) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_PseudoRnaViaGeneHasProduct,
                         "An RNA overlapped by a pseudogene should not have a product", feat);
            }
        }
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
        bool chk = true;
        if (curr.GetEmbeddingSeq_loc().IsInt()) {
            chk = sequence::IsValid(curr.GetEmbeddingSeq_loc().GetInt(), m_Scope);
        } else if (curr.GetEmbeddingSeq_loc().IsPnt()) {
            chk = sequence::IsValid(curr.GetEmbeddingSeq_loc().GetPnt(), m_Scope);
        } else {
            continue;
        }

        if ( !chk ) {
            string lbl;
            curr.GetEmbeddingSeq_loc().GetLabel(&lbl);
            PostErr(eDiag_Critical, eErr_SEQ_FEAT_Range,
                "Anticodon location [" + lbl + "] out of range", feat);
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

    ENa_strand loc_strand = feat.GetLocation().GetStrand();
    ENa_strand ac_strand = anticodon.GetStrand();
    if (loc_strand == eNa_strand_minus && ac_strand != eNa_strand_minus) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_BadAnticodonStrand, 
                 "Anticodon should be on minus strand", feat);
    } else if (loc_strand != eNa_strand_minus && ac_strand == eNa_strand_minus) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_BadAnticodonStrand, 
                 "Anticodon should be on plus strand", feat);
    }

    // trans splicing exception turns off both mixed_strand and out_of_order messages
    bool trans_splice = false;
    if (feat.CanGetExcept()  &&  feat.GetExcept()  && feat.CanGetExcept_text()) {
        if (NStr::FindNoCase(feat.GetExcept_text(), "trans-splicing") != NPOS) {
            trans_splice = true;
        }
    }
    if (!trans_splice) {
        string loc_lbl = "";
        anticodon.GetLabel(&loc_lbl);
        if (mixed_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                "Mixed strands in Anticodon [" + loc_lbl + "]", feat);
        }
        if (unmarked_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                "Mixed plus and unknown strands in Anticodon [" + loc_lbl + "]", feat);
        }
        if (!ordered) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqLocOrder,
                "Intervals out of order in Anticodon [" + loc_lbl + "]", feat);
        }
    }
}


void CValidError_feat::ValidateRnaProductType
(const CRNA_ref& rna,
 const CSeq_feat& feat)
{
    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
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
                               74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 
                               84, 85, 86, 87, 88, 89, 90 };

// in Ncbistdaa order
static const char* kAANames[] = {
    "---", "Ala", "Asx", "Cys", "Asp", "Glu", "Phe", "Gly", "His", "Ile",
    "Lys", "Leu", "Met", "Asn", "Pro", "Gln", "Arg", "Ser", "Thr", 
    "Val", "Trp", "OTHER", "Tyr", "Glx", "Sec", "TERM", "Pyl", "Xle"
};


const char* GetAAName(unsigned char aa, bool is_ascii)
{
    try {
        if (is_ascii) {
            aa = CSeqportUtil::GetMapToIndex
                (CSeq_data::e_Ncbieaa, CSeq_data::e_Ncbistdaa, aa);
        }
        return (aa < sizeof(kAANames)/sizeof(*kAANames)) ? kAANames[aa] : "OTHER";
    } catch (CException ) {
        return "OTHER";
    } catch (std::exception ) {
        return "OTHER";
    }
}


static string GetGeneticCodeName (int gcode)
{
    const CGenetic_code_table& code_table = CGen_code_table::GetCodeTable();
    const list<CRef<CGenetic_code> >& codes = code_table.Get();

    for ( list<CRef<CGenetic_code> >::const_iterator code_it = codes.begin(), code_it_end = codes.end();  code_it != code_it_end;  ++code_it ) {
        if ((*code_it)->GetId() == gcode) {
            return (*code_it)->GetName();
        }
    }
    return "unknown";
}

void CValidError_feat::ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat)
{
    if (!trna.IsSetAa()) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_BadTrnaAA, "Missing tRNA amino acid", feat);
        return;
    }

    unsigned char aa = 0, orig_aa;
    vector<char> seqData;
    string str = "";
    
    switch (trna.GetAa().Which()) {
        case CTrna_ext::C_Aa::e_Iupacaa:
            str = trna.GetAa().GetIupacaa();
            CSeqConvert::Convert(str, CSeqUtil::e_Iupacaa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
            aa = seqData[0];
            break;
        case CTrna_ext::C_Aa::e_Ncbi8aa:
            str = trna.GetAa().GetNcbi8aa();
            CSeqConvert::Convert(str, CSeqUtil::e_Ncbi8aa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
            aa = seqData[0];
            break;
        case CTrna_ext::C_Aa::e_Ncbistdaa:
            str = trna.GetAa().GetNcbi8aa();
            CSeqConvert::Convert(str, CSeqUtil::e_Ncbistdaa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
            aa = seqData[0];
            break;
        case CTrna_ext::C_Aa::e_Ncbieaa:
            seqData.push_back(trna.GetAa().GetNcbieaa());
            aa = seqData[0];
            break;
        default:
            NCBI_THROW (CCoreException, eCore, "Unrecognized tRNA aa coding");
            break;
    }

    // make sure the amino acid is valid
    bool found = false;
    for ( unsigned int i = 0; i < sizeof (s_LegalNcbieaaValues) / sizeof (int); ++i ) {
        if ( aa == s_LegalNcbieaaValues[i] ) {
            found = true;
            break;
        }
    }
    orig_aa = aa;
    if ( !found ) {
        aa = ' ';
    }

    // Retrive the Genetic code id for the tRNA
    int gcode = 1;
    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
    if ( bsh ) {
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

    string codename = GetGeneticCodeName (gcode);
    char buf[2];
    buf[0] = aa;
    buf[1] = 0;
    string aaname = buf;
    aaname += "/";
    aaname += GetAAName (aa, true);
        
    EDiagSev sev = (aa == 'U' || aa == 'O') ? eDiag_Warning : eDiag_Error;

    bool modified_codon_recognition = false;
    bool rna_editing = false;
    if ( feat.CanGetExcept_text() ) {
        string excpt_text = feat.GetExcept_text();
        if ( NStr::FindNoCase(excpt_text, "modified codon recognition") != NPOS ) {
            modified_codon_recognition = true;
        }
        if ( NStr::FindNoCase(excpt_text, "RNA editing") != NPOS ) {
            rna_editing = true;
        }
    }

    vector<string> recognized_codon_values;
    vector<unsigned char> recognized_taa_values;

    ITERATE( CTrna_ext::TCodon, iter, trna.GetCodon() ) {
        if (*iter == 255) continue;
        // test that codon value is in range 0 - 63
        if ( *iter > 63 ) {
            PostErr(sev, eErr_SEQ_FEAT_BadTrnaCodon,
                "tRNA codon value " + NStr::IntToString(*iter) + 
                " is greater than maximum 63", feat);
            continue;
        }

        if ( !modified_codon_recognition && !rna_editing ) {
            unsigned char taa = ncbieaa[*iter];
            string codon = CGen_code_table::IndexToCodon(*iter);
            recognized_codon_values.push_back (codon);
            recognized_taa_values.push_back (taa);

            if ( taa != aa ) {
                if ( (aa == 'U')  &&  (taa == '*')  &&  (*iter == 14) ) {
                    // selenocysteine normally uses TGA (14), so ignore without requiring exception in record
                    // TAG (11) is used for pyrrolysine in archaebacteria
                    // TAA (10) is not yet known to be used for an exceptional amino acid
                } else {
                    NStr::ReplaceInPlace (codon, "T", "U");

                    PostErr(sev, eErr_SEQ_FEAT_TrnaCodonWrong,
                      "Codon recognized by tRNA (" + codon + ") does not match amino acid (" 
                       + aaname + ") specified by genetic code ("
                       + NStr::IntToString (gcode) + "/" + codename + ")", feat);
                }
            }
        }
    }

    // see if anticodon is compatible with codons recognized and amino acid
    string anticodon = "?";
    vector<string> codon_values;
    vector<unsigned char> taa_values;

    if (trna.IsSetAnticodon() && GetLength (trna.GetAnticodon(), m_Scope) == 3) {
        try {
            anticodon = GetSequenceStringFromLoc(trna.GetAnticodon(), *m_Scope);
            // get reverse complement sequence for location
            CRef<CSeq_loc> codon_loc (new CSeq_loc());
            codon_loc->Add (trna.GetAnticodon());
            if (codon_loc->GetStrand() == eNa_strand_minus) {
                codon_loc->SetStrand (eNa_strand_plus);
            } else {
                codon_loc->SetStrand (eNa_strand_minus);
            }
            string codon = GetSequenceStringFromLoc(*codon_loc, *m_Scope);
            if (codon.length() > 3) {
                codon = codon.substr (0, 3);
            }

            // expand wobble base to known binding partners
            string wobble = "";


            char ch = anticodon.c_str()[0];
            switch (ch) {
                case 'A' :
                    wobble = "ACT";
                    break;
                case 'C' :
                    wobble = "G";
                    break;
                case 'G' :
                    wobble = "CT";
                    break;
                case 'T' :
                    wobble = "AG";
                    break;
                default :
                    break;
            }
            if (!NStr::IsBlank(wobble)) {
                string::iterator str_it = wobble.begin();
                while (str_it != wobble.end()) {
                    codon[2] = *str_it;
                    int index = CGen_code_table::CodonToIndex (codon);
                    if (index < 64 && index > -1) {
                        unsigned char taa = ncbieaa[index];
                        taa_values.push_back(taa);
                        codon_values.push_back(codon);
                    }
                    ++str_it;
                }
            }
            NStr::ReplaceInPlace (anticodon, "T", "U");
            if (anticodon.length() > 3) {
                anticodon = anticodon.substr(0, 3);
            }
            } catch (CException ) {
            } catch (std::exception ) {
        }

        if (codon_values.size() > 0) {
            bool ok = false;
            // check that codons predicted from anticodon can transfer indicated amino acid
            for (size_t i = 0; i < codon_values.size(); i++) {
                if (!NStr::IsBlank (codon_values[i]) && aa == taa_values[i]) {
                    ok = true;
                }
            }
            if (!ok) {
                if (aa == 'U' && NStr::Equal (anticodon, "UCA")) {
                    // ignore TGA codon for selenocysteine
                } else if (aa == 'O' && NStr::Equal (anticodon, "CUA")) {
                    // ignore TAG codon for pyrrolysine
                } else if (!feat.IsSetExcept_text()
                          || (NStr::FindNoCase(feat.GetExcept_text(), "modified codon recognition") == string::npos 
                              &&NStr::FindNoCase(feat.GetExcept_text(), "RNA editing") == string::npos)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadAnticodonAA,
                              "Codons predicted from anticodon (" + anticodon
                              + ") cannot produce amino acid (" + aaname + ")",
                              feat);
                }
            }

            // check that codons recognized match codons predicted from anticodon
            if (recognized_codon_values.size() > 0) {
                bool ok = false;
                for (size_t i = 0; i < codon_values.size() && !ok; i++) {
                    for (size_t j = 0; j < recognized_codon_values.size() && !ok; j++) {
                        if (NStr::Equal (codon_values[i], recognized_codon_values[j])) {
                            ok = true;
                        }
                    }
                }
                if (!ok 
                    && (!feat.IsSetExcept_text() 
                        || NStr::FindNoCase (feat.GetExcept_text(), "RNA editing") == string::npos)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadAnticodonCodon,
                             "Codon recognized cannot be produced from anticodon ("
                             + anticodon + ")", feat);
                }
            }
        }
    }

    if (!feat.IsSetPseudo() || !feat.GetPseudo()) {
        if (orig_aa == 0 || orig_aa == 255) {
            PostErr (sev, eErr_SEQ_FEAT_BadTrnaAA, "Missing tRNA amino acid", feat);
        } else {
            // verify that legal amino acid is indicated
            unsigned int idx;
            if (aa != '*') {
                idx = aa - 64;
            } else {
                idx = 25;
            }
            if (idx == 0 || idx >= 28) {
                PostErr (sev, eErr_SEQ_FEAT_BadTrnaAA, "Invalid tRNA amino acid", feat);
            }
        }
    }
}


void CValidError_feat::ValidateGapFeature (const CSeq_feat& feat)
{
    int loc_len = GetLength (feat.GetLocation(), m_Scope);
    // look for estimated length qualifier
    FOR_EACH_GBQUAL_ON_SEQFEAT (it, feat) {
        if ((*it)->IsSetQual() && NStr::EqualNocase ((*it)->GetQual(), "estimated_length")
            && (*it)->IsSetVal() && !NStr::EqualNocase ((*it)->GetVal(), "unknown")) {
            try {
                int estimated_length = NStr::StringToInt ((*it)->GetVal());
                if (estimated_length != loc_len) {
                    PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                             "Gap feature estimated_length " + NStr::IntToString (estimated_length)
                             + " does not match " + NStr::IntToString (loc_len) + " feature length",
                             feat);
                    return;
                }
            } catch (CException ) {
            } catch (std::exception ) {
            }
        }
    }
    try {
        string s_data = GetSequenceStringFromLoc(feat.GetLocation(), *m_Scope);
        CSeqVector vec = GetSequenceFromFeature(feat, *m_Scope);
        if ( !vec.empty() ) {
            string vec_data;
            vec.GetSeqData(0, vec.size(), vec_data);
            int num_n = 0;
            int num_real = 0;
            unsigned int num_gap = 0;
            int pos = 0;
            string::iterator it = vec_data.begin();
            while (it != vec_data.end()) {
                if (*it == 'N') {
                    if (vec.IsInGap(pos)) {
                        // gap not N
                        num_gap++;
                    } else {
                        num_n++;
                    }
                } else if (*it != '-') {
                    num_real++;
                }
                ++it;
                ++pos;
            }
            if (num_real > 0 && num_n > 0) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                         "Gap feature over " + NStr::IntToString (num_real)
                         + " real bases and " + NStr::IntToString (num_n)
                         + " Ns", feat);
            } else if (num_real > 0) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                         "Gap feature over " + NStr::IntToString (num_real)
                         + " real bases", feat);
            } else if (num_n > 0) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                         "Gap feature over " + NStr::IntToString (num_n)
                         + " Ns", feat);
            } else if (num_gap != GetLength (feat.GetLocation(), m_Scope)) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                         "Gap feature estimated_length " + NStr::IntToString (loc_len)
                         + " does not match " + NStr::IntToString (num_gap)
                         + " gap characters", feat);
            }
        }

    } catch (CException ) {
    } catch (std::exception ) {
    }

}


void CValidError_feat::ValidateImp(const CImp_feat& imp, const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();

    const string& key = imp.GetKey();
    if (NStr::IsBlank(key)) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey, 
                "NULL feature key", feat);
        return;
    }

    if (subtype == CSeqFeatData::eSubtype_imp || subtype == CSeqFeatData::eSubtype_bad) {
        if (NStr::Equal(key, "mRNA")) {
            subtype = CSeqFeatData::eSubtype_mRNA;
        } else if (NStr::Equal(key, "tRNA")) {
            subtype = CSeqFeatData::eSubtype_tRNA;
        } else if (NStr::Equal(key, "tRNA")) {
            subtype = CSeqFeatData::eSubtype_tRNA;
        } else if (NStr::Equal(key, "rRNA")) {
            subtype = CSeqFeatData::eSubtype_rRNA;
        } else if (NStr::Equal(key, "snRNA")) {
            subtype = CSeqFeatData::eSubtype_snRNA;
        } else if (NStr::Equal(key, "scRNA")) {
            subtype = CSeqFeatData::eSubtype_scRNA;
        } else if (NStr::Equal(key, "snoRNA")) {
            subtype = CSeqFeatData::eSubtype_snoRNA;
        } else if (NStr::Equal(key, "misc_RNA")) {
            subtype = CSeqFeatData::eSubtype_misc_RNA;
        } else if (NStr::Equal(key, "precursor_RNA")) {
            subtype = CSeqFeatData::eSubtype_precursor_RNA;
        } else if (NStr::EqualNocase (key, "mat_peptide")) {
            subtype = CSeqFeatData::eSubtype_mat_peptide;
        } else if (NStr::EqualNocase (key, "sig_peptide")) {
            subtype = CSeqFeatData::eSubtype_sig_peptide;
        } else if (NStr::EqualNocase (key, "transit_peptide")) {
            subtype = CSeqFeatData::eSubtype_transit_peptide;
        } else if (NStr::EqualNocase (key, "preprotein")
            || NStr::EqualNocase(key, "proprotein")) {
            subtype = CSeqFeatData::eSubtype_preprotein;
        } else if (NStr::EqualNocase (key, "virion")) {
            subtype = CSeqFeatData::eSubtype_virion;
        } else if (NStr::EqualNocase(key, "mutation")) {
            subtype = CSeqFeatData::eSubtype_mutation;
        } else if (NStr::EqualNocase(key, "allele")) {
            subtype = CSeqFeatData::eSubtype_allele;
        } else if (NStr::EqualNocase (key, "CDS")) {
            subtype = CSeqFeatData::eSubtype_Imp_CDS;
        } else if (NStr::EqualNocase(key, "Import")) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_UnknownImpFeatKey,
                    "Feature key Import is no longer legal", feat);
            return;
        }
    }

    switch ( subtype ) {
    case CSeqFeatData::eSubtype_exon:
        if ( m_Imp.IsValidateExons() ) {
            ValidateSplice(feat, true);
        }
        break;

    case CSeqFeatData::eSubtype_bad:
    case CSeqFeatData::eSubtype_site_ref:
    case CSeqFeatData::eSubtype_gene:
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey, 
            "Unknown feature key " + key, feat);
        break;

    case CSeqFeatData::eSubtype_virion:
    case CSeqFeatData::eSubtype_mutation:
    case CSeqFeatData::eSubtype_allele:
        PostErr(eDiag_Error, eErr_SEQ_FEAT_UnknownImpFeatKey,
            "Feature key " + key + " is no longer legal", feat);
        break;
    case CSeqFeatData::eSubtype_misc_feature:
        if ((!feat.IsSetComment() || NStr::IsBlank (feat.GetComment()))
            && (!feat.IsSetQual() || feat.GetQual().empty())
            && (!feat.IsSetDbxref() || feat.GetDbxref().empty())) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NeedsNote,
                    "A note or other qualifier is required for a misc_feature", feat);
        }
        break;
    case CSeqFeatData::eSubtype_polyA_site:
        {
            CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
            if ( range.GetFrom() != range.GetTo() ) {
                EDiagSev sev = eDiag_Warning;
                if (m_Imp.IsRefSeq()) {
                    sev = eDiag_Error;
                }
                PostErr(sev, eErr_SEQ_FEAT_PolyAsiteNotPoint,
                    "PolyA_site should be a single point", feat);
            }
        }
        break;

    case CSeqFeatData::eSubtype_polyA_signal:
      {
            CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
            if ( range.GetFrom() == range.GetTo() ) {
                EDiagSev sev = eDiag_Warning;
                if (m_Imp.IsRefSeq()) {
                    sev = eDiag_Error;
                }
                PostErr (sev, eErr_SEQ_FEAT_PolyAsignalNotRange, "PolyA_signal should be a range", feat);
            }
      }
      break;

    case CSeqFeatData::eSubtype_mat_peptide:
    case CSeqFeatData::eSubtype_sig_peptide:
    case CSeqFeatData::eSubtype_transit_peptide:
        if (m_Imp.IsEmbl() || m_Imp.IsDdbj()) {
            const CSeq_loc& loc = feat.GetLocation();
            CConstRef<CSeq_feat> cds = GetOverlappingCDS(loc, *m_Scope);
            PostErr(!cds ? eDiag_Error : eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatureLacksCDS,
                "sig/mat/transit_peptide feature cannot be associated with a "
                "protein product of a coding region feature", feat);
        } else {
            PostErr(m_Imp.IsRefSeq() ? eDiag_Error : eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatureLacksCDS,
                "Peptide processing feature should be converted to the "
                "appropriate protein feature subtype", feat);
        }
        ValidatePeptideOnCodonBoundry(feat, key);
        break;
    case CSeqFeatData::eSubtype_preprotein:
        if (m_Imp.IsEmbl() || m_Imp.IsDdbj()) {
            const CSeq_loc& loc = feat.GetLocation();
            CConstRef<CSeq_feat> cds = GetOverlappingCDS(loc, *m_Scope);
            PostErr(!cds ? eDiag_Error : eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatureLacksCDS,
                "Pre/pro protein feature cannot be associated with a "
                "protein product of a coding region feature", feat);
        } else {
            PostErr(m_Imp.IsRefSeq() ? eDiag_Error : eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatureLacksCDS,
                "Peptide processing feature should be converted to the "
                "appropriate protein feature subtype", feat);
        }
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

            FOR_EACH_GBQUAL_ON_FEATURE (gbqual, feat) {
                if ( NStr::CompareNocase( (*gbqual)->GetQual(), "translation") == 0 ) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpCDShasTranslation,
                        "ImpFeat CDS with /translation found", feat);
                }
            }
        }
        break;
    case CSeqFeatData::eSubtype_imp:
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey,
            "Unknown feature key " + key, feat);
        break;
    case CSeqFeatData::eSubtype_gap:
        ValidateGapFeature (feat);
        break;
    case CSeqFeatData::eSubtype_intron:
        ValidateIntron (feat);
        break;
    case CSeqFeatData::eSubtype_assembly_gap:
        {
            bool is_far_delta = false;
            const CSeq_loc& location = feat.GetLocation();
            CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, location);
            if ( bsh.IsSetInst_Repr()) {
                CBioseq_Handle::TInst::TRepr repr = bsh.GetInst_Repr();
                if ( repr == CSeq_inst::eRepr_delta ) {
                    is_far_delta = true;
                    const CBioseq& seq = *(bsh.GetCompleteBioseq());
                    const CSeq_inst& inst = seq.GetInst();
                    ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
                        if ( !(*sg) ) continue;
                        if (( (**sg).Which() == CDelta_seq::e_Loc )) continue;
                        is_far_delta = false;
                    }
                }
            }
            if (! is_far_delta) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_GapFeatureProblem,
                        "An assembly_gap feature should only be on a contig record", feat);
            }
            if (location.IsInt()) {
                TSeqPos from = location.GetInt().GetFrom();
                TSeqPos to = location.GetInt().GetTo();
                CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
                string sequence;
                bool is5 = false;
                bool is3 = false;
                long int count = 0;
                vec.GetSeqData(from - 1, from, sequence);
                if (NStr::Equal (sequence, "N")) {
                    is5 = true;
                }
                vec.GetSeqData(to + 1, to + 2, sequence);
                if (NStr::Equal (sequence, "N")) {
                    is3 = true;
                }
                EDiagSev sv = eDiag_Warning;
                if (m_Imp.IsGenomeSubmission()) {
                    sv = eDiag_Error;
                }
                if (is5 && is3) {
                    PostErr(sv, eErr_SEQ_FEAT_AssemblyGapAdjacentToNs,
                            "Assembly_gap flanked by Ns on 5' and 3' sides", feat);
                } else if (is5) {
                    PostErr(sv, eErr_SEQ_FEAT_AssemblyGapAdjacentToNs,
                            "Assembly_gap flanked by Ns on 5' side", feat);
                } else if (is3) {
                    PostErr(sv, eErr_SEQ_FEAT_AssemblyGapAdjacentToNs,
                            "Assembly_gap flanked by Ns on 3' side", feat);
                }
                vec.GetSeqData(from, to + 1, sequence);
                for (size_t i = 0; i < sequence.size(); i++) {
                    if (sequence[i] != 'N') {
                        count++;
                    }
                }
                if (count > 0) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_AssemblyGapCoversSequence, "Assembly_gap extends into sequence", feat);
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
                    temp_loc + " for feature " + key, feat);
            }
        }
    }

    if ( feat.CanGetQual() ) {
        ValidateImpGbquals(imp, feat);
    }
    
    // Make sure a feature has its mandatory qualifiers
    ITERATE (CSeqFeatData::TQualifiers, required, CSeqFeatData::GetMandatoryQualifiers(subtype)) {
        bool found = false;
        FOR_EACH_GBQUAL_ON_FEATURE (qual, feat) {
            if ((*qual)->IsSetQual() && CSeqFeatData::GetQualifierType((*qual)->GetQual()) == *required) {
                found = true;
                break;
            }
        }
        if (!found && *required == CSeqFeatData::eQual_citation) {
            if (feat.IsSetCit()) {
                found = true;
            } else if (feat.IsSetComment() && !NStr::IsBlank(feat.GetComment())) {
                // RefSeq allows conflict with accession in comment instead of sfp->cit
                CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
                if (bsh) {
                    FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetCompleteBioseq())) {
                        if ((*it)->IsOther()) {
                            found = true;
                            break;
                        }
                    }
                }
            }
            if (!found 
                && (NStr::EqualNocase (key, "conflict") 
                    || NStr::EqualNocase (key, "old_sequence"))) {
                // compare qualifier can now substitute for citation qualifier for conflict and old_sequence
                FOR_EACH_GBQUAL_ON_FEATURE (qual, feat) {
                    if ((*qual)->IsSetQual() && CSeqFeatData::GetQualifierType((*qual)->GetQual()) == CSeqFeatData::eQual_compare) {
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingQualOnImpFeat,
                "Missing qualifier " + CSeqFeatData::GetQualifierAsString(*required) +
                " for feature " + key, feat);
        }
    }
}


void CValidError_feat::ValidateNonImpFeat (const CSeq_feat& feat)
{
    string key = feat.GetData().GetKey();

    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();

    ValidateNonImpFeatGbquals (feat);

    // look for mandatory qualifiers
    EDiagSev sev = eDiag_Warning;

    ITERATE (CSeqFeatData::TQualifiers, required, CSeqFeatData::GetMandatoryQualifiers(subtype)) {
        bool found = false;
        FOR_EACH_GBQUAL_ON_FEATURE (qual, feat) {
            if ((*qual)->IsSetQual() && CSeqFeatData::GetQualifierType((*qual)->GetQual()) == *required) {
                found = true;
                break;
            }
        }

        if (!found) {
            if (*required == CSeqFeatData::eQual_citation) {
                if (feat.IsSetCit()) {
                    found = true;
                } else if (feat.IsSetComment() && NStr::EqualNocase (key, "conflict")) {
                    // RefSeq allows conflict with accession in comment instead of sfp->cit
                    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
                    FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetCompleteBioseq())) {
                        if ((*it)->IsOther()) {
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        if (!found && (NStr::EqualNocase (key, "conflict") || NStr::EqualNocase (key, "old_sequence"))) {
            FOR_EACH_GBQUAL_ON_SEQFEAT (qual, feat) {
                if ((*qual)->IsSetQual() && NStr::EqualNocase ((*qual)->GetQual(), "compare")
                    && (*qual)->IsSetVal() && !NStr::IsBlank ((*qual)->GetVal())) {
                    found = true;
                    break;
                }
            }
        }
        if (!found && *required == CSeqFeatData::eQual_ncRNA_class) {
            sev = eDiag_Error;
            if (feat.GetData().IsRna() && feat.GetData().GetRna().IsSetExt()
                && feat.GetData().GetRna().GetExt().IsGen()
                && feat.GetData().GetRna().GetExt().GetGen().IsSetClass()
                && !NStr::IsBlank(feat.GetData().GetRna().GetExt().GetGen().GetClass())) {
                found = true;
            }
        }

        if (!found) {
            PostErr(sev, eErr_SEQ_FEAT_MissingQualOnFeature,
                "Missing qualifier " + CSeqFeatData::GetQualifierAsString(*required) +
                " for feature " + key, feat);
        }
    }
}



static bool s_RptUnitIsBaseRange (string str, TSeqPos& from, TSeqPos& to)

{
    if (str.length() > 25) {
        return false;
    }
    SIZE_TYPE pos = NStr::Find (str, "..");
    if (pos == string::npos) {
        return false;
    }

    int tmp_from, tmp_to;
    try {
        tmp_from = NStr::StringToInt (str.substr(0, pos));
        from = tmp_from;
        tmp_to = NStr::StringToInt (str.substr (pos + 2));
        to = tmp_to;
      } catch (CException ) {
          return false;
      } catch (std::exception ) {
        return false;
    }
    if (tmp_from < 0 || tmp_to < 0) {
        return false;
    }
    return true;
}

static bool s_StringConsistsOf (string str, string consist)
{
    const char *src = str.c_str();
    const char *find = consist.c_str();
    bool rval = true;

    while (*src != 0 && rval) {
        if (strchr (find, *src) == NULL) {
            rval = false;
        }
        src++;
    }
    return rval;
}


static bool s_IsValidRpt_typeQual (string val)
{
    bool error = false;

    if (NStr::StartsWith (val, "(") && NStr::EndsWith (val, ")")) {
        // look for list of values
        vector<string> rpt_types;
        NStr::Tokenize(val.substr (1, val.length() - 2), ",", rpt_types);
        ITERATE(vector<string>, it, rpt_types) {
            bool found_valid = false;
            for ( size_t i = 0; 
                  i < sizeof(s_LegalRepeatTypes) / sizeof(string) && !found_valid; 
                  ++i ) {
                if (NStr::EqualNocase (*it, s_LegalRepeatTypes[i])) {
                    found_valid = true;
                }
            }
            if (!found_valid) {
                error = true;
            }
        }
    } else {
        bool found_valid = false;
        for ( size_t i = 0; 
              i < sizeof(s_LegalRepeatTypes) / sizeof(string) && !found_valid; 
              ++i ) {
            if (NStr::EqualNocase (val, s_LegalRepeatTypes[i])) {
                found_valid = true;
            }
        }
        if (!found_valid) {
            error = true;
        }
    }
    return !error;
}


void CValidError_feat::ValidateRptUnitVal (const string& val, const string& key, const CSeq_feat& feat)
{
    bool /* found = false, */ multiple_rpt_unit = false;
    ITERATE(string, it, val) {
        if ( *it <= ' ' ) {
            /* found = true; */
        } else if ( *it == '('  ||  *it == ')'  ||
            *it == ','  ||  *it == '.'  ||
            isdigit((unsigned char)(*it)) ) {
            multiple_rpt_unit = true;
        }
    }
    /*
    if ( found || 
    (!multiple_rpt_unit && val.length() > 48) ) {
    error = true;
    }
    */
    if ( NStr::CompareNocase(key, "repeat_region") == 0  &&
         !multiple_rpt_unit ) {
        if (val.length() <= GetLength(feat.GetLocation(), m_Scope) ) {
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
                    if (NStr::FindNoCase (vec_data, val) == string::npos) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                            "repeat_region /rpt_unit and underlying "
                            "sequence do not match", feat);
                    }
                }
            }
        } else {
            PostErr(eDiag_Info, eErr_SEQ_FEAT_InvalidQualifierValue,
                "Length of rpt_unit_seq is greater than feature length", feat);
        }                            
    }
}


void CValidError_feat::ValidateRptUnitSeqVal (const string& val, const string& key, const CSeq_feat& feat)
{
    // do validation common to rpt_unit
    ValidateRptUnitVal (val, key, feat);

    // do the validation specific to rpt_unit_seq
    const char *cp = val.c_str();
    bool badchars = false;
    while (*cp != 0 && !badchars) {
        if (*cp < ' ') {
            badchars = true;
        } else if (*cp != '(' && *cp != ')' 
                   && !isdigit (*cp) && !isalpha (*cp) 
                   && *cp != ',' && *cp != ';') {
            badchars = true;
        }
        cp++;
    }
    if (badchars) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
            "/rpt_unit_seq has illegal characters", feat);
    }
}


void CValidError_feat::ValidateRptUnitRangeVal (const string& val, const CSeq_feat& feat)
{
    TSeqPos from = kInvalidSeqPos, to = kInvalidSeqPos;
    if (!s_RptUnitIsBaseRange(val, from, to)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                 "/rpt_unit_range is not a base range", feat);
    } else {
        CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
        if (from - 1 < range.GetFrom() || from - 1> range.GetTo() || to - 1 < range.GetFrom() || to - 1 > range.GetTo()) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_RptUnitRangeProblem,
                     "/rpt_unit_range is not within sequence length", feat);   
        } else {
            bool nulls_between = false;
            for ( CTypeConstIterator<CSeq_loc> lit = ConstBegin(feat.GetLocation()); lit; ++lit ) {
                if ( lit->Which() == CSeq_loc::e_Null ) {
                    nulls_between = true;
                }
            }
            if (nulls_between) {
                bool in_range = false;
                for ( CSeq_loc_CI it(feat.GetLocation()); it; ++it ) {
                    range = it.GetEmbeddingSeq_loc().GetTotalRange();
                    if (from - 1 < range.GetFrom() || from - 1> range.GetTo() || to - 1 < range.GetFrom() || to - 1 > range.GetTo()) {
                    } else {
                        in_range = true;
                    }
                }
                if (! in_range) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_RptUnitRangeProblem,
                             "/rpt_unit_range is not within ordered intervals", feat);   
                }
            }
        }
    }
}


void CValidError_feat::ValidateLabelVal (const string& val, const CSeq_feat& feat)
{
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
    if (only_digits  ||  has_spaces) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue, "Illegal value for qualifier label", feat);
    }
}


void CValidError_feat::ValidateCompareVal (const string& val, const CSeq_feat& feat)
{
    if (!NStr::StartsWith (val, "(")) {
        EAccessionFormatError valid_accession = ValidateAccessionString (val, true);  
        if (valid_accession == eAccessionFormat_missing_version) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                     val + " accession missing version for qualifier compare", feat);
        } else if (valid_accession == eAccessionFormat_bad_version) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                     val + " accession has bad version for qualifier compare", feat);
        } else if (valid_accession != eAccessionFormat_valid) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                     val + " is not a legal accession for qualifier compare", feat);
        } else if (m_Imp.IsINSDInSep() && NStr::Find (val, "_") != string::npos) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                     "RefSeq accession " + val + " cannot be used for qualifier compare", feat);
        }
    }
}


TGi CValidError_feat::x_SeqIdToGiNumber(
    const string& seq_id,
    const string database_name )
{
    TGi gi = ZERO_GI;
    try {
        CRef<CSeq_id> id(new CSeq_id(seq_id));
        if (id->IsGenbank()) {
            gi = GI_FROM(TIntId, 1);
        }
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
        gi = m_Scope->GetGi (idh);
    } catch (CException ) {
    } catch (std::exception ) {
    }
    return gi;
};


/*
Return values are:
 0: no problem - Accession is in proper format
-1: Accession did not start with a letter (or two letters)
-2: Accession did not contain five numbers (or six numbers after 2 letters)
-3: the original Accession number to be validated was NULL
-4: the original Accession number is too long (>16)
*/

static int ValidateAccessionFormat (string accession) 
{
    if (NStr::IsBlank (accession)) {
        return -3;
    }
    if (accession.length() >= 16) {
        return -4;
    }
    if (accession.c_str()[0] < 'A' || accession.c_str()[0] > 'Z') {
        return -1;
    }

    if (NStr::StartsWith(accession, "NZ_")) {
        accession = accession.substr(3);
    }

    const char *cp = accession.c_str();
    int num_alpha = 0;
    int num_underscore = 0;
    int num_digits = 0;

    while (isalpha (*cp)) {
        num_alpha++;
        cp++;
    }
    while (*cp == '_') {
        num_underscore++;
        cp++;
    }
    while (isdigit (*cp)) {
        num_digits ++;
        cp++;
    }

    if (*cp != 0 && *cp != ' ' && *cp != '.') {
        return -2;
    }

    if (num_underscore > 1) {
        return -2;
    }

    if (num_underscore == 0) {
        if (num_alpha == 1 && num_digits == 5) return 0;
        if (num_alpha == 2 && num_digits == 6) return 0;
        if (num_alpha == 3 && num_digits == 5) return 0;
        if (num_alpha == 4 && num_digits == 8) return 0;
        if (num_alpha == 5 && num_digits == 7) return 0;
    } else if (num_underscore == 1) {
        if (num_alpha != 2 || (num_digits != 6 && num_digits != 8 && num_digits != 9)) return -2;
        char first_char = accession.c_str()[0];
        char second_char = accession.c_str()[1];
        if (first_char == 'N' || first_char == 'X' || first_char == 'Z') {
          if (second_char == 'M' ||
              accession [1] == 'C' ||
              accession [1] == 'T' ||
              accession [1] == 'P' ||
              accession [1] == 'G' ||
              accession [1] == 'R' ||
              accession [1] == 'S' ||
              accession [1] == 'W' ||
              accession [1] == 'Z') {
            return 0;
          }
        }
        if (accession [0] == 'A' || accession [0] == 'W' || accession [0] == 'Y') {
          if (accession [1] == 'P') return 0;
        }
      }

  return -2;
}


bool CValidError_feat::GetPrefixAndAccessionFromInferenceAccession (string inf_accession, string &prefix, string &accession)
{
    size_t pos1 = NStr::Find (inf_accession, ":");
    size_t pos2 = NStr::Find (inf_accession, "|");
    size_t pos = string::npos;

    if (pos1 < pos2) {
      pos = pos1;
    } else {
      pos = pos2;
    }
    if (pos == string::npos) {
        return false;
    } else {
        prefix = inf_accession.substr(0, pos);
        NStr::TruncateSpacesInPlace (prefix);
        accession = inf_accession.substr(pos + 1);
        NStr::TruncateSpacesInPlace (accession);
        return true;
    }
}


bool s_IsSraPrefix (string str)

{
    if (str.length() < 3) {
        return false;
    }
    char ch = str.c_str()[0];
    if (ch != 'S' && ch != 'E' && ch != 'D') return false;
    ch = str.c_str()[1];
    if (ch != 'R') return false;
    ch = str.c_str()[2];
    if (ch != 'A' && ch != 'P' && ch != 'X' && ch != 'R' && ch != 'S' && ch != 'Z') return false;
    return true;
}


bool s_IsAllDigitsOrPeriods (string str)

{
    if (NStr::IsBlank(str) || NStr::StartsWith(str, ".") || NStr::EndsWith(str, ".")) {
        return false;
    }
    bool rval = true;
        ITERATE(string, it, str) {
        if (!isdigit(*it) && *it != '.') {
            rval = false;
            break;
        }
    }
    return rval;
}


bool s_IsAllDigits (string str)
{
    if (NStr::IsBlank(str)) {
        return false;
    }
    bool rval = true;
    ITERATE(string, it, str) {
        if (!isdigit(*it)) {
            rval = false;
            break;
        }
    }
    return rval;
}


CValidError_feat::EInferenceValidCode CValidError_feat::ValidateInferenceAccession (string accession, bool fetch_accession, bool is_similar_to)
{
    if (NStr::IsBlank (accession)) {
        return eInferenceValidCode_empty;
    }

    CValidError_feat::EInferenceValidCode rsult = eInferenceValidCode_valid;

    string prefix, remainder;
    if (GetPrefixAndAccessionFromInferenceAccession (accession, prefix, remainder)) {
        bool is_insd = false, is_refseq = false, is_blast = false;

        if (NStr::EqualNocase (prefix, "INSD")) {
            is_insd = true;
        } else if (NStr::EqualNocase (prefix, "RefSeq")) {
            is_refseq = true;
        } else if (NStr::StartsWith (prefix, "BLAST", NStr::eNocase)) {
            is_blast = true;
        }
        if (is_insd || is_refseq) {
            if (remainder.length() > 3) {
                if (remainder.c_str()[2] == '_') {
                    if (is_insd) {
                        rsult = eInferenceValidCode_bad_accession_type;
                    }
                } else {
                    if (is_refseq) {
                        rsult = eInferenceValidCode_bad_accession_type;
                    }
                }
            } 
            if (s_IsSraPrefix (remainder) && s_IsAllDigitsOrPeriods(remainder.substr(3))) {
                // SRA
            } else if (NStr::StartsWith(remainder, "MAP_") && s_IsAllDigits(remainder.substr(4))) {
            } else {
                string ver = "";
                int acc_code = ValidateAccessionFormat (remainder);
                if (acc_code == 0) {
                    //-5: missing version number
                    //-6: bad version number
                    size_t dot_pos = NStr::Find (remainder, ".");
                    if (dot_pos == string::npos || NStr::IsBlank(remainder.substr(dot_pos + 1))) {
                        acc_code = -5;
                    } else {
                        const string& cps = remainder.substr(dot_pos + 1);
                        const char *cp = cps.c_str();
                        while (*cp != 0 && isdigit (*cp)) {
                            ++cp;
                        }
                        if (*cp != 0) {
                            acc_code = -6;
                        }
                    }
                }

                if (acc_code == -5 || acc_code == -6) {
                    rsult = eInferenceValidCode_bad_accession_version;
                } else if (acc_code != 0) {
                    rsult = eInferenceValidCode_bad_accession;
                } else if (fetch_accession) {
                    // Test to see if accession is public
                    const char* database_names[] = { "Nucleotide", "Protein" };
                    const int num_databases = sizeof( database_names ) / sizeof( const char* );
                    TGi gi_number = ZERO_GI;
                    for ( int i=0; (gi_number == ZERO_GI) && (i < num_databases); ++ i ) {
                        gi_number = x_SeqIdToGiNumber( remainder, database_names[ i ] );
                    }
                    if (gi_number == ZERO_GI) {
                        rsult = eInferenceValidCode_accession_version_not_public;
                    }
                }
            }
        } else if (is_blast && is_similar_to) {
            rsult = eInferenceValidCode_bad_accession_type;
        }
        if (NStr::Find (remainder, " ") != string::npos) {
            rsult = eInferenceValidCode_spaces;
        }
    } else {
        rsult = eInferenceValidCode_single_field;
    }

    return rsult;
}


vector<string> CValidError_feat::GetAccessionsFromInferenceString (string inference, string &prefix, string &remainder, bool &same_species)
{
    vector<string> accessions;

    accessions.clear();
    CInferencePrefixList::GetPrefixAndRemainder (inference, prefix, remainder);
    if (NStr::IsBlank (prefix)) {
        return accessions;
    }

    same_species = false;

    if (NStr::StartsWith (remainder, "(same species)", NStr::eNocase)) {
        same_species = true;
        remainder = remainder.substr(14);
        NStr::TruncateSpacesInPlace (remainder);
    }

    if (NStr::StartsWith (remainder, ":")) {
        remainder = remainder.substr (1);
        NStr::TruncateSpacesInPlace (remainder);
    } else if (NStr::IsBlank (remainder)) {
        return accessions;
    } else {
        prefix = "";
    }

    if (NStr::IsBlank (remainder)) {
        return accessions;
    }

    if (NStr::Equal(prefix, "similar to sequence")
        || NStr::Equal(prefix, "similar to AA sequence")
        || NStr::Equal(prefix, "similar to DNA sequence")
        || NStr::Equal(prefix, "similar to RNA sequence")
        || NStr::Equal(prefix, "similar to RNA sequence, mRNA")
        || NStr::Equal(prefix, "similar to RNA sequence, EST")
        || NStr::Equal(prefix, "similar to RNA sequence, other RNA")) {
        NStr::Tokenize(remainder, ",", accessions);
    } else if (NStr::Equal(prefix, "alignment")) {
        NStr::Tokenize(remainder, ",", accessions);
    }
    return accessions;
}


CValidError_feat::EInferenceValidCode CValidError_feat::ValidateInference(string inference, bool fetch_accession)
{
    if (NStr::IsBlank (inference)) {
        return eInferenceValidCode_empty;
    }

    string prefix, remainder;
    bool same_species = false;

    vector<string> accessions = GetAccessionsFromInferenceString (inference, prefix, remainder, same_species);

    if (NStr::IsBlank (prefix)) {
        return eInferenceValidCode_bad_prefix;
    }

    if (NStr::IsBlank (remainder)) {
        return eInferenceValidCode_bad_body;
    }

    CValidError_feat::EInferenceValidCode rsult = eInferenceValidCode_valid;
    bool is_similar_to = NStr::StartsWith (prefix, "similar to");
    if (same_species && !is_similar_to) {
        rsult = eInferenceValidCode_same_species_misused;
    }

    if (rsult == eInferenceValidCode_valid) {
        for (size_t i = 0; i < accessions.size(); i++) {
            NStr::TruncateSpacesInPlace (accessions[i]);
            rsult = ValidateInferenceAccession (accessions[i], fetch_accession, is_similar_to);
            if (rsult != eInferenceValidCode_valid) {
                break;
            }
        }
    }
    if (rsult == eInferenceValidCode_valid) {
        int num_spaces = 0;
        FOR_EACH_CHAR_IN_STRING(str_itr, remainder) {
            const char& ch = *str_itr;
            if (ch == ' ') {
                num_spaces++;
            }
        }
        if (num_spaces > 3) {
            rsult = eInferenceValidCode_comment;
        } else if (num_spaces > 0){
            rsult = eInferenceValidCode_spaces;
        }
    }
    return rsult;
}


void CValidError_feat::ValidateImpGbquals
(const CImp_feat& imp,
 const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype ftype = feat.GetData().GetSubtype();
    const string& key = imp.GetKey();
    if (ftype == CSeqFeatData::eSubtype_imp && NStr::EqualNocase (key, "gene")) {
        ftype = CSeqFeatData::eSubtype_gene;
    } else if (ftype == CSeqFeatData::eSubtype_imp) { 
        ftype = CSeqFeatData::eSubtype_bad;
    } else if (ftype == CSeqFeatData::eSubtype_Imp_CDS 
               || ftype == CSeqFeatData::eSubtype_site_ref
               || ftype == CSeqFeatData::eSubtype_org) {
        ftype = CSeqFeatData::eSubtype_bad;
    }

    FOR_EACH_GBQUAL_ON_FEATURE (qual, feat) {

        const string& qual_str = (*qual)->GetQual();

        if ( NStr::Equal (qual_str, "gsdb_id")) {
            continue;
        }
        CSeqFeatData::EQualifier gbqual = CSeqFeatData::GetQualifierType(qual_str);
        
        if ( gbqual == CSeqFeatData::eQual_bad ) {
            if (!(*qual)->IsSetQual() || NStr::IsBlank((*qual)->GetQual())) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                    "NULL qualifier", feat);
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                    "Unknown qualifier " + qual_str, feat);
            }
        } else {
            if ( ftype != CSeqFeatData::eSubtype_bad && !CSeqFeatData::IsLegalQualifier(ftype, gbqual) ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                    "Wrong qualifier " + qual_str + " for feature " + 
                    key, feat);
            }
            
            string val = (*qual)->GetVal();
            
            bool error = false;
            switch ( gbqual ) {
            case CSeqFeatData::eQual_rpt_type:
                {{
                    error = !s_IsValidRpt_typeQual(val);
                }}
                break;
                
            case CSeqFeatData::eQual_rpt_unit:
                {{
                    ValidateRptUnitVal (val, key, feat);
                }}
                break;

            case CSeqFeatData::eQual_rpt_unit_seq:
                {{
                    ValidateRptUnitSeqVal (val, key, feat);
                }}
                break;

            case CSeqFeatData::eQual_rpt_unit_range:
                {{
                    ValidateRptUnitRangeVal (val, feat);
                }}
                break;
            case CSeqFeatData::eQual_label:
                {{
                    ValidateLabelVal (val, feat);
                }}
                break;
            case CSeqFeatData::eQual_replace:
                {{
                    CBioseq_Handle bh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
                    if (bh) {
                        if (bh.IsNa()) {
                            if (NStr::Equal(key, "variation")) {
                                if (!s_StringConsistsOf (val, "acgt")) {
                                    PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                                             val + " is not a legal value for qualifier " + qual_str
                                             + " - should only be composed of acgt unambiguous nucleotide bases",
                                             feat);
                                }
                            } else if (!s_StringConsistsOf (val, "acgtmrwsykvhdbn")) {
                                  PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                                           val + " is not a legal value for qualifier " + qual_str
                                           + " - should only be composed of acgtmrwsykvhdbn nucleotide bases",
                                           feat);
                            }
                        } else if (bh.IsAa()) {
                            if (!s_StringConsistsOf (val, "acdefghiklmnpqrstuvwy*")) {
                                PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                                         val + " is not a legal value for qualifier " + qual_str
                                         + " - should only be composed of acdefghiklmnpqrstuvwy* amino acids",
                                         feat);
                            }
                        }

                        // if no point in location with fuzz, info if text matches sequence
                        bool has_fuzz = false;
                        for( objects::CSeq_loc_CI it(feat.GetLocation()); it && !has_fuzz; ++it) {
                            if (it.IsPoint() && (it.GetFuzzFrom() != NULL || it.GetFuzzTo() != NULL)) {
                                has_fuzz = true;
                            }
                        }
                        if (!has_fuzz && val.length() == GetLength (feat.GetLocation(), m_Scope)) {
                            try {
                                CSeqVector nuc_vec(feat.GetLocation(), *m_Scope, CBioseq_Handle::eCoding_Iupac);
                                string bases = "";
                                nuc_vec.GetSeqData(0, nuc_vec.size(), bases);
                                if (NStr::EqualNocase(val, bases)) {
                                    PostErr (eDiag_Info, eErr_SEQ_FEAT_SuspiciousQualifierValue,
                                             "/replace already matches underlying sequence (" + val + ")",
                                             feat);
                                }
                            } catch (CException ) {
                            } catch (std::exception ) {
                            }
                        }
                    }
                }}
                break;

            case CSeqFeatData::eQual_mobile_element_type:
                {{
                    bool found = false;
                    for (size_t i = 0; 
                         i < sizeof(s_LegalMobileElementStrings) / sizeof(string);
                         ++i) {
                         if ( NStr::StartsWith (val, s_LegalMobileElementStrings[i], NStr::eNocase)) {
                             string rest = val.substr (s_LegalMobileElementStrings[i].length());
                             if (!NStr::IsBlank (rest) 
                                 && (!NStr::StartsWith (rest, ":") || NStr::Equal(rest, ":"))) {
                                 PostErr (eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                                          val + " is not a legal value for qualifier " + qual_str,
                                          feat);
                             } else if (NStr::StartsWith(val, "other") 
                                        && (!NStr::StartsWith(rest, ":") || NStr::Equal(rest, ":"))) {
                                 // if other, must have additional text after colon
                                 PostErr (eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                                          val + " is not a legal value for qualifier " + qual_str,
                                          feat);
                             }                                                                  
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                         PostErr (eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                                  val + " is not a legal value for qualifier " + qual_str,
                                  feat);
                    }
                }}
                break;

            case CSeqFeatData::eQual_compare:
                {{
                    ValidateCompareVal (val, feat);
                }}
                break;

            case CSeqFeatData::eQual_standard_name:
                {{
                    if (ftype == CSeqFeatData::eSubtype_misc_feature
                        && NStr::EqualCase (val, "Vector Contamination")) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_VectorContamination, 
                                 "Vector Contamination region should be trimmed from sequence",
                                 feat);
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


void CValidError_feat::ValidateNonImpFeatGbquals (const CSeq_feat& feat)
{
    string key = feat.GetData().GetKey();

    if (NStr::Equal (key, "Gene")) {
        key = "gene";
    }

    CSeqFeatData::ESubtype ftype = feat.GetData().GetSubtype();

    FOR_EACH_GBQUAL_ON_FEATURE (qual, feat) {
        if (!(*qual)->IsSetQual() || NStr::IsBlank ((*qual)->GetQual())) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownFeatureQual, "NULL qualifier", feat);
            continue;
        }
            
        const string& qual_str = (*qual)->GetQual();

        if (NStr::Equal (qual_str, "gsdb_id")) {
            continue;
        }

        CSeqFeatData::EQualifier gbqual = CSeqFeatData::GetQualifierType(qual_str);

        if ( gbqual == CSeqFeatData::eQual_bad ) {
            CSeqFeatData::E_Choice chs = feat.GetData().Which();
            if (chs == CSeqFeatData::e_Gene) {
                if (NStr::Equal (qual_str, "gen_map")
                    || NStr::Equal (qual_str, "cyt_map")
                    || NStr::Equal (qual_str, "rad_map")) {
                    continue;
                }
            } else if (chs == CSeqFeatData::e_Cdregion) {
                if (NStr::Equal (qual_str, "orig_transcript_id")) {
                    continue;
                }
            } else if (chs == CSeqFeatData::e_Rna) {
                if (NStr::Equal (qual_str, "orig_protein_id")
                    || NStr::Equal (qual_str, "orig_transcript_id")) {
                    continue;
                }
            }
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_UnknownFeatureQual, "Unknown qualifier " + qual_str, feat);
        } else if (ftype != CSeqFeatData::eSubtype_bad) { 
            if (!feat.GetData().IsLegalQualifier(gbqual)) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                    "Wrong qualifier " + qual_str + " for feature " + 
                    key, feat);
            }

            if ((*qual)->IsSetVal() && !NStr::IsBlank ((*qual)->GetVal())) {
                const string& val = (*qual)->GetVal();
                switch ( gbqual ) {
                case CSeqFeatData::eQual_rpt_type:
                    {{
                        if (!s_IsValidRpt_typeQual(val)) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue, 
                                    val + " is not a legal value for qualifier " + qual_str, feat);
                        }
                    }}
                    break;
                case CSeqFeatData::eQual_rpt_unit:
                    {{
                        ValidateRptUnitVal (val, key, feat);
                    }}
                    break;

                case CSeqFeatData::eQual_rpt_unit_seq:
                    {{
                        ValidateRptUnitSeqVal (val, key, feat);
                    }}
                    break;

                case CSeqFeatData::eQual_rpt_unit_range:
                    {{
                        ValidateRptUnitRangeVal (val, feat);
                    }}
                    break;

                case CSeqFeatData::eQual_label:
                    {{
                        ValidateLabelVal (val, feat);
                    }}
                    break;
                case CSeqFeatData::eQual_compare:
                    {{
                        ValidateCompareVal (val, feat);
                    }}
                    break;

                  default:
                      break;
                }
            }
        }
    }
}


static CRef<CSeq_loc> s_MakePointForLocationStop (const CSeq_loc& loc)
{
    CRef<CSeq_loc> stop(new CSeq_loc());

    for ( CSeq_loc_CI citer (loc); citer; ++citer ) {
        stop->SetPnt().SetId().Assign(citer.GetSeq_id());
    }
    stop->SetPnt().SetPoint(loc.GetStop(eExtreme_Biological));
    return stop;
}


void CValidError_feat::ValidatePeptideOnCodonBoundry
(const CSeq_feat& feat, 
 const string& key)
{
    const CSeq_loc& loc = feat.GetLocation();

    CConstRef<CSeq_feat> cds = GetOverlappingCDS(loc, *m_Scope);
    if ( !cds ) {
        return;
    }
    const CCdregion& cdr = cds->GetData().GetCdregion();

    TSeqPos pos1 = LocationOffset(cds->GetLocation(), loc, eOffset_FromStart);
    bool pos1_not_in = false;
    if (pos1 == ((TSeqPos)-1)) {
        pos1_not_in = true;
    }
    CRef<CSeq_loc> tmp = s_MakePointForLocationStop(loc);
    TSeqPos pos2 = LocationOffset(cds->GetLocation(), *tmp, eOffset_FromStart);
    bool pos2_not_in = false;
    if (pos2 == ((TSeqPos)-1)) {
        pos2_not_in = true;
    }
    if (pos1_not_in && pos2_not_in && NStr::Equal(key, "sig_peptide")) {
    // ignore sig_peptide that is completely before coding region
        return;
    }

    unsigned int frame = 0;
    switch (cdr.GetFrame()) {
        case CCdregion::eFrame_not_set:
        case CCdregion::eFrame_one:
            frame = 0;
            break;
        case CCdregion::eFrame_two:
            frame = 1;
            break;
        case CCdregion::eFrame_three:
            frame = 2;
            break;
    }
    // note - have to add 3 to prevent negative result from subtraction
    TSeqPos mod1 = (pos1 + 3 - frame) %3;
    TSeqPos mod2 = (pos2 + 3 - frame) %3;

    if ( mod1 != 0 && loc.IsPartialStart(eExtreme_Biological) 
         && cds->GetLocation().IsPartialStart(eExtreme_Biological) 
         && pos1 == 0) {
        mod1 = 0;
    } else if (pos1 < frame) {
        // start is out of frame - it's before the coding region begins
        mod1 = 1;
    }

    if ( mod2 != 0 && loc.IsPartialStop(eExtreme_Biological) 
         && cds->GetLocation().IsPartialStop(eExtreme_Biological) 
         && pos2 == GetLength (cds->GetLocation(), m_Scope)) {
        mod2 = 0;
    } else if (pos2 <= frame) {
        // stop is out of frame - it's before the coding region begins
        mod2 = 1;
    }
    if (pos2 > GetLength (cds->GetLocation(), m_Scope)) {
        // stop is out of frame - it's after the coding region ends
        mod2 = 1;
    }

    if (loc.IsPartialStart(eExtreme_Biological)) {
        mod1 = 0;
    }
    if (loc.IsPartialStop(eExtreme_Biological)) {
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


static const char* const sc_BypassMrnaTransCheckText[] = {
    "RNA editing",
    "adjusted for low-quality genome",
    "annotated by transcript or proteomic data",
    "artificial frameshift",
    "mismatches in transcription"
    "reasons given in citation",
    "transcribed product replaced",
    "unclassified transcription discrepancy",    
};
typedef CStaticArraySet<const char*, PCase_CStr> TBypassMrnaTransCheckSet;
DEFINE_STATIC_ARRAY_MAP(TBypassMrnaTransCheckSet, sc_BypassMrnaTransCheck, sc_BypassMrnaTransCheckText);


static bool s_IsBioseqPartial (CBioseq_Handle bsh)
{
    CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);
    if ( !sd ) {
        return false;
    }
    const CMolInfo& molinfo = sd->GetMolinfo();
    if (!molinfo.IsSetCompleteness ()) {
        return false;
    } 
    CMolInfo::TCompleteness completeness = molinfo.GetCompleteness();
    if (completeness == CMolInfo::eCompleteness_partial
        || completeness == CMolInfo::eCompleteness_no_ends 
        || completeness == CMolInfo::eCompleteness_no_left
        || completeness == CMolInfo::eCompleteness_no_right) {
        return true;
    } else {
        return false;
    }
}


static bool s_BioseqHasRefSeqThatStartsWithPrefix (CBioseq_Handle bsh, string prefix)
{
    bool rval = false;
    FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetBioseqCore())) {
        if ((*it)->IsOther() && (*it)->GetTextseq_Id()->IsSetAccession()
            && NStr::StartsWith ((*it)->GetTextseq_Id()->GetAccession(), prefix)) {
            rval = true;
            break;
        }
    }
    return rval;
}


void CValidError_feat::ValidateMrnaTrans(const CSeq_feat& feat)
{
    bool has_errors = false, unclassified_except = false, other_than_mismatch = false,
        mismatch_except = false, report_errors = true, product_replaced = false;

    EDiagSev sev = eDiag_Error;

    string farstr;

    if (feat.CanGetPseudo()  &&  feat.GetPseudo()) {
        return;
    }
    if (!feat.CanGetProduct()) {
        return;
    }

    bool rna_editing = false;

    if (!m_Imp.IgnoreExceptions() &&
        feat.CanGetExcept()  &&  feat.GetExcept()  &&
        feat.CanGetExcept_text()) {
        const string& except_text = feat.GetExcept_text();
        ITERATE (TBypassMrnaTransCheckSet, it, sc_BypassMrnaTransCheck) {
            if (NStr::FindNoCase(except_text, *it) != NPOS) {
                report_errors = false;  // biological exception
            }
        }
        if (NStr::FindNoCase(except_text, "RNA editing") != NPOS) {
            rna_editing = true;
        }
        if (NStr::FindNoCase(except_text, "unclassified transcription discrepancy") != NPOS) {
            unclassified_except = true;
        }
        if (NStr::FindNoCase(except_text, "mismatches in transcription") != NPOS) {
            mismatch_except = true;
            report_errors = true;
        }
        if (NStr::FindNoCase(except_text, "transcribed product replaced") != NPOS) {
            product_replaced = true;
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
    
    CBioseq_Handle nuc = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
    if (!nuc) {
        has_errors = true;
        if (report_errors  ||  unclassified_except) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_MrnaTransFail,
                "Unable to transcribe mRNA", feat);
        }
        return;
    }

    bool is_refseq = false;
    size_t mismatches = 0, total = 0;

    FOR_EACH_SEQID_ON_BIOSEQ (it, *(nuc.GetCompleteBioseq())) {
        if ((*it)->IsOther()) {
            is_refseq = true;
            break;
        }
    }

    // note - exception will be thrown when creating CSeqVector if wrong type set for bioseq seq-data
    try {
        if (nuc) {
            
            CBioseq_Handle rna = m_Scope->GetBioseqHandleFromTSE(*product_id, nuc);
            if (!rna) {
                // if not local bioseq product, lower severity (with the exception of Refseq)
                if (!is_refseq) {
                    sev = eDiag_Warning;
                }

                if (m_Imp.IsFarFetchMRNAproducts()) {
                    rna = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
                    if (!rna) {
                        string label;
                        product_id->GetLabel(&label);
                        PostErr (eDiag_Error, eErr_SEQ_FEAT_ProductFetchFailure, 
                                 "Unable to fetch mRNA transcript '" + label + "'", feat);
                    }
                }
                if (!rna) {
                    return;
                }
                farstr = "(far) ";
                if (feat.IsSetPartial() 
                    && !s_IsBioseqPartial(rna) 
                    && s_BioseqHasRefSeqThatStartsWithPrefix(rna, "NM_")) {
                    sev = eDiag_Warning;
                }
            }
            _ASSERT(nuc  &&  rna);

            // if unclassified exception, drop back down to warning
            if (is_refseq && unclassified_except) {
                sev = eDiag_Warning;
            }
        
            CSeqVector nuc_vec(feat.GetLocation(), *m_Scope,
                               CBioseq_Handle::eCoding_Iupac);
            CSeqVector rna_vec(feat.GetProduct(), *m_Scope,
                               CBioseq_Handle::eCoding_Iupac);

            size_t nuc_len = nuc_vec.size();
            size_t rna_len = rna_vec.size();

            if (nuc_len != rna_len) {
                has_errors = true;
                other_than_mismatch = true;
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
                        if (report_errors || rna_editing) {
                            PostErr(sev, eErr_SEQ_FEAT_TranscriptLen,
                                "Transcript length [" + NStr::SizetToString(nuc_len) + 
                                "] less than " + farstr + "product length [" +
                                NStr::SizetToString(rna_len) + "], and tail < 95% polyA",
                                feat);
                        }
                    } else if (count_a > 0 && count_no_a == 0) {
                        has_errors = true;
                        other_than_mismatch = true;
                        if (report_errors || rna_editing) {
                            if (m_Imp.IsGpipe() && m_Imp.IsGenomic()) {
                                // suppress
                            } else {
                                PostErr (eDiag_Info, eErr_SEQ_FEAT_PolyATail, 
                                         "Transcript length [" + NStr::SizetToString(nuc_len)
                                         + "] less than " + farstr + "product length ["
                                         + NStr::SizetToString (rna_len) + "], but tail is 100% polyA",
                                         feat);
                            }
                        }
                    } else {
                        if (report_errors) {
                            PostErr(eDiag_Info, eErr_SEQ_FEAT_PolyATail,
                                "Transcript length [" + NStr::SizetToString(nuc_len) + 
                                "] less than " + farstr + "product length [" +
                                NStr::SizetToString(rna_len) + "], but tail >= 95% polyA",
                                feat);
                        }
                    }  
                    // allow base-by-base comparison on common length
                    rna_len = nuc_len = min(nuc_len, rna_len);

                } else {
                    if (report_errors) {
                        PostErr(sev, eErr_SEQ_FEAT_TranscriptLen,
                            "Transcript length [" + NStr::IntToString(nuc_vec.size()) + "] " +
                            "greater than " + farstr +"product length [" + 
                            NStr::IntToString(rna_vec.size()) + "]", feat);
                    }
                }
            }
            
            if (rna_len == nuc_len && nuc_len > 0) {
                CSeqVector_CI nuc_ci(nuc_vec);
                CSeqVector_CI rna_ci(rna_vec);

                // compare content of common length
                while ((nuc_ci  &&  rna_ci)  &&  (nuc_ci.GetPos() < nuc_len)) {
                    if (*nuc_ci != *rna_ci) {
                        ++mismatches;
                    }
                    ++nuc_ci;
                    ++rna_ci;
                    ++total;
                }
                if (mismatches > 0) {
                    has_errors = true;
                    if (report_errors  &&  !mismatch_except) {
                        PostErr(sev, eErr_SEQ_FEAT_TranscriptMismatches,
                            "There are " + NStr::SizetToString(mismatches) + 
                            " mismatches out of " + NStr::SizetToString(nuc_len) +
                            " bases between the transcript and " + farstr + "product sequence",
                            feat);
                    }
                }
            }
        }
    } catch (CException ) {
    } catch (std::exception ) {
    }

    if (!report_errors) {
        if (!has_errors) {            
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
                "mRNA has exception but passes transcription test", feat);
        } else if (unclassified_except && ! other_than_mismatch) {
            if (mismatches * 50 <= total) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_ErroneousException,
                  "mRNA has unclassified exception but only difference is " + NStr::SizetToString (mismatches) 
                  + " mismatches out of " + NStr::SizetToString (total) + " bases", feat);
            }
        } else if (product_replaced) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnqualifiedException,
                    "mRNA has unqualified transcribed product replaced exception", feat);
        }
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

    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
    if ( !bsh ) {
        bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
        if (bsh) {
            CSeq_entry_Handle seh = bsh.GetTopLevelEntry();
            if (seh.IsSet() && seh.GetSet().IsSetClass()
                && (seh.GetSet().GetClass() == CBioseq_set::eClass_gen_prod_set
                    || seh.GetSet().GetClass() == CBioseq_set::eClass_other)) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_MissingMRNAproduct,
                    "Product Bioseq of mRNA feature is not "
                    "packaged in the record", feat);
            }
        }
    } else {

        CConstRef<CSeq_feat> mrna = m_Imp.GetmRNAGivenProduct (*(bsh.GetCompleteBioseq()));
        if (mrna.GetPointer() != &feat) {
            PostErr(eDiag_Critical, eErr_SEQ_FEAT_MultipleMRNAproducts,
                    "Same product Bioseq from multiple mRNA features", feat);
        }
    }
}


// check that there is no conflict between the gene on the genomic 
// and the gene on the mrna.
void CValidError_feat::ValidatemRNAGene (const CSeq_feat &feat)
{
    if (feat.IsSetProduct()) {
        // get gene ref for mRNA feature
        const CGene_ref* genomicgrp = feat.GetGeneXref();
        if ( genomicgrp == 0 ) {
            const CSeq_feat* gene = 
                GetOverlappingGene(feat.GetLocation(), *m_Scope);
            if ( gene != 0 ) {
                genomicgrp = &gene->GetData().GetGene();
            }
        }
        if ( genomicgrp != 0 ) {
            // get gene for mRNA product sequence
            CBioseq_Handle seq = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
            if (seq) {
                CFeat_CI mrna_gene(seq, CSeqFeatData::e_Gene);
                if ( mrna_gene ) {
                    const CGene_ref& mrnagrp = mrna_gene->GetData().GetGene();
                    if ( !s_EqualGene_ref(*genomicgrp, mrnagrp) ) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_GenesInconsistent,
                            "Gene on mRNA bioseq does not match gene on genomic bioseq",
                            mrna_gene->GetOriginalFeature());
                    }
                }
            }
        }
    }
}


void CValidError_feat::ValidateBothStrands(const CSeq_feat& feat)
{
    const CSeq_loc& location = feat.GetLocation ();
    bool bothstrands = false, bothreverse = false;

    for ( CSeq_loc_CI citer (location); citer; ++citer ) {
        ENa_strand strand = citer.GetStrand();
        if ( strand == eNa_strand_both ) {
            bothstrands = true;
        } else if (strand == eNa_strand_both_rev) {
            bothreverse = true;
        }
    }
    if (bothstrands || bothreverse) {
        EDiagSev sev = eDiag_Warning;
        string prefix = "Feature";
        if (feat.IsSetData()) {
            if (feat.GetData().IsCdregion()) {
                prefix = "CDS";
                sev = eDiag_Error;
            } else if (feat.GetData().IsRna() && feat.GetData().GetRna().IsSetType()
                       && feat.GetData().GetRna().GetType() == CRNA_ref::eType_mRNA) {
                prefix = "mRNA";
                sev = eDiag_Error;
            }
        }
        string suffix = "";
        if (bothstrands && bothreverse) {
            suffix = "(forward and reverse)";
        } else if (bothstrands) {
            suffix = "(forward)";
        } else if (bothreverse) {
            suffix = "(reverse)";
        }

        PostErr (sev, eErr_SEQ_FEAT_BothStrands, 
                prefix + " may not be on both " + suffix + " strands", feat);  
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

    CBioseq_Handle prod;
    const CSeq_id * sid = feat.GetProduct().GetId();
    if (sid) {
        prod = m_Scope->GetBioseqHandleFromTSE(*sid, BioseqHandleFromLocation(m_Scope, feat.GetLocation()));
    }
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

        PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingCDSproduct,
            "Unable to find product Bioseq from CDS feature", feat);
        return;
    }
    CBioseq_Handle nuc  = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
    if ( nuc ) {
        bool is_nt = s_BioseqHasRefSeqThatStartsWithPrefix (nuc, "NT_");
        if (!is_nt) {
            CSeq_entry_Handle wgs = nuc.GetExactComplexityLevel (CBioseq_set::eClass_gen_prod_set);
            if (!wgs) {
                CSeq_entry_Handle prod_nps = 
                    prod.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
                CSeq_entry_Handle nuc_nps = 
                    nuc.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);

                if ( ! prod_nps || ! nuc_nps || prod_nps != nuc_nps ) {
                    if (m_Imp.IsSmallGenomeSet()) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSproductPackagingProblem,
                            "Protein product not packaged in nuc-prot set with nucleotide in small genome set", 
                            feat);
                    } else {
                        PostErr(eDiag_Error, eErr_SEQ_FEAT_CDSproductPackagingProblem,
                            "Protein product not packaged in nuc-prot set with nucleotide", 
                            feat);
                    }
                }
            }
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
            CBioseq_Handle sfh = BioseqHandleFromLocation(m_Scope, sfp->GetLocation());
            if ( nuc != sfh ) {
                return;
            }
        }
        PostErr(eDiag_Critical, eErr_SEQ_FEAT_MultipleCDSproducts, 
            "Same product Bioseq from multiple CDS features", feat);
    }
}


bool CValidError_feat::DoesCDSHaveShortIntrons(const CSeq_feat& feat)
{
    if (!feat.IsSetData() || !feat.GetData().IsCdregion() 
        || !feat.IsSetLocation() 
        || feat.IsSetPseudo() || IsOverlappingGenePseudo(feat)) {
        return false;
    }

    CSeq_loc_CI li(feat.GetLocation());
    const CSeq_loc& start_loc = li.GetEmbeddingSeq_loc();
    TSeqPos last_start = start_loc.GetStart(eExtreme_Positional);
    TSeqPos last_stop = start_loc.GetStop(eExtreme_Positional);

    bool found_short = false;
    ++li;
    while (li && !found_short) {
        const CSeq_loc& this_loc = li.GetEmbeddingSeq_loc();
        TSeqPos this_start = this_loc.GetStart(eExtreme_Positional);
        TSeqPos this_stop = this_loc.GetStop(eExtreme_Positional);
        if (abs ((int)this_start - (int)last_stop) < 11 || abs ((int)this_stop - (int)last_start) < 11) {
            found_short = true;
        }
        last_start = this_start;
        last_stop = this_stop;
        ++li;
    }

    return found_short;        
}


bool CValidError_feat::IsIntronShort(const CSeq_feat& feat)
{
    if (!feat.IsSetData() 
        || feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_intron 
        || !feat.IsSetLocation()
        || feat.IsSetPseudo()
        || IsOverlappingGenePseudo(feat)) {
        return false;
    }

    const CSeq_loc& loc = feat.GetLocation();
    bool is_short = false;

    if (GetLength(loc, m_Scope) < 11) {
        bool partial_left = loc.IsPartialStart(eExtreme_Positional);
        bool partial_right = loc.IsPartialStop(eExtreme_Positional);
        
        CBioseq_Handle bsh;
        if (partial_left && loc.GetStart(eExtreme_Positional) == 0) {
            // partial at beginning of sequence, ok
        } else if (partial_right && (bsh = BioseqHandleFromLocation(m_Scope, loc))
                   && loc.GetStop(eExtreme_Positional) == bsh.GetBioseqLength() - 1) {
            // partial at end of sequence
        } else {
            is_short = true;
        }
    }
    return is_short;
}


static bool s_CDS3primePartialTest (const CSeq_feat& feat, CScope *scope)
{
    if (!scope) {
        return false;
    }

    CBioseq_Handle bsh = BioseqHandleFromLocation(scope, feat.GetLocation());
    if (!bsh) {
        return false;
    }
    CSeq_loc_CI last;
    for ( CSeq_loc_CI sl_iter(feat.GetLocation()); sl_iter; ++sl_iter ) { 
        last = sl_iter;
    }

    if (last) {
        if (last.GetStrand() == eNa_strand_minus) {
            if (last.GetRange().GetFrom() == 0) {
                return true;
            }
        } else {
            if (last.GetRange().GetTo() == bsh.GetInst_Length() - 1) {
                return true;
            }
        }
    }
    return false;    
}


static bool s_CDS5primePartialTest (const CSeq_feat& feat, CScope *scope)
{
    if (!scope) {
        return false;
    }

    CBioseq_Handle bsh = BioseqHandleFromLocation (scope, feat.GetLocation());
    if (!bsh) {
        return false;
    }
    CSeq_loc_CI first(feat.GetLocation());

    if (first) {
        if (first.GetStrand() == eNa_strand_minus) {
            if (first.GetRange().GetTo() == bsh.GetInst_Length() - 1) {
                return true;
            }
        } else {
            if (first.GetRange().GetFrom() == 0) {
                return true;
            }
        }
    }
    return false;    
}


static const char* const sc_BypassCdsPartialCheckText[] = {
  "RNA editing",
  "annotated by transcript or proteomic data",
  "artificial frameshift",
  "mismatches in translation",
  "rearrangement required for product",
  "reasons given in citation",
  "translated product replaced",
  "unclassified translation discrepancy"
};
typedef CStaticArraySet<const char*, PCase_CStr> TBypassCdsPartialCheckSet;
DEFINE_STATIC_ARRAY_MAP(TBypassCdsPartialCheckSet, sc_BypassCdsPartialCheck, sc_BypassCdsPartialCheckText);


void CValidError_feat::ValidateCDSPartial(const CSeq_feat& feat)
{
    if ( !feat.CanGetProduct()  ||  !feat.CanGetLocation() ) {
        return;
    }

    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetProduct());
    if ( !bsh ) {
        return;
    }

    if (feat.CanGetExcept()  &&  feat.GetExcept()  &&  feat.CanGetExcept_text()) {
        const string& except_text = feat.GetExcept_text();
        ITERATE (TBypassCdsPartialCheckSet, it, sc_BypassCdsPartialCheck) {
            if (NStr::FindNoCase(except_text, *it) != NPOS) {
                return;  // biological exception
            }
        }
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
                EDiagSev sev = eDiag_Error;
                if (s_CDS3primePartialTest(feat, m_Scope)) {
                    sev = eDiag_Warning;
                }
                PostErr(sev, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 3' partial but protein is NH2 partial", feat);
            }
            break;

        case CMolInfo::eCompleteness_no_right:
            if (! partial3) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 3' complete but protein is CO2 partial", feat);
            }
            if (partial5) {
                EDiagSev sev = eDiag_Error;
                if (s_CDS5primePartialTest(feat, m_Scope)) {
                    sev = eDiag_Warning;
                }
                PostErr(sev, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 5' partial but protein is CO2 partial", feat);
            }
            break;

        case CMolInfo::eCompleteness_no_ends:
            if ( partial5 && partial3 ) {
            } else if ( partial5 ) {
                EDiagSev sev = eDiag_Error;
                if (s_CDS5primePartialTest(feat, m_Scope)) {
                    sev = eDiag_Warning;
                }
                PostErr(sev, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 5' partial but protein has neither end", feat);
            } else if ( partial3 ) {
                EDiagSev sev = eDiag_Error;
                if (s_CDS3primePartialTest(feat, m_Scope)) {
                    sev = eDiag_Warning;
                }

                PostErr(sev, eErr_SEQ_FEAT_PartialProblem,
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

    bool pseudo = false;
    if (feat.IsSetPseudo()) {
        pseudo = true;
    } else if (IsOverlappingGenePseudo (feat)) {
        pseudo = true;
    }

    EErrType err_type = eErr_SEQ_FEAT_CDSmRNArange;
    if (pseudo) {
        err_type = eErr_SEQ_FEAT_PseudoCDSmRNArange;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_SubsetRev,
        *m_Scope);

    EDiagSev sev = eDiag_Warning;
    if (pseudo) {
        sev = eDiag_Info;
    }
    if ( mrna ) {
        // ribosomal slippage exception suppresses CDSmRNArange warning
        bool supress = false;

        if ( feat.CanGetExcept_text() ) {
            const CSeq_feat::TExcept_text& text = feat.GetExcept_text();
            if ( NStr::FindNoCase(text, "ribosomal slippage") != NPOS 
                || NStr::FindNoCase(text, "trans-splicing") != NPOS) {
                supress = true;
            }
        }
        if ( !supress ) {
            PostErr(sev, err_type,
                "mRNA contains CDS but internal intron-exon boundaries "
                "do not match", feat);
        }
    } else {
        PostErr(sev, err_type,
            "mRNA overlaps or contains CDS but does not completely "
            "contain intervals", feat);
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
    "annotated by transcript or proteomic data",
    "heterogeneous population sequenced",
    "low-quality sequence region",
    "unextendable partial coding region",
    "artificial location",
    "gene split at contig boundary",
    "gene split at sequence boundary",
    "genetic code exception",
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
    if (feat.IsSetExcept_text() && !NStr::IsBlank (feat.GetExcept_text()) && (!feat.IsSetExcept() || !feat.GetExcept())) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptInconsistent,
            "Exception text is present, but exception flag is not set", feat);
    } else if (feat.IsSetExcept() &&  feat.GetExcept() && (!feat.IsSetExcept_text() || NStr::IsBlank (feat.GetExcept_text()))) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptInconsistent,
            "Exception flag is set, but exception text is empty", feat);
    }
    if ( feat.CanGetExcept_text()  &&  !feat.GetExcept_text ().empty() ) {
        ValidateExceptText(feat.GetExcept_text(), feat);
    }
}


static bool s_IsNcOrNt(const string& accession)
{
    if (NStr::StartsWith(accession, "NC_", NStr::eNocase)) {
        return true;
    } else if (NStr::StartsWith(accession, "NT_", NStr::eNocase)) {
        return true;
    } else {
        return false;
    }
}


static bool s_IsNcOrNt (const CSeq_loc& loc, CScope *scope)
{
    const CSeq_id *id = loc.GetId();
    if (id != NULL && id->IsOther() 
        && id->GetOther().IsSetAccession()
        && s_IsNcOrNt(id->GetOther().GetAccession())) {
        return true;
    }        

    if (!scope) {
        return false;
    }
    CBioseq_Handle bsh = BioseqHandleFromLocation(scope, loc);
    if (!bsh) {
        return false;
    }
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(bsh.GetCompleteBioseq())) {
        if ((*id_it)->IsOther() 
            && (*id_it)->GetOther().IsSetAccession()
            && s_IsNcOrNt((*id_it)->GetOther().GetAccession())) {
            return true;
        }
    }
    return false;
}


void CValidError_feat::ValidateExceptText(const string& text, const CSeq_feat& feat)
{
    if ( text.empty() ) return;

    EDiagSev sev = eDiag_Error;
    bool found = false;

    string str;
    
    bool reasons_in_cit = false;
    bool annotated_by_transcript_or_proteomic = false;
    bool redundant_with_comment = false;
    bool refseq_except = false;
    vector<string> exceptions;
    NStr::Tokenize(text, ",", exceptions);
    ITERATE(vector<string>, it, exceptions) {
        found = false;
        str = NStr::TruncateSpaces( *it );
        if (NStr::IsBlank(*it)) {
            continue;
        }
        for (size_t i = 0; !found && i < sizeof(s_LegalExceptionStrings) / sizeof(string); i++) {
            if (NStr::EqualNocase (s_LegalExceptionStrings[i], str)) {
                found = true;
            }
        }
        if ( found ) {
            if (NStr::EqualNocase(str, "reasons given in citation")) {
                reasons_in_cit = true;
            } else if (NStr::EqualNocase(str, "annotated by transcript or proteomic data")) {
                annotated_by_transcript_or_proteomic = true;
            }
        }
        if ( !found) {
            if (m_Scope) {
                CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
                bool check_refseq = false;
                if (bsh) {
                    if (GetGenProdSetParent (bsh)) {
                        check_refseq = true;
                    } else {
                        FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(bsh.GetCompleteBioseq())) {
                            if ((*id_it)->IsOther()) {
                                check_refseq = true;
                                break;
                            }
                        }
                    }
                }
                if (check_refseq) {
                    for (size_t i = 0; !found && i < sizeof(s_RefseqExceptionStrings) / sizeof(string); i++) {
                        if (NStr::EqualNocase (s_RefseqExceptionStrings[i], str)) {
                            found = true;
                            refseq_except = true;
                        }
                    }
                }
            }
        }
        if ( !found ) {
            if ( s_IsNcOrNt (feat.GetLocation(), m_Scope) ) {
                sev = eDiag_Warning;
            }
            PostErr(sev, eErr_SEQ_FEAT_ExceptionProblem,
                str + " is not a legal exception explanation", feat);
        }
        if (feat.IsSetComment() && NStr::Find(feat.GetComment(), str) != string::npos) {
            if (!NStr::EqualNocase(str, "ribosomal slippage") &&
                !NStr::EqualNocase(str, "trans-splicing") &&
                !NStr::EqualNocase(str, "RNA editing") &&
                !NStr::EqualNocase(str, "artificial location")) {
                redundant_with_comment = true;
            } else if (NStr::EqualNocase(feat.GetComment(), str)) {
                redundant_with_comment = true;
            }                
        }
    }
    if (redundant_with_comment) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_ExceptionProblem, 
            "Exception explanation text is also found in feature comment", feat);
    }
    if (refseq_except) {
        bool other_exp = false;
        for (size_t i = 0; !other_exp && i < sizeof(s_RefseqExceptionStrings) / sizeof(string); i++) {
            if (NStr::EqualNocase (s_RefseqExceptionStrings[i], str)) {
                other_exp = true;
            }
        }

        if ( other_exp ) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_ExceptionProblem, 
                     "Genome processing exception should not be combined with other explanations", feat);
        }
    }

    if (reasons_in_cit && !feat.IsSetCit()) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptionProblem,
            "Reasons given in citation exception does not have the required citation", feat);
    }
    if (annotated_by_transcript_or_proteomic) {
        bool has_inference = false;
        FOR_EACH_GBQUAL_ON_SEQFEAT (it, feat) {
            if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "inference")) {
                has_inference = true;
                break;
            }
        }
        if (!has_inference) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptionProblem,
                "Annotated by transcript or proteomic data exception does not have the required inference qualifier", feat);
        }
    }
}


void CValidError_feat::ValidateSeqFeatXref (const CSeqFeatXref& xref, const CSeq_feat& feat, CTSE_Handle tse)
{
    if (!xref.IsSetId() && !xref.IsSetData()) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem, 
                 "SeqFeatXref with no id or data field", feat);
    } else if (xref.IsSetId()) {
        if (xref.GetId().IsLocal() && xref.GetId().GetLocal().IsId()) {
            CSeq_feat_Handle far_feat = tse.GetFeatureWithId(CSeqFeatData::e_not_set,
                                                   xref.GetId().GetLocal().GetId());
            if (far_feat) {
                bool has_xref = false;
                FOR_EACH_SEQFEATXREF_ON_SEQFEAT (it, *(far_feat.GetSeq_feat())) {
                    if ((*it)->IsSetId()) {
                        has_xref = true;
                        if (!feat.IsSetId() || !s_FeatureIdsMatch(feat.GetId(), (*it)->GetId())) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefNotReciprocal, 
                                     "Cross-referenced feature does not link reciprocally",
                                     feat);
                        } else if (!feat.IsSetData() || !far_feat.IsSetData()
                                   || (!(feat.GetData().IsCdregion() 
                                         && far_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA)
                                       && !(feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA
                                             && far_feat.GetData().IsCdregion()))) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem, 
                                     "Cross-references are not between CDS and mRNA pair",
                                     feat);
                        } else if (feat.GetData().IsCdregion()) {
                            ECompare comp = Compare(feat.GetLocation(), far_feat.GetLocation(), m_Scope);
                            if ( (comp != eContained) && (comp != eSame)) {
                                PostErr (eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAXrefLocationProblem, 
                                         "CDS not contained within cross-referenced mRNA", feat);
                            }
                        }
                    }
                }
                if (!has_xref) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem, 
                             "Cross-referenced feature does not have its own cross-reference",
                             feat);
                }
            } else {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefFeatureMissing,
                         "Cross-referenced feature cannot be found", feat);
            }
        } else {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefFeatureMissing,
                     "Cross-referenced feature cannot be found",
                     feat);
        }
    } else if (xref.IsSetData() && xref.GetData().IsGene() && feat.GetData().IsGene()) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryGeneXref,
                 "Gene feature has gene cross-reference",
                 feat);
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


static const char* const sc_BypassCdsTransCheckText[] = {
  "RNA editing",
  "adjusted for low-quality genome",
  "annotated by transcript or proteomic data",
  "artificial frameshift",
  "mismatches in translation",
  "rearrangement required for product",
  "reasons given in citation",
  "translated product replaced",
  "unclassified translation discrepancy"
};
typedef CStaticArraySet<const char*, PCase_CStr> TBypassCdsTransCheckSet;
DEFINE_STATIC_ARRAY_MAP(TBypassCdsTransCheckSet, sc_BypassCdsTransCheck, sc_BypassCdsTransCheckText);


static void s_LocIdType(const CSeq_loc& loc, CScope& scope, const CSeq_entry& tse,
                        bool& is_nt, bool& is_ng, bool& is_nw, bool& is_nc)
{
    is_nt = is_ng = is_nw = is_nc = false;
    if (!IsOneBioseq(loc, &scope)) {
        return;
    }
    const CSeq_id& id = GetId(loc, &scope);
    CBioseq_Handle bsh = scope.GetBioseqHandleFromTSE(id, tse);
    if (bsh) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetBioseqCore())) {
            CSeq_id::EAccessionInfo info = (*it)->IdentifyAccession();
            is_nt |= (info == CSeq_id::eAcc_refseq_contig);
            is_ng |= (info == CSeq_id::eAcc_refseq_genomic);
            is_nw |= (info == CSeq_id::eAcc_refseq_wgs_intermed);
            is_nc |= (info == CSeq_id::eAcc_refseq_chromosome);
        }
    }
}


static bool s_LocIsNmAccession (const CSeq_loc& loc, CScope& scope)
{
    CBioseq_Handle bsh = BioseqHandleFromLocation (&scope, loc);
    if (bsh) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetBioseqCore())) {
            if ((*it)->IsOther() && (*it)->GetOther().IsSetAccession() 
                && NStr::StartsWith ((*it)->GetOther().GetAccession(), "NM_")) {
                return true;
            }
        }
    }
    return false;
}


bool CValidError_feat::ValidateCdRegionTranslation 
(const CSeq_feat& feat,
 const string& transl_prot,
 bool report_errors, 
 bool unclassified_except,
 bool& has_errors,
 bool& other_than_mismatch,
 bool& reported_bad_start_codon,
 bool& prot_ok)
{
    if (!feat.IsSetData() || !feat.GetData().IsCdregion()) {
        return false;
    }

    if (NStr::IsBlank(transl_prot)) {
        return true;
    }

    int gc = 0;
    if ( feat.GetData().GetCdregion().IsSetCode() ) {
        // We assume that the id is set for all Genetic_code
        gc = feat.GetData().GetCdregion().GetCode().GetId();
    }
    string gccode = NStr::IntToString(gc);

    // count internal stops and Xs
    size_t internal_stop_count = 0;

    bool got_dash = (transl_prot[0] == '-');
    bool got_x = (transl_prot[0] == 'X' 
                  && !feat.GetLocation().IsPartialStart(eExtreme_Biological));
    size_t num_x = 0, num_nonx = 0;

    ITERATE(string, it, transl_prot) {
        if ( *it == '*' ) {
            ++internal_stop_count;
        }
        if ( *it == 'X' ) {
            num_x++;
        } else {
            num_nonx++;
        }
    }
    // if stop at end, reduce count by one (since one of the stops counted isn't internal)
    if (transl_prot[transl_prot.length() - 1] == '*') {
        --internal_stop_count;
    }

    // report too many Xs
    if (num_x > num_nonx) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_CDShasTooManyXs, "CDS translation consists of more than 50% X residues", feat);
    }

    if (internal_stop_count > 0) {
        has_errors = true;
        other_than_mismatch = true;
        if (got_dash || got_x) {
            string codon_desc = got_dash ? "Illegal" : "Ambiguous";
            if (report_errors  ||  unclassified_except) {
                EDiagSev sev = eDiag_Error;
                if (unclassified_except) {
                    sev = eDiag_Warning;
                } else {                
                    PostErr(sev, eErr_SEQ_FEAT_StartCodon,
                        codon_desc + " start codon (and " + 
                        NStr::SizetToString(internal_stop_count) +
                        " internal stops). Probably wrong genetic code [" +
                        gccode + "]", feat);
                    reported_bad_start_codon = true;
                }
                NStr::ToLower(codon_desc);
                if (unclassified_except && m_Imp.IsGpipe() && m_Imp.IsGenomic()) {
                    // suppress if gpipe genomic
                } else {
                    PostErr(sev, eErr_SEQ_FEAT_InternalStop, 
                        NStr::SizetToString(internal_stop_count) + 
                        " internal stops (and " + codon_desc + " start codon). Genetic code [" + gccode + "]", feat);
                }
            }
        } else if (report_errors /* ||  unclassified_except */ ) {
            if (unclassified_except && m_Imp.IsGpipe() && m_Imp.IsGenomic()) {
                // suppress if gpipe genomic
            } else {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InternalStop, 
                    NStr::SizetToString(internal_stop_count) + 
                    " internal stops. Genetic code [" + gccode + "]", feat);
            }
        }
        prot_ok = false;
        if (internal_stop_count > 5) {
            return false;
        }
    } else if (got_dash || got_x) {
        string codon_desc = got_dash ? "Illegal" : "Ambiguous";
        has_errors = true;
        other_than_mismatch = true;
        if (report_errors && !reported_bad_start_codon && !unclassified_except) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon, 
                codon_desc + " start codon used. Wrong genetic code [" +
                gccode + "] or protein should be partial", feat);
            reported_bad_start_codon = true;
        }
    }
    return true;
}


void CValidError_feat::x_GetExceptionFlags
(const string& except_text,
 bool& report_errors,
 bool& unclassified_except,
 bool& mismatch_except,
 bool& frameshift_except,
 bool& rearrange_except,
 bool& product_replaced,
 bool& mixed_population,
 bool& low_quality,
 bool& rna_editing,
 bool& transcript_or_proteomic)
{
    ITERATE (TBypassCdsTransCheckSet, it, sc_BypassCdsTransCheck) {
        if (NStr::FindNoCase(except_text, *it) != NPOS) {
            report_errors = false;  // biological exception
        }
    }
    if (NStr::FindNoCase(except_text, "unclassified translation discrepancy") != NPOS) {
        unclassified_except = true;
    }
    if (NStr::FindNoCase(except_text, "mismatches in translation") != NPOS) {
        mismatch_except = true;
        report_errors = true;
    }
    if (NStr::FindNoCase(except_text, "artificial frameshift") != NPOS) {
        frameshift_except = true;
        report_errors = true;
    }
    if (NStr::FindNoCase(except_text, "rearrangement required for product") != NPOS) {
        rearrange_except = true;
    }
    if (NStr::FindNoCase(except_text, "translated product replaced") != NPOS) {
        product_replaced = true;
    }
    if (NStr::FindNoCase(except_text, "heterogeneous population sequenced") != NPOS) {
        mixed_population = true;
    }
    if (NStr::FindNoCase(except_text, "low-quality sequence region") != NPOS) {
        low_quality = true;
    }
    if (NStr::FindNoCase (except_text, "RNA editing") != NPOS) {
        rna_editing = true;
    }
}


void CValidError_feat::x_CheckCDSFrame
(const CSeq_feat& feat,
 const bool report_errors,
 bool& has_errors,
 bool& other_than_mismatch)
{
    const CCdregion& cdregion = feat.GetData().GetCdregion();
    const CSeq_loc& location = feat.GetLocation();
    unsigned int part_loc = SeqLocPartialCheck(location, m_Scope);
    string comment_text = "";
    if (feat.IsSetComment()) {
        comment_text = feat.GetComment();
    }

    // check frame
    if (cdregion.IsSetFrame() 
        && (cdregion.GetFrame() == CCdregion::eFrame_two || cdregion.GetFrame() == CCdregion::eFrame_three)) {
        // coding region should be 5' partial
        if (!(part_loc & eSeqlocPartial_Start)) {
            EDiagSev sev = eDiag_Error;
            /*
            EDiagSev sev = eDiag_Warning;
            if (s_LocIsNmAccession (location, *m_Scope)) {
                sev = eDiag_Error;
            }
            */
            has_errors = true;
            other_than_mismatch = true;
            if (report_errors) {
                PostErr (sev, eErr_SEQ_FEAT_SuspiciousFrame, 
                         "Suspicious CDS location - frame > 1 but not 5' partial", feat);
            }
        } else if ((part_loc & eSeqlocPartial_Start) && !Is5AtEndSpliceSiteOrGap (location)) {
            if (s_PartialAtGapOrNs(m_Scope, location, eSeqlocPartial_Nostart)
                && NStr::Find(comment_text, "coding region disrupted by sequencing gap") != string::npos) {
                // suppress
            } else {
                EDiagSev sev = eDiag_Warning;
                if (s_LocIsNmAccession (location, *m_Scope)) {
                    sev = eDiag_Error;
                }
                has_errors = true;
                other_than_mismatch = true;
                if (report_errors) {
                    PostErr (sev, eErr_SEQ_FEAT_SuspiciousFrame, 
                             "Suspicious CDS location - frame > 1 and not at consensus splice site", feat);
                }
            }
        }
    }
}


void CValidError_feat::x_FindTranslationStops
(const CSeq_feat& feat,
 bool& got_stop,
 bool& show_stop,
 bool& unable_to_translate,
 bool& alt_start,
 string& transl_prot
)
{
    const CCdregion& cdregion = feat.GetData().GetCdregion();
    try {
        if (m_Scope) {
            CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
            if (bsh) {
                if (feat.GetLocation().IsWhole()) {
                    size_t start = 0;
                    if (cdregion.IsSetFrame()) {
                        if (cdregion.GetFrame() == 2) {
                            start = 1;
                        } else if (cdregion.GetFrame() == 3) {
                            start = 2;
                        }
                    }
                    const CGenetic_code* genetic_code = NULL;
                    if (cdregion.IsSetCode()) {
                        genetic_code = &(cdregion.GetCode());
                    }
                    CRef<CSeq_id> id(new CSeq_id());
                    id->Assign(feat.GetLocation().GetWhole());
                    CRef<CSeq_loc> tmp(new CSeq_loc(*id, start, bsh.GetInst_Length() - 1));
                    CSeqTranslator::Translate(*tmp, bsh.GetScope(), transl_prot, genetic_code, true, false, &alt_start);
                    unable_to_translate = false;
                    if (!NStr::IsBlank(transl_prot)) {
                        if (NStr::EndsWith(transl_prot, "*")) {
                            got_stop = true;
                        }
                        show_stop = true;
                    }
                } else {
                    CSeqTranslator::Translate(feat, *m_Scope, transl_prot,
                                                true,   // include stop codons
                                                false,  // do not remove trailing X/B/Z
                                                &alt_start);
                    unable_to_translate = false;
                    if (!NStr::IsBlank(transl_prot)) {
                        if (NStr::EndsWith(transl_prot, "*")) {
                            got_stop = true;
                        }
                        show_stop = true;
                    }
                }
            }
        }
    } catch (CException&) {
    }
}


const CSeq_id* CValidError_feat::x_GetCDSProduct
(const CSeq_feat& feat,
 bool report_errors,
 size_t translation_length,
 CBioseq_Handle& prot_handle,
 string& farstr,
 bool& has_errors,
 bool& other_than_mismatch)
{
    const CSeq_id* protid = 0;

    if (feat.IsSetProduct()) {
        try {
            protid = &GetId(feat.GetProduct(), m_Scope);
        } catch (CException&) {}
        if (protid != NULL) {
            prot_handle = m_Scope->GetBioseqHandleFromTSE(*protid, m_Imp.GetTSE());
            if (!prot_handle  &&  m_Imp.IsFarFetchCDSproducts()) {
                prot_handle = m_Scope->GetBioseqHandle(*protid);
                farstr = "(far) ";
                if (!prot_handle) {
                    string label;
                    protid->GetLabel(&label);

                    PostErr(eDiag_Error, eErr_SEQ_FEAT_ProductFetchFailure, 
                          "Unable to fetch CDS product '" + label + "'", feat);
                    m_Imp.SetFarFetchFailure();
                }
            }
        }
    }
    if (!prot_handle) {
        if  (!m_Imp.IsStandaloneAnnot()) {
            if (translation_length > 6) {
                bool is_nt, is_ng, is_nw, is_nc;
                s_LocIdType(feat.GetLocation(), *m_Scope, m_Imp.GetTSE(), is_nt, is_ng, is_nw, is_nc);
                if (!(is_nt || is_ng || is_nw)) {
                    EDiagSev sev = eDiag_Error;
                    if (IsDeltaOrFarSeg(feat.GetLocation(), m_Scope)) {
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
                        other_than_mismatch = true;
                        if (report_errors) {
                            PostErr(sev, eErr_SEQ_FEAT_NoProtein, 
                                "No protein Bioseq given", feat);
                        }
                    }
                }
            }
        }
    }
    return protid;
}


void CValidError_feat::x_CheckTranslationMismatches
(const CSeq_feat& feat,
 CBioseq_Handle prot_handle,
 const string& transl_prot,
 const string& gccode,
 const string& farstr,
 bool report_errors,
 bool got_stop,
 bool rna_editing,
 bool mismatch_except,
 bool unclassified_except,
 bool reported_bad_start_codon,
 bool no_beg,
 bool no_end,
 size_t& len,
 size_t& num_mismatches,
 bool& no_product,
 bool& prot_ok,
 bool& show_stop,
 bool& has_errors,
 bool& other_than_mismatch)
{
    // can't check for mismatches unless there is a product
    if (prot_handle && prot_handle.IsAa()) {
        no_product = false;
        CSeqVector prot_vec = prot_handle.GetSeqVector();
        prot_vec.SetCoding(CSeq_data::e_Ncbieaa);
        size_t prot_len = prot_vec.size(); 
        len = transl_prot.length();

        if (prot_len > 1.2 * len) {
            if ((! feat.IsSetExcept_text()) || NStr::Find(feat.GetExcept_text(), "annotated by transcript or proteomic data") != string::npos) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProductLength,
                        "Protein product length [" + NStr::SizetToString(prot_len) + 
                        "] is more than 120% of the " + farstr + "translation length [" + 
                        NStr::SizetToString(len) + "]", feat);
            }
        }

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
                CSeqVectorTypes::TResidue p_res = prot_vec[i];
                CSeqVectorTypes::TResidue t_res = transl_prot[i];

                if (t_res != p_res) {
                    has_errors = true;
                    mismatches.push_back(i);
                    prot_ok = false;
                }
            }
        } else {
            has_errors = true;
            other_than_mismatch = true;
            if (report_errors || (rna_editing && (prot_len < len - 1 || prot_len > len))) {
                PostErr(rna_editing ? eDiag_Warning : eDiag_Error, eErr_SEQ_FEAT_TransLen,
                    "Given protein length [" + NStr::SizetToString(prot_len) + 
                    "] does not match " + farstr + "translation length [" + 
                    NStr::SizetToString(len) + "]", feat);
            }
        }
        
        // Mismatch on first residue
        string msg;
        if (!mismatches.empty()  &&  mismatches.front() == 0) {
            if (feat.IsSetPartial() && feat.GetPartial() && (!no_beg) && (!no_end)) {
                if (report_errors) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem, 
                        "Start of location should probably be partial",
                        feat);
                }
            } else if (transl_prot[mismatches.front()] == '-' ||
                       (transl_prot[mismatches.front()] == 'X' && !no_beg)) {
                if (report_errors && !reported_bad_start_codon && !unclassified_except) {
                    string codon_desc = transl_prot[mismatches.front()] == '-' ? "Illegal" : "Ambiguous";
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                        codon_desc + " start codon used. Wrong genetic code [" +
                        gccode + "] or protein should be partial", feat);
                }
            }
        }

        char prot_res, transl_res;
        string nuclocstr;
        num_mismatches = mismatches.size();
        if (num_mismatches > 10) {
            // report total number of mismatches and the details of the 
            // first and last.
            nuclocstr = MapToNTCoords(feat, feat.GetProduct(), mismatches.front());
            prot_res = prot_vec[mismatches.front()];
            transl_res = Residue(transl_prot[mismatches.front()]);
            msg = 
                NStr::SizetToString(mismatches.size()) + " mismatches found. " +
                "First mismatch at " + NStr::IntToString(mismatches.front() + 1) +
                ", residue in protein [" + prot_res + "]" +
                " != translation [" + transl_res + "]";
            if (!nuclocstr.empty()) {
                msg += " at " + nuclocstr;
            }
            nuclocstr = MapToNTCoords(feat, feat.GetProduct(), mismatches.back());
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
                PostErr(m_Imp.IsGeneious() ? eDiag_Warning : eDiag_Error, eErr_SEQ_FEAT_MisMatchAA, msg, feat);
            }
        } else {
            // report individual mismatches
            for (size_t i = 0; i < mismatches.size(); ++i) {
                nuclocstr = MapToNTCoords(feat, feat.GetProduct(), mismatches[i]);
                prot_res = prot_vec[mismatches[i]];
                transl_res = transl_prot[mismatches[i]];
                if (mismatches[i] == 0 && transl_res == '-') {
                    // skip - dash is expected to differ
                    num_mismatches--;
                } else {
                    EDiagSev sev = eDiag_Error;
                    if (prot_res == 'X' && (transl_res == 'B' || transl_res == 'Z' || transl_res == 'J')) {
                        sev = eDiag_Warning;
                    }
                    msg = farstr + "Residue " + NStr::IntToString(mismatches[i] + 1) +
                        " in protein [" + prot_res + "]" +
                        " != translation [" + transl_res + "]";
                    if (!nuclocstr.empty()) {
                        msg += " at " + nuclocstr;
                    }
                    if (report_errors  &&  !mismatch_except) {
                        PostErr(m_Imp.IsGeneious() ? eDiag_Warning : sev, eErr_SEQ_FEAT_MisMatchAA, msg, feat);
                    }
                }
            }
        }

        if (feat.CanGetPartial()  &&  feat.GetPartial()  &&
            num_mismatches == 0) {
            if (!no_beg  && !no_end) {
                if (report_errors) {
                    if (m_Imp.IsGpipe () && m_Imp.IsGenomic()) {
                        // suppress in gpipe genomic
                    } else {
                        if (!got_stop) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem, 
                                "End of location should probably be partial", feat);
                        } else {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                                "This SeqFeat should not be partial", feat);
                        }
                    }
                }
                show_stop = false;
            }
        }

        if (!transl_prot.empty()) {
            // look for discrepancy in number of terminal Xs between product and translation
            size_t transl_terminal_x = 0;
            size_t i = transl_prot.length() - 1;
            while (i > 0) {
                if (i == transl_prot.length() - 1 && transl_prot[i] == '*') {
                    // skip stop
                } else if (transl_prot[i] == 'X') {
                    transl_terminal_x++;
                } else {
                    break;
                }
                i--;
            }
            if (i == 0 && transl_prot[0] == 'X') {
                transl_terminal_x++;
            }
            size_t prod_terminal_x = 0;
            size_t prod_len = prot_vec.size() - 1;
            while (prod_len > 0 && prot_vec[prod_len] == 'X') {
                prod_terminal_x++;
                prod_len--;
            }
            if (prod_len == 0 && prot_vec[prod_len] == 'X') {
                prod_terminal_x++;
            }
            if (transl_terminal_x != prod_terminal_x) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_TerminalXDiscrepancy,
                         "Terminal X count for CDS translation (" + NStr::SizetToString (transl_terminal_x)
                         + ") and protein product sequence (" + NStr::SizetToString (prod_terminal_x)
                         + ") are not equal", feat);
            }
        }
    } else {
        if (feat.IsSetPartial() && feat.GetPartial() && (!no_beg) && (!no_end)) {
        } else if (NStr::StartsWith(transl_prot, "-")) {
            if (report_errors && !reported_bad_start_codon && !unclassified_except) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                        "Illegal start codon used. Wrong genetic code [" +
                        gccode + "] or protein should be partial", feat);
            }
        }
    }
}


void CValidError_feat::ValidateCdTrans(const CSeq_feat& feat)
{
    // bail if not CDS
    if (!feat.GetData().IsCdregion()) {
        return;
    }

    // do not validate for pseudo gene
    FOR_EACH_GBQUAL_ON_FEATURE (it, feat) {
        if ((*it)->IsSetQual()  &&  NStr::EqualNocase((*it)->GetQual(), "pseudo")) {
            return;
        }
    }

    bool prot_ok = true;
    int  ragged = 0;
    bool has_errors = false, unclassified_except = false,
        mismatch_except = false, frameshift_except = false,
        rearrange_except = false, product_replaced = false,
        mixed_population = false, low_quality = false,
        report_errors = true, other_than_mismatch = false,
        rna_editing = false, transcript_or_proteomic = false;
    string farstr;
    
    if (!m_Imp.IgnoreExceptions() &&
        feat.CanGetExcept()  &&  feat.GetExcept()  &&
        feat.CanGetExcept_text()) {
        const string& except_text = feat.GetExcept_text();
        x_GetExceptionFlags (except_text,
                             report_errors,
                             unclassified_except,
                             mismatch_except,
                             frameshift_except,
                             rearrange_except,
                             product_replaced,
                             mixed_population,
                             low_quality,
                             rna_editing,
                             transcript_or_proteomic);
    }

    string comment_text = "";
    if (feat.IsSetComment()) {
        comment_text = feat.GetComment();
    }

    const CCdregion& cdregion = feat.GetData().GetCdregion();
    const CSeq_loc& location = feat.GetLocation();
    unsigned int part_loc = SeqLocPartialCheck(location, m_Scope);

    // check frame
    x_CheckCDSFrame(feat, report_errors, has_errors, other_than_mismatch);

    // check for unparsed transl_except
    bool transl_except = false;
    if (!cdregion.IsSetCode_break()) {
        FOR_EACH_GBQUAL_ON_FEATURE (it, feat) {
            if ((*it)->IsSetQual()  &&  NStr::Equal((*it)->GetQual(), "transl_except")) {
                transl_except = true;
            }
        }
    }
    
    
    int gc = 0;
    if ( cdregion.CanGetCode() ) {
        // We assume that the id is set for all Genetic_code
        gc = cdregion.GetCode().GetId();
    }
    string gccode = NStr::IntToString(gc);

    string transl_prot;   // translated protein
    bool got_stop = false;
    bool alt_start = false;
    bool show_stop = false;
    bool unable_to_translate = true;

    x_FindTranslationStops(feat, got_stop, show_stop, unable_to_translate, alt_start, transl_prot);

    if (unable_to_translate) {
        if (report_errors) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_CdTransFail, 
                "Unable to translate", feat);
        }
        other_than_mismatch = true;
    }

    // check alternative start codon
    if (alt_start  &&  gc == 1) {
        // do not report
    } else if (!alt_start && feat.IsSetExcept() && feat.IsSetExcept_text()
               && NStr::Find(feat.GetExcept_text(), "alternative start codon") != string::npos) {
        CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
        if (bsh) {
            bool is_refseq_loc = false;
            FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(bsh.GetCompleteBioseq())) {
                if ((*id_it)->IsOther() && (*id_it)->GetOther().IsSetAccession()
                    && NStr::StartsWith((*id_it)->GetOther().GetAccession(), "NM_")) {
                    is_refseq_loc = true;
                    break;
                }
            }
            if (is_refseq_loc) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_AltStartCodon, 
                         "Unnecessary alternative start codon exception", feat);
            }
        }
    }

    bool no_end = false;
    unsigned int part_prod = eSeqlocPartial_Complete;
    if (feat.IsSetProduct()) {
        part_prod = SeqLocPartialCheck(feat.GetProduct(), m_Scope);
        if ( (part_loc & eSeqlocPartial_Stop)  ||
            (part_prod & eSeqlocPartial_Stop) ) {
            no_end = true;
        } else {    
            // complete stop, so check for ragged end
            ragged = CheckForRaggedEnd(location, cdregion);
        }
    } else if (part_loc & eSeqlocPartial_Stop) {
        no_end = true;
    }

    if (report_errors) {
      x_ValidateCdregionCodebreak(cdregion, feat);
    }

    // check for code break not on a codon
    if (x_ValidateCodeBreakNotOnCodon(feat, location, cdregion, report_errors)) {
        has_errors = true;
        other_than_mismatch = true;
    }
    
    bool no_beg = 
        (part_loc & eSeqlocPartial_Start)  ||  (part_prod & eSeqlocPartial_Start);

    bool reported_bad_start_codon = false;

    if (!ValidateCdRegionTranslation (feat, transl_prot, report_errors, unclassified_except, has_errors, 
        other_than_mismatch, reported_bad_start_codon, prot_ok)) {
        return;
    }

    
    CBioseq_Handle prot_handle;
    x_GetCDSProduct(feat, 
                    report_errors,
                    transl_prot.length(),
                    prot_handle,
                    farstr,
                    has_errors,
                    other_than_mismatch);

    size_t num_mismatches = 0;
    size_t len = 0;

    show_stop = true;

    bool no_product = true;
    x_CheckTranslationMismatches(feat,
                                 prot_handle,
                                 transl_prot,
                                 gccode,
                                 farstr,
                                 report_errors,
                                 got_stop,
                                 rna_editing,
                                 mismatch_except,
                                 unclassified_except,
                                 reported_bad_start_codon,
                                 no_beg,
                                 no_end,
                                 len,
                                 num_mismatches,
                                 no_product,
                                 prot_ok,
                                 show_stop,
                                 has_errors,
                                 other_than_mismatch);

    if (!unable_to_translate) {
        ReportCdTransErrors(feat, show_stop, got_stop, no_end, ragged, 
            report_errors, has_errors);
    }

    if (report_errors && transl_except) {
        if (prot_ok) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExcept,
                "Unparsed transl_except qual (but protein is okay). Skipped", feat);
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExcept,
                "Unparsed transl_except qual. Skipped", feat);
        }
    }

    if (!report_errors && !no_product) {
        if (! has_errors) {
            if (!frameshift_except && !rearrange_except && !mixed_population && !low_quality) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
                    "CDS has exception but passes translation test", feat);
            }
        } else if (unclassified_except && !other_than_mismatch) {
            if (num_mismatches * 50 <= len) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_ErroneousException,
                    "CDS has unclassified exception but only difference is "
                    + NStr::SizetToString (num_mismatches) + " mismatches out of " 
                    + NStr::SizetToString (len) + " residues", feat);
            }
        } else if (product_replaced) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnqualifiedException,
                "CDS has unqualified translated product replaced exception", feat);
        }
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
    bool alt_start = false;

    // need to translate a version of the coding region without the code breaks,
    // to see if the code breaks are necessary
    CRef<CSeq_feat> tmp_cds (new CSeq_feat());
    tmp_cds->SetLocation().Assign(feat.GetLocation());
    tmp_cds->SetData().SetCdregion();
    if (cdregion.IsSetFrame()) {
        tmp_cds->SetData().SetCdregion().SetFrame(cdregion.GetFrame());
    }
    if (cdregion.IsSetCode()) {
        tmp_cds->SetData().SetCdregion().SetCode().Assign(cdregion.GetCode());
    }

    // now, will use tmp_cds to translate individual code breaks;
    tmp_cds->SetData().SetCdregion().ResetFrame();

    FOR_EACH_CODEBREAK_ON_CDREGION (cbr, cdregion) {
        if (!(*cbr)->IsSetLoc()) {
            continue;
        }
        // if the code break is outside the coding region, skip
        ECompare comp = Compare((*cbr)->GetLoc(), feat.GetLocation(), m_Scope);
        if ( (comp != eContained) && (comp != eSame)) {
            continue;
        }

        size_t codon_length = GetLength((*cbr)->GetLoc(), m_Scope);
        TSeqPos from = LocationOffset(loc, (*cbr)->GetLoc(), 
            eOffset_FromStart, m_Scope);
        
        TSeqPos from_end = LocationOffset(loc, (*cbr)->GetLoc(), 
            eOffset_FromEnd, m_Scope);
        
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
        if ((*cbr)->IsSetAa() && (*cbr)->IsSetLoc()) {
            tmp_cds->SetLocation().Assign((*cbr)->GetLoc());
            tmp_cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
            tmp_cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
            string cb_trans = "";
            try {
                CSeqTranslator::Translate(*tmp_cds, *m_Scope, cb_trans,
                                          true,   // include stop codons
                                          false,  // do not remove trailing X/B/Z
                                          &alt_start);
            } catch (CException&) {
            }
            size_t prot_pos = from / 3;

            unsigned char ex = 0;
            vector<char> seqData;
            string str = "";
            bool not_set = false;

            switch ((*cbr)->GetAa().Which()) {
                case CCode_break::C_Aa::e_Ncbi8aa:
                    str = (*cbr)->GetAa().GetNcbi8aa();
                    CSeqConvert::Convert(str, CSeqUtil::e_Ncbi8aa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
                    ex = seqData[0];
                    break;
                case CCode_break::C_Aa::e_Ncbistdaa:
                    str = (*cbr)->GetAa().GetNcbi8aa();
                    CSeqConvert::Convert(str, CSeqUtil::e_Ncbistdaa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
                    ex = seqData[0];
                    break;
                case CCode_break::C_Aa::e_Ncbieaa:
                    seqData.push_back((*cbr)->GetAa().GetNcbieaa());
                    ex = seqData[0];
                    break;
                default:
                    // do nothing, code break wasn't actually set
                    not_set = true;
                    break;
            }

            if (!not_set) {
                string except_char = "";
                except_char += ex;

               //At the beginning of the CDS
                if (prot_pos == 0 && ex != 'M') {
                    if ((! feat.IsSetPartial()) || (! feat.GetPartial())) {
                        string msg = "Suspicious transl_except ";
                        msg += ex;
                        msg += " at first codon of complete CDS";
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryTranslExcept,
                                 msg,
                                 feat);
                    }
                }

               // Anywhere in CDS, where exception has no effect
                if (from_end > 0) {
                    if (from_end < 2 && NStr::Equal (except_char, "*")) {
                        // this is a necessary terminal transl_except
                    } else if (NStr::EqualNocase (cb_trans, except_char)) {
                        string msg = "Unnecessary transl_except ";
                        msg += ex;
                        msg += " at position ";
                        msg += NStr::SizetToString (prot_pos + 1);
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryTranslExcept,
                                 msg,
                                 feat);
                    }
                } else if (from_end <= 0) {
                    if (!NStr::Equal (except_char, "*")) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryTranslExcept,
                                 "Unexpected transl_except " + except_char
                                 + " at position " + NStr::SizetToString (prot_pos + 1)
                                 + " just past end of protein",
                                 feat);
                    }
                }
            }
        }
    }
    return has_errors;
}


vector < CConstRef <CSeq_feat> > GetFeaturesWithLabel (CBioseq_Handle bsh, string label, CScope * scope)
{
    vector < CConstRef <CSeq_feat> > feat_list;
    string feat_label;

    for (CFeat_CI it(bsh); it; ++it) {
        feature::GetLabel(it->GetOriginalFeature(), &feat_label, feature::fFGL_Content, scope);
        if (NStr::Equal(label, feat_label)) {
            const CSeq_feat& f = it->GetOriginalFeature();
            CConstRef <CSeq_feat> r(&f);
            feat_list.push_back (r);
        }
    }
    return feat_list;
}


static bool s_LocationStrandsIncompatible (const CSeq_loc& loc1, const CSeq_loc& loc2, CScope * scope)
{
    ENa_strand strand1 = loc1.GetStrand();
    ENa_strand strand2 = loc2.GetStrand();

    if (strand1 == strand2) {
        return false;
    }
    if ((strand1 == eNa_strand_unknown || strand1 == eNa_strand_plus) &&
        (strand2 == eNa_strand_unknown || strand2 == eNa_strand_plus)) {
            return false;
    }
    if (strand1 == eNa_strand_other) {
        ECompare comp = Compare(loc1, loc2, scope);
        if (comp == eContains) {
            return false;
        }
    } else if (strand2 == eNa_strand_other) {
        ECompare comp = Compare(loc1, loc2, scope);
        if (comp == eContained) {
            return false;
        }
    }

    return true;
}


static bool s_GeneRefsAreEquivalent (const CGene_ref& g1, const CGene_ref& g2, string& label)
{
    bool equivalent = false;
    if (g1.IsSetLocus_tag()
        && g2.IsSetLocus_tag()) {
        if (NStr::EqualNocase(g1.GetLocus_tag(),
                              g2.GetLocus_tag())) {
            label = g1.GetLocus_tag();
            equivalent = true;
        }
    } else if (g1.IsSetLocus()
               && g2.IsSetLocus()) {
        if (NStr::EqualNocase(g1.GetLocus(),
                              g2.GetLocus())) {
            label = g1.GetLocus();
            equivalent = true;
        }
    } else if (g1.IsSetSyn()
               && g2.IsSetSyn()) {
        if (NStr::EqualNocase (g1.GetSyn().front(),
                               g2.GetSyn().front())) {
            label = g1.GetSyn().front();
            equivalent = true;
        }
    }                    
    return equivalent;
}


// Check for redundant gene Xref
// Do not call if feat is gene
void CValidError_feat::ValidateGeneXRef(const CSeq_feat& feat)
{
    if (feat.IsSetData() && feat.GetData().IsGene()) {
        return;
    }

    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
    // if we can't get the bioseq on which the gene is located, we can't check for 
    // overlapping/ambiguous/redundant conditions
    if (!bsh) {
        return;
    }

    const CGene_ref* gene_xref = feat.GetGeneXref();

    size_t num_genes = 0;
    size_t max = 0;
    size_t num_trans_spliced = 0;
    bool equivalent = false;
    /*
    CFeat_CI gene_it(bsh, CSeqFeatData::e_Gene);
    */
    CFeat_CI gene_it(*m_Scope, feat.GetLocation(), SAnnotSelector (CSeqFeatData::e_Gene));
    CFeat_CI prev_gene;
    string label = "?";
    bool xref_match_same_as_overlap = false;

    while (gene_it) {
        if (TestForOverlapEx (gene_it->GetLocation(), feat.GetLocation(), 
          gene_it->GetLocation().IsInt() ? eOverlap_Contained : eOverlap_Subset, m_Scope) >= 0) {
            size_t len = GetLength(gene_it->GetLocation(), m_Scope);
            if (len < max || num_genes == 0) {
                num_genes = 1;
                max = len;
                num_trans_spliced = 0;
                if (gene_it->IsSetExcept() && gene_it->IsSetExcept_text() &&
                    NStr::FindNoCase (gene_it->GetExcept_text(), "trans-splicing") != string::npos) {
                    num_trans_spliced++;
                }
                equivalent = false;
                prev_gene = gene_it;
                if (gene_xref && s_GeneRefsAreEquivalent(*gene_xref, gene_it->GetData().GetGene(), label)) {
                    xref_match_same_as_overlap = true;
                } else {
                    xref_match_same_as_overlap = false;
                }
            } else if (len == max) {
                equivalent |= s_GeneRefsAreEquivalent(gene_it->GetData().GetGene(), prev_gene->GetData().GetGene(), label);
                num_genes++;
                if (gene_it->IsSetExcept() && gene_it->IsSetExcept_text() &&
                    NStr::FindNoCase (gene_it->GetExcept_text(), "trans-splicing") != string::npos) {
                    num_trans_spliced++;
                }
            }
        }
        ++gene_it;
    }

    if ( gene_xref == 0) {
        // if there is no gene xref, then there should be 0 or 1 overlapping genes
        // so that mapping by overlap is unambiguous
        if (num_genes > 1) {
            if (m_Imp.IsSmallGenomeSet() && num_genes == num_trans_spliced) {
                /* suppress for trans-spliced genes on small genome set */
            } else if (equivalent) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneXrefNeeded,
                         "Feature overlapped by "
                         + NStr::SizetToString(num_genes)
                         + " identical-length equivalent genes but has no cross-reference",
                         feat);
            } else {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_MissingGeneXref,
                         "Feature overlapped by "
                         + NStr::SizetToString(num_genes)
                         + " identical-length genes but has no cross-reference",
                         feat);
            }
        } else if (num_genes == 1 
                   && prev_gene->GetData().GetGene().IsSetAllele()
                   && !NStr::IsBlank(prev_gene->GetData().GetGene().GetAllele())) {
            const string& allele = prev_gene->GetData().GetGene().GetAllele();
            // overlapping gene should not conflict with allele qualifier
            FOR_EACH_GBQUAL_ON_FEATURE (qual_iter, feat) {
                const CGb_qual& qual = **qual_iter;
                if ( qual.IsSetQual()  &&
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
    } else if ( !gene_xref->IsSuppressed() ) {
        // we are counting features with gene xrefs
        m_Imp.IncrementGeneXrefCount();

        // compare gene xref to overlapping gene
        if (xref_match_same_as_overlap && num_genes == 1) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryGeneXref,
                "Unnecessary gene cross-reference " + label, feat);
        } else {
            // report suspicious gene xrefs
            bool do_report = true;
            if (feat.IsSetExcept_text() && NStr::Find (feat.GetExcept_text(), "dicistronic gene") != string::npos) {
                // but not if feature has dicistronic gene exception
                do_report = false;
            }

            if (do_report) {
                do_report = false;
                // only do test if in gen-prod-set or has one of a specific kind of accession
                FOR_EACH_SEQID_ON_BIOSEQ (it, *bsh.GetCompleteBioseq()) {
                    if ((*it)->IsOther() && (*it)->GetOther().IsSetAccession()) {
                        const string& accession = (*it)->GetOther().GetAccession();
                        if (NStr::StartsWith (accession, "NT_")
                            || NStr::StartsWith (accession, "NC_")
                            || NStr::StartsWith (accession, "NG_")
                            || NStr::StartsWith (accession, "NW_")) {
                            do_report = true;
                            break;
                        }
                    }
                }
                if (!do_report && GetGenProdSetParent (bsh)) {
                    do_report = true;
                }
            }
            if (do_report) {
                do_report = false;
                // only test if drosophila, because
                // curated fly source still has duplicate features
                CSeqdesc_CI dbsrc_i(bsh, CSeqdesc::e_Source);
                if (dbsrc_i && dbsrc_i->GetSource().IsSetTaxname()
                    && NStr::EqualNocase (dbsrc_i->GetSource().GetTaxname(), "Drosophila melanogaster")) {
                    do_report = true;
                }
            }
            if (do_report) {
                string xref_label;
                gene_xref->GetLabel(&xref_label);
                vector < CConstRef <CSeq_feat> > features_with_label = GetFeaturesWithLabel (bsh, xref_label, m_Scope);
                if (features_with_label.size() > 0) {
                    // ignore if gene xref is necessary because of strandedness
                    vector < CConstRef <CSeq_feat> >::iterator f1 = features_with_label.begin();
                    if (Compare (feat.GetLocation(), (*f1)->GetLocation(), m_Scope) == eContained) {
                        ENa_strand strand1 = eNa_strand_unknown;
                        ENa_strand strand2 = eNa_strand_unknown;
                        if ((*f1)->IsSetLocation() && (*f1)->GetLocation().IsSetStrand()) {
                            strand1 = (*f1)->GetLocation().GetStrand();
                        }
                        if (feat.IsSetLocation() && feat.GetLocation().IsSetStrand()) {
                            strand2 = feat.GetLocation().GetStrand();
                        }
                        if (strand1 == strand2) {
                            do_report = false;
                        } else if (strand1 == eNa_strand_unknown && strand2 != eNa_strand_minus) {
                            do_report = false;
                        } else if (strand1 != eNa_strand_minus && strand2 == eNa_strand_unknown) {
                            do_report = false;
                        } else if (strand1 == eNa_strand_both && strand2 != eNa_strand_minus) {
                            do_report = false;
                        } else if (strand1 != eNa_strand_minus && strand2 == eNa_strand_both) {
                            do_report = false;
                        }
                    }
                }
                if (do_report) {
                    if (NStr::IsBlank(xref_label)) {
                        xref_label = "?";
                    }
                    
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_SuspiciousGeneXref, 
                             "Curated Drosophila record should not have gene cross-reference " + xref_label,
                             feat);
                }
            }
        }

        // make sure overlapping gene and gene xref do not conflict
        if (gene_xref->IsSetAllele() && !NStr::IsBlank(gene_xref->GetAllele())) {
            const string& allele = gene_xref->GetAllele();

            FOR_EACH_GBQUAL_ON_FEATURE (qual_iter, feat) {
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


        // find gene on bioseq to match genexref
        if (gene_xref->IsSetLocus() && !NStr::IsBlank (gene_xref->GetLocus())) {
            SAnnotSelector sel (CSeqFeatData::e_Gene);
            CFeat_CI f (bsh, sel);
            bool found = false;
            while (f && !found) {
                string label;
                f->GetData().GetGene().GetLabel(&label);
                if (NStr::Equal(label, gene_xref->GetLocus())) {
                    found = true;
                    if (s_LocationStrandsIncompatible(f->GetLocation(), feat.GetLocation(), m_Scope)) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneXrefStrandProblem,
                            "Gene cross-reference is not on expected strand", feat);
                    }
                } else {
                    ++f;
                }
            }
            if (!found && bsh.IsAa()) {
                const CSeq_feat* cds = GetCDSForProduct(bsh);
                if ( cds != 0 ) {
                    if (cds->IsSetLocation()) {
                        const CSeq_loc& loc = cds->GetLocation();
                        const CSeq_id* id = loc.GetId();
                        if (id != NULL) {
                            CBioseq_Handle nbsh = m_Scope->GetBioseqHandle(*id);
                            if (nbsh) {
                                CFeat_CI nf (nbsh, sel);
                                while (nf && !found) {
                                    string label;
                                    nf->GetData().GetGene().GetLabel(&label);
                                    if (NStr::Equal(label, gene_xref->GetLocus())) {
                                        found = true;
                                    } else {
                                        ++nf;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (!found) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneXrefWithoutGene,
                         "Feature has gene locus cross-reference but no equivalent gene feature exists", feat);
            }
        }
        if (gene_xref->IsSetLocus_tag() 
            && !NStr::IsBlank (gene_xref->GetLocus_tag())) {
            SAnnotSelector sel (CSeqFeatData::e_Gene);
            CFeat_CI f (bsh, sel);
            bool found = false;
            while (f && !found) {
                if (f->GetData().GetGene().IsSetLocus_tag() 
                    && NStr::EqualCase (f->GetData().GetGene().GetLocus_tag(), gene_xref->GetLocus_tag())) {
                    found = true;
                    if (s_LocationStrandsIncompatible(f->GetLocation(), feat.GetLocation(), m_Scope)) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneXrefStrandProblem,
                            "Gene cross-reference is not on expected strand", feat);
                    }
                } else {
                    ++f;
                }
            }
            if (!found && bsh.IsAa()) {
                const CSeq_feat* cds = GetCDSForProduct(bsh);
                if ( cds != 0 ) {
                    if (cds->IsSetLocation()) {
                        const CSeq_loc& loc = cds->GetLocation();
                        const CSeq_id* id = loc.GetId();
                        if (id != NULL) {
                            CBioseq_Handle nbsh = m_Scope->GetBioseqHandle(*id);
                            if (nbsh) {
                                CFeat_CI nf (nbsh, sel);
                                while (nf && !found) {
                                    string label;
                                    nf->GetData().GetGene().GetLabel(&label);
                                    if (gene_xref->IsSetLocus() && NStr::Equal(label, gene_xref->GetLocus())) {
                                        found = true;
                                    } else {
                                        ++nf;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (!found) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneXrefWithoutGene,
                         "Feature has gene locus_tag cross-reference but no equivalent gene feature exists", feat);
            }
            if ((!gene_xref->IsSetLocus() || NStr::IsBlank (gene_xref->GetLocus()))
                && found && f->GetData().GetGene().IsSetLocus() 
                && !NStr::IsBlank (f->GetData().GetGene().GetLocus())) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneXrefWithoutLocus,
                         "Feature has Gene Xref with locus_tag but no locus, gene with locus_tag and locus exists", feat);
            }
        }
    }

}


void CValidError_feat::ValidateOperon(const CSeq_feat& gene)
{
    if (!m_Scope) {
        return;
    }

    CConstRef<CSeq_feat> operon = 
        GetOverlappingOperon(gene.GetLocation(), *m_Scope);
    if ( !operon  ||  !operon->CanGetQual() ) {
        return;
    }

    string label;
    feature::GetLabel(gene, &label, feature::fFGL_Content, m_Scope);
    if ( label.empty() ) {
        return;
    }

    FOR_EACH_GBQUAL_ON_FEATURE (qual_iter, *operon) {
        const CGb_qual& qual = **qual_iter;
        if( qual.CanGetQual()  &&  qual.CanGetVal() ) {
            if ( NStr::Compare(qual.GetQual(), "operon") == 0  &&
                 NStr::CompareNocase(qual.GetVal(), label) == 0) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Operon is same as gene - " + qual.GetVal(), gene);
            }
        }
    }
}

bool CValidError_feat::Is5AtEndSpliceSiteOrGap (const CSeq_loc& loc)
{
    CSeq_loc_CI loc_it(loc);
    if (!loc_it) {
        return false;
    }
    CConstRef<CSeq_loc> rng = loc_it.GetRangeAsSeq_loc();
    if (!rng) {
        return false;
    }

    TSeqPos end = rng->GetStart(eExtreme_Biological);
    CBioseq_Handle bsh = BioseqHandleFromLocation (m_Scope, *rng);
    if (!bsh) {
        return false;
    }

    ENa_strand strand = rng->GetStrand();
    if (strand == eNa_strand_minus) {
          TSeqPos seq_len = bsh.GetBioseqLength();
          if (end < seq_len - 1) {
              CSeqVector vec = bsh.GetSeqVector (CBioseq_Handle::eCoding_Iupac);
              if (vec.IsInGap(end + 1)) {
                  if (vec.IsInGap (end)) {
                      // not ok - location overlaps gap
                      return false;
                  } else {
                      // ok, location abuts gap
                  }
              } else if (end < seq_len - 2 && IsResidue (vec[end + 1]) && s_ConsistentWithC(vec[end + 1])
                         && IsResidue (vec[end + 2]) && s_ConsistentWithT(vec[end + 2])) {
                  // it's ok, it's abutting the reverse complement of AG
              } else {
                  return false;
              }
          } else {
              // it's ok, location endpoint is at the 3' end
          }
    } else {
          if (end > 0) {
              CSeqVector vec = bsh.GetSeqVector (CBioseq_Handle::eCoding_Iupac);
              if (vec.IsInGap(end - 1)) {
                  if (vec.IsInGap (end)) {
                      // not ok - location overlaps gap
                      return false;
                  } else {
                      // ok, location abuts gap
                  }
              } else {
                  if (end > 1 && IsResidue (vec[end - 1]) && s_ConsistentWithG(vec[end - 1])
                      && IsResidue(vec[end - 2]) && s_ConsistentWithA(vec[end - 2])) {
                      //it's ok, it's abutting "AG"
                  } else {
                      return false;
                  }
              }
          } else {
              // it's ok, location endpoint is at the 5' end
          }
    }   
    return true;
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
    if (ContainsSgml(comment)) {
        PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                 "feature comment " + comment + " has SGML",
                 feat);
    }
    if (feat.IsSetData() && feat.GetData().IsCdregion()
        && NStr::Find(comment, "ambiguity in stop codon") != string::npos
        && edit::DoesCodingRegionHaveTerminalCodeBreak(feat.GetData().GetCdregion())
        && m_Scope) {        
        CRef<CSeq_loc> stop_codon_loc = edit::GetLastCodonLoc(feat, *m_Scope);
        if (stop_codon_loc) {
            TSeqPos len = sequence::GetLength(*stop_codon_loc, m_Scope);
            CSeqVector vec(*stop_codon_loc, m_Scope, CBioseq_Handle::eCoding_Iupac);
            string seq_string;
            vec.GetSeqData(0, len - 1, seq_string);  
            bool found_ambig = false;
            string::iterator it = seq_string.begin();
            while (it != seq_string.end() && !found_ambig) {
                if (*it != 'A' && *it != 'T' && *it != 'C' && *it != 'G' && *it != 'U') {
                    found_ambig = true;
                }
                ++it;
            }
            if (!found_ambig) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_BadComment,
                    "Feature comment indicates ambiguity in stop codon "
                    "but no ambiguities are present in stop codon.", feat);                
            }
        }
    }
}


void CValidError_feat::ValidateFeatBioSource
(const CBioSource& bsrc,
 const CSeq_feat& feat)
{
    if ( bsrc.IsSetIs_focus() ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_FocusOnBioSourceFeature,
            "Focus must be on BioSource descriptor, not BioSource feature.",
            feat);
    }

    m_Imp.ValidateBioSource(bsrc, feat);

    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, feat.GetLocation());
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
            if ( !dbsrc.IsSetIs_focus()  &&  !m_Imp.IsTransgenic(dbsrc) ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_BioSourceNeedsFocus,
                    "BioSource descriptor must have focus or transgenic "
                    "when BioSource feature with different taxname is "
                    "present.", feat);
            }
        }
    }

}


size_t CValidError_feat::x_FindStartOfGap (CBioseq_Handle bsh, int pos)
{
    if (!bsh || !bsh.IsNa() || !bsh.IsSetInst_Repr() 
        || bsh.GetInst_Repr() != CSeq_inst::eRepr_delta
        || !bsh.GetInst().IsSetExt()
        || !bsh.GetInst().GetExt().IsDelta()) {
        return bsh.GetInst_Length();
    }
    size_t offset = 0;

    ITERATE (CSeq_inst::TExt::TDelta::Tdata, it, bsh.GetInst_Ext().GetDelta().Get()) {
        int len = 0;
        if ((*it)->IsLiteral()) {
            len = (*it)->GetLiteral().GetLength();
        } else if ((*it)->IsLoc()) {
            len = sequence::GetLength((*it)->GetLoc(), m_Scope);
        }
        if (pos >= offset && pos < offset + len) {
            return offset;
        } else {
            offset += len;
        }
    }
    return offset;
}


static bool xf_IsDeltaLitOnly (CBioseq_Handle bsh)

{
    if ( bsh.IsSetInst_Ext() ) {
        const CBioseq_Handle::TInst_Ext& ext = bsh.GetInst_Ext();
        if ( ext.IsDelta() ) {
            ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
                if ( (*it)->IsLoc() ) {
                    return false;
                }
            }
        }
    }
    return true;
}



void CValidError_feat::x_ValidateSeqFeatLoc(const CSeq_feat& feat)
{
    // check for bond locations - only allowable in bond feature and under special circumstances for het
    bool is_seqloc_bond = false;
    if (feat.IsSetData()) {
        if (feat.GetData().IsHet()) {
            // heterogen can have mix of bonds with just "a" point specified */
            for ( CSeq_loc_CI it(feat.GetLocation()); it; ++it ) {
                if (it.GetEmbeddingSeq_loc().IsBond() 
                    && (!it.GetEmbeddingSeq_loc().GetBond().IsSetA()
                        || it.GetEmbeddingSeq_loc().GetBond().IsSetB())) {
                    is_seqloc_bond = true;
                    break;
                }
            }
        } else if (!feat.GetData().IsBond()) {
            for ( CSeq_loc_CI it(feat.GetLocation()); it; ++it ) {
                if (it.GetEmbeddingSeq_loc().IsBond()) {
                    is_seqloc_bond = true;
                    break;
                }
            }
        }
    } else {
        for ( CSeq_loc_CI it(feat.GetLocation()); it; ++it ) {
            if (it.GetEmbeddingSeq_loc().IsBond()) {
                is_seqloc_bond = true;
                break;
            }
        }
    }

    if (is_seqloc_bond) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ImproperBondLocation,
                "Bond location should only be on bond features", feat);
    }

    // look for accession IDs in location
    if (m_Imp.DoesAnyFeatLocHaveGI() && s_SeqLocHasAccession (feat.GetLocation()) && !m_Imp.IsGpipe()) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureRefersToAccession, "Feature location refers to accession", feat);
    }
                    
    // check for location on multiple near non-part bioseqs
    //CSeq_entry_Handle tse = m_Scope->GetSeq_entryHandle(m_Imp.GetTSE());
    CSeq_entry_Handle tse = m_Imp.GetTSEH();
    const CSeq_loc& loc = feat.GetLocation();
    if (!IsOneBioseq(loc, m_Scope)) {
        CBioseq_Handle bsh;
        for (CSeq_loc_CI it(loc) ;it; ++it) {
            CBioseq_Handle seq = m_Scope->GetBioseqHandleFromTSE(it.GetSeq_id(), tse);
            if (seq  &&
                (!seq.GetExactComplexityLevel(CBioseq_set::eClass_parts)) &&
                (!seq.GetExactComplexityLevel(CBioseq_set::eClass_small_genome_set))) {
                if (bsh  &&  seq != bsh) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_MultipleBioseqs,
                        "Feature location refers to multiple near bioseqs.", feat);
                } else {
                    bsh = seq;
                }
            }
        }
    } 

    // feature location should not be whole
    if (loc.IsWhole ()) {
        string prefix = "Feature";
        if (feat.IsSetData()) {
            if (feat.GetData().IsCdregion()) {
                prefix = "CDS";
            } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
                prefix = "mRNA";
            }
        }
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_WholeLocation, prefix + " may not have whole location", feat);
    }

    if (m_Scope) {
        CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, loc);
        if (bsh) {
            // look for mismatch in capitalization for IDs
            CNcbiOstrstream os;
            const CSeq_id *id = loc.GetId();
            if (id) {
                id->WriteAsFasta(os);
                string loc_id = CNcbiOstrstreamToString(os);
                FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetCompleteBioseq())) {
                    if ((*it)->IsGi() || (*it)->IsGibbsq() || (*it)->IsGibbmt()) {
                        continue;
                    }
                    CNcbiOstrstream os2;
                    (*it)->WriteAsFasta(os2);
                    string bs_id = CNcbiOstrstreamToString(os2);
                    if (NStr::EqualNocase (loc_id, bs_id) && !NStr::EqualCase (loc_id, bs_id)) {
                        PostErr (eDiag_Error, eErr_SEQ_FEAT_FeatureSeqIDCaseDifference,
                                 "Sequence identifier in feature location differs in capitalization with identifier on Bioseq",
                                 feat);
                    }
                }
            }
            // look for protein features on the minus strand
            if (bsh.IsAa() && loc.IsSetStrand() && loc.GetStrand() == eNa_strand_minus) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_MinusStrandProtein, 
                         "Feature on protein indicates negative strand",
                         feat);
            }    

            // look for features inside gaps, crossing unknown gaps, or starting or ending in gaps
            // ignore gap features for this
            if (bsh.IsNa() && bsh.IsSetInst_Repr() && bsh.GetInst_Repr() == CSeq_inst::eRepr_delta
                && (!feat.GetData().IsImp() 
                    || !feat.GetData().GetImp().IsSetKey() 
                    || !NStr::EqualNocase(feat.GetData().GetImp().GetKey(), "gap"))) {
                try {
                    int num_n = 0;
                    int num_real = 0;
                    int num_gap = 0;
                    int num_unknown_gap = 0;
                    bool first_in_gap = false, last_in_gap = false;
                    bool local_first_gap, local_last_gap;
                    bool startsOrEndsInGap = false;
                    bool first = true;

                    for ( CSeq_loc_CI loc_it(loc); loc_it; ++loc_it ) {        
                        CSeqVector vec = GetSequenceFromLoc (loc_it.GetEmbeddingSeq_loc(), *m_Scope);
                        if ( !vec.empty() ) {
                            CBioseq_Handle ph = BioseqHandleFromLocation(m_Scope, loc_it.GetEmbeddingSeq_loc());
                            TSeqPos offset = loc_it.GetEmbeddingSeq_loc().GetStart (eExtreme_Positional);
                            string vec_data;
                            vec.GetSeqData(0, vec.size(), vec_data);

                            int pos = 0;
                            string::iterator it = vec_data.begin();
                            while (it != vec_data.end()) {
                                if (*it == 'N') {
                                    CSeqMap_CI map_iter(ph, SSeqMapSelector(), offset + pos);
                                    if (map_iter.GetType() == CSeqMap::eSeqGap) {
                                        if (map_iter.IsUnknownLength()) {
                                            num_unknown_gap ++;
                                        } else {
                                            num_gap++;
                                        }
                                    } else {
                                        num_n++;
                                    }
                                } else {
                                    num_real++;
                                }
                                ++it;
                                ++pos;
                            }
                        }
                        local_first_gap = false;
                        local_last_gap = false;
                        if (!vec.empty()) {
                            local_first_gap = vec.IsInGap(0);
                            local_last_gap = vec.IsInGap(vec.size() - 1);
                        }
                        if (first) {
                            first_in_gap = local_first_gap;
                            first = false;
                        }
                        last_in_gap = local_last_gap;
                        if (local_first_gap || local_last_gap) {
                            startsOrEndsInGap = true;
                        }
                    }
                    bool misc_feature_matches_gap = false;

                    if (num_real == 0 && num_n == 0
                        && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
                        TSeqPos start = loc.GetStart(eExtreme_Positional);
                        TSeqPos stop = loc.GetStop(eExtreme_Positional);
                        if ((start == 0 || CSeqMap_CI(bsh, SSeqMapSelector(), start - 1).GetType() != CSeqMap::eSeqGap ) 
                            && (stop == bsh.GetBioseqLength() - 1 || CSeqMap_CI(bsh, SSeqMapSelector(), stop + 1).GetType() != CSeqMap::eSeqGap )) {
                            misc_feature_matches_gap = true;
                        }
                    }
                            

                    if (num_gap == 0 && num_unknown_gap == 0 && num_n == 0) {
                        // ignore features that do not cover any gap characters
                    } else if (first_in_gap || last_in_gap) {
                        if (num_real > 0) {
                            size_t gap_start = x_FindStartOfGap(bsh, 
                                        first_in_gap ? loc.GetStart(eExtreme_Biological) 
                                                     : loc.GetStop(eExtreme_Biological));

                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureBeginsOrEndsInGap, 
                                    "Feature begins or ends in gap starting at " + NStr::NumericToString(gap_start + 1), feat);

                        } else if (misc_feature_matches_gap) {
                            // ignore (misc_) features that exactly cover the gap
                        } else {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureInsideGap, 
                                     "Feature inside sequence gap", feat);
                        }
                    } else if (num_real == 0 && num_gap == 0 && num_unknown_gap == 0 && num_n >= 50) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureInsideGap, 
                                 "Feature inside gap of Ns", feat);
                    } else if ((feat.GetData().IsCdregion() || feat.GetData().IsRna()) && num_unknown_gap > 0) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCrossesGap, 
                                 "Feature crosses gap of unknown length", feat);
                    } else if ((feat.GetData().IsCdregion() || feat.GetData().IsRna()) && startsOrEndsInGap) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_IntervalBeginsOrEndsInGap, 
                                 "Internal interval begins or ends in gap", feat);
                    }            
                } catch (CException ) {
                } catch (std::exception ) {
                }
            }

            // look for features with > 50% Ns
            // ignore gap features for this
            if (bsh.IsNa() && bsh.IsSetInst_Repr()) {
                CSeq_inst::TRepr repr = bsh.GetInst_Repr();
                if (repr == CSeq_inst::eRepr_raw ||
                    (repr == CSeq_inst::eRepr_delta && xf_IsDeltaLitOnly(bsh))) {
                    if (feat.GetData().IsImp() && feat.GetData().GetImp().IsSetKey() &&
                        (NStr::EqualNocase(feat.GetData().GetImp().GetKey(), "gap") || feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature)) {
                    } else {
                        try {
                            int num_n = 0;
                            int real_bases = 0;

                            for ( CSeq_loc_CI loc_it(loc); loc_it; ++loc_it ) {        
                                CSeqVector vec = GetSequenceFromLoc (loc_it.GetEmbeddingSeq_loc(), *m_Scope);
                                if ( !vec.empty() ) {
                                    CBioseq_Handle ph = BioseqHandleFromLocation(m_Scope, loc_it.GetEmbeddingSeq_loc());
                                    TSeqPos offset = loc_it.GetEmbeddingSeq_loc().GetStart (eExtreme_Positional);
                                    string vec_data;
                                    vec.GetSeqData(0, vec.size(), vec_data);

                                    int pos = 0;
                                    string::iterator it = vec_data.begin();
                                    while (it != vec_data.end()) {
                                        if (*it == 'N') {
                                            CSeqMap_CI map_iter(ph, SSeqMapSelector(), offset + pos);
                                            if (map_iter.GetType() == CSeqMap::eSeqGap) {
                                            } else {
                                                num_n++;
                                            }
                                        } else {
                                            if ((unsigned)(*it + 1) <= 256 && isalpha(*it)) {
                                              real_bases++;
                                            }
                                        }
                                        ++it;
                                        ++pos;
                                    }
                                }
                            }

                            if (num_n > real_bases) {
                                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureIsMostlyNs, 
                                         "Feature contains more than 50% Ns", feat);
                            }

                        } catch (CException ) {
                        } catch (std::exception ) {
                        }
                    }
                }
            }
        }
    }

    // for coding regions, internal exons should not be 15 or less bp long
    if (feat.IsSetData() && feat.GetData().IsCdregion()) {
        int num_short_exons = 0;
        CSeq_loc_CI it(loc);
        if (it) {
            // note - do not want to warn for first or last exon
            ++it;
            size_t prev_len = 16;
            while (it) {
                if (prev_len <= 15) {
                    num_short_exons ++;
                }
                prev_len = GetLength(*(it.GetRangeAsSeq_loc()), m_Scope);
                ++it;
            }            
        }
        if (num_short_exons > 1) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_ShortExon, 
                     "Coding region has multiple internal exons that are too short", feat);
        } else if (num_short_exons > 0) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_ShortExon, 
                     "Internal coding region exon is too short", feat);
        }
    }

}


void CValidError_feat::ValidateCharactersInField (string value, string field_name, const CSeq_feat& feat)
{
    if (HasBadCharacter (value)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadInternalCharacter, 
                 field_name + " contains undesired character", feat);
    }
    if (EndsWithBadCharacter (value)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadTrailingCharacter, 
                 field_name + " ends with undesired character", feat);
    }
    if (NStr::EndsWith (value, "-")) {
        if (!m_Imp.IsGpipe() || !m_Imp.BioSourceKind().IsOrganismEukaryote() )
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadTrailingHyphen, 
                field_name + " ends with hyphen", feat);
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
