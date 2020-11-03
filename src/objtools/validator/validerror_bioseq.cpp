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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Mati Shomrat ......
 *
 * File Description:
 *   validation of bioseq 
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objtools/validator/validerror_desc.hpp>
#include <objtools/validator/validerror_descr.hpp>
#include <objtools/validator/validerror_annot.hpp>
#include <objtools/validator/validerror_bioseq.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/dup_feats.hpp>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI8aa.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIpaa.hpp>
#include <objects/seq/NCBIpna.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <objects/seq/Pubdesc.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>
#include <objects/seqres/Int_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objects/taxon1/taxon1.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/util/create_defline.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/indexer.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>

#include <objtools/error_codes.hpp>
#include <objtools/edit/struc_comm_field.hpp>

#include <algorithm>

#include <objmgr/seq_loc_mapper.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
USING_SCOPE(sequence);
USING_SCOPE(feature);

class CCdsMatchInfo;

class CMrnaMatchInfo : public CObject {
public:
    CMrnaMatchInfo(const CSeq_feat& mrna, CScope* scope);
    const CSeq_feat& GetSeqfeat(void) const;
    bool Overlaps(const CSeq_feat& cds) const;
    void SetMatch();
    bool HasMatch(void) const;
    void SetPseudo(bool val = true) { m_IsPseudo = val; }
    bool OkWithoutCds(void) const;

private:
    CConstRef<CSeq_feat> m_Mrna;

    CScope* m_Scope;
    bool m_HasMatch;
    bool m_IsPseudo;
};


class CCdsMatchInfo : public CObject {
public:
    CCdsMatchInfo(const CSeq_feat& cds, CScope* scope);
    const CSeq_feat& GetSeqfeat(void) const;
    bool Overlaps(const CSeq_feat& mrna) const;
    bool AssignXrefMatch(TmRNAList& unmatched_mrnas, const CTSE_Handle& tse);
    bool AssignOverlapMatch(TmRNAList& unmatched_mrnas, CScope& scope);
    void UpdateOtherMrnas(const TmRNAList& unmatched_mrnas);    
    size_t CountOtherMrnas() { return m_OtherMrnas.size(); }
    bool AreMrnaProductsUnique() { return m_ProductsUnique; }
    void SetMatch(CRef<CMrnaMatchInfo> match);
    bool AssignMatch(TmRNAList& mrna_map, CFeatTree& feat_tree, CScope& scope);
    bool HasMatch(void) const;
    void NeedsMatch(bool needs_match);
    bool NeedsMatch(void) const;
    const CMrnaMatchInfo& GetMatch(void) const;
    bool IsPseudo(void) const;
    void SetPseudo(void);

private:
    CConstRef<CSeq_feat> m_Cds;
    CRef<CMrnaMatchInfo> m_BestMatch;

    sequence::EOverlapType m_OverlapType;
    CScope* m_Scope;
    bool m_IsPseudo;
    bool m_NeedsMatch;
    list<CConstRef<CSeq_feat> > m_OtherMrnas;
    bool m_ProductsUnique;
};


// =============================================================================
//                                     Public
// =============================================================================

CValidError_bioseq::CValidError_bioseq(CValidError_imp& imp) :
    CValidError_base(imp), m_AnnotValidator(imp), m_DescrValidator(imp), m_FeatValidator(imp), m_GeneIt(NULL), m_AllFeatIt(NULL)
{
}


CValidError_bioseq::~CValidError_bioseq()
{
}

void CValidError_bioseq::x_SetupCommonFlags (CBioseq_Handle bsh)
{

    CMolInfo::TTech tech = CMolInfo::eTech_unknown;
    CSeq_inst::TRepr repr = CSeq_inst::eRepr_not_set;

    if (bsh.IsSetInst_Repr()) {
        repr = bsh.GetInst_Repr();
    }

    CSeqdesc_CI m(bsh, CSeqdesc::e_Molinfo);
    while (m)
    {
        const CSeqdesc::TMolinfo& mi = m->GetMolinfo();
        if (mi.IsSetTech()) {
             tech = mi.GetTech();
        }

        ++m;
    }

    for (auto id : bsh.GetId()) {
        CSeq_id::EAccessionInfo acc_info = id.IdentifyAccession();
        unsigned int acc_div =  acc_info & CSeq_id::eAcc_division_mask;
        if (acc_div == CSeq_id::eAcc_wgs && tech == CMolInfo::eTech_wgs && repr == CSeq_inst::eRepr_virtual) {
            bool is_wgs_master = (acc_info & CSeq_id::fAcc_master) != 0;
            if (is_wgs_master) {
                m_report_short_seq = false;
            }
        }
    }

    CSeqdesc_CI d(bsh, CSeqdesc::e_Source);
    while (d)
    {
        const CSeqdesc::TSource& source = d->GetSource();

        // look for chromosome, prokaryote, linkage group
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE(it, source) {
            if ((*it)->IsSetSubtype() && (*it)->IsSetName() && !NStr::IsBlank((*it)->GetName())) {
                if ((*it)->GetSubtype() == CSubSource::eSubtype_chromosome) {
                    m_report_missing_chromosome = false;
                } else if ((*it)->GetSubtype() == CSubSource::eSubtype_linkage_group) {
                    m_report_missing_chromosome = false;
                }
            }
        }
        if (source.IsSetLineage()) {
            string lineage = source.GetLineage();
            if (NStr::StartsWith(lineage, "Bacteria; ") ||
                NStr::StartsWith(lineage, "Archaea; ")) {
                m_splicing_not_expected = true;
                m_report_missing_chromosome = false;
                m_is_bact_or_arch = true;
            }
            if (NStr::StartsWith(lineage, "Viruses; ")) {
                m_report_missing_chromosome = false;
            }
        }
        if (source.IsSetDivision()) {
            string div = source.GetDivision();
            if (NStr::Equal(div, "BCT") || NStr::Equal(div, "VRL")) {
                m_splicing_not_expected = true;
                m_report_missing_chromosome = false;
            }
        }
        if (source.IsSetGenome()) {
            CBioSource::TGenome genome = source.GetGenome();
            // check for organelle
            if (IsOrganelle(genome)) {
                m_report_missing_chromosome = false;
            }
            m_is_plasmid = (genome == NCBI_GENOME(plasmid));
            m_is_chromosome = (genome == NCBI_GENOME(chromosome));
            m_is_extrachrom = (genome == NCBI_GENOME(extrachrom));
        }

        ++d;
    }

}


void CValidError_bioseq::ValidateBioseq (
    const CBioseq& seq)
{
    m_splicing_not_expected = false;
    m_report_missing_chromosome = true;
    m_report_short_seq = true;
    m_is_bact_or_arch = false;
    m_is_plasmid = false;
    m_is_chromosome = false;
    m_is_extrachrom = false;

    try {
        m_CurrentHandle = m_Scope->GetBioseqHandle(seq);

        x_SetupCommonFlags(m_CurrentHandle);

        CSeq_entry_Handle appropriate_parent;
        if (m_Imp.ShouldSubdivide()) {
            appropriate_parent = GetAppropriateXrefParent(m_CurrentHandle.GetSeq_entry_Handle());
        }
        if (appropriate_parent) {
            CRef<CScope> tmp_scope(new CScope(*(CObjectManager::GetInstance())));
            tmp_scope->AddDefaults();
            CSeq_entry_Handle this_seh = tmp_scope->AddTopLevelSeqEntry(*(appropriate_parent.GetCompleteSeq_entry()));
            m_FeatValidator.SetScope(*tmp_scope);
            m_FeatValidator.SetTSE(this_seh);
        } else {
            m_FeatValidator.SetScope(*m_Scope);
            m_FeatValidator.SetTSE(m_CurrentHandle.GetTopLevelEntry());
        }
 
        try {
            CCacheImpl::SFeatKey gene_key(
                CSeqFeatData::e_Gene, CCacheImpl::kAnyFeatSubtype,
                m_CurrentHandle);
            m_GeneIt = &GetCache().GetFeatFromCache(gene_key);

            CCacheImpl::SFeatKey all_feat_key(
                CCacheImpl::kAnyFeatType, CCacheImpl::kAnyFeatSubtype,
                m_CurrentHandle);
            m_AllFeatIt = &GetCache().GetFeatFromCache(all_feat_key);
        } catch ( const exception& ) {
            // sequence might be too broken to validate features
            m_GeneIt = NULL;
            m_AllFeatIt = NULL;
        }
        ValidateSeqIds(seq);
        ValidateInst(seq);
        ValidateBioseqContext(seq);
        ValidateHistory(seq);
        FOR_EACH_ANNOT_ON_BIOSEQ (annot, seq) {
            m_AnnotValidator.ValidateSeqAnnot(**annot);
            m_AnnotValidator.ValidateSeqAnnotContext(**annot, seq);
        }
        if (seq.IsSetDescr()) {
            if (m_CurrentHandle) {
                CSeq_entry_Handle ctx = m_CurrentHandle.GetSeq_entry_Handle();
                if (ctx) {
                    m_DescrValidator.ValidateSeqDescr (seq.GetDescr(), *(ctx.GetCompleteSeq_entry()));
                }
            }
        }
        if (IsWGSMaster(seq, m_CurrentHandle.GetScope())) {
            ValidateWGSMaster(m_CurrentHandle);
        }
        if (appropriate_parent) {
            m_FeatValidator.SetScope(*m_Scope);
            m_FeatValidator.SetTSE(m_Imp.GetTSEH());
        }

    } catch ( const exception& e ) {
        m_Imp.PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
            string("Exception while validating bioseq. EXCEPTION: ") +
            e.what(), seq);
    }
    m_CurrentHandle.Reset();
    if (m_GeneIt) {
        m_GeneIt = NULL;
    }
    if (m_AllFeatIt) {
        m_AllFeatIt = NULL;
    }
}


static bool s_IsSkippableDbtag (const CDbtag& dbt) 
{
    if (!dbt.IsSetDb()) {
        return false;
    }
    const string& db = dbt.GetDb();
    if (NStr::EqualNocase(db, "TMSMART")
        || NStr::EqualNocase(db, "BankIt")
        || NStr::EqualNocase (db, "NCBIFILE")) {
        return true;
    } else {
        return false;
    }
}

static char CheckForBadSeqIdChars (const string& id)

{
    FOR_EACH_CHAR_IN_STRING(itr, id) {
        const char& ch = *itr;
        if (ch == '|' || ch == ',') return ch;
    }
    return '\0';
}

// VR-748
static char CheckForBadLocalIdChars(const string& id)
{
    for (size_t i = 0; i < id.length(); i++) {
        if (!CSeq_id::IsValidLocalID(id.substr(i, 1))) {
            return id.c_str()[i];
        }
    }
    return '\0';
}


static char CheckForBadFileIDSeqIdChars(const string& id)
{
    FOR_EACH_CHAR_IN_STRING(itr, id) {
        const char& ch = *itr;
        if (ch == '|' || ch == ',') return ch;
    }
    return '\0';
}


// validation for individual Seq-id
void CValidError_bioseq::ValidateSeqId(const CSeq_id& id, const CBioseq& ctx)
{
    // see if ID can be used to find ctx
    CBioseq_Handle ctx_handle = m_Scope->GetBioseqHandle(ctx);
    if (!ctx_handle) {
        if (!m_Imp.IsPatent()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
                "BioseqFind (" + id.AsFastaString() +
                ") unable to find itself - possible internal error", ctx);
        }
        return;
    }
    CTSE_Handle tse = ctx_handle.GetTSE_Handle();
    CBioseq_Handle bsh = tse.GetBioseqHandle(id);

    if (bsh) {
        CConstRef<CBioseq> core = bsh.GetBioseqCore();
        if (!core) {
            if ( !m_Imp.IsPatent() ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
                    "BioseqFind (" + id.AsFastaString() + 
                    ") unable to find itself - possible internal error", ctx);
            }
        } else if ( core.GetPointer() != &ctx ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
                "SeqID " + id.AsFastaString() + 
                " is present on multiple Bioseqs in record", ctx);
        }
    } else {
        PostErr(eDiag_Error, eErr_SEQ_INST_IdOnMultipleBioseqs,
            "BioseqFind (" + id.AsFastaString() + 
            ") unable to find itself - possible internal error", ctx);
    }

    //check formatting
    const CTextseq_id* tsid = id.GetTextseq_Id();

    switch (id.Which()) {
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            if ( IsHistAssemblyMissing(ctx)  &&  ctx.IsNa() ) {
                PostErr(eDiag_Info, eErr_SEQ_INST_HistAssemblyMissing,
                    "TPA record " + ctx.GetId().front()->AsFastaString() +
                    " should have Seq-hist.assembly for PRIMARY block", 
                    ctx);
            }
        // Fall thru 
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
            if ( tsid  &&  tsid->IsSetAccession() ) {
                const string& acc = tsid->GetAccession();
                const char badch = CheckForBadSeqIdChars (acc);
                if (badch != '\0') {
                    PostErr(eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat,
                           "Bad character '" + string(1, badch) + "' in accession '" + acc + "'", ctx);
                }
                CSeq_id::EAccessionInfo info = CSeq_id::IdentifyAccession(acc);
                if (info == CSeq_id::eAcc_unknown || 
                    (ctx.IsNa() && (info & CSeq_id::fAcc_prot)) ||
                    (ctx.IsAa() && (info & CSeq_id::fAcc_nuc))) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                        "Bad accession " + acc, ctx);
                }
                // Check for secondary conflicts
                ValidateSecondaryAccConflict(acc, ctx, CSeqdesc::e_Genbank);
                ValidateSecondaryAccConflict(acc, ctx, CSeqdesc::e_Embl);
            }
        // Fall thru
        case CSeq_id::e_Other:
            if ( tsid ) {
                if ( tsid->IsSetName() ) {
                    const string& name = tsid->GetName();
                    ITERATE (string, s, name) {
                        if (isspace((unsigned char)(*s))) {
                            PostErr(eDiag_Critical,
                                eErr_SEQ_INST_SeqIdNameHasSpace,
                                "Seq-id.name '" + name + "' should be a single "
                                "word without any spaces", ctx);
                            break;
                        }
                    }
                }

                if ( tsid->IsSetAccession()  &&  id.IsOther() ) {                    
                    const string& acc = tsid->GetAccession();
                    const char badch = CheckForBadSeqIdChars (acc);
                    if (badch != '\0') {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat,
                                "Bad character '" + string(1, badch) + "' in accession '" + acc + "'", ctx);
                    }
                    size_t num_letters = 0;
                    size_t num_digits = 0;
                    size_t num_underscores = 0;
                    bool bad_id_chars = false;
                    bool is_NZ = (NStr::CompareNocase(acc, 0, 3, "NZ_") == 0);
                    size_t i = 0;
                    bool letter_after_digit = false;

                    if ( is_NZ ) {
                        i = 3;
                    }

                    for ( ; i < acc.length(); ++i ) {
                        if ( isupper((unsigned char) acc[i]) ) {
                            num_letters++;
                        } else if ( isdigit((unsigned char) acc[i]) ) {
                            num_digits++;
                        } else if ( acc[i] == '_' ) {
                            num_underscores++;
                            if ( num_digits > 0  ||  num_underscores > 1 ) {
                                letter_after_digit = true;
                            }
                        } else {
                            bad_id_chars = true;
                        }
                    }

                    if ( letter_after_digit  ||  bad_id_chars ) {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad accession " + acc, ctx);
                    } else if ( is_NZ  &&  num_letters == 4  && 
                        ( num_digits == 8 || num_digits == 9 )  &&  num_underscores == 0 ) {
                        // valid accession - do nothing!
                    } else if ( is_NZ  &&  ValidateAccessionString (acc, false) == eAccessionFormat_valid ) {
                        // valid accession - do nothing!
                    } else if ( num_letters == 2  &&
                        (num_digits == 6  ||  num_digits == 8  || num_digits == 9)  &&
                        num_underscores == 1 ) {
                        // valid accession - do nothing!
                    } else if (num_letters == 4 && num_digits == 10 && ctx.IsNa()) {
                    } else {
                        PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad accession " + acc, ctx);
                    }
                }
            }
        // Fall thru
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
        case CSeq_id::e_Prf:
            if ( tsid ) {
                if ( ctx.IsNa()  &&  
                     (!tsid->IsSetAccession() || tsid->GetAccession().empty())) {
                    if ( ctx.GetInst().GetRepr() != CSeq_inst::eRepr_seg  ||
                        m_Imp.IsGI()) {
                        if (!id.IsDdbj()  ||
                            ctx.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
                            string msg = "Missing accession for " + id.AsFastaString();
                            PostErr(eDiag_Error,
                                eErr_SEQ_INST_BadSeqIdFormat,
                                msg, ctx);
                        }
                    }
                }
            } else {
                PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                    "Seq-id type not handled", ctx);
            }
            break;
        case CSeq_id::e_Gi:
            if (id.GetGi() <= ZERO_GI) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_ZeroGiNumber,
                         "Invalid GI number", ctx);
            }
            break;
        case CSeq_id::e_General:
            if (!id.GetGeneral().IsSetDb() || NStr::IsBlank(id.GetGeneral().GetDb())) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat, "General identifier missing database field", ctx);
            }
            if (id.GetGeneral().IsSetDb()) {
                const CDbtag& dbt = id.GetGeneral();
                size_t dblen = dbt.GetDb().length();
                EDiagSev sev = eDiag_Error;
                if (m_Imp.IsLocalGeneralOnly()) {
                    sev = eDiag_Critical;
                } else if (m_Imp.IsRefSeq()) {
                    sev = eDiag_Error;
                } else if (m_Imp.IsINSDInSep()) {
                    sev = eDiag_Error;
                } else if (m_Imp.IsIndexerVersion()) {
                    sev = eDiag_Error;
                }
                static const auto max_dblen = CSeq_id::kMaxGeneralDBLength;
                if (dblen > max_dblen) {
                    PostErr(sev, eErr_SEQ_INST_BadSeqIdFormat, "General database longer than " + NStr::NumericToString(max_dblen) + " characters", ctx);
                }
                if (! s_IsSkippableDbtag(dbt)) {
                    if (dbt.IsSetTag() && dbt.GetTag().IsStr()) {
                        size_t idlen = dbt.GetTag().GetStr().length();
                        static const auto maxlen = CSeq_id::kMaxGeneralTagLength;
                        if (idlen > maxlen) {
                            PostErr(sev, eErr_SEQ_INST_BadSeqIdFormat, "General identifier longer than " + NStr::NumericToString(maxlen) + " characters", ctx);
                        }
                        if (idlen == 0) {
                            PostErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat, "General identifier must not be an empty string", ctx);
                        }
                    }
                }
                if (dbt.IsSetTag() && dbt.GetTag().IsStr()) {
                    const string& acc = dbt.GetTag().GetStr();
                    char badch;
                    if (dbt.IsSetDb() && (NStr::Equal(dbt.GetDb(), "NCBIFILE") || NStr::Equal(dbt.GetDb(), "BankIt"))) {
                        badch = CheckForBadFileIDSeqIdChars(acc);
                    } else {
                        badch = CheckForBadLocalIdChars(acc);
                        if (badch == '\0' && dbt.IsSetDb()) {
                            badch = CheckForBadLocalIdChars(dbt.GetDb());
                        }
                    }
                    if (badch != '\0') {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat,
                                "Bad character '" + string(1, badch) + "' in sequence ID '" + id.AsFastaString() + "'", ctx);
                    }
                }
             }
           break;
        case CSeq_id::e_Local:
            if (id.IsLocal() && id.GetLocal().IsStr() && id.GetLocal().GetStr().length() > CSeq_id::kMaxLocalIDLength) {
                EDiagSev sev = eDiag_Error;
                if (! m_Imp.IsINSDInSep()) {
                    sev = eDiag_Critical;
                } else if (! m_Imp.IsIndexerVersion()) {
                    sev = eDiag_Error;
                }
                PostErr(sev, eErr_SEQ_INST_BadSeqIdFormat, "Local identifier longer than " + NStr::NumericToString(CSeq_id::kMaxLocalIDLength) + " characters", ctx);
            }
            if (id.IsLocal() && id.GetLocal().IsStr()) {
                const string& acc = id.GetLocal().GetStr();
                const char badch = CheckForBadLocalIdChars(acc);
                if (badch != '\0') {
                    PostErr(eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat,
                            "Bad character '" + string(1, badch) + "' in local ID '" + acc + "'", ctx);
                }
            }
            break;
        case CSeq_id::e_Pdb:
            if (id.IsPdb()) {
                const CPDB_seq_id& pdb = id.GetPdb();
                if (pdb.IsSetChain() && pdb.IsSetChain_id()) {
                    int chain = pdb.GetChain();
                    const string& chain_id = pdb.GetChain_id();
                    if (chain_id.size() == 1  &&  chain_id[0] == chain) {
                        break; // OK (straightforward match)
                    } else if (islower(chain)  &&  chain_id.size() == 2
                               &&  chain_id[0] == chain_id[1]
                               &&  chain_id[0] == toupper(chain)) {
                        break; // OK (historic special case)
                    } else if (chain == '|'  &&  chain_id == "VB") {
                        break; // OK (likewise)
                    } else {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                                "PDB Seq-id contains mismatched \'chain\' and"
                                " \'chain-id\' slots", ctx);
                    }
                }
            }
            break;
        default:
            break;
    }

#if 0 
        // disabled for now
        if (!IsNCBIFILESeqId(**i)) {
            string label;
            (*i)->GetLabel(&label);
            if (label.length() > 40) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat, 
                         "Sequence ID is unusually long (" + 
                         NStr::IntToString(label.length()) + "): " + label,
                         seq);
            }
        }
#endif

}

static bool x_IsWgsSecondary (const CBioseq& seq)

{
    FOR_EACH_DESCRIPTOR_ON_BIOSEQ (sd, seq) {
        const list< string > *extra_acc = 0;
        const CSeqdesc& desc = **sd;
        switch (desc.Which()) {
            case CSeqdesc::e_Genbank:
                if (desc.GetGenbank().IsSetExtra_accessions()) {
                    extra_acc = &(desc.GetGenbank().GetExtra_accessions());
                }
                break;
            case CSeqdesc::e_Embl:
                if (desc.GetEmbl().IsSetExtra_acc()) {
                    extra_acc = &(desc.GetEmbl().GetExtra_acc());
                }
                break;
            default:
                break;
        }
        if ( extra_acc ) {
            FOR_EACH_STRING_IN_LIST (acc, *extra_acc) {
                CSeq_id::EAccessionInfo info = CSeq_id::IdentifyAccession (*acc);
                if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs
                     && (info & CSeq_id::fAcc_master) != 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

// VR-728
// cannot have only seq-ids that will be stripped when loading to ID
void CValidError_bioseq::x_CheckGeneralIDs(const CBioseq& seq)
{
    bool found_good = false;
    ITERATE(CBioseq::TId, id_it, seq.GetId()) {
        if (!IsTemporary(**id_it)) {
            found_good = true;
        }
    }
    if (!found_good) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_NoIdOnBioseq,
            "The only ids on this Bioseq will be stripped during ID load", seq);
    }
}


void CValidError_bioseq::ValidateSeqIds
(const CBioseq& seq)
{
    // Ensure that CBioseq has at least one CSeq_id
    if ( !seq.IsSetId() || seq.GetId().empty() ) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_NoIdOnBioseq,
                 "No ids on a Bioseq", seq);
        return;
    }

    CSeq_inst::ERepr repr = seq.GetInst().GetRepr();

    // Loop thru CSeq_ids for this CBioseq. Determine if seq has
    // gi, NG, or NC. Check that the same CSeq_id not included more
    // than once.
    bool has_gi = false;
    bool is_lrg = false;
    bool has_ng = false;
    bool wgs_tech_needs_wgs_accession = false;
    bool is_segset_accession = false;
    bool has_wgs_general = false;
    bool is_eb_db = false;

    FOR_EACH_SEQID_ON_BIOSEQ (i, seq) {
        // first, do standalone validation
        ValidateSeqId (**i, seq);

        if ((*i)->IsGeneral() && (*i)->GetGeneral().IsSetDb()) {
            if (NStr::EqualNocase((*i)->GetGeneral().GetDb(), "LRG")) {
                is_lrg = true;
            }
            if (NStr::StartsWith((*i)->GetGeneral().GetDb(), "WGS:")) {
                has_wgs_general = true;
            }
        } else if ((*i)->IsOther() && (*i)->GetOther().IsSetAccession()) {
            const string& acc = (*i)->GetOther().GetAccession();
            if (NStr::StartsWith(acc, "NG_")) {
                has_ng = true;
                wgs_tech_needs_wgs_accession = true;
            } else if (NStr::StartsWith(acc, "NM_")
                        || NStr::StartsWith(acc, "NP_")
                        || NStr::StartsWith(acc, "NR_")) {
                wgs_tech_needs_wgs_accession = true;
            }
        } else if ((*i)->IsEmbl() && (*i)->GetEmbl().IsSetAccession()) {
            is_eb_db = true;
        } else if ((*i)->IsDdbj() && (*i)->GetDdbj().IsSetAccession()) {
            is_eb_db = true;
        }

        // Check that no two CSeq_ids for same CBioseq are same type
        CBioseq::TId::const_iterator j;
        for (j = i, ++j; j != seq.GetId().end(); ++j) {
            if ((**i).Compare(**j) != CSeq_id::e_DIFF) {
                CNcbiOstrstream os;
                os << "Conflicting ids on a Bioseq: (";
                (**i).WriteAsFasta(os);
                os << " - ";
                (**j).WriteAsFasta(os);
                os << ")";
                PostErr(eDiag_Error, eErr_SEQ_INST_ConflictingIdsOnBioseq,
                    CNcbiOstrstreamToString (os) /* os.str() */, seq);
            }
        }

        if ( (*i)->IsGenbank()  ||  (*i)->IsEmbl()  ||  (*i)->IsDdbj() ) {
            wgs_tech_needs_wgs_accession = true;
        } 

        if ( (*i)->IsGi() ) {
            has_gi = true;
        }

        if ( (*i)->IdentifyAccession() == CSeq_id::eAcc_segset) {
            is_segset_accession = true;
        }

    }
    if (is_lrg && !has_ng) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_ConflictingIdsOnBioseq,
            "LRG sequence needs NG_ accession", seq);
    }


    // Loop thru CSeq_ids to check formatting
    bool is_wgs = false;
    unsigned int gi_count = 0;
    unsigned int accn_count = 0;
    unsigned int lcl_count = 0;
    FOR_EACH_SEQID_ON_BIOSEQ (k, seq) {
        const CTextseq_id* tsid = (*k)->GetTextseq_Id();
        switch ((**k).Which()) {
        case CSeq_id::e_Local:
            lcl_count++;
            break;
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
            if ( tsid  &&  tsid->IsSetAccession() ) {
                if ((*k)->IsGenbank() || (*k)->IsEmbl() || (*k)->IsDdbj()) {
                    is_wgs |= IsWGSAccession(**k);
                }

                if ( has_gi ) {
                    if (tsid->IsSetVersion() && tsid->GetVersion() == 0) {
                        const string& acc = tsid->GetAccession();
                        PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                            "Accession " + acc + " has 0 version", seq);
                    }
                }
            }
        // Fall thru
        case CSeq_id::e_Other:
            if ( tsid ) {

                if ( has_gi && !tsid->IsSetAccession() && tsid->IsSetName() ) {
                    if ( (*k)->IsDdbj()  &&  repr == CSeq_inst::eRepr_seg ) {
                        // Don't report ddbj segmented sequence missing accessions 
                    } else {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                            "Missing accession for " + tsid->GetName(), seq);
                    }
                }
                accn_count++;
            }
            break;
            // Fall thru
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
        case CSeq_id::e_Prf:
            if ( tsid) {
                if ((!tsid->IsSetAccession() || NStr::IsBlank(tsid->GetAccession())) &&
                    (!tsid->IsSetName() || NStr::IsBlank(tsid->GetName())) &&
                    seq.GetInst().IsAa()) {
                    string label = (*k)->AsFastaString();
                    PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                            "Missing identifier for " + label, seq);
                }
                accn_count++;
            }
            break;
        
        case CSeq_id::e_Gi:
            gi_count++;
            break;
        default:
            break;
        }
    }

    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq));
    if (!SeqIsPatent(seq) && !seq.IsAa()) {
        if ( is_wgs ) {
            if ( !mi  ||  !mi->IsSetTech()  ||  
                ( mi->GetTech() != CMolInfo::eTech_wgs  &&
                  mi->GetTech() != CMolInfo::eTech_tsa &&
                  mi->GetTech() != CMolInfo::eTech_targeted) ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentMolInfoTechnique,
                    "WGS accession should have Mol-info.tech of wgs", seq);
            }
        } else if ( mi  &&  mi->IsSetTech()  &&  
                    mi->GetTech() == CMolInfo::eTech_wgs  &&
                    wgs_tech_needs_wgs_accession &&
                    !is_segset_accession &&
                    !has_wgs_general &&
                    !x_IsWgsSecondary(seq)) {
            EDiagSev sev = eDiag_Error;
            if (is_eb_db) {
                sev = eDiag_Warning;
            }
            if (! is_eb_db) {
                PostErr(sev, eErr_SEQ_DESCR_InconsistentWGSFlags,
                    "Mol-info.tech of wgs should have WGS accession", seq);
            }
        }

        if ((IsNTNCNWACAccession(seq) || IsNG(seq)) && mi && seq.IsNa()
                && (!mi->IsSetBiomol() 
                || (mi->GetBiomol() != CMolInfo::eBiomol_genomic 
                    && mi->GetBiomol() != CMolInfo::eBiomol_cRNA))) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentRefSeqMoltype,
                     "genomic RefSeq accession should use genomic or cRNA moltype",
                     seq);
        }
    }
    if (seq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
        if (mi && mi->IsSetBiomol()) {
            switch (mi->GetBiomol()) {
                case CMolInfo::eBiomol_pre_RNA:
                case CMolInfo::eBiomol_mRNA:
                case CMolInfo::eBiomol_rRNA:
                case CMolInfo::eBiomol_tRNA:
                case CMolInfo::eBiomol_snRNA:
                case CMolInfo::eBiomol_scRNA:
                case CMolInfo::eBiomol_cRNA:
                case CMolInfo::eBiomol_snoRNA:
                case CMolInfo::eBiomol_transcribed_RNA:
                case CMolInfo::eBiomol_ncRNA:
                case CMolInfo::eBiomol_tmRNA:
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentMolType,
                            "Molecule type (DNA) does not match biomol (RNA)", seq);
                    break;
                default:
                    break;
            }
        }
    }

    // Check that a sequence with a gi number has exactly one accession
    if ( gi_count > 0  &&  accn_count == 0  &&  !m_Imp.IsPDB()  &&  
         repr != CSeq_inst::eRepr_virtual ) {
        PostErr(eDiag_Error, eErr_SEQ_INST_GiWithoutAccession,
            "No accession on sequence with gi number", seq);
    }
    if (gi_count > 0  &&  accn_count > 1) {
        PostErr(eDiag_Error, eErr_SEQ_INST_MultipleAccessions,
            "Multiple accessions on sequence with gi number", seq);
    }

    x_CheckGeneralIDs(seq);

    if ( m_Imp.IsValidateIdSet() ) {
        ValidateIDSetAgainstDb(seq);
    }

    // C toolkit ensures that there is exactly one CBioseq for a CSeq_id
    // Not done here because object manager will not allow
    // the same Seq-id on multiple Bioseqs

}


bool CValidError_bioseq::IsHistAssemblyMissing(const CBioseq& seq)
{
    bool rval = false;
    const CSeq_inst& inst = seq.GetInst();
    if (inst.IsSetHist() && inst.GetHist().IsSetAssembly()) {
        return false;
    }
    CSeq_inst::TRepr repr = inst.CanGetRepr() ?
        inst.GetRepr() : CSeq_inst::eRepr_not_set;

    if ( seq.IsNa()  &&  repr != CSeq_inst::eRepr_seg ) {
        rval = true;
        // look for keyword
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
        CSeqdesc_CI genbank_i(bsh, CSeqdesc::e_Genbank);
        if (genbank_i && genbank_i->GetGenbank().IsSetKeywords()) {
            CGB_block::TKeywords::const_iterator keyword = genbank_i->GetGenbank().GetKeywords().begin();
            while (keyword != genbank_i->GetGenbank().GetKeywords().end() && rval) {
                if (NStr::EqualNocase(*keyword, "TPA:reassembly")) {
                    rval = false;
                }
                ++keyword;
            }
        }
        if (rval) {
            CSeqdesc_CI embl_i(bsh, CSeqdesc::e_Embl);
            if (embl_i && embl_i->GetEmbl().IsSetKeywords()) {
                CEMBL_block::TKeywords::const_iterator keyword = embl_i->GetEmbl().GetKeywords().begin();
                while (keyword != embl_i->GetEmbl().GetKeywords().end() && rval) {
                    if (NStr::EqualNocase(*keyword, "TPA:reassembly")) {
                        rval = false;
                    }
                    ++keyword;
                }
            }
        }
    }
    return rval;
}


void CValidError_bioseq::ValidateSecondaryAccConflict
(const string &primary_acc,
 const CBioseq &seq,
 int choice)
{
    CSeqdesc_CI sd(m_Scope->GetBioseqHandle(seq), static_cast<CSeqdesc::E_Choice>(choice));
    for (; sd; ++sd) {
        const list< string > *extra_acc = 0;
        if ( choice == CSeqdesc::e_Genbank  &&
            sd->GetGenbank().IsSetExtra_accessions() ) {
            extra_acc = &(sd->GetGenbank().GetExtra_accessions());
        } else if ( choice == CSeqdesc::e_Embl  &&
            sd->GetEmbl().IsSetExtra_acc() ) {
            extra_acc = &(sd->GetEmbl().GetExtra_acc());
        }

        if ( extra_acc ) {
            FOR_EACH_STRING_IN_LIST (acc, *extra_acc) {
                if ( NStr::CompareNocase(primary_acc, *acc) == 0 ) {
                    // If the same post error
                    PostErr(eDiag_Error,
                        eErr_SEQ_INST_BadSecondaryAccn,
                        primary_acc + " used for both primary and"
                        " secondary accession", seq);
                }
            }
        }
    }
}


bool HasUnverified(CBioseq_Handle bsh)
{
    for (CSeqdesc_CI it(bsh, CSeqdesc::e_User); it; ++it) {
        if (it->GetUser().GetObjectType() == CUser_object::eObjectType_Unverified) {
            return true;
        }
    }
    return false;
}


void CValidError_bioseq::x_ValidateBarcode(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle (seq);
    CSeq_entry_Handle seh = bsh.GetSeq_entry_Handle();
    CConstRef<CSeq_entry> ctx = seh.GetCompleteSeq_entry();

    bool has_barcode_tech = false;

    CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
    if (di && di->GetMolinfo().IsSetTech() && di->GetMolinfo().GetTech() == CMolInfo::eTech_barcode) {
        has_barcode_tech = true;
    }

    bool has_barcode_keyword = false;
    for (CSeqdesc_CI it(bsh, CSeqdesc::e_Genbank); it; ++it) {
        FOR_EACH_KEYWORD_ON_GENBANKBLOCK (k, it->GetGenbank()) {
            if (NStr::EqualNocase (*k, "BARCODE")) {
                has_barcode_keyword = true;
                break;
            }
        }
        if (has_barcode_keyword && !has_barcode_tech) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadKeywordNoTechnique,
                     "BARCODE keyword without Molinfo.tech barcode",
                     *ctx, *it);
        }
    }
    if (has_barcode_tech && !has_barcode_keyword && di) {
        PostErr (eDiag_Info, eErr_SEQ_DESCR_NoKeywordHasTechnique,
                 "Molinfo.tech barcode without BARCODE keyword",
                 *ctx, *di);
    }
    if (has_barcode_keyword && HasUnverified(bsh)) {
        PostErr(eDiag_Warning, eErr_SEQ_DESCR_BadKeywordUnverified,
            "Sequence has both BARCODE and UNVERIFIED keywords",
            seq);
    }
}


void CValidError_bioseq::ValidateInst(
    const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();


    // Check representation
    if ( !ValidateRepr(inst, seq) ) {
        return;
    }

    // Check molecule, topology, and strand
    if (!inst.IsSetMol()) {
        PostErr(eDiag_Error, eErr_SEQ_INST_MolNotSet, "Bioseq.mol is 0",
            seq);
    } else {
        const CSeq_inst::EMol& mol = inst.GetMol();
        switch (mol) {

            case CSeq_inst::eMol_na:
                PostErr(eDiag_Error, eErr_SEQ_INST_MolNuclAcid,
                         "Bioseq.mol is type nucleic acid", seq);
                break;

            case CSeq_inst::eMol_aa:
                if ( inst.IsSetTopology()  &&
                     inst.GetTopology() != CSeq_inst::eTopology_not_set  &&
                     inst.GetTopology() != CSeq_inst::eTopology_linear ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_CircularProtein,
                             "Non-linear topology set on protein", seq);
                }
                if ( inst.IsSetStrand()  &&
                     inst.GetStrand() != CSeq_inst::eStrand_ss &&
                     inst.GetStrand() != CSeq_inst::eStrand_not_set) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_BadProteinMoltype,
                             "Protein not single stranded", seq);
                }
                break;

            case CSeq_inst::eMol_dna:
                if (seq.IsSetInst() && seq.GetInst().IsSetTopology() && seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular) {
                    if (m_is_bact_or_arch) {
                        if (! m_is_plasmid && ! m_is_chromosome  && ! m_is_extrachrom) {
                            EDiagSev sev = eDiag_Error;
                            if (IsRefSeq(seq) || m_Imp.IsRefSeqConventions()) {
                                sev = eDiag_Error;
                            } else if (IsEmblOrDdbj(seq)) {
                                sev = eDiag_Warning;
                            }
                            PostErr(sev, eErr_SEQ_INST_CircBactGenomeProblem,
                                     "Circular Bacteria or Archaea should be chromosome, or plasmid, or extrachromosomal", seq);
                        }
                    }
                }
                break;

            case CSeq_inst::eMol_not_set:
                PostErr(eDiag_Error, eErr_SEQ_INST_MolNotSet, "Bioseq.mol is 0",
                    seq);
                break;

            case CSeq_inst::eMol_other:
                PostErr(eDiag_Error, eErr_SEQ_INST_MolinfoOther,
                         "Bioseq.mol is type other", seq);
                break;

            default:
                break;
        }
    }

    CSeq_inst::ERepr rp = seq.GetInst().GetRepr();

    if (rp == CSeq_inst::eRepr_raw  ||  rp == CSeq_inst::eRepr_const) {    
        // Validate raw and constructed sequences
        ValidateRawConst(seq);
    }

    // per VR-779
#if 1
    if (rp == CSeq_inst::eRepr_seg) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_ReprInvalid, "Segmented set format is not supported", seq);
    } else if (rp == CSeq_inst::eRepr_ref) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_ReprInvalid, "Repr_ref format is not supported", seq);
    }
#else
    if (rp == CSeq_inst::eRepr_seg  ||  rp == CSeq_inst::eRepr_ref) {
        // Validate segmented and reference sequences
        ValidateSegRef(seq);
    }
#endif

    if (rp == CSeq_inst::eRepr_delta) {
        // Validate delta sequences
        ValidateDelta(seq);
    }

    if (rp == CSeq_inst::eRepr_seg  &&  seq.GetInst().IsSetExt()  &&
        seq.GetInst().GetExt().IsSeg()) {
        // Validate part of segmented sequence
        ValidateSeqParts(seq);
    }

    if (rp == CSeq_inst::eRepr_raw || rp == CSeq_inst::eRepr_delta) {
        x_ValidateBarcode (seq);
    }
    
    x_ValidateTitle(seq);
    /*if ( seq.IsAa() ) {
         Validate protein title (amino acids only)
        ValidateProteinTitle(seq);
    }*/
    
    if ( seq.IsNa() ) {
        // check for N bases at start or stop of sequence,
        // or sequence entirely made of Ns
        ValidateNsAndGaps(seq);

    }

    // Validate sequence length
    ValidateSeqLen(seq);

    // proteins should not have gaps
    if (seq.IsAa() && x_HasGap(seq)) {
        PostErr(eDiag_Error, eErr_SEQ_INST_ProteinShouldNotHaveGaps, "Protein sequences should not have gaps", seq);
    }
}


bool CValidError_bioseq::x_ShowBioProjectWarning(const CBioseq& seq)

{
    bool is_wgs = false;
    bool is_grc = false;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    CSeqdesc_CI user(bsh, CSeqdesc::e_User);
    while (user) {
        if (user->GetUser().GetObjectType() == CUser_object::eObjectType_DBLink &&
            user->GetUser().HasField("BioProject", ".", NStr::eNocase)) {
            // bioproject field found
            return false;
        }
        ++user;
    }

    CSeqdesc_CI title(bsh, CSeqdesc::e_Title);
    while (title) {
        if (NStr::StartsWith(title->GetTitle(), "GRC")) {
            is_grc = true;
            break;
        }
        ++title;
    }

    is_wgs = IsWGS(bsh);

    bool is_gb = false, /* is_eb_db = false, */ is_refseq = false, is_ng = false;

    FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, seq) {
        const CSeq_id& sid = **sid_itr;
        switch (sid.Which()) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
                // is_eb_db = true;
                // fall through
            case CSeq_id::e_Ddbj:
                is_gb = true;
                break;
            case CSeq_id::e_Other:
                {
                    is_refseq = true;
                    if (sid.GetOther().IsSetAccession()) {
                        string acc = sid.GetOther().GetAccession().substr(0, 3);
                        if (acc == "NG_") {
                            is_ng = true;
                        }
                    }
                }
                break;
            default:
                break;
        }
    }

    if (is_refseq || m_Imp.IsRefSeqConventions()) {
        if (is_ng) return false;
    } else if (is_gb) {
        if (! is_wgs && ! is_grc) return false;
    } else {
        return false;
    }

    const CSeq_inst & inst = seq.GetInst();
    CSeq_inst::TRepr repr = inst.GetRepr();

    if (repr == CSeq_inst::eRepr_delta) {
        if (x_IsDeltaLitOnly(inst)) return false;
    } else if (repr != CSeq_inst::eRepr_map) {
        return false;
    }

    return true;
}

void CValidError_bioseq::ValidateBioseqContext(
    const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    // Check that proteins in nuc_prot set have a CdRegion
    if ( CdError(bsh) ) {
        EDiagSev sev = eDiag_Error;
        CBioseq_set_Handle bssh = GetNucProtSetParent (bsh);
        if (bssh) {
            CBioseq_Handle nbsh = GetNucBioseq (bssh);
            if (nbsh) {
                CSeqdesc_CI desc( nbsh, CSeqdesc::e_Molinfo );
                const CMolInfo* mi = desc ? &(desc->GetMolinfo()) : 0;
                if (mi) {
                    CMolInfo::TTech tech = mi->IsSetTech() ?
                        mi->GetTech() : CMolInfo::eTech_unknown;
                    if (tech == CMolInfo::eTech_wgs) {
                        sev = eDiag_Critical;
                    }
                }
            }
        }
        PostErr(sev, eErr_SEQ_PKG_NoCdRegionPtr,
            "No CdRegion in nuc-prot set points to this protein", 
            seq);
    }

    bool is_patent = SeqIsPatent (seq);

    bool is_complete = false;
    CSeqdesc_CI desc(bsh, CSeqdesc::e_Molinfo);
    if (desc) {
        const CMolInfo& mi = desc->GetMolinfo();
        if (mi.GetCompleteness() == CMolInfo::eCompleteness_complete) {
            is_complete = true;
        }
    }

    try {

        // if there are no Seq-ids, the following tests can't be run
        if (seq.IsSetId()) {

            ValidateSeqFeatContext(seq, is_complete);

            // Check for duplicate features and overlapping peptide features.
            ValidateDupOrOverlapFeats(seq);

            // Check for introns within introns.
            ValidateTwintrons(seq);

            // check for tRNA contained in tmRNA features
            x_ValidateOverlappingRNAFeatures (bsh);

            // check for equivalent source features
            x_ValidateSourceFeatures (bsh);

            // check for equivalen pub features
            x_ValidatePubFeatures (bsh);

            // Check for colliding genes
            ValidateCollidingGenes(seq);

            // Detect absence of BioProject DBLink for complete bacterial genomes
            ValidateCompleteGenome(seq);
        }

        m_dblink_count = 0;
        m_taa_count = 0;
        m_bs_count = 0;
        m_as_count = 0;
        m_pdb_count = 0;
        m_sra_count = 0;
        m_bp_count = 0;
        m_unknown_count = 0;

        // Validate descriptors that affect this bioseq
        ValidateSeqDescContext(seq);


        if (m_dblink_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_MultipleDBLinkObjects,
                NStr::IntToString(m_dblink_count) + " DBLink user objects apply to a Bioseq", seq);
        }

        if (m_taa_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "Trace Assembly Archive entries appear in " + NStr::IntToString(m_taa_count) + " DBLink user objects", seq);
        }

        if (m_bs_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "BioSample entries appear in " + NStr::IntToString(m_bs_count) + " DBLink user objects", seq);
        }

        if (m_as_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkBadAssembly,
                "Assembly entries appear in " + NStr::IntToString(m_as_count) + " DBLink user objects", seq);
        }

        if (m_pdb_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "ProbeDB entries appear in " + NStr::IntToString(m_pdb_count) + " DBLink user objects", seq);
        }

        if (m_sra_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "Sequence Read Archive entries appear in " + NStr::IntToString(m_sra_count) + " DBLink user objects", seq);
        }

        if (m_bp_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkProblem,
                "BioProject entries appear in " + NStr::IntToString(m_bp_count) + " DBLink user objects", seq);
        }

        if (m_unknown_count > 1) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkBadFormat,
                "Unrecognized entries appear in " + NStr::IntToString(m_unknown_count) + " DBLink user objects", seq);
        } else if (m_unknown_count > 0) {
            PostErr(eDiag_Critical, eErr_SEQ_DESCR_DBLinkBadFormat,
                "Unrecognized entries appear in " + NStr::IntToString(m_unknown_count) + " DBLink user object", seq);
        }

        // make sure that there is a pub on this bioseq
        if ( !m_Imp.IsNoPubs() ) {  
            CheckForPubOnBioseq(seq);
        }
        // make sure that there is a source on this bioseq
        if ( !m_Imp.IsNoBioSource() ) {
            CheckSourceDescriptor(bsh);
            //CheckForBiosourceOnBioseq(seq);
        }

        if (x_ShowBioProjectWarning (seq)) {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_ScaffoldLacksBioProject,
                "BioProject entries not present on CON record", seq);
        }

    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating BioseqContext. EXCEPTION: ") +
                e.what(), seq);
        }
    }

    if (!is_patent) {
        // flag missing molinfo even if not in Sequin
        CheckForMolinfoOnBioseq(seq);
    }

    CValidError_graph graph_validator(m_Imp);
    graph_validator.ValidateGraphsOnBioseq(seq);

    CheckTpaHistory(seq);
    
    // check for multiple publications with identical identifiers
    x_ValidateMultiplePubs(bsh);

    // look for orphaned proteins
    if (seq.IsAa() && bsh && !GetNucProtSetParent(bsh) && !AllowOrphanedProtein(seq, m_Imp.IsRefSeqConventions())) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_OrphanedProtein,
                "Orphaned stand-alone protein", seq);
    }
    
    // look for extra protein features
    if (seq.IsAa()) {
        CCacheImpl::SFeatKey prot_key(
            CCacheImpl::kAnyFeatType, CSeqFeatData::eSubtype_prot, bsh);
        const CCacheImpl::TFeatValue & prot_feats =
            GetCache().GetFeatFromCache(prot_key);

        if (prot_feats.size() > 1) {
            ITERATE(CCacheImpl::TFeatValue, feat, prot_feats) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_ExtraProteinFeature,
                        "Protein sequence has multiple unprocessed protein features", 
                        feat->GetOriginalFeature());
            }
        }
    }

    if (!m_Imp.IsNoCitSubPubs() && !x_HasCitSub(bsh) && !m_Imp.IsSeqSubmitParent()) {
        PostErr(m_Imp.IsGenomeSubmission() ? eDiag_Error : eDiag_Info, eErr_GENERIC_MissingPubRequirement,
                "Expected submission citation is missing for this Bioseq", seq);
    }

    //  RW-1053 check sig_peptides and mat_peptides with instantiated products
    if (seq.IsAa()) {

        SAnnotSelector sel(CSeqFeatData::eSubtype_mat_peptide_aa);
        sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_sig_peptide_aa);
        sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_transit_peptide_aa);
        try {
            for (CFeat_CI feat_ci(bsh, sel); feat_ci;  ++feat_ci) {

                const CSeq_feat& matpeptide = feat_ci->GetOriginalFeature();
                if (matpeptide.IsSetProduct()) {
                    const CSeq_loc& loc = matpeptide.GetLocation();
                    const CSeq_loc& prd = matpeptide.GetProduct();

                    int matlen = GetLength(loc, m_Scope);
                    int prdlen = GetLength(prd, m_Scope);
                    if (matlen != prdlen) {
                        PostErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA,
                                "Mat_peptide does not match length of instantiated product",
                                matpeptide);
                     }

                    CSeqVector mat_vec(loc, *m_Scope, CBioseq_Handle::eCoding_Iupac);
                    CSeqVector prd_vec(prd, *m_Scope, CBioseq_Handle::eCoding_Iupac);

                    int len = matlen;
                    if (len > prdlen) {
                        len = prdlen;
                    }

                    for (TSeqPos i = 0; i < len; ++i) {
                        CSeqVectorTypes::TResidue m_res = mat_vec[i];
                        CSeqVectorTypes::TResidue p_res = prd_vec[i];

                        if (m_res != p_res) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA,
                                    "Mismatch in mat_peptide (" + string(1, (char)m_res) + ") and instantiated product (" + \
                                     string(1, (char)p_res) + ") at position " + NStr::NumericToString(i + 1),
                                    matpeptide);
                       }
                    }
                }
            }

        }
        catch (CException&) {
        }
    }
}


bool CValidError_bioseq::x_HasCitSub(const CPub_equiv& pub)
{
    ITERATE(CPub_equiv::Tdata, it, pub.Get()) {
        if (x_HasCitSub(**it)) {
            return true;
        }
    }
    return false;
}


bool CValidError_bioseq::x_HasCitSub(const CPub& pub)
{
    if (pub.IsSub()) {
        return true;
    } else if (pub.IsEquiv() && x_HasCitSub(pub.GetEquiv())) {
        return true;
    } else {
        return false;
    }
}


bool CValidError_bioseq::x_HasCitSub(CBioseq_Handle bsh) const
{
    bool has_cit_sub = false;
    CSeqdesc_CI p(bsh, CSeqdesc::e_Pub);
    while (p && !has_cit_sub) {
        if (p->GetPub().IsSetPub()) {
            has_cit_sub = x_HasCitSub(p->GetPub().GetPub());
        }
        ++p;
    }

    return has_cit_sub;
}


template <class Iterator, class Predicate>
bool lists_match(Iterator iter1, Iterator iter1_stop, Iterator iter2, Iterator iter2_stop, Predicate pred)
{
    while (iter1 != iter1_stop && iter2 != iter2_stop) {
        if (!pred(*iter1, *iter2)) {
            return false;
        }
        ++iter1;
        ++iter2;
    }
    if (iter1 != iter1_stop || iter2 != iter2_stop) {
        return false;
    } else {
        return true;
    }
}


static bool s_OrgModEqual (
    const CRef<COrgMod>& om1,
    const CRef<COrgMod>& om2
)

{
    const COrgMod& omd1 = *(om1);
    const COrgMod& omd2 = *(om2);

    const string& str1 = omd1.GetSubname();
    const string& str2 = omd2.GetSubname();

    if (NStr::CompareNocase (str1, str2) != 0) return false;

    TORGMOD_SUBTYPE chs1 = omd1.GetSubtype();
    TORGMOD_SUBTYPE chs2 = omd2.GetSubtype();

    if (chs1 == chs2) return true;
    if (chs2 == NCBI_ORGMOD(other)) return true;

    return false;
}


bool s_DbtagEqual (const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2)
{
    // is dbt1 == dbt2
    return dbt1->Compare(*dbt2) == 0;
}


// Two OrgRefs are identical if the taxnames are identical, the dbxrefs are identical,
// and the orgname orgmod lists are identical
static bool s_OrgrefEquivalent (const COrg_ref& org1, const COrg_ref& org2)
{
    if ((org1.IsSetTaxname() && !org2.IsSetTaxname())
        || (!org1.IsSetTaxname() && org2.IsSetTaxname())
        || (org1.IsSetTaxname() && org2.IsSetTaxname() 
            && !NStr::EqualNocase (org1.GetTaxname(), org2.GetTaxname()))) {
        return false;
    }

    if ((org1.IsSetDb() && !org2.IsSetDb())
        || (!org1.IsSetDb() && org2.IsSetDb())
        || (org1.IsSetDb() && org2.IsSetDb() 
            && !lists_match (org1.GetDb().begin(), org1.GetDb().end(),
                             org2.GetDb().begin(), org2.GetDb().end(),
                             s_DbtagEqual))) {
        return false;
    }

    if ((org1.IsSetOrgname() && !org2.IsSetOrgname())
        || (!org1.IsSetOrgname() && org2.IsSetOrgname())) {
        return false;
    }
    if (org1.IsSetOrgname() && org2.IsSetOrgname()) {
        const COrgName& on1 = org1.GetOrgname();
        const COrgName& on2 = org2.GetOrgname();
        if ((on1.IsSetMod() && !on2.IsSetMod())
            || (!on1.IsSetMod() && on2.IsSetMod())
            || (on1.IsSetMod() && on2.IsSetMod()
                && !lists_match (on1.GetMod().begin(), on1.GetMod().end(),
                                 on2.GetMod().begin(), on2.GetMod().end(),
                                 s_OrgModEqual))) {
            return false;
        }
    }

    return true;
}


// Two SubSources are equal and duplicates if:
// they have the same subtype
// and the same name (or don't require a name).

static bool s_SubsourceEquivalent (
    const CRef<CSubSource>& st1,
    const CRef<CSubSource>& st2
)

{
    const CSubSource& sbs1 = *(st1);
    const CSubSource& sbs2 = *(st2);

    TSUBSOURCE_SUBTYPE chs1 = sbs1.GetSubtype();
    TSUBSOURCE_SUBTYPE chs2 = sbs2.GetSubtype();

    if (chs1 != chs2) return false;
    if (CSubSource::NeedsNoText(chs2)) return true;

    if (sbs1.IsSetName() && sbs2.IsSetName()) {
        if (NStr::CompareNocase (sbs1.GetName(), sbs2.GetName()) == 0) return true;
    }
    if (! sbs1.IsSetName() && ! sbs2.IsSetName()) return true;

    return false;
}


static bool s_BiosrcFullLengthIsOk (const CBioSource& src)
{
    if (src.IsSetIs_focus()) {
        return true;
    }
    FOR_EACH_SUBSOURCE_ON_BIOSOURCE (it, src) {
        if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == CSubSource::eSubtype_transgenic) {
            return true;
        }
    }
    return false;
}


static bool s_SuppressMultipleEquivBioSources (const CBioSource& src)
{
    if (!src.IsSetOrg() || !src.GetOrg().IsSetTaxname()) {
        return false;
    }
    if (NStr::EqualNocase(src.GetOrg().GetTaxname(), "unidentified phage")) {
        return true;
    }
    if (src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetLineage()
        && NStr::StartsWith(src.GetOrg().GetOrgname().GetLineage(), "Viruses", NStr::eNocase)) {
        return true;
    }
#if 0
    if (!src.GetOrg().IsSetOrgname()) {
      printf ("Orgname not set!\n");
    } else if (!src.GetOrg().GetOrgname().IsSetLineage()) {
      printf ("Lineage not set!\n");
    } else {
      printf ("Lineage is %s!\n", src.GetOrg().GetOrgname().GetLineage().c_str());
    }
#endif
    return false;
}


bool s_OverlapOrAbut(const CSeq_loc& loc1, const CSeq_loc& loc2, CScope* scope)
{
    TSeqPos start1 = loc1.GetStart(eExtreme_Positional);
    TSeqPos stop1 = loc1.GetStop(eExtreme_Positional);
    TSeqPos start2 = loc2.GetStart(eExtreme_Positional);
    TSeqPos stop2 = loc2.GetStop(eExtreme_Positional);

    if (start1 == stop2 + 1 || start2 == stop1 + 1) {
        // abut
        return true;
    } else if (TestForOverlapEx(loc1, loc2, eOverlap_Simple, scope) >= 0) {
        return true;
    } else {
        return false;
    }
}


bool s_ContainedIn(const CSeq_loc& loc1, const CSeq_loc& loc2, CScope* scope)
{
    TSeqPos start1 = loc1.GetStart(eExtreme_Positional);
    TSeqPos stop1 = loc1.GetStop(eExtreme_Positional);
    TSeqPos start2 = loc2.GetStart(eExtreme_Positional);
    TSeqPos stop2 = loc2.GetStop(eExtreme_Positional);

    if (start1 == stop2 + 1 || start2 == stop1 + 1) {
        // abut
        return false;
    } else if (TestForOverlapEx(loc1, loc2, eOverlap_Contained, scope) >= 0) {
        return true;
    } else {
        return false;
    }
}


bool s_CheckIntervals(const CSeq_loc& loc1, const CSeq_loc& loc2, CScope* scope)
{
    TSeqPos start1 = loc1.GetStart(eExtreme_Positional);
    TSeqPos stop1 = loc1.GetStop(eExtreme_Positional);
    TSeqPos start2 = loc2.GetStart(eExtreme_Positional);
    TSeqPos stop2 = loc2.GetStop(eExtreme_Positional);

    if (start1 == stop2 + 1 || start2 == stop1 + 1) {
        // abut
        return false;
    } else if (TestForOverlapEx(loc1, loc2, eOverlap_CheckIntervals, scope) >= 0) {
        return true;
    } else {
        return false;
    }
}


void CValidError_bioseq::x_ValidateOverlappingRNAFeatures(
    const CBioseq_Handle& bsh)
{
    // don't bother if can't build all feature iterator
    if (!m_AllFeatIt) {
        return;
    }
    try {
        CCacheImpl::SFeatKey rna_key(
            CSeqFeatData::e_Rna, CCacheImpl::kAnyFeatSubtype, bsh);
        const CCacheImpl::TFeatValue & rnas = GetCache().GetFeatFromCache(rna_key);
        CCacheImpl::TFeatValue::const_iterator feat = rnas.begin();
        if (feat != rnas.end()) {
 
            CCacheImpl::TFeatValue::const_iterator feat_prev = feat;
            ++feat;
            for ( ; feat != rnas.end(); ++feat_prev, ++feat) {

                if (!s_OverlapOrAbut(feat_prev->GetLocation(),
                    feat->GetLocation(), m_Scope)) {
                    continue;
                }

                const CRNA_ref& tm = feat_prev->GetData().GetRna();
                const CRNA_ref& tr = feat->GetData().GetRna();
                if ( tm.IsSetType() && tm.GetType() == CRNA_ref::eType_tmRNA ) {
                    if ( tr.IsSetType() && tr.GetType() == CRNA_ref::eType_tRNA ) {
                        if (s_ContainedIn(feat_prev->GetLocation(),
                            feat->GetLocation(), m_Scope)) {
                            PostErr (eDiag_Error, eErr_SEQ_FEAT_TRNAinsideTMRNA,
                                     "tRNA contained within tmRNA",
                                     feat->GetOriginalFeature());
                        }
                    }
                }
            }
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating RNA features. EXCEPTION: ") +
                e.what(), *(bsh.GetCompleteBioseq()));
        }
    }
}



void CValidError_bioseq::x_ValidateSourceFeatures(
    const CBioseq_Handle& bsh)
{
    // don't bother if can't build all feature iterator
    if (!m_AllFeatIt) {
        return;
    }
    try {
        CCacheImpl::SFeatKey biosrc_key(
            CSeqFeatData::e_Biosrc, CCacheImpl::kAnyFeatSubtype, bsh);
        const CCacheImpl::TFeatValue & biosrcs = GetCache().GetFeatFromCache(biosrc_key);
        CCacheImpl::TFeatValue::const_iterator feat = biosrcs.begin();
        if (feat != biosrcs.end()) {
            if (IsLocFullLength(feat->GetLocation(), bsh)) {
                CSeqdesc_CI di(bsh, CSeqdesc::e_Source);
                if (di) {
                    if (!s_BiosrcFullLengthIsOk(di->GetSource())) {
                                PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadFullLengthFeature,
                         "Source feature is full length, should be descriptor",
                                 feat->GetOriginalFeature());
                    }
                }
            }

            CCacheImpl::TFeatValue::const_iterator feat_prev = feat;
            ++feat;
            for ( ; feat != biosrcs.end(); ++feat_prev, ++feat) {
                if (IsLocFullLength(feat->GetLocation(), bsh)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadFullLengthFeature,
                             "Multiple full-length source features, should only be one if descriptor is transgenic",
                             feat->GetOriginalFeature());
                }

                if (!s_OverlapOrAbut(feat_prev->GetLocation(),
                    feat->GetLocation(), m_Scope)) {
                    // not close enough
                    continue;
                }

                // compare to see if feature sources are identical
                bool are_identical = true;
                if (feat_prev->IsSetComment() && feat->IsSetComment() 
                    && !NStr::EqualNocase (feat_prev->GetComment(), feat->GetComment())) {
                    are_identical = false;
                } else {
                    const CBioSource& src_prev = feat_prev->GetData().GetBiosrc();
                    const CBioSource& src = feat->GetData().GetBiosrc();
                    if ((src.IsSetIs_focus() && !src_prev.IsSetIs_focus())
                        || (!src.IsSetIs_focus() && src_prev.IsSetIs_focus())) {
                        are_identical = false;
                    } else if ((src.IsSetSubtype() && !src_prev.IsSetSubtype())
                               || (!src.IsSetSubtype() && src_prev.IsSetSubtype())
                               || (src.IsSetSubtype() && src_prev.IsSetSubtype() 
                                   && !lists_match (src.GetSubtype().begin(), src.GetSubtype().end(),
                                                    src_prev.GetSubtype().begin(), src_prev.GetSubtype().end(),
                                                    s_SubsourceEquivalent))) {
                        are_identical = false;
                    } else if ((src.IsSetOrg() && !src_prev.IsSetOrg())
                               || (!src.IsSetOrg() && src_prev.IsSetOrg())
                               || (src.IsSetOrg() && src_prev.IsSetOrg()
                                   && !s_OrgrefEquivalent (src.GetOrg(), src_prev.GetOrg()))) {
                        are_identical = false;
                    }                    
                }
                if (are_identical && !s_SuppressMultipleEquivBioSources(feat->GetData().GetBiosrc())) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleEquivBioSources, 
                             "Multiple equivalent source features should be combined into one multi-interval feature",
                             feat->GetOriginalFeature());
                }
            }
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating source features. EXCEPTION: ") +
                e.what(), *(bsh.GetCompleteBioseq()));
        }
    }

}


static void s_MakePubLabelString (const CPubdesc& pd, string& label)

{
    label = "";

    FOR_EACH_PUB_ON_PUBDESC (it, pd) {
        if ((*it)->IsGen() && (*it)->GetGen().IsSetCit()
            && !(*it)->GetGen().IsSetCit()
            && !(*it)->GetGen().IsSetJournal()
            && !(*it)->GetGen().IsSetDate()
            && (*it)->GetGen().IsSetSerial_number()) {
            // skip over just serial number
        } else {
            (*it)->GetLabel (&label, CPub::eContent, true);
            break;
        }
    }
}


void CValidError_bioseq::x_ValidatePubFeatures(
    const CBioseq_Handle& bsh)
{
    // don't bother if can't build feature iterator at all
    if (!m_AllFeatIt) {
        return;
    }
    try {
        CCacheImpl::SFeatKey pub_key(
            CSeqFeatData::e_Pub, CCacheImpl::kAnyFeatSubtype, bsh);
        const CCacheImpl::TFeatValue & pubs =
            GetCache().GetFeatFromCache(pub_key);
        CCacheImpl::TFeatValue::const_iterator feat = pubs.begin();
        if (feat != pubs.end()) {
            if (IsLocFullLength(feat->GetLocation(), bsh)) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadFullLengthFeature,
                         "Publication feature is full length, should be descriptor",
                         feat->GetOriginalFeature());
            }
     
            CCacheImpl::TFeatValue::const_iterator feat_prev = feat;
            string prev_label;
            if( feat_prev != pubs.end()) {
                s_MakePubLabelString(feat_prev->GetData().GetPub(), prev_label);
            ++feat;
            }
            for ( ; feat != pubs.end(); ++feat, ++feat_prev) {
                if (IsLocFullLength(feat->GetLocation(), bsh)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadFullLengthFeature,
                             "Publication feature is full length, should be descriptor",
                             feat->GetOriginalFeature());
                }
                // compare to see if feature sources are identical
                bool are_identical = true;
                if (feat_prev->IsSetComment() && feat->IsSetComment() 
                    && !NStr::EqualNocase (feat_prev->GetComment(), feat->GetComment())) {
                    are_identical = false;
                } else {
                    string label;
                    s_MakePubLabelString (feat->GetData().GetPub(), label);
                    if (!NStr::IsBlank (label) && !NStr::IsBlank(prev_label)
                        && !NStr::EqualNocase (label, prev_label)) {
                        are_identical = false;
                    }

                    // swap is faster than assignment
                    prev_label.swap(label);

                    // TODO: also check authors
                }

                if (are_identical) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleEquivPublications, 
                             "Multiple equivalent publication features should be combined into one multi-interval feature",
                             feat->GetOriginalFeature());
                }
            }
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating pub features. EXCEPTION: ") +
                e.what(), *(bsh.GetCompleteBioseq()));
        }
    }

}


struct SCaseInsensitiveQuickLess
{
public:
    // faster than lexicographical order
    bool operator()(const string& lhs, const string& rhs) const
    {
        if( lhs.length() != rhs.length() ) {
            return (lhs.length() < rhs.length());
        }
        return NStr::CompareNocase (lhs, rhs) < 0;
    }
};

struct SCaseInsensitiveLess
{
public:
    bool operator()(const string& lhs, const string& rhs) const
    {
        return NStr::CompareNocase (lhs, rhs) < 0;
    }
};


void CValidError_bioseq::x_ReportDuplicatePubLabels (
    const CBioseq& seq, const vector<CTempString>& labels)
{
    if (labels.size() <= 1) {
        // optimize fast case
        return;
    }
    if (m_Imp.IsRefSeqConventions() || m_Imp.IsRefSeq()) {
        return;
    }

    static const string kWarningPrefix =
        "Multiple equivalent publications annotated on this sequence [";
    static const string::size_type kMaxSummaryLen = 100;

    // TTempStringCount maps a CTempString to the number of times it appears
    // (Note case-insensitivity and non-lexicographical order)
    typedef map<CTempString, int, SCaseInsensitiveQuickLess> TLabelCount;
    TLabelCount label_count;

    ITERATE(vector<CTempString>, label_it, labels) {
        ++label_count[*label_it];
                }

    // put the dups into a vector and sort
    vector<CTempString> sorted_dup_labels;
    ITERATE(TLabelCount, label_count_it, label_count) {
        int num_appearances = label_count_it->second;
        _ASSERT(num_appearances > 0);
        if( num_appearances > 1 ) {
            const CTempString & dup_label = label_count_it->first;
            sorted_dup_labels.push_back(dup_label);
            }
            }
    sort(BEGIN_COMMA_END(sorted_dup_labels), SCaseInsensitiveLess());

    // find all that appear multiple times
    string err_msg = kWarningPrefix; // avoid create and destroy on each iter'n
    ITERATE(vector<CTempString>, dup_label_it, sorted_dup_labels) {
        const CTempString & summary = *dup_label_it;

        err_msg.resize(kWarningPrefix.length());
        if (summary.length() > kMaxSummaryLen) {
            err_msg += summary.substr(0, kMaxSummaryLen);
            err_msg += "...";
        } else {
            err_msg += summary;
        }
        err_msg += "]";
        PostErr (eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications,
                 err_msg, seq);
    }
}


void CValidError_bioseq::x_ValidateMultiplePubs(
    const CBioseq_Handle& bsh)
{
    // used to check for dups.  Currently only deals with cases where
    // there's an otherpub, but check if this comment is out of date.
    set<TEntrezId> muids_seen;
    set<TEntrezId> pmids_seen;

    vector<int> serials;
    vector<CTempString> published_labels;
    vector<CTempString> unpublished_labels;

    CConstRef<CSeq_entry> ctx =
        bsh.GetSeq_entry_Handle().GetCompleteSeq_entry();

    const CBioseq& seq = *(bsh.GetCompleteBioseq());

    for (CSeqdesc_CI it(bsh, CSeqdesc::e_Pub); it; ++it) {
        CConstRef<CPubdesc> pub = ConstRef(&it->GetPub());
        // first, try to receive from cache
        const CCacheImpl::CPubdescInfo & pubdesc_info =
            GetCache().GetPubdescToInfo(pub);
        // note that some (e.g. pmids are ignored other than maybe storing
        // in the cache above)
        copy(BEGIN_COMMA_END(pubdesc_info.m_published_labels),
             back_inserter(published_labels));
        copy(BEGIN_COMMA_END(pubdesc_info.m_unpublished_labels),
             back_inserter(unpublished_labels));

        TEntrezId muid = ZERO_ENTREZ_ID;
        TEntrezId pmid = ZERO_ENTREZ_ID;
        bool otherpub = false;
        FOR_EACH_PUB_ON_PUBDESC (pub_it, *pub) {
            switch ( (*pub_it)->Which() ) {
            case CPub::e_Muid:
                muid = (*pub_it)->GetMuid();
                break;
            case CPub::e_Pmid:
                pmid = (*pub_it)->GetPmid();
                break;
            default:
                otherpub = true;
                break;
            } 
        }
    
        if ( otherpub ) {
            bool collision = false;
            if ( muid > ZERO_ENTREZ_ID ) {
                if ( muids_seen.find(muid) != muids_seen.end() ) {
                    collision = true;
                } else {
                    muids_seen.insert(muid);
                }
            }
            if ( pmid > ZERO_ENTREZ_ID ) {
                if ( pmids_seen.find(pmid) != pmids_seen.end() ) {
                    collision = true;
                } else {
                    pmids_seen.insert(pmid);
                }
            }
            if ( collision && !m_Imp.IsRefSeqConventions() && !m_Imp.IsRefSeq() ) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPubMedID,
                    "Multiple publications with identical PubMed ID", *ctx, *it);
            }
        }
    }

    x_ReportDuplicatePubLabels (seq, unpublished_labels);
    x_ReportDuplicatePubLabels (seq, published_labels);

}


void CValidError_bioseq::ValidateHistory(const CBioseq& seq)
{
    if ( !seq.GetInst().IsSetHist() ) {
        return;
    }
    
    TGi gi = ZERO_GI;
    FOR_EACH_SEQID_ON_BIOSEQ (id, seq) {
        if ( (*id)->IsGi() ) {
            gi = (*id)->GetGi();
            break;
        }
    }
    if ( gi == ZERO_GI ) {
        return;
    }

    const CSeq_hist& hist = seq.GetInst().GetHist();
    if ( hist.IsSetReplaced_by() && hist.GetReplaced_by().IsSetDate() ) {
        const CSeq_hist_rec& rec = hist.GetReplaced_by();
        ITERATE( CSeq_hist_rec::TIds, id, rec.GetIds() ) {
            if ( (*id)->IsGi() ) {
                if ( gi == (*id)->GetGi() ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_HistoryGiCollision,
                        "Replaced by gi (" + 
                        NStr::NumericToString(gi) + ") is same as current Bioseq",
                        seq);
                    break;
                }
            }
        }
    }

    if ( hist.IsSetReplaces() && hist.GetReplaces().IsSetDate() ) {
        const CSeq_hist_rec& rec = hist.GetReplaces();
        ITERATE( CSeq_hist_rec::TIds, id, rec.GetIds() ) {
            if ( (*id)->IsGi() ) {
                if ( gi == (*id)->GetGi() ) {
                    PostErr(eDiag_Error, eErr_SEQ_INST_HistoryGiCollision,
                        "Replaces gi (" + 
                        NStr::NumericToString(gi) + ") is same as current Bioseq",
                        seq);
                    break;
                }
            }
        }
    }
}


// =============================================================================
//                                     Private
// =============================================================================




// Is the id contained in the bioseq?
bool CValidError_bioseq::IsIdIn(const CSeq_id& id, const CBioseq& seq)
{
    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        if (id.Match(**it)) {
            return true;
        }
    }
    return false;
}


size_t CValidError_bioseq::GetDataLen(const CSeq_inst& inst)
{
    if (!inst.IsSetSeq_data()) {
        return 0;
    }

    const CSeq_data& seqdata = inst.GetSeq_data();
    switch (seqdata.Which()) {
    case CSeq_data::e_not_set:
        return 0;
    case CSeq_data::e_Iupacna:
        return seqdata.GetIupacna().Get().size();
    case CSeq_data::e_Iupacaa:
        return seqdata.GetIupacaa().Get().size();
    case CSeq_data::e_Ncbi2na:
        return seqdata.GetNcbi2na().Get().size();
    case CSeq_data::e_Ncbi4na:
        return seqdata.GetNcbi4na().Get().size();
    case CSeq_data::e_Ncbi8na:
        return seqdata.GetNcbi8na().Get().size();
    case CSeq_data::e_Ncbipna:
        return seqdata.GetNcbipna().Get().size();
    case CSeq_data::e_Ncbi8aa:
        return seqdata.GetNcbi8aa().Get().size();
    case CSeq_data::e_Ncbieaa:
        return seqdata.GetNcbieaa().Get().size();
    case CSeq_data::e_Ncbipaa:
        return seqdata.GetNcbipaa().Get().size();
    case CSeq_data::e_Ncbistdaa:
        return seqdata.GetNcbistdaa().Get().size();
    default:
        return 0;
    }
}


// Returns true if seq derived from translation ending in "*" or
// seq is 3' partial (i.e. the right of the sequence is incomplete)
bool CValidError_bioseq::SuppressTrailingXMsg(const CBioseq& seq)
{

    // Look for the Cdregion feature used to create this aa product
    // Use the Cdregion to translate the associated na sequence
    // and check if translation has a '*' at the end. If it does.
    // message about 'X' at the end of this aa product sequence is suppressed
    try {
        const CSeq_feat* sfp = m_Imp.GetCDSGivenProduct(seq);
        if ( sfp ) {
        
            // Translate na CSeq_data
            string prot;   
            CSeqTranslator::Translate(*sfp, *m_Scope, prot); 
            
            if ( prot[prot.size() - 1] == '*' ) {
                return true;
            }
            return false;
        }

        // Get CMolInfo for seq and determine if completeness is
        // "eCompleteness_no_right or eCompleteness_no_ends. If so
        // suppress message about "X" at end of aa sequence is suppressed
        CTypeConstIterator<CMolInfo> mi = ConstBegin(seq);
        if (mi  &&  mi->IsSetCompleteness()) {
            if (mi->GetCompleteness() == CMolInfo::eCompleteness_no_right  ||
              mi->GetCompleteness() == CMolInfo::eCompleteness_no_ends) {
                return true;
            }
        }
    } catch (CException ) {
    } catch (std::exception ) {
    }
    return false;
}


//LCOV_EXCL_START
//part of segset validation, no longer used
CRef<CSeq_loc> CValidError_bioseq::GetLocFromSeq(const CBioseq& seq)
{
    CRef<CSeq_loc> loc;
    if (!seq.GetInst().IsSetExt()) {
        return loc;
    }

    if (seq.GetInst().GetExt().IsSeg()) {
        CRef<CSeq_loc> nloc(new CSeq_loc());
        loc = nloc;
        CSeq_loc_mix& mix = loc->SetMix();
        ITERATE (list< CRef<CSeq_loc> >, it,
            seq.GetInst().GetExt().GetSeg().Get()) {
            mix.Set().push_back(*it);
        }
    } else if (seq.GetInst().GetExt().IsRef()) {
        CRef<CSeq_loc> nloc(new CSeq_loc());
        loc = nloc;
        loc->Add(seq.GetInst().GetExt().GetRef());
    }
    return loc;
}
//LCOV_EXCL_STOP


// Check if CdRegion required but not found
bool CValidError_bioseq::CdError(const CBioseq_Handle& bsh)
{
    if ( bsh  &&  CSeq_inst::IsAa(bsh.GetInst_Mol()) ) {
        CSeq_entry_Handle nps = 
            bsh.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
        if ( nps ) {
            const CSeq_feat* cds = GetCDSForProduct(bsh);
            if ( cds == 0 ) {
                const CSeq_feat* mat = GetPROTForProduct(bsh);
                if ( mat == 0 ) {
                    return true;
                }
            }
        }
    }

    return false;
}


bool CValidError_bioseq::IsMrna(const CBioseq_Handle& bsh) 
{
    CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);

    if ( sd ) {
        const CMolInfo &mi = sd->GetMolinfo();
        if ( mi.IsSetBiomol() ) {
            return mi.GetBiomol() == CMolInfo::eBiomol_mRNA;
        }
    } else if (bsh.GetBioseqMolType() == CSeq_inst::eMol_rna) {
        // if no molinfo, assume rna is mrna
        return true;
    }

    return false;
}


size_t CValidError_bioseq::NumOfIntervals(const CSeq_loc& loc) 
{
    size_t counter = 0;
    for ( CSeq_loc_CI slit(loc); slit; ++slit ) {
        if ( !IsFarLocation(slit.GetEmbeddingSeq_loc(), m_Imp.GetTSEH()) ) {
            ++counter;
        }
    }
    return counter;
}


static bool s_NotPeptideException
(const CSeq_feat& curr,
 const CSeq_feat& prev)
{
    if (curr.IsSetExcept()  &&  curr.GetExcept()  &&  curr.IsSetExcept_text()) {
        if (NStr::FindNoCase(curr.GetExcept_text(), "alternative processing") != NPOS) {
            return false;
        }
    }
    if (prev.IsSetExcept()  &&  prev.GetExcept()  &&  prev.IsSetExcept_text()) {
        if (NStr::FindNoCase(prev.GetExcept_text(), "alternative processing") != NPOS) {
            return false;
        }
    }

    const CProt_ref& currP = curr.GetData().GetProt();
    const CProt_ref& prevP = prev.GetData().GetProt();

    if (currP.IsSetName() && prevP.IsSetName()) {
        string currN;
        for (auto it : currP.GetName()) {
            currN = it;
            break;
        }
        string prevN;
        for (auto it : prevP.GetName()) {
            prevN = it;
            break;
        }
        if (NStr::EqualNocase (currN, "anchored capsid protein ancC") && NStr::EqualNocase (prevN, "capsid protein C")) {
            return false;
        }
        if (NStr::EqualNocase (prevN, "anchored capsid protein ancC") && NStr::EqualNocase (currN, "capsid protein C")) {
            return false;
        }
        if (NStr::EqualNocase (currN, "membrane glycoprotein precursor prM") && NStr::EqualNocase (prevN, "protein pr")) {
            return false;
        }
        if (NStr::EqualNocase (prevN, "membrane glycoprotein precursor prM") && NStr::EqualNocase (currN, "protein pr")) {
            return false;
        }
        if (NStr::EqualNocase (currN, "membrane glycoprotein precursor prM") && NStr::EqualNocase (prevN, "membrane glycoprotein M")) {
            return false;
        }
        if (NStr::EqualNocase (prevN, "membrane glycoprotein precursor prM") && NStr::EqualNocase (currN, "membrane glycoprotein M")) {
            return false;
        }
        if (NStr::EqualNocase (currN, "anchored capsid protein C") && NStr::EqualNocase (prevN, "capsid protein C")) {
            return false;
        }
        if (NStr::EqualNocase (prevN, "anchored capsid protein C") && NStr::EqualNocase (currN, "capsid protein C")) {
            return false;
        }
        if (NStr::EqualNocase (currN, "membrane glycoprotein precursor M") && NStr::EqualNocase (prevN, "protein pr")) {
            return false;
        }
        if (NStr::EqualNocase (prevN, "membrane glycoprotein precursor M") && NStr::EqualNocase (currN, "protein pr")) {
            return false;
        }
        if (NStr::EqualNocase (currN, "membrane glycoprotein precursor M") && NStr::EqualNocase (prevN, "membrane glycoprotein M")) {
            return false;
        }
        if (NStr::EqualNocase (prevN, "membrane glycoprotein precursor M") && NStr::EqualNocase (currN, "membrane glycoprotein M")) {
            return false;
        }
    }


    return true;
}


#define FOR_EACH_SEQID_ON_BIOSEQ_HANDLE(Itr, Var) \
ITERATE (CBioseq_Handle::TId, Itr, Var.GetId())

bool CValidError_bioseq::IsWGSMaster(const CBioseq& seq, CScope& scope)
{
    if (!IsMaster(seq)) {
        return false;
    }
    CBioseq_Handle bsh = scope.GetBioseqHandle(seq);
    return IsWGS(bsh);
}


bool CValidError_bioseq::IsWGSMaster(const CSeq_entry& entry)
{
    bool rval = false;
    if (entry.IsSeq()) {
        if (IsMaster(entry.GetSeq()) && IsWGS(entry.GetSeq())) {
            rval = true;
        }
    } else if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {
        ITERATE(CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set()) {
            if (IsWGSMaster(**it)) {
                rval = true;
                break;
            }
        }
    }
    return rval;
}


bool CValidError_bioseq::IsWGS(const CBioseq& seq)
{
    if (!seq.IsSetDescr()) {
        return false;
    }
    ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get()) {
        if ((*it)->IsMolinfo() && (*it)->GetMolinfo().IsSetTech() && (*it)->GetMolinfo().GetTech() == CMolInfo::eTech_wgs) {
            return true;
        }
    }
    return false;
}


bool CValidError_bioseq::IsWGS(CBioseq_Handle bsh)
{
    CSeqdesc_CI molinfo(bsh, CSeqdesc::e_Molinfo);
    if (molinfo && molinfo->GetMolinfo().IsSetTech() && molinfo->GetMolinfo().GetTech() == CMolInfo::eTech_wgs) {
        return true;
    }
    return false;
}


bool CValidError_bioseq::IsWGS(const CSeq_entry& entry)
{
    bool rval = false;
    if (entry.IsSeq()) {
        rval = IsWGS(entry.GetSeq());
    } else if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {
        for (auto it : entry.GetSet().GetSeq_set()) {
            if (IsWGS(*it)) {
                rval = true;
                break;
            }
        }
    }
    return rval;
}


bool CValidError_bioseq::IsWGSAccession(const CSeq_id& id)
{
    const CTextseq_id* txt = id.GetTextseq_Id();
    if (txt == NULL || !txt->IsSetAccession()) {
        return false;
    }
    CSeq_id::EAccessionInfo info = CSeq_id::IdentifyAccession(txt->GetAccession());
    if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs) {
        return true;
    } else {
        return false;
    }
}


bool CValidError_bioseq::IsWGSAccession(const CBioseq& seq)
{
    if (!seq.IsSetId()) {
        return false;
    }
    ITERATE(CBioseq::TId, id, seq.GetId()) {
        if (IsWGSAccession(**id)) {
            return true;
        }
    }
    return false;
}


bool CValidError_bioseq::IsTSAAccession(const CSeq_id& id)
{
    const CTextseq_id* txt = id.GetTextseq_Id();
    if (txt == NULL || !txt->IsSetAccession()) {
        return false;
    }
    CSeq_id::EAccessionInfo info = CSeq_id::IdentifyAccession(txt->GetAccession());
    if ((info & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_tsa) {
        return true;
    } else {
        return false;
    }
}


bool CValidError_bioseq::IsTSAAccession(const CBioseq& seq)
{
    if (!seq.IsSetId()) {
        return false;
    }
    ITERATE(CBioseq::TId, id, seq.GetId()) {
        if (IsTSAAccession(**id)) {
            return true;
        }
    }
    return false;
}


bool CValidError_bioseq::IsPartial(const CBioseq& seq, CScope& scope)
{
    CBioseq_Handle bsh = scope.GetBioseqHandle(seq);
    CSeqdesc_CI desc(bsh, CSeqdesc::e_Molinfo);
    if (desc && desc->GetMolinfo().IsSetCompleteness()) {
        CMolInfo::TCompleteness completeness = desc->GetMolinfo().GetCompleteness();
        if (completeness == CMolInfo::eCompleteness_partial
            || completeness == CMolInfo::eCompleteness_no_left
            || completeness == CMolInfo::eCompleteness_no_right
            || completeness == CMolInfo::eCompleteness_no_ends) {
            return true;
        }
    }
    return false;
}


bool CValidError_bioseq::IsPdb(const CBioseq& seq)
{
    FOR_EACH_SEQID_ON_BIOSEQ(id, seq) {
        if ((*id)->IsPdb()) {
            return true;
        }
    }
    return false;
}


void CValidError_bioseq::ValidateSeqLen(const CBioseq& seq)
{
    if (IsPdb(seq) || IsWGSMaster(seq, *m_Scope)) {
        return;
    }
    const CSeq_inst& inst = seq.GetInst();

    TSeqPos len = inst.IsSetLength() ? inst.GetLength() : 0;
    if ( seq.IsAa() ) {
        if (len <= 3 && !IsPartial(seq, *m_Scope)) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residues", seq);
        }
    } else {
        if ( len <= 10 && m_report_short_seq) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residues", seq);
        }
    }
    
}


// Assumes that seq is segmented and has Seq-ext data
void CValidError_bioseq::ValidateSeqParts(const CBioseq& seq)
{
    // Get parent CSeq_entry of seq and then find the next
    // CSeq_entry in the set. This CSeq_entry should be a CBioseq_set
    // of class parts.
    const CSeq_entry* se = seq.GetParentEntry();
    if (!se) {
        return;
    }
    const CSeq_entry* parent = se->GetParentEntry ();
    if (!parent) {
        return;
    }
    if ( !parent->IsSet() || !parent->GetSet().IsSetClass() || parent->GetSet().GetClass() != CBioseq_set::eClass_segset) {
        return;
    }

    // Loop through seq_set looking for the parts set.
    FOR_EACH_SEQENTRY_ON_SEQSET (it, parent->GetSet()) {
        if ((*it)->Which() == CSeq_entry::e_Set
            && (*it)->GetSet().IsSetClass()
            && (*it)->GetSet().GetClass() == CBioseq_set::eClass_parts) {
            const CBioseq_set::TSeq_set& parts = (*it)->GetSet().GetSeq_set();
            const CSeg_ext::Tdata& locs = seq.GetInst().GetExt().GetSeg().Get();

            // Make sure the number of locations (excluding null locations)
            // match the number of parts
            size_t nulls = 0;
            ITERATE ( CSeg_ext::Tdata, loc, locs ) {
                if ( (*loc)->IsNull() ) {
                    nulls++;
                }
            }
            if ( locs.size() - nulls < parts.size() ) {
                 PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                    "Parts set contains too many Bioseqs", seq);
                 return;
            } else if ( locs.size() - nulls > parts.size() ) {
                PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                    "Parts set does not contain enough Bioseqs", seq);
                return;
            }

            // Now, simultaneously loop through the parts of se_parts and CSeq_locs of 
            // seq's CSseq-ext. If don't compare, post error.
            size_t size = locs.size();  // == parts.size()
            CSeg_ext::Tdata::const_iterator loc_it = locs.begin();
            CBioseq_set::TSeq_set::const_iterator part_it = parts.begin();
            for ( size_t i = 0; i < size; ++i ) {
                try {
                    if ( (*loc_it)->IsNull() ) {
                        ++loc_it;
                        continue;
                    }
                    if ( !(*part_it)->IsSeq() ) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                            "Parts set component is not Bioseq", seq);
                        return;
                    }
                    const CSeq_id& loc_id = GetId(**loc_it, m_Scope);
                    if ( !IsIdIn(loc_id, (*part_it)->GetSeq()) ) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                            "Segmented bioseq seq_ext does not correspond to parts "
                            "packaging order", seq);
                        return;
                    }
                    
                    // advance both iterators
                    ++part_it;
                    ++loc_it;
                } catch (const CObjmgrUtilException&) {
                    ERR_POST_X(4, "Seq-loc not for unique sequence");
                    return;
                } catch (CException &x1) {
                    string err_msg = "Unknown error:";
                    err_msg += x1.what();
                    ERR_POST_X(5, err_msg);
                    return;
                } catch (std::exception &x2) {
                    string err_msg = "Unknown error:";
                    err_msg += x2.what();
                    ERR_POST_X(5, err_msg);
                    return;
                }
            }
        }
    }
}

static bool s_IsConWithGaps(const CBioseq& seq)

{

    if (! seq.IsSetInst ()) return false;
    const CSeq_inst& inst = seq.GetInst();
    if (! inst.IsSetExt ()) return false;
    if (! inst.GetExt().IsDelta()) return false;

    ITERATE(CDelta_ext::Tdata, iter, inst.GetExt().GetDelta().Get()) {
        if (! (*iter)->IsLiteral() ) continue;
        const CSeq_literal& lit = (*iter)->GetLiteral();
        if (!lit.IsSetSeq_data()) return true;
        if  (lit.GetSeq_data().IsGap() && lit.GetLength() > 0) return true;
    }

    return false;
}


bool CValidError_bioseq::x_HasGap(const CBioseq& seq)
{
    bool has_gap = false;
    if (seq.GetInst().IsSetExt() && seq.GetInst().GetExt().IsDelta()) {
        ITERATE(CDelta_ext::Tdata, iter, seq.GetInst().GetExt().GetDelta().Get()) {
            if ((*iter)->IsLiteral() &&
                (!(*iter)->GetLiteral().IsSetSeq_data() || (*iter)->GetLiteral().GetSeq_data().IsGap())) {
                has_gap = true;
                break;
            }
        }
    }
    return has_gap;
}

void CValidError_bioseq::x_ValidateTitle(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if (!bsh) {
        return;
    }

    string title = sequence::CDeflineGenerator().GenerateDefline(bsh);

/*bsv
    CMolInfo::TTech tech = CMolInfo::eTech_unknown;
*/
    CSeqdesc_CI desc(bsh, CSeqdesc::e_Molinfo);
    if (desc) {
        const CMolInfo& mi = desc->GetMolinfo();
/*bsv
        tech = mi.GetTech();
*/
        if (mi.GetCompleteness() != CMolInfo::eCompleteness_complete) {
            if (m_Imp.IsGenbank()) {
                if (NStr::Find(title, "complete genome") != NPOS) {
                    const CSeq_entry& ctx = *seq.GetParentEntry();
                    PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteTitleProblem,
                        "Complete genome in title without complete flag set",
                        ctx, *desc);
                }
            }
            if (bsh.GetInst_Topology() == CSeq_inst::eTopology_circular &&
                (! s_IsConWithGaps (seq)) &&
                !m_Imp.IsEmbl() && !m_Imp.IsDdbj()) {
                const CSeq_entry& ctx = *seq.GetParentEntry();
                PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteCircleProblem,
                    "Circular topology without complete flag set", ctx, *desc);
            }
        }
    }

    // warning if title contains complete genome but sequence contains gap features
    if (NStr::FindNoCase (title, "complete genome") != NPOS && x_HasGap(seq)) {
        PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteGenomeHasGaps, 
                "Title contains 'complete genome' but sequence has gaps", seq);
    }


    // note - test for protein titles was moved to CValidError_bioseqset::ValidateNucProtSet
    // because it only applied for protein sequences in nuc-prot sets and it's more efficient
    // to create the defline generator once per nuc-prot set
}

static bool HasAssemblyOrNullGap (const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    if (inst.CanGetRepr() && inst.GetRepr() == CSeq_inst::eRepr_delta && inst.CanGetExt()  &&  inst.GetExt().IsDelta()) {
        ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
            if ( !(*sg) ) continue;
            if ((**sg).Which() != CDelta_seq::e_Literal) continue;
            const CSeq_literal& lit = (*sg)->GetLiteral();
            if (! lit.IsSetSeq_data()) return true;
            if (lit.GetSeq_data().IsGap()) return true;
        }
    }

    return false;
}


void CValidError_bioseq::ReportBadAssemblyGap (const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    if (inst.CanGetRepr() && inst.GetRepr() == CSeq_inst::eRepr_delta && inst.CanGetExt()  &&  inst.GetExt().IsDelta()) {
        ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
            if ( !(*sg) ) continue;
            if ((**sg).Which() != CDelta_seq::e_Literal) continue;
            const CSeq_literal& lit = (*sg)->GetLiteral();
            if (! lit.IsSetSeq_data()) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_SeqGapProblem, "TSA Seq_data NULL", seq);
            } else {
                const CSeq_data& data = lit.GetSeq_data();
                if (data.Which() == CSeq_data::e_Gap) {
                    const CSeq_gap& gap = data.GetGap();
                    if (gap.IsSetType()) {
                        int gaptype = gap.GetType();
                        if (gaptype == CSeq_gap::eType_unknown) {
                            PostErr(eDiag_Warning, eErr_SEQ_INST_TSAseqGapProblem, "TSA Seq_gap.unknown", seq);
                        } else if (gaptype == CSeq_gap::eType_other) {
                            PostErr(eDiag_Warning, eErr_SEQ_INST_TSAseqGapProblem, "TSA Seq_gap.other", seq);
                        }
                    } else {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_TSAseqGapProblem, "TSA Seq_gap NULL", seq);
                    }
                }
            }
        }
    }
}


bool CValidError_bioseq::HasBadWGSGap(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    if (inst.CanGetRepr() && inst.GetRepr() == CSeq_inst::eRepr_delta && inst.CanGetExt() && inst.GetExt().IsDelta()) {
        ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
            if (!(*sg)) continue;
             // CON division - far delta - suppresses errors
            if ((**sg).Which() != CDelta_seq::e_Literal) /* continue */ return false;
            const CSeq_literal& lit = (*sg)->GetLiteral();
            if (!lit.IsSetSeq_data()) {
                return true;
            } else {
                const CSeq_data& data = lit.GetSeq_data();
                if (data.Which() == CSeq_data::e_Gap) {
                    const CSeq_gap& gap = data.GetGap();
                    CSeq_gap::TType gap_type = gap.IsSetType() ? gap.GetType() : CSeq_gap::eType_unknown;

                    if (gap_type != CSeq_gap::eType_centromere && gap_type != CSeq_gap::eType_heterochromatin &&
                        gap_type != CSeq_gap::eType_short_arm && gap_type != CSeq_gap::eType_telomere &&
                        gap_type != CSeq_gap::eType_contig) {

                        if (!gap.IsSetLinkage_evidence() || gap.GetLinkage_evidence().empty()) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}


void CValidError_bioseq::ReportBadWGSGap(const CBioseq& seq)
{
    if (HasBadWGSGap(seq)) {
        PostErr(eDiag_Error, eErr_SEQ_INST_WGSseqGapProblem, 
            "WGS submission includes wrong gap type. Gaps for WGS genomes should be Assembly Gaps with linkage evidence.", seq);
    }
}


void CValidError_bioseq::ReportBadTSAGap(const CBioseq& seq)
{
    if (HasBadWGSGap(seq)) {
        PostErr(eDiag_Error, eErr_SEQ_INST_TSAseqGapProblem,
            "TSA submission includes wrong gap type. Gaps for TSA should be Assembly Gaps with linkage evidence.", seq);
    }
}


void CValidError_bioseq::ReportBadGenomeGap(const CBioseq& seq)
{
    if (HasBadWGSGap(seq)) {
        PostErr(eDiag_Error, eErr_SEQ_INST_SeqGapProblem,
            "Genome submission includes wrong gap type. Gaps for genomes should be Assembly Gaps with linkage evidence.", seq);
    }
}


bool s_FieldHasLabel(const CUser_field& field, const string& label)
{
    if (field.IsSetLabel() && field.GetLabel().IsStr() &&
        NStr::EqualNocase(field.GetLabel().GetStr(), label)) {
        return true;
    } else {
        return false;
    }
}


bool s_FieldHasNonBlankValue(const CUser_field& field)
{
    if (!field.IsSetData()) {
        return false;
    }
    bool rval = false;
    if (field.GetData().IsStr()) {
        if (!NStr::IsBlank(field.GetData().GetStr())) {
            rval = true;
        }
    } else if (field.GetData().IsStrs()) {
        ITERATE(CUser_field::TData::TStrs, s, field.GetData().GetStrs()) {
            if (!NStr::IsBlank(*s)) {
                rval = true;
                break;
            }
        }
    }
    return rval;
}


void CValidError_bioseq::ValidateWGSMaster(CBioseq_Handle bsh)
{
    bool has_biosample = false;
    bool has_bioproject = false;

    CSeqdesc_CI d(bsh, CSeqdesc::e_User);
    while (d) {
        if (d->GetUser().GetObjectType() == CUser_object::eObjectType_DBLink && d->GetUser().IsSetData()) {
            for (auto it: d->GetUser().GetData()) {
                if (s_FieldHasLabel(*it, "BioSample")) {
                    if (s_FieldHasNonBlankValue(*it)) {
                        has_biosample = true;
                    }
                } else if (s_FieldHasLabel(*it, "BioProject")) {
                    if (s_FieldHasNonBlankValue(*it)) {
                        has_bioproject = true;
                    }
                }
            }
        } else if (m_Imp.IsGenbank()) {
            const CUser_object& uo = d->GetUser();
            if (uo.GetType().IsStr()) {
                const string& type = uo.GetType().GetStr();
                if ( NStr::CompareNocase(type, "WGSProjects") == 0 ) {
                    int fr = 0;
                    int to = 0;

                    for (auto it: uo.GetData()) {
                        if ( !it->GetLabel().IsStr() ) {
                            continue;
                        }
                        const string& label = it->GetLabel().GetStr();
                        if ( NStr::CompareNocase(label, "WGS_accession_first") == 0  ||
                             NStr::CompareNocase(label, "Accession_first") == 0 ) {
                            const string& str = it->GetData().GetStr();
                            auto fst = str.find_first_of("0123456789");
                            fr = NStr::StringToInt (str.substr(fst));
                        } else if ( NStr::CompareNocase(label, "WGS_accession_last") == 0 ||
                                    NStr::CompareNocase(label, "Accession_last") == 0 ) {
                            const string& str = it->GetData().GetStr();
                            auto lst = str.find_first_of("0123456789");
                            to = NStr::StringToInt (str.substr(lst));
                        }
                    }
                    if ( (fr != 0)  &&  (to != 0) ) {
                        int df = to - fr + 1;
                        int blen = bsh.GetBioseqLength();
                        if (df != blen) {
                            PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentWGSFlags,
                                "Number of accessions (" + NStr::IntToString(df) + ") does not correspond to number of records (" + NStr::IntToString(blen) +")",
                                *(bsh.GetCompleteBioseq()));
                        }
                    }
                }
            }
        }
        ++d;
    }
    if (!has_biosample && !has_bioproject) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_WGSMasterLacksBothBioSampleBioProject, 
                "WGS master lacks both BioSample and BioProject", 
                *(bsh.GetCompleteBioseq()));
    } else if (!has_biosample) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_WGSmasterLacksBioSample,
            "WGS master lacks BioSample",
            *(bsh.GetCompleteBioseq()));
    } else if (!has_bioproject) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_WGSmasterLacksBioProject,
            "WGS master lacks BioProject",
            *(bsh.GetCompleteBioseq()));
    }
    if (!has_biosample || !has_bioproject) {
    }
}


static EDiagSev GetBioseqEndWarning (const CBioseq& seq, bool is_circular, EBioseqEndIsType end_is_char)
{
    EDiagSev sev;
    bool only_local = true;
    bool is_NCACNTNW = false;
    bool is_patent = false;
    FOR_EACH_SEQID_ON_BIOSEQ(id_it, seq) {
        if (!(*id_it)->IsLocal()) {
            only_local = false;
            if ((*id_it)->IsPatent()) {
                is_patent = true;
            } else if (IsNTNCNWACAccession(**id_it)) {
                is_NCACNTNW = true;
            }
        }
    }

    if (is_NCACNTNW || is_patent) {
        sev = eDiag_Warning;
    } else if (is_circular) {
        sev = eDiag_Warning;
    } else if (only_local) {
        sev = eDiag_Warning;
    } else if (end_is_char == eBioseqEndIsType_All) {
        sev = eDiag_Error;
    } else {
        sev = eDiag_Warning;
    }
    return sev;
}


void CValidError_bioseq::x_CalculateNsStretchAndTotal(const CBioseq& seq, TSeqPos& num_ns, TSeqPos& max_stretch, bool& n5, bool& n3)
{
    num_ns = 0;
    max_stretch = 0;
    n5 = false;
    n3 = false;

    if (HasAssemblyOrNullGap (seq)) return;
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( !bsh ) {
        return;
    }

    CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);                        

    TSeqPos this_stretch = 0; 
    for (TSeqPos i = 0; i < vec.size(); i++) {
        if (vec[i] == 'N') {
            num_ns++;
            if (vec.IsInGap(i)) {
                if (max_stretch < this_stretch) {
                    max_stretch = this_stretch;
                }
                this_stretch = 0;
            } else {
                this_stretch++;
                if (this_stretch >= 10) {
                    if (i < 20) {
                        n5 = true;
                    } 
                    if (vec.size() > 20 && i > vec.size() - 10) {
                        n3 = true;
                    }
                }
            }
        } else {
            if (max_stretch < this_stretch) {
                max_stretch = this_stretch;
            }
            this_stretch = 0;
        }
    }
    if (max_stretch < this_stretch) {
        max_stretch = this_stretch;
    }
}


bool CValidError_bioseq::GetTSANStretchErrors(const CBioseq& seq)
{
    TSeqPos num_ns = 0;
    TSeqPos max_stretch = 0;
    bool n5 = false;
    bool n3 = false;
    bool rval = false;

    x_CalculateNsStretchAndTotal(seq, num_ns, max_stretch, n5, n3);

    if (max_stretch >= 15) {
        PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentStretch, 
                    "Sequence has a stretch of " + NStr::IntToString(max_stretch) + " Ns", seq);
        rval = true;
    } else {
        if (n5) {
            PostErr (eDiag_Warning, eErr_SEQ_INST_HighNcontent5Prime, 
                        "Sequence has a stretch of at least 10 Ns within the first 20 bases", seq);
            rval = true;
        }
        if (n3) {
            PostErr (eDiag_Warning, eErr_SEQ_INST_HighNcontent3Prime, 
                        "Sequence has a stretch of at least 10 Ns within the last 20 bases", seq);
            rval = true;
        }
    }
    return rval;
}


// check to see if sequence is all Ns
bool CValidError_bioseq::IsAllNs(const CSeqVector& vec)
{
    bool rval = true;
    bool at_least_one = false;
    try {
        for (CSeqVector_CI sv_iter(vec); (sv_iter) && rval; ++sv_iter) {
            if (*sv_iter != 'N') {
                rval = false;
            }
            at_least_one = true;
        }
    } catch (CException& ) {

    }
    return (rval && at_least_one);
}


static int CountNs(const CSeq_data& seq_data, TSeqPos len)
{
    int total = 0;
    switch (seq_data.Which()) {
        case CSeq_data::e_Ncbi4na:
            {
                vector<char>::const_iterator it = seq_data.GetNcbi4na().Get().begin();
                unsigned char mask = 0xf0;
                unsigned char shift = 4;
                for (size_t n = 0; n < len; n++) {
                    unsigned char c = ((*it) & mask) >> shift;
                    mask >>= 4;
                    shift -= 4;
                    if (!mask) {
                        mask = 0xf0;
                        shift = 4;
                        ++it;
                    }
                    if (c == 15) {
                        total++;
                    }
                }
            }
            return total;
        case CSeq_data::e_Iupacna:
            {
                const string& s = seq_data.GetIupacna().Get();
                for (size_t n = 0; n < len && n < s.length(); n++) {
                    if (s[n] == 'N') {
                        total++;
                    }
                }
            }
            return total;
        case CSeq_data::e_Ncbi8na:
        case CSeq_data::e_Ncbipna:
            {
                CSeq_data iupacna;
                if (!CSeqportUtil::Convert(seq_data, &iupacna, CSeq_data::e_Iupacna)) {
                    return total;
                }
                const string& s = iupacna.GetIupacna().Get();
                for (size_t n = 0; n < len; n++) {
                    if (s[n] == 'N') {
                        total++;
                    }
                }
            }
            return total;
        default:
            return total;    
    }
}


int CValidError_bioseq::PctNs(CBioseq_Handle bsh)
{
    int count = 0;
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindData | CSeqMap::fFindGap | CSeqMap::fFindLeafRef);
    for (CSeqMap_CI seq_iter(bsh, sel); seq_iter; ++seq_iter) {
        switch (seq_iter.GetType()) {
            case CSeqMap::eSeqData:
                count += CountNs(seq_iter.GetData(), seq_iter.GetLength());
                break;
            default:
                break;
        }
    }
/*
    int pct_n = 0;
    try {
        CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        TSeqPos num_ns = 0;
        for (size_t i = 0; i < vec.size(); i++) {
            try {
                if (vec[i] == 'N' && !vec.IsInGap(i)) {
                    num_ns++;
                }
            } catch (CException& e2) {
                //bad character
            }
        }
        pct_n = (num_ns * 100) / bsh.GetBioseqLength();
    } catch (CException& e) {
        pct_n = 100;
    }
*/
    return bsh.GetBioseqLength() ? count * 100 / bsh.GetBioseqLength() : 100;
}


bool s_GetFlankingGapTypes(const CSeq_inst& inst, CSeq_gap::TType& fst, CSeq_gap::TType& lst)
{
    CSeq_gap::TType first = CSeq_gap::eType_unknown;
    CSeq_gap::TType last = CSeq_gap::eType_unknown;
    bool is_first = true;

    if ( inst.CanGetExt()  &&  inst.GetExt().IsDelta() ) {
        ITERATE(CDelta_ext::Tdata, iter, inst.GetExt().GetDelta().Get()) {
            if ( (*iter)->IsLoc() ) {
                return false;
            }
            if ( (*iter)->IsLiteral() ) {
                const CSeq_literal& lit = (*iter)->GetLiteral();
                if (lit.IsSetSeq_data() && lit.GetSeq_data().IsGap()) {
                    const CSeq_gap& gap = lit.GetSeq_data().GetGap();
                    int gaptype = 0;
                    if (gap.IsSetType()) {
                        gaptype = gap.GetType();
                    }
                    if (is_first) {
                        first = gaptype;
                    } else {
                        last = gaptype;
                    }
                } else {
                    last = 0;
                }
            }
            is_first = false;
        }
    }
    fst = first;
    lst = last;
    return true;
}

static bool s_WillReportTerminalGap(const CBioseq& seq, CBioseq_Handle bsh)
{
    if (!seq.IsSetInst() || !seq.GetInst().IsSetRepr()) {
        return false;
    }
    if (!seq.GetInst().IsSetMol() || seq.GetInst().GetMol() == CSeq_inst::eMol_aa) {
        return false;
    }
    CSeq_inst::TRepr repr = seq.GetInst().GetRepr();

    if (repr != CSeq_inst::eRepr_delta) {
        return false;
    }

    if ( !bsh ) {
        return false;
    }    

    if (!seq.GetInst().IsSetLength() || seq.GetInst().GetLength() < 10) {
        return false;
    }

    if (!ShouldCheckForNsAndGap(bsh)) {
        return false;
    }

    return true;
}


void CValidError_bioseq::ValidateNsAndGaps(const CBioseq& seq)
{
    if (!seq.IsSetInst() || !seq.GetInst().IsSetRepr()) {
        // can't check if no Inst or Repr
        return;
    }
    if (!seq.GetInst().IsSetMol() || seq.GetInst().GetMol() == CSeq_inst::eMol_aa) {
        // don't check proteins here
        return;
    }
    CSeq_inst::TRepr repr = seq.GetInst().GetRepr();

    // only check for raw or for delta sequences that are delta lit only
    if (repr == CSeq_inst::eRepr_virtual || repr == CSeq_inst::eRepr_map) {
        return;
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( !bsh ) {
        // no check if Bioseq not in scope
        return;
    }    

    try { 
        CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

        if (IsAllNs(vec)) {
            PostErr(m_Imp.IsPDB() ? eDiag_Warning : eDiag_Critical, eErr_SEQ_INST_AllNs, "Sequence is all Ns", seq);
            return;
        }

        // don't bother checking if length is less than 10
        if (!seq.IsSetInst() || !seq.GetInst().IsSetRepr() 
            || !seq.GetInst().IsSetLength() || seq.GetInst().GetLength() < 10) {
            return;
        }
                
        EBioseqEndIsType begin_n = eBioseqEndIsType_None;
        EBioseqEndIsType begin_gap = eBioseqEndIsType_None;
        EBioseqEndIsType end_n = eBioseqEndIsType_None;
        EBioseqEndIsType end_gap = eBioseqEndIsType_None;
        bool begin_ambig = false, end_ambig = false;
        CSeq_gap::TType fst = CSeq_gap::eType_unknown;
        CSeq_gap::TType lst = CSeq_gap::eType_unknown;
        if (ShouldCheckForNsAndGap(bsh) && x_IsDeltaLitOnly(seq.GetInst())) {
            CheckBioseqEndsForNAndGap(vec, begin_n, begin_gap, end_n, end_gap, begin_ambig, end_ambig);
            s_GetFlankingGapTypes(seq.GetInst(), fst, lst);
        }

        bool is_circular = false;
        if (bsh.IsSetInst_Topology() && bsh.GetInst_Topology() == CSeq_inst::eTopology_circular) {
            is_circular = true;
        }
        EDiagSev sev;
        if (begin_n != eBioseqEndIsType_None) {
            sev = GetBioseqEndWarning(seq, is_circular, begin_n);
            PostErr(sev, eErr_SEQ_INST_TerminalNs, "N at beginning of sequence", seq);
        } else if (begin_gap != eBioseqEndIsType_None && fst != CSeq_gap::eType_contamination) {
            sev = GetBioseqEndWarning(seq, is_circular, begin_gap);
            PostErr (sev, eErr_SEQ_INST_TerminalGap, "Gap at beginning of sequence", seq);
        }

        if (end_n != eBioseqEndIsType_None) {
            sev = GetBioseqEndWarning(seq, is_circular, end_n);
            PostErr(sev, eErr_SEQ_INST_TerminalNs, "N at end of sequence", seq);
        } else if (end_gap != eBioseqEndIsType_None &&  lst != CSeq_gap::eType_contamination) {
            sev = GetBioseqEndWarning(seq, is_circular, end_gap);
            PostErr (sev, eErr_SEQ_INST_TerminalGap, "Gap at end of sequence", seq);
        }

        if (begin_ambig && !s_WillReportTerminalGap(seq, bsh)) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_HighNpercent5Prime,
                "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases",
                seq);
        }
        if (end_ambig && !s_WillReportTerminalGap(seq, bsh)) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_HighNpercent3Prime,
                "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases",
                seq);
        }

        // don't check N content for patent sequences
        if (SeqIsPatent(seq)) {
            return;
        }
                        

        // if TSA, check for percentage of Ns and max stretch of Ns
        if (IsBioseqTSA(seq, m_Scope)) {
            ReportBadAssemblyGap (seq);
            bool n5 = false;
            bool n3 = false;
            TSeqPos num_ns = 0, max_stretch = 0;
            x_CalculateNsStretchAndTotal(seq, num_ns, max_stretch, n5, n3);

            int pct_n = (num_ns * 100) / seq.GetLength();
            if (pct_n > 10) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentPercent,
                            "Sequence contains " + NStr::IntToString(pct_n) + " percent Ns", seq);
            }

            if (max_stretch >= 15) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentStretch, 
                            "Sequence has a stretch of " + NStr::IntToString(max_stretch) + " Ns", seq);
            } else {
                if (n5) {
                    PostErr (eDiag_Warning, eErr_SEQ_INST_HighNcontent5Prime, 
                                "Sequence has a stretch of at least 10 Ns within the first 20 bases", seq);
                }
                if (n3) {
                    PostErr (eDiag_Warning, eErr_SEQ_INST_HighNcontent3Prime, 
                                "Sequence has a stretch of at least 10 Ns within the last 20 bases", seq);
                }
            }
        } else {
            // not TSA, just check for really high N percent
            int pct_n = PctNs(bsh);
            if (pct_n > 50) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_HighNContentPercent, 
                            "Sequence contains " + NStr::IntToString(pct_n) + " percent Ns", seq);
            }
        }

        if (!m_Imp.IsRefSeqConventions() && !IsRefSeq(seq) && !IsEmblOrDdbj(seq)) {
            if (IsWGS(bsh)) {
                ReportBadWGSGap(seq);
            } else if (IsBioseqTSA(seq, m_Scope)) {
                ReportBadTSAGap(seq);
            } else if (m_Imp.IsGenomeSubmission()) {
                ReportBadGenomeGap(seq);
            }
        }
    } catch ( exception& ) {
        // just ignore, and continue with the validation process.
    }
}


// Assumes that seq is eRepr_raw or eRepr_inst
void CValidError_bioseq::ValidateRawConst(
    const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    const string& rpr = tv->FindName(inst.GetRepr(), true);

    if (inst.IsSetFuzz() && (!inst.IsSetSeq_data() || !inst.GetSeq_data().IsGap())) {
        PostErr(eDiag_Error, eErr_SEQ_INST_FuzzyLen,
            "Fuzzy length on " + rpr + " Bioseq", seq);
    }

    if (!inst.IsSetLength()  ||  inst.GetLength() == 0) {
        string len = inst.IsSetLength() ?
            NStr::IntToString(inst.GetLength()) : "0";
        PostErr(eDiag_Error, eErr_SEQ_INST_InvalidLen,
                 "Invalid Bioseq length [" + len + "]", seq);
    }

    if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
        const CMolInfo* mi = 0;
        CSeqdesc_CI mi_desc(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_Molinfo);
        if ( mi_desc ) {
            mi = &(mi_desc->GetMolinfo());
        }
        CMolInfo::TTech tech = 
            mi != 0 ? mi->GetTech() : CMolInfo::eTech_unknown;
        if (tech == CMolInfo::eTech_htgs_2  &&
            !GraphsOnBioseq(seq)  &&
            !x_IsActiveFin(seq)) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_BadHTGSeq,
                "HTGS 2 raw seq has no gaps and no graphs", seq);
        }
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    CSeq_data::E_Choice seqtyp = inst.IsSetSeq_data() ?
        inst.GetSeq_data().Which() : CSeq_data::e_not_set;
    if (seqtyp != CSeq_data::e_Gap) {
        switch (seqtyp) {
        case CSeq_data::e_Iupacna:
        case CSeq_data::e_Ncbi2na:
        case CSeq_data::e_Ncbi4na:
        case CSeq_data::e_Ncbi8na:
        case CSeq_data::e_Ncbipna:
            if (inst.IsAa()) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                         "Using a nucleic acid alphabet on a protein sequence",
                         seq);
                return;
            }
            break;
        case CSeq_data::e_Iupacaa:
        case CSeq_data::e_Ncbi8aa:
        case CSeq_data::e_Ncbieaa:
        case CSeq_data::e_Ncbipaa:
        case CSeq_data::e_Ncbistdaa:
            if (inst.IsNa()) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                         "Using a protein alphabet on a nucleic acid",
                         seq);
                return;
            }
            break;
        case CSeq_data::e_Gap:
            break;
        default:
            PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                     "Sequence alphabet not set",
                     seq);
            return;
        }

        bool check_alphabet = false;
        unsigned int factor = 1;
        switch (seqtyp) {
        case CSeq_data::e_Iupacaa:
        case CSeq_data::e_Iupacna:
        case CSeq_data::e_Ncbieaa:
        case CSeq_data::e_Ncbistdaa:
            check_alphabet = true;
            break;
        case CSeq_data::e_Ncbi8na:
        case CSeq_data::e_Ncbi8aa:
            break;
        case CSeq_data::e_Ncbi4na:
            factor = 2;
            break;
        case CSeq_data::e_Ncbi2na:
            factor = 4;
            break;
        case CSeq_data::e_Ncbipna:
            factor = 5;
            break;
        case CSeq_data::e_Ncbipaa:
            factor = 21;
            break;
        default:
            // Logically, should not occur
            PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                     "Sequence alphabet not set",
                     seq);
            return;
        }
        TSeqPos calc_len = inst.IsSetLength() ? inst.GetLength() : 0;
        
        if (calc_len % factor) {
            calc_len += factor;
        }
        calc_len /= factor;

        string s_len = NStr::UIntToString(inst.GetLength());

        size_t data_len = GetDataLen(inst);
        string data_len_str = NStr::NumericToString(data_len * factor);
        if (calc_len > data_len) {
            PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                     "Bioseq.seq_data too short [" + data_len_str +
                     "] for given length [" + s_len + "]", seq);
            return;
        } else if (calc_len < data_len) {
            PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                     "Bioseq.seq_data is larger [" + data_len_str +
                     "] than given length [" + s_len + "]", seq);
        }

        if (check_alphabet) {
            unsigned int trailingX = 0;
            size_t dashes = 0;
            bool leading_x = false, found_lower = false;

            CConstRef<CSeqVector> sv = MakeSeqVectorForResidueCounting(bsh); 
            CSeqVector sv_res = bsh.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);

            size_t bad_cnt = 0;
            TSeqPos pos = 1;
            for ( CSeqVector_CI sv_iter(*sv), sv_res_iter(sv_res); (sv_iter) && (sv_res_iter); ++sv_iter, ++sv_res_iter ) {
                CSeqVector::TResidue res = *sv_iter;
                CSeqVector::TResidue n_res = *sv_res_iter;
                if ( !IsResidue(n_res) ) {
                    if (res == 'U' && bsh.IsSetInst_Mol() && bsh.GetInst_Mol() == CSeq_inst::eMol_rna) {
                        // U is ok for RNA
                    }
                    else if (res == '*' && bsh.IsAa()) {
                        trailingX = 0;
                    } else if (res == '-' && bsh.IsAa()) {
                        dashes++;
                        PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                                "Invalid residue [" + NStr::UIntToString(res) 
                                + "] at position [" + NStr::UIntToString(pos) + "]",
                                seq);
                    } else {
                        if ( ! IsResidue(res)) {
                            if ( ++bad_cnt > 10 ) {
                                PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                                    "More than 10 invalid residues. Checking stopped",
                                    seq);
                                return;
                            } else {
                                PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                                        "Invalid residue [" + NStr::UIntToString(res) 
                                        + "] at position [" + NStr::UIntToString(pos) + "]",
                                        seq);
                            }
                        } else if (islower (res)) {
                            found_lower = true;
                        } else {
                            string msg = "Invalid";
                            if (seq.IsNa() && strchr ("EFIJLOPQXZ", res) != NULL) {
                                msg += " nucleotide";
                            } else if (seq.IsNa() && res == 'U') {
                                msg += " nucleotide";
                            }
                            msg += " residue ";
                            if (seqtyp == CSeq_data::e_Ncbistdaa) {
                                msg += "[" + NStr::UIntToString(res) + "]";
                            } else {
                                msg += "'";
                                msg += res;
                                msg += "'";
                            }
                            msg += " at position [" + NStr::UIntToString(pos) + "]";

                            PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue, 
                                msg, seq);
                        }
                    }
                } else if ( res == '-' || sv->IsInGap(pos - 1) ) {
                    dashes++;
                } else if ( res == '*') {
                    trailingX = 0;
                } else if ( res == 'X' ) {
                    trailingX++;
                    if (pos == 1) {
                        leading_x = true;
                    }
                } else if (!isalpha (res)) {
                    string msg = "Invalid residue [";
                    msg += res;
                    msg += "] in position [" + NStr::UIntToString(pos) + "]";
                    PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue, 
                        msg, seq);
                } else {
                    trailingX = 0;
                }
                ++pos;
            }

            bool gap_at_start = HasBadProteinStart(*sv);
            size_t terminations = CountProteinStops(*sv);

            // only show leading or trailing X if product of NNN in nucleotide
            if (seq.IsAa() && (leading_x || trailingX > 0)) {
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle (seq);
                const CSeq_feat* cds = GetCDSForProduct(bsh);
                if (cds && cds->IsSetLocation()) {
                    size_t dna_len = GetLength (cds->GetLocation(), m_Scope);
                    if (dna_len > 5) {
                        string cds_seq = GetSequenceStringFromLoc (cds->GetLocation(), *m_Scope);
                        if (cds->GetData().GetCdregion().IsSetFrame()) {
                            if (cds->GetData().GetCdregion().GetFrame() == 2) {
                                cds_seq = cds_seq.substr(1);
                            } else if (cds->GetData().GetCdregion().GetFrame() == 3) {
                                cds_seq = cds_seq.substr(2);
                            }
                        }

                        if (!NStr::StartsWith (cds_seq, "NNN")) {
                            leading_x = false;
                        }
                        if (cds_seq.length() >= 3) {
                            string lastcodon = cds_seq.substr(cds_seq.length() - 3);
                            if (!NStr::StartsWith(lastcodon, "NNN")) {
                                trailingX = 0;
                            }
                        }
                    }
                }
            }

            if (leading_x) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_LeadingX, 
                         "Sequence starts with leading X", seq);
            }
                
            if ( trailingX > 0 && !SuppressTrailingXMsg(seq) ) {
                // Suppress if cds ends in "*" or 3' partial
                string msg = "Sequence ends in " +
                    NStr::IntToString(trailingX) + " trailing X";
                if ( trailingX > 1 ) {
                    msg += "s";
                }
                PostErr(eDiag_Warning, eErr_SEQ_INST_TrailingX, msg, seq);
            }

            if (found_lower) {
                PostErr (eDiag_Critical, eErr_SEQ_INST_InvalidResidue, 
                         "Sequence contains lower-case characters", seq);
            }

            if (terminations > 0 || dashes > 0) {
                // Post error indicating terminations found in protein sequence
                // if possible, get gene and protein names
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle (seq);
                // First get gene label
                string gene_label = "";
                try {
                    const CSeq_feat* cds = GetCDSForProduct(bsh);
                    if (cds) {
                        CConstRef<CSeq_feat> gene = m_Imp.GetCachedGene(cds);
                        if (gene && gene->IsSetData() && gene->GetData().IsGene()) {
                            gene->GetData().GetGene().GetLabel(&gene_label);
                        }
                    }
                } catch (...) {
                }
                // get protein label
                string protein_label = "";
                try {
                    CCacheImpl::SFeatKey prot_key(
                        CSeqFeatData::e_Prot, CCacheImpl::kAnyFeatSubtype, bsh);
                    const CCacheImpl::TFeatValue & prots =
                        GetCache().GetFeatFromCache(prot_key);
                    if( ! prots.empty() ) {
                        const CSeqFeatData_Base::TProt & first_prot =
                            prots[0].GetData().GetProt();
                        if( ! RAW_FIELD_IS_EMPTY_OR_UNSET(first_prot, Name) ) {
                            protein_label = first_prot.GetName().front();
                    }
                    }
                } catch (CException ) {
                } catch (std::exception ) {
                }

                if (NStr::IsBlank(gene_label)) {
                    gene_label = "gene?";
                }
                if (NStr::IsBlank(protein_label)) {
                    protein_label = "prot?";
                }

                if (dashes > 0) {
                    if (gap_at_start && dashes == 1) {
                      PostErr (eDiag_Error, eErr_SEQ_INST_BadProteinStart, 
                               "gap symbol at start of protein sequence (" + gene_label + " - " + protein_label + ")",
                               seq);
                    } else if (gap_at_start) {
                      PostErr (eDiag_Error, eErr_SEQ_INST_BadProteinStart, 
                               "gap symbol at start of protein sequence (" + gene_label + " - " + protein_label + ")",
                               seq);
                      PostErr (eDiag_Error, eErr_SEQ_INST_GapInProtein, 
                               "[" + NStr::SizetToString (dashes - 1) + "] internal gap symbols in protein sequence (" + gene_label + " - " + protein_label + ")",
                               seq);
                    } else {
                      PostErr (eDiag_Error, eErr_SEQ_INST_GapInProtein, 
                               "[" + NStr::SizetToString (dashes) + "] internal gap symbols in protein sequence (" + gene_label + " - " + protein_label + ")",
                               seq);
                    }
                }

                if (terminations > 0) {
                    string msg = "[" + NStr::SizetToString(terminations) + "] termination symbols in protein sequence";
                    msg += " (" + gene_label + " - " + protein_label + ")";
                    const CSeq_feat* cds = GetCDSForProduct(bsh);
                    if (cds) {
                        PostErr(eDiag_Error, eErr_SEQ_INST_StopInProtein, msg, *cds);
                    } else {
                        PostErr(eDiag_Error, eErr_SEQ_INST_StopInProtein, msg, seq);
                    }
                }
            }
        }

        bool is_wgs = IsWGS(bsh);

        if (seq.IsNa() && seq.GetInst().GetRepr() == CSeq_inst::eRepr_raw) {
            // look for runs of Ns and gap characters
            bool has_gap_char = false;
            size_t run_len = 0;
            TSeqPos start_pos = 0;
            TSeqPos pos = 1;
            CSeqVector sv = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            const size_t run_len_cutoff = ( is_wgs ? 20 : 100 );
            for ( CSeqVector_CI sv_iter(sv); (sv_iter); ++sv_iter, ++pos ) {
                CSeqVector::TResidue res = *sv_iter;
                switch(res) {
                case 'N':
                    if (run_len == 0) {
                        start_pos = pos;
                    }
                    run_len++;
                    break;
                case '-':
                    has_gap_char = true;
                    ///////////////////////////////////
                    ////////// FALL-THROUGH! //////////
                    ///////////////////////////////////
                default:
                    if (run_len >= run_len_cutoff && start_pos > 1)
                    {
                        PostErr (eDiag_Warning, eErr_SEQ_INST_InternalNsInSeqRaw, 
                                 "Run of " + NStr::SizetToString (run_len) + " Ns in raw sequence starting at base "
                                 + NStr::IntToString (start_pos),
                                 seq);
                    }
                    run_len = 0;
                    break;
                    }
                }
            if (has_gap_char) {
                PostErr (eDiag_Warning, eErr_SEQ_INST_InternalGapsInSeqRaw, 
                         "Raw nucleotide should not contain gap characters", seq);
            }
        }
    }
}


//LCOV_EXCL_START
//part of segset validation, no longer used
// Assumes seq is eRepr_seg or eRepr_ref
void CValidError_bioseq::ValidateSegRef(const CBioseq& seq)
{
    string id_test_label;
    seq.GetLabel(&id_test_label, CBioseq::eContent);

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    const CSeq_inst& inst = seq.GetInst();

    // Validate extension data -- wrap in CSeq_loc_mix for convenience
    CRef<CSeq_loc> loc = GetLocFromSeq(seq);
    if (loc) {
        if (inst.IsSetRepr() && inst.GetRepr() == CSeq_inst::eRepr_seg) {
            m_Imp.ValidateSeqLoc(*loc, bsh, true, "Segmented Bioseq", seq);
        }

        // Validate Length
        try {
            TSeqPos loclen = GetLength(*loc, m_Scope);
            TSeqPos seqlen = inst.IsSetLength() ? inst.GetLength() : 0;
            if (seqlen > loclen) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                    "Bioseq.seq_data too short [" + NStr::IntToString(loclen) +
                    "] for given length [" + NStr::IntToString(seqlen) + "]",
                    seq);
            } else if (seqlen < loclen) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                    "Bioseq.seq_data is larger [" + NStr::IntToString(loclen) +
                    "] than given length [" + NStr::IntToString(seqlen) + "]",
                    seq);
            }
        } catch (const CObjmgrUtilException&) {
            ERR_POST_X(6, Critical << "Unable to calculate length: ");
        }
    }

    // Check for multiple references to the same Bioseq
    if (inst.IsSetExt()  &&  inst.GetExt().IsSeg()) {
        const list< CRef<CSeq_loc> >& locs = inst.GetExt().GetSeg().Get();
        ITERATE(list< CRef<CSeq_loc> >, i1, locs) {
           if (!IsOneBioseq(**i1, m_Scope)) {
                continue;
            }
            const CSeq_id& id1 = GetId(**i1, m_Scope);
            list< CRef<CSeq_loc> >::const_iterator i2 = i1;
            for (++i2; i2 != locs.end(); ++i2) {
                if (!IsOneBioseq(**i2, m_Scope)) {
                    continue;
                }
                const CSeq_id& id2 = GetId(**i2, m_Scope);
                if (IsSameBioseq(id1, id2, m_Scope)) {
                    string sid;
                    id1.GetLabel(&sid);
                    if ((**i1).IsWhole()  &&  (**i2).IsWhole()) {
                        PostErr(eDiag_Error,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid, seq);
                    } else {
                        PostErr(eDiag_Warning,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid + " that are not SEQLOC_WHOLE", seq);
                    }
                }
            }
        }
    }

    // Check that partial sequence info on sequence segments is consistent with
    // partial sequence info on sequence -- aa  sequences only
    int partial = SeqLocPartialCheck(*loc, m_Scope);
    if (seq.IsAa()) {
        bool got_partial = false;
        FOR_EACH_DESCRIPTOR_ON_BIOSEQ (sd, seq) {
            if (!(*sd)->IsMolinfo() || !(*sd)->GetMolinfo().IsSetCompleteness()) {
                continue;
            }
           
            switch ((*sd)->GetMolinfo().GetCompleteness()) {
                case CMolInfo::eCompleteness_partial:
                    got_partial = true;
                    if (!partial) {
                        PostErr (eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                                 "Complete segmented sequence with MolInfo partial", seq);
                    }
                    break;
                case CMolInfo::eCompleteness_no_left:
                    if (!(partial & eSeqlocPartial_Start) || (partial & eSeqlocPartial_Stop)) {
                        PostErr (eDiag_Error, eErr_SEQ_INST_PartialInconsistent, 
                                 "No-left inconsistent with segmented SeqLoc",
                                 seq);
                    }
                    got_partial = true;
                    break;
                case CMolInfo::eCompleteness_no_right:
                    if (!(partial & eSeqlocPartial_Stop) || (partial & eSeqlocPartial_Start)) {
                        PostErr (eDiag_Error, eErr_SEQ_INST_PartialInconsistent, 
                                 "No-right inconsistent with segmented SeqLoc",
                                 seq);
                    }
                    got_partial = true;
                    break;
                case CMolInfo::eCompleteness_no_ends:
                    if (!(partial & eSeqlocPartial_Start) || !(partial & eSeqlocPartial_Stop)) {
                        PostErr (eDiag_Error, eErr_SEQ_INST_PartialInconsistent, 
                                 "No-ends inconsistent with segmented SeqLoc",
                                 seq);
                    }
                    got_partial = true;
                    break;
                default:
                    break;
            }
        }
        if (!got_partial) {
            PostErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                "Partial segmented sequence without MolInfo partial", seq);
        }
    }
}
//LCOV_EXCL_STOP


static int s_MaxNsInSeqLitForTech (CMolInfo::TTech tech)
{
    int max_ns = -1;

    switch (tech) {
        case CMolInfo::eTech_htgs_1:
        case CMolInfo::eTech_htgs_2:
        case CMolInfo::eTech_composite_wgs_htgs:
            max_ns = 80;
            break;
        case CMolInfo::eTech_wgs:
            max_ns = 19;
            break;
        default:
            max_ns = 99;
            break;
    }
    return max_ns;
}


static bool s_IsSwissProt (const CBioseq& seq)
{
    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        if ((*it)->IsSwissprot()) {
            return true;
        }
    }
    return false;
}

static bool s_LocSortCompare (const CConstRef<CSeq_loc>& q1, const CConstRef<CSeq_loc>& q2)
{
    TIntId cmp = q1->GetId()->CompareOrdered(*(q2->GetId()));
    if (cmp < 0) {
        return true;
    } else if (cmp > 0) {
        return false;
    }

    TSeqPos start1 = q1->GetStart(eExtreme_Positional);
    TSeqPos start2 = q2->GetStart(eExtreme_Positional);
    if (start1 < start2) {
        return true;
    } else if (start2 < start1) {
        return false;
    }
        
    TSeqPos stop1 = q1->GetStop(eExtreme_Positional);
    TSeqPos stop2 = q2->GetStop(eExtreme_Positional);

    if (stop1 < stop2) {
        return true;
    } else {
        return false;
    }
}


bool CValidError_bioseq::IsSelfReferential(const CBioseq& seq)
{
    bool rval = false;

    if (!seq.IsSetInst() || !seq.GetInst().IsSetExt() ||
        !seq.GetInst().GetExt().IsDelta()) {
        return false;
    }

    ITERATE(CDelta_ext::Tdata, sg, seq.GetInst().GetExt().GetDelta().Get()) {
        if (!(*sg)) {
            // skip NULL element
        } else if ((*sg)->IsLoc()) {
            const CSeq_id *id = (*sg)->GetLoc().GetId();
            if (id) {
                FOR_EACH_SEQID_ON_BIOSEQ(id_it, seq) {
                    if ((*id_it)->Compare(*id) == CSeq_id::e_YES) {
                        rval = true;
                        break;
                    }
                }
            }
            if (rval) break;
        }        
    }
    return rval;
}


bool HasExcludedAnnotation(const CSeq_loc& loc, CBioseq_Handle far_bsh)
{
    if (!loc.IsInt()) {
        return false;
    }

    TSeqPos stop = loc.GetStop(eExtreme_Positional);
    TSeqPos start = loc.GetStart(eExtreme_Positional);

    if (start > 1) {
        CRef<CSeq_loc> far_loc(new CSeq_loc());
        far_loc->SetInt().SetFrom(start - 2);
        far_loc->SetInt().SetTo(start - 1);
        far_loc->SetInt().SetId().Assign(loc.GetInt().GetId());
        CFeat_CI f(far_bsh.GetScope(), *far_loc);
        if (f) {
            return true;
        }
    }
    if (stop < far_bsh.GetBioseqLength() - 2) {
        CRef<CSeq_loc> far_loc(new CSeq_loc());
        far_loc->SetInt().SetFrom(stop + 1);
        far_loc->SetInt().SetTo(stop + 2);
        far_loc->SetInt().SetId().Assign(loc.GetInt().GetId());
        CFeat_CI f(far_bsh.GetScope(), *far_loc);
        if (f) {
            return true;
        }
    }
    return false;
}


void CValidError_bioseq::ValidateDeltaLoc
(const CSeq_loc& loc,
 const CBioseq& seq, 
 TSeqPos& len)
{
    if (loc.IsWhole()) {
        PostErr (eDiag_Error, eErr_SEQ_INST_WholeComponent, 
                 "Delta seq component should not be of type whole", seq);
    }

    const CSeq_id *id = loc.GetId();
    if (id) {
        if (id->IsGi() && loc.GetId()->GetGi() == ZERO_GI) {
            PostErr (eDiag_Critical, eErr_SEQ_INST_DeltaComponentIsGi0, 
                     "Delta component is gi|0", seq);
        }
        if (!loc.IsWhole()
            && (id->IsGi() 
                || id->IsGenbank() 
                || id->IsEmbl()
                || id->IsDdbj() || id->IsTpg()
                || id->IsTpe()
                || id->IsTpd()
                || id->IsOther())) {
            TSeqPos stop = loc.GetStop(eExtreme_Positional);
            try {
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id, CScope::eGetBioseq_All);
                if (bsh) {
                    TSeqPos seq_len = bsh.GetBioseqLength(); 
                    if (seq_len <= stop) {
                        string id_label = id->AsFastaString();
                        PostErr (eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                                 "Seq-loc extent (" + NStr::IntToString (stop + 1) 
                                 + ") greater than length of " + id_label
                                 + " (" + NStr::IntToString(seq_len) + ")",
                                seq);
                    }
                    if (!m_Imp.IsRefSeq() && IsWGS(seq) && HasExcludedAnnotation(loc, bsh)) {
                        string id_label = id->AsFastaString();
                        PostErr(eDiag_Error, eErr_SEQ_INST_FarLocationExcludesFeatures,
                                "Scaffold points to some but not all of " +
                                id_label + ", excluded portion contains features", seq);
                    }
                } else {
                    PostErr(eDiag_Error, eErr_GENERIC_DeltaSeqError,
                        "Unable to find far delta sequence component", seq);
                }
            } catch (CException ) {
            } catch (std::exception ) {
            }
        }
    }

    try {
        if (seq.IsSetInst ()) {
            const CSeq_inst& inst = seq.GetInst();
            TSeqPos loc_len = GetLength(loc, m_Scope);
            if (loc_len == numeric_limits<TSeqPos>::max()) {
                PostErr (eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
                         "-1 length on seq-loc of delta seq_ext", seq);
                string loc_str;
                loc.GetLabel(&loc_str);
                if ( loc_str.empty() ) {
                    loc_str = "?";
                }
                if (x_IsDeltaLitOnly(inst)) {
                    PostErr(eDiag_Warning, eErr_SEQ_INST_SeqLocLength,
                        "Short length (-1) on seq-loc (" + loc_str + ") of delta seq_ext", seq);
                }
            } else {
                len += loc_len;
            }
            if ( loc_len <= 10 ) {
                string loc_str;
                loc.GetLabel(&loc_str);
                if ( loc_str.empty() ) {
                    loc_str = "?";
                }
                if (x_IsDeltaLitOnly(inst)) {
                    PostErr(eDiag_Warning, eErr_SEQ_INST_SeqLocLength,
                        "Short length (" + NStr::SizetToString(loc_len) + 
                        ") on seq-loc (" + loc_str + ") of delta seq_ext", seq);
                }
            }
        }

    } catch (const CObjmgrUtilException&) {
        string loc_str;
        loc.GetLabel(&loc_str);
        if ( loc_str.empty() ) {
            loc_str = "?";
        }
        PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
            "No length for Seq-loc (" + loc_str + ") of delta seq-ext",
            seq);
    }
}


static size_t s_GetDeltaLen (const CDelta_seq& seg, CScope* scope)
{
    if (seg.IsLiteral()) {
        return seg.GetLiteral().GetLength();
    } else if (seg.IsLoc()) {
        return GetLength (seg.GetLoc(), scope);
    } else {
        return 0;
    }
}


static string linkEvStrings [] = {
    "paired-ends",
    "align genus",
    "align xgenus",
    "align trnscpt",
    "within clone",
    "clone contig",
    "map",
    "strobe",
    "unspecified",
    "pcr",
    "proximity ligation",
    "other",
    "UNKNOWN VALUE"
};

/*bsv
static bool s_IsGapComponent (const CDelta_seq& seg)
{
    if (! seg.IsLiteral()) return false;
    const CSeq_literal& lit = seg.GetLiteral();
    if (! lit.IsSetSeq_data()) return true;
    if  (lit.GetSeq_data().IsGap() && lit.GetLength() > 0) return true;
    return false;
}
*/

static bool s_IsUnspecified(const CSeq_gap& gap)
{
    bool is_unspec = false;
    ITERATE(CSeq_gap::TLinkage_evidence, ev_itr, gap.GetLinkage_evidence()) {
        const CLinkage_evidence & evidence = **ev_itr;
        if (!evidence.CanGetType()) continue;
        int linktype = evidence.GetType();
        if (linktype == 8) {
            is_unspec = true;
        }
    }
    return is_unspec;
}


bool CValidError_bioseq::x_IgnoreEndGap(CBioseq_Handle bsh, CSeq_gap::TType gap_type)
{
    // always ignore for circular sequences
    if (bsh.GetInst().IsSetTopology() &&
        bsh.GetInst().GetTopology() == CSeq_inst::eTopology_circular) {
        return true;
    }

    // ignore if location is genomic and gap is of certain type
    if (gap_type != CSeq_gap::eType_centromere &&
        gap_type != CSeq_gap::eType_telomere &&
        gap_type != CSeq_gap::eType_heterochromatin &&
        gap_type != CSeq_gap::eType_short_arm &&
        gap_type != CSeq_gap::eType_contamination) {
        return false;
    }

    CSeqdesc_CI src(bsh, CSeqdesc::e_Source);
    if (src && src->GetSource().IsSetGenome() && src->GetSource().GetGenome() == CBioSource::eGenome_chromosome) {
        return true;
    } else {
        return false;
    }
}


// Assumes seq is a delta sequence
void CValidError_bioseq::ValidateDelta(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    // Get CMolInfo and tech used for validating technique and gap positioning
    const CMolInfo* mi = 0;
    CSeqdesc_CI mi_desc(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_Molinfo);
    if ( mi_desc ) {
        mi = &(mi_desc->GetMolinfo());
    }
    CMolInfo::TTech tech = 
        mi != 0 ? mi->GetTech() : CMolInfo::eTech_unknown;


    if (!inst.IsSetExt()  ||  !inst.GetExt().IsDelta()  ||
        inst.GetExt().GetDelta().Get().empty()) {
        PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
            "No CDelta_ext data for delta Bioseq", seq);
    }

    bool any_tech_ok = false;
    bool has_gi = false;
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
        if (IsNTNCNWACAccession(**id_it)) {
            any_tech_ok = true;
            break;
        } else if ((*id_it)->IsGi()) {
            has_gi = true;
        }
    }
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if (!any_tech_ok && seq.IsNa()
        && tech != CMolInfo::eTech_htgs_0 && tech != CMolInfo::eTech_htgs_1 
        && tech != CMolInfo::eTech_htgs_2 && tech != CMolInfo::eTech_htgs_3 
        && tech != CMolInfo::eTech_wgs && tech != CMolInfo::eTech_composite_wgs_htgs 
        && tech != CMolInfo::eTech_unknown && tech != CMolInfo::eTech_standard
        && tech != CMolInfo::eTech_htc && tech != CMolInfo::eTech_barcode 
        && tech != CMolInfo::eTech_tsa) {
        PostErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq, 
            "Delta seq technique should not be [" + NStr::IntToString(tech) + "]", seq);
    }

   // set severity for first / last gap error
    TSeqPos len = 0;
    TSeqPos seg = 0;
    bool last_is_gap = false;
    int prev_gap_linkage = -1;
    CSeq_gap::TType prev_gap_type = CSeq_gap::eType_unknown;
    int gap_linkage = -1;
    CSeq_gap::TType gap_type = CSeq_gap::eType_unknown;
    size_t num_gaps = 0;
    size_t num_adjacent_gaps = 0;
    bool non_interspersed_gaps = false;
    bool first = true;
    int num_gap_known_or_spec = 0;
    int num_gap_unknown_unspec = 0;

    vector<CConstRef<CSeq_loc> > delta_locs;

    ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
        ++seg;
        if ( !(*sg) ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
                "NULL pointer in delta seq_ext valnode (segment " +
                NStr::IntToString(seg) + ")", seq);
            continue;
        }
        switch ( (**sg).Which() ) {
        case CDelta_seq::e_Loc:
        {
            const CSeq_loc& loc = (**sg).GetLoc(); 
            CConstRef<CSeq_loc> tmp(&loc);
            delta_locs.push_back (tmp);
            
            ValidateDeltaLoc (loc, seq, len);

            if ( !last_is_gap && !first) {
                non_interspersed_gaps = true;
            }
            last_is_gap = false;
            prev_gap_linkage = -1;
            prev_gap_type = CSeq_gap::eType_unknown;
            gap_linkage = CSeq_gap::eType_unknown;
            first = false;
            break;
        }
        case CDelta_seq::e_Literal:
        {
            // The C toolkit code checks for valid alphabet here
            // The C++ object serializaton will not load if invalid alphabet
            // so no check needed here
            const CSeq_literal& lit = (*sg)->GetLiteral();
            TSeqPos start_len = len;
            len += lit.CanGetLength() ? lit.GetLength() : 0;
            if (lit.IsSetSeq_data() && ! lit.GetSeq_data().IsGap()
                && (!lit.IsSetLength() || lit.GetLength() == 0)) {
                PostErr (eDiag_Error, eErr_SEQ_INST_SeqLitDataLength0, 
                         "Seq-lit of length 0 in delta chain", seq);
            }

            // Check for invalid residues
            if ( lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap() ) {
                if ( !last_is_gap && !first) {
                    non_interspersed_gaps = true;
                }
                last_is_gap = false;
                prev_gap_linkage = -1;
                prev_gap_type = CSeq_gap::eType_unknown;
                const CSeq_data& data = lit.GetSeq_data();
                vector<TSeqPos> badIdx;
                CSeqportUtil::Validate(data, &badIdx);
                const string* ss = 0;
                switch (data.Which()) {
                case CSeq_data::e_Iupacaa:
                    ss = &data.GetIupacaa().Get();
                    break;
                case CSeq_data::e_Iupacna:
                    ss = &data.GetIupacna().Get();
                    break;
                case CSeq_data::e_Ncbieaa:
                    ss = &data.GetNcbieaa().Get();
                    break;
                case CSeq_data::e_Ncbistdaa:
                    {
                        const vector<char>& c = data.GetNcbistdaa().Get();
                        ITERATE (vector<TSeqPos>, ci, badIdx) {
                            PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                                "Invalid residue [" +
                                NStr::IntToString((int)c[*ci]) + "] at position [" +
                                NStr::IntToString((*ci) + 1) + "]", seq);
                        }
                        break;
                    }
                default:
                    break;
                }

                if ( ss ) {
                    ITERATE (vector<TSeqPos>, it, badIdx) {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            ss->substr(*it, 1) + "] at position [" +
                            NStr::IntToString((*it) + 1) + "]", seq);
                    }
                }
                            
                if (mi) {
                    // Count adjacent Ns in Seq-lit
                    int max_ns = s_MaxNsInSeqLitForTech (tech);
                    size_t adjacent_ns = x_CountAdjacentNs(lit);
                    if (max_ns > -1 && adjacent_ns > max_ns) {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_InternalNsInSeqLit,
                                "Run of " + NStr::NumericToString(adjacent_ns) + 
                                " Ns in delta component " + NStr::UIntToString(seg) +
                                " that starts at base " + NStr::UIntToString(start_len + 1),
                                seq);
                    }
                }
            } else {
                gap_linkage = -1;
                gap_type = CSeq_gap::eType_unknown;
                if ( lit.IsSetSeq_data() && lit.GetSeq_data().IsGap() ) {
                    const CSeq_data& data = lit.GetSeq_data();
                    if (data.Which() == CSeq_data::e_Gap) {
                        const CSeq_gap& gap = data.GetGap();

                        if (gap.IsSetType()) {
                            gap_type = gap.GetType();
                            if (gap_type == CSeq_gap::eType_unknown && s_IsUnspecified(gap)) {
                                num_gap_unknown_unspec++;
                            }
                            else {
                                num_gap_known_or_spec++;
                            }
                        }
                        if(gap.IsSetLinkage())
                            gap_linkage = gap.GetLinkage();
                    }
                }
                if (first  && !x_IgnoreEndGap(bsh, gap_type) && !s_WillReportTerminalGap(seq, bsh)) {
                    EDiagSev sev = eDiag_Error;
                    if (tech != CMolInfo::eTech_htgs_0 && tech != CMolInfo::eTech_htgs_1
                        && tech != CMolInfo::eTech_htgs_2 && tech != CMolInfo::eTech_htgs_3) {
                        sev = eDiag_Warning;
                    }
                    PostErr(sev, eErr_SEQ_INST_BadDeltaSeq,
                            "First delta seq component is a gap", seq);
                }

                if(last_is_gap &&
                   (prev_gap_type == gap_type ||
                    prev_gap_linkage != gap_linkage ||
                    gap_linkage != CSeq_gap::eLinkage_unlinked)) {
                        if (prev_gap_type != CSeq_gap::eType_contamination && gap_type != CSeq_gap::eType_contamination) {
                            ++num_adjacent_gaps;
                        }
                    }

                if (lit.IsSetSeq_data() && lit.GetSeq_data().IsGap()) {
                    ValidateSeqGap(lit.GetSeq_data().GetGap(), seq);
                } else if (!lit.CanGetLength() || lit.GetLength() == 0) {
                    if (!lit.IsSetFuzz() || !lit.GetFuzz().IsLim() || lit.GetFuzz().GetLim() != CInt_fuzz::eLim_unk) {
                        PostErr(s_IsSwissProt(seq) ? eDiag_Warning : eDiag_Error, eErr_SEQ_INST_SeqLitGapLength0,
                            "Gap of length 0 in delta chain", seq);
                    } else {
                        PostErr(s_IsSwissProt(seq) ? eDiag_Warning : eDiag_Error, eErr_SEQ_INST_SeqLitGapLength0,
                            "Gap of length 0 with unknown fuzz in delta chain", seq);
                    }
                } else if (lit.CanGetLength() && lit.GetLength() != 100) {
                    if (lit.IsSetFuzz()) {
                        PostErr(eDiag_Warning, eErr_SEQ_INST_UnknownLengthGapNot100,
                            "Gap of unknown length should have length 100", seq);
                    }
                }
                last_is_gap = true;
                prev_gap_type = gap_type;
                prev_gap_linkage = gap_linkage;
                ++num_gaps;
            }
            first = false;
            break;
        }
        default:
            PostErr(eDiag_Critical, eErr_SEQ_INST_ExtNotAllowed,
                "CDelta_seq::Which() is e_not_set", seq);
        }
    }

    if (num_gap_unknown_unspec > 0 && num_gap_known_or_spec == 0) {
        if (num_gap_unknown_unspec > 1) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_SeqGapBadLinkage,
                "All " + NStr::IntToString(num_gap_unknown_unspec) +
                " Seq-gaps have unknown type and unspecified linkage", seq);
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_INST_SeqGapBadLinkage,
                "Single Seq-gap has unknown type and unspecified linkage", seq);
        }
    }

    if (inst.GetLength() > len) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
            "Bioseq.seq_data too short [" + NStr::IntToString(len) +
            "] for given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    } else if (inst.GetLength() < len) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
            "Bioseq.seq_data is larger [" + NStr::IntToString(len) +
            "] than given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    }
    if ( non_interspersed_gaps  &&  !has_gi && mi  &&
        (tech == CMolInfo::eTech_htgs_0  ||  tech == CMolInfo::eTech_htgs_1  ||
         tech == CMolInfo::eTech_htgs_2) ) {
        EDiagSev missing_gaps_sev = eDiag_Error;
        CSeqdesc_CI desc_i(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_User);
        while (desc_i) {
            if (desc_i->GetUser().IsRefGeneTracking()) {
                missing_gaps_sev = eDiag_Info;
                break;
            }
            ++desc_i;
        }

        PostErr(missing_gaps_sev, eErr_SEQ_INST_MissingGaps,
            "HTGS delta seq should have gaps between all sequence runs", seq);
    }
    if ( num_adjacent_gaps >= 1 ) {
        string msg = (num_adjacent_gaps == 1) ?
            "There is 1 adjacent gap in delta seq" :
            "There are " + NStr::SizetToString(num_adjacent_gaps) +
            " adjacent gaps in delta seq";
        PostErr(m_Imp.IsRefSeq() ? eDiag_Warning : eDiag_Error, eErr_SEQ_INST_BadDeltaSeq, msg, seq);
    }
    if (last_is_gap && !x_IgnoreEndGap(bsh, gap_type) && !s_WillReportTerminalGap(seq, bsh)) {
        EDiagSev sev = eDiag_Error;
        if (tech != CMolInfo::eTech_htgs_0 && tech != CMolInfo::eTech_htgs_1
            && tech != CMolInfo::eTech_htgs_2 && tech != CMolInfo::eTech_htgs_3) {
            sev = eDiag_Warning;
        }
        PostErr(sev, eErr_SEQ_INST_BadDeltaSeq,
                "Last delta seq component is a gap", seq);
    }
    
    // Validate technique
    if (num_gaps == 0  &&  mi) {
        if ( tech == CMolInfo::eTech_htgs_2  &&
             !GraphsOnBioseq(seq)  &&
             !x_IsActiveFin(seq) ) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_BadHTGSeq,
                "HTGS 2 delta seq has no gaps and no graphs", seq);
        }
    }

    // look for multiple delta locs overlapping
    if (delta_locs.size() > 1) {
        stable_sort (delta_locs.begin(), delta_locs.end(), s_LocSortCompare);
        vector<CConstRef<CSeq_loc> >::iterator it1 = delta_locs.begin();
        vector<CConstRef<CSeq_loc> >::iterator it2 = it1;
        ++it2;
        while (it2 != delta_locs.end()) {
            if ((*it1)->GetId()->Compare(*(*it2)->GetId()) == CSeq_id::e_YES
                && Compare (**it1, **it2, m_Scope, fCompareOverlapping) != eNoOverlap) {
                string seq_label = (*it1)->GetId()->AsFastaString();
                PostErr (eDiag_Warning, eErr_SEQ_INST_OverlappingDeltaRange,
                         "Overlapping delta range " + NStr::IntToString((*it2)->GetStart(eExtreme_Positional) + 1)
                         + "-" + NStr::IntToString((*it2)->GetStop(eExtreme_Positional) + 1)
                         + " and " + NStr::IntToString((*it1)->GetStart(eExtreme_Positional) + 1)
                         + "-" + NStr::IntToString((*it1)->GetStop(eExtreme_Positional) + 1)
                         + " on a Bioseq " + seq_label,
                         seq);
            }
            ++it1;
            ++it2;
        }
    }

    if (IsSelfReferential(seq)) {
        PostErr(eDiag_Critical, eErr_SEQ_INST_SelfReferentialSequence,
            "Self-referential delta sequence", seq);
    }

    // look for Ns next to gaps 
    if (seq.IsNa() && seq.GetLength() > 1 && x_IsDeltaLitOnly(inst)) {
        try {
            TSeqPos pos = 0;
            CSeqVector sv = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            ITERATE (CDelta_ext::Tdata, delta_i, seq.GetInst().GetExt().GetDelta().Get()) {
                if (delta_i->Empty()) {
                    continue; // Ignore NULLs, reported separately above.
                }
                const CDelta_seq& seg = **delta_i;
                TSeqPos delta_len = (TSeqPos)s_GetDeltaLen (seg, m_Scope);
                if (pos > 0) {
                    if (sv.IsInGap (pos)) {
                        CSeqVector::TResidue res = sv [pos - 1];
                        if (res == 'N' && !sv.IsInGap(pos - 1)) {
                            PostErr (eDiag_Error, eErr_SEQ_INST_InternalNsAdjacentToGap,
                                     "Ambiguous residue N is adjacent to a gap around position " + NStr::SizetToString (pos + 1),
                                     seq);
                        }
                    } 
                }
                if (delta_len > 0 && pos + delta_len < len) {
                    if (sv.IsInGap(pos + delta_len - 1)) {
                        CSeqVector::TResidue res = sv[pos + delta_len];
                        if (res == 'N' && !sv.IsInGap(pos + delta_len)) {
                            PostErr(eDiag_Error, eErr_SEQ_INST_InternalNsAdjacentToGap,
                                "Ambiguous residue N is adjacent to a gap around position " + NStr::SizetToString(pos + delta_len + 1),
                                seq);
                        }
                    }
                }
                pos += delta_len;
            }                        
        } catch (CException ) {
        } catch (std::exception ) {
        }
    }

}


bool s_HasGI(const CBioseq& seq)
{
    bool has_gi = false;
    FOR_EACH_SEQID_ON_BIOSEQ(id_it, seq) {
        if ((*id_it)->IsGi()) {
            has_gi = true;
            break;
        }
    }
    return has_gi;
}


void CValidError_bioseq::ValidateSeqGap(const CSeq_gap& gap, const CBioseq& seq)
{
    if (gap.IsSetLinkage_evidence()) {
        int linkcount = 0;
        int linkevarray[13];
        for (int i = 0; i < 13; i++) {
            linkevarray[i] = 0;
        }
        bool is_unspec = false;
        ITERATE(CSeq_gap::TLinkage_evidence, ev_itr, gap.GetLinkage_evidence()) {
            const CLinkage_evidence & evidence = **ev_itr;
            if (!evidence.CanGetType()) continue;
            int linktype = evidence.GetType();
            if (linktype == 8) {
                is_unspec = true;
            }
            linkcount++;
            if (linktype == 255) {
                (linkevarray[11])++;
            }
            else if (linktype < 0 || linktype > 10) {
                (linkevarray[12])++;
            }
            else {
                (linkevarray[linktype])++;
            }
        }
        if (linkevarray[8] > 0 && linkcount > linkevarray[8]) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqGapBadLinkage,
                "Seq-gap type has unspecified and additional linkage evidence", seq);
        }
        for (int i = 0; i < 13; i++) {
            if (linkevarray[i] > 1) {
                PostErr(eDiag_Error, eErr_SEQ_INST_SeqGapBadLinkage,
                    "Linkage evidence '" + linkEvStrings[i] + "' appears " +
                    NStr::IntToString(linkevarray[i]) + " times", seq);
            }
        }
        if (!gap.IsSetLinkage() || gap.GetLinkage() != CSeq_gap::eLinkage_linked) {
            PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapBadLinkage,
                "Seq-gap with linkage evidence must have linkage field set to linked", seq);
        }
        if (gap.IsSetType()) {
            int gaptype = gap.GetType();
            if (gaptype != CSeq_gap::eType_fragment &&
                gaptype != CSeq_gap::eType_clone &&
                gaptype != CSeq_gap::eType_repeat &&
                gaptype != CSeq_gap::eType_scaffold) {
                if (gaptype == CSeq_gap::eType_unknown && is_unspec) {
                    /* suppress for legacy records */
                } else if (gaptype == CSeq_gap::eType_contamination) {
                    if (linkevarray[8] > 0 && linkcount == linkevarray[8]) {
                        /* contamination can only have linked unspecified */
                    } else {
                        PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapBadLinkage,
                           "Contamination gaps must have linkage evidence 'unspecified'", seq);
                    }
                } else {
                    PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapBadLinkage,
                       "Seq-gap of type " + NStr::IntToString(gaptype) +
                       " should not have linkage evidence", seq);
                }
            }
        }
    }
    else {
        if (gap.IsSetType()) {
            int gaptype = gap.GetType();
            if (gaptype == CSeq_gap::eType_scaffold) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapProblem,
                    "Seq-gap type == scaffold is missing required linkage evidence", seq);
            }
            if (gaptype == CSeq_gap::eType_repeat && gap.IsSetLinkage() && gap.GetLinkage() == CSeq_gap::eLinkage_linked)
            {
                bool suppress_SEQ_INST_SeqGapProblem = false;
                if (seq.IsSetDescr() && s_HasGI(seq))
                {
                    ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get())
                    {
                        if ((**it).IsCreate_date())
                        {
                            CDate threshold_date(CTime(2012, 10, 1));
                            if ((**it).GetCreate_date().Compare(threshold_date) == CDate::eCompare_before)
                                suppress_SEQ_INST_SeqGapProblem = true;
                            break;
                        }
                    }
                }
                if (!suppress_SEQ_INST_SeqGapProblem)
                    PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapProblem,
                    "Seq-gap type == repeat and linkage == linked is missing required linkage evidence", seq);

            }
            if (gaptype == CSeq_gap::eType_contamination) {
                PostErr(eDiag_Critical, eErr_SEQ_INST_SeqGapBadLinkage,
                    "Contamination gap-types must be linked and have linkage-evidence of type 'unspecified'", seq);
            }
        }
    }
}


bool CValidError_bioseq::ValidateRepr
(const CSeq_inst& inst,
 const CBioseq&   seq)
{
    bool rtn = true;
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    string rpr = tv->FindName(inst.GetRepr(), true);
    if (NStr::Equal(rpr, "ref")) {
        rpr = "reference";
    } else if (NStr::Equal(rpr, "const")) {
        rpr = "constructed";
    }
    const string err0 = "Bioseq-ext not allowed on " + rpr + " Bioseq";
    const string err1 = "Missing or incorrect Bioseq-ext on " + rpr + " Bioseq";
    const string err2 = "Missing Seq-data on " + rpr + " Bioseq";
    const string err3 = "Seq-data not allowed on " + rpr + " Bioseq";
    switch (inst.GetRepr()) {
    case CSeq_inst::eRepr_virtual:
        if (inst.IsSetExt()) {
            PostErr(eDiag_Critical, eErr_SEQ_INST_ExtNotAllowed, err0, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_map:
        if (!inst.IsSetExt() || !inst.GetExt().IsMap()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_ref:
        if (!inst.IsSetExt() || !inst.GetExt().IsRef() ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_seg:
        if (!inst.IsSetExt() || !inst.GetExt().IsSeg() ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_raw:
    case CSeq_inst::eRepr_const:
        if (inst.IsSetExt()) {
            PostErr(eDiag_Critical, eErr_SEQ_INST_ExtNotAllowed, err0, seq);
            rtn = false;
        }
        if (!inst.IsSetSeq_data() ||
          inst.GetSeq_data().Which() == CSeq_data::e_not_set
          || inst.GetSeq_data().Which() == CSeq_data::e_Gap)
        {
            PostErr(eDiag_Critical, eErr_SEQ_INST_SeqDataNotFound, err2, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_delta:
        if (!inst.IsSetExt() || !inst.GetExt().IsDelta() ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            PostErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    default:
        PostErr(
            eDiag_Critical, eErr_SEQ_INST_ReprInvalid,
            "Invalid Bioseq->repr = " +
            NStr::IntToString(static_cast<int>(inst.GetRepr())), seq);
        rtn = false;
    }
    return rtn;
}


bool CValidError_bioseq::x_ParentAndComponentLocationsDiffer(CBioseq_Handle bsh, CBioSource::TGenome parent_location)
{
    bool rval = false;

    // not a delta sequence, so no components to check
    if (!bsh.IsSetInst()
        || !bsh.GetInst().IsSetRepr()
        || bsh.GetInst().GetRepr() != CSeq_inst::eRepr_delta
        || !bsh.GetInst().IsSetExt()
        || !bsh.GetInst().GetExt().IsDelta()
        || !bsh.GetInst().GetExt().GetDelta().IsSet()) {
        return rval;
    }

    for (auto it : bsh.GetInst().GetExt().GetDelta().Get()) {
        if (! it->IsLoc()) continue;
        CBioseq_Handle hdl = bsh.GetScope().GetBioseqHandle(it->GetLoc());
        if (! hdl) continue;
        CSeqdesc_CI ci(hdl, CSeqdesc::e_Source);
        if (! ci) continue;
        const CBioSource& crc = ci->GetSource();
        CBioSource::TGenome cgenome = CBioSource::eGenome_unknown;
        if (crc.IsSetGenome()) {
            cgenome = crc.GetGenome();
        }
        if (parent_location == cgenome) break;
        if (parent_location == CBioSource::eGenome_unknown || parent_location == CBioSource::eGenome_genomic) break;
        if (cgenome == CBioSource::eGenome_unknown || cgenome == CBioSource::eGenome_genomic) break;
        rval = true;
        break;
    }
    return rval;
}


// From VR-796:
// If the lineage contains Metazoa, the topology is circular, and the 
// location is mitochondrion, the sequence length should not be
// greater than 65000bp.
// This is erring on the side of caution as most Metazoan genomes are less than 17000 bp.
size_t CValidError_bioseq::x_BadMetazoanMitochondrialLength(const CBioSource& src, const CSeq_inst& inst)
{
    if (src.IsSetGenome() && src.GetGenome() == CBioSource::eGenome_mitochondrion &&
        inst.IsSetTopology() && inst.GetTopology() == CSeq_inst::eTopology_circular &&
        src.IsSetOrg() && src.GetOrg().IsSetLineage() &&
        inst.IsSetLength() &&
        NStr::Find(src.GetOrg().GetLineage(), "Metazoa") != NPOS) {
        return 65000;
    }
    return 0;
}


void CValidError_bioseq::CheckSourceDescriptor(
    const CBioseq_Handle& bsh)
{
    CSeqdesc_CI di(bsh, CSeqdesc::e_Source);
    if (!di) {
        // add to list of sources with no descriptor later to be reported
        m_Imp.AddBioseqWithNoBiosource(*(bsh.GetBioseqCore()));
        return;
    }
    _ASSERT(di);

    const CBioSource& src = di->GetSource();

    if (m_Imp.IsTransgenic(src)  &&
        CSeq_inst::IsNa(bsh.GetInst_Mol())) {
        // "if" means "if no biosrcs on bsh"
        if( GetCache().GetFeatFromCache(
                CCacheImpl::SFeatKey(
                    CSeqFeatData::e_Biosrc, CCacheImpl::kAnyFeatSubtype, bsh)).empty() )
        {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_TransgenicProblem,
                "Transgenic source descriptor requires presence of source feature",
                *(bsh.GetBioseqCore()));
        }
    }

    if (src.IsSetGenome() && x_ParentAndComponentLocationsDiffer(bsh, src.GetGenome())) {
        PostErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentBioSources_ConLocation,
            "Genome difference between parent and component",
            *(bsh.GetBioseqCore()));
    }

    size_t max_len = x_BadMetazoanMitochondrialLength(src, bsh.GetInst());
    if (max_len > 0 && bsh.GetInst().IsSetLength() && bsh.GetInst().GetLength() > max_len) {
        PostErr(eDiag_Error, eErr_SEQ_INST_MitoMetazoanTooLong,
            "Mitochondrial Metazoan sequences should be less than 65000 bp",
            *(bsh.GetBioseqCore()));
    }
}


void CValidError_bioseq::CheckForMolinfoOnBioseq(const CBioseq& seq)
{
    CSeqdesc_CI sd(m_CurrentHandle, CSeqdesc::e_Molinfo);

    if ( !sd ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoMolInfoFound, 
            "No Mol-info applies to this Bioseq",
            seq);
    }
}


void CValidError_bioseq::CheckForPubOnBioseq(const CBioseq& seq)
{
    string label = "";
    seq.GetLabel(&label, CBioseq::eBoth);

    if ( !m_CurrentHandle ) {
        return;
    }

    if ( !CSeqdesc_CI( m_CurrentHandle, CSeqdesc::e_Pub) && m_AllFeatIt) {
        // look for pub or feat with cit
        ITERATE(CCacheImpl::TFeatValue, all_feat_it, *m_AllFeatIt) {
            if (all_feat_it->IsSetCit() || all_feat_it->GetData().IsPub()) {
                    return;
                }
            }

        m_Imp.AddBioseqWithNoPub(seq);
    }
}


void CValidError_bioseq::x_ReportSuspiciousUseOfComplete(const CBioseq& seq, EDiagSev sev)
{
    CConstRef<CSeqdesc> closest_molinfo = seq.GetClosestDescriptor(CSeqdesc::e_Molinfo);
    if (closest_molinfo) {
        const CSeq_entry& ctx = *seq.GetParentEntry();
        PostErr(sev, eErr_SEQ_DESCR_UnwantedCompleteFlag,
            "Suspicious use of complete", ctx, *closest_molinfo);
    } else {
        PostErr(sev, eErr_SEQ_DESCR_UnwantedCompleteFlag,
            "Suspicious use of complete", seq);
    }
}


void CValidError_bioseq::x_ValidateCompletness
(const CBioseq& seq,
 const CMolInfo& mi)
{
    if ( !mi.IsSetCompleteness() ) {
        return;
    }
    if ( !seq.IsNa() ) {
        return;
    }

    CMolInfo::TCompleteness comp = mi.GetCompleteness();
    CMolInfo::TBiomol biomol = mi.IsSetBiomol() ? 
        mi.GetBiomol() : CMolInfo::eBiomol_unknown;
    EDiagSev sev = mi.GetTech() == CMolInfo::eTech_htgs_3 ? 
        eDiag_Warning : /* eDiag_Error */ eDiag_Warning;

    CSeqdesc_CI desc(m_CurrentHandle, CSeqdesc::e_Title);
    if ( desc ) {
        const string& title = desc->GetTitle();
        if (!NStr::IsBlank(title)) {
            if (NStr::FindNoCase(title, "complete sequence") != string::npos
                || NStr::FindNoCase(title, "complete genome") != string::npos) {
                return;
            }
        }
    }

    bool reported = false;

    if ( comp == CMolInfo::eCompleteness_complete ) {
        if ( biomol == CMolInfo::eBiomol_genomic || biomol == CMolInfo::eBiomol_cRNA ) {
            bool is_gb = false;
            FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
                if ( (*it)->IsGenbank() ) {
                    is_gb = true;
                    break;
                }
            }

            if ( is_gb ) {
                if (seq.IsSetInst() && seq.GetInst().IsSetTopology()
                    && seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular) {
                    const CSeq_entry& ctx = *seq.GetParentEntry();
                    PostErr(eDiag_Warning, eErr_SEQ_INST_CompleteCircleProblem,
                            "Circular topology has complete flag set, but title should say complete sequence or complete genome",
                            ctx);
                } else {
                    x_ReportSuspiciousUseOfComplete(seq, sev);
                    reported = true;
                }
            }
        }

        if (!reported) {
            // for SQD-1484
            // warn if completeness = complete, organism not viral and origin not artificial, no location set or location is genomic
            CSeqdesc_CI src_desc(m_CurrentHandle, CSeqdesc::e_Source);
            if (src_desc) {
                const CBioSource& biosrc = src_desc->GetSource();
                if ((!biosrc.IsSetLineage() 
                     || (NStr::FindNoCase(biosrc.GetLineage(), "Viruses") == string::npos
                         && NStr::FindNoCase(biosrc.GetLineage(), "Viroids") == string::npos)) // not viral
                    && (!biosrc.IsSetOrigin() || biosrc.GetOrigin() != CBioSource::eOrigin_artificial) // not artificial
                    && (!src_desc->GetSource().IsSetGenome()
                        || src_desc->GetSource().GetGenome() == CBioSource::eGenome_genomic)) { // location not set or genomic
                    x_ReportSuspiciousUseOfComplete(seq, eDiag_Warning);
                    reported = true;
                }
            }
        }
        if (!reported && HasAssemblyOrNullGap(seq)) {
            // for VR-614
            x_ReportSuspiciousUseOfComplete(seq, eDiag_Warning);
        }
    }
}


static bool s_StandaloneProt(const CBioseq_Handle& bsh)
{
    // proteins are never standalone within the context of a Genbank / Refseq
    // record.

    CSeq_entry_Handle eh = bsh.GetSeq_entry_Handle();
    while ( eh ) {
        if ( eh.IsSet() ) {
            CBioseq_set_Handle bsh = eh.GetSet();
            if ( bsh.IsSetClass() ) {
                CBioseq_set::TClass cls = bsh.GetClass();
                switch ( cls ) {
                case CBioseq_set::eClass_nuc_prot:
                case CBioseq_set::eClass_mut_set:
                case CBioseq_set::eClass_pop_set:
                case CBioseq_set::eClass_phy_set:
                case CBioseq_set::eClass_eco_set:
                case CBioseq_set::eClass_gen_prod_set:
                    return false;
                default:
                    break;
                }
            }
        }
        eh = eh.GetParentEntry();
    }

    return true;
}


static CBioseq_Handle s_GetParent(const CBioseq_Handle& part)
{
    CBioseq_Handle parent;

    if ( part ) {
        CSeq_entry_Handle segset = 
            part.GetExactComplexityLevel(CBioseq_set::eClass_segset);
        if ( segset ) {
            for ( CSeq_entry_CI it(segset); it; ++it ) {
                if ( it->IsSeq()  &&  it->GetSeq().IsSetInst_Repr()  &&
                    it->GetSeq().GetInst_Repr() == CSeq_inst::eRepr_seg ) {
                    parent = it->GetSeq();
                    break;
                }
            }
        }
    }
    return parent;
}


static bool s_SeqIdCompare (const CConstRef<CSeq_id>& q1, const CConstRef<CSeq_id>& q2)
{
    // is q1 < q2
    return (q1->CompareOrdered(*q2) < 0);
}


static bool s_SeqIdMatch (const CConstRef<CSeq_id>& q1, const CConstRef<CSeq_id>& q2)
{
    // is q1 == q2
    return (q1->CompareOrdered(*q2) == 0);
}


void CValidError_bioseq::ValidateMultipleGeneOverlap (const CBioseq_Handle& bsh)
{
    if (!m_GeneIt) {
        return;
    }
    /*
    bool is_circular = bsh.IsSetInst_Topology() && bsh.GetInst_Topology() == CSeq_inst::eTopology_circular;
    */
    try {
        vector< CConstRef < CSeq_feat > > containing_genes;
        vector< int > num_contained;
        ITERATE(CCacheImpl::TFeatValue, fi, *m_GeneIt ) {
            TSeqPos left = fi->GetLocation().GetStart(eExtreme_Positional);
            vector< CConstRef < CSeq_feat > >::iterator cit = containing_genes.begin();
            vector< int >::iterator nit = num_contained.begin();
            while (cit != containing_genes.end() && nit != num_contained.end()) {
                ECompare comp = Compare(fi->GetLocation(), (*cit)->GetLocation(), m_Scope, fCompareOverlapping);
                if (comp == eContained  ||  comp == eSame) {
                    (*nit)++;
                }
                TSeqPos n_right = (*cit)->GetLocation().GetStop(eExtreme_Positional);
                if (n_right < left) {
                    // report if necessary
                    if (*nit > 4) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleGeneOverlap, 
                                 "Gene contains " + NStr::IntToString (*nit) + " other genes",
                                 **cit);
                    }
                    // remove from list
                    cit = containing_genes.erase(cit);
                    nit = num_contained.erase(nit);
                } else {
                    ++cit;
                    ++nit;
                }
            }

            const CSeq_feat& ft = fi->GetOriginalFeature();
            const CSeq_feat* p = &ft;
            CConstRef<CSeq_feat> ref(p);
            containing_genes.push_back (ref);
            num_contained.push_back (0);
        }

        vector< CConstRef < CSeq_feat > >::iterator cit = containing_genes.begin();
        vector< int >::iterator nit = num_contained.begin();
        while (cit != containing_genes.end() && nit != num_contained.end()) {
            if (*nit > 4) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleGeneOverlap, 
                         "Gene contains " + NStr::IntToString (*nit) + " other genes",
                         **cit);
            }
            ++cit;
            ++nit;
        }
    } catch ( const exception& e ) {
        PostErr(eDiag_Error, eErr_INTERNAL_Exception,
            string("Exception while validating bioseq MultipleGeneOverlap. EXCEPTION: ") +
            e.what(), *(bsh.GetCompleteBioseq()));
    }
}


void CValidError_bioseq::x_ReportGeneOverlapError(const CSeq_feat& feat, const string& gene_label)
{
    string msg("gene [");
    msg += gene_label;

    if (feat.GetData().IsCdregion()) {

        msg += "] overlaps CDS but does not completely contain it";
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSgeneRange, msg, feat);

    } else if (feat.GetData().IsRna()) {

        if (GetOverlappingOperon(feat.GetLocation(), *m_Scope)) {
            return;
        }

        msg += "] overlaps mRNA but does not completely contain it";
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_mRNAgeneRange, msg, feat);
    }
}

static void s_GetGeneTextLabel(const CSeq_feat& feat, string& label)
{
    _ASSERT(feat.IsSetData() && feat.GetData().IsGene() && "Here should be a gene feature");
    _ASSERT(feat.IsSetLocation() && "The feature should have a location");

    if (feat.IsSetData() && feat.GetData().IsGene()) {

        const CGene_ref& gene = feat.GetData().GetGene();
        if (gene.IsSetLocus()) {
            label += gene.GetLocus();
        }
        else if (gene.IsSetDesc()) {
            label += gene.GetDesc();
        }

        if (feat.IsSetLocation()) {
            string loc_label;
            feat.GetLocation().GetLabel(&loc_label);

            if (!label.empty()) {
                label += ':';
            }
            label += loc_label;
        }

        if (gene.IsSetLocus_tag()) {

            if (!label.empty()) {
                label += ':';
            }
            label += gene.GetLocus_tag();
        }
    }
}

void CValidError_bioseq::ValidateBadGeneOverlap(const CSeq_feat& feat)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( grp != 0 && grp->IsSuppressed()) {
        return;
    }

    CConstRef<CSeq_feat> connected_gene = m_Imp.GetCachedGene(&feat);
    if (connected_gene) {
        EOverlapType overlap_type = eOverlap_Contained;
        if (feat.IsSetExcept_text() &&
            NStr::FindNoCase(feat.GetExcept_text(), "trans-splicing") != string::npos) {
            overlap_type = eOverlap_Subset;
        }

        if (TestForOverlapEx(connected_gene->GetLocation(), feat.GetLocation(),
                             overlap_type, m_Scope) < 0) {

            string gene_label;
            s_GetGeneTextLabel(*connected_gene, gene_label);
            x_ReportGeneOverlapError(feat, gene_label);
        }
        return;
    }
    /*
    const CGene_ref* grp = feat.GetGeneXref();
    if ( grp != 0 && grp->IsSuppressed()) {
        return;
    }
    */

    const CSeq_loc& loc = feat.GetLocation();

    CConstRef<CSeq_feat> gene = GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_gene, eOverlap_Simple, *m_Scope);
    if (! gene) return;
    if (TestForOverlapEx(gene->GetLocation(), feat.GetLocation(), eOverlap_Contained, m_Scope) < 0) {

        string gene_label;
        s_GetGeneTextLabel(*gene, gene_label);

        // found an intersecting (but not overlapping) gene
        x_ReportGeneOverlapError(feat, gene_label);
    }
}


bool s_IsCDDFeat(const CMappedFeat& feat)
{
    if ( feat.GetData().IsRegion() ) {
        FOR_EACH_DBXREF_ON_FEATURE (db, feat) {
            if ( (*db)->CanGetDb()  &&
                NStr::Compare((*db)->GetDb(), "CDD") == 0 ) {
                return true;
            }
        }
    }
    return false;
}


bool s_CheckPosNOrGap(TSeqPos pos, const CSeqVector& vec)
{
    if (vec.IsInGap(pos) || vec[pos] == 'N') {
        return true;
    } else {
        return false;
    }
}


bool s_AfterIsGapORN(TSeqPos pos, TSeqPos after, TSeqPos len,  const CSeqVector& vec)
{
    if (pos < len - after && s_CheckPosNOrGap(pos + after, vec)) {
        return true;
    } else {
        return false;
    }
}


bool s_AfterIsGap(TSeqPos pos, TSeqPos after, TSeqPos len, const CSeqVector& vec)
{
    if (pos < len - after && vec.IsInGap(pos + after)) {
        return true;
    } else {
        return false;
    }
}


bool s_BeforeIsGapOrN(TSeqPos pos, TSeqPos before, const CSeqVector& vec)
{
    if (pos >= before && s_CheckPosNOrGap(pos - before, vec)) {
        return true;
    } else {
        return false;
    }
}


bool s_BeforeIsGap(TSeqPos pos, TSeqPos before, const CSeqVector& vec)
{
    if (pos >= before && vec.IsInGap(pos - before)) {
        return true;
    } else {
        return false;
    }
}


bool CValidError_bioseq::x_IsPartialAtSpliceSiteOrGap
(const CSeq_loc& loc,
 unsigned int tag,
 bool& bad_seq,
 bool& is_gap,
 bool& abuts_n)
{
    bad_seq = false;
    is_gap = false;
    abuts_n = false;
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

    if (!m_Scope) {
        return false;
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandleFromTSE(*(temp.GetRangeAsSeq_loc()->GetId()), m_Imp.GetTSE_Handle() );
    if (!bsh) {
        return false;
    }
    
    TSeqPos acceptor = temp.GetRange().GetFrom();
    TSeqPos donor = temp.GetRange().GetTo();
    TSeqPos start = acceptor;
    TSeqPos stop = donor;

    CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,
        temp.GetStrand());
    TSeqPos len = bsh.GetBioseqLength();
    if (start >= len || stop >= len) {
        return false;
    }

    if ( temp.GetStrand() == eNa_strand_minus ) {
        swap(acceptor, donor);
        stop = len - donor - 1;
        start = len - acceptor - 1;
    }

    bool result = false;

    try {
        if (tag == eSeqlocPartial_Nostop) {
            if (s_AfterIsGap(stop, 1, len, vec) || s_AfterIsGap(stop, 2, len, vec)) {
                is_gap = true;
                return true;
            } else if (s_AfterIsGapORN(stop, 1, len, vec)) {
                abuts_n = true;
                return false;
            }
        } else if (tag == eSeqlocPartial_Nostart && start > 0) {
            if (s_BeforeIsGap(start, 1, vec) || s_BeforeIsGap(start, 2, vec)) {
                is_gap = true;
                return true;
            } else if (s_BeforeIsGapOrN(start, 1, vec)) {
                abuts_n = true;
                return false;
            }
        }
    } catch ( exception& ) {
        
        return false;
    }

    if ( (tag == eSeqlocPartial_Nostop)  &&  (stop < len - 2) ) {
        try {
            CSeqVector::TResidue res1 = vec[stop + 1];
            CSeqVector::TResidue res2 = vec[stop + 2];

            if ( IsResidue(res1)  &&  IsResidue(res2) && isalpha (res1) && isalpha (res2)) {
                if ( (res1 == 'G'  &&  res2 == 'T')  || 
                    (res1 == 'G'  &&  res2 == 'C') ) {
                    result = true;
                }
            } else {
                bad_seq = true;
            }
        } catch ( exception& ) {
            return false;
        }
    } else if ( (tag == eSeqlocPartial_Nostart)  &&  (start > 1) ) {
        try {
            CSeqVector::TResidue res1 = vec[start - 2];    
            CSeqVector::TResidue res2 = vec[start - 1];
        
            if ( IsResidue(res1)  &&  IsResidue(res2) && isalpha (res1) && isalpha (res2)) {
                if ( (res1 == 'A')  &&  (res2 == 'G') ) { 
                    result = true;
                }
            } else {
                bad_seq = true;
            }
        } catch ( exception& ) {
            return false;
        }
    }

    return result;    
}


static bool s_MatchPartialType (const CSeq_loc& loc1, const CSeq_loc& loc2, unsigned int partial_type)
{
    bool rval = false;

    switch (partial_type) {
        case eSeqlocPartial_Nostart:
            if (loc1.GetStart(eExtreme_Biological) == loc2.GetStart(eExtreme_Biological)) {
                rval = true;
            }
            break;
        case eSeqlocPartial_Nostop:
            if (loc1.GetStop(eExtreme_Biological) == loc2.GetStop(eExtreme_Biological)) {
                rval = true;
            }
            break;
        default:
            rval = false;
            break;
    }
    return rval;
}


// REQUIRES: feature is either Gene or mRNA
bool CValidError_bioseq::x_IsSameAsCDS(
    const CMappedFeat& feat)
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
            if ( TestForOverlapEx(
                    cds->GetLocation(),
                    feat.GetLocation(),
                    eOverlap_Simple) == 0 ) {
                return true;
            }
        }
    return false;
}


bool CValidError_bioseq::x_MatchesOverlappingFeaturePartial (const CMappedFeat& feat, unsigned int partial_type)
{
    bool rval = false;


    if (feat.GetData().IsGene()) {
        TSeqPos gene_start = feat.GetLocation().GetStart(eExtreme_Biological);
        TSeqPos gene_stop = feat.GetLocation().GetStop(eExtreme_Biological);

        // gene is ok if its partialness matches the overlapping coding region or mRNA
        CRef<feature::CFeatTree> tr = m_Imp.GetGeneCache().GetFeatTreeFromCache(feat.GetLocation(), *m_Scope);
        vector<CMappedFeat> children = tr->GetChildren(feat);
        ITERATE(vector<CMappedFeat>, it, children) {
            if ((it->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA || it->GetData().IsCdregion()) &&
                it->GetLocation().GetStart(eExtreme_Biological) == gene_start &&
                it->GetLocation().GetStop(eExtreme_Biological) == gene_stop &&
                s_MatchPartialType(feat.GetLocation(), it->GetLocation(), partial_type)) {
                rval = true;
                break;
            }
        }
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
        bool look_for_gene = true;
        TSeqPos mrna_start = feat.GetLocation().GetStart(eExtreme_Biological);
        TSeqPos mrna_stop = feat.GetLocation().GetStop(eExtreme_Biological);

        vector<CMappedFeat> cds_children = m_Imp.GetGeneCache().GetFeatTreeFromCache(m_CurrentHandle)->GetChildren(feat);
        if (cds_children.size() > 0) {
            look_for_gene = false;
            for (auto it = cds_children.begin(); it != cds_children.end(); it++) {
                if (partial_type == sequence::eSeqlocPartial_Nostart) {
                    if (it->GetLocation().GetStart(eExtreme_Biological) == mrna_start) {
                        rval = true;
                    } else {
                        rval = false;
                    }
                } else if (partial_type == sequence::eSeqlocPartial_Nostop) {
                    if (it->GetLocation().GetStop(eExtreme_Biological) == mrna_stop) {
                        rval = true;
                    } else {
                        rval = false;
                    }
                }
            }
        }

        if (!rval && look_for_gene) {
            CConstRef<CSeq_feat> gene = m_Imp.GetGeneCache().GetGeneFromCache(feat.GetSeq_feat(), *m_Scope);
            if (gene) {
                const CSeq_loc& gene_loc = gene->GetLocation();
                if (gene_loc.GetStart(eExtreme_Biological) == mrna_start
                    && gene_loc.GetStop(eExtreme_Biological) == mrna_stop
                    && s_MatchPartialType(feat.GetLocation(), gene_loc, partial_type)) {
                    rval = true;
                }
            }
        }
    } else if (feat.GetData().IsCdregion()) {
        // coding region is ok if same as mRNA AND partial at splice site or gap
        CRef<feature::CFeatTree> tr = m_Imp.GetGeneCache().GetFeatTreeFromCache(feat.GetLocation(), *m_Scope);
        CMappedFeat mrna = tr->GetParent(feat, CSeqFeatData::eSubtype_mRNA);
        if (mrna) {
            const CSeq_loc& mrna_loc = mrna.GetLocation();
            bool bad_seq = false;
            bool is_gap = false;
            bool abuts_n = false;
            if (s_MatchPartialType(feat.GetLocation(), mrna_loc, partial_type)
                && x_IsPartialAtSpliceSiteOrGap(feat.GetLocation(), partial_type, bad_seq, is_gap, abuts_n)) {
                rval = true;
            }
        }
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_exon) {
        // exon is ok if its partialness and endpoint matches the mRNA endpoint
#ifdef USE_FEAT_TREE_FOR_EXON
        CRef<feature::CFeatTree> tr = m_Imp.GetGeneCache().GetFeatTreeFromCache(feat.GetLocation(), *m_Scope);
        CMappedFeat mrna = tr->GetParent(feat, CSeqFeatData::eSubtype_mRNA);
        if (mrna) {
            const CSeq_loc& mrna_loc = mrna.GetLocation();
            if (s_MatchPartialType(feat.GetLocation(), mrna_loc, partial_type)) {
                if (partial_type == eSeqlocPartial_Nostart
                    && mrna_loc.IsPartialStart(eExtreme_Biological)) {
                    rval = true;
                } else if (partial_type == eSeqlocPartial_Nostop
                    && mrna_loc.IsPartialStop(eExtreme_Biological)) {
                    rval = true;
                }
            }
        }
#else
        TFeatScores mRNAs;
        GetOverlappingFeatures(feat.GetLocation(), CSeqFeatData::e_Rna, 
            CSeqFeatData::eSubtype_mRNA, eOverlap_CheckIntRev, mRNAs, *m_Scope);
        ITERATE(TFeatScores, s, mRNAs) {
            const CSeq_loc& mrna_loc = s->second->GetLocation();
            if (s_MatchPartialType(feat.GetLocation(), mrna_loc, partial_type)) {
                if (partial_type == eSeqlocPartial_Nostart
                    && mrna_loc.IsPartialStart(eExtreme_Biological)) {
                    rval = true;
                } else if (partial_type == eSeqlocPartial_Nostop
                    && mrna_loc.IsPartialStop(eExtreme_Biological)) {
                    rval = true;
                    break;
                }
            }
        }
#endif
    }

    return rval;
}


void CValidError_bioseq::x_ReportImproperPartial(const CSeq_feat& feat)
{
    if (m_Imp.x_IsFarFetchFailure(feat.GetLocation())) {
        m_Imp.SetFarFetchFailure();
    } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion
        && feat.IsSetExcept()
        && NStr::Find(feat.GetExcept_text(), "rearrangement required for product") != string::npos) {
        // suppress
    } else {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
            "PartialLocation: Improper use of partial (greater than or less than)", feat);
    }
}


void CValidError_bioseq::x_ReportInternalPartial(const CSeq_feat& feat)
{
    if (m_Imp.x_IsFarFetchFailure(feat.GetLocation())) {
        m_Imp.SetFarFetchFailure();
    } else if (m_Imp.IsRefSeq()) {
        // suppress
    } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion
        && feat.IsSetExcept()
        && NStr::Find(feat.GetExcept_text(), "rearrangement required for product") != string::npos) {
        // suppress
    } else if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
        // ignore start/stop not at end in genomic gpipe sequence
    } else {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
            "PartialLocation: Internal partial intervals do not include first/last residue of sequence", feat);
    }
}


bool StrandsMatch(ENa_strand s1, ENa_strand s2)
{
    if (s1 == eNa_strand_minus) {
        if (s2 == eNa_strand_minus) {
            return true;
        } else {
            return false;
        }
    } else {
        if (s2 == eNa_strand_minus) {
            return false;
        } else {
            return true;
        }
    }
}


bool CValidError_bioseq::x_PartialAdjacentToIntron(const CSeq_loc& loc)
{
    bool partial_start = loc.IsPartialStart(eExtreme_Positional);
    bool partial_stop = loc.IsPartialStop(eExtreme_Positional);
    if (!partial_start && !partial_stop) {
        return false;
    }
    TSeqPos start = loc.GetStart(eExtreme_Positional);
    TSeqPos stop = loc.GetStop(eExtreme_Positional);
    ENa_strand feat_strand = loc.GetStrand();

    CBioseq_Handle bsh;
    try {
        bsh = m_Scope->GetBioseqHandle(loc);
    } catch (CException& ex) {
        return false;
    }
    if (!bsh) {
        return false;
    }

    CFeat_CI intron(bsh, CSeqFeatData::eSubtype_intron);
    while (intron) {
        ENa_strand intron_strand = intron->GetLocation().GetStrand();
        if (StrandsMatch(feat_strand, intron_strand)) {            
            TSeqPos intron_start = intron->GetLocation().GetStart(eExtreme_Positional);
            if (intron_start == stop + 1 && partial_stop) {
                return true;
            } 
            if (intron_start > stop + 1) {
                return false;
            }
            if (start > 0 && partial_start) {
                TSeqPos intron_stop = intron->GetLocation().GetStop(eExtreme_Positional);
                if (intron_stop == start - 1) {
                    return true;
                }
            }
        }
        ++intron;
    }
    return false;
}


void CValidError_bioseq::x_ReportStartStopPartialProblem(int partial_type, bool at_splice_or_gap, bool abuts_n, const CSeq_feat& feat)
{
    EDiagSev sev = eDiag_Warning;
    if (m_Imp.IsGenomeSubmission() && 
        (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA ||
         feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA)) {
        sev = eDiag_Error;
    }

    string msg = (partial_type == 0 ? "Start" : "Stop");
    msg += " does not include first/last residue of ";

    bool mrna = false;
    bool organelle = false;
    bool not_expected = false;
    if (at_splice_or_gap) {
        if (feat.GetData().IsCdregion() || feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
            if (m_CurrentHandle && IsMrna(m_CurrentHandle)) {
                msg += "mRNA ";
                mrna = true;
            }
            else if (m_CurrentHandle && IsOrganelle(m_CurrentHandle)) {
                msg += "organelle ";
                organelle = true;
                sev = eDiag_Info;
            }
            else if (m_splicing_not_expected) {
                not_expected = true;
                sev = eDiag_Info;
            }
            else {
                // do not report
                return;
            }
        }
    }
    msg += "sequence";
    if (organelle) {
        msg += " (organelle does not use standard splice site convention)";
    }
    if (not_expected) {
        msg += " (but is at consensus splice site)";
    }

    EErrType err_type;

    if (sev == eDiag_Warning && abuts_n) {
        sev = eDiag_Info;
    }

    if (partial_type == 0) {
        if (mrna) {
            err_type = eErr_SEQ_FEAT_PartialProblemmRNASequence5Prime;
        }
        else if (organelle) {
            err_type = eErr_SEQ_FEAT_PartialProblemOrganelle5Prime;
        }
        else {
            err_type = eErr_SEQ_FEAT_PartialProblem5Prime;
        }
    } else {
        if (mrna) {
            err_type = eErr_SEQ_FEAT_PartialProblemmRNASequence3Prime;
        } else if (organelle) {
            err_type = eErr_SEQ_FEAT_PartialProblemOrganelle3Prime;
        } else {
            err_type = eErr_SEQ_FEAT_PartialProblem3Prime;
        }
    }

    PostErr(sev, err_type, msg, feat);
}


void CValidError_bioseq::ValidateFeatPartialInContext (
    const CMappedFeat& feat, bool is_complete)
{
    unsigned int partial_loc  = eSeqlocPartial_Complete;

    bool is_partial = feat.IsSetPartial()  &&  feat.GetPartial();
    // NOTE - have to use original seqfeat in order for this to work correctly 
    // for features on segmented sequences 
    partial_loc  = SeqLocPartialCheck(feat.GetOriginalSeq_feat()->GetLocation(), m_Scope );
    if (feat.IsSetProduct () && !feat.GetProduct().IsWhole()) {
        PostErr(eDiag_Info, eErr_SEQ_FEAT_ProductShouldBeWhole, 
            "Feature products should be entire sequences.", *(feat.GetSeq_feat()));
    }

    if (partial_loc  == eSeqlocPartial_Complete && !is_partial ) {
        return;
    }

    if (partial_loc & eSeqlocPartial_Nointernal) {
        x_ReportInternalPartial(*(feat.GetSeq_feat()));
    }

    if (partial_loc & eSeqlocPartial_Limwrong) {
        x_ReportImproperPartial(*(feat.GetSeq_feat()));
    }

    if (s_IsCDDFeat(feat)) {
        // no additional warnings
        return;
    }

    string except_text = "";
    bool no_nonconsensus_except = true;
    if (feat.IsSetExcept_text()) {
        except_text = feat.GetExcept_text();
        if (feat.IsSetExcept()) {
            if (NStr::Find (except_text, "nonconsensus splice site") != string::npos ||
                NStr::Find (except_text, "heterogeneous population sequenced") != string::npos ||
                NStr::Find (except_text, "low-quality sequence region") != string::npos ||
                NStr::Find (except_text, "artificial location") != string::npos) {
                no_nonconsensus_except = false;
            }
        }
    }

    string comment_text = "";
    if (feat.IsSetComment()) {
        comment_text = feat.GetComment();
    }

    if (is_complete && feat.GetData().Which() == CSeqFeatData::e_Cdregion) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
            "Partial CDS on complete sequence",
            *(feat.GetSeq_feat()));
    }

    // partial location
    unsigned int errtype = eSeqlocPartial_Nostart;
    for (int j = 0; j < 2; ++j) {
        if (partial_loc & errtype) {
            bool bad_seq = false;
            bool is_gap = false;
            bool abuts_n = false;

            if ( m_Scope && x_MatchesOverlappingFeaturePartial(feat, errtype)) {
                // error is suppressed
            } else if (m_Imp.x_IsFarFetchFailure(feat.GetLocation())) {
                m_Imp.SetFarFetchFailure();
            } else if (feat.GetData().IsCdregion() && 
                IsOrganelle(m_CurrentHandle) &&
                x_PartialAdjacentToIntron(feat.GetLocation())) {
                // suppress
            } else if ( x_IsPartialAtSpliceSiteOrGap(feat.GetLocation(), errtype, bad_seq, is_gap, abuts_n) ) {
                if (is_gap || m_Imp.GetGeneCache().IsPseudo(*(feat.GetOriginalSeq_feat()), *m_Scope)) {
                    // suppress for everything
                } else {
                    x_ReportStartStopPartialProblem(j, true, abuts_n, *(feat.GetSeq_feat()));
                }
            } else if ( bad_seq) {                
                PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                    (errtype == eSeqlocPartial_Nostart ?
                     "PartialLocation: Start does not include first/last residue of sequence (and is at bad sequence)" :
                     "PartialLocation: Stop does not include first/last residue of sequence (and is at bad sequence)"),
                    *(feat.GetSeq_feat()));
            } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion) {
                if (feat.IsSetExcept()
                    && NStr::Find(except_text, "rearrangement required for product") != string::npos) {
                    // suppress
                }
                else if (feat.IsSetComment() &&
                    NStr::Find(comment_text, "coding region disrupted by sequencing gap") != string::npos) {
                    // suppress
                } else if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                    // suppress
                } else if (!no_nonconsensus_except) {
                    // suppress
                } else if (s_PartialAtGapOrNs (m_Scope, feat.GetLocation(), errtype, true)) {                                
                    // suppress
                } else {
                    EDiagSev sev = eDiag_Warning;
                    if (abuts_n) {
                        sev = eDiag_Info;
                    }
                    if (j == 0) {
                        PostErr(sev, eErr_SEQ_FEAT_PartialProblemNotSpliceConsensus5Prime,
                            "5' partial is not at beginning of sequence, gap, or consensus splice site",
                            *(feat.GetSeq_feat()));
                    } else {
                        PostErr(sev, eErr_SEQ_FEAT_PartialProblemNotSpliceConsensus3Prime,
                            "3' partial is not at end of sequence, gap, or consensus splice site",
                            *(feat.GetSeq_feat()));
                    }                   
                }
            } else if ((feat.GetData().Which() == CSeqFeatData::e_Gene ||
                feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) &&
                x_IsSameAsCDS(feat)) {
                if (j == 0) {
                    PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem5Prime,
                        "Start does not include first/last residue of sequence", *(feat.GetSeq_feat()));
                } else if (j == 1) {
                    PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem3Prime,
                        "Stop does not include first/last residue of sequence", *(feat.GetSeq_feat()));
                }
            } else if (m_Imp.GetGeneCache().IsPseudo(*(feat.GetSeq_feat()), *m_Scope)) {
                // suppress
            } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA && 
                j == 0 && x_PartialAdjacentToIntron(feat.GetLocation())) {
                // suppress tRNAs adjacent to introns
            } else if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                // ignore start/stop not at end in genomic gpipe sequence
            } else {
                x_ReportStartStopPartialProblem(j, false, abuts_n, *(feat.GetSeq_feat()));
            }
        }
        errtype <<= 1;
    }
}


bool CValidError_bioseq::x_HasPGAPStructuredComment(CBioseq_Handle bsh)
{
    CSeqdesc_CI di(bsh, CSeqdesc::e_User);
    while (di) {
        if (di->GetUser().HasField("StructuredCommentPrefix")) {
            const CUser_field& field = di->GetUser().GetField("StructuredCommentPrefix");
            if (field.IsSetData() && field.GetData().IsStr() && NStr::EqualNocase(field.GetData().GetStr(), "##Genome-Annotation-Data-START##")) {
                return true;
            }
        }
        ++di;
    }
    return false;
}


void CValidError_bioseq::ValidateSeqFeatContext(
    const CBioseq& seq, bool is_complete)
{
    // test
    string accession = "";
    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        if ((*it)->IsGenbank()) {
            if ((*it)->GetGenbank().IsSetAccession()) {
                accession = (*it)->GetGenbank().GetAccession();
                break;
            }
        } else if ((*it)->IsDdbj()) {
            if ((*it)->GetDdbj().IsSetAccession()) {
                accession = (*it)->GetDdbj().GetAccession();
                break;
            }
        } else if ((*it)->IsGi()) {
            accession = NStr::NumericToString((*it)->GetGi());
        }
    }

    try {
        unsigned int nummrna = 0, numcds = 0, numcrgn = 0, numvseg = 0, numdseg = 0, numjseg = 0;
        int          numgene = 0, num_pseudomrna = 0, num_pseudocds = 0, num_rearrangedcds = 0;
        vector< CConstRef < CSeq_id > > cds_products, mrna_products;
        
        int num_full_length_prot_ref = 0;

        bool is_mrna = IsMrna(m_CurrentHandle);
        bool is_aa = CSeq_inst::IsAa(m_CurrentHandle.GetInst_Mol());
        bool is_virtual = (m_CurrentHandle.GetInst_Repr() == CSeq_inst::eRepr_virtual);
        TSeqPos len = m_CurrentHandle.IsSetInst_Length() ? m_CurrentHandle.GetInst_Length() : 0;

        bool is_emb = false, non_pseudo_16S_rRNA = false;
        bool is_refseq = m_Imp.IsRefSeqConventions();
        FOR_EACH_SEQID_ON_BIOSEQ (seq_it, seq) {
            if ((*seq_it)->IsEmbl()) {
                is_emb = true;
            } else if ((*seq_it)->IsOther()) {
                is_refseq = true;
            }
        }

        int firstcdsgencode = 0;
        bool mixedcdsgencodes = false;

        if (m_AllFeatIt) {
            ITERATE(CCacheImpl::TFeatValue, fi, *m_AllFeatIt) {
                const CSeq_feat& feat = fi->GetOriginalFeature();
                
                CSeqFeatData::E_Choice ftype = feat.GetData().Which();
                CSeqFeatData::ESubtype subtype = fi->GetFeatSubtype();

                if (ftype == CSeqFeatData::e_Gene) {
                    numgene++;
                    const CGene_ref& gene_ref = feat.GetData().GetGene();
                    if (gene_ref.IsSetLocus()) {
                        string locus = gene_ref.GetLocus();
                        if (m_GeneIt) {
                            ITERATE(CCacheImpl::TFeatValue, gene_it, *m_GeneIt) {
                                const CSeq_feat& gene_feat = gene_it->GetOriginalFeature();
                                const CGene_ref& other_gene_ref = gene_feat.GetData().GetGene();
                                if (other_gene_ref.IsSetLocus_tag() 
                                    && NStr::EqualCase (other_gene_ref.GetLocus_tag(), locus)
                                    && (!other_gene_ref.IsSetLocus() 
                                    || !NStr::EqualCase(other_gene_ref.GetLocus(), locus))) {
                                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneLocusCollidesWithLocusTag,
                                        "locus collides with locus_tag in another gene", feat);
                                }
                            }
                        }
                    }
                } else if (feat.GetData().IsCdregion()) {
                    numcds++;
                    if (feat.IsSetProduct()) {
                        const CSeq_id* p = feat.GetProduct().GetId();
                        CConstRef<CSeq_id> ref(p);
                        cds_products.push_back (ref);
                    } else if (feat.IsSetPseudo() && feat.GetPseudo()) {
                            num_pseudocds++;
                    } else {
                        CConstRef<CSeq_feat> gene = m_Imp.GetCachedGene(&feat);
                        if (gene != 0 && gene->IsSetPseudo() && gene->GetPseudo()) {
                            num_pseudocds++;
                        } else if (feat.IsSetExcept_text() 
                                   && NStr::Find (feat.GetExcept_text(), "rearrangement required for product") != string::npos) {
                            num_rearrangedcds++;
                        }
                    }
                    ValidateBadGeneOverlap (feat);

                    const CCdregion& cdregion = feat.GetData().GetCdregion();
                    if (cdregion.IsSetCode()) {
                        int cdsgencode = 0;
                        ITERATE(CCdregion::TCode::Tdata, it, cdregion.GetCode().Get()) {
                            if ((*it)->IsId()) {
                                cdsgencode = (*it)->GetId();
                            }
                        }
                        if (cdsgencode != 0) {
                            if (firstcdsgencode == 0) {
                                firstcdsgencode = cdsgencode;
                            } else if (firstcdsgencode != cdsgencode) {
                                mixedcdsgencodes = true;
                            }
                        }
                    }
                } else if (fi->GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA) {
                    nummrna++;
                    CConstRef <CSeq_feat> gene = m_Imp.GetCachedGene(&feat);
                    if (feat.IsSetProduct()) {
                        const CSeq_id* p = feat.GetProduct().GetId();
                        CConstRef<CSeq_id> ref(p);
                        mrna_products.push_back (ref);
                    } else if (feat.IsSetPseudo() && feat.GetPseudo()) {
                            num_pseudomrna++;
                    } else {
                        if (gene != 0 && gene->IsSetPseudo() && gene->GetPseudo()) {
                            num_pseudomrna++;
                        }
                    }                
                    ValidateBadGeneOverlap(feat);

                } else if (fi->GetFeatSubtype() == CSeqFeatData::eSubtype_rRNA) {
                    if (! feat.IsSetPseudo() || ! feat.GetPseudo()) {
                        const CRNA_ref& rref = feat.GetData().GetRna();
                        if (rref.CanGetExt() && rref.GetExt().IsName()) {
                            const string& rna_name = rref.GetExt().GetName();
                            if (NStr::EqualNocase(rna_name, "16S ribosomal RNA")) {
                                non_pseudo_16S_rRNA = true;
                            }
                        }
                    }
                } else if (subtype == CSeqFeatData::eSubtype_C_region) {
                    numcrgn++;
                } else if (subtype == CSeqFeatData::eSubtype_V_segment) {
                    numvseg++;
                } else if (subtype == CSeqFeatData::eSubtype_D_segment) {
                    numdseg++;
                } else if (subtype == CSeqFeatData::eSubtype_J_segment) {
                    numjseg++;
                }

                m_FeatValidator.ValidateSeqFeatContext(feat, seq);
                
                if (seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
                    ValidateFeatPartialInContext (*fi, is_complete);
                }

                if ( is_aa ) {                // protein
                    switch ( ftype ) {
                    case CSeqFeatData::e_Prot:
                        {
                            if (IsOneBioseq(feat.GetLocation(), m_Scope)) {
                                CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
                            
                                if ( (range.IsWhole()  ||
                                      (range.GetFrom() == 0  &&  range.GetTo() == len - 1)) &&
                                     (!feat.GetData().GetProt().IsSetProcessed() ||
                                      (feat.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_not_set) ||
                                      (feat.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_preprotein))){
                                     num_full_length_prot_ref++;
                                }
                            }
                        }
                        break;
                        
                    case CSeqFeatData::e_Gene:
                        if (x_HasPGAPStructuredComment(m_CurrentHandle)) {
                            if (feat.GetData().GetGene().IsSetLocus_tag()) {
                                PostErr(eDiag_Error, eErr_SEQ_FEAT_LocusTagProblem,
                                    "Genes on protein sequences with PGAP annotation should not have locus tags.", feat);
                            }
                        } else if (!s_StandaloneProt(m_CurrentHandle)) {
                            // report only if NOT standalone protein
                            // and NOT PGAP
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFeatureForProtein,
                                "Invalid feature for a protein Bioseq.", feat);
                        }
                        break;

                    default:
                        break;
                    }
                }
 
                if ( is_mrna ) {              // mRNA
                    switch ( ftype ) {
                    case CSeqFeatData::e_Cdregion:
                    {{
                        // Test for Multi interval CDS feature
                        if ( NumOfIntervals(feat.GetLocation()) > 1 ) {
                            bool excpet = feat.IsSetExcept()  &&  feat.GetExcept();
                            bool slippage_except = false;
                            if ( feat.IsSetExcept_text() ) {
                                const string& text = feat.GetExcept_text();
                                slippage_except = 
                                    NStr::FindNoCase(text, "ribosomal slippage") != NPOS;
                            }
                            if ( !excpet  ||  !slippage_except ) {
                                EDiagSev sev = is_refseq ? eDiag_Warning : eDiag_Error;
                                PostErr(sev, eErr_SEQ_FEAT_InvalidForType,
                                    "Multi-interval CDS feature is invalid on an mRNA "
                                    "(cDNA) Bioseq.",
                                    feat);
                            }
                        }
                        break;
                    }}    
                    case CSeqFeatData::e_Rna:
                    {{
                        const CRNA_ref& rref = feat.GetData().GetRna();
                        if ( rref.GetType() == CRNA_ref::eType_mRNA ) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFeatureForMRNA,
                                "mRNA feature is invalid on an mRNA (cDNA) Bioseq.",
                                feat);
                        }
                        
                        break;
                    }}
                    case CSeqFeatData::e_Imp:
                    {{
                        const CImp_feat& imp = feat.GetData().GetImp();
                        if ( imp.GetKey() == "intron") {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                                "Invalid feature for an mRNA Bioseq.", feat);
                        }
                        break;
                    }}
                    default:
                        break;
                    }
                }    

                if (!is_emb) {
                    if (IsFarLocation(feat.GetLocation(), m_Imp.GetTSEH())) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_FarLocation,
                            "Feature has 'far' location - accession not packaged in record",
                            feat);
                    }
                }

            }  // end of for loop

            if (non_pseudo_16S_rRNA && m_CurrentHandle) {
                CSeqdesc_CI src_desc(m_CurrentHandle, CSeqdesc::e_Source);
                if ( src_desc ) {
                    const CBioSource& biosrc = src_desc->GetSource();
                    int genome = 0;
                    bool isEukaryote = false;
                    bool isMicrosporidia = false;
                    if ( biosrc.IsSetGenome() ) {
                        genome = biosrc.GetGenome();
                    }
                    if ( biosrc.IsSetLineage() ) {
                        string lineage = biosrc.GetLineage();
                        if (NStr::StartsWith(lineage, "Eukaryota; ", NStr::eNocase)) {
                            isEukaryote = true;
                            if (NStr::StartsWith(lineage, "Eukaryota; Fungi; Microsporidia; ", NStr::eNocase)) {
                                isMicrosporidia = true;
                            }
                        }
                    }
                    if (isEukaryote && (! isMicrosporidia) &&
                        genome != CBioSource::eGenome_mitochondrion &&
                        genome != CBioSource::eGenome_chloroplast &&
                        genome != CBioSource::eGenome_chromoplast &&
                        genome != CBioSource::eGenome_kinetoplast &&
                        genome != CBioSource::eGenome_plastid &&
                        genome != CBioSource::eGenome_apicoplast &&
                        genome != CBioSource::eGenome_leucoplast &&
                        genome != CBioSource::eGenome_proplastid &&
                        genome != CBioSource::eGenome_chromatophore) {
                        PostErr(eDiag_Error, eErr_SEQ_DESCR_WrongOrganismFor16SrRNA,
                            "Improper 16S ribosomal RNA",
                            *(seq.GetParentEntry()), *src_desc);
                    }
                }
            }
        } // end of branch where features are present

        if (mixedcdsgencodes) {
            EDiagSev sev = eDiag_Error;
            if (IsSynthetic(seq)) {
                sev = eDiag_Warning;
            }
            PostErr (sev, eErr_SEQ_FEAT_MultipleGenCodes,
                     "Multiple CDS genetic codes on sequence", seq);
        }

        // if no full length prot feature on a part of a segmented bioseq
        // search for such feature on the master bioseq
        if ( is_aa  &&  num_full_length_prot_ref == 0) {
            CBioseq_Handle parent = s_GetParent(m_CurrentHandle);
            if ( parent ) {
                TSeqPos parent_len = 0;
                if (parent.IsSetInst() && parent.GetInst().IsSetLength()) {
                    parent_len = parent.GetInst().GetLength();
                }
                for ( CFeat_CI it(parent, CSeqFeatData::e_Prot); it; ++it ) {
                    try {
                        const CSeq_feat& prot_feat = it->GetOriginalFeature();
                        CSeq_loc::TRange range = prot_feat.GetLocation().GetTotalRange();
                        
                        if ( (range.IsWhole()  ||
                              (range.GetFrom() == 0  &&  range.GetTo() == parent_len - 1) ) &&
                             (!prot_feat.GetData().GetProt().IsSetProcessed() ||
                              (prot_feat.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_not_set) ||
                              prot_feat.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_preprotein)){
                            num_full_length_prot_ref ++;
                        }
                    } catch ( const exception& ) {
                        CSeq_loc::TRange range = it->GetLocation().GetTotalRange();
                        if ( (range.IsWhole()  ||
                              (range.GetFrom() == 0  &&  range.GetTo() == parent_len - 1) ) &&
                             (!it->GetData().GetProt().IsSetProcessed() ||
                              (it->GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_not_set ||
                              it->GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_preprotein))){
                            num_full_length_prot_ref ++;
                        }
                    }
                }
            }
        }

        if ( is_aa  &&  num_full_length_prot_ref == 0  &&  !is_virtual  &&  !m_Imp.IsPDB()   ) {
            m_Imp.AddProtWithoutFullRef(m_CurrentHandle);
        }

        if ( is_aa && num_full_length_prot_ref > 1 && !SeqIsPatent (seq)) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultipleProtRefs, 
                     NStr::IntToString (num_full_length_prot_ref)
                     + " full-length protein features present on protein", seq);
        }

        
        if ( !is_aa ) {
            // validate abutting UTRs for nucleotides
            x_ValidateAbuttingUTR(m_CurrentHandle);

            // validate coding regions between UTRs
            ValidateCDSUTR(seq);
        }

        // validate abutting RNA features
        x_ValidateAbuttingRNA (m_CurrentHandle);

        // before validating CDS/mRNA matches, determine whether to suppress duplicate messages
        bool cds_products_unique = true;
        if (cds_products.size() > 1) {
            stable_sort (cds_products.begin(), cds_products.end(), s_SeqIdCompare);
            cds_products_unique = seq_mac_is_unique (cds_products.begin(), cds_products.end(), s_SeqIdMatch);
        }

        bool mrna_products_unique = true;
        if (mrna_products.size() > 1) {
            stable_sort (mrna_products.begin(), mrna_products.end(), s_SeqIdCompare);
            mrna_products_unique = seq_mac_is_unique (mrna_products.begin(), mrna_products.end(), s_SeqIdMatch);
        }

        if (numcds > 0 && nummrna > 1) {
            if (cds_products.size() > 0 && cds_products.size() + num_pseudocds + num_rearrangedcds != numcds) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureProductInconsistency, 
                         NStr::SizetToString (numcds) + " CDS features have "
                         + NStr::SizetToString (cds_products.size()) + " product references",
                        seq);
            }
            if (cds_products.size() > 0 && (! cds_products_unique)) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureProductInconsistency, 
                         "CDS products are not unique", seq);
            }
            if (mrna_products.size() > 0 && mrna_products.size() + num_pseudomrna != nummrna) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureProductInconsistency, 
                         NStr::SizetToString (nummrna) + " mRNA features have "
                         + NStr::SizetToString (mrna_products.size()) + " product references",
                         seq);
            }
            if (mrna_products.size() > 0 && (! mrna_products_unique)) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureProductInconsistency, 
                         "mRNA products are not unique", seq);
            }
        }

        x_ValidateCDSmRNAmatch(
            m_CurrentHandle, numgene, numcds, nummrna);

        if (numcds > 0 && numcrgn + numvseg + numdseg + numjseg > 0 && m_Imp.DoCompareVDJCtoCDS() && m_Imp.IsRefSeq()) {
            /*
            LOG_POST_XX(Corelib_App, 1, "numcds: " + NStr::IntToString(numcds) + "\n");
            LOG_POST_XX(Corelib_App, 1, "nummrna: " + NStr::IntToString(nummrna) + "\n");
            LOG_POST_XX(Corelib_App, 1, "numcrgn: " + NStr::IntToString(numcrgn) + "\n");
            LOG_POST_XX(Corelib_App, 1, "numvseg: " + NStr::IntToString(numvseg) + "\n");
            LOG_POST_XX(Corelib_App, 1, "numdseg: " + NStr::IntToString(numdseg) + "\n");
            LOG_POST_XX(Corelib_App, 1, "numjseg: " + NStr::IntToString(numjseg) + "\n");
            */
            // x_ValidateCDSVDJCmatch(m_CurrentHandle, numcds, numcrgn, numvseg, numdseg, numjseg);
            x_ValidateCDSagainstVDJC(m_CurrentHandle);
        }

        if (!SeqIsPatent(seq)) {
            ValidateMultipleGeneOverlap (m_CurrentHandle);
        }

    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating Seqfeat Context. EXCEPTION: ") +
                e.what(), seq);
        }
    }

}


TGi GetGIForSeqId(const CSeq_id& id, CScope& scope)
{
    if (id.IsGi()) {
        return id.GetGi();
    }
    CBioseq_Handle bsh = scope.GetBioseqHandle(id);
    if (!bsh) {
        return ZERO_GI;
    }
    ITERATE(CBioseq::TId, id_it, bsh.GetBioseqCore()->GetId()) {
        if ((*id_it)->IsGi()) {
            return (*id_it)->GetGi();
        }
    }
    return ZERO_GI;
}


string s_GetMrnaProteinLink(const CUser_field& field)
{
    string ml = kEmptyStr;
    if (field.IsSetLabel() && field.GetLabel().IsStr() &&
        NStr::Equal(field.GetLabel().GetStr(), "protein seqID") &&
        field.IsSetData() && field.GetData().IsStr()) {
        ml = field.GetData().GetStr();
    }
    return ml;
}


string s_GetMrnaProteinLink(const CUser_object& user)
{
    string ml = kEmptyStr;
    if (user.IsSetType() && user.GetType().IsStr() &&
        NStr::Equal(user.GetType().GetStr(), "MrnaProteinLink") &&
        user.IsSetData()) {
        ITERATE(CUser_object::TData, it, user.GetData()) {
            ml = s_GetMrnaProteinLink(**it);
            if (!NStr::IsBlank(ml)) {
                break;
            }
        }
    }
    return ml;
}


string s_GetMrnaProteinLink(const CSeq_feat &mrna)
{
    string ml = kEmptyStr;

    if (mrna.IsSetExt()) {
        ml = s_GetMrnaProteinLink(mrna.GetExt());
    }
    return ml;

}


unsigned int CValidError_bioseq::x_IdXrefsNotReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return 0;
    }

    bool match1 = false, match2 = false;
    bool has1 = false, has2 = false;
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId()) {
            has1 = true;
            if (s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
                match1 = true;
            }
        }
    }

            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId()) {
            has2 = true;
            if (s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
                match2 = true;
            }
        }
    }

    if ((has1 || has2) && (!match1 || !match2)) {
        return 1;
    }

    if (!cds.IsSetProduct() || !mrna.IsSetExt()) {
        return 0;
    }
    
    TGi gi = GetGIForSeqId(*(cds.GetProduct().GetId()), *m_Scope);

    if (gi == ZERO_GI) {
        return 0;
    }

    string ml = s_GetMrnaProteinLink(mrna);
    if (!NStr::IsBlank(ml)) {
        try {
            CSeq_id id (ml);
            if (id.IsGi()) {
                if (id.GetGi() == gi) {
                    return 0;
                } else {
                    return 2;
                }
            }
        } catch (CException ) {
            return 2;
        } catch (std::exception ) {
            return 2;
        }
    }
    return 0;
    
}

bool s_IdXrefsAreReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return false;
    }

    bool match = false;

    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
            match = true;
            break;
        }
    }
    if (!match) {
        return false;
    }
    match = false;
            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
            match = true;
            break;
        }
    }

    return match;
}

bool CValidError_bioseq::x_IdXrefsAreReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna)
{
    return s_IdXrefsAreReciprocal(cds, mrna);
}

CMrnaMatchInfo::CMrnaMatchInfo(const CSeq_feat& mrna,
                               CScope* scope) :
    m_Mrna(&mrna),
    m_Scope(scope),
    m_HasMatch(false),
    m_IsPseudo(false)
{
}

const CSeq_feat& CMrnaMatchInfo::GetSeqfeat(void) const 
{
    return *m_Mrna;
}

bool CMrnaMatchInfo::Overlaps(const CSeq_feat& cds) const 
{
    EOverlapType overlap_type = eOverlap_CheckIntRev;

    if (cds.IsSetExcept_text() &&
       (NStr::FindNoCase(cds.GetExcept_text(), "ribosomal slippage") != string::npos))
    {
        overlap_type = eOverlap_SubsetRev;
    }
    return (TestForOverlapEx(cds.GetLocation(), m_Mrna->GetLocation(), overlap_type, m_Scope) >= 0);
}

void CMrnaMatchInfo::SetMatch() {
    m_HasMatch = true;
}


bool CMrnaMatchInfo::HasMatch(void) const {
    return m_HasMatch;
}


bool CMrnaMatchInfo::OkWithoutCds(void) const
{
    if (m_IsPseudo) return true;
    const CSeq_loc& loc = m_Mrna->GetLocation();
    TSeqPos len = GetLength(loc, m_Scope);
    if (len < 6) {
        return true;
    }
    TSeqPos mrna_start = loc.GetStart(eExtreme_Biological);
    TSeqPos mrna_stop = loc.GetStop(eExtreme_Biological);
    TFeatScores utr5s;
    GetOverlappingFeatures(loc, CSeqFeatData::e_Imp,
        CSeqFeatData::eSubtype_5UTR, eOverlap_Contains, utr5s, *m_Scope);
    ITERATE(TFeatScores, s, utr5s) {
        const CSeq_loc& utr5_loc = s->second->GetLocation();
        TSeqPos utr5_start = utr5_loc.GetStart(eExtreme_Biological);
        TSeqPos utr5_stop = utr5_loc.GetStop(eExtreme_Biological);
        if (utr5_start == mrna_start) {
            if (mrna_stop >= utr5_stop && mrna_stop - utr5_stop < 6) {
                return true;
            } else if (utr5_stop >= mrna_stop && utr5_stop - mrna_stop < 6) {
                return true;
            }
        }
    }
    TFeatScores utr3s;
    GetOverlappingFeatures(loc, CSeqFeatData::e_Imp,
        CSeqFeatData::eSubtype_3UTR, eOverlap_Contains, utr3s, *m_Scope);
    ITERATE(TFeatScores, s, utr3s) {
        const CSeq_loc& utr3_loc = s->second->GetLocation();
        TSeqPos utr3_start = utr3_loc.GetStart(eExtreme_Biological);
        TSeqPos utr3_stop = utr3_loc.GetStop(eExtreme_Biological);
        if (utr3_stop == mrna_stop) {
            if (mrna_start >= utr3_start && mrna_start - utr3_start < 6) {
                return true;
            } else if (utr3_start >= mrna_start && utr3_start - mrna_start < 6) {
                return true;
            }
        }
    }
    return false;
}


CCdsMatchInfo::CCdsMatchInfo(const CSeq_feat& cds,
                             CScope* scope) :
    m_Cds(&cds),
    m_Scope(scope),
    m_IsPseudo(false),
    m_NeedsMatch(true),
    m_ProductsUnique(true)
{
    m_OverlapType = eOverlap_CheckIntRev;
    if (m_Cds->IsSetExcept_text() &&
       (NStr::FindNoCase(m_Cds->GetExcept_text(), "ribosomal slippage") != string::npos))
    {
        m_OverlapType = eOverlap_SubsetRev;
    }
    m_OtherMrnas.clear();
}


const CSeq_feat& CCdsMatchInfo::GetSeqfeat(void) const {
    return *m_Cds;
}


bool CCdsMatchInfo::Overlaps(const CSeq_feat& mrna) const 
{
    if (m_Cds.IsNull()) {
        return false;
    }
    return (TestForOverlapEx(m_Cds->GetLocation(), mrna.GetLocation(), m_OverlapType, m_Scope) >= 0);
}


void CCdsMatchInfo::NeedsMatch(const bool needs_match) {
    m_NeedsMatch = needs_match;
}


bool CCdsMatchInfo::NeedsMatch(void) const {
    return m_NeedsMatch;
}


bool CCdsMatchInfo::HasMatch(void) const {
    return  (m_BestMatch != nullptr);
}

const CMrnaMatchInfo& CCdsMatchInfo::GetMatch(void) const {
    return *m_BestMatch;
}


bool CCdsMatchInfo::IsPseudo(void) const {
    return m_IsPseudo;
}


void CCdsMatchInfo::SetPseudo(void) {
    m_IsPseudo = true;
}

bool CCdsMatchInfo::AssignXrefMatch(TmRNAList& unmatched_mrnas, const CTSE_Handle& tse)
{
    if (unmatched_mrnas.empty()) {
        return false;
    }
    bool rval = false;
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT(xref_it, *m_Cds) {
        if (!(*xref_it)->IsSetId() ||
            !(*xref_it)->GetId().IsLocal()) {
            continue;
        }
        vector<CSeq_feat_Handle> handles = tse.GetFeaturesWithId(CSeqFeatData::eSubtype_mRNA, (*xref_it)->GetId().GetLocal());
        ITERATE(vector<CSeq_feat_Handle>, h, handles) {
            if (HasMatch()) {
                m_OtherMrnas.push_back(h->GetSeq_feat());
            } else {
                TmRNAList::iterator mrna_it = unmatched_mrnas.find((*h).GetSeq_feat());
                if (mrna_it != unmatched_mrnas.end()) {
                    m_BestMatch = mrna_it->second;
                    m_BestMatch->SetMatch();
                    unmatched_mrnas.erase(mrna_it);
                    rval = true;
                }
            }
        }
    }
    return rval;
}


bool CCdsMatchInfo::AssignOverlapMatch(TmRNAList& unmatched_mrnas, CScope& scope)
{
    if (unmatched_mrnas.empty()) {
        return false;
    }

    bool rval = false;
    TFeatScores feats;
    GetOverlappingFeatures(m_Cds->GetLocation(), CSeqFeatData::e_Rna, CSeqFeatData::eSubtype_mRNA,
                           m_OverlapType, feats, scope);
    if (feats.size() == 0) {
        TmRNAList::iterator mrna_it = unmatched_mrnas.begin();
        while (mrna_it != unmatched_mrnas.end()) {
            if (Overlaps(mrna_it->second->GetSeqfeat())) {
                m_BestMatch = mrna_it->second;
                m_BestMatch->SetMatch();
                unmatched_mrnas.erase(mrna_it);
                return true;
            }
            ++mrna_it;
        }
    } else {
        NON_CONST_ITERATE(TFeatScores, it, feats) {
            TmRNAList::iterator mrna_it = unmatched_mrnas.find(it->second);
            if (mrna_it != unmatched_mrnas.end()) {
                if (rval) {
                    m_OtherMrnas.push_back(it->second);
                } else {
                    m_BestMatch = mrna_it->second;
                    m_BestMatch->SetMatch();
                    unmatched_mrnas.erase(mrna_it);
                    rval = true;
                }
            }
        }
    }
    
    return rval;
}


string s_GetMrnaProductString(const CSeq_feat& mrna)
{
    string product_string = "";

    if (!mrna.IsSetProduct()) {
        return product_string;
    }

    mrna.GetProduct().GetLabel(&product_string);

    return product_string;
}


void CCdsMatchInfo::UpdateOtherMrnas(const TmRNAList& unmatched_mrnas)
{
    list<CConstRef<CSeq_feat> >::iterator it = m_OtherMrnas.begin();
    list<string> product_strings;
    while (it != m_OtherMrnas.end()) {
        TmRNAList::const_iterator mrna_it = unmatched_mrnas.find(it->GetPointer());
        if (mrna_it == unmatched_mrnas.end()) {
            it = m_OtherMrnas.erase(it);
        } else {
            auto product_string = s_GetMrnaProductString(**it);
            product_strings.push_back(product_string);
            ++it;
        }
    }
    if (m_OtherMrnas.size() == 0) {
        return;
    }
    m_OtherMrnas.sort();
    m_OtherMrnas.unique();
    product_strings.push_back(s_GetMrnaProductString(m_BestMatch->GetSeqfeat()));
    const auto num_products = product_strings.size();
    if (product_strings.size() > 1) {
        m_ProductsUnique = false;
        product_strings.sort();
        product_strings.unique();
        const auto num_unique_products = product_strings.size();
        if (num_unique_products == num_products) {
            m_ProductsUnique = true;
        }
    }
}


void CCdsMatchInfo::SetMatch(CRef<CMrnaMatchInfo> match)
{
    m_BestMatch = match;
    m_BestMatch->SetMatch();
}


bool CCdsMatchInfo::AssignMatch(TmRNAList& mrna_map, CFeatTree& feat_tree, CScope& scope)
{
    CSeq_feat_Handle cds = scope.GetSeq_featHandle(*m_Cds);
    CMappedFeat mrna = feat_tree.GetParent(cds, CSeqFeatData::eSubtype_mRNA);
    if (mrna) {
        const CSeq_feat* key = mrna.GetSeq_feat();
        CRef<CMrnaMatchInfo> match = mrna_map[key];
        bool rval = false;
        if (match) {
            SetMatch(match);
            rval = true;
        }
        mrna_map.erase(key);
        return rval;
    } else {
        return false;
    }
}


void CValidError_bioseq::x_CheckForMultiplemRNAs(CCdsMatchInfo& cds_match,
    const TmRNAList& unmatched_mrnas)
{
    if (!cds_match.HasMatch()) {
        return;
    }
    cds_match.UpdateOtherMrnas(unmatched_mrnas);
    size_t num_mrnas = cds_match.CountOtherMrnas() + 1;
    if (num_mrnas < 2) {
        return;
    }
    if (cds_match.AreMrnaProductsUnique()) {
        PostErr (eDiag_Info, eErr_SEQ_FEAT_CDSwithMultipleMRNAs, 
            "CDS matches " + NStr::NumericToString(num_mrnas)
            + " mRNAs, but product locations are unique",
            cds_match.GetSeqfeat());
        return;
    } else {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSwithMultipleMRNAs,
            "CDS matches " + NStr::NumericToString(num_mrnas)
            + " mRNAs",
            cds_match.GetSeqfeat());
    }
}


void CValidError_bioseq::x_CheckMrnaProteinLink(const CCdsMatchInfo& cds_match)
{
    if (!cds_match.HasMatch()) {
        return;
    }

    const auto& mrna_feat = cds_match.GetMatch().GetSeqfeat();
    const auto& cds_feat = cds_match.GetSeqfeat();

    const auto xrefs_match = x_IdXrefsNotReciprocal(cds_feat, mrna_feat);

    // Could also check that xrefs are reciprocal here, but that is checked in CValidError_feat
    if (xrefs_match == 2) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem, 
            "MrnaProteinLink inconsistent with feature ID cross-references",
            mrna_feat);
    }
}


bool s_GeneralTagsMatch(const string& protein_id, const CDbtag& dbtag)
{
    size_t start_pos = NStr::Find(protein_id, "gnl|");
    if (start_pos == string::npos) {
        return false;
    }
    start_pos = NStr::Find(protein_id, "|", start_pos + 5);
    if (start_pos == string::npos) {
        return false;
    }
    size_t end_pos = NStr::Find(protein_id, "|", start_pos + 1);
    string prot_tag = kEmptyStr;
    if (end_pos == string::npos) {
        prot_tag = protein_id.substr(start_pos + 1);
    } else {
        prot_tag = protein_id.substr(start_pos + 1, end_pos - start_pos - 1);
    }
    
    if (dbtag.IsSetTag()) {
        if (dbtag.GetTag().IsStr()) {
            if (NStr::Equal(dbtag.GetTag().GetStr(), prot_tag)) {
                return true;
            }
        }
        else if (dbtag.GetTag().IsId()) {
            if (NStr::Equal(NStr::NumericToString(dbtag.GetTag().GetId()), prot_tag)) {
                return true;
            }
        }
    }
    return false;
}

void CValidError_bioseq::x_TranscriptIDsMatch(const string& protein_id, const CSeq_feat& cds)
{
    if (!cds.IsSetProduct() || !cds.GetProduct().GetId()) {
        if (!sequence::IsPseudo(cds, *m_Scope)) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAMissingProteinIDs,
                "CDS-mRNA pair has one missing protein_id (" + protein_id + ")", cds);
        }
        return;
    }
    const CSeq_id& product_id = *(cds.GetProduct().GetId());
    if (product_id.IsGeneral()) {
        if (!s_GeneralTagsMatch(protein_id, product_id.GetGeneral())) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAMismatchProteinIDs,
                "CDS-mRNA pair has mismatching protein_ids (" +
                product_id.AsFastaString() + ", " + protein_id + ")", cds);
        }
        return;
    }
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(product_id);
    if (bsh) {
        ITERATE(CBioseq::TId, id_it, bsh.GetBioseqCore()->GetId()) {
            if ((*id_it)->IsGeneral()) {
                if (!s_GeneralTagsMatch(protein_id, (*id_it)->GetGeneral())) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAMismatchProteinIDs,
                        "CDS-mRNA pair has mismatching protein_ids (" +
                        (*id_it)->AsFastaString() + ", " + protein_id + ")", cds);                   
                }
                return;
            }
        }
    }
    if (m_Imp.IsRefSeq()) {
        // per VR-760, do not report if RefSeq and no general tag
        return;
    }
    
    // no general tags, try plain match
    if (bsh) {
        ITERATE(CBioseq::TId, id_it, bsh.GetBioseqCore()->GetId()) {
            if (NStr::Equal(protein_id, (*id_it)->AsFastaString())) {
                return;
            }
        }
    } else if (NStr::Equal(protein_id, product_id.AsFastaString())) {
        return;
    }

    PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAMissingProteinIDs,
        "CDS-mRNA pair has one missing protein_id (" + protein_id + ")", cds);
}


void CValidError_bioseq::x_CheckOrigProteinAndTranscriptIds(const CCdsMatchInfo& cds_match)
{
    if (!cds_match.HasMatch()) {
        return;
    }
    const auto& mrna_feat = cds_match.GetMatch().GetSeqfeat();
    const auto& cds_feat = cds_match.GetSeqfeat();
    string cds_transcript_id = kEmptyStr;
    string mrna_transcript_id = kEmptyStr;
    string mrna_protein_id = kEmptyStr;
    bool must_reconcile = false;
    if (mrna_feat.IsSetQual()) {
        ITERATE(CSeq_feat::TQual, q, mrna_feat.GetQual()) {
            if ((*q)->IsSetQual() && (*q)->IsSetVal()) {
                if (NStr::EqualNocase((*q)->GetQual(), "orig_transcript_id")) {
                    mrna_transcript_id = (*q)->GetVal();
                    must_reconcile = true;
                } else if (NStr::EqualNocase((*q)->GetQual(), "orig_protein_id")) {
                    mrna_protein_id = (*q)->GetVal();
                    must_reconcile = true;
                }
            }
        }
    }
    if (cds_feat.IsSetQual()) {
        ITERATE(CSeq_feat::TQual, q, cds_feat.GetQual()) {
            if ((*q)->IsSetQual() && (*q)->IsSetVal()) {
                if (NStr::EqualNocase((*q)->GetQual(), "orig_transcript_id")) {
                    cds_transcript_id = (*q)->GetVal();
                    must_reconcile = true;
                }
            }
        }
    }

    if (must_reconcile) {
        if (!NStr::Equal(mrna_transcript_id, cds_transcript_id)) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAMismatchTranscriptIDs,
                "CDS-mRNA pair has mismatching transcript_ids ("
                + cds_transcript_id + "," + mrna_transcript_id + ")", 
                cds_feat);
        }
        x_TranscriptIDsMatch(mrna_protein_id, cds_feat);
    }

}


static bool x_FeatIsCDS(const CSeq_feat& ft)

{
    if (ft.IsSetData()) {
        CSeqFeatData::ESubtype sbt = ft.GetData().GetSubtype();
        if (sbt == CSeqFeatData::eSubtype_cdregion) {
            return true;
        }
    }
    return false;
}

static bool x_FeatIsVDJC(const CSeq_feat& ft)

{
    if (ft.IsSetData()) {
        CSeqFeatData::ESubtype sbt = ft.GetData().GetSubtype();
        if (sbt == CSeqFeatData::eSubtype_C_region ||
            sbt == CSeqFeatData::eSubtype_V_segment ||
            sbt == CSeqFeatData::eSubtype_D_segment ||
            sbt == CSeqFeatData::eSubtype_J_segment) {
            return true;
        }
    }
    return false;
}

static bool x_BadCDSinVDJC(const CSeq_loc& cdsloc, const CSeq_loc& vdjcloc, CScope* scope)

{
    if (s_OverlapOrAbut(vdjcloc, cdsloc, scope)) {
        if (! s_CheckIntervals(vdjcloc, cdsloc, scope)) {
            return true;
        }
    }
    return false;
}

bool x_IsPseudo(const CGene_ref& ref)
{
    if (ref.IsSetPseudo() && ref.GetPseudo()) {
        return true;
    } else {
        return false;
    }
}

bool x_HasNamedQual(const CSeq_feat& feat, const string& qual)
{
    bool rval = false;
    if (feat.IsSetQual()) {
        for (auto it : feat.GetQual()) {
            if (it->IsSetQual() && NStr::EqualNocase(it->GetQual(), qual)) {
                rval = true;
                break;
            }
        }
    }
    return rval;
}

bool x_IsPseudo(const CSeq_feat& feat, CValidError_imp& imp)
{
    if (feat.IsSetPseudo() && feat.GetPseudo()) {
        return true;
    } else if (x_HasNamedQual(feat, "pseudogene")) {
        return true;
    } else if (feat.IsSetData() && feat.GetData().IsGene() &&
        x_IsPseudo(feat.GetData().GetGene())) {
        return true;
    } else {
        try {
            CConstRef<CSeq_feat> gene = imp.GetCachedGene(&feat);
            if (gene) {
                if (gene->IsSetPseudo() && gene->GetPseudo()) {
                    return true;
                }
                if (gene->IsSetData()) {
                    const CSeqFeatData& data = gene->GetData();
                    if (data.IsGene()) {
                        const CGene_ref& ref = data.GetGene();
                        if (x_IsPseudo(ref)) {
                            return true;
                        }
                    }
                }
            }
        } catch (...) {
        }
    }
    return false;
}

void CValidError_bioseq::x_ValidateCDSagainstVDJC(const CBioseq_Handle& seq)
{
    SAnnotSelector sel;

    CFeat_CI it1(seq, sel);

    if (! it1) {
        return;
    }

    while (it1) {
        const CSeq_feat& ft1 = it1->GetOriginalFeature();
        if ( x_FeatIsCDS(ft1) || x_FeatIsVDJC(ft1) ) {
            break;
        }
        ++it1;
    }

    if (! it1) {
        return;
    }

    CFeat_CI it2 = it1;
    ++it2;

    while (it2) {
        const CSeq_feat& ft2 = it2->GetOriginalFeature();
        if ( x_FeatIsCDS(ft2) || x_FeatIsVDJC(ft2) ) {
 
            const CSeq_feat& ft1 = it1->GetOriginalFeature();

            const CSeq_loc& loc1 = ft1.GetLocation();
            const CSeq_loc& loc2 = ft2.GetLocation();

            ENa_strand strand1 = eNa_strand_plus;
            if (loc1.IsSetStrand() && loc1.GetStrand() == eNa_strand_minus) {
                strand1 = eNa_strand_minus;
            }

            ENa_strand strand2 = eNa_strand_plus;
            if (loc2.IsSetStrand() && loc2.GetStrand() == eNa_strand_minus) {
                strand2 = eNa_strand_minus;
            }

            if (strand1 == strand2) {
                bool bad = false;
                if ( x_FeatIsCDS(ft1) && x_FeatIsVDJC(ft2) ) {
                    bad = x_BadCDSinVDJC(loc1, loc2, m_Scope);
                    if (bad) {
                        if ( x_IsPseudo(ft1, m_Imp) || x_IsPseudo(ft2, m_Imp) ) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSdoesNotMatchVDJC,
                                    "No parent for (pseudo) CdRegion", ft1);
                        } else {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSdoesNotMatchVDJC,
                                    "No parent for CdRegion", ft1);
                        }
                    }
                } else if ( x_FeatIsVDJC(ft1) && x_FeatIsCDS(ft2) ) {
                    bad = x_BadCDSinVDJC(loc2, loc1, m_Scope);
                    if (bad) {
                        if ( x_IsPseudo(ft1, m_Imp) || x_IsPseudo(ft2, m_Imp) ) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSdoesNotMatchVDJC,
                                    "No parent for (pseudo) CdRegion", ft2);
                        } else {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSdoesNotMatchVDJC,
                                    "No parent for CdRegion", ft2);
                        }
                    }
                }
            }
            it1 = it2;
        }
        ++it2;
    }
}



void CValidError_bioseq::x_ValidateCDSVDJCmatch(const CBioseq_Handle& seq,  int numcds,
                                                int numcrgn, int numvseg,
                                                int numdseg, int numjseg)
{
    unsigned int lclcds = 0, lclcrgn = 0, lclvseg = 0, lcldseg = 0, lcljseg = 0, lclnone = 0, lclothr = 0;

    CSeq_entry_Handle topseh = seq.GetTopLevelEntry();
    CSeqEntryIndex idx(topseh);
    CRef<CBioseqIndex> bsx = idx.GetBioseqIndex(seq);
    bsx->IterateFeatures([this, &lclcds, &lclcrgn, &lclvseg, &lcldseg, &lcljseg, &lclnone, &lclothr](CFeatureIndex& sfx) {
        CSeqFeatData::ESubtype sbt = sfx.GetSubtype();
        if (sbt == CSeqFeatData::ESubtype::eSubtype_cdregion) {
            lclcds++;
            CConstRef<CSeq_loc> cloc = sfx.GetMappedLocation();
            CRef<CFeatureIndex> prnt = sfx.GetBestParent();
            if (prnt) {
                CSeqFeatData::ESubtype ptyp = prnt->GetSubtype();
                // CConstRef<CSeq_loc> ploc = prnt->GetMappedLocation();
                if (ptyp == CSeqFeatData::ESubtype::eSubtype_C_region) {
                    lclcrgn++;
                } else if (ptyp == CSeqFeatData::ESubtype::eSubtype_V_segment) {
                    lclvseg++;
                } else if (ptyp == CSeqFeatData::ESubtype::eSubtype_D_segment) {
                    lcldseg++;
                } else if (ptyp == CSeqFeatData::ESubtype::eSubtype_J_segment) {
                    lcljseg++;
                } else {
                    lclothr++;
                }
            } else {
                lclnone++;
                string sloc_str;
                cloc->GetLabel(&sloc_str);
                // LOG_POST_XX(Corelib_App, 1, "No parent for CdRegion at: " + sloc_str + "\n");
                CSeq_feat_Handle sfh = sfx.GetSeqFeatHandle();
                CConstRef<CSeq_feat> sf = sfh.GetOriginalSeq_feat();
                if (sf) {
                    string locus;
                    CRef<CFeatureIndex> gne = sfx.GetBestGene();
                    if (gne) {
                        const CGene_ref& gene = gne->GetMappedFeat().GetData().GetGene();
                        if (gene.IsSetLocus()) {
                           locus = gene.GetLocus();
                        } else if (gene.IsSetLocus_tag()) {
                            locus = gene.GetLocus_tag();
                        } else {
                            CConstRef<CSeq_loc> gloc = gne->GetMappedLocation();
                            if (gloc) {
                                // string gloc_str;
                                gloc->GetLabel(&locus);
                                // LOG_POST_XX(Corelib_App, 1, "  but GetBestGene is: " + gloc_str + "\n");
                            }
                        }
                    }
                    if (locus.length() > 0) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSdoesNotMatchVDJC,
                            "No parent for CdRegion (gene is " + locus + ")", *sf);
                    } else {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSdoesNotMatchVDJC,
                            "No parent for CdRegion", *sf);
                    }
                }
            }
        }
    });
    /*
    LOG_POST_XX(Corelib_App, 1, "lclcds: " + NStr::IntToString(lclcds) + "\n");
    LOG_POST_XX(Corelib_App, 1, "lclcrgn: " + NStr::IntToString(lclcrgn) + "\n");
    LOG_POST_XX(Corelib_App, 1, "lclvseg: " + NStr::IntToString(lclvseg) + "\n");
    LOG_POST_XX(Corelib_App, 1, "lcldseg: " + NStr::IntToString(lcldseg) + "\n");
    LOG_POST_XX(Corelib_App, 1, "lcljseg: " + NStr::IntToString(lcljseg) + "\n");
    LOG_POST_XX(Corelib_App, 1, "lclothr: " + NStr::IntToString(lclothr) + "\n");
    LOG_POST_XX(Corelib_App, 1, "lclnone: " + NStr::IntToString(lclnone) + "\n");
    */
}

void CValidError_bioseq::x_ValidateCDSmRNAmatch(const CBioseq_Handle& seq, 
                                                int numgene, 
                                                int numcds, 
                                                int nummrna)
{
    if (!m_AllFeatIt) {
        return;
    }

    list<CRef<CCdsMatchInfo>> cds_list;
    TmRNAList mrna_map;

    // Loop over all features
    // Populate the cds and mrna lists
    for (const auto& mapped_feat : *m_AllFeatIt) {
        if (!mapped_feat.IsSetData()) {
            continue;
        }

        if (mapped_feat.GetData().IsCdregion()) {
            const auto& cds_feat = *mapped_feat.GetSeq_feat();
          
            auto cds_match = Ref(new CCdsMatchInfo(cds_feat, m_Scope));

            // If pseudo, no need to match with mRNA 
            if (cds_feat.IsSetPseudo() && cds_feat.GetPseudo()) {
                cds_match->SetPseudo();
            } else {  
                CConstRef<CSeq_feat> gene_feat = m_Imp.GetCachedGene(&cds_feat);
                if (gene_feat && 
                    gene_feat->IsSetPseudo() && gene_feat->GetPseudo()) {
                    cds_match->SetPseudo();
                }
            }
            cds_list.push_back(cds_match);
        } else if (mapped_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
            const auto& feat = *mapped_feat.GetSeq_feat();
            mrna_map[mapped_feat.GetSeq_feat()] = Ref(new CMrnaMatchInfo(feat, m_Scope));
        } 
    }

    if (!mrna_map.empty()) {
        x_ValidateGeneCDSmRNACounts(seq);
    }
   
    const size_t num_mrna = mrna_map.size();
    const CTSE_Handle& tse = seq.GetTSE_Handle();
    // First attempt to match by xref
    for (auto&& cds : cds_list) {
        cds->AssignXrefMatch(mrna_map, tse);
    }
    // Now attempt to match by overlap
    if (!mrna_map.empty()) {
        for (auto&& cds : cds_list) {
            if (!cds->HasMatch()) {
                cds->AssignOverlapMatch(mrna_map, *m_Scope);
            }
        }
    }
    // Now loop over cds to find number of matched cds and number of matched mrna
    int num_matched_cds = 0;
    int num_unmatched_cds = 0;
    for (auto&& cds : cds_list) {
        // Check to see if a CDS feat references or overlaps multiple mRNAs
        // mrna_list now contains only unmatched mrnas
        x_CheckForMultiplemRNAs(*cds, mrna_map);
        x_CheckMrnaProteinLink(*cds);
        // check for mismatching qualifiers
        x_CheckOrigProteinAndTranscriptIds(*cds);

        if (cds->IsPseudo() || 
            (cds->GetSeqfeat().IsSetExcept() &&
             cds->GetSeqfeat().IsSetExcept_text() &&
             NStr::Find(cds->GetSeqfeat().GetExcept_text(), "rearrangement required for product") != string::npos)) {
            cds->NeedsMatch(false); // In this case, we don't require a matching mRNA
            continue;
        }

        if (cds->HasMatch()) {
            ++num_matched_cds;
        } else {
            ++num_unmatched_cds;
        }
    }

    // Code now returns a eErr_SEQ_FEAT_CDSwithNoMRNA warning, 
    // even when no CDS features are matched
    if (num_unmatched_cds > 0 &&
        num_mrna > 0) {
        if (num_unmatched_cds >= 10) {
            const auto numcds = num_matched_cds + num_unmatched_cds;
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_CDSwithNoMRNA,
                    NStr::NumericToString (num_unmatched_cds)
                    + " out of " + NStr::IntToString (numcds)
                    + " CDSs unmatched",
                    *(seq.GetCompleteBioseq()));
        } else {
            for (const auto& cds : cds_list) {
                if (!cds->HasMatch() && cds->NeedsMatch()) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSwithNoMRNA,
                            "Unmatched CDS", cds->GetSeqfeat());
                }
            }
        }
    }

    // check to see if remaining mRNAs are pseudo
    size_t num_unmatched_mrna = 0;

    NON_CONST_ITERATE(TmRNAList, it, mrna_map) {
        CConstRef<CSeq_feat> gene_feat = m_Imp.GetCachedGene(CConstRef<CSeq_feat>(&(it->second->GetSeqfeat())));
        if (gene_feat &&
            gene_feat->IsSetPseudo() && gene_feat->GetPseudo()) {
            it->second->SetPseudo();
        }
        if (!it->second->OkWithoutCds()) {
            num_unmatched_mrna++;
        }
    }

    if (num_unmatched_mrna > 10) {
        string msg = "No matches for " + NStr::NumericToString(num_unmatched_mrna) + " mRNAs";
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNANotMatched,
                 msg, *(seq.GetCompleteBioseq()));
    } else {
        ITERATE(TmRNAList, it, mrna_map) {
            if (!it->second->OkWithoutCds()) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAMismatchLocation,
                    "No CDS location match for 1 mRNA", it->second->GetSeqfeat());
            }
        }
    }
}

void CValidError_bioseq::x_ValidateGeneCDSmRNACounts (const CBioseq_Handle& seq)
{
    if (m_GeneIt != NULL && m_AllFeatIt != NULL) {
        if (! m_GeneIt->empty()) {
            // nothing to validate if there aren't any genes
            // count mRNAs and CDSs for each gene.
            typedef map<CConstRef<CSeq_feat>, SIZE_TYPE> TFeatCount;
            TFeatCount cds_count, mrna_count;

            // create indices for gene labels and locus tags if needed
            typedef map<string, CConstRef<CSeq_feat> > TGeneList;
            TGeneList gene_labels, gene_locus_tags;

            CConstRef<CSeq_feat> gene;

            ITERATE(CCacheImpl::TFeatValue, it, *m_AllFeatIt) {
                CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
                if (subtype != CSeqFeatData::eSubtype_cdregion && subtype != CSeqFeatData::eSubtype_mRNA) {
                    continue;
                }
                const CSeq_feat& feat = it->GetOriginalFeature();

                gene = m_Imp.GetCachedGene(&feat);

                if (gene) {
                    if (cds_count.find(gene) == cds_count.end()) {
                        cds_count[gene] = mrna_count[gene] = 0;
                    }

                    switch (subtype) {
                        case CSeqFeatData::eSubtype_cdregion:
                            cds_count[gene]++;
                            break;
                        case CSeqFeatData::eSubtype_mRNA:
                            mrna_count[gene]++;
                            break;
                        default:
                            break;
                    }
                }
            }

            ITERATE (TFeatCount, it, cds_count) {
                SIZE_TYPE cds_num = it->second,
                          mrna_num = mrna_count[it->first];
                if (cds_num > 0 && mrna_num > 1 && cds_num != mrna_num) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAmismatchCount,
                        "mRNA count (" + NStr::SizetToString(mrna_num) + 
                        ") does not match CDS (" + NStr::SizetToString(cds_num) +
                        ") count for gene", *it->first);
                }
            }
        }
    }    
}


void CValidError_bioseq::x_ValidateAbuttingUTR(
    const CBioseq_Handle& seq)
{
    // note - if we couldn't build the feature iterator, no point
    // in trying this
    if (!m_AllFeatIt) {
        return;
    }

    // count features of interest, find strand for coding region
    ENa_strand strand = eNa_strand_unknown;

    // we want a few different

    // In feat_key, feat_subtype will be overwritten a few times
    CCacheImpl::SFeatKey feat_key(
        CCacheImpl::kAnyFeatType, CSeqFeatData::eSubtype_cdregion, seq);
    // cdregion is special because it can set strand
    const CCacheImpl::TFeatValue & cd_region_feats =
        GetCache().GetFeatFromCache(feat_key);
    const size_t num_cds = cd_region_feats.size();
    feat_key.feat_subtype = CSeqFeatData::eSubtype_3UTR;
    const size_t num_3utr = GetCache().GetFeatFromCache(feat_key).size();
    feat_key.feat_subtype = CSeqFeatData::eSubtype_5UTR;
    const size_t num_5utr = GetCache().GetFeatFromCache(feat_key).size();
    feat_key.feat_subtype = CSeqFeatData::eSubtype_gene;
    const size_t num_gene = GetCache().GetFeatFromCache(feat_key).size();

    // cdregion is special because it can set strand
    if( num_cds > 0 ) {
        strand = cd_region_feats.back().GetLocation().GetStrand();
        }

    bool is_mrna = false;
    if (seq.CanGetInst_Mol() && seq.GetInst_Mol() == CSeq_inst::eMol_rna) {
        CSeqdesc_CI sd(seq, CSeqdesc::e_Molinfo);
        if (!sd) {
            // assume RNA sequence is mRNA
            is_mrna = true;
        } else  if (sd->GetMolinfo().IsSetBiomol() && sd->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
            is_mrna = true;
        }
    }    

    if (is_mrna) {

        ITERATE(CCacheImpl::TFeatValue, cdregion_it, cd_region_feats) {
            if (cdregion_it->GetLocation().GetStrand() == eNa_strand_minus) {
                    PostErr (eDiag_Error, eErr_SEQ_FEAT_CDSonMinusStrandMRNA,
                         "CDS should not be on minus strand of mRNA molecule", cdregion_it->GetOriginalFeature());
                }
            }
        }

    if (is_mrna || (num_cds == 1 && num_gene < 2)) {

        if (is_mrna) {
            // if this is an mRNA sequence, features should be on the plus strand
            strand = eNa_strand_plus;
        }

        int utr5_right = 0;
        int utr3_right = 0;
        int cds_right = 0;
        bool first_cds = true;

        // get multiple kinds of features
        vector<CCacheImpl::SFeatKey> featKeys;
        CCacheImpl::SFeatKey multi_feat_key_template(
            CCacheImpl::kAnyFeatType,
            CCacheImpl::kAnyFeatSubtype, // will be overwritten a few times
            seq);
        multi_feat_key_template.feat_subtype =
            CSeqFeatData::eSubtype_cdregion;
        featKeys.push_back(multi_feat_key_template);
        multi_feat_key_template.feat_subtype = CSeqFeatData::eSubtype_3UTR;
        featKeys.push_back(multi_feat_key_template);
        multi_feat_key_template.feat_subtype = CSeqFeatData::eSubtype_5UTR;
        featKeys.push_back(multi_feat_key_template);
        multi_feat_key_template.feat_subtype = CSeqFeatData::eSubtype_gene;
        featKeys.push_back(multi_feat_key_template);

        AutoPtr<CCacheImpl::TFeatValue> cug_feats =
            GetCache().GetFeatFromCacheMulti(featKeys);

        if (strand == eNa_strand_minus) {
            // minus strand - expect 3'UTR, CDS, 5'UTR
            ITERATE(CCacheImpl::TFeatValue, cug_it, *cug_feats) {
                CSeqFeatData::ESubtype subtype = cug_it->GetData().GetSubtype();
                int this_left = cug_it->GetLocation().GetStart (eExtreme_Positional);
                int this_right = cug_it->GetLocation().GetStop (eExtreme_Positional);
                if (subtype == CSeqFeatData::eSubtype_3UTR) {
                    if (cug_it->GetLocation().GetStrand() != eNa_strand_minus) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "3'UTR is not on minus strand", cug_it->GetOriginalFeature());
                    } else if (utr5_right > 0 && utr5_right + 1 != this_left) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "Previous 5'UTR does not abut next 3'UTR", cug_it->GetOriginalFeature());
                    }
                    utr3_right = this_right;
                } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
                    if (utr3_right > 0 && utr3_right + 1 != this_left) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "CDS does not abut 3'UTR", cug_it->GetOriginalFeature());
                    }
                    first_cds = false;
                    cds_right = this_right;
                } else if (subtype == CSeqFeatData::eSubtype_5UTR && num_5utr < 2) {
                    if (cug_it->GetLocation().GetStrand() != eNa_strand_minus) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "5'UTR is not on minus strand", cug_it->GetOriginalFeature());
                    } else if (cds_right > 0 && cds_right + 1 != this_left) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "5'UTR does not abut CDS", cug_it->GetOriginalFeature());
                    }
                    utr5_right = this_right;
                }
            }
        } else {
            // plus strand - expect 5'UTR, CDS, 3'UTR
            ITERATE(CCacheImpl::TFeatValue, cug_it, *cug_feats) {
                CSeqFeatData::ESubtype subtype = cug_it->GetData().GetSubtype();
                int this_left = cug_it->GetLocation().GetStart (eExtreme_Positional);
                int this_right = cug_it->GetLocation().GetStop (eExtreme_Positional);
                if (subtype == CSeqFeatData::eSubtype_5UTR && num_5utr < 2) {
                    if (cug_it->GetLocation().GetStrand() == eNa_strand_minus) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "5'UTR is not on plus strand", cug_it->GetOriginalFeature());
                    } else if (utr3_right > 0 && utr3_right + 1 != this_left) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "Previous 3'UTR does not abut next 5'UTR", cug_it->GetOriginalFeature());
                    }
                    utr5_right = this_right;
                } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
                    if (utr5_right > 0 && utr5_right + 1 != this_left && first_cds ) {

                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "5'UTR does not abut CDS", cug_it->GetOriginalFeature());
                    }
                    first_cds = false;
                    cds_right = this_right;
                } else if (subtype == CSeqFeatData::eSubtype_3UTR) {
                    if (cug_it->GetLocation().GetStrand() == eNa_strand_minus) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
                                 "3'UTR is not on plus strand", cug_it->GetOriginalFeature());
                    } else if (cds_right > 0 && cds_right + 1 != this_left && num_3utr == 1) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotAbutCDS, 
                                 "CDS does not abut 3'UTR", cug_it->GetOriginalFeature());
                    }
                    if (is_mrna && num_cds == 1 && num_3utr == 1 && this_right != (int) seq.GetBioseqLength() - 1) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UTRdoesNotExtendToEnd, 
                                 "3'UTR does not extend to end of mRNA", cug_it->GetOriginalFeature());
                    }
                }
            }
        }
    }
}

enum ERnaPosition {
    e_RnaPosition_Ignore = 0,
    e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT,
    e_RnaPosition_INTERNAL_SPACER_1,
    e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT,
    e_RnaPosition_INTERNAL_SPACER_2,
    e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT,
    e_RnaPosition_INTERNAL_SPACER_X
};

static ERnaPosition s_RnaPosition (const CSeq_feat& feat)
{
    ERnaPosition rval = e_RnaPosition_Ignore;

    if (!feat.IsSetData() || !feat.GetData().IsRna()) {
        return e_RnaPosition_Ignore;
    }
    const CRNA_ref& rna = feat.GetData().GetRna();
    if (!rna.IsSetType()) {
        rval = e_RnaPosition_Ignore;
    } else if (!rna.IsSetExt()) {
        rval = e_RnaPosition_Ignore;
    } else if (rna.GetType() == CRNA_ref::eType_rRNA) {
        const string& product = rna.GetExt().GetName();
        if (NStr::StartsWith (product, "small ", NStr::eNocase)
            || NStr::StartsWith (product, "18S ", NStr::eNocase)
            || NStr::StartsWith (product, "16S ", NStr::eNocase)
            // variant spellings
            || NStr::StartsWith (product, "18 ", NStr::eNocase)
            || NStr::StartsWith (product, "16 ", NStr::eNocase)) {
            rval = e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT;
        } else if (NStr::StartsWith (product, "5.8S ", NStr::eNocase)
                   // variant spellings
                   || NStr::StartsWith (product, "5.8 ", NStr::eNocase)) {
            rval = e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT;
        } else if (NStr::StartsWith (product, "large ", NStr::eNocase)
                   || NStr::StartsWith (product, "26S ", NStr::eNocase)
                   || NStr::StartsWith (product, "28S ", NStr::eNocase)
                   || NStr::StartsWith (product, "23S ", NStr::eNocase)
                   // variant spellings
                   || NStr::StartsWith (product, "26 ", NStr::eNocase)
                   || NStr::StartsWith (product, "28 ", NStr::eNocase)
                   || NStr::StartsWith (product, "23 ", NStr::eNocase)) {
            rval = e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT;
        }
    } else if (rna.GetType() == CRNA_ref::eType_other || rna.GetType() == CRNA_ref::eType_miscRNA) {
        string product = "";
        if (rna.GetExt().IsName()) {
            product = rna.GetExt().GetName();
            if (NStr::EqualNocase (product, "misc_RNA")) {
                FOR_EACH_GBQUAL_ON_SEQFEAT (it, feat) {
                    if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "product")
                        && (*it)->IsSetVal() && !NStr::IsBlank ((*it)->GetVal())) {
                        product = (*it)->GetVal();
                        break;
                    }
                }
            }
        } else if (rna.GetExt().IsGen()) {
            if (rna.GetExt().GetGen().IsSetProduct()) {
                product = rna.GetExt().GetGen().GetProduct();
            }
        }
        if (NStr::EqualNocase (product, "internal transcribed spacer 1")
            || NStr::EqualNocase (product, "internal transcribed spacer1")) {
            rval = e_RnaPosition_INTERNAL_SPACER_1;
        } else if (NStr::EqualNocase (product, "internal transcribed spacer 2")
                   || NStr::EqualNocase (product, "internal transcribed spacer2")) {
            rval = e_RnaPosition_INTERNAL_SPACER_2;
        } else if (NStr::EqualNocase (product, "internal transcribed spacer")
                   || NStr::EqualNocase (product, "ITS")
                   || NStr::EqualNocase (product, "16S-23S ribosomal RNA intergenic spacer")
                   || NStr::EqualNocase (product, "16S-23S intergenic spacer")
                   || NStr::EqualNocase (product, "intergenic spacer")) {
            rval = e_RnaPosition_INTERNAL_SPACER_X;
        }
    }
    return rval;
}


bool CValidError_bioseq::x_IsRangeGap (const CBioseq_Handle& seq, int start, int stop)
{
    if (!seq.IsSetInst() || !seq.GetInst().IsSetRepr() 
        || seq.GetInst().GetRepr() != CSeq_inst::eRepr_delta
        || !seq.GetInst().IsSetExt()
        || !seq.GetInst().GetExt().IsDelta()
        || !seq.GetInst().GetExt().GetDelta().IsSet()) {
        return false;
    }
    if (start < 0 || (unsigned int) stop >= seq.GetInst_Length() || start > stop) {
        return false;
    }

    int offset = 0;
    ITERATE (CDelta_ext::Tdata, it, seq.GetInst().GetExt().GetDelta().Get()) {
        int this_len = 0;
        if ((*it)->IsLiteral()) {
            this_len = (*it)->GetLiteral().GetLength();
        } else if ((*it)->IsLoc()) {
            this_len = GetLength((*it)->GetLoc(), m_Scope);
        }
        if ((*it)->IsLiteral() && 
            (!(*it)->GetLiteral().IsSetSeq_data() || (*it)->GetLiteral().GetSeq_data().IsGap())) {
              if (start >= offset && stop < offset + this_len) {
                  return true;
              }
        }
        offset += this_len;
        if (offset > start) {
            return false;
        }
    }
    return false;
}


bool s_AreAdjacent(ERnaPosition pos1, ERnaPosition pos2)
{
    if ((pos1 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT && (pos2 == e_RnaPosition_INTERNAL_SPACER_1 || pos2 == e_RnaPosition_INTERNAL_SPACER_X)) ||
        (pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT && (pos2 == e_RnaPosition_INTERNAL_SPACER_2 || pos2 == e_RnaPosition_INTERNAL_SPACER_X)) ||
        ((pos1 == e_RnaPosition_INTERNAL_SPACER_1 || pos1 == e_RnaPosition_INTERNAL_SPACER_X) && pos2 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT) ||
        (pos1 == e_RnaPosition_INTERNAL_SPACER_2 && pos2 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT) ||
        (pos1 == e_RnaPosition_INTERNAL_SPACER_X && pos2 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT)) {
        return true;
    } else {
        return false;
    }
}


void CValidError_bioseq::x_ValidateAbuttingRNA(const CBioseq_Handle& seq)
{
    // if unable to build feature iterator for this sequence, this will
    // also fail
    if (!m_AllFeatIt) {
        return;
    }
    if (seq.IsSetInst() && seq.GetInst().IsSetMol() && seq.GetInst().GetMol() == CSeq_inst::eMol_rna) {
        CSeqdesc_CI sd(seq, CSeqdesc::e_Molinfo);
        if (sd && sd->GetMolinfo().IsSetBiomol() && sd->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
            // do not check for mRNA sequences
            return;
        }
    }
    // VR-802 do not report certain errors for organelle sequences
    bool is_organelle = IsOrganelle(seq);

    SAnnotSelector sel (CSeqFeatData::e_Rna);

    CFeat_CI it(seq, sel);


    if (it) {
        
        ENa_strand strand1 = eNa_strand_plus;
        if (it->GetLocation().IsSetStrand() && it->GetLocation().GetStrand() == eNa_strand_minus) {
            strand1 = eNa_strand_minus;
        }
        ERnaPosition pos1 = s_RnaPosition (it->GetOriginalFeature());
        int right1 = it->GetLocation().GetStop(eExtreme_Positional);

        CFeat_CI it2 = it;
        ++it2;
        while (it2) {
            ERnaPosition pos2 = s_RnaPosition (it2->GetOriginalFeature());
            if (pos2 != e_RnaPosition_Ignore) {
                ENa_strand strand2 = eNa_strand_plus;
                if (it2->GetLocation().IsSetStrand() && it2->GetLocation().GetStrand() == eNa_strand_minus) {
                    strand2 = eNa_strand_minus;
                }
                int left2 = it2->GetLocation().GetStart(eExtreme_Positional);
                int right2 = it2->GetLocation().GetStop(eExtreme_Positional);
    
                if ((strand1 == eNa_strand_minus && strand2 != eNa_strand_minus)
                    || (strand1 != eNa_strand_minus && strand2 == eNa_strand_minus)) {
                    // different strands
                    if (pos1 != e_RnaPosition_Ignore && pos2 != e_RnaPosition_Ignore) {
                        if ((pos1 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT || pos1 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT || pos1 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT) &&
                            (pos2 == e_RnaPosition_LEFT_RIBOSOMAL_SUBUNIT || pos2 == e_RnaPosition_MIDDLE_RIBOSOMAL_SUBUNIT || pos2 == e_RnaPosition_RIGHT_RIBOSOMAL_SUBUNIT)) {
                        } else if ((pos1 == e_RnaPosition_INTERNAL_SPACER_1 || pos1 == e_RnaPosition_INTERNAL_SPACER_2 || pos1 == e_RnaPosition_INTERNAL_SPACER_X) &&
                            (pos2 == e_RnaPosition_INTERNAL_SPACER_1 || pos2 == e_RnaPosition_INTERNAL_SPACER_2 || pos2 == e_RnaPosition_INTERNAL_SPACER_X)) {
                        } else {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_InconsistentRRNAstrands, 
                                     "Inconsistent strands for rRNA components",
                                     it2->GetOriginalFeature());
                        }
                    }
                } else if (pos1 == e_RnaPosition_Ignore || pos2 == e_RnaPosition_Ignore) {
                    // ignore
                } else if (right1 + 1 < left2) {
                    // gap between features
                    if (x_IsRangeGap (seq, right1 + 1, left2 - 1)) {
                        // ignore, gap between features is gap in sequence
                    } else if (strand1 == eNa_strand_minus) {
                        if (s_AreAdjacent(pos2, pos1)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_ITSdoesNotAbutRRNA, 
                                     "ITS does not abut adjacent rRNA component",
                                     it2->GetOriginalFeature());
                        }
                    } else {
                        if (s_AreAdjacent(pos1, pos2)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_ITSdoesNotAbutRRNA, 
                                     "ITS does not abut adjacent rRNA component",
                                     it2->GetOriginalFeature());
                        }
                    }
                } else if (right1 + 1 > left2) {
                    // features overlap
                    if (strand1 == eNa_strand_minus) {
                        // on minus strand
                        if (s_AreAdjacent(pos2, pos1)) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOverlapRRNA,
                                     "ITS overlaps adjacent rRNA component",
                                     it2->GetOriginalFeature());
                        } else {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOverlapAndOrder,
                                "rRNA components overlap and out of order", it2->GetOriginalFeature());
                        }
                    } else {
                        // on plus strand
                        if (s_AreAdjacent(pos1, pos2)) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOverlapRRNA,
                                "ITS overlaps adjacent rRNA component",
                                it2->GetOriginalFeature());
                        } else {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOverlapAndOrder,
                                "rRNA components overlap and out of order", it2->GetOriginalFeature());
                        }
                    }
    
                } else if (!is_organelle) {
                    // features abut
                    if (strand1 == eNa_strand_minus) {
                        // on minus strand
                        if (pos1 == pos2
                            && it->GetLocation().IsPartialStop(eExtreme_Positional)
                            && it2->GetLocation().IsPartialStart(eExtreme_Positional)
                            && seq.IsSetInst_Repr() && seq.GetInst_Repr() == CSeq_inst::eRepr_seg) {
                            /* okay in segmented set */
                        }
                        else if (!s_AreAdjacent(pos2, pos1)) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOrder,
                                "Problem with order of abutting rRNA components",
                                it2->GetOriginalFeature());
                        }
                    } else {
                        // on plus strand
                        if (pos1 == pos2 
                            && it->GetLocation().IsPartialStop (eExtreme_Positional)
                            && it2->GetLocation().IsPartialStart (eExtreme_Positional)
                            && seq.IsSetInst_Repr() && seq.GetInst_Repr() == CSeq_inst::eRepr_seg) {
                          /* okay in segmented set */
                        } else if (!s_AreAdjacent(pos1, pos2)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOrder,
                                     "Problem with order of abutting rRNA components",
                                     it2->GetOriginalFeature());
                        }
                    }
                }
                it = it2;
                pos1 = pos2;
                strand1 = strand2;
                right1 = right2;
            }
            ++it2;
        }
    }

}


EDiagSev CValidError_bioseq::x_DupFeatSeverity 
(const CSeq_feat& curr,
 const CSeq_feat& prev, 
 bool is_viral,
 bool is_htgs,
 bool same_annot,
 bool same_label)
{
    if (!same_annot && !same_label) {
        return eDiag_Warning;
    }

    EDiagSev severity = eDiag_Warning;
    CSeqFeatData::ESubtype curr_subtype = curr.GetData().GetSubtype();

    if ( (prev.IsSetDbxref()  &&
          IsFlybaseDbxrefs(prev.GetDbxref()))  || 
         (curr.IsSetDbxref()  && 
          IsFlybaseDbxrefs(curr.GetDbxref())) ) {
        severity = eDiag_Error;
    }

    if (curr_subtype == CSeqFeatData::eSubtype_repeat_region
        || curr_subtype == CSeqFeatData::eSubtype_site
        || curr_subtype == CSeqFeatData::eSubtype_bond) {
        severity = eDiag_Warning;
    }

    if (same_label) {
        CConstRef<CSeq_feat> g1 = m_Imp.GetCachedGene(&curr);
        CConstRef<CSeq_feat> g2 = m_Imp.GetCachedGene(&prev);
        if (g1 && g2 && g1 != g2) {
            // different genes
            severity = eDiag_Warning;
        }
    } else {
        //same annot
        // lower severity for some pairs of partial features or pseudo features
        if (curr.IsSetPartial() && curr.GetPartial()
            && prev.IsSetPartial() && prev.GetPartial()) {
            if (curr_subtype == CSeqFeatData::eSubtype_gene
                || curr_subtype == CSeqFeatData::eSubtype_mRNA
                || (curr_subtype == CSeqFeatData::eSubtype_cdregion && is_viral)) {
                severity = eDiag_Warning;
            }
        }
        if (curr_subtype == CSeqFeatData::eSubtype_gene
            && curr.IsSetPseudo() && curr.GetPseudo()
            && prev.IsSetPseudo() && prev.GetPseudo()) {
            severity = eDiag_Warning;
        } else if (curr_subtype == CSeqFeatData::eSubtype_gene && is_viral) {
            severity = eDiag_Warning;
        } else if (curr_subtype == CSeqFeatData::eSubtype_cdregion && is_htgs) {
            severity = eDiag_Warning;
        }        
    }

    return severity;
}


// assumption: this would only be called if the feature subtypes are already known to match
bool CValidError_bioseq::x_SuppressDicistronic(const CSeq_feat_Handle & f1, const CSeq_feat_Handle & f2, bool fruit_fly)
{
    if (!IsDicistronicGene(f1) || !IsDicistronicGene(f2)) {
        return false;
    }
    if (fruit_fly) {
        return true;
    }
    if (m_Imp.IsRefSeq()) {
        return true;
    }
    return false;
}


bool CValidError_bioseq::x_ReportDupOverlapFeaturePair (const CSeq_feat_Handle & f1, const CSeq_feat_Handle & f2, bool fruit_fly, bool viral, bool htgs)
{
    if (x_SuppressDicistronic(f1, f2, fruit_fly)) {
        return false;
    }

    bool rval = false;

    // Get type of duplication, if any
    EDuplicateFeatureType dup_type = IsDuplicate (f1, f2);
    const CSeq_feat& feat1 = *(f1.GetSeq_feat());
    const CSeq_feat& feat2 = *(f2.GetSeq_feat());

    switch (dup_type) {
        case eDuplicate_Duplicate:
            {{
                EDiagSev severity = x_DupFeatSeverity(feat1, feat2, viral, htgs, true, true);
                CConstRef <CSeq_feat> g1 =
                    m_Imp.GetCachedGene(&feat1);
                CConstRef <CSeq_feat> g2 =
                    m_Imp.GetCachedGene(&feat2);
                if (g1 && g2 && g1.GetPointer() != g2.GetPointer()) {
                    severity = eDiag_Warning;
                }
                PostErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                    "Duplicate feature", feat2);
                rval = true;
            }}
            break;
        case eDuplicate_SameIntervalDifferentLabel:
            if (PartialsSame(feat1.GetLocation(), feat2.GetLocation())) {
                EDiagSev severity = x_DupFeatSeverity(feat1, feat2, viral, htgs, true, false);
                if (feat1.GetData().IsImp()) {
                    severity = eDiag_Warning;
                }
                PostErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                    "Features have identical intervals, but labels differ",
                    feat2);
                rval = true;
            }
            break;
        case eDuplicate_DuplicateDifferentTable:
            {{
                EDiagSev severity = x_DupFeatSeverity(feat1, feat2, viral, htgs, false, true);
                PostErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                    "Duplicate feature (packaged in different feature table)",
                    feat2);
                rval = true;
            }}
            break;
        case eDuplicate_SameIntervalDifferentLabelDifferentTable:
            {{
                EDiagSev severity = x_DupFeatSeverity(feat1, feat2, viral, htgs, false, false);
                PostErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                    "Features have identical intervals, but labels "
                    "differ (packaged in different feature table)",
                    feat2);
                rval = true;
            }}
            break;
        case eDuplicate_Not:
            // no error
            break;
    }
    return rval;
}


void CValidError_bioseq::x_ReportOverlappingPeptidePair (CSeq_feat_Handle f1, CSeq_feat_Handle f2, const CBioseq& bioseq, bool& reported_last_peptide)
{
    const CSeq_feat& feat1 = *(f1.GetSeq_feat());
    const CSeq_feat& feat2 = *(f2.GetSeq_feat());
    // subtypes
    CSeqFeatData::ESubtype feat1_subtype = feat1.GetData().GetSubtype();
    CSeqFeatData::ESubtype feat2_subtype = feat2.GetData().GetSubtype();
    // locations
    const CSeq_loc& feat1_loc = feat1.GetLocation();
    const CSeq_loc& feat2_loc = feat2.GetLocation();

    if ( (feat1_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
          feat1_subtype == CSeqFeatData::eSubtype_propeptide_aa        ||
          feat1_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
          feat1_subtype == CSeqFeatData::eSubtype_transit_peptide_aa)) {
        if ((feat2_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
             feat2_subtype == CSeqFeatData::eSubtype_propeptide_aa       ||
             feat2_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
             feat2_subtype == CSeqFeatData::eSubtype_transit_peptide_aa) &&
            sequence::Compare(feat1_loc, feat2_loc, m_Scope, sequence::fCompareOverlapping) != eNoOverlap  &&
            s_NotPeptideException(feat1, feat2)) {
            EDiagSev overlapPepSev = 
                m_Imp.IsOvlPepErr() ? eDiag_Error :eDiag_Warning;
            string msg = "Signal, Transit, or Mature peptide features overlap";

            try {
                const CSeq_feat* cds = m_Imp.GetCDSGivenProduct(bioseq);
                if (cds != NULL) {
                    string cds_loc = "";
                    const CSeq_id* id = cds->GetLocation().GetId();
                    if (id != NULL) {
                        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id);
                        if (bsh && bsh.GetCompleteBioseq()) {
                            AppendBioseqLabel (cds_loc, *(bsh.GetCompleteBioseq()), true);
                            if (NStr::StartsWith(cds_loc, "BIOSEQ: ")) {
                                cds_loc = cds_loc.substr(8);
                            }
                        } else {
                            id->GetLabel(&cds_loc, CSeq_id::eContent);
                        }
                    }
                    if (!NStr::IsBlank(cds_loc)) {
                        cds_loc = " (parent CDS is on " + cds_loc + ")";
                        msg += cds_loc;
                    }
                }
            } catch ( const exception& ) {
            }

            if (!reported_last_peptide) {
                PostErr( overlapPepSev,
                    eErr_SEQ_FEAT_OverlappingPeptideFeat,
                    msg,
                    feat1); 
            }
            PostErr( overlapPepSev,
                eErr_SEQ_FEAT_OverlappingPeptideFeat,
                msg,
                feat2);
            reported_last_peptide = true;
        } else {
            reported_last_peptide = false;
        }
    }
}


void CValidError_bioseq::ValidateDupOrOverlapFeats(
    const CBioseq& bioseq)
{
    if (!m_AllFeatIt) {
        return;
    }

    try {
        
        bool fruit_fly = false;
        bool viral = false;
        bool htgs = false;

        CSeqdesc_CI di(m_CurrentHandle, CSeqdesc::e_Source);
        if ( di && di->GetSource().IsSetOrg()) {
            if (di->GetSource().GetOrg().IsSetTaxname()
                && NStr::StartsWith(di->GetSource().GetOrg().GetTaxname(), "Drosophila ", NStr::eNocase)) {
                fruit_fly = true;
            }
            if ( di->GetSource().GetOrg().IsSetOrgname()
                 && di->GetSource().GetOrg().GetOrgname().IsSetLineage()
                 && NStr::StartsWith (di->GetSource().GetOrg().GetOrgname().GetLineage(), "Viruses; ")) {
                 viral = true;
            }
        }

        CSeqdesc_CI mi(m_CurrentHandle, CSeqdesc::e_Molinfo);
        if (mi && mi->GetMolinfo().IsSetTech()) {
            CMolInfo::TTech tech = mi->GetMolinfo().GetTech();
            htgs = (tech == CMolInfo::eTech_htgs_1
                    || tech == CMolInfo::eTech_htgs_2
                    || tech == CMolInfo::eTech_htgs_3);
        }

        // TODO: fix: quadratic in size of m_AllFeatIt.
        ITERATE(CCacheImpl::TFeatValue, prev_it, *m_AllFeatIt) {
            CCacheImpl::TFeatValue::const_iterator curr_it = prev_it;
            ++curr_it;
            CConstRef<CSeq_feat> prev_feat = prev_it->GetSeq_feat();
            CSeq_feat_Handle f1 = prev_it->GetSeq_feat_Handle();
            TSeqPos prev_end = prev_feat->GetLocation().GetStop (eExtreme_Positional);
            for( ; curr_it != m_AllFeatIt->end(); ++curr_it) {
                CConstRef<CSeq_feat>curr_feat = curr_it->GetSeq_feat();
                TSeqPos curr_start = curr_feat->GetLocation().GetStart(eExtreme_Positional);
                if (curr_start > prev_end) {
                    break;
                }
                if (x_ReportDupOverlapFeaturePair (f1, curr_it->GetSeq_feat_Handle(), fruit_fly, viral, htgs)) {
                    break;
                }
            }
        }

        CCacheImpl::TFeatValue::const_iterator prev_prot = m_AllFeatIt->begin();
        if (prev_prot != m_AllFeatIt->end()) {
            CCacheImpl::TFeatValue::const_iterator curr_prot = prev_prot;
            ++curr_prot;           
            bool reported_last_peptide = false;
            for( ; curr_prot != m_AllFeatIt->end(); ++prev_prot, ++curr_prot) {
                x_ReportOverlappingPeptidePair(prev_prot->GetSeq_feat_Handle(), curr_prot->GetSeq_feat_Handle(), bioseq, reported_last_peptide);
            }
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating duplicate/overlapping features. EXCEPTION: ") +     
                e.what(), bioseq);
        }
    }
}

static vector<int> s_LocationToStartStopPairs(const CSeq_loc& loc) {

    vector<int> intervalpoints;

    CSeq_loc_CI curr(loc);
    while (curr) {
        const CSeq_loc& part = curr.GetEmbeddingSeq_loc();
        if (part.IsInt()) {
            const CSeq_interval& ivl = part.GetInt();
            intervalpoints.push_back(ivl.GetFrom());
            intervalpoints.push_back(ivl.GetTo());
        } else if (part.IsPacked_int()) {
            ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
                const CSeq_interval& ivl = **it;
                intervalpoints.push_back(ivl.GetFrom());
                intervalpoints.push_back(ivl.GetTo());
                ++curr;
            }
            continue;
        } else if (part.IsPnt()) {
            const CSeq_point& pnt = part.GetPnt();
            intervalpoints.push_back(pnt.GetPoint());
            intervalpoints.push_back(pnt.GetPoint());
        }
        ++curr;
    }

    return intervalpoints;
}


static bool s_SubsequentIntron(CFeat_CI feat_ci_dup, Int4 start, Int4 stop, Int4 max)
{
    ++feat_ci_dup;

    while (feat_ci_dup) {

        const CSeq_feat& const_feat_dup = feat_ci_dup->GetOriginalFeature();
        const CSeq_loc& loc_dup = const_feat_dup.GetLocation();
        for (CSeq_loc_CI curr(loc_dup); curr; ++curr) {
            Int4 fr = 0;
            Int4 to = 0;
            const CSeq_loc& part = curr.GetEmbeddingSeq_loc();
            if (part.IsInt()) {
                const CSeq_interval& ivl = part.GetInt();
                fr = ivl.GetFrom();
                to = ivl.GetTo();
            } else if (part.IsPnt()) {
                const CSeq_point& pnt = part.GetPnt();
                fr = pnt.GetPoint();
                to = pnt.GetPoint();
            } else {
                continue;
            }
            if (start + 1 == fr && stop - 1 == to) {
                return true;
            }
            if (start + 1 == fr && to == max) {
                return true;
            }
            if (to > stop) {
                return false;
            }
        }

        ++feat_ci_dup;
    }

    return false;
}

void CValidError_bioseq::ValidateTwintrons(
    const CBioseq& bioseq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(bioseq);
    if (!bsh) return;
    SAnnotSelector sel(CSeqFeatData::eSubtype_intron);
    try {
        for (CFeat_CI feat_ci(bsh, sel); feat_ci;  ++feat_ci) {

            const CSeq_feat& const_feat = feat_ci->GetOriginalFeature();
            const CSeq_loc& loc = const_feat.GetLocation();

            vector<int> intervalpoints = s_LocationToStartStopPairs(loc);

            int len = intervalpoints.size();
            if (len < 4) {
              continue;
            }
            int max = len - 1;

            bool twintron = true;

            for (int pos = 1; pos < max; pos += 2) {
                Int4 intL = intervalpoints[pos];
                Int4 intR = intervalpoints[pos + 1];

                CFeat_CI feat_ci_dup = feat_ci;
                if (! s_SubsequentIntron(feat_ci_dup, intL, intR, intervalpoints[max])) {
                    twintron = false;
                    break;
                }
            }

            EDiagSev sev = eDiag_Error;
            if (m_Imp.IsEmbl() || m_Imp.IsDdbj()) {
                sev = eDiag_Warning;
            }
            if (twintron) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_MultiIntervalIntron,
                         "Multi-interval intron contains possible twintron",
                          const_feat);
            } else {
                PostErr (sev, eErr_SEQ_FEAT_MultiIntervalIntron,
                         "An intron should not have multiple intervals",
                          const_feat);
            }
        }
    }
    catch (CException& e) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            if (! IsSelfReferential(bioseq)) {
                ERR_POST(Error << "ValidateTwintrons error: " << e.what());
            }
        }
    }
}


bool CValidError_bioseq::IsFlybaseDbxrefs(const TDbtags& dbxrefs)
{
    ITERATE (TDbtags, db, dbxrefs) {
        if ( (*db)->CanGetDb() ) {
            if ( NStr::EqualCase((*db)->GetDb(), "FLYBASE") ||
                 NStr::EqualCase((*db)->GetDb(), "FlyBase") ) {
                return true;
            }
        }
    }
    return false;
}


static bool s_IsTPAAssemblyOkForBioseq (const CBioseq& seq, bool has_refseq)
{
    bool has_local = false, has_genbank = false;
    bool has_gi = false, has_tpa = false, has_bankit = false, has_smart = false;

    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        switch ((*it)->Which()) {
            case CSeq_id::e_Local:
                has_local = true;
                break;
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
                has_genbank = true;
                break;
            case CSeq_id::e_Other:
                has_refseq = true;
                break;
            case CSeq_id::e_Gi:
                has_gi = true;
                break;
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                has_tpa = true;
                break;
            case CSeq_id::e_General:
                if ((*it)->GetGeneral().IsSetDb()) {
                    if (NStr::Equal((*it)->GetGeneral().GetDb(), "BankIt", NStr::eNocase)) {
                        has_bankit = true;
                    } else if (NStr::Equal((*it)->GetGeneral().GetDb(), "TMSMART", NStr::eNocase)) {
                        has_smart = true;
                    }
                }
                break;
            default:
                break;
        }
    }

    if (has_genbank) return false;
    if (has_tpa) return true;
    if (has_refseq) return false;
    if (has_bankit) return true;
    if (has_smart) return true;
    if (has_gi) return false;
    if (has_local) return true;

    return false;    
}


static void GetDateString (string & out_date_str, const CDate& date)
{
    if (date.IsStr()) {
        out_date_str = date.GetStr();
    } else if (date.IsStd()) {
        if (date.GetStd().IsSetYear()) {
            date.GetDate(&out_date_str, "%{%3N %{%D, %}%}%Y");
        }
    }
}


static string s_GetKeywordForStructuredComment (const CUser_object& obj)
{
    string prefix = CComment_rule::GetStructuredCommentPrefix(obj);
    if (NStr::IsBlank(prefix)) {
        return "";
    }
    string keyword = CComment_rule::KeywordForPrefix(prefix);
    return keyword;
}


void CValidError_bioseq::CheckForMultipleStructuredComments(const CBioseq& seq)
{
    // list of structured comment prefixes
    vector<string> sc_prefixes;

    CSeqdesc_CI di(m_Scope->GetBioseqHandle(seq), CSeqdesc::e_User);
    while (di) {
        const CUser_object& obj = di->GetUser();
        if (CComment_rule::IsStructuredComment(obj)) {
            string prefix = CComment_rule::GetStructuredCommentPrefix(obj, false);
            if (!NStr::IsBlank(prefix)) {
                sc_prefixes.push_back(prefix);
            }
        }
        ++di;
    }

    sort(sc_prefixes.begin(), sc_prefixes.end());
    int num_seen = 0;
    string previous = "";
    ITERATE(vector<string>, it, sc_prefixes) {
        if (NStr::EqualNocase(previous, *it)) {
            num_seen++;
        } else {
            if (num_seen > 1) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_MultipleStrucComms, 
                    "Multiple structured comments with prefix " + previous,
                        seq);
            }
            previous = *it;
            num_seen = 1;
        }
    }
    if (num_seen > 1) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_MultipleStrucComms, 
            "Multiple structured comments with prefix " + previous,
                seq);
    } 

}


bool s_IsGenbankMasterAccession(const string& acc)
{
    bool rval = false;
    switch (acc.length()) {
        case 12:
            if (NStr::EndsWith(acc, "000000")) {
                rval = true;
            }
            break;
        case 13:
            if (NStr::EndsWith(acc, "0000000")) {
                rval = true;
            }
            break;
        case 14:
            if (NStr::EndsWith(acc, "00000000")) {
                rval = true;
            }
            break;
        default:
            break;
    }
    return rval;
}

bool s_IsMasterAccession(const CSeq_id& id)
{
    bool rval = false;
    switch (id.Which()) {
        case CSeq_id::e_Other:
            if (id.GetOther().IsSetAccession()) {
                const string& acc = id.GetOther().GetAccession();
                switch (acc.length()) {
                    case 15:
                        if (NStr::EndsWith(acc, "000000")) {
                            rval = true;
                        }
                        break;
                    case 16:
                    case 17:
                        if (NStr::EndsWith(acc, "0000000")) {
                            rval = true;
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        case CSeq_id::e_Genbank:
            if (id.GetGenbank().IsSetAccession()) {
                rval = s_IsGenbankMasterAccession(id.GetGenbank().GetAccession());
            }
            break;
        case CSeq_id::e_Ddbj:
            if (id.GetDdbj().IsSetAccession()) {
                rval = s_IsGenbankMasterAccession(id.GetDdbj().GetAccession());
            }
            break;
        case CSeq_id::e_Embl:
            if (id.GetEmbl().IsSetAccession()) {
                rval = s_IsGenbankMasterAccession(id.GetEmbl().GetAccession());
            }
            break;
        case CSeq_id::e_Tpg:
            if (id.GetTpg().IsSetAccession()) {
                rval = s_IsGenbankMasterAccession(id.GetTpg().GetAccession());
            }
            break;
        default:
            break;
    }

    return rval;
}


bool CValidError_bioseq::IsMaster(const CBioseq& seq)
{
    if (!seq.IsSetId() || !seq.IsSetInst() || !seq.GetInst().IsSetRepr() ||
        seq.GetInst().GetRepr() != CSeq_inst::eRepr_virtual) {
        return false;
    }
    bool is_master = false;
    ITERATE(CBioseq::TId, id, seq.GetId()) {
        is_master |= s_IsMasterAccession(**id);
    }

    return is_master;
}


bool CValidError_bioseq::IsWp(CBioseq_Handle bsh)
{
    bool is_WP = false;

    FOR_EACH_SEQID_ON_BIOSEQ_HANDLE(sid_itr, bsh) {
        CSeq_id_Handle sid = *sid_itr;
        switch (sid.Which()) {
        case NCBI_SEQID(Embl):
        case NCBI_SEQID(Ddbj):
        case NCBI_SEQID(Other):
        case NCBI_SEQID(Genbank):
        {
            CConstRef<CSeq_id> id = sid.GetSeqId();
            const CTextseq_id& tsid = *id->GetTextseq_Id();
            if (tsid.IsSetAccession()) {
                const string& acc = tsid.GetAccession();
                TACCN_CHOICE type = CSeq_id::IdentifyAccession(acc);

                if (type == NCBI_ACCN(refseq_unique_prot)) {
                    is_WP = true;
                    break;
                }
            }
            break;
        }
        default:
            break;
        }
    }
    return is_WP;
}


bool CValidError_bioseq::IsEmblOrDdbj(const CBioseq& seq)
{
    bool embl_or_ddbj = false;
    ITERATE(CBioseq::TId, id, seq.GetId()) {
        if ((*id)->IsEmbl() || (*id)->IsDdbj()) {
            embl_or_ddbj = true;
            break;
        }
    }

    return embl_or_ddbj;
}


bool CValidError_bioseq::IsGenbank(const CBioseq& seq)
{
    ITERATE(CBioseq::TId, id, seq.GetId()) {
        if ((*id)->IsGenbank()) {
            return true;
        }
    }
    return false;
}


bool CValidError_bioseq::IsRefSeq(const CBioseq& seq)
{
    ITERATE(CBioseq::TId, id, seq.GetId()) {
        if ((*id)->IsOther()) {
            return true;
        }
    }
    return false;
}


void CValidError_bioseq::x_CheckForMultipleComments(CBioseq_Handle bsh)
{
    CSeqdesc_CI di(bsh, CSeqdesc::e_Comment);
    while (di) {
        CSeqdesc_CI di2 = di;
        ++di2;
        while (di2) {
            if (NStr::EqualNocase(di->GetComment(), di2->GetComment())) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleComments,
                    "Undesired multiple comment descriptors, identical text",
                    *(bsh.GetParentEntry().GetCompleteSeq_entry()), *di2);
            }
            ++di2;
        }
        ++di;
    }
}


void CValidError_bioseq::CheckForMissingChromosome(CBioseq_Handle bsh)
{
    if (!bsh) {
        return;
    }
    CConstRef<CBioseq> b = bsh.GetCompleteBioseq();
    if (!b) {
        return;
    }
    if (SeqIsPatent(*b)) {
        return;
    }
    bool is_nc = false;
    bool is_ac = false;
    FOR_EACH_SEQID_ON_BIOSEQ(id_it, *b) {
        if ((*id_it)->IsOther() && (*id_it)->GetOther().IsSetAccession()) {
            string accession = (*id_it)->GetOther().GetAccession();
            if (NStr::StartsWith(accession, "NC_")) {
                is_nc = true;
                break;
            } else if (NStr::StartsWith(accession, "AC_")) {
                is_ac = true;
                break;
            }
        }
    }
    if (!is_nc && !is_ac) {
        return;
    }

    if (m_report_missing_chromosome) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_MissingChromosome, "Missing chromosome qualifier on NC or AC RefSeq record",
            *b);
    }

}


// Validate CSeqdesc within the context of a bioseq. 
// See: CValidError_desc for validation of standalone CSeqdesc,
// and CValidError_descr for validation of descriptors in the context
// of descriptor list.
void CValidError_bioseq::ValidateSeqDescContext(const CBioseq& seq)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    size_t  num_gb   = 0,
            num_embl = 0,
            num_pir  = 0,
            num_pdb  = 0,
            num_prf  = 0,
            num_sp   = 0;
    CConstRef<CSeqdesc> last_gb, 
                        last_embl,
                        last_pir,
                        last_pdb,
                        last_prf,
                        last_sp;
    CConstRef<CSeqdesc> create_desc, update_desc;
    string create_str;
    int biomol = -1;
      int tech = -1, completeness = -1;
    CConstRef<COrg_ref> org;

      string name_str = "";
      string comment_str = "";

    bool is_genome_assembly = false;
    bool is_assembly = false;
    bool is_finished_status = false;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    // some validation is for descriptors that affect a bioseq,
    // other validation is only for descriptors _on_ a bioseq
    FOR_EACH_DESCRIPTOR_ON_BIOSEQ (it, seq) {
        const CSeqdesc& desc = **it;

        switch ( desc.Which() ) {
            case CSeqdesc::e_Title:
                  {
                      string title = desc.GetTitle();
                      size_t pos = NStr::Find(title, "[");
                      if (pos != string::npos) {
                          pos = NStr::Find(title, "=", pos + 1);
                      }
                      if (pos != string::npos) {
                          pos = NStr::Find(title, "]", pos + 1);
                      }
                      if (pos != string::npos) {
                          bool report_fasta_brackets = true;
                          FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
                              if ((*id_it)->IsGeneral()) {
                                  const CDbtag& dbtag = (*id_it)->GetGeneral();
                                  if (dbtag.IsSetDb()) {
                                      if (NStr::EqualNocase(dbtag.GetDb(), "TMSMART")
                                          || NStr::EqualNocase(dbtag.GetDb(), "BankIt")) {
                                          report_fasta_brackets = false;
                                          break;
                                      }
                                  }
                              }
                          }
                          if (report_fasta_brackets) {
                              IF_EXISTS_CLOSEST_BIOSOURCE (bs_ref, seq, NULL) {
                                  const CBioSource& bsrc = (*bs_ref).GetSource();
                                  if (bsrc.IsSetOrg()) {
                                      const COrg_ref& orgref = bsrc.GetOrg();
                                      if (orgref.IsSetTaxname()) {
                                          string taxname = orgref.GetTaxname();
                                          size_t pos = NStr::Find (taxname, "=");
                                          if (pos != string::npos) {
                                              pos = NStr::Find (title, taxname);
                                              if (pos != string::npos) {
                                                  report_fasta_brackets = false;
                                              }
                                          }
                                      }
                                  }
                              }
                          }
                          if (report_fasta_brackets) {
                              PostErr(eDiag_Warning, eErr_SEQ_DESCR_FastaBracketTitle, 
                                      "Title may have unparsed [...=...] construct",
                                      ctx, desc);
                          }
                      }
            }
            break;
        default:
            break;
        }
    }

    // collect keywords - needed for validating structured comments
    vector<string> keywords;
    for ( CSeqdesc_CI di(m_CurrentHandle, CSeqdesc::e_Genbank); di; ++ di ) {
        FOR_EACH_KEYWORD_ON_GENBANKBLOCK (key, di->GetGenbank()) {
            keywords.push_back (*key);
        }
    }

    for ( CSeqdesc_CI di(m_CurrentHandle); di; ++ di ) {
        const CSeqdesc& desc = *di;

        switch ( desc.Which() ) {

        case CSeqdesc::e_Org:
            if ( !org ) {
                org = &(desc.GetOrg());
            }
            ValidateOrgContext(di, desc.GetOrg(), *org, seq, desc);
            break;

        case CSeqdesc::e_Pir:
            num_pir++;
            last_pir = &desc;
            break;

        case CSeqdesc::e_Genbank:
            num_gb++;
            last_gb = &desc;
                  ValidateGBBlock (desc.GetGenbank(), seq, desc);
            break;

        case CSeqdesc::e_Sp:
            num_sp++;
            last_sp = &desc;
            break;

        case CSeqdesc::e_Embl:
            num_embl++;
            last_embl = &desc;
            break;

        case CSeqdesc::e_Create_date:
            {
            const CDate& current = desc.GetCreate_date();
            if ( create_desc ) {
                if ( create_desc->GetCreate_date().Compare(current) != CDate::eCompare_same && m_Imp.HasGiOrAccnVer() ) {
                    string current_str;
                    GetDateString(current_str, current);
                    const CSeq_entry *use_ctx = ctx.GetParentEntry();
                    if (use_ctx == NULL || !use_ctx->IsSet() 
                        || !use_ctx->GetSet().IsSetClass()
                        || use_ctx->GetSet().GetClass() != CBioseq_set::eClass_nuc_prot) {
                        use_ctx = &ctx;
                    }
                    PostErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentDates,
                        "Inconsistent create_dates [" + current_str +
                        "] and [" + create_str + "]", *use_ctx, desc);
                }
            } else {
                create_desc = &desc;
                GetDateString(create_str, create_desc->GetCreate_date());
            }

            if ( update_desc ) {
                ValidateUpdateDateContext(update_desc->GetUpdate_date(), current, seq, *create_desc);
            }
            }
            break;

        case CSeqdesc::e_Update_date:
            if ( create_desc ) {
                ValidateUpdateDateContext(desc.GetUpdate_date(), create_desc->GetCreate_date(), 
                    seq, *create_desc);
            } else {
                update_desc = &desc;
            }
            break;

        case CSeqdesc::e_Prf:
            num_prf++;
            last_prf = &desc;
            break;

        case CSeqdesc::e_Pdb:
            num_pdb++;
            last_pdb = &desc;
            break;

        case CSeqdesc::e_Source:
            {
                const CSeqdesc::TSource& source = desc.GetSource();

                        m_Imp.ValidateBioSourceForSeq (source, desc, &ctx, m_CurrentHandle);

                        // look at orgref in comparison to other descs
                        if (source.IsSetOrg()) {
                            const COrg_ref& orgref = source.GetOrg();
                            if ( !org ) {
                                org = &orgref;
                            }
                            ValidateOrgContext(di, orgref, 
                                                *org, seq, desc);
                        }
                
            }
            break;

        case CSeqdesc::e_Molinfo:
            ValidateMolInfoContext(desc.GetMolinfo(), biomol, tech, completeness, seq, desc);
            break;

        case CSeqdesc::e_User:
            if (desc.GetUser().IsSetType()) {
                const CUser_object& usr = desc.GetUser();
                const CObject_id& oi = usr.GetType();
                if (oi.IsStr() && NStr::CompareNocase(oi.GetStr(), "TpaAssembly") == 0 
                    && !s_IsTPAAssemblyOkForBioseq(seq, m_Imp.IsRefSeqConventions())) {
                    string id_str;
                    seq.GetLabel(&id_str, CBioseq::eContent, false);
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_TPAassemblyWithoutTPAKeyword,
                        "Non-TPA record " + id_str + " should not have TpaAssembly object", seq);
                }
                if (desc.GetUser().IsRefGeneTracking()
                    && !CValidError_imp::IsWGSIntermediate(seq) 
                    && !m_Imp.IsRefSeq()) {
                              PostErr(eDiag_Error, eErr_SEQ_DESCR_RefGeneTrackingOnNonRefSeq, 
                                      "RefGeneTracking object should only be in RefSeq record", 
                                      ctx, desc);
                } else if (desc.GetUser().IsStructuredComment()) {
                    const CUser_object& obj = desc.GetUser();
                    string keyword = s_GetKeywordForStructuredComment(obj);
                    if (!NStr::IsBlank(keyword)) {
                        // does sequence have keyword?
                        bool found = false;
                        ITERATE (vector<string>, key, keywords) {
                            if (NStr::EqualNocase(keyword, *key)) {
                                found = true;
                                break;
                            }
                        }
                        // is structured comment valid for this keyword?
                        if (!m_DescrValidator.ValidateStructuredComment(desc.GetUser(), desc, false)) {
                            // error if keyword is present
                            if (found) {
                                PostErr(eDiag_Info, eErr_SEQ_DESCR_BadKeywordForStrucComm, 
                                        "Structured Comment is non-compliant, keyword should be removed", ctx, desc);
                            }
                        }
                    } else {
                        ITERATE (CUser_object::TData, field, obj.GetData()) {
                            if ((*field)->IsSetLabel() && (*field)->GetLabel().IsStr()) {
                                if (NStr::EqualNocase((*field)->GetLabel().GetStr(), "StructuredCommentPrefix")) {
                                    const string& prefix = (*field)->GetData().GetStr();
                                    if (NStr::EqualCase(prefix, "##Genome-Assembly-Data-START##")) {
                                        is_genome_assembly = true;
                                    } else if (NStr::EqualCase(prefix, "##Assembly-Data-START##")) {
                                        is_assembly = true;
                                    }
                                } else if (NStr::EqualNocase((*field)->GetLabel().GetStr(), "Current Finishing Status")) {
                                    const string& prefix = (*field)->GetData().GetStr();
                                    if (NStr::EqualCase(prefix, "Finished")) {
                                        is_finished_status = true;
                                    }
                                }
                            }
                        }
                    }
                } else if (oi.IsStr() && NStr::EqualCase(oi.GetStr(), "DBLink")) {
                    m_dblink_count++;
                    FOR_EACH_USERFIELD_ON_USEROBJECT (ufd_it, usr) {
                        const CUser_field& fld = **ufd_it;
                        if (FIELD_IS_SET_AND_IS(fld, Label, Str)) {
                            const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                            if (NStr::EqualNocase(label_str, "Trace Assembly Archive")) {
                                m_taa_count++;
                            } else if (NStr::EqualNocase(label_str, "BioSample")) {
                                m_bs_count++;
                            } else if (NStr::EqualNocase(label_str, "Assembly")) {
                                m_as_count++;
                            } else if (NStr::EqualNocase(label_str, "ProbeDB")) {
                                m_pdb_count++;
                            } else if (NStr::EqualNocase(label_str, "Sequence Read Archive")) {
                                m_sra_count++;
                            } else if (NStr::EqualNocase(label_str, "BioProject")) {
                                m_bp_count++;
                            } else {
                                m_unknown_count++;
                            }
                        }
                    }
                }
            }
            break;
        case CSeqdesc::e_Title:
            {
                string title = desc.GetTitle();

                // nucleotide refseq sequences should start with the organism name,
                // protein refseq sequences should end with the organism name.
                bool is_refseq = m_Imp.IsRefSeqConventions();
                FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
                    if ((*id_it)->IsOther()) {
                        is_refseq = true;
                        break;
                    }
                }
                if (is_refseq) {
                    string taxname = "";

                    CSeqdesc_CI src_i(m_CurrentHandle, CSeqdesc::e_Source);
                    if (src_i && src_i->GetSource().IsSetOrg() && src_i->GetSource().GetOrg().IsSetTaxname()) {
                        taxname = src_i->GetSource().GetOrg().GetTaxname();
                    }
                    if (seq.IsAa()) {
                        CConstRef<CSeq_feat> cds = m_Imp.GetCDSGivenProduct(seq);
                        if (cds) {
                            CConstRef<CSeq_feat> src_f = GetBestOverlappingFeat(
                                cds->GetLocation(),
                                CSeqFeatData::eSubtype_biosrc,
                                eOverlap_Contained,
                                *m_Scope);
                            if (src_f && src_f->IsSetData() && src_f->GetData().IsBiosrc()
                                && src_f->GetData().GetBiosrc().IsSetTaxname()) {
                                taxname = src_f->GetData().GetBiosrc().GetTaxname();
                            }
                        }
                    }
                    if (!NStr::IsBlank(taxname) && !SeqIsPatent (seq)) {
                        if (NStr::StartsWith(title, "PREDICTED: ")) {
                            title = title.substr (11);
                        }
                        if (seq.IsNa()) {
                            if (!NStr::StartsWith(title, taxname, NStr::eNocase)) {
                                PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrganismInTitle, 
                                        "RefSeq nucleotide title does not start with organism name",
                                        ctx, desc);
                            }
                        } else if (seq.IsAa()) {
                            taxname = "[" + taxname + "]";
                            if (!NStr::EndsWith(title, taxname, NStr::eNocase)) {
                                if (! IsWp(bsh) || NStr::FindNoCase(title, taxname) == NPOS) {
                                    PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrganismInTitle,
                                            "RefSeq protein title does not end with organism name",
                                            ctx, desc);
                                }
                            }
                        }
                    }
                }
            }
            break;

        case CSeqdesc::e_Name:
            name_str = desc.GetName();
            if (!NStr::IsBlank(name_str)) {                
                CSeqdesc_CI di2 = di;
                ++di2;
                while (di2) {
                    if (di2->IsName()) {
                        if (NStr::EqualNocase(name_str, di2->GetName())) {
                            PostErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleNames, 
                                    "Undesired multiple name descriptors, identical text",
                                    ctx, desc);
                        } else {
                            PostErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleNames, 
                                    "Undesired multiple name descriptors, different text",
                                    ctx, desc);
                        }
                    }
                    ++di2;
                }
            }
            break;

        case CSeqdesc::e_Method:
            if (!seq.IsAa()) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_ProteinTechniqueOnNucleotide,
                        "Nucleic acid with protein sequence method",
                        ctx, desc);
            }
            break;

        default:
            break;
        }
    }

    CheckForMissingChromosome(m_CurrentHandle);

    if (is_genome_assembly && is_finished_status && tech == CMolInfo::eTech_wgs) {
        const string& buf = seq.GetId().front()->AsFastaString();
        PostErr (eDiag_Warning, eErr_SEQ_DESCR_FinishedStatusForWGS, "WGS record " + buf + " should not have Finished status", seq);
    }

    if (IsMaster(seq)) {
        if (tech == CMolInfo::eTech_wgs && !is_genome_assembly) {
            PostErr(IsEmblOrDdbj(seq) ? eDiag_Warning : eDiag_Error, eErr_SEQ_INST_WGSMasterLacksStrucComm, "WGS master without Genome Assembly Data user object", seq);
        }
        if (tech == CMolInfo::eTech_tsa && !is_assembly) {
            PostErr(IsEmblOrDdbj(seq) ? eDiag_Warning : eDiag_Error, eErr_SEQ_INST_TSAMasterLacksStrucComm, "TSA master without Assembly Data user object", seq);
        }
    }

    if ( num_gb > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentGenBankblocks,
            "Multiple GenBank blocks", ctx, *last_gb);
    }

    if ( num_embl > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple EMBL blocks", ctx, *last_embl);
    }

    if ( num_pir > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PIR blocks", ctx, *last_pir);
    }

    if ( num_pdb > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PDB blocks", ctx, *last_pdb);
    }

    if ( num_prf > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple PRF blocks", ctx, *last_prf);
    }

    if ( num_sp > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
            "Multiple SWISS-PROT blocks", ctx, *last_sp);
    }

    ValidateModifDescriptors (seq);
    ValidateMoltypeDescriptors (seq);

    CheckForMultipleStructuredComments(seq);
    x_CheckForMultipleComments(bsh);
}


void CValidError_bioseq::ValidateGBBlock
(const CGB_block& gbblock,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    bool has_tpa_inf = false, has_tpa_exp = false;
    FOR_EACH_KEYWORD_ON_GENBANKBLOCK (it, gbblock) {
        if (NStr::EqualNocase (*it, "TPA:experimental")) {
            has_tpa_exp = true;
        } else if (NStr::EqualNocase (*it, "TPA:inferential")) {
            has_tpa_inf = true;
        }
    }
    if (has_tpa_inf && has_tpa_exp) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentTPA,
                 "TPA:experimental and TPA:inferential should not both be in the same set of keywords",
                 ctx, desc);
    }
}


bool CValidError_bioseq::GetTSAConflictingBiomolTechErrors(const CBioseq& seq)
{
    bool rval = false;
    if (seq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
        PostErr(eDiag_Error, eErr_SEQ_INST_TSAshouldBNotBeDNA,
            "TSA sequence should not be DNA", seq);
        rval = true;
    }
    return rval;
}


void CValidError_bioseq::ValidateMolInfoContext
(const CMolInfo& minfo,
 int& seq_biomol,
 int& last_tech, 
 int& last_completeness,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    bool is_synthetic_construct = false;
    bool is_artificial = false;

    CSeqdesc_CI src_di(m_CurrentHandle, CSeqdesc::e_Source);

    while (src_di) {
        if (!is_synthetic_construct) {
            is_synthetic_construct = m_Imp.IsSyntheticConstruct(src_di->GetSource());
        }
        if (!is_artificial) {
            is_artificial = m_Imp.IsArtificial(src_di->GetSource());
        }
        x_ValidateMolInfoForBioSource(src_di->GetSource(), minfo, desc);

        ++src_di;
    }

    if ( minfo.IsSetBiomol() ) {
        int biomol = minfo.GetBiomol();
        if ( seq_biomol < 0 ) {
            seq_biomol = biomol;
        }
        
        switch ( biomol ) {
        case CMolInfo::eBiomol_peptide:
            if ( seq.IsNa() ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidMolInfo,
                    "Nucleic acid with Molinfo = peptide", ctx, desc);
            }
            break;

        case CMolInfo::eBiomol_other_genetic:
            if ( !is_artificial ) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_MoltypeOtherGenetic,
                    "Molinfo-biomol = other genetic", ctx, desc);
            }
            break;
            
        case CMolInfo::eBiomol_other:
            if ( !m_Imp.IsXR() ) {
                if ( !IsSynthetic(seq) ) {
                    if ( !x_IsMicroRNA(seq)) {
                        PostErr(eDiag_Warning, eErr_SEQ_DESCR_MoltypeOther,
                            "Molinfo-biomol other used", ctx, desc);
                    }
                }
            }
            break;
            
        default:  // the rest are nucleic acid
            if ( seq.IsAa() ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                    "Molinfo-biomol [" + NStr::IntToString(biomol) +
                    "] used on protein", ctx, desc);
            } else {
                if ( biomol != seq_biomol ) {
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentMolInfo,
                        "Inconsistent Molinfo-biomol [" + 
                        NStr::IntToString(seq_biomol) + "] and [" +
                        NStr::IntToString(biomol) + "]", ctx, desc);
                }
            }
        }
        // look for double-stranded mRNA
        if (biomol == CMolInfo::eBiomol_mRNA
            && seq.IsNa() 
            && seq.IsSetInst() && seq.GetInst().IsSetStrand()
            && seq.GetInst().GetStrand() != CSeq_inst::eStrand_not_set
            && seq.GetInst().GetStrand() != CSeq_inst::eStrand_ss) {
            PostErr(eDiag_Error, eErr_SEQ_INST_mRNAshouldBeSingleStranded, 
                    "mRNA should be single stranded not double stranded", ctx, desc);
        }
    } else {
        if (is_synthetic_construct && !seq.IsAa()) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_SyntheticConstructWrongMolType, "synthetic construct should have other-genetic", ctx, desc);
        }
    }


    if ( minfo.IsSetTech() ) {        
        int tech = minfo.GetTech();

        if ( seq.IsNa() ) {
            switch ( tech ) {
            case CMolInfo::eTech_concept_trans:
            case CMolInfo::eTech_seq_pept:
            case CMolInfo::eTech_both:
            case CMolInfo::eTech_seq_pept_overlap:
            case CMolInfo::eTech_seq_pept_homol:
            case CMolInfo::eTech_concept_trans_a:
                PostErr(eDiag_Error, eErr_SEQ_DESCR_ProteinTechniqueOnNucleotide,
                    "Nucleic acid with protein sequence method", ctx, desc);
                break;
            default:
                break;
            }
        } else {
            switch ( tech ) {
            case CMolInfo::eTech_est:
            case CMolInfo::eTech_sts:
            case CMolInfo::eTech_genemap:
            case CMolInfo::eTech_physmap:
            case CMolInfo::eTech_htgs_1:
            case CMolInfo::eTech_htgs_2:
            case CMolInfo::eTech_htgs_3:
            case CMolInfo::eTech_fli_cdna:
            case CMolInfo::eTech_htgs_0:
            case CMolInfo::eTech_htc:
            case CMolInfo::eTech_wgs:
            case CMolInfo::eTech_barcode:
            case CMolInfo::eTech_composite_wgs_htgs:
                PostErr(eDiag_Error, eErr_SEQ_DESCR_NucleotideTechniqueOnProtein,
                    "Protein with nucleic acid sequence method", ctx, desc);
                break;
            default:
                break;
            }
        }

        switch ( tech) {
        case CMolInfo::eTech_sts:
        case CMolInfo::eTech_survey:
        case CMolInfo::eTech_wgs:
        case CMolInfo::eTech_htgs_0:
        case CMolInfo::eTech_htgs_1:
        case CMolInfo::eTech_htgs_2:
        case CMolInfo::eTech_htgs_3:
        case CMolInfo::eTech_composite_wgs_htgs:
            if (tech == CMolInfo::eTech_sts  &&
                seq.GetInst().GetMol() == CSeq_inst::eMol_rna  &&
                minfo.IsSetBiomol()  &&
                minfo.GetBiomol() == CMolInfo::eBiomol_mRNA) {
                // !!!
                // Ok, there are some STS sequences derived from 
                // cDNAs, so do not report these
            } else if (!minfo.IsSetBiomol()  
                       || minfo.GetBiomol() != CMolInfo::eBiomol_genomic) {
                PostErr(eDiag_Error, eErr_SEQ_INST_HTGS_STS_GSS_WGSshouldBeGenomic,
                    "HTGS/STS/GSS/WGS sequence should be genomic", seq);
            } else if (seq.GetInst().GetMol() != CSeq_inst::eMol_dna  &&
                seq.GetInst().GetMol() != CSeq_inst::eMol_na) {
                PostErr(eDiag_Error, eErr_SEQ_INST_HTGS_STS_GSS_WGSshouldNotBeRNA,
                    "HTGS/STS/GSS/WGS sequence should not be RNA", seq);
            } 
            break;
        case CMolInfo::eTech_est:
            if (tech == CMolInfo::eTech_est  &&
                 ((! minfo.IsSetBiomol())  ||
                minfo.GetBiomol() != CMolInfo::eBiomol_mRNA)) {
                PostErr(eDiag_Warning, eErr_SEQ_INST_ESTshouldBemRNA,
                    "EST sequence should be mRNA", seq);
            }
            break;
        case CMolInfo::eTech_tsa:
            GetTSAConflictingBiomolTechErrors(seq);
            break;
        default:
            break;
        }

        if (tech == CMolInfo::eTech_htgs_3) {
            CSeqdesc_CI gb_i(m_CurrentHandle, CSeqdesc::e_Genbank);
            bool has_draft = false;
            bool has_prefin = false;
            bool has_activefin = false;
            bool has_fulltop = false;
            while (gb_i) {
                if (gb_i->GetGenbank().IsSetKeywords()) {
                    CGB_block::TKeywords::const_iterator key_it = gb_i->GetGenbank().GetKeywords().begin();
                    while (key_it != gb_i->GetGenbank().GetKeywords().end()) {
                        if (NStr::EqualNocase(*key_it, "HTGS_DRAFT")) {
                            has_draft = true;
                        } else if (NStr::EqualNocase(*key_it, "HTGS_PREFIN")) {
                            has_prefin = true;
                        } else if (NStr::EqualNocase(*key_it, "HTGS_ACTIVEFIN")) {
                            has_activefin = true;
                        } else if (NStr::EqualNocase(*key_it, "HTGS_FULLTOP")) {
                            has_fulltop = true;
                        }
                        ++key_it;
                    }
                }
                ++gb_i;
            }
            if (has_draft) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadHTGSeq,
                        "HTGS 3 sequence should not have HTGS_DRAFT keyword", seq);
            }
            if (has_prefin) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadHTGSeq,
                        "HTGS 3 sequence should not have HTGS_PREFIN keyword", seq);
            }
            if (has_activefin) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadHTGSeq,
                        "HTGS 3 sequence should not have HTGS_ACTIVEFIN keyword", seq);
            }
            if (has_fulltop) {
                PostErr(eDiag_Error, eErr_SEQ_INST_BadHTGSeq,
                        "HTGS 3 sequence should not have HTGS_FULLTOP keyword", seq);
            }
        }                        

        if (last_tech > 0) {
            if (last_tech != tech) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentMolInfoTechnique,
                    "Inconsistent Molinfo-tech [" + NStr::IntToString (last_tech)
                    + "] and [" + NStr::IntToString(tech) + "]", ctx, desc);
            }
        } else {
            last_tech = tech;
        }
    } else {
        if (last_tech > -1) {
            if (last_tech != 0) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentMolInfoTechnique,
                    "Inconsistent Molinfo-tech [" + NStr::IntToString (last_tech)
                    + "] and [0]", ctx, desc);
            }
        } else {
            last_tech = 0;
        }
    }

      if (minfo.IsSetCompleteness()) {
            if (last_completeness > 0) {
                  if (last_completeness != minfo.GetCompleteness()) {
                      PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentMolInfo,
                              "Inconsistent Molinfo-completeness [" + NStr::IntToString (last_completeness)
                              + "] and [" + NStr::IntToString(minfo.GetCompleteness()) + "]", ctx, desc);
                  }
            } else {
                  last_completeness = minfo.GetCompleteness();
            }
    } else {
        if (last_completeness > -1) {
            if (last_completeness != 0) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentMolInfo,
                          "Inconsistent Molinfo-completeness [" + NStr::IntToString (last_completeness)
                          + "] and [0]", ctx, desc);
                  }
            } else {
                  last_completeness = 0;
            }
    }
    // need to look at closest molinfo descriptor, not all of them
    CConstRef<CSeqdesc> closest_molinfo = seq.GetClosestDescriptor(CSeqdesc::e_Molinfo);
    if (closest_molinfo) {
        const CMolInfo& molinfo = closest_molinfo->GetMolinfo();
        if (molinfo.IsSetCompleteness()) {
            x_ValidateCompletness(seq, molinfo);
        }
    }

}


// refactored in VR-918
void CValidError_bioseq::x_ValidateMolInfoForBioSource(
    const CBioSource& src,
    const CMolInfo& minfo,
    const CSeqdesc& desc
)
{
    // common reality checks before calling x_CheckSingleStrandedRNAViruses and x_ReportLineageConflictWithMol
    if (m_CurrentHandle.IsAa()) {
        return;
    }
    if (!src.IsSetOrg() || !src.GetOrg().IsSetLineage()) {
        return;
    }

    const string& lineage = src.GetOrg().GetLineage();
    if (! NStr::StartsWith(lineage, "Viruses; ")) {
        return;
    }

    string stranded_mol = s_GetStrandedMolStringFromLineage(lineage);
    if (NStr::FindNoCase(stranded_mol, "unknown") != NPOS) {
        return;
    }

    const CSeq_entry* ctx = m_CurrentHandle.GetParentEntry().GetCompleteSeq_entry();

    CMolInfo::TBiomol biomol = CMolInfo::eBiomol_unknown;
    if (minfo.IsSetBiomol()) {
        biomol = minfo.GetBiomol();
    }

    x_CheckSingleStrandedRNAViruses(src, lineage, stranded_mol, biomol, m_CurrentHandle, desc, ctx);

    CSeq_inst::EMol mol = CSeq_inst::eMol_not_set;
    if (m_CurrentHandle.IsSetInst_Mol()) {
        mol = m_CurrentHandle.GetInst_Mol();
    }

    x_ReportLineageConflictWithMol(lineage, stranded_mol, biomol, mol, desc, ctx);
}


void CValidError_bioseq::x_ReportLineageConflictWithMol(
    const string& lineage,
    const string& stranded_mol,
    const CMolInfo::TBiomol biomol,
    CSeq_inst::EMol mol,
    const CSerialObject& obj,
    const CSeq_entry    *ctx
)
{
    if (mol != CSeq_inst::eMol_rna && mol != CSeq_inst::eMol_dna)  {
        return;
    }

    // special cases
    if (NStr::FindNoCase(lineage, "Retroviridae") != NPOS && NStr::EqualNocase(stranded_mol, "ssRNA-RT")) {
        // retrovirus can be rna or dna
        return;
    }

    if (NStr::EqualNocase(stranded_mol, "dsRNA")) {
        if (biomol != CMolInfo::eBiomol_genomic) {
             m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "dsRNA virus should be genomic RNA",
                    obj, ctx);
            return;
        }
    }

    // otherwise look for molecule match regardless of strandedness
    if (mol == CSeq_inst::eMol_dna) {
        if (NStr::FindNoCase(stranded_mol, "DNA") != NPOS) {
            return;
        }
    } else if (mol == CSeq_inst::eMol_rna) {
        if (NStr::FindNoCase(stranded_mol, "RNA") != NPOS) {
            return;
        }
    }

    string mssg = "";
    if (NStr::FindNoCase(stranded_mol, "ssRNA") != NPOS) {
        mssg = "single-stranded RNA";
    } else if (NStr::FindNoCase(stranded_mol, "dsRNA") != NPOS) {
        mssg = "double-stranded RNA";
    } else if (NStr::FindNoCase(stranded_mol, "ssDNA") != NPOS) {
        mssg = "single-stranded DNA";
    } else if (NStr::FindNoCase(stranded_mol, "dsDNA") != NPOS) {
        mssg = "double-stranded DNA";
    } else if (NStr::FindNoCase(stranded_mol, "RNA") != NPOS) {
        mssg = "unknown-stranded RNA";
    } else if (NStr::FindNoCase(stranded_mol, "DNA") != NPOS) {
        mssg = "unknown-stranded DNA";
    }

    m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MolInfoConflictsWithBioSource,
        "Taxonomy indicates " + mssg +
        ", molecule type (" + CSeq_inst::GetMoleculeClass(mol) +
        ") is conflicting.",
        obj, ctx);
}


void CValidError_bioseq::x_CheckSingleStrandedRNAViruses(
    const CBioSource& source,
    const string& lineage,
    const string& stranded_mol,
    const CMolInfo::TBiomol biomol,
    const CBioseq_Handle& bsh,
    const CSerialObject& obj,
    const CSeq_entry    *ctx
)
{
    if (NStr::FindNoCase(stranded_mol, "ssRNA") == NPOS) {
        return;
    }
    if (NStr::FindNoCase(stranded_mol, "DNA") != NPOS) {
        return;
    }

    const bool is_ambisense = NStr::EqualNocase(stranded_mol, "ssRNA(+/-)");

    // special cases
    if (is_ambisense) {
        if (biomol != CMolInfo::eBiomol_genomic && biomol != CMolInfo::eBiomol_cRNA) {
            m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "Ambisense virus should be genomic RNA or cRNA",
                    obj, ctx);
        }
        return;
    }

    if (NStr::FindNoCase(lineage, "Retroviridae") != NPOS && NStr::EqualNocase(stranded_mol, "ssRNA-RT")) {
        if (biomol != CMolInfo::eBiomol_genomic) {
             m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "Retrovirus should be genomic RNA or genomic DNA",
                    obj, ctx);
        }
        return;
    }

    bool negative_strand_virus = false;
    bool plus_strand_virus = false;
    if (NStr::FindNoCase(stranded_mol, "-)") != NPOS) {
        negative_strand_virus = true;
    }
    if (NStr::FindNoCase(stranded_mol, "(+") != NPOS) {
        plus_strand_virus = true;
    }
    if (!negative_strand_virus && !plus_strand_virus) {
        return;
    }

    bool is_synthetic = false;
    if (source.GetOrg().IsSetDivision() && NStr::EqualNocase(source.GetOrg().GetDivision(), "SYN")) {
        is_synthetic = true;
    } else if (source.IsSetOrigin()) {
        CBioSource_Base::TOrigin orig = source.GetOrigin();
        if (orig == CBioSource::eOrigin_mut
            || orig == CBioSource::eOrigin_artificial
            || orig == CBioSource::eOrigin_synthetic) {
            is_synthetic = true;
        }
    }

    bool has_cds = false;
    bool has_plus_cds = false;
    bool has_minus_cds = false;

    CFeat_CI cds_ci(bsh, SAnnotSelector(CSeqFeatData::e_Cdregion));
    while (cds_ci) {
        has_cds = true;
        if (cds_ci->GetLocation().GetStrand() == eNa_strand_minus) {
            has_minus_cds = true;
        }
        else {
            has_plus_cds = true;
        }
        if (has_minus_cds && has_plus_cds) {
            break;
        }

        ++cds_ci;
    }

    bool has_minus_misc_feat = false;
    bool has_plus_misc_feat = false;

    if (!has_cds) {
        CFeat_CI misc_ci(bsh, SAnnotSelector(CSeqFeatData::eSubtype_misc_feature));
        while (misc_ci) {
            if (misc_ci->IsSetComment()
                && NStr::FindNoCase(misc_ci->GetComment(), "nonfunctional") != NPOS) {
                if (misc_ci->GetLocation().GetStrand() == eNa_strand_minus) {
                    has_minus_misc_feat = true;
                }
                else {
                    has_plus_misc_feat = true;
                }
            }
            if (has_minus_misc_feat && has_plus_misc_feat) {
                break;
            }
            ++misc_ci;
        }
    }

    if (negative_strand_virus) {

        if (has_minus_cds) {
            if (biomol != CMolInfo::eBiomol_genomic) {
                m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "Negative-sense single-stranded RNA virus with minus strand CDS should be genomic RNA",
                    obj, ctx);
            }
        }

        if (has_plus_cds && !is_synthetic && !is_ambisense) {
            if (biomol != CMolInfo::eBiomol_cRNA) {
                m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "Negative-sense single-stranded RNA virus with plus strand CDS should be cRNA",
                    obj, ctx);
            }
        }

        if (has_minus_misc_feat) {
            if (biomol != CMolInfo::eBiomol_genomic) {
                m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "Negative-sense single-stranded RNA virus with nonfunctional minus strand misc_feature should be genomic RNA",
                    obj, ctx);
            }
        }

        if (has_plus_misc_feat && !is_synthetic && !is_ambisense) {
            if (biomol != CMolInfo::eBiomol_cRNA) {
                m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "Negative-sense single-stranded RNA virus with nonfunctional plus strand misc_feature should be cRNA",
                    obj, ctx);
            }
        }
    }

    if (plus_strand_virus) {

        if (has_minus_cds) {
            m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                "CDS should not be on minus strand of a positive-sense single-stranded RNA virus",
                obj, ctx);
        }

        if (!is_synthetic && !is_ambisense) {
            if (biomol != CMolInfo::eBiomol_genomic) {
                m_Imp.PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "Positive-sense single-stranded RNA virus should be genomic RNA",
                    obj, ctx);
            }
        }
    }
}


typedef map<string, string> TViralMap;
static const TViralMap kViralStrandMap {
    {"root",                                        "dsDNA"},
    {"Alphasatellitidae",                           "ssDNA"},
    {"Anelloviridae",                               "ssDNA(-)"},
    {"Bacilladnaviridae",                           "ssDNA"},
    {"Bidnaviridae",                                "ssDNA"},
    {"Circoviridae",                                "ssDNA(+/-)"},
    {"Geminiviridae",                               "ssDNA(+/-)"},
    {"Genomoviridae",                               "ssDNA"},
    {"Hepadnaviridae",                              "dsDNA-RT"},
    {"Inoviridae",                                  "ssDNA(+)"},
    {"Microviridae",                                "ssDNA(+)"},
    {"Nanoviridae",                                 "ssDNA(+)"},
    {"Ortervirales",                                "ssRNA-RT"},
    {"Caulimoviridae",                              "dsDNA-RT"},
    {"Parvoviridae",                                "ssDNA(+/-)"},
    {"Alphapleolipovirus",                          "dsDNA; ssDNA"},
    {"Riboviria",                                   "RNA"},
    {"Albetovirus",                                 "ssRNA(+)"},
    {"Alphatetraviridae",                           "ssRNA(+)"},
    {"Alvernaviridae",                              "ssRNA(+)"},
    {"Amalgaviridae",                               "dsRNA"},
    {"Astroviridae",                                "ssRNA(+)"},
    {"Aumaivirus",                                  "ssRNA(+)"},
    {"Avsunviroidae",                               "ssRNA"},
    {"Barnaviridae",                                "ssRNA(+)"},
    {"Benyviridae",                                 "ssRNA(+)"},
    {"Birnaviridae",                                "dsRNA"},
    {"Botourmiaviridae",                            "ssRNA(+)"},
    {"Botybirnavirus",                              "dsRNA"},
    {"Bromoviridae",                                "ssRNA(+)"},
    {"Caliciviridae",                               "ssRNA(+)"},
    {"Carmotetraviridae",                           "ssRNA(+)"},
    {"Chrysoviridae",                               "dsRNA"},
    {"Closteroviridae",                             "ssRNA(+)"},
    {"Cystoviridae",                                "dsRNA"},
    {"Deltavirus",                                  "ssRNA(-)"},
    {"dsRNA viruses",                               "dsRNA"},
    {"Endornaviridae",                              "dsRNA"},
    {"Flaviviridae",                                "ssRNA(+)"},
    {"Hepeviridae",                                 "ssRNA(+)"},
    {"Hypoviridae",                                 "ssRNA(+)"},
    {"Idaeovirus",                                  "ssRNA(+)"},
    {"Kitaviridae",                                 "ssRNA(+)"},
    {"Leviviridae",                                 "ssRNA(+)"},
    {"Luteoviridae",                                "ssRNA(+)"},
    {"Matonaviridae",                               "ssRNA(+)"},
    {"Megabirnaviridae",                            "dsRNA"},
    {"Narnaviridae",                                "ssRNA(+)"},
    {"Haploviricotina",                             "ssRNA(-)"},
    {"Arenaviridae",                                "ssRNA(+/-)"},
    {"Coguvirus",                                   "ssRNA(-)"},
    {"Cruliviridae",                                "ssRNA(-)"},
    {"Fimoviridae",                                 "ssRNA(-)"},
    {"Hantaviridae",                                "ssRNA(-)"},
    {"Leishbuviridae",                              "ssRNA(-)"},
    {"Mypoviridae",                                 "ssRNA(-)"},
    {"Nairoviridae",                                "ssRNA(-)"},
    {"Peribunyaviridae",                            "ssRNA(-)"},
    {"Phasmaviridae",                               "ssRNA(-)"},
    {"Banyangvirus",                                "ssRNA(+/-)"},
    {"Beidivirus",                                  "ssRNA(-)"},
    {"Goukovirus",                                  "ssRNA(-)"},
    {"Horwuvirus",                                  "ssRNA(-)"},
    {"Hudivirus",                                   "ssRNA(-)"},
    {"Hudovirus",                                   "ssRNA(-)"},
    {"Kabutovirus",                                 "ssRNA(-)"},
    {"Laulavirus",                                  "ssRNA(-)"},
    {"Mobuvirus",                                   "ssRNA(-)"},
    {"Phasivirus",                                  "ssRNA(-)"},
    {"Phlebovirus",                                 "ssRNA(+/-)"},
    {"Pidchovirus",                                 "ssRNA(-)"},
    {"Tenuivirus",                                  "ssRNA(-)"},
    {"Wenrivirus",                                  "ssRNA(-)"},
    {"Wubeivirus",                                  "ssRNA(-)"},
    {"Tospoviridae",                                "ssRNA(+/-)"},
    {"Wupedeviridae",                               "ssRNA(-)"},
    {"Insthoviricetes",                             "ssRNA(-)"},
    {"Nidovirales",                                 "ssRNA(+)"},
    {"Nodaviridae",                                 "ssRNA(+)"},
    {"Papanivirus",                                 "ssRNA(+)"},
    {"Partitiviridae",                              "dsRNA"},
    {"Permutotetraviridae",                         "ssRNA(+)"},
    {"Picobirnaviridae",                            "dsRNA"},
    {"Picornavirales",                              "ssRNA(+)"},
    {"Pospiviroidae",                               "ssRNA"},
    {"Potyviridae",                                 "ssRNA(+)"},
    {"Quadriviridae",                               "dsRNA"},
    {"Reoviridae",                                  "dsRNA"},
    {"Sarthroviridae",                              "ssRNA(+)"},
    {"Sinaivirus",                                  "ssRNA(+)"},
    {"Solemoviridae",                               "ssRNA(+)"},
    {"Solinviviridae",                              "ssRNA(+)"},
    {"Togaviridae",                                 "ssRNA(+)"},
    {"Tombusviridae",                               "ssRNA(+)"},
    {"Totiviridae",                                 "dsRNA"},
    {"Tymovirales",                                 "ssRNA(+)"},
    {"Virgaviridae",                                "ssRNA(+)"},
    {"Virtovirus",                                  "ssRNA(+)"},
    {"ssRNA viruses",                               "ssRNA"},
    {"unclassified ssRNA viruses",                  "ssRNA"},
    {"unclassified ssRNA negative-strand viruses",  "ssRNA(-)"},
    {"unclassified ssRNA positive-strand viruses",  "ssRNA(+)"},
    {"unclassified viroids",                        "ssRNA"},
    {"DNA satellites",                              "DNA"},
    {"RNA satellites",                              "RNA"},
    {"Smacoviridae",                                "ssDNA"},
    {"Spiraviridae",                                "ssDNA(+)"},
    {"Tolecusatellitidae",                          "ssDNA"},
    {"unclassified viruses",                        "unknown"},
    {"unclassified DNA viruses",                    "DNA"},
    {"unclassified archaeal dsDNA viruses",         "dsDNA"},
    {"unclassified dsDNA phages",                   "dsDNA"},
    {"unclassified dsDNA viruses",                  "dsDNA"},
    {"unclassified ssDNA bacterial viruses",        "ssDNA"},
    {"unclassified ssDNA viruses",                  "ssDNA"},
    {"environmental samples",                       "unknown"}
};


static TViralMap s_ViralTaxonMap;
static const TViralMap* s_ViralMap = &kViralStrandMap;
static volatile bool s_ViralMapInitialized;
DEFINE_STATIC_FAST_MUTEX(s_ViralMapMutex);


string CValidError_bioseq::s_GetStrandedMolStringFromLineage(const string& lineage)
{
    if ( !s_ViralMapInitialized ) {
        CFastMutexGuard GUARD(s_ViralMapMutex);
        if ( !s_ViralMapInitialized ) {
            try {
                CTaxon1 tax;
                CTaxon1::TInfoList moltypes;
                if ( tax.GetInheritedPropertyDefines( "genomic_moltype", moltypes ) ) {
                  for (auto it: moltypes) {
                      string sName;
                      if ( tax.GetScientificName(TAX_ID_FROM(TIntId, it->GetIval1()), sName ) ) {
                          if ( it->GetIval2() == 1 ) {
                              s_ViralTaxonMap [sName] = it->GetSval();
                          }
                      }
                   }
                }
                if ( !s_ViralTaxonMap.empty() ) {
                    s_ViralMap = &s_ViralTaxonMap;
                }
            } catch (CException&) {
                // report if desired (at severity info or warning, probably)
            }
            s_ViralMapInitialized = true;
        }
    }

    // Retroviridae no longer in list
    if (NStr::FindNoCase(lineage, "Retroviridae") != NPOS) {
        return "ssRNA-RT";
    }

    // Topsovirus (ambisense) not in list
    if (NStr::FindNoCase(lineage, "Tospovirus") != NPOS) {
        return "ssRNA(+/-)";
    }

    // Tenuivirus has several segments, most of which are ambisense
    if (NStr::FindNoCase(lineage, "Tenuivirus") != NPOS) {
        return "ssRNA(+/-)";
    }

    // Arenaviridae is ambisense, has priority over old-style checks
    if (NStr::FindNoCase(lineage, "Arenaviridae") != NPOS) {
        return "ssRNA(+/-)";
    }

    // Phlebovirus is ambisense, has priority over old-style checks
    if (NStr::FindNoCase(lineage, "Phlebovirus") != NPOS) {
        return "ssRNA(+/-)";
    }

    // unclassified viruses have old-style lineage
    if (NStr::FindNoCase(lineage, "negative-strand viruses") != NPOS) {
        return "ssRNA(-)";
    }
    if (NStr::FindNoCase(lineage, "positive-strand viruses") != NPOS) {
        return "ssRNA(+)";
    }

    if (s_ViralMap) {
        for (const auto & x : *s_ViralMap) {
            if (NStr::FindNoCase(lineage, x.first) != NPOS) {
                return x.second;
            }
        }
    }

    // use root value for default
    return "dsDNA";
}


void CValidError_bioseq::ReportModifInconsistentError (int new_mod, int& old_mod, const CSeqdesc& desc, const CSeq_entry& ctx)
{
    if (old_mod >= 0) {
        if (new_mod != old_mod) {
            PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                     "Inconsistent GIBB-mod [" + NStr::IntToString (old_mod) + "] and ["
                     + NStr::IntToString (new_mod) + "]", ctx, desc);
        }
    } else {
        old_mod = new_mod;
    }
}


void CValidError_bioseq::ValidateModifDescriptors (const CBioseq& seq)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    int last_na_mod = -1;
    int last_organelle = -1;
    int last_partialness = -1;
    int last_left_right = -1;

    CSeqdesc_CI desc_ci(m_CurrentHandle, CSeqdesc::e_Modif);
    while (desc_ci) {
      CSeqdesc::TModif modif = desc_ci->GetModif();
        CSeqdesc::TModif::const_iterator it = modif.begin();
        while (it != modif.end()) {
            int modval = *it;
            switch (modval) {
                case eGIBB_mod_dna:
                case eGIBB_mod_rna:
                    if (m_CurrentHandle && m_CurrentHandle.IsAa()) {
                        PostErr (eDiag_Error, eErr_SEQ_DESCR_InvalidForTypeGIBB, 
                                 "Nucleic acid GIBB-mod [" + NStr::IntToString (modval) + "] on protein",
                                 ctx, *desc_ci);
                    } else {
                        ReportModifInconsistentError (modval, last_na_mod, *desc_ci, ctx);
                    }
                    break;
                case eGIBB_mod_mitochondrial:
                case eGIBB_mod_chloroplast:
                case eGIBB_mod_kinetoplast:
                case eGIBB_mod_cyanelle:
                case eGIBB_mod_macronuclear:
                    ReportModifInconsistentError (modval, last_organelle, *desc_ci, ctx);
                    break;
                case eGIBB_mod_partial:
                case eGIBB_mod_complete:
                    ReportModifInconsistentError (modval, last_partialness, *desc_ci, ctx);
                    if (last_left_right >= 0 && modval == eGIBB_mod_complete) {
                        PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                                 "Inconsistent GIBB-mod [" + NStr::IntToString (last_left_right) + "] and ["
                                 + NStr::IntToString (modval) + "]",
                                 ctx, *desc_ci);
                    }
                    break;
                case eGIBB_mod_no_left:
                case eGIBB_mod_no_right:
                    if (last_partialness == eGIBB_mod_complete) {
                        PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                                 "Inconsistent GIBB-mod [" + NStr::IntToString (last_partialness) + "] and ["
                                 + NStr::IntToString (modval) + "]",
                                 ctx, *desc_ci);
                    }
                    last_left_right = modval;
                    break;
                default:
                    break;

            }
            ++it;
        }
        ++desc_ci;
    }
}


void CValidError_bioseq::ValidateMoltypeDescriptors (const CBioseq& seq)
{
    const CSeq_entry& ctx = *seq.GetParentEntry();

    int last_na_mol = 0;
    CSeqdesc_CI desc_ci(m_CurrentHandle, CSeqdesc::e_Mol_type);
    while (desc_ci) {
        int modval = desc_ci->GetMol_type();
        switch (modval) {
            case eGIBB_mol_peptide:
                if (!seq.IsAa()) {
                    PostErr (eDiag_Error, eErr_SEQ_DESCR_InvalidForTypeGIBB, 
                             "Nucleic acid with GIBB-mol = peptide",
                             ctx, *desc_ci);
                }
                break;
            case eGIBB_mol_unknown:
            case eGIBB_mol_other:
                PostErr (eDiag_Error, eErr_SEQ_DESCR_InvalidForTypeGIBB, 
                         "GIBB-mol unknown or other used",
                         ctx, *desc_ci);
                break;
            default:                   // the rest are nucleic acid
                if (seq.IsAa()) {
                    PostErr (eDiag_Error, eErr_SEQ_DESCR_InvalidForTypeGIBB, 
                             "GIBB-mol [" + NStr::IntToString (modval) + "] used on protein",
                             ctx, *desc_ci);
                } else {
                    if (last_na_mol) {
                        if (last_na_mol != modval) {
                            PostErr (eDiag_Error, eErr_SEQ_DESCR_Inconsistent, 
                              "Inconsistent GIBB-mol [" + NStr::IntToString (last_na_mol)
                              + "] and [" + NStr::IntToString (modval) + "]",
                              ctx, *desc_ci);
                        }
                    } else {
                        last_na_mol = modval;
                    }
                }
                break;
        }
        ++desc_ci;
    }
}


bool CValidError_bioseq::IsSynthetic(const CBioseq& seq) const
{
    if ( m_CurrentHandle ) {
        CSeqdesc_CI sd(m_CurrentHandle, CSeqdesc::e_Source);
        if ( sd ) {
            const CSeqdesc::TSource& source = sd->GetSource();
            if ( source.CanGetOrigin()  &&
                 source.GetOrigin() == CBioSource::eOrigin_synthetic ) {
                return true;
            }
            if ( source.CanGetOrg()  &&  source.GetOrg().CanGetOrgname() ) {
                const COrgName& org_name = source.GetOrg().GetOrgname();
                if ( org_name.CanGetDiv() ) {
                    if ( NStr::CompareNocase(org_name.GetDiv(), "SYN") == 0 ) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}


bool CValidError_bioseq::x_IsMicroRNA(const CBioseq& seq) const 
{
    SAnnotSelector selector(CSeqFeatData::e_Rna);
    selector.SetFeatSubtype(CSeqFeatData::eSubtype_otherRNA);
    CFeat_CI fi(m_CurrentHandle, selector);

    for ( ; fi; ++fi ) {
        const CRNA_ref& rna_ref = fi->GetData().GetRna();
        if ( rna_ref.IsSetExt()  &&  rna_ref.GetExt().IsName() ) {
            if ( NStr::Find(rna_ref.GetExt().GetName(), "microRNA") != NPOS ) {
                return true;
            }
        }
    }
    return false;
}


void CValidError_bioseq::ValidateUpdateDateContext
(const CDate& update,
 const CDate& create,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    if ( update.Compare(create) == CDate::eCompare_before && m_Imp.HasGiOrAccnVer() ) {

        string create_str;
        GetDateString(create_str, create);
        string update_str;
        GetDateString(update_str, update);

        string err_msg = "Inconsistent create_date [";
        err_msg += create_str;
        err_msg += "] and update_date [";
        err_msg += update_str;
        err_msg += "]";

        CSeq_entry *ctx = seq.GetParentEntry();
        PostErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentDates,
            err_msg, *ctx, desc);
    }
}


void CValidError_bioseq::ValidateOrgContext
(const CSeqdesc_CI& curr,
 const COrg_ref& this_org,
 const COrg_ref& org,
 const CBioseq& seq,
 const CSeqdesc& desc)
{
    if ( this_org.IsSetTaxname()  &&  org.IsSetTaxname() ) {
        if ( this_org.GetTaxname() != org.GetTaxname() ) {
            bool is_wp = false;
            FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, seq) {
                const CSeq_id& sid = **sid_itr;
                CSeq_id::E_Choice typ = sid.Which();
                if (typ == CSeq_id::e_Other) {
                    if (sid.GetOther().IsSetAccession()) {
                        string acc =sid.GetOther().GetAccession().substr(0, 3);
                        if (acc == "WP_") {
                            is_wp = true;
                        }
                    }
                }
            }
            if (! is_wp) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentTaxName,
                    "Inconsistent organism names [" + this_org.GetTaxname() + 
                    "] and [" + org.GetTaxname() + "]",
                    *seq.GetParentEntry(), desc);
            }
        }
    }
}


static bool s_ReportableCollision (const CGene_ref& g1, const CGene_ref& g2)
{
    if (g1.IsSetLocus() && !NStr::IsBlank(g1.GetLocus())
        && g2.IsSetLocus() && !NStr::IsBlank(g2.GetLocus())
        && NStr::EqualNocase(g1.GetLocus(), g2.GetLocus())) {
        return true;
    } else if (g1.IsSetLocus_tag() && !NStr::IsBlank(g1.GetLocus_tag())
        && g2.IsSetLocus_tag() && !NStr::IsBlank(g2.GetLocus_tag())
        && NStr::EqualNocase(g1.GetLocus_tag(), g2.GetLocus_tag())) {
        return true;
    } else if (g1.IsSetDesc() && !NStr::IsBlank(g1.GetDesc())
        && g2.IsSetDesc() && !NStr::IsBlank(g2.GetDesc())
        && NStr::EqualNocase(g1.GetDesc(), g2.GetDesc())) {
        return false;
    } else {
        return true;
    }
}


void CValidError_bioseq::x_CompareStrings
(const TStrFeatMap& str_feat_map,
 const string& type)
{
    bool is_gene_locus = NStr::EqualNocase (type, "names");

    // iterate through multimap and compare strings
    bool first = true;
    bool reported_first = false;
    bool lastIsSplit = false;
    const string* strp = 0;
    const CSeq_feat* feat = 0;
    ITERATE (TStrFeatMap, it, str_feat_map) {
        if ( first ) {
            first = false;
            strp = &(it->first);
            feat = it->second;
            lastIsSplit = (bool) (feat->IsSetExcept() &&
                                  feat->IsSetExcept_text() &&
                                  NStr::FindNoCase (feat->GetExcept_text(), "gene split at ") != string::npos);
            continue;
        }

        string message = "";
        if (NStr::Equal (*strp, it->first)) {
            message = "Colliding " + type + " in gene features";
        } else if (NStr::EqualNocase (*strp, it->first)) {
            message = "Colliding " + type + " (with different capitalization) in gene features";
        }

        if (!NStr::IsBlank (message)
            && s_ReportableCollision(feat->GetData().GetGene(), it->second->GetData().GetGene())) {

            bool suppress_message = false;
            if (m_Imp.IsSmallGenomeSet()) {
                if (feat->IsSetExcept() && feat->IsSetExcept_text()
                    && NStr::FindNoCase (feat->GetExcept_text(), "trans-splicing") != string::npos &&
                    it->second->IsSetExcept() && it->second->IsSetExcept_text()
                    && NStr::FindNoCase (it->second->GetExcept_text(), "trans-splicing") != string::npos) {
                    // suppress for trans-spliced genes on small genome set
                    suppress_message = true;
                }
            }

            if (suppress_message) {
                // suppress for trans-spliced genes on small genome set
            } else if (is_gene_locus && sequence::Compare(feat->GetLocation(),
                                                   (*it->second).GetLocation(),
                                                   m_Scope,
                                                   sequence::fCompareOverlapping) == eSame) {
                PostErr (eDiag_Info, eErr_SEQ_FEAT_DuplicateGeneConflictingLocusTag,
                         message + ", but feature locations are identical", *it->second);
            } else if (!is_gene_locus) {
                const CSeq_feat* nfeat = it->second;
                bool isSplit = (bool) (nfeat->IsSetExcept() &&
                                      nfeat->IsSetExcept_text() &&
                                      NStr::FindNoCase (nfeat->GetExcept_text(), "gene split at ") != string::npos);

                if (!suppress_message && ((! isSplit) || (! lastIsSplit))) {                    
                    if (!reported_first) {
                        // for now, don't report first colliding gene - C Toolkit doesn't
                        // PostErr(sev, err, message, *feat);
                        reported_first = true;
                    }


                    PostErr(eDiag_Error, eErr_SEQ_FEAT_CollidingLocusTags, message, *it->second);
                }
            }
        }
        strp = &(it->first);
        feat = it->second;
        lastIsSplit = (bool) (feat->IsSetExcept() &&
                              feat->IsSetExcept_text() &&
                              NStr::FindNoCase (feat->GetExcept_text(), "gene split at ") != string::npos);
    }
}


void CValidError_bioseq::ValidateCollidingGenes(const CBioseq& seq)
{
    try {
        TStrFeatMap label_map;
        TStrFeatMap locus_tag_map;
        TStrFeatMap locus_map;
        TStrFeatMap syn_map;

        // Loop through genes and insert into multimap sorted by
        // gene label / locus_tag -- case insensitive
        if (m_GeneIt) {
            ITERATE(CCacheImpl::TFeatValue, fi, *m_GeneIt) {
                const CSeq_feat& feat = fi->GetOriginalFeature();
                // record label
                string label;
                GetLabel(feat, &label, feature::fFGL_Content, m_Scope);
                label_map.insert(TStrFeatMap::value_type(label, &feat));
                // record locus_tag
                const CGene_ref& gene = feat.GetData().GetGene();
                if ( gene.IsSetLocus_tag()  &&  !NStr::IsBlank(gene.GetLocus_tag()) ) {
                    locus_tag_map.insert(TStrFeatMap::value_type(gene.GetLocus_tag(), &feat));
                }
                // record locus
                if ( gene.IsSetLocus() && !NStr::IsBlank(gene.GetLocus())) {
                    locus_map.insert(TStrFeatMap::value_type(gene.GetLocus(), &feat));
                }
                // record synonyms
                FOR_EACH_SYNONYM_ON_GENEREF (syn_it, gene) {
                    syn_map.insert(TStrFeatMap::value_type((*syn_it), &feat));
                }
            }
            x_CompareStrings(label_map, "names");
            x_CompareStrings(locus_tag_map, "locus_tags");
            // look for synonyms on genes that match locus of different genes,
            // but not if gpipe
            if (!m_Imp.IsGpipe()) {
                ITERATE (TStrFeatMap, syngene_it, syn_map) {
                    TStrFeatMap::iterator gene_it = locus_map.find(syngene_it->first);
                    if (gene_it != locus_map.end()) {
                        bool found = false;
                        FOR_EACH_SYNONYM_ON_GENEREF (syn_it, gene_it->second->GetData().GetGene()) {
                            if (NStr::Equal (*syn_it, syngene_it->first)) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_IdenticalGeneSymbolAndSynonym, 
                                    "gene synonym has same value (" + syngene_it->first + ") as locus of another gene feature",
                                    *syngene_it->second);
                        }
                    }
                }   
            }
        }

    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating colliding genes. EXCEPTION: ") +
                e.what(), seq);
        }
    }
}

void CValidError_bioseq::ValidateCompleteGenome(const CBioseq& seq)
{
    // This check is applicable to nucleotide sequences only
    if (!seq.IsNa()) {
        return;
    }

    // EMBL or DDBJ check
    bool embl_ddbj = false;
    ITERATE(CBioseq::TId, id, seq.GetId()) {
        if ((*id)->IsDdbj() || (*id)->IsEmbl()) {
            embl_ddbj = true;
            break;
        }
    }

    if (!embl_ddbj) {
        return;
    }

    // Completness check
    bool complete_genome = false;
    CSeqdesc_CI ti(m_CurrentHandle, CSeqdesc::e_Title);
    complete_genome = (ti && NStr::Find(ti->GetTitle(), "complete genome") != string::npos);

    if (!complete_genome) {

        CSeqdesc_CI ei(m_CurrentHandle, CSeqdesc::e_Embl);
        if (ei && ei->GetEmbl().IsSetKeywords()) {

            ITERATE(CEMBL_block::TKeywords, keyword, ei->GetEmbl().GetKeywords()) {
                if (NStr::EqualNocase(*keyword, "complete genome")) {
                    complete_genome = true;
                    break;
                }
            }
        }
    }

    if (!complete_genome) {
        return;
    }

    // Division check
    CSeqdesc_CI si(m_CurrentHandle, CSeqdesc::e_Source);
    if (!si || !si->GetSource().IsSetDivision() || si->GetSource().GetDivision() != "BCT") {
        return;
    }

    // Check for BioProject accession
    bool bioproject_accession_set = false;
    for (CSeqdesc_CI ui(m_CurrentHandle, CSeqdesc::e_User); ui; ++ui) {

        if (ui->GetUser().IsSetData() && ui->GetUser().IsSetType() && ui->GetUser().GetType().IsStr() && NStr::EqualCase(ui->GetUser().GetType().GetStr(), "DBLink")) {
            
            bioproject_accession_set = !ui->GetUser().GetData().empty();
            break;
        }
    }

    if (bioproject_accession_set)
        return;

    // Delta-seq check
    bool no_gaps = true;
    if (seq.IsSetInst() && seq.GetInst().IsSetRepr() && seq.GetInst().GetRepr() == CSeq_inst::eRepr_delta && seq.GetInst().IsSetExt() && seq.GetInst().GetExt().IsDelta()) {

        const CDelta_ext& delta = seq.GetInst().GetExt().GetDelta();
        if (delta.IsSet()) {

            ITERATE(CDelta_ext::Tdata, part, delta.Get()) {

                if ((*part)->IsLiteral()) {
                    const CSeq_literal& literal = (*part)->GetLiteral();
                    if (literal.IsSetLength() && !literal.IsSetSeq_data()) {
                        no_gaps = false;
                        break;
                    }

                    if (literal.IsSetSeq_data() && literal.GetSeq_data().IsGap()) {
                        no_gaps = false;
                        break;
                    }
                }
            }
        }
    }
    
    // Check for instantiated gaps
    if (no_gaps) {

        CFeat_CI feat(m_CurrentHandle, CSeqFeatData::eSubtype_gap);
        if (feat) {
            no_gaps = false;
        }
   }


    if (no_gaps) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_CompleteGenomeLacksBioProject,
                "No BioProject Accession exists for what appears to be a complete genome",
                seq);
    }
}


void CValidError_bioseq::ValidateIDSetAgainstDb(const CBioseq& seq)
{
    const CSeq_id*  gb_id = 0;
    CRef<CSeq_id> db_gb_id;
    TGi             gi = ZERO_GI,
                    db_gi = ZERO_GI;
    CRef<CSeq_id>   db_general_id(NULL);
    const CDbtag*   general_id = 0;

    FOR_EACH_SEQID_ON_BIOSEQ (id, seq) {
        switch ( (*id)->Which() ) {
        case CSeq_id::e_Genbank:
            gb_id = id->GetPointer();
            break;

        case CSeq_id::e_Gi:
            gi = (*id)->GetGi();
            break;

        case CSeq_id::e_General:
            general_id = &((*id)->GetGeneral());
            break;
    
        default:
            break;
        }
    }

    if ( gi == ZERO_GI  &&  gb_id != 0 ) {
        gi = GetGIForSeqId(*gb_id);
    }

    if ( gi <= ZERO_GI ) {
        return;
    }

    CScope::TIds id_set = GetSeqIdsForGI(gi);
    if ( !id_set.empty() ) {
        ITERATE( CScope::TIds, id, id_set ) {
            switch ( (*id).Which() ) {
            case CSeq_id::e_Genbank: 
                db_gb_id = CRef<CSeq_id> (new CSeq_id);
                db_gb_id->Assign (*(id->GetSeqId()));
                break;
                
            case CSeq_id::e_Gi:
                db_gi = (*id).GetGi();
                break;
                
            case CSeq_id::e_General:
                db_general_id.Reset(new CSeq_id());
                db_general_id->Assign(*((*id).GetSeqId()));
                break;
                
            default:
                break;
            }
        }

        string gi_str = NStr::NumericToString(gi);

        if ( db_gi != gi ) {
          PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
              "New gi number (" + gi_str + ")" +
              " does not match one in NCBI sequence repository (" + NStr::NumericToString(db_gi) + ")", 
              seq);
        }
        if ( (gb_id != 0)  &&  (db_gb_id != 0) ) {
          if ( !gb_id->Match(*db_gb_id) ) {
              PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                  "New accession (" + gb_id->AsFastaString() + 
                  ") does not match one in NCBI sequence repository (" + db_gb_id->AsFastaString() + 
                  ") on gi (" + gi_str + ")", seq);
          }
        } else if ( gb_id != NULL) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Gain of accession (" + gb_id->AsFastaString() + ") on gi (" +
                gi_str + ") compared to the NCBI sequence repository", seq);
        } else if ( db_gb_id != 0 ) {
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Loss of accession (" + db_gb_id->AsFastaString() + 
                ") on gi (" + gi_str + ") compared to the NCBI sequence repository", seq);
        }

        string new_gen_label, old_gen_label;
        if ( general_id  != 0  &&  db_general_id != 0 ) {
            if ( !general_id->Match(db_general_id->GetGeneral()) ) {
                db_general_id->GetGeneral().GetLabel(&old_gen_label);
                general_id->GetLabel(&new_gen_label);
                PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                    "New general ID (" + new_gen_label + 
                    ") does not match one in NCBI sequence repository (" + old_gen_label +
                    ") on gi (" + gi_str + ")", seq);
            }
        } else if ( general_id != 0 ) {
            general_id->GetLabel(&new_gen_label);
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Gain of general ID (" + new_gen_label + ") on gi (" + 
                gi_str + ") compared to the NCBI sequence repository", seq);
        } else if ( db_general_id != 0 ) {
            db_general_id->GetGeneral().GetLabel(&old_gen_label);
            PostErr(eDiag_Warning, eErr_SEQ_INST_UnexpectedIdentifierChange,
                "Loss of general ID (" + old_gen_label + ") on gi (" +
                gi_str + ") compared to the NCBI sequence repository", seq);
        }
    }
}


bool CValidError_bioseq::GraphsOnBioseq(const CBioseq& seq) const
{
    return CGraph_CI(m_CurrentHandle);
}


bool CValidError_bioseq::x_IsActiveFin(const CBioseq& seq) const
{
    CSeqdesc_CI gb_desc(m_CurrentHandle, CSeqdesc::e_Genbank);
    if ( gb_desc ) {
        const CGB_block& gb = gb_desc->GetGenbank();
        if ( gb.IsSetKeywords() ) {
            FOR_EACH_KEYWORD_ON_GENBANKBLOCK (iter, gb) {
                if ( NStr::CompareNocase(*iter, "HTGS_ACTIVEFIN") == 0 ) {
                    return true;
                }
            }
        }
    }
    return false;
}


size_t CValidError_bioseq::x_CountAdjacentNs(const CSeq_literal& lit)
{
    if ( !lit.CanGetSeq_data() ) {
        return 0;
    }

    const CSeq_data& lit_data = lit.GetSeq_data();
    CSeq_data data;
    size_t count = 0;
    size_t max = 0;
    size_t pos = 0;

    if (lit_data.IsIupacna()
        || lit_data.IsNcbi2na()
        || lit_data.IsNcbi4na()
        || lit_data.IsNcbi8na()) {
        CSeqportUtil::Convert(lit_data, &data, CSeq_data::e_Iupacna);
        ITERATE(string, res, data.GetIupacna().Get() ) {
            if ( *res == 'N' ) {
                ++count;
                if ( count > max ) {
                    max = count;
                }
            }
            else {
                count = 0;
            }
            // CSeqportUtil::Convert will convert more data 
            //than the specified length, if the data is present
            ++pos;
            if (pos >= lit.GetLength()) {
                break;
            }
        }
    } else {
        CSeqportUtil::Convert(lit_data, &data, CSeq_data::e_Iupacaa);
        ITERATE(string, res, data.GetIupacaa().Get() ) {
            if ( *res == 'N' ) {
                ++count;
                if ( count > max ) {
                    max = count;
                }
            }
            else {
                count = 0;
            }
            ++pos;
            // CSeqportUtil::Convert will convert more data 
            //than the specified length, if the data is present
            if (pos >= lit.GetLength()) {
                break;
            }
        }
    }

    return max;
}


bool CValidError_bioseq::x_IsDeltaLitOnly(const CSeq_inst& inst) const
{
    if ( inst.CanGetExt()  &&  inst.GetExt().IsDelta() ) {
        ITERATE(CDelta_ext::Tdata, iter, inst.GetExt().GetDelta().Get()) {
            if ( (*iter)->IsLoc() ) {
                return false;
            }
        }
    }
    return true;
}


bool s_HasTpaUserObject(CBioseq_Handle bsh)
{
    for ( CSeqdesc_CI it(bsh, CSeqdesc::e_User); it; ++it ) {
        const CUser_object& uo = it->GetUser();

        if ( uo.CanGetType()  &&  uo.GetType().IsStr()  &&
             NStr::CompareNocase(uo.GetType().GetStr(), "TpaAssembly") == 0 ) {
            return true;
        }
    }

    return false;
}


void CValidError_bioseq::CheckTpaHistory(const CBioseq& seq)
{
    if ( !s_HasTpaUserObject(m_CurrentHandle) ) {
        return;
    }

    if ( seq.CanGetInst()  &&  
         seq.GetInst().CanGetHist()  &&
         !seq.GetInst().GetHist().GetAssembly().empty() ) {
        m_Imp.IncrementTpaWithHistoryCount();
    } else {
        m_Imp.IncrementTpaWithoutHistoryCount();
    }
}


bool CValidError_bioseq::x_ReportUTRPair(const CSeq_feat& utr5, const CSeq_feat& utr3)
{
    CConstRef<CSeq_feat> gene3 = m_Imp.GetCachedGene(&utr3);
    if (gene3) {
        CConstRef<CSeq_feat> gene5 = m_Imp.GetCachedGene(&utr5);
        if (gene5 && gene3.GetPointer() == gene5.GetPointer()) {
            return true;
        }
    }
    return false;
}


void CValidError_bioseq::ValidateCDSUTR(const CBioseq& seq)
{
    if (!m_AllFeatIt) {
        return;
    }
    CConstRef<CSeq_feat> cds_minus(NULL);
    CConstRef<CSeq_feat> utr3_minus(NULL);
    CConstRef<CSeq_feat> utr5_minus(NULL);
    CConstRef<CSeq_feat> cds_plus(NULL);
    CConstRef<CSeq_feat> utr3_plus(NULL);
    CConstRef<CSeq_feat> utr5_plus(NULL);

    ITERATE(CCacheImpl::TFeatValue, f, *m_AllFeatIt) {
        ENa_strand strand = f->GetLocation().GetStrand();
        if (f->GetData().IsCdregion()) {
            if (strand == eNa_strand_minus) {
                cds_minus = f->GetSeq_feat();
            } else {
                cds_plus = f->GetSeq_feat();
            }
        } else if (f->GetData().GetSubtype() == CSeqFeatData::eSubtype_3UTR) {
            if (strand == eNa_strand_minus) {
                utr3_minus = f->GetSeq_feat();
            } else {
                utr3_plus = f->GetSeq_feat();
                if (!cds_plus && utr5_plus && x_ReportUTRPair(*utr5_plus, *utr3_plus)) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoCDSbetweenUTRs,
                            "CDS not between 5'UTR and 3'UTR on plus strand", *utr3_plus);
                }
                utr5_plus.Reset(NULL);
                cds_plus.Reset(NULL);
                utr3_plus.Reset(NULL);
            }
        } else if (f->GetData().GetSubtype() == CSeqFeatData::eSubtype_5UTR) {
            if (strand == eNa_strand_minus) {
                utr5_minus = f->GetSeq_feat();
                if (!cds_minus && utr3_minus && x_ReportUTRPair(*utr5_minus, *utr3_minus)) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoCDSbetweenUTRs,
                        "CDS not between 5'UTR and 3'UTR on minus strand", *utr5_minus);
                }
                utr5_minus.Reset(NULL);
                cds_minus.Reset(NULL);
                utr3_minus.Reset(NULL);
            } else {
                utr5_plus = f->GetSeq_feat();
            }
        }
    }
}


#if 0
CValidError_bioseq::CmRNACDSIndex::CmRNACDSIndex() 
{
}


CValidError_bioseq::CmRNACDSIndex::~CmRNACDSIndex()
{
}
#endif

bool s_IdXrefsAreReciprocal (const CMappedFeat &cds, const CMappedFeat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return false;
    }

    bool match = false;

    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
            match = true;
            break;
        }
    }
    if (!match) {
        return false;
    }
    match = false;
            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId() && s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
            match = true;
            break;
        }
    }

    return match;
}


unsigned int s_IdXrefsNotReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna)
{
    if (!cds.IsSetId() || !cds.GetId().IsLocal()
        || !mrna.IsSetId() || !mrna.GetId().IsLocal()) {
        return 0;
    }

    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, cds) {
        if ((*itx)->IsSetId() && !s_FeatureIdsMatch ((*itx)->GetId(), mrna.GetId())) {
            return 1;
        }
    }

            
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, mrna) {
        if ((*itx)->IsSetId() && !s_FeatureIdsMatch ((*itx)->GetId(), cds.GetId())) {
            return 1;
        }
    }

    if (!cds.IsSetProduct() || !mrna.IsSetExt()) {
        return 0;
    }
    
    TGi gi = ZERO_GI;
    if (cds.GetProduct().GetId()->IsGi()) {
        gi = cds.GetProduct().GetId()->GetGi();
    } else {
        // TODO: get gi for other kinds of SeqIds
    }

    if (gi == ZERO_GI) {
        return 0;
    }

    if (mrna.IsSetExt() && mrna.GetExt().IsSetType() && mrna.GetExt().GetType().IsStr()
        && NStr::EqualNocase(mrna.GetExt().GetType().GetStr(), "MrnaProteinLink")
        && mrna.GetExt().IsSetData()
        && mrna.GetExt().GetData().front()->IsSetLabel()
        && mrna.GetExt().GetData().front()->GetLabel().IsStr()
        && NStr::EqualNocase (mrna.GetExt().GetData().front()->GetLabel().GetStr(), "protein seqID")
        && mrna.GetExt().GetData().front()->IsSetData()
        && mrna.GetExt().GetData().front()->GetData().IsStr()) {
        string str = mrna.GetExt().GetData().front()->GetData().GetStr();
        try {
            CSeq_id id (str);
            if (id.IsGi()) {
                if (id.GetGi() == gi) {
                    return 0;
                } else {
                    return 2;
                }
            }
        } catch (CException ) {
        } catch (std::exception ) {
        }
    }
    return 0;
    
}




END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
