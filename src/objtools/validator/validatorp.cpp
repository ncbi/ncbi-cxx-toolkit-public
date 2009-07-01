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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Mati Shomrat, ....
 *
 * File Description:
 *   Implementation of private parts of the validator
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>

#include "validatorp.hpp"
#include "utilities.hpp"

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/PubStatus.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Affil.hpp>

#include <objtools/error_codes.hpp>
#include <util/sgml_entity.hpp>

#include <algorithm>

#include <serial/iterator.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


auto_ptr<CTextFsa> CValidError_imp::m_SourceQualTags;


// =============================================================================
//                            CValidError_imp Public
// =============================================================================

// Constructor
CValidError_imp::CValidError_imp
(CObjectManager& objmgr, 
 CValidError*       errs,
 Uint4              options) :
      m_ObjMgr(&objmgr),
      m_Scope(0),
      m_TSE(0),
      m_ErrRepository(errs),
      m_NonASCII((options & CValidator::eVal_non_ascii) != 0),
      m_SuppressContext((options & CValidator::eVal_no_context) != 0),
      m_ValidateAlignments((options & CValidator::eVal_val_align) != 0),
      m_ValidateExons((options & CValidator::eVal_val_exons) != 0),
      m_SpliceErr((options & CValidator::eVal_splice_err) != 0),
      m_OvlPepErr((options & CValidator::eVal_ovl_pep_err) != 0),
      m_RequireTaxonID((options & CValidator::eVal_need_taxid) != 0),
      m_RequireISOJTA((options & CValidator::eVal_need_isojta) != 0),
      m_ValidateIdSet((options & CValidator::eVal_validate_id_set) != 0),
      m_RemoteFetch((options & CValidator::eVal_remote_fetch) != 0),
      m_FarFetchMRNAproducts((options & CValidator::eVal_far_fetch_mrna_products) != 0),
      m_FarFetchCDSproducts((options & CValidator::eVal_far_fetch_cds_products) != 0),
      m_LocusTagGeneralMatch((options & CValidator::eVal_locus_tag_general_match) != 0),
      m_DoRubiscoText((options & CValidator::eVal_do_rubisco_test) != 0),
      m_IndexerVersion((options & CValidator::eVal_indexer_version) != 0),
      m_PerfBottlenecks((options & CValidator::eVal_perf_bottlenecks) != 0),
      m_IsStandaloneAnnot(false),
      m_NoPubs(false),
      m_NoBioSource(false),
      m_IsGPS(false),
      m_IsGED(false),
      m_IsPDB(false),
      m_IsPatent(false),
      m_IsRefSeq(false),
      m_IsNC(false),
      m_IsNG(false),
      m_IsNM(false),
      m_IsNP(false),
      m_IsNR(false),
      m_IsNS(false),
      m_IsNT(false),
      m_IsNW(false),
      m_IsXR(false),
      m_IsGI(false),
      m_IsGB(false),
      m_FeatLocHasGI(false),
      m_ProductLocHasGI(false),
      m_GeneHasLocusTag(false),
      m_ProteinHasGeneralID(false),
      m_PrgCallback(0),
      m_NumAlign(0),
      m_NumAnnot(0),
      m_NumBioseq(0),
      m_NumBioseq_set(0),
      m_NumDesc(0),
      m_NumDescr(0),
      m_NumFeat(0),
      m_NumGraph(0),
      m_NumMisplacedFeatures(0),
      m_IsTbl2Asn(false)
{
    if ( m_SourceQualTags.get() == 0 ) {
        InitializeSourceQualTags();
    }
}


// Destructor
CValidError_imp::~CValidError_imp()
{
}


// Error post methods
void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj)
{
    const CSeqdesc* desc = dynamic_cast < const CSeqdesc* > (&obj);
    if (desc != 0) {
        LOG_POST_X(1, Warning << "Seqdesc validation error using default context.");
        PostErr (sv, et, msg, GetTSE(), *desc);
        return;
    }
    const CSeq_feat* feat = dynamic_cast < const CSeq_feat* > (&obj);
    if (feat != 0) {
        PostErr (sv, et, msg, *feat);
        return;
    }
    const CBioseq* seq = dynamic_cast < const CBioseq* > (&obj);
    if (seq != 0) {
        PostErr (sv, et, msg, *seq);
        return;
    }
    const CBioseq_set* set = dynamic_cast < const CBioseq_set* > (&obj);
    if (set != 0) {
        PostErr (sv, et, msg, *set);
        return;
    }
    const CSeq_annot* annot = dynamic_cast < const CSeq_annot* > (&obj);
    if (annot != 0) {
        PostErr (sv, et, msg, *annot);
        return;
    }
    const CSeq_graph* graph = dynamic_cast < const CSeq_graph* > (&obj);
    if (graph != 0) {
        PostErr (sv, et, msg, *graph);
        return;
    }
    const CSeq_align* align = dynamic_cast < const CSeq_align* > (&obj);
    if (align != 0) {
        PostErr (sv, et, msg, *align);
        return;
    }
    const CSeq_entry* entry = dynamic_cast < const CSeq_entry* > (&obj);
    if (entry != 0) {
        PostErr (sv, et, msg, *entry);
        return;
    }
}


/*
void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TDesc          ds)
{
    // Append Descriptor label
    string desc = "DESCRIPTOR: ";
    ds.GetLabel (&desc, CSeqdesc::eBoth);
    desc += ", NO Descriptor Context";
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, *m_Scope);
}
*/


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TFeat          ft)
{
    // Add feature part of label
    string desc = "FEATURE: ";
    feature::GetLabel(ft, &desc, feature::eBoth, m_Scope);

    // Add feature location part of label
    string loc_label;
    if (m_SuppressContext) {
        CSeq_loc loc;
        loc.Assign(ft.GetLocation());
        ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
        loc.GetLabel(&loc_label);
    } else {
        ft.GetLocation().GetLabel(&loc_label);
    }
    if (loc_label.size() > 800) {
        loc_label.replace(797, NPOS, "...");
    }
    if (!loc_label.empty()) {
        desc += "[";
        desc += loc_label;
        desc += "]";
    }

    // Append label for bioseq of feature location
    if (!m_SuppressContext) {
        try {
            CBioseq_Handle hnd = m_Scope->GetBioseqHandle(ft.GetLocation());
            if( hnd ) {
                CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
                desc += "[";
                bc->GetLabel(&desc, CBioseq::eBoth);
                desc += "]";
            }
		} catch (CException &x1) {
		} catch (std::exception &x2) {
        };
    }

    // Append label for product of feature
    if (ft.IsSetProduct()) {
        loc_label.erase();
        if (m_SuppressContext) {
            CSeq_loc loc;
            loc.Assign(ft.GetProduct());
            ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
            loc.GetLabel(&loc_label);
        } else {
            ft.GetProduct().GetLabel(&loc_label);
        }
        if (loc_label.size() > 800) {
            loc_label.replace(797, NPOS, "...");
        }
        if (!loc_label.empty()) {
            desc += "[";
            desc += loc_label;
            desc += "]";
        }
    }
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ft, GetAccessionFromObjects(&ft, NULL, *m_Scope));
}


static void s_AppendBioseqLabel(string& str, CValidError_imp::TBioseq sq, bool supress_context)
{
    str += "BIOSEQ: ";
    sq.GetLabel(&str, CBioseq::eContent, supress_context);
}

void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TBioseq        sq)
{
    // Append bioseq label
    string desc;
    s_AppendBioseqLabel(desc, sq, m_SuppressContext);
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, sq, GetAccessionFromObjects(&sq, NULL, *m_Scope));
}


static void s_AppendSetLabel(string& str, CValidError_imp::TSet st, bool supress_context)
{
    str += "BIOSEQ-SET: ";

    // GetLabel for CBioseq_set does not follow C Toolkit conventions
    // AND is a horrible performance hit for sets with lots of sequences

    if (!supress_context && st.IsSetClass()) {
        const CEnumeratedTypeValues* tv =
            CBioseq_set::GetTypeInfo_enum_EClass();
        const string& cn = tv->FindName(st.GetClass(), true);
        str += cn;
        str += ": ";
    }

    const CBioseq* best = 0;
    CTypeConstIterator<CBioseq> si(ConstBegin(st));
    if (si) {
        best = &(*si);
    }
    // Add content to label.
    if (!best) {
        str += "(No Bioseqs)";
    } else {
        CNcbiOstrstream os;
        if (best->GetFirstId()) {
            os << best->GetFirstId()->DumpAsFasta();
            str += CNcbiOstrstreamToString(os);
        }
    }
}

void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TSet          st)
{
    // Append Bioseq_set label
    string desc = "BIOSEQ-SET: ";
    s_AppendSetLabel(desc, st, m_SuppressContext);
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, st, GetAccessionFromObjects(&st, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TEntry         ctx,
 TDesc          ds)
{
    // Append Descriptor label
    string desc("DESCRIPTOR: ");
    ds.GetLabel(&desc, CSeqdesc::eBoth);

    if (ctx.IsSeq()) {
        s_AppendBioseqLabel(desc, ctx.GetSeq(), m_SuppressContext);
    } else {
        s_AppendSetLabel(desc, ctx.GetSet(), m_SuppressContext);
    }
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, ctx, GetAccessionFromObjects(&ds, &ctx, *m_Scope));
}


//void CValidError_imp::PostErr
//(EDiagSev       sv,
// EErrType       et,
// const string&  msg,
// TBioseq        sq,
// TDesc          ds)
//{
//    // Append Descriptor label
//    string desc("DESCRIPTOR: ");
//    ds.GetLabel(&desc, CSeqdesc::eBoth);
//    
//    s_AppendBioseqLabel(desc, sq, m_SuppressContext);
//    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, *m_Scope);
//    //PostErr(sv, et, msg, sq);
//}


//void CValidError_imp::PostErr
//(EDiagSev        sv,
// EErrType        et,
// const string&   msg,
// TSet            st,
// TDesc           ds)
//{
//    // Append Descriptor label
//    string desc =  " DESCRIPTOR: ";
//    ds.GetLabel(&desc, CSeqdesc::eBoth);
//    s_AppendSetLabel(desc, st, m_SuppressContext);
//    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, st, *m_Scope);
//
//}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TAnnot         an)
{
    // Append Annotation label
    string desc = "ANNOTATION: ";

    // !!! need to decide on the message

    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, an, GetAccessionFromObjects(&an, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TGraph         graph)
{
    // Append Graph label
    string desc = "GRAPH: ";
    if (graph.IsSetTitle()) {
        desc += graph.GetTitle();
    } else {
        desc += "<Unnamed>";
    }
    desc += " ";
    graph.GetLoc().GetLabel(&desc);

    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, graph, GetAccessionFromObjects(&graph, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TBioseq        sq,
 TGraph         graph)
{
    // Append Graph label
    string desc("GRAPH: ");
    if ( graph.IsSetTitle() ) {
        desc += graph.GetTitle();
    } else {
        desc += "<Unnamed>";
    }
    desc += " ";
    graph.GetLoc().GetLabel(&desc);
    s_AppendBioseqLabel(desc, sq, m_SuppressContext);
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, graph, GetAccessionFromObjects(&graph, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TAlign        align)
{
    // Append Alignment label
    string desc = "ALIGNMENT: ";
    desc += align.ENUM_METHOD_NAME(EType)()->FindName(align.GetType(), true);
    try {
        CSeq_align::TDim dim = align.GetDim();
        desc += ", dim=" + NStr::IntToString(dim);
    } catch ( const CUnassignedMember &e ) {
        desc += ", dim=UNASSIGNED";
    }

    desc += " SEGS: ";
    desc += align.GetSegs().SelectionName(align.GetSegs().Which());

    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, align, GetAccessionFromObjects(&align, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TEntry        entry)
{
    string desc = "SEQ-ENTRY: ";
    entry.GetLabel(&desc, CSeq_entry::eContent);

    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, entry, GetAccessionFromObjects(&entry, NULL, *m_Scope));
}


void CValidError_imp::PostObjErr 
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if (ctx == 0) {
        PostErr (sv, et, msg, obj);
    } else {
        const CSeqdesc* desc = dynamic_cast < const CSeqdesc* > (&obj);
        if (desc == 0) {
            PostErr (sv, et, msg, obj);
        } else {
            PostErr (sv, et, msg, *ctx, *desc);
        }
    }
}


static string s_GetDateErrorDescription (int flags)
{
    string reasons = "";

    if (flags & eDateValid_empty_date) {
        reasons += "EMPTY_DATE ";
    }
    if (flags & eDateValid_bad_str) {
        reasons += "BAD_STR ";
    }
    if (flags & eDateValid_bad_year) {
        reasons += "BAD_YEAR ";
    }
    if (flags & eDateValid_bad_month) {
        reasons += "BAD_MONTH ";
    }
    if (flags & eDateValid_bad_day) {
        reasons += "BAD_DAY ";
    }
    if (flags & eDateValid_bad_season) {
        reasons += "BAD_SEASON ";
    }
    if (flags & eDateValid_bad_other) {
        reasons += "BAD_OTHER ";
    }
    return reasons;
}


void CValidError_imp::PostBadDateError 
(EDiagSev             sv,
 const string&        msg,
 int                  flags,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    string reasons = s_GetDateErrorDescription(flags);

    NStr::TruncateSpacesInPlace (reasons);
    reasons = msg + " - " + reasons;

    PostObjErr (sv, eErr_GENERIC_BadDate, reasons, obj, ctx);
}


bool CValidError_imp::Validate
(const CSeq_entry& se,
 const CCit_sub* cs,
 CScope* scope)
{
    CSeq_entry_Handle seh;
    try {
        seh = scope->GetSeq_entryHandle(se);
    } catch (const CException& e) { ; }
    if (! seh) {
        seh = scope->AddTopLevelSeqEntry(se);
        if (!seh) {
            return false;
        }
    }
    return Validate(seh, cs);
}

bool CValidError_imp::ValidateDescriptorInSeqEntry (const CSeq_entry& se, CValidError_desc *descval)
{
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY (it, se) {
        try {
            descval->ValidateSeqDesc(**it, se);
            if ( m_PrgCallback ) {
                m_PrgInfo.m_CurrentDone++;
                m_PrgInfo.m_TotalDone++;
                if ( m_PrgCallback(&m_PrgInfo) ) {
                    return false;
                }
            }
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating descriptor. EXCEPTION: ") +
                e.what(), se, **it);
            return true;
        }
    }
    if (se.Which() == CSeq_entry::e_Set) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            if (!ValidateDescriptorInSeqEntry (**it, descval)) {
                return false;
            }
        }
    }
    return true;
}
       

bool CValidError_imp::ValidateDescriptorInSeqEntry (const CSeq_entry& se)
{
    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Desc;
        m_PrgInfo.m_Current = m_NumDesc;
        m_PrgInfo.m_CurrentDone = 0;
        m_PrgCallback(&m_PrgInfo);
    }

    //CRef<CValidError_desc> desc_validator(new CValidError_desc(*this)); 
    CValidError_desc desc_validator(*this);
    return ValidateDescriptorInSeqEntry (se, &desc_validator);        
}


bool CValidError_imp::ValidateSeqDescrInSeqEntry (const CSeq_entry& se, CValidError_descr *descr_val)
{
    if (se.IsSetDescr()) {
        descr_val->ValidateSeqDescr (se.GetDescr(), se);
    }
    if (se.Which() == CSeq_entry::e_Set) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            if (!ValidateSeqDescrInSeqEntry (**it, descr_val)) {
                return false;
            }
        }
    }
    return true;
}
       

bool CValidError_imp::ValidateSeqDescrInSeqEntry (const CSeq_entry& se)
{
    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Desc;
        m_PrgInfo.m_Current = m_NumDesc;
        m_PrgInfo.m_CurrentDone = 0;
        m_PrgCallback(&m_PrgInfo);
    }

    //CRef<CValidError_desc> desc_validator(new CValidError_desc(*this)); 
    CValidError_descr desc_validator(*this);
    return ValidateSeqDescrInSeqEntry (se, &desc_validator);        
}


bool CValidError_imp::Validate
(const CSeq_entry_Handle& seh,
 const CCit_sub* cs)
{
    _ASSERT(seh);

    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Initializing;
        if ( m_PrgCallback(&m_PrgInfo) ) {
            return false;
        }
    }

    // Check that CSeq_entry has data
    if (seh.Which() == CSeq_entry::e_not_set) {
        ERR_POST_X(2, Warning << "Seq_entry not set");
        return false;
    }

    Setup(seh);
 
    // Get first CBioseq object pointer for PostErr below.
    CTypeConstIterator<CBioseq> seq(ConstBegin(*m_TSE));
    if (!seq) {
        ERR_POST_X(3, "No Bioseq anywhere on this Seq-entry");
        return false;
    }

    // If m_NonASCII is true, then this flag was set by the caller
    // of validate to indicate that a non ascii character had been
    // read from a file being used to create a CSeq_entry, that the
    // error had been corrected, but that the error needs to be reported
    // by Validate. Note, Validate is not doing anything other than
    // reporting an error if m_NonASCII is true;
    if (m_NonASCII) {
        PostErr(eDiag_Critical, eErr_GENERIC_NonAsciiAsn,
                  "Non-ascii chars in input ASN.1 strings", *seq);
        // Only report the error once
        m_NonASCII = false;
    }

    // Iterate thru components of record and validate each

    // Features:

    // before validating, need to know if we have insd seq in seq-entry
    bool is_insd_in_sep = false;
    for (CBioseq_CI bi(GetTSEH(), CSeq_inst::eMol_not_set, CBioseq_CI::eLevel_All); bi && !is_insd_in_sep; ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bi->GetCompleteBioseq())) {
            if ((*it)->IsGenbank()
                || (*it)->IsEmbl()
                || (*it)->IsDdbj()
                || (*it)->IsTpg()
                || (*it)->IsTpe()
                || (*it)->IsTpd()) {
                is_insd_in_sep = true;
            }
        }
    }

    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Feat;
        m_PrgInfo.m_Current = m_NumFeat;
        m_PrgInfo.m_CurrentDone = 0;
        if ( m_PrgCallback(&m_PrgInfo) ) {
            return false;
        }
    }
    CValidError_feat feat_validator(*this);
    vector < int > feature_ids;
    for (CFeat_CI fi(GetTSEH()); fi; ++fi) {
        const CSeq_feat& sf = fi->GetOriginalFeature();
        try {
            if (sf.IsSetId() && sf.GetId().IsLocal() && sf.GetId().GetLocal().IsId()) {
                feature_ids.push_back(sf.GetId().GetLocal().GetId());
            }
            feat_validator.ValidateSeqFeat(sf, is_insd_in_sep);
            if ( m_PrgCallback ) {
                m_PrgInfo.m_CurrentDone++;
                m_PrgInfo.m_TotalDone++;
                if ( m_PrgCallback(&m_PrgInfo) ) {
                    return false;
                }
            }
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating feature. EXCEPTION: ") +
                e.what(), sf);
            return true;
        }
    }
    // look for colliding feature IDs
    if (feature_ids.size() > 0) {
        const CTSE_Handle& tse = seh.GetTSE_Handle ();
        stable_sort (feature_ids.begin(), feature_ids.end());
        vector <int>::iterator it = feature_ids.begin();
        int id = *it;
        ++it;
        while (it != feature_ids.end()) {
            if (*it == id) {
                vector<CSeq_feat_Handle> handles = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, id);
                ITERATE( vector<CSeq_feat_Handle>, feat_it, handles ) {
                    PostErr (eDiag_Critical, eErr_SEQ_FEAT_CollidingFeatureIDs,
                             "Colliding feature ID " + NStr::IntToString (id), *(feat_it->GetSeq_feat()));
                }
                while (it != feature_ids.end() && *it == id) {
                    ++it;
                }
                if (it != feature_ids.end()) {
                    id = *it;
                    ++it;
                }
            } else {
                id = *it;
                ++it;
            }
        }
    }

        
    if ( feat_validator.GetNumGenes() == 0  &&  
         feat_validator.GetNumGeneXrefs() > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_OnlyGeneXrefs,
            "There are " + NStr::IntToString(feat_validator.GetNumGeneXrefs()) +
            " gene xrefs and no gene features in this record.", *m_TSE);
    }

    feat_validator.ValidateCitations (seh);

    // Descriptors:

    if (!ValidateDescriptorInSeqEntry (*(GetTSEH().GetCompleteSeq_entry()))) {
        return false;
    }
    
    // Bioseqs:

    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Bioseq;
        m_PrgInfo.m_Current = m_NumBioseq;
        m_PrgInfo.m_CurrentDone = 0;
        m_PrgCallback(&m_PrgInfo);
    }
    CValidError_bioseq bioseq_validator(*this);
    bool has_nucleotide_sequence = false;
    for (CBioseq_CI bi(GetTSEH(), CSeq_inst::eMol_not_set, CBioseq_CI::eLevel_All); bi; ++bi) {
        const CBioseq& bs = *bi->GetCompleteBioseq();

        if (bs.IsNa()) {
            has_nucleotide_sequence = true;
        }
        try {
            bioseq_validator.ValidateSeqIds(bs);
            bioseq_validator.ValidateInst(bs);
            bioseq_validator.ValidateBioseqContext(bs);
            bioseq_validator.ValidateHistory(bs);
            if ( m_PrgCallback ) {
                m_PrgInfo.m_CurrentDone++;
                m_PrgInfo.m_TotalDone++;
                if ( m_PrgCallback(&m_PrgInfo) ) {
                    return false;
                }
            }
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating bioseq. EXCEPTION: ") +
                e.what(), bs);
            return true;
        }
    }
    if ( bioseq_validator.GetTpaWithHistory() > 0  &&
         bioseq_validator.GetTpaWithoutHistory() > 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_INST_TpaAssmeblyProblem,
            "There are " +
            NStr::IntToString(bioseq_validator.GetTpaWithHistory()) +
            " TPAs with history and " + 
            NStr::IntToString(bioseq_validator.GetTpaWithoutHistory()) +
            " without history in this record.", *m_TSE);
    }
    if ( bioseq_validator.GetTpaWithoutHistory() > 0) {
        bool has_gi = false;
        for (CBioseq_CI bi(GetTSEH()); bi && !has_gi; ++bi) {
            // will only report sequences with TPA Assembly user objects and no Seq-hist if at least one sequence
            // has a gi.
            FOR_EACH_SEQID_ON_BIOSEQ (it, *((*bi).GetCompleteBioseq())) {
                if ((*it)->IsGi()) {
                    has_gi = true;
                }
            }
        }             

        if (has_gi) {
            PostErr (eDiag_Warning, eErr_SEQ_INST_TpaAssmeblyProblem,
                "There are " +
                NStr::IntToString(bioseq_validator.GetTpaWithoutHistory()) +
                " TPAs without history in this record, but the record has a gi number assignment.", *m_TSE);
        }
    }
    if (IsIndexerVersion() && DoesAnyProteinHaveGeneralID() && !IsRefSeq() && has_nucleotide_sequence) {
        PostErr (eDiag_Info, eErr_SEQ_INST_ProteinsHaveGeneralID, 
                 "INDEXER_ONLY - Protein bioseqs have general seq-id.",
                 *(seh.GetCompleteSeq_entry()));
    }
        
    // Bioseq sets:

    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Bioseq_set;
        m_PrgInfo.m_Current = m_NumBioseq_set;
        m_PrgInfo.m_CurrentDone = 0;
        m_PrgCallback(&m_PrgInfo);
    }
    CValidError_bioseqset bioseqset_validator(*this);
    for (CTypeConstIterator<CBioseq_set> si(*m_TSE); si; ++si) {
        try {
            bioseqset_validator.ValidateBioseqSet(*si);
            if ( m_PrgCallback ) {
                m_PrgInfo.m_CurrentDone++;
                m_PrgInfo.m_TotalDone++;
                if ( m_PrgCallback(&m_PrgInfo) ) {
                    return false;
                }
            }
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating bioseq set. EXCEPTION: ") +
                e.what(), *si);
            return true;
        }
    }

    // Alignments:

    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Align;
        m_PrgInfo.m_Current = m_NumAlign;
        m_PrgInfo.m_CurrentDone = 0;
        m_PrgCallback(&m_PrgInfo);
    }
    CValidError_align align_validator(*this);
    for (CAlign_CI ai(GetTSEH()); ai; ++ai) {
        const CSeq_align& sa = ai.GetOriginalSeq_align();
        try {
            align_validator.ValidateSeqAlign(sa);
            if ( m_PrgCallback ) {
                m_PrgInfo.m_CurrentDone++;
                m_PrgInfo.m_TotalDone++;
                if ( m_PrgCallback(&m_PrgInfo) ) {
                    return false;
                }
            }
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating alignment. EXCEPTION: ") +
                e.what(), sa);
            return true;
        }
    }

    // Graphs:

    CValidError_graph graph_validator(*this);
    try {
        graph_validator.ValidateSeqGraph (GetTSEH());
    } catch ( const exception& e ) {
        PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
            string("Exeption while validating graph. EXCEPTION: ") +
            e.what(), *(GetTSEH().GetCompleteSeq_entry()));
        return true;
    }
    SIZE_TYPE misplaced = graph_validator.GetNumMisplacedGraphs();
    if ( misplaced > 0 ) {
        string num = NStr::IntToString(misplaced);
        PostErr(eDiag_Critical, eErr_SEQ_PKG_GraphPackagingProblem,
            string("There ") + ((misplaced > 1) ? "are " : "is ") + num + 
            " mispackaged graph" + ((misplaced > 1) ? "s" : "") + " in this record.",
            *m_TSE);
    }

    // Annotation:
    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Annot;
        m_PrgInfo.m_Current = m_NumAnnot;
        m_PrgInfo.m_CurrentDone = 0;
        m_PrgCallback(&m_PrgInfo);
    }
    CValidError_annot annot_validator(*this);
    for (CSeq_annot_CI ni(GetTSEH()); ni; ++ni) {
        const CSeq_annot& sn = *ni->GetCompleteSeq_annot();
        try {
            annot_validator.ValidateSeqAnnot(*ni);
            if ( m_PrgCallback ) {
                m_PrgInfo.m_CurrentDone++;
                m_PrgInfo.m_TotalDone++;
                if ( m_PrgCallback(&m_PrgInfo) ) {
                    return false;
                }
            }
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating annotation. EXCEPTION: ") +
                e.what(), sn);
            return true;
        }
    }

    // Descriptor lists:
    if (!ValidateSeqDescrInSeqEntry (*(GetTSEH().GetCompleteSeq_entry()))) {
        return false;
    }
    
    ReportMissingPubs(*m_TSE, cs);
    ReportMissingBiosource(*m_TSE);

    if (m_NumMisplacedFeatures > 1) {
        PostErr (eDiag_Critical, eErr_SEQ_PKG_FeaturePackagingProblem, 
                 "There are " + NStr::IntToString (m_NumMisplacedFeatures) + " mispackaged features in this record.",
                 *(seh.GetCompleteSeq_entry()));
    } else if (m_NumMisplacedFeatures == 1) {
        PostErr (eDiag_Critical, eErr_SEQ_PKG_FeaturePackagingProblem,
                 "There is 1 mispackaged feature in this record.",
                 *(seh.GetCompleteSeq_entry()));
    }

    FindEmbeddedScript(*(seh.GetCompleteSeq_entry()));
    FindCollidingSerialNumbers(*(seh.GetCompleteSeq_entry()));

    return true;
}


void CValidError_imp::Validate(const CSeq_submit& ss, CScope* scope)
{
    // Check that ss is type e_Entrys
    if ( ss.GetData().Which() != CSeq_submit::C_Data::e_Entrys ) {
        return;
    }

    // Get CCit_sub pointer
    const CCit_sub* cs = &ss.GetSub().GetCit();

    // Just loop thru CSeq_entrys
    FOR_EACH_SEQENTRY_ON_SEQSUBMIT (se_itr, ss) {
        const CSeq_entry& se = **se_itr;
        Validate (se, cs, scope);
    }
}


void CValidError_imp::Validate(const CSeq_annot_Handle& sah)
{
    Setup(sah);
    
    // Iterate thru components of record and validate each

    CValidError_annot annot_validator(*this);
    annot_validator.ValidateSeqAnnot(sah);

    switch (sah.Which()) {
    case CSeq_annot::TData::e_Ftable :
        {
            CValidError_feat feat_validator(*this);
            // for (CTypeConstIterator <CSeq_feat> fi (sa); fi; ++fi) {
            for (CFeat_CI fi (sah); fi; ++fi) {
                const CSeq_feat& sf = fi->GetOriginalFeature();
                feat_validator.ValidateSeqFeat(sf, false);
            }
        }
        break;

    case CSeq_annot::TData::e_Align :
        {
            CValidError_align align_validator(*this);
            // for (CTypeConstIterator <CSeq_align> ai (sa); ai; ++ai) {
            for (CAlign_CI ai(sah); ai; ++ai) {
                const CSeq_align& sa = ai.GetOriginalSeq_align();
                align_validator.ValidateSeqAlign(sa);
            }
        }
        break;

    case CSeq_annot::TData::e_Graph :
        {
            CValidError_graph graph_validator(*this);
            // for (CTypeConstIterator <CSeq_graph> gi (sa); gi; ++gi) {
            for (CGraph_CI gi(sah); gi; ++gi) {
                const CSeq_graph& sg = gi->GetOriginalGraph();
                graph_validator.ValidateSeqGraph(sg);
            }
        }
        break;
    default:
        break;
    }
    FindEmbeddedScript(*(sah.GetCompleteSeq_annot()));
    FindCollidingSerialNumbers(*(sah.GetCompleteSeq_annot()));
}


void CValidError_imp::SetProgressCallback
(CValidator::TProgressCallback callback,
 void* user_data)
{
    m_PrgCallback = callback;
    m_PrgInfo.m_UserData = user_data;
}


void CValidError_imp::ValidatePubdesc
(const CPubdesc& pubdesc,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    int uid = 0, pmid = 0, muid = 0;
    bool conflicting_pmids = false, redundant_pmids = false, conflicting_muids = false, redundant_muids = false;

    ValidatePubHasAuthor(pubdesc, obj, ctx);

    // need to get uid (pmid or muid) in first pass for ValidatePubArticle
    FOR_EACH_PUB_ON_PUBDESC (pub_iter, pubdesc) {
        const CPub& pub = **pub_iter;

        switch( pub.Which() ) {
        case CPub::e_Muid:
            if ( muid == 0 ) {
                muid = pub.GetMuid();
            } else if ( muid != pub.GetMuid() ) {
                conflicting_muids = true;
            } else {
                redundant_muids = true;
            }
            if ( uid == 0 ) {
                uid = pub.GetMuid();
            }
            break;

        case CPub::e_Pmid:
            if ( pmid == 0 ) {
                pmid = pub.GetPmid();
            } else if ( pmid != pub.GetPmid() ) {
                conflicting_pmids = true;
            } else {
                redundant_pmids = true;
            }
            if ( uid == 0 ) {
                uid = pub.GetPmid();
            }
            break;
            
        default:
            break;
        }
    }

    if ( conflicting_pmids ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications, 
                "Multiple conflicting pmids in a single publication", obj, ctx);
    }
    if ( redundant_pmids ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications, 
                "Multiple redundant pmids in a single publication", obj, ctx);
    }
    if ( conflicting_muids ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications, 
                "Multiple conflicting muids in a single publication", obj, ctx);
    }
    if ( redundant_muids ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications, 
                "Multiple redundant muids in a single publication", obj, ctx);
    }

    // second pass for remaining (non-uid) types
    FOR_EACH_PUB_ON_PUBDESC (pub_iter, pubdesc) {
        const CPub& pub = **pub_iter;

        switch( pub.Which() ) {
        case CPub::e_Gen:
            ValidatePubGen(pub.GetGen(), obj, ctx);
            break;

        case CPub::e_Sub:
            ValidateCitSub(pub.GetSub(), obj, ctx);
            break;

        case CPub::e_Medline:
            PostObjErr(eDiag_Error, eErr_GENERIC_MedlineEntryPub, 
                "Publication is medline entry", obj, ctx);
            break;

        /*
        case CPub::e_Muid:
            if ( uid == 0 ) {
                uid = pub.GetMuid();
            }
            break;

        case CPub::e_Pmid:
            if ( uid == 0 ) {
                uid = pub.GetPmid();
            }
            break;
        */
            
        case CPub::e_Article:
            ValidatePubArticle(pub.GetArticle(), uid, obj, ctx);
            if ( ! pubdesc.IsSetFig()  ||  IsBlankString(pubdesc.GetFig()) ) {
                // ValidateArticleHasJournal(pub.GetArticle(), obj);
            }
            break;

        case CPub::e_Equiv:
            PostObjErr(eDiag_Warning, eErr_GENERIC_UnnecessaryPubEquiv,
                "Publication has unexpected internal Pub-equiv", obj, ctx);
            break;

        default:
            break;
        }
    }
    if (pubdesc.IsSetPub()) {
        ValidateAuthorsInPubequiv (pubdesc.GetPub(), obj, ctx);
    }

}


void CValidError_imp::ValidatePubGen
(const CCit_gen& gen,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( gen.IsSetCit()  &&  !gen.GetCit().empty() ) {
        const string& cit = gen.GetCit();
        if ( (NStr::CompareNocase(cit, 0, 8,  "submitted") != 0)          &&
             (NStr::CompareNocase(cit, 0, 11, "unpublished") != 0)        &&
             (NStr::CompareNocase(cit, 0, 18, "Online Publication") != 0) &&
             (NStr::CompareNocase(cit, 0, 26, "Published Only in DataBase") != 0) ) {
            
            PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
                "Unpublished citation text invalid", obj, ctx);
        }

        if (NStr::FindCase (cit, "Title=") != string::npos) {
            PostObjErr(eDiag_Error, eErr_GENERIC_StructuredCitGenCit, 
                    "Unpublished citation has embedded Title", obj, ctx);
        }
        if (NStr::FindCase (cit, "Journal=") != string::npos) {
            PostObjErr(eDiag_Error, eErr_GENERIC_StructuredCitGenCit, 
                    "Unpublished citation has embedded Journal", obj, ctx);
        }

    }
    if (gen.IsSetSerial_number()) {
        m_PubSerialNumbers.push_back(gen.GetSerial_number());
    }
    if (gen.IsSetDate()) {
        int rval = CheckDate (gen.GetDate());
        if (rval != eDateValid_valid) {
            PostBadDateError (eDiag_Error, "Publication date has error", rval, obj, ctx);
        }
    }
}


void CValidError_imp::ValidateArticleHasJournal
(const CCit_art& art,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( art.GetFrom().IsJournal() ) {
        const CCit_jour& jour = art.GetFrom().GetJournal();
        if ( !HasTitle(jour.GetTitle()) ) {
            PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
                    "Journal title missing", obj, ctx);
        }
        if ( jour.CanGetImp() ) {
            const CImprint& imp = jour.GetImp();
        }
    }
}


void CValidError_imp::ValidatePubArticle
(const CCit_art& art,
 int uid,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( !art.IsSetTitle()  ||  !HasTitle(art.GetTitle()) ) { 
        PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Publication has no title", obj, ctx);
    }
    
    if ( art.IsSetAuthors() ) { 
        if ( ! HasName(art.GetAuthors()) ) {
            PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
                "Article has no author names", obj, ctx);
        }
    }
    
    
    if ( art.GetFrom().IsJournal() ) {
        const CCit_jour& jour = art.GetFrom().GetJournal();
        
        bool has_iso_jta = HasIsoJTA(jour.GetTitle());
        bool in_press = false;

        if ( !HasTitle(jour.GetTitle()) ) {
            PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Journal title missing", obj, ctx);
        }

        if ( jour.CanGetImp() ) {
            const CImprint& imp = jour.GetImp();

            if ( imp.CanGetPrepub() ) {
                in_press =  imp.GetPrepub() == CImprint::ePrepub_in_press;
            }

            if ( !imp.IsSetPrepub()        &&  
                 (!imp.CanGetPubstatus()   ||  
                  imp.GetPubstatus() != ePubStatus_aheadofprint) ) {
                bool no_vol = !imp.IsSetVolume()  || 
                              IsBlankString(imp.GetVolume());
                bool no_pages = !imp.IsSetPages()  ||
                                IsBlankString(imp.GetPages());

                EDiagSev sev = IsRefSeq() ? eDiag_Warning : eDiag_Error;

                if ( no_vol  &&  no_pages ) {
                    PostObjErr(sev, eErr_GENERIC_MissingPubInfo, 
                        "Journal volume and pages missing", obj, ctx);
                } else if ( no_vol ) {
                    PostObjErr(sev, eErr_GENERIC_MissingPubInfo,
                        "Journal volume missing", obj, ctx);
                } else if ( no_pages ) {
                    PostObjErr(sev, eErr_GENERIC_MissingPubInfo,
                        "Journal pages missing", obj, ctx);
                }
                
                if ( !no_pages ) {
                    x_ValidatePages(imp.GetPages(), obj, ctx);
                }
                if (imp.IsSetDate()) {
                    if (imp.GetDate().IsStr() && NStr::Equal (imp.GetDate().GetStr(), "?")) {
                        PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                    "Publication date marked as '?'", obj, ctx);
                    } else if (imp.GetDate().IsStd() 
                               && (!imp.GetDate().GetStd().IsSetYear() || imp.GetDate().GetStd().GetYear() == 0)) {
                        PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                    "Publication date missing", obj, ctx);
                    } else {
                        int rval = CheckDate (imp.GetDate());
                        if (rval != eDateValid_valid) {
                            PostBadDateError (eDiag_Error, "Publication date has error", rval, obj, ctx);
                        }
                    }
                } else {
                    PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                "Publication date missing", obj, ctx);
                }
            }
            if (imp.IsSetPubstatus() && imp.GetPubstatus() == ePubStatus_aheadofprint
                && (!imp.IsSetPrepub() || imp.GetPrepub() != CImprint::ePrepub_in_press)) {
                PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, 
                         "Ahead-of-print without in-press", obj, ctx);
            }  
            if (imp.IsSetPubstatus() && imp.GetPubstatus() == ePubStatus_epublish 
                && imp.IsSetPrepub() && imp.GetPrepub() == CImprint::ePrepub_in_press) {
                PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, 
                         "Electronic-only publication should not also be in-press", obj, ctx);
            }
        }
        if ( !has_iso_jta  &&  (uid > 0  ||  in_press  ||  IsRequireISOJTA()) ) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_MissingPubInfo,
                "ISO journal title abbreviation missing", obj, ctx);
        }
    }
}


bool s_GetDigits(const string& pages, string& digits)
{
    string::size_type pos = 0;
    string::size_type len = pages.length();

    digits.erase();

    // skip alpha at the begining 
    while (pos < len  &&  !isdigit((unsigned char) pages[pos])) {
        ++pos;
    }

    while (pos < len  &&  isdigit((unsigned char) pages[pos])) {
        digits += pages[pos];
        ++pos;
    }

    _ASSERT (pos >= len  ||  !isdigit((unsigned char) pages[pos]));

    while (pos < len) {
        if (isdigit((unsigned char) pages[pos])) {
            digits.erase();
            return false;
        }
        ++pos;
    }

    return true;
}


void CValidError_imp::x_ValidatePages
(const string& pages,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    static const string kRoman = "IVXLCDM";

    if (pages.empty()) {
        return;
    }

    EDiagSev sev = IsRefSeq() ? eDiag_Warning : eDiag_Error;
    
    ITERATE (string, it, pages) {
        if (!isalnum((unsigned char)(*it))  &&  *it != '-') {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
                "Page numbering contain bad characters", obj, ctx);
            return;
        }
    }

    string start, stop;
    NStr::SplitInTwo(pages, "-", start, stop);
    if (start.empty()  ||  stop.empty()) {
        PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
            "Page numbering doesn't contain '-'", obj, ctx);
        return;
    }

    // roman numerals are fine
    if (start.find_first_not_of(kRoman) == NPOS  &&
        stop.find_first_not_of(kRoman) == NPOS) {
        return;
    }
    
    if ((isalpha((unsigned char) start[0])  &&  !isalpha((unsigned char) stop[0]))  ||
        (isdigit((unsigned char) start[0])  &&  !isdigit((unsigned char) stop[0]))) {
        PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
            "Inconsistent page numbering", obj, ctx);
        return;
    }

    string digits;
    int beg = 0, end = 0;
    size_t good = 0;
    try {
        if (!s_GetDigits(start, digits)) {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
                "Page numbering stop start strange", obj, ctx);
        } else {
            beg = NStr::StringToInt(digits);
            ++good;
        }
        if (!s_GetDigits(stop, digits)) {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
                "Page numbering stop looks strange", obj, ctx);
        } else {
            end = NStr::StringToInt(digits);
            ++good;
        }
    } catch (CStringException&) {
        PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
                "Page numbering looks strange", obj, ctx);
        return;
    }
    if (good != 2) {
        return;
    }
    if (beg == 0  ||  end == 0) {
        PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
            "Page numbering has zero value", obj, ctx);
    } else if (beg < 0  ||  end < 0) {
        PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
            "Page numbering has negative value", obj, ctx);
    } else if (beg > end) {
        PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
            "Page numbering out of order", obj, ctx);
    } else if ( beg - end > 50 ) {
        PostObjErr(sev, eErr_GENERIC_BadPageNumbering,
            "Page numbering difference greater than 50", obj, ctx);
    }
}


bool CValidError_imp::HasTitle(const CTitle& title)
{
    ITERATE (CTitle::Tdata, item, title.Get() ) {
        const string *str = 0;
        switch ( (*item)->Which() ) {
        case CTitle::C_E::e_Name:
            str = &(*item)->GetName();
            break;

        case CTitle::C_E::e_Tsub:
            str = &(*item)->GetTsub();
            break;

        case CTitle::C_E::e_Trans:
            str = &(*item)->GetTrans();
            break;

        case CTitle::C_E::e_Jta:
            str = &(*item)->GetJta();
            break;

        case CTitle::C_E::e_Iso_jta:
            str = &(*item)->GetIso_jta();
            break;

        case CTitle::C_E::e_Ml_jta:
            str = &(*item)->GetMl_jta();
            break;

        case CTitle::C_E::e_Coden:
            str = &(*item)->GetCoden();
            break;

        case CTitle::C_E::e_Issn:
            str = &(*item)->GetIssn();
            break;

        case CTitle::C_E::e_Abr:
            str = &(*item)->GetAbr();
            break;

        case CTitle::C_E::e_Isbn:
            str = &(*item)->GetIsbn();
            break;

        default:
            break;
        };
        if ( IsBlankString(*str) ) {
            return false;
        }
    }
    return true;
}


bool CValidError_imp::HasIsoJTA(const CTitle& title)
{
    ITERATE (CTitle::Tdata, item, title.Get() ) {
        if ( (*item)->IsIso_jta() ) {
            return true;
        }
    }
    return false;
}


bool CValidError_imp::HasName(const CAuth_list& authors)
{
    if ( authors.CanGetNames() ) {
        const CAuth_list::TNames& names = authors.GetNames();
        switch ( names.Which() ) {
            case CAuth_list::TNames::e_Std:
                ITERATE ( list< CRef< CAuthor > >, auth, names.GetStd() ) {
                    const CPerson_id& pid = (*auth)->GetName();
                    if ( pid.IsName() ) {
                        if ( ! IsBlankString(pid.GetName().GetLast()) ) {
                            return true;
                        }
                    }
                }
                break;
                
            case CAuth_list::TNames::e_Ml:
                if ( ! IsBlankStringList(names.GetMl()) ) {
                    return true;
                }
                break;
                
            case CAuth_list::TNames::e_Str:
                if ( ! IsBlankStringList(names.GetStr()) ) {
                    return true;
                }
                break;
                
            default:
                break;
        }
    }
    return false;
}


void CValidError_imp::ValidatePubHasAuthor
(const CPubdesc& pubdesc,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    bool no_figs = ! pubdesc.IsSetFig()  ||  IsBlankString(pubdesc.GetFig());
    
    const CPub_equiv::Tdata& pubs = pubdesc.GetPub().Get();
    bool only_pmid = pubs.size() == 1  && pubs.front()->IsPmid();
    
    bool no_patent_pub = true;
    FOR_EACH_PUB_ON_PUBDESC (pub, pubdesc) {
        if ( (*pub)->IsPatent() ) {
            no_patent_pub = false;
            break;
        }
    }
    
    bool no_names = true;
    FOR_EACH_PUB_ON_PUBDESC (pub_iter, pubdesc) {
        const CPub& pub = **pub_iter;
        const CAuth_list* authors = 0;
        switch (pub.Which() ) {
            case CPub::e_Gen:
                if ( pub.GetGen().IsSetAuthors() ) {
                    authors = &(pub.GetGen().GetAuthors());
                }
                break;
            case CPub::e_Sub:
                authors = &(pub.GetSub().GetAuthors());
                break;
            case CPub::e_Article:
                if ( pub.GetArticle().IsSetAuthors() ) {
                    authors = &(pub.GetArticle().GetAuthors());
                }
                break;
            case CPub::e_Book:
                authors = &(pub.GetBook().GetAuthors());
                break;
            case CPub::e_Proc:
                authors = &(pub.GetProc().GetBook().GetAuthors());
                break;
            case CPub::e_Man:
                authors = &(pub.GetMan().GetCit().GetAuthors());
                break;
            default:
                break;
        }
        
        if ( authors  && HasName(*authors) ) {
            no_names = false;
            break;
        }
    }
    
    if ( no_figs  &&  ! only_pmid  &&  no_patent_pub  &&  no_names ) {
        PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
                "Publication has no author names", obj, ctx);
    }
    
}


static bool s_BadCharsInAuthorName (string str, string& badauthor, bool allowcomma, bool allowperiod, bool last)
{
    if (NStr::IsBlank (str)) {
        return false;
    }


    size_t stp = string::npos;
    if (last && NStr::StartsWith (str, "St.")) {
        stp = 2;
    }

    size_t pos = 0;
    const char *ptr = str.c_str();

    while (*ptr != 0) {
        if (isalpha (*ptr)
            || *ptr == '-'
            || *ptr == '\''
            || *ptr == ' '
            || (*ptr == ',' && allowcomma)
            || *ptr == '.' && (allowperiod || pos == stp)) {
            // all these are ok
            ptr++;
            pos++;
        } else {
            badauthor = str;
            return true;
        }
    }
    return false;
}


static bool s_BadCharsInAuthor (const CAuthor& author, string& badauthor, bool& last_is_bad)
{
    badauthor = "";
    last_is_bad = false;

    if (author.IsSetName() && author.GetName().IsName()) {
        if (author.GetName().GetName().IsSetLast()
            && NStr::EqualNocase (author.GetName().GetName().GetLast(), "et al.")) {
            return false;
        }
        if (author.GetName().GetName().IsSetLast()
            && s_BadCharsInAuthorName (author.GetName().GetName().GetLast(), badauthor, false, false, true)) {
            last_is_bad = true;
            return true;
        }
        if (author.GetName().GetName().IsSetFirst()
            && s_BadCharsInAuthorName (author.GetName().GetName().GetFirst(), badauthor, false, true, false)) {
            return true;
        }
        if (author.GetName().GetName().IsSetInitials()
            && s_BadCharsInAuthorName (author.GetName().GetName().GetInitials(), badauthor, false, true, false)) {
            return true;
        }
        if (author.GetName().GetName().IsSetSuffix()
            && s_BadCharsInAuthorName (author.GetName().GetName().GetSuffix(), badauthor, false, true, false)) {
            return true;
        }
    }
    return false;
}


void CValidError_imp::ValidateAuthorList 
(const CAuth_list::C_Names& names,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    EDiagSev sev = m_IsRefSeq ? eDiag_Warning : eDiag_Error;

    if (names.IsStd()) {
        list<string> consortium_list;

        ITERATE ( CAuth_list::C_Names::TStd, name, names.GetStd() ) {
            string badauthor = "";
            bool   last_is_bad = false;
            if (s_BadCharsInAuthor (**name, badauthor, last_is_bad)) {
                if (last_is_bad) {
                    PostObjErr (sev, eErr_SEQ_FEAT_BadCharInAuthorLastName, 
                                "Bad characters in author " + badauthor, obj, ctx);
                } else {
                    PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadCharInAuthorName,
                                "Bad characters in author " + badauthor, obj, ctx);
                }
            }
            if ( (*name)->GetName().IsName() ) {
                const CName_std& nstd = (*name)->GetName().GetName();
                string last = "";
                if (nstd.IsSetLast()) {
                    last = nstd.GetLast();
                    NStr::ReplaceInPlace (last, ".", " ");
                    NStr::ReplaceInPlace (last, "  ", " ");
                    NStr::TruncateSpacesInPlace (last);
                }
                string initials = "";
                if (nstd.IsSetInitials()) {
                    initials = nstd.GetInitials();
                    NStr::ReplaceInPlace (initials, ".", " ");
                    NStr::ReplaceInPlace (initials, "  ", " ");
                    NStr::TruncateSpacesInPlace (initials);
                }
                if ( (NStr::CompareNocase(last, "et al") == 0)  ||
                     (NStr::CompareNocase(last, "et") == 0  
                      &&  NStr::CompareNocase(initials, "al") == 0
                      &&  (!nstd.IsSetFirst() || nstd.GetFirst().empty()))) {
                    CAuth_list::C_Names::TStd::const_iterator temp = name;
                    if ( ++temp == names.GetStd().end() ) {
                        PostObjErr(eDiag_Warning, eErr_GENERIC_AuthorListHasEtAl,
                            "Author list ends in et al.", obj, ctx);
                    } else {
                        PostObjErr(eDiag_Warning, eErr_GENERIC_AuthorListHasEtAl,
                            "Author list contains et al.", obj, ctx);
                    }
                }
                // validate suffix, if set and nonempty
                if (nstd.IsSetSuffix() && !NStr::IsBlank (nstd.GetSuffix())) {
                    string suffix = nstd.GetSuffix();

                    typedef CName_std::TSuffixes TSuffixes;
                    const TSuffixes& suffixes = CName_std::GetStandardSuffixes();
                    bool found = false;
                    ITERATE (TSuffixes, it, suffixes) {
                        if (NStr::EqualNocase (suffix, *it)) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadAuthorSuffix, 
                                 "Bad author suffix " + suffix,
                                 obj, ctx);
                    }
                }
            } else if ( (*name)->GetName().IsConsortium() ) {
                const string& consortium = (*name)->GetName().GetConsortium();
                if (NStr::IsBlank (consortium)) {
                    PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, "Empty consortium", obj, ctx);
                } else {
                    bool found = false;
                    ITERATE (list<string>, cons_str, consortium_list) {
                        if (NStr::EqualNocase (consortium, *cons_str)) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, 
                                 "Duplicate consortium '" + consortium + "'", obj, ctx);
                    } else {
                        consortium_list.push_back(consortium);
                    }
                }
            }
        }
    } else if (names.IsMl()) {
        ITERATE ( list< string >, str, names.GetMl()) {
            string badauthor = "";
            if (s_BadCharsInAuthorName (*str, badauthor, true, true, false)) {
                if (NStr::IsBlank (badauthor)) {
                    badauthor = "?";
                }
                PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadCharInAuthorName, 
                            "Bad characters in author " + badauthor, obj, ctx);
            }
        }           
    } else if (names.IsStr()) {

        ITERATE ( list< string >, str, names.GetStr()) {
            string badauthor = "";
            if (s_BadCharsInAuthorName (*str, badauthor, true, true, false)) {
                if (NStr::IsBlank (badauthor)) {
                    badauthor = "?";
                }
                PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadCharInAuthorName, 
                            "Bad characters in author " + badauthor, obj, ctx);
            }
        }           
    }
}


void CValidError_imp::ValidateAuthorsInPubequiv 
(const CPub_equiv& pe,
 const CSerialObject& obj,
 const CSeq_entry* ctx)
{
    FOR_EACH_PUB_ON_PUBEQUIV (pub_iter, pe) {
        const CPub& pub = **pub_iter;
        const CAuth_list* authors = 0;
        switch ( pub.Which() ) {
        case CPub::e_Gen:
            if ( pub.GetGen().IsSetAuthors() ) {
                authors = &(pub.GetGen().GetAuthors());
            }
            break;
        case CPub::e_Sub:
            authors = &(pub.GetSub().GetAuthors());
            break;
        case CPub::e_Article:
            if ( pub.GetArticle().IsSetAuthors() ) {
                authors = &(pub.GetArticle().GetAuthors());
            }
            break;
        case CPub::e_Book:
            authors = &(pub.GetBook().GetAuthors());
            break;
        case CPub::e_Proc:
            authors = &(pub.GetProc().GetBook().GetAuthors());
            break;
        case CPub::e_Man:
            authors = &(pub.GetMan().GetCit().GetAuthors());
            break;
        case CPub::e_Equiv:
            ValidateAuthorsInPubequiv (pub.GetEquiv(), obj, ctx);
            break;
        default:
            break;
        }

        if ( !authors ) {
            continue;
        }

        const CAuth_list::C_Names& names = authors->GetNames();
        ValidateAuthorList (names, obj, ctx);
    }
}


void CValidError_imp::ValidateDbxref
(const CDbtag& xref,
 const CSerialObject& obj,
 bool biosource,
 const CSeq_entry *ctx)
{
	if (xref.IsSetTag() && xref.GetTag().IsStr()) {
		if (ContainsSgml(xref.GetTag().GetStr())) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
                "dbxref value" + xref.GetTag().GetStr() + " has SGML",
				obj, ctx);
		}
	}

    if ( !xref.CanGetDb() ) {
        return;
    }
    const string& db = xref.GetDb();

	if (ContainsSgml(db)) {
        PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
            "dbxref database" + db + " has SGML",
			obj, ctx);
	}

    CDbtag::EDbtagType db_type = xref.GetType();
    bool is_pid = db_type == CDbtag::eDbtagType_PID   ||
                  db_type == CDbtag::eDbtagType_PIDd  ||
                  db_type == CDbtag::eDbtagType_PIDe  ||
                  db_type == CDbtag::eDbtagType_PIDg;

    if ( biosource ) {
        if ( !xref.IsApproved() || is_pid ) {
            PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                "Illegal db_xref type " + db, obj, ctx);
        }
    } else {  // Seq_feat
        const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(&obj);
        if ( feat != 0 ) {
            bool is_cdregion = 
                feat->CanGetData()  &&  feat->GetData().IsCdregion();
            bool refseq = IsRefSeq();
            if ( !xref.IsApproved(refseq)  ||  (!is_cdregion  &&  is_pid) ) {
                const char* legal = xref.IsApprovedNoCase(refseq);
                if ( legal != 0 ) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                        "Illegal db_xref type " + db +
                        ", legal capitalization is " + legal, obj, ctx);
                } else {
                    PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                        "Illegal db_xref type " + db, obj, ctx);
                }
            }
        }
    }
}


void CValidError_imp::ValidateDbxref
(TDbtags& xref_list,
 const CSerialObject& obj,
 bool biosource,
 const CSeq_entry *ctx)
{
    ITERATE( TDbtags, xref, xref_list) {
        ValidateDbxref(**xref, obj, biosource, ctx);
    }
}


static bool s_UnbalancedParentheses (string str)
{
	if (NStr::IsBlank(str)) {
		return false;
	}

    int par = 0, bkt = 0;
	string::iterator it = str.begin();
	while (it != str.end()) {
		if (*it == '(') {
            ++par;
		} else if (*it == ')') {
			--par;
			if (par < 0) {
				return true;
			}
		} else if (*it == '[') {
			++bkt;
		} else if (*it == ']') {
			--bkt;
			if (bkt < 0) {
				return true;
			}
		}
		++it;
	}
	if (par > 0 || bkt > 0) {
		return true;
	} else {
		return false;
	}
}


void CValidError_imp::ValidateBioSource
(const CBioSource&    bsrc,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( !bsrc.CanGetOrg() ) {
        PostObjErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism has been applied to this Bioseq.", obj, ctx);
        return;
    }

	const COrg_ref& orgref = bsrc.GetOrg();
  
	// Organism must have a name.
	if ( (!orgref.IsSetTaxname()  ||  orgref.GetTaxname().empty())  &&
         (!orgref.IsSetCommon()   ||  orgref.GetCommon().empty()) ) {
		  PostObjErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism name has been applied to this Bioseq.", obj, ctx);
	}

	// validate legal locations.
	if ( bsrc.GetGenome() == CBioSource::eGenome_transposon  ||
		 bsrc.GetGenome() == CBioSource::eGenome_insertion_seq ) {
		  PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceLocation,
            "Transposon and insertion sequence are no longer legal locations.",
            obj, ctx);
	}

	if (IsIndexerVersion()
		&& bsrc.IsSetGenome() 
		&& bsrc.GetGenome() == CBioSource::eGenome_chromosome) {
		PostObjErr(eDiag_Info, eErr_SEQ_DESCR_ChromosomeLocation, 
			       "INDEXER_ONLY - BioSource location is chromosome",
				   obj, ctx);
	}

    bool isViral = false, isAnimal = false, isPlant = false, isBacteria = false, isArchaea = false, isFungal = false;
	if (bsrc.IsSetLineage()) {
		string lineage = bsrc.GetLineage();
		if (NStr::StartsWith(lineage, "Viruses; ", NStr::eNocase)) {
            isViral = true;
	    } else if (NStr::StartsWith(lineage, "Eukaryota; Metazoa; ", NStr::eNocase)) {
			isAnimal = true;
		} else if (NStr::StartsWith(lineage, "Eukaryota; Viridiplantae; Streptophyta; Embryophyta; ", NStr::eNocase)
                   || NStr::StartsWith(lineage, "Eukaryota; Rhodophyta; ", NStr::eNocase)
                   || NStr::StartsWith(lineage, "Eukaryota; stramenopiles; Phaeophyceae; ", NStr::eNocase)) {
			isPlant = true;
		} else if (NStr::StartsWith(lineage, "Bacteria; ", NStr::eNocase)) {
			isBacteria = true;
		} else if (NStr::StartsWith(lineage, "Archaea; ", NStr::eNocase)) {
			isArchaea = true;
		} else if (NStr::StartsWith(lineage, "Eukaryota; Fungi; ", NStr::eNocase)) {
			isFungal = true;
		}
	}

	int chrom_count = 0;
	bool chrom_conflict = false;
	int country_count = 0, lat_lon_count = 0;
	int fwd_primer_seq_count = 0, rev_primer_seq_count = 0;
	int fwd_primer_name_count = 0, rev_primer_name_count = 0;
    const CSubSource *chromosome = 0;
	string countryname;
    bool germline = false;
    bool rearranged = false;

    FOR_EACH_SUBSOURCE_ON_BIOSOURCE (ssit, bsrc) {
		if (!(*ssit)->IsSetSubtype()) {
			continue;
		}

		int subtype = (*ssit)->GetSubtype();
        switch ( subtype ) {
            
        case CSubSource::eSubtype_country:
            {
				++country_count;
                if (!NStr::IsBlank (countryname)) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadCountryCode, 
                                "Multiple country names on BioSource", obj, ctx);
                }
                countryname = (**ssit).GetName();
                bool is_miscapitalized = false;
                if ( CCountries::IsValid(countryname, is_miscapitalized) ) {
                    if (is_miscapitalized) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadCountryCapitalization, 
                                    "Bad country capitalization [" + countryname + "]",
                                    obj, ctx);
                    }
                } else {
                    if ( countryname.empty() ) {
                        countryname = "?";
                    }
                    if ( CCountries::WasValid(countryname) ) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_ReplacedCountryCode,
                                "Replaced country name " + countryname, obj, ctx);
                    } else {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCountryCode,
                                "Bad country name " + countryname, obj, ctx);
                    }
                }
            }
            break;

		case CSubSource::eSubtype_lat_lon:
			{
				++lat_lon_count;
				if (lat_lon_count > 1) {
					PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_LatLonProblem, 
						       "Multiple lat_lon on BioSource", obj, ctx);
				}
			}
			break;
            
        case CSubSource::eSubtype_chromosome:
            ++chrom_count;
            if ( chromosome != 0 ) {
                if ( NStr::CompareNocase((**ssit).GetName(), chromosome->GetName()) != 0) {
                    chrom_conflict = true;
                }          
            } else {
                chromosome = ssit->GetPointer();
            }
            break;

		case CSubSource::eSubtype_fwd_primer_name:
			++fwd_primer_name_count;
			break;

		case CSubSource::eSubtype_rev_primer_name:
			++rev_primer_name_count;
			break;
            
		case CSubSource::eSubtype_fwd_primer_seq:
			++fwd_primer_seq_count;
			break;

		case CSubSource::eSubtype_rev_primer_seq:
			++rev_primer_seq_count;
			break;

		case CSubSource::eSubtype_transposon_name:
        case CSubSource::eSubtype_insertion_seq_name:
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceQual,
                "Transposon name and insertion sequence name are no "
                "longer legal qualifiers.", obj, ctx);
            break;
            
        case 0:
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadSubSource,
                "Unknown subsource subtype 0.", obj, ctx);
            break;
            
        case CSubSource::eSubtype_other:
            ValidateSourceQualTags((**ssit).GetName(), obj, ctx);
            break;

        case CSubSource::eSubtype_germline:
            germline = true;
            break;

        case CSubSource::eSubtype_rearranged:
            rearranged = true;
            break;

        default:
            break;
        }

		if (!CSubSource::NeedsNoText(subtype) && (*ssit)->IsSetName()) {
            const string& subname = (*ssit)->GetName();
			if (s_UnbalancedParentheses(subname)) {
				PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_UnbalancedParentheses,
                           "Unbalanced parentheses in '" + subname + "'",
						   obj, ctx);
			}
			if (ContainsSgml(subname)) {
				PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
					       "subsource " + subname + " has SGML", 
						   obj, ctx);
			}
		}
    }
    if ( germline  &&  rearranged ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadSubSource,
            "Germline and rearranged should not both be present", obj, ctx);
    }
	  if ( chrom_count > 1 ) {
		    string msg = 
                chrom_conflict ? "Multiple conflicting chromosome qualifiers" :
                                 "Multiple identical chromosome qualifiers";
		    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleChromosomes, msg, obj, ctx);
	  }

    if (country_count > 1) {
		PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
			       "Multiple country qualifiers present", obj, ctx);
	}
    if (lat_lon_count > 1) {
		PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
			       "Multiple lat_lon qualifiers present", obj, ctx);
	}
    if (fwd_primer_seq_count > 1) {
		PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
			       "Multiple fwd_primer_seq qualifiers present", obj, ctx);
	}
    if (rev_primer_seq_count > 1) {
		PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
			       "Multiple rev_primer_seq qualifiers present", obj, ctx);
	}
    if (fwd_primer_name_count > 1) {
		PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
			       "Multiple fwd_primer_name qualifiers present", obj, ctx);
	}
    if (rev_primer_name_count > 1) {
		PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
			       "Multiple rev_primer_name qualifiers present", obj, ctx);
	}

	// validates orgref in the context of lineage
    if ( !orgref.IsSetOrgname()  ||
         !orgref.GetOrgname().IsSetLineage()  ||
         orgref.GetOrgname().GetLineage().empty() ) {
		PostObjErr(eDiag_Error, eErr_SEQ_DESCR_MissingLineage, 
			     "No lineage for this BioSource.", obj, ctx);
	} else {
        const string& lineage = orgref.GetOrgname().GetLineage();
		if ( bsrc.GetGenome() == CBioSource::eGenome_kinetoplast ) {
			if ( lineage.find("Kinetoplastida") == string::npos ) {
				PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
						 "Only Kinetoplastida have kinetoplasts", obj, ctx);
			}
		} 
		if ( bsrc.GetGenome() == CBioSource::eGenome_nucleomorph ) {
			if ( lineage.find("Chlorarchniophyta") == string::npos  &&
				lineage.find("Cryptophyta") == string::npos ) {
				PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
                    "Only Chlorarchniophyta and Cryptophyta have nucleomorphs", obj, ctx);
			}
		}
	}

	ValidateOrgRef (orgref, obj, ctx);
}


void CValidError_imp::ValidateOrgRef
(const COrg_ref&    orgref,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( orgref.IsSetDb() ) {
        ValidateDbxref(orgref.GetDb(), obj, true, ctx);
    }

    if ( IsRequireTaxonID() ) {
        bool found = false;
        FOR_EACH_DBXREF_ON_ORGREF (dbt, orgref) {
            if ( NStr::CompareNocase((*dbt)->GetDb(), "taxon") == 0 ) {
                found = true;
                break;
            }
        }
        if ( !found ) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_NoTaxonID,
                "BioSource is missing taxon ID", obj, ctx);
        }
    }

    if ( !orgref.IsSetOrgname() ) {
        return;
    }
    const COrgName& orgname = orgref.GetOrgname();
    ValidateOrgName(orgname, obj, ctx);
}


void CValidError_imp::ValidateOrgName
(const COrgName&    orgname,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( orgname.IsSetMod() ) {
        FOR_EACH_ORGMOD_ON_ORGNAME (omd_itr, orgname) {
            const COrgMod& omd = **omd_itr;
            int subtype = omd.GetSubtype();
            
			switch (subtype) {
				case 0:
				case 1:
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
                               "Unknown orgmod subtype " + NStr::IntToString(subtype), obj, ctx);
					break;
				case COrgMod::eSubtype_variety:
					if ( NStr::CompareNocase( orgname.GetDiv(), "PLN" ) != 0 ) {
						PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
							"Orgmod variety should only be in plants or fungi", 
							obj, ctx);
					}
					break;
				case COrgMod::eSubtype_other:
					ValidateSourceQualTags( omd.GetSubname(), obj, ctx);
					break;
				case COrgMod::eSubtype_culture_collection:
					if (omd.IsSetSubname() && NStr::Find(omd.GetSubname(), ":") == string::npos) {
						PostObjErr(eDiag_Error, eErr_SEQ_DESCR_UnstructuredVoucher, 
							       "Culture_collection should be structured, but is not",
								   obj, ctx);
					}
					break;
				default:
					break;
            }
			if ( omd.IsSetSubname()) {
				const string& subname = omd.GetSubname();

				if (s_UnbalancedParentheses(subname)) {
					PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_UnbalancedParentheses,
							   "Unbalanced parentheses in '" + subname + "'",
							   obj, ctx);
				}
				if (ContainsSgml(subname)) {
					PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
							   "orgmod " + subname + " has SGML", 
							   obj, ctx);
				}
			}
        }

		// look for multiple source vouchers
        FOR_EACH_ORGMOD_ON_ORGNAME (omd_itr, orgname) {
			if(!(*omd_itr)->IsSetSubtype() || !(*omd_itr)->IsSetSubname()) {
				continue;
			}
            const COrgMod& omd = **omd_itr;
            int subtype = omd.GetSubtype();

			if (subtype != COrgMod::eSubtype_specimen_voucher
				&& subtype != COrgMod::eSubtype_bio_material
				&& subtype != COrgMod::eSubtype_culture_collection) {
				continue;
			}

			string inst1 = "", coll1 = "", id1 = "";
			if (!COrgMod::ParseStructuredVoucher(omd.GetSubname(), inst1, coll1, id1)) {
				if (NStr::IsBlank(inst1)) {
					PostObjErr (eDiag_Error, eErr_SEQ_DESCR_BadInstitutionCode, 
						        "Voucher is missing institution code", obj, ctx);
				}
				if (NStr::IsBlank (id1)) {
					PostObjErr (eDiag_Error, eErr_SEQ_DESCR_BadVoucherID, 
					               "Voucher is missing specific identifier", obj, ctx);
			    }
			    continue;
		    }
            if (NStr::EqualNocase(inst1, "personal")
				|| NStr::EqualCase(coll1, "DNA")) {
				continue;
			}

			COrgName::TMod::const_iterator it_next = omd_itr;
			++it_next;
			while (it_next != orgname.GetMod().end()) {
				if (!(*it_next)->IsSetSubtype() || !(*it_next)->IsSetSubname()) {
					++it_next;
					continue;
				}
				int subtype_next = (*it_next)->GetSubtype();
				if (subtype_next != COrgMod::eSubtype_specimen_voucher
					&& subtype_next != COrgMod::eSubtype_bio_material
					&& subtype_next != COrgMod::eSubtype_culture_collection) {
					++it_next;
					continue;
				}
			    string inst2 = "", coll2 = "", id2 = "";
				if (!COrgMod::ParseStructuredVoucher((*it_next)->GetSubname(), inst2, coll2, id2)
					|| NStr::IsBlank(inst2)
					|| NStr::EqualNocase(inst2, "personal")
					|| NStr::EqualCase(coll2, "DNA")
					|| !NStr::EqualNocase (inst1, inst2)) {
					++it_next;
					continue;
				}
				// at this point, we have identified two vouchers 
				// with the same institution codes
				// that are not personal and not DNA collections
				if (!NStr::IsBlank(coll1) && !NStr::IsBlank(coll2) 
					&& NStr::EqualNocase(coll1, coll2)) {
					PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceVouchers, 
						        "Multiple vouchers with same institution:collection",
								obj, ctx);
				} else {
					PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceVouchers, 
						        "Multiple vouchers with same institution",
								obj, ctx);
				}
                ++it_next;
			}

		}          

    }
}


bool CValidError_imp::IsTransgenic(const CBioSource& bsrc)
{
    if (bsrc.CanGetSubtype()) {
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE (sbs_itr, bsrc) {
            const CSubSource& sbs = **sbs_itr;
            if (sbs.GetSubtype() == CSubSource::eSubtype_transgenic) {
                return true;
            }
        }
    }
    return false;
}


static bool s_IsRefSeqInSep(const CSeq_entry& se, CScope& scope)
{
    for (CBioseq_CI it(scope, se); it; ++it) {
        FOR_EACH_SEQID_ON_BIOSEQ (id, *(it->GetCompleteBioseq())) {
            if ((*id)->IsOther()) {
                const CTextseq_id* tsip = (*id)->GetTextseq_Id();
                if (tsip != NULL  &&  tsip->IsSetAccession()) {
                    return true;
                }
            }
        }
    }
    return false;
}


static bool s_IsHtgInSep(const CSeq_entry& se)
{
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY (it, se) {
        if ((*it)->Which() == CSeqdesc::e_Molinfo) {
            CMolInfo::TTech tech = (*it)->GetMolinfo().GetTech();
            if (tech == CMolInfo::eTech_htgs_0  ||
                tech == CMolInfo::eTech_htgs_1  ||
                tech == CMolInfo::eTech_htgs_2  ||
                tech == CMolInfo::eTech_htgs_3) {
                return true;
            }
        }
    }
    if (se.IsSet()) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            if (s_IsHtgInSep(**it)) {
                return true;
            }
        }
    }
    return false;
}


void CValidError_imp::ValidateCitSub
(const CCit_sub& cs,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    bool has_name  = false,
         has_affil = false;

    if ( cs.CanGetAuthors() ) {
        const CAuth_list& authors = cs.GetAuthors();
        has_name = HasName(authors);

        if ( authors.CanGetAffil() ) {
            const CAffil& affil = authors.GetAffil();

            switch ( affil.Which() ) {
            case CAffil::e_Str:
                {{
                    if ( !IsBlankString(affil.GetStr()) ) {
                        has_affil = true;
                    }
                }}
                break;

            case CAffil::e_Std:
                {{
                    const CAffil::TStd& std = affil.GetStd();
#define HAS_VALUE(x) (std.CanGet##x()  &&  !IsBlankString(std.Get##x()))
                    if ( HAS_VALUE(Affil)    ||  HAS_VALUE(Div)      ||
                         HAS_VALUE(City)     ||  HAS_VALUE(Sub)      ||
                         HAS_VALUE(Country)  ||  HAS_VALUE(Street)   ||
                         HAS_VALUE(Email)    ||  HAS_VALUE(Fax)      ||
                         HAS_VALUE(Phone)    ||  HAS_VALUE(Postal_code) ) {
                        has_affil = true;
                    }
                    if (!std.IsSetCountry() || NStr::IsBlank(std.GetCountry())) {
                        PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                    "Submission citation affiliation has no country",
                                    obj, ctx);
                    } else if (NStr::EqualCase (std.GetCountry(), "USA")) {
                        if (!std.IsSetSub() || NStr::IsBlank (std.GetSub())) {
                            PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                        "Submission citation affiliation has no state",
                                        obj, ctx);
                        }
                    }
                }}
#undef HAS_VALUE
                break;

            default:
                break;
            }
            
        }
    }

    if ( !has_name ) {
        PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Submission citation has no author names", obj, ctx);
    }
    if ( !has_affil ) {
        EDiagSev sev = 
            s_IsRefSeqInSep(GetTSE(), *m_Scope)  ||  s_IsHtgInSep(GetTSE()) ?
                eDiag_Warning : eDiag_Error;
        PostObjErr(sev, eErr_GENERIC_MissingPubInfo,
            "Submission citation has no affiliation", obj, ctx);
    }

    if (cs.IsSetDate()) {
        int rval = CheckDate (cs.GetDate());
        if (rval != eDateValid_valid) {
            PostBadDateError (eDiag_Error, "Submission citation date has error", rval, obj, ctx);
        }
    }
}


void CValidError_imp::ValidateSeqLoc
(const CSeq_loc& loc,
 const CBioseq_Handle&  seq,
 const string&   prefix,
 const CSerialObject& obj)
{
    bool circular = false;
    circular = seq  &&  seq.GetInst_Topology() == CSeq_inst::eTopology_circular;
    
    bool ordered = true, adjacent = false, chk = true,
        unmarked_strand = false, mixed_strand = false;
    const CSeq_id* id_cur = 0, *id_prv = 0;
    const CSeq_interval *int_cur = 0, *int_prv = 0;
    ENa_strand strand_cur = eNa_strand_unknown,
        strand_prv = eNa_strand_unknown;

    unsigned int num_mix = 0;
    unsigned int zero_gi = 0;

    CTypeConstIterator<CSeq_loc> lit = ConstBegin(loc);
    for (; lit; ++lit) {
        try {
            switch (lit->Which()) {
            case CSeq_loc::e_Int:
                int_cur = &lit->GetInt();
                strand_cur = int_cur->IsSetStrand() ?
                    int_cur->GetStrand() : eNa_strand_unknown;
                id_cur = &int_cur->GetId();
                chk = IsValid(*int_cur, m_Scope);
                if (chk  &&  int_prv  && ordered  &&
                    !circular  && id_prv) {
                    if (IsSameBioseq(*id_prv, *id_cur, m_Scope)) {
                        if (strand_cur == eNa_strand_minus) {
                            if (int_prv->GetTo() < int_cur->GetTo()) {
                                ordered = false;
                            }
                            if (int_cur->GetTo() + 1 == int_prv->GetFrom()) {
                                adjacent = true;
                            }
                        } else {
                            if (int_prv->GetTo() > int_cur->GetTo()) {
                                ordered = false;
                            }
                            if (int_prv->GetTo() + 1 == int_cur->GetFrom()) {
                                adjacent = true;
                            }
                        }
                    }
                }
                if (int_prv) {
                    if (IsSameBioseq(int_prv->GetId(), int_cur->GetId(), m_Scope)){
                        if (strand_prv == strand_cur  &&
                            int_prv->GetFrom() == int_cur->GetFrom()  &&
                            int_prv->GetTo() == int_cur->GetTo()) {
                            PostErr(eDiag_Error,
                                eErr_SEQ_FEAT_DuplicateInterval,
                                "Duplicate exons in location", obj);
                        }
                    }
                }
                int_prv = int_cur;
                break;
            case CSeq_loc::e_Pnt:
                strand_cur = lit->GetPnt().IsSetStrand() ?
                    lit->GetPnt().GetStrand() : eNa_strand_unknown;
                id_cur = &lit->GetPnt().GetId();
                chk = IsValid(lit->GetPnt(), m_Scope);
                int_prv = 0;
                break;
            case CSeq_loc::e_Packed_pnt:
                strand_cur = lit->GetPacked_pnt().IsSetStrand() ?
                    lit->GetPacked_pnt().GetStrand() : eNa_strand_unknown;
                id_cur = &lit->GetPacked_pnt().GetId();
                chk = IsValid(lit->GetPacked_pnt(), m_Scope);
                int_prv = 0;
                break;
            case CSeq_loc::e_Null:
                break;
            default:
                strand_cur = eNa_strand_other;
                id_cur = 0;
                int_prv = 0;
                break;
            }
            if (!chk) {
                string lbl;
                lit->GetLabel(&lbl);
                PostErr(eDiag_Critical, eErr_SEQ_FEAT_Range,
                    prefix + ": Seq-loc " + lbl + " out of range", obj);
            }

            if (lit->IsMix()) {
                num_mix ++;
            }

            if (lit->GetId() != 0 && lit->GetId()->IsGi() && lit->GetId()->GetGi() == 0) {
                zero_gi ++;
            }
            
            if (lit->Which() != CSeq_loc::e_Null) {
                if (strand_prv != eNa_strand_other  &&
                    strand_cur != eNa_strand_other) {
                    if (id_cur  &&  id_prv  &&
                        IsSameBioseq(*id_cur, *id_prv, m_Scope)) {
                        if (strand_prv != strand_cur) {
                            if ((strand_prv == eNa_strand_plus  &&
                                strand_cur == eNa_strand_unknown)  ||
                                (strand_prv == eNa_strand_unknown  &&
                                strand_cur == eNa_strand_plus)) {
                                unmarked_strand = true;
                            } else {
                                mixed_strand = true;
                            }
                        }
                    }
                }                
                strand_prv = strand_cur;
                id_prv = id_cur;
            }
        } catch( const exception& e ) {
            string label;
            lit->GetLabel(&label);
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,  
                "Exception caught while validating location " +
                label + ". Exception: " + e.what(), obj);
                
            strand_cur = eNa_strand_other;
            id_cur = 0;
            int_prv = 0;
        }
        
    }

    if (num_mix > 1) {
        string label;
        loc.GetLabel(&label);
        PostErr (eDiag_Error, eErr_SEQ_FEAT_NestedSeqLocMix, 
                 prefix + "SeqLoc [" + label + "] has nested SEQLOC_MIX elements",
                 obj);
    }
    if (zero_gi > 0) {
        string label = "?";
        if (seq && seq.IsSetId()) {
            label = seq.GetId().front().GetSeqId()->AsFastaString();
        }

        PostErr (eDiag_Critical, eErr_SEQ_FEAT_FeatureLocationIsGi0,
                 "Feature has " + NStr::IntToString(zero_gi) 
                 + " gi|0 location" + (zero_gi > 1 ? "s" : "")
                 + " on Bioseq " + label,
                 obj);
    }


    // Warn if different parts of a seq-loc refer to the same bioseq using 
    // differnt id types (i.e. gi and accession)
    ValidateSeqLocIds(loc, obj);
    
    bool trans_splice = false;
    bool exception = false;
    const CSeq_feat* sfp = dynamic_cast<const CSeq_feat*>(&obj);
    if (sfp != 0) {
        
        // Publication intervals ordering does not matter
        
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_pub ) {
            ordered = true;
            adjacent = false;
        }
        
        // ignore ordering of heterogen bonds
        
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_het ) {
            ordered = true;
            adjacent = false;
        }
        
        // misc_recomb intervals SHOULD be in reverse order
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_recomb ) {
            ordered = true;
        }
        
        // primer_bind intervals MAY be in on opposite strands
        
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_primer_bind ) {
            mixed_strand = false;
            unmarked_strand = false;
            ordered = true;
        }
        
        exception = sfp->IsSetExcept() ?  sfp->GetExcept() : false;
        if (exception  &&  sfp->CanGetExcept_text()) {
            // trans splicing exception turns off both mixed_strand and
            // out_of_order messages
            if (NStr::FindNoCase(sfp->GetExcept_text(), "trans-splicing") != NPOS) {
                trans_splice = true;
            }
        }
    }

    string loc_lbl;
    if (adjacent) {
        loc.GetLabel(&loc_lbl);
        EDiagSev sev = exception ? eDiag_Warning : eDiag_Error;
        PostErr(sev, eErr_SEQ_FEAT_AbuttingIntervals,
            prefix + ": Adjacent intervals in SeqLoc [" +
            loc_lbl + "]", obj);
    }

    if (trans_splice) {
        return;
    }

    if (mixed_strand  ||  unmarked_strand  ||  !ordered) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        if (mixed_strand) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
                prefix + ": Mixed strands in SeqLoc [" +
                loc_lbl + "]", obj);
        } else if (unmarked_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                prefix + ": Mixed plus and unknown strands in SeqLoc "
                " [" + loc_lbl + "]", obj);
        }
        if (!ordered) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
                prefix + ": Intervals out of order in SeqLoc [" +
                loc_lbl + "]", obj);
        }
        return;
    }

    if ( seq  &&
         seq.IsSetInst_Repr()  &&
         seq.GetInst_Repr() != CSeq_inst::eRepr_seg ) {
        return;
    }

    // Check for intervals out of order on segmented Bioseq
    if ( seq  &&  BadSeqLocSortOrder(*seq.GetCompleteBioseq(), loc, m_Scope) ) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
            prefix + "Intervals out of order in SeqLoc [" +
            loc_lbl + "]", obj);
    }

    // Check for mixed strand on segmented Bioseq
    if ( IsMixedStrands(loc) ) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        PostErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
            prefix + ": Mixed strands in SeqLoc [" +
            loc_lbl + "]", obj);
    }
}


void CValidError_imp::AddBioseqWithNoPub(const CBioseq& seq)
{
    m_BioseqWithNoPubs.push_back(CConstRef<CBioseq>(&seq));
}


void CValidError_imp::AddBioseqWithNoBiosource(const CBioseq& seq)
{
    m_BioseqWithNoSource.push_back(CConstRef<CBioseq>(&seq));
}


void CValidError_imp::AddProtWithoutFullRef(const CBioseq_Handle& seq)
{
    const CSeq_feat* cds = GetCDSForProduct(seq);
    if ( cds == 0 ) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_NoProtRefFound, 
                 "No full length Prot-ref feature applied to this Bioseq", *(seq.GetCompleteBioseq()));
    } else {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_NoProtRefFound, 
                 "No full length Prot-ref feature applied to this Bioseq", *cds);
    }
}


void CValidError_imp::ReportMissingPubs(const CSeq_entry& se, const CCit_sub* cs)
{
    if ( m_NoPubs ) {
        if ( !m_IsGPS  &&  !m_IsRefSeq  &&  !cs) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_NoPubFound, 
                "No publications anywhere on this entire record", se);
        } 
        return;
    }

    size_t num_no_pubs = m_BioseqWithNoPubs.size();

    EDiagSev sev = IsCuratedRefSeq() ? eDiag_Error : eDiag_Warning;

    if ( num_no_pubs == 1 ) {
        PostErr(sev, eErr_SEQ_DESCR_NoPubFound, 
            "No publications refer to this Bioseq.",
            *(m_BioseqWithNoPubs[0]));
    } else if ( num_no_pubs > 10 ) {
        PostErr(sev, eErr_SEQ_DESCR_NoPubFound, 
            NStr::IntToString(num_no_pubs) + 
            " Bioseqs without publication in this record  (first reported)",
            *(m_BioseqWithNoPubs[0]));
    } else {
        string msg;
        for ( size_t i = 0; i < num_no_pubs; ++i ) {
            msg = NStr::IntToString(i + 1) + " of " + 
                NStr::IntToString(num_no_pubs) + 
                " Bioseqs without publication";
            PostErr(sev, eErr_SEQ_DESCR_NoPubFound, msg, 
                *(m_BioseqWithNoPubs[i]));
        }
    }
}


void CValidError_imp::ReportMissingBiosource(const CSeq_entry& se)
{
    if(m_NoBioSource  &&  !m_IsPatent  &&  !m_IsPDB) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism name anywhere on this entire record", se);
        return;
    }
    
    size_t num_no_source = m_BioseqWithNoSource.size();
    
    if ( num_no_source == 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound, 
            "No organism name has been applied to this Bioseq.",
            *(m_BioseqWithNoSource[0]));
    } else if ( num_no_source > 10 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound, 
            NStr::IntToString(num_no_source) + 
            " Bioseqs without organism name in this record (first reported)",
            *(m_BioseqWithNoSource[0]));
    } else {
        string msg;
        for ( size_t i = 0; i < num_no_source; ++i ) {
            msg = NStr::IntToString(i + 1) + " of " + 
                NStr::IntToString(num_no_source) + 
                " Bioseqs without organism name";
            PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound, msg, 
                *(m_BioseqWithNoSource[i]));
        }
    }
}


bool CValidError_imp::IsNucAcc(const string& acc)
{
    if ( isupper((unsigned char) acc[0])  &&  acc.find('_') != NPOS ) {
        return true;
    }

    return false;
}


bool CValidError_imp::IsFarLocation(const CSeq_loc& loc)
{
    for ( CSeq_loc_CI citer(loc); citer; ++citer ) {
        CConstRef<CSeq_id> id(&citer.GetSeq_id());
        if ( id ) {
            CBioseq_Handle near_seq = 
                m_Scope->GetBioseqHandleFromTSE(*id, GetTSE());
            if ( !near_seq ) {
                return true;
            }
        }
    }

    return false;
}


CConstRef<CSeq_feat> CValidError_imp::GetCDSGivenProduct(const CBioseq& seq)
{
    CConstRef<CSeq_feat> feat;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);


    // In case of a NT bioseq limit the search to features packaged on the 
    // NT (we assume features have been pulled from the segments to the NT).
    CSeq_entry_Handle limit;
    if ( IsNT()  &&  m_TSE ) {
        limit = m_Scope->GetSeq_entryHandle(*m_TSE);
    }

    if ( bsh ) {
        CFeat_CI fi(bsh, 
                    SAnnotSelector(CSeqFeatData::e_Cdregion)
                    .SetByProduct()
                    .SetLimitTSE(limit));
        if ( fi ) {
            // return the first one (should be the one packaged on the
            // nuc-prot set).
            feat.Reset(&(fi->GetOriginalFeature()));
        }
    }

    return feat;
}


const CSeq_entry* CValidError_imp::GetAncestor
(const CBioseq& seq,
 CBioseq_set::EClass clss)
{
    const CSeq_entry* parent = 0;
    for ( parent = seq.GetParentEntry(); 
          parent != 0;
          parent = parent->GetParentEntry() ) {
        if ( parent->IsSet() ) {
            const CBioseq_set& set = parent->GetSet();
            if ( set.IsSetClass()  &&  set.GetClass() == clss ) {
                break;
            }
        }
    }
    return parent;
}


bool CValidError_imp::IsSerialNumberInComment(const string& comment)
{
    size_t pos = comment.find('[', 0);
    while ( pos != string::npos ) {
        ++pos;
        if ( isdigit((unsigned char) comment[pos]) ) {
            while ( isdigit((unsigned char) comment[pos]) ) {
                ++pos;
            }
            if ( comment[pos] == ']' ) {
                return true;
            }
        }

        pos = comment.find('[', pos);
    }
    return false;
}


bool CValidError_imp::CheckSeqVector(const CSeqVector& vec)
{
    if ( IsSequenceAvaliable(vec) ) {
        return true;
    }

    if ( IsRemoteFetch() ) {
        // issue some sort of error
    }
    return false;
}


bool CValidError_imp::IsSequenceAvaliable(const CSeqVector& vec)
{
    // IMPORTANT: This is a temporary implementation, due to (yet) restricted
    // implementation of the Scope / object manager classes.
    // if the first and last elements are accesible the sequence is available.
    try {
        vec[0]; 
        vec[vec.size() - 1];
    } catch ( const exception& ) {
        // do something
        return false;
    }

    return true;
}


// =============================================================================
//                                  Private
// =============================================================================


const char * script_tags[] = {"<script", "<object", "<applet", "<embed", "<form", "javascript:", "vbscript:", NULL};

void CValidError_imp::FindEmbeddedScript (const CSerialObject& obj)
{
    bool found = false;
    CStdTypeConstIterator<string> it(obj);

    while (it && !found) {
        for (int i = 0; script_tags[i] != NULL && !found; i++) {
            if (NStr::FindNoCase (*it, script_tags[i]) != string::npos) {
                found = true;
            }
        }
        ++it;
    }

    if (found) {
        PostErr (eDiag_Error, eErr_GENERIC_EmbeddedScript, "Script tag found in item", obj);
    }
}


void CValidError_imp::FindCollidingSerialNumbers (const CSerialObject& obj)
{
    if (m_PubSerialNumbers.size() < 2) {
        return;
    }
    sort (m_PubSerialNumbers.begin(), m_PubSerialNumbers.end());

    vector<int>::iterator it1 = m_PubSerialNumbers.begin();
    vector<int>::iterator it2 = it1;
    ++it2;
    while (it2 != m_PubSerialNumbers.end()) {
        if (*it1 == *it2) {
            PostErr (eDiag_Warning, eErr_GENERIC_CollidingSerialNumbers,
              "Multiple publications have serial number " + NStr::IntToString(*it1),
              obj);
            while (it2 != m_PubSerialNumbers.end() && *it2 == *it1) {
                ++it2;
            }
            if (it2 != m_PubSerialNumbers.end()) {
                it1 = it2;
                ++it2;
            }
        } else {
            it1 = it2;
            ++it2;
        }
    }
}


bool CValidError_imp::IsMixedStrands(const CSeq_loc& loc)
{
    if ( SeqLocCheck(loc, m_Scope) == eSeqLocCheck_warning ) {
        return false;
    }

    CSeq_loc_CI curr(loc);
    if ( !curr ) {
        return false;
    }
    CSeq_loc_CI prev = curr;
    ++curr;
    
    while ( curr ) {
        ENa_strand curr_strand = curr.GetStrand();
        ENa_strand prev_strand = prev.GetStrand();

        if ( (prev_strand == eNa_strand_minus  &&  
              curr_strand != eNa_strand_minus)   ||
             (prev_strand != eNa_strand_minus  &&  
              curr_strand == eNa_strand_minus) ) {
            return true;
        }

        prev = curr;
        ++curr;
    }

    return false;
}


static bool s_SeqLocHasGI (const CSeq_loc& loc)
{
    bool rval = false;

    for ( CSeq_loc_CI it(loc); it && !rval; ++it ) {
        if (it.GetSeq_id().IsGi()) {
            rval = true;
        }
    }
    return rval;
}


void CValidError_imp::Setup(const CSeq_entry_Handle& seh) 
{
    // "Save" the Seq-entry
    m_TSEH = seh;

    m_Scope.Reset(&m_TSEH.GetScope());
    
    m_TSE = m_TSEH.GetCompleteSeq_entry();
    
    // If no Pubs/BioSource in CSeq_entry, post only one error
    CTypeConstIterator<CPub> pub(ConstBegin(*m_TSE));
    m_NoPubs = !pub;
    CTypeConstIterator<CBioSource> src(ConstBegin(*m_TSE));
    m_NoBioSource = !src;
    
    // Look for genomic product set
    for (CTypeConstIterator <CBioseq_set> si (*m_TSE); si; ++si) {
        if (si->IsSetClass ()) {
            if (si->GetClass () == CBioseq_set::eClass_gen_prod_set) {
                m_IsGPS = true;
            }
        }
    }

    // Examine all Seq-ids on Bioseqs
    for (CTypeConstIterator <CBioseq> bi (*m_TSE); bi; ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, *bi) {
            const CSeq_id& sid = **sid_itr;
            CSeq_id::E_Choice typ = sid.Which();
            switch (typ) {
                case CSeq_id::e_not_set:
                    break;
                case CSeq_id::e_Local:
                    break;
                case CSeq_id::e_Gibbsq:
                    break;
                case CSeq_id::e_Gibbmt:
                    break;
                case CSeq_id::e_Giim:
                    break;
                case CSeq_id::e_Genbank:
                    m_IsGB = true;
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Embl:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Pir:
                    break;
                case CSeq_id::e_Swissprot:
                    break;
                case CSeq_id::e_Patent:
                    m_IsPatent = true;
                    break;
                case CSeq_id::e_Other:
                    m_IsRefSeq = true;
                    // and do RefSeq subclasses up front as well
                    if (sid.GetOther().IsSetAccession()) {
                        string acc =sid.GetOther().GetAccession().substr(0, 3);
                        if (acc == "NC_") {
                            m_IsNC = true;
                        } else if (acc == "NG_") {
                            m_IsNG = true;
                        } else if (acc == "NM_") {
                            m_IsNM = true;
                        } else if (acc == "NP_") {
                            m_IsNP = true;
                        } else if (acc == "NR_") {
                            m_IsNR = true;
                        } else if (acc == "NS_") {
                            m_IsNS = true;
                        } else if (acc == "NT_") {
                            m_IsNT = true;
                        } else if (acc == "NW_") {
                            m_IsNW = true;
                        } else if (acc == "XR_") {
                            m_IsXR = true;
                        }
                    }
                    break;
                case CSeq_id::e_General:
                    if ((*bi).IsAa()) {
                        m_ProteinHasGeneralID = true;
                    }
                    break;
                case CSeq_id::e_Gi:
                    m_IsGI = true;
                    break;
                case CSeq_id::e_Ddbj:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Prf:
                    break;
                case CSeq_id::e_Pdb:
                    m_IsPDB = true;
                    break;
                case CSeq_id::e_Tpg:
                    break;
                case CSeq_id::e_Tpe:
                    break;
                case CSeq_id::e_Tpd:
                    break;
                default:
                    break;
            }
        }
    }

    // examine features for location gi, product gi, and locus tag
    for (CFeat_CI feat_ci (seh); 
         feat_ci && (!m_FeatLocHasGI || !m_ProductLocHasGI || !m_GeneHasLocusTag);
         ++feat_ci) {
        if (s_SeqLocHasGI(feat_ci->GetLocation())) {
            m_FeatLocHasGI = true;
        }
        if (feat_ci->IsSetProduct() && s_SeqLocHasGI(feat_ci->GetProduct())) {
            m_ProductLocHasGI = true;
        }
        if (feat_ci->IsSetData() && feat_ci->GetData().IsGene() 
            && feat_ci->GetData().GetGene().IsSetLocus_tag()
            && !NStr::IsBlank (feat_ci->GetData().GetGene().GetLocus_tag())) {
            m_GeneHasLocusTag = true;
        }
    }
    
    if ( m_PrgCallback ) {
        m_NumAlign = 0;
        for (CTypeConstIterator<CSeq_align> i(*m_TSE); i; ++i) {
            m_NumAlign++;
        }
        m_NumAnnot = 0;
        for (CTypeConstIterator<CSeq_annot> i(*m_TSE); i; ++i) {
            m_NumAnnot++;
        }
        m_NumBioseq = 0;
        for (CTypeConstIterator<CBioseq> i(*m_TSE); i; ++i) {
            m_NumBioseq++;
        }
        m_NumBioseq_set = 0;
        for (CTypeConstIterator<CBioseq_set> i(*m_TSE); i; ++i) {
            m_NumBioseq_set++;
        }
        m_NumDesc = 0;
        for (CTypeConstIterator<CSeqdesc> i(*m_TSE); i; ++i) {
            m_NumDesc++;
        }
        m_NumDescr = 0;
        for (CTypeConstIterator<CSeq_descr> i(*m_TSE); i; ++i) {
            m_NumDescr++;
        }
        m_NumFeat = 0;
        for (CTypeConstIterator<CSeq_feat> i(*m_TSE); i; ++i) {
            m_NumFeat++;
        }
        m_NumGraph = 0;
        for (CTypeConstIterator<CSeq_graph> i(*m_TSE); i; ++i) {
            m_NumGraph++;
        }
        m_PrgInfo.m_Total = m_NumAlign + m_NumAnnot + m_NumBioseq + 
            m_NumBioseq_set + m_NumDesc + m_NumDescr + m_NumFeat + 
            m_NumGraph;
    }

    if (CNcbiApplication::Instance()->GetProgramDisplayName() == "tbl2asn") {
        m_IsTbl2Asn = true;
    }
    if (!m_IsTbl2Asn) {
        m_RequireTaxonID = true;
    }
}


void CValidError_imp::SetScope(const CSeq_entry& se)
{
    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddTopLevelSeqEntry(*const_cast<CSeq_entry*>(&se));
    m_Scope->AddDefaults();
}


void CValidError_imp::Setup(const CSeq_annot_Handle& sah)
{
    m_IsStandaloneAnnot = true;
    if (! m_Scope) {
        m_Scope.Reset(& sah.GetScope());
    }
    m_TSE.Reset(new CSeq_entry); // set a dummy Seq-entry
    m_TSEH = m_Scope->AddTopLevelSeqEntry(*m_TSE);
}


const string CValidError_imp::sm_SourceQualPrefixes[] = {
    "acronym:",
    "anamorph:",
    "authority:",
    "biotype:",
    "biovar:",
    "bio_material:",
    "breed:",
    "cell_line:",
    "cell_type:",
    "chemovar:",
    "chromosome:",
    "clone:",
    "clone_lib:",
    "collected_by:",
    "collection_date:",
    "common:",
    "country:",
    "cultivar:",
    "culture_collection:",
    "dev_stage:",
    "dosage:",
    "ecotype:",
    "endogenous_virus_name:",
    "environmental_sample:",
    "forma:",
    "forma_specialis:",
    "frequency:",
    "fwd_pcr_primer_name",
    "fwd_pcr_primer_seq",
    "fwd_primer_name",
    "fwd_primer_seq",
    "genotype:",
    "germline:",
    "group:",
    "haplotype:",
    "identified_by:",
    "insertion_seq_name:",
    "isolate:",
    "isolation_source:",
    "lab_host:",
    "lat_lon:"
    "left_primer:",
    "map:",
    "metagenome_source:",
    "metagenomic:",
    "nat_host:",
    "pathovar:",
    "plasmid_name:",
    "plastid_name:",
    "pop_variant:",
    "rearranged:",
    "rev_pcr_primer_name",
    "rev_pcr_primer_seq",
    "rev_primer_name",
    "rev_primer_seq",
    "right_primer:",
    "segment:",
    "serogroup:",
    "serotype:",
    "serovar:",
    "sex:",
    "specimen_voucher:",
    "strain:",
    "subclone:",
    "subgroup:",
    "substrain:",
    "subtype:",
    "sub_species:",
    "synonym:",
    "taxon:",
    "teleomorph:",
    "tissue_lib:",
    "tissue_type:",
    "transgenic:",
    "transposon_name:",
    "type:",
    "variety:",
};


void CValidError_imp::InitializeSourceQualTags() 
{
    m_SourceQualTags.reset(new CTextFsa);
    size_t size = sizeof(sm_SourceQualPrefixes) / sizeof(string);

    for (size_t i = 0; i < size; ++i ) {
        m_SourceQualTags->AddWord(sm_SourceQualPrefixes[i]);
    }

    m_SourceQualTags->Prime();
}


void CValidError_imp::ValidateSourceQualTags
(const string& str,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( str.empty() ) return;

    size_t str_len = str.length();

    int state = m_SourceQualTags->GetInitialState();
    for ( size_t i = 0; i < str_len; ++i ) {
        state = m_SourceQualTags->GetNextState(state, str[i]);
        if ( m_SourceQualTags->IsMatchFound(state) ) {
            string match = m_SourceQualTags->GetMatches(state)[0];
            if ( match.empty() ) {
                match = "?";
            }

            bool okay = true;
            if ( (int)(i - str_len) >= 0 ) {
                char ch = str[i - str_len];
                if ( !isspace((unsigned char) ch) || ch != ';' ) {
                    okay = false;
                }
            }
            if ( okay ) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_StructuredSourceNote,
                    "Source note has structured tag " + match, obj, ctx);
            }
        }
    }
}


bool CValidError_imp::IsCuratedRefSeq(void) const {
    return !(IsNM()  ||  IsNP()  || IsNG()  ||  IsNR());
}


void CValidError_imp::ValidateSeqLocIds
(const CSeq_loc& loc,
 const CSerialObject& obj)
{
    for ( CSeq_loc_CI lit(loc); lit; ++lit ) {
        const CSeq_id& id1 = lit.GetSeq_id();
        CSeq_loc_CI  lit2 = lit;
        for ( ++lit2; lit2; ++lit2 ) {
            const CSeq_id& id2 = lit2.GetSeq_id();
            if ( IsSameBioseq(id1, id2, m_Scope)  &&  !id1.Match(id2) ) {
                PostErr(eDiag_Warning,
                    eErr_SEQ_FEAT_DifferntIdTypesInSeqLoc,
                    "Two ids refer to the same bioseq but are of "
                    "different type", obj);
            }
        }
    } 
}


// =============================================================================
//                         CValidError_base Implementation
// =============================================================================


CValidError_base::CValidError_base(CValidError_imp& imp) :
    m_Imp(imp), m_Scope(imp.GetScope())
{
}


CValidError_base::~CValidError_base()
{
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSerialObject& obj) 
{
    m_Imp.PostErr(sv, et, msg, obj);
}


//void CValidError_base::PostErr
//(EDiagSev sv,
// EErrType et,
// const string& msg,
// TDesc ds)
//{
//    m_Imp.PostErr(sv, et, msg, ds);
//}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TFeat ft)
{
    m_Imp.PostErr(sv, et, msg, ft);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TBioseq sq)
{
    m_Imp.PostErr(sv, et, msg, sq);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TEntry ctx,
 TDesc ds)
{
    m_Imp.PostErr(sv, et, msg, ctx, ds);
}


//void CValidError_base::PostErr
//(EDiagSev sv,
// EErrType et,
// const string& msg,
// TBioseq sq,
// TDesc ds)
//{
//    m_Imp.PostErr(sv, et, msg, sq, ds);
//}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg, 
 TSet set)
{
    m_Imp.PostErr(sv, et, msg, set);
}


//void CValidError_base::PostErr
//(EDiagSev sv, 
// EErrType et, 
// const string& msg, 
// TSet set,
// TDesc ds)
//{
//    m_Imp.PostErr(sv, et, msg, set, ds);
//}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TAnnot annot)
{
    m_Imp.PostErr(sv, et, msg, annot);
}

void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TGraph graph)
{
    m_Imp.PostErr(sv, et, msg, graph);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TBioseq sq,
 TGraph graph)
{
    m_Imp.PostErr(sv, et, msg, sq, graph);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TAlign align)
{
    m_Imp.PostErr(sv, et, msg, align);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TEntry entry)
{
    m_Imp.PostErr(sv, et, msg, entry);
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
