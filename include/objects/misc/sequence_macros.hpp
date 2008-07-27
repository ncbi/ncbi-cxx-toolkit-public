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
* Author: Jonathan Kans
*
* File Description: Utility macros for exploring NCBI objects
*
* ===========================================================================
*/

#ifndef __SEQUENCE_MACROS__HPP__
#define __SEQUENCE_MACROS__HPP__


#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/submit/submit__.hpp>
#include <objects/seqblock/seqblock__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/biblio/biblio__.hpp>
#include <objects/pub/pub__.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// @NAME Convenience macros for NCBI object iterators
/// @{


/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeqdesc definitions

#define NCBI_SEQDESC(Type) CSeqdesc::e_##Type

typedef CSeqdesc::E_Choice TSEQDESC_CHOICE;


/// CSubSource definitions

#define NCBI_SUBSRC(Type) CSubSource::eSubtype_##Type

typedef CSubSource::TSubtype TSUBSRC_SUBTYPE;


/// COrgMod definitions

#define NCBI_ORGMOD(Type) COrgMod::eSubtype_##Type

typedef COrgMod::TSubtype TORGMOD_SUBTYPE;


/// CPub definitions

#define NCBI_PUB(Type) CPub::e_##Type

typedef CPub::E_Choice TPUB_CHOICE;


/////////////////////////////////////////////////////////////////////////////
/// Macros for obtaining closest specific CSeqdesc applying to a CBioseq
/////////////////////////////////////////////////////////////////////////////


/// IF_EXISTS_CLOSEST_MOLINFO
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CMolInfo& molinf = (*cref).GetMolinfo();
#define IF_EXISTS_CLOSEST_MOLINFO(Cref, Bsq) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Molinfo))

/// IF_EXISTS_CLOSEST_BIOSOURCE
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CBioSource& source = (*cref).GetSource();
#define IF_EXISTS_CLOSEST_BIOSOURCE(Cref, Bsq) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Source))

/// IF_EXISTS_CLOSEST_TITLE
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const string& title = (*cref).GetTitle();
#define IF_EXISTS_CLOSEST_TITLE(Cref, Bsq) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Title))


/////////////////////////////////////////////////////////////////////////////
/// Macros to recursively explore within NCBI data model objects
/////////////////////////////////////////////////////////////////////////////


/// NCBI_SERIAL_TEST_EXPLORE base macro tests to see if loop should be entered
// If okay, calls CTypeConstIterator for recursive exploration
#define NCBI_SERIAL_TEST_EXPLORE(Test, Type, Var, Cont) \
if (! (Test)) {} else for (CTypeConstIterator<Type> Var (Cont); Var; ++Var)


/// CSeq_entry explorers

/// FOR_ALL_BIOSEQS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CBioseq&
// Dereference with const CBioseq& bioseq = *iter;
#define FOR_ALL_BIOSEQS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CBioseq, \
    Iter, \
    (Seq))

/// FOR_ALL_SEQSETS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CBioseq_set&
// Dereference with const CBioseq_set& bss = *iter;
#define FOR_ALL_SEQSETS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CBioseq_set, \
    Iter, \
    (Seq))

/// FOR_ALL_SEQENTRYS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& seqentry = *iter;
#define FOR_ALL_SEQENTRYS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_entry, \
    Iter, \
    (Bss))

/// FOR_ALL_DESCRIPTORS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = *iter;
#define FOR_ALL_DESCRIPTORS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeqdesc, \
    Iter, \
    (Seq))
    
/// FOR_ALL_ANNOTS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = *iter;
#define FOR_ALL_ANNOTS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_annot, \
    Iter, \
    (Seq))
    
/// FOR_ALL_FEATURES_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_feat& feat = *iter;
#define FOR_ALL_FEATURES_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_feat, \
    Iter, \
    (Seq))
    
/// FOR_ALL_ALIGNS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_align& align = *iter;
#define FOR_ALL_ALIGNS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_align, \
    Iter, \
    (Seq))
    
/// FOR_ALL_GRAPHS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_graph& graph = *iter;
#define FOR_ALL_GRAPHS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_graph, \
    Iter, \
    (Seq))
    
    
/////////////////////////////////////////////////////////////////////////////
/// Macros to iterate over standard template containers (non-recursive)
/////////////////////////////////////////////////////////////////////////////


/// NCBI_TEST_ITERATE base macro tests to see if loop should be entered
// If okay, calls ITERATE for linear STL iteration
#define NCBI_TEST_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ITERATE(Type, Var, Cont)


/// CSeq_submit iterators

/// FOR_EACH_SEQENTRY_ON_SEQSUBMIT
// Takes const CSeq_submit& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& se = **iter;
#define FOR_EACH_SEQENTRY_ON_SEQSUBMIT(Iter, Ss) \
NCBI_TEST_ITERATE((Ss).IsEntrys(), \
    CSeq_submit::TData::TEntrys, \
    Iter, \
    (Ss).GetData().GetEntrys())


/// CSeq_entry iterators

/// FOR_EACH_DESCRIPTOR_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter
#define FOR_EACH_DESCRIPTOR_ON_SEQENTRY(Iter, Seq) \
NCBI_TEST_ITERATE((Seq).IsSetDescr(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Seq).GetDescr().Get())


/// CBioseq iterators

/// FOR_EACH_DESCRIPTOR_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter
#define FOR_EACH_DESCRIPTOR_ON_BIOSEQ(Iter, Bsq) \
NCBI_TEST_ITERATE((Bsq).IsSetDescr(), \
    CBioseq::TDescr::Tdata, \
    Iter, \
    (Bsq).GetDescr().Get())

/// FOR_EACH_ANNOT_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = **iter;
#define FOR_EACH_ANNOT_ON_BIOSEQ(Iter, Bsq) \
NCBI_TEST_ITERATE((Bsq).IsSetAnnot(), \
    CBioseq::TAnnot, \
    Iter, \
    (Bsq).GetAnnot())

/// FOR_EACH_SEQID_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_id&
// Dereference with const CSeq_id& sid = **iter;
#define FOR_EACH_SEQID_ON_BIOSEQ(Iter, Bsq) \
NCBI_TEST_ITERATE((Bsq).IsSetId(), \
    CBioseq::TId, \
    Iter, \
    (Bsq).GetId())


/// CBioseq_set iterators

/// FOR_EACH_DESCRIPTOR_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter;
#define FOR_EACH_DESCRIPTOR_ON_SEQSET(Iter, Bss) \
NCBI_TEST_ITERATE((Bss).IsSetDescr(), \
    CBioseq_set::TDescr::Tdata, \
    Iter, \
    (Bss).GetDescr().Get())

/// FOR_EACH_ANNOT_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = **iter;
#define FOR_EACH_ANNOT_ON_SEQSET(Iter, Bss) \
NCBI_TEST_ITERATE((Bss).IsSetAnnot(), \
    CBioseq_set::TAnnot, \
    Iter, \
    (Bss).GetAnnot())

/// FOR_EACH_SEQENTRY_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& se = **iter;
#define FOR_EACH_SEQENTRY_ON_SEQSET(Iter, Bss) \
NCBI_TEST_ITERATE((Bss).IsSetSeq_set(), \
    CBioseq_set::TSeq_set, \
    Iter, \
    (Bss).GetSeq_set())


/// CSeq_annot iterators

/// FOR_EACH_FEATURE_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_feat& feat = **iter;
#define FOR_EACH_FEATURE_ON_ANNOT(Iter, San) \
NCBI_TEST_ITERATE((San).IsFtable(), \
    CSeq_annot::TData::TFtable, \
    Iter, \
    (San).GetData().GetFtable())

/// FOR_EACH_ALIGN_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_align&
// Dereference with const CSeq_align& align = **iter;
#define FOR_EACH_ALIGN_ON_ANNOT(Iter, San) \
NCBI_TEST_ITERATE((San).IsAlign(), \
    CSeq_annot::TData::TAlign, \
    Iter, \
    (San).GetData().GetAlign())

/// FOR_EACH_GRAPH_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_graph&
// Dereference with const CSeq_graph& graph = **iter;
#define FOR_EACH_GRAPH_ON_ANNOT(Iter, San) \
NCBI_TEST_ITERATE((San).IsGraph(), \
    CSeq_annot::TData::TGraph, \
    Iter, \
    (San).GetData().GetGraph())


/// CSeq_descr iterators

/// FOR_EACH_DESCRIPTOR_ON_DESCR
// Takes const CSeq_descr& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter;
#define FOR_EACH_DESCRIPTOR_ON_DESCR(Iter, Descr) \
NCBI_TEST_ITERATE((Descr).IsSet(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Descr).Get())


/// CBioSource iterators

/// FOR_EACH_SUBSOURCE_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const CSubSource&
// Dereference with const CSubSource& sbs = **iter
#define FOR_EACH_SUBSOURCE_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_TEST_ITERATE((Bsrc).IsSetSubtype(), \
    CBioSource::TSubtype, \
    Iter, \
    (Bsrc).GetSubtype())

/// FOR_EACH_ORGMOD_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter
#define FOR_EACH_ORGMOD_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_TEST_ITERATE((Bsrc).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Bsrc).GetOrgname().GetMod())


/// COrgRef iterators

/// FOR_EACH_ORGMOD_ON_ORGREF
// Takes const COrgRef& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter
#define FOR_EACH_ORGMOD_ON_ORGREF(Iter, Org) \
NCBI_TEST_ITERATE((Org).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Org).GetOrgname().GetMod())


/// COrgName iterators

/// FOR_EACH_ORGMOD_ON_ORGNAME
// Takes const COrgName& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter
#define FOR_EACH_ORGMOD_ON_ORGNAME(Iter, Onm) \
NCBI_TEST_ITERATE((Onm).IsSetMod(), \
    COrgName::TMod, \
    Iter, \
    (Onm).GetMod())


/// CPubdesc iterators

/// FOR_EACH_PUB_ON_PUBDESC
// Takes const CPubdesc& as input and makes iterator to const CPub&
// Dereference with const CPub& pub = **iter;
#define FOR_EACH_PUB_ON_PUBDESC(Iter, Pbd) \
NCBI_TEST_ITERATE((Pbd).IsSetPub() && \
        (Pbd).GetPub().IsSet(), \
    CPub_equiv::Tdata, \
    Iter, \
    (Pbd).GetPub().Get())


/// CPub iterators

//// FOR_EACH_AUTHOR_ON_PUB
// Takes const CPub& as input and makes iterator to const CAuthor&
// Dereference with const CAuthor& auth = **iter;
#define FOR_EACH_AUTHOR_ON_PUB(Iter, Pub) \
NCBI_TEST_ITERATE((Pub).IsSetAuthors() && \
        (Pub).GetAuthors().IsSetNames() && \
        (Pub).GetAuthors().GetNames().IsStd() , \
    CAuth_list::C_Names::TStd, \
    Iter, \
    (Pub).GetAuthors().GetNames().GetStd())


/// CSeq_feat iterators

/// FOR_EACH_GBQUAL_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CGb_qual&
// Dereference with const CGb_qual& gbq = **iter;
#define FOR_EACH_GBQUAL_ON_FEATURE(Iter, Sft) \
NCBI_TEST_ITERATE((Sft).IsSetQual(), \
    CSeq_feat::TQual, \
    Iter, \
    (Sft).GetQual())


/// CCdregion iterators

/// FOR_EACH_CODEBREAK_ON_CDREGION
// Takes const CCdregion& as input and makes iterator to const CCode_break&
// Dereference with const CCode_break& cbk = **iter;
#define FOR_EACH_CODEBREAK_ON_CDREGION(Iter, Cdr) \
NCBI_TEST_ITERATE((Cdr).IsSetCode_break(), \
    CCdregion::TCode_break, \
    Iter, \
    (Cdr).GetCode_break())


/// @}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* __SEQUENCE_MACROS__HPP__ */

