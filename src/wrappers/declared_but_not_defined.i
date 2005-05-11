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
 * Authors:  Josh Cherry
 *
 * File Description:
 *      SWIG %ignore's for things that are declared but not defined
 *
 */

%ignore BLAST_RPSSearchEngine;

%ignore ncbi::CNlmZipBtRdr::GetCompressedSize;
%ignore ncbi::CNlmZipBtRdr::GetDecompressionTime;

%ignore ncbi::CAlignSelector::SetPublicOnly;

%ignore ncbi::CHTML_area::CHTML_area();

%ignore ncbi::CSeqDBIter::CSeqDBIter(const CSeqDBIter&);

// objtools/data_loaders/cdd/cdd.hpp
%ignore ncbi::objects::CCddDataLoader::CCddDataLoader();

%ignore ncbi::objects::CScope_Impl::AddAnnot(const CSeq_annot& annot,
                                          TPriority pri = kPriority_NotSet);	
%ignore ncbi::objects::CTSE_Info::SetUsedMemory(size_t);
%ignore ncbi::objects::CCachedId1Reader::LoadBlob
	(CID1server_back&, CRef<CID2S_Split_Info>&, CBlob_id const&);


%ignore ncbi::objects::CFlatCodonQV::CFlatCodonQV(const string&);
%ignore ncbi::CIntervalTree::Add(const interval_type &interval,
                                 const mapped_type &value);
%ignore ncbi::CIntervalTree::Add(const interval_type &interval,
                                 const mapped_type &value, const nothrow_t &);
%ignore ncbi::CIntervalTree::Set(const interval_type&, const mapped_type&);
%ignore ncbi::CIntervalTree::Find();
%ignore ncbi::CIntervalTree::Find() const;
%ignore ncbi::CIntervalTree::Reset(const interval_type&);
%ignore ncbi::CIntervalTree::Delete(const interval_type&);
%ignore ncbi::CIntervalTree::Delete(const interval_type&, nothrow_t const&);
%ignore ncbi::CIntervalTree::Replace(const interval_type&, const mapped_type&);
%ignore ncbi::CIntervalTree::Replace(const interval_type&, const mapped_type&,
                                     nothrow_t const&);

%ignore ncbi::CMemoryChunk::SetNextChunk(CRef<CMemoryChunk>);
%ignore ncbi::x_SwapPointers(void* volatile*, void*);
%ignore ncbi::CPIDGuard::Remove;
%ignore ncbi::objects::CPubseqReader::GetConnection(TConn);
%ignore ncbi::objects::CSeq_annot_Info::sx_ShallowCopy(TObject&);
%ignore ncbi::objects::CSeq_entry_Info::x_Attach(CRef<CBioseq_Info>);
%ignore ncbi::objects::CSeq_entry_Info::x_Attach(CRef<CSeq_entry_Info>, int);
%ignore ncbi::objects::CSeq_entry_Info::x_Attach;
%ignore ncbi::objects::CBioseq_Base_Info::ResetAnnot();
// inline in .cpp
%ignore ncbi::objects::CBioseq_Base_Info::x_IsEndNextDesc(TDesc_CI iter) const;
%ignore ncbi::objects::CSeqMap_Delta_seqs::
    CSeqMap_Delta_seqs(const TObject& obj,
                       CSeqMap_Delta_seqs *parent, size_t index);
%ignore ncbi::objects::CSeqMap_CI_SegmentInfo::
    CSeqMap_CI_SegmentInfo(const CConstRef< CSeqMap > &seqMap, size_t index);
%ignore ncbi::objects::CScope::AddSeq_annot(const CSeq_annot &annot,
                                            TPriority pri=kPriority_NotSet);
%ignore ncbi::objects::CSeqMap_CI::x_GetSubSeqMap(bool) const;
%ignore ncbi::objects::CSeqMap_CI::x_GetSubSeqMap() const;
%ignore ncbi::objects::CConstSageData::GetMethod() const;
%ignore ncbi::objects::CIndexedStrings::StoreTo(CNcbiOstream&) const;

%ignore ncbi::objects::CBioseq_EditHandle::AddId(CSeq_id_Handle const&) const;
%ignore ncbi::objects::CBioseq_EditHandle::ResetId() const;
%ignore ncbi::objects::CBioseq_EditHandle::
    RemoveId(CSeq_id_Handle const&) const;
%ignore ncbi::objects::CBioseq_set_EditHandle::RemoveDesc(CSeqdesc&) const;
%ignore ncbi::objects::CBioseq_set_EditHandle::AddAllDescr(CSeq_descr&) const;
%ignore ncbi::objects::CBioseq_set_EditHandle::AddDesc(CSeqdesc&) const;

%ignore ncbi::objects::CAlnMap::
    GetNumberOfInsertedSegmentsOnLeft(TNumrow row, TNumseg seg) const;
%ignore ncbi::objects::CAlnMap::
    GetNumberOfInsertedSegmentsOnRight(TNumrow row, TNumseg seg) const;
%ignore ncbi::objects::CSeqMap::ResolveBioseqLength(const CSeq_id& id,
                                                    CScope *scope);

%ignore ncbi::CHTML_table::ColumnWidth(CHTML_table *, TIndex, const string &);
%ignore ncbi::CHTMLHelper::LoadIDList(TIDList& ids,  
                                      const TCgiEntries& values,
                                      const string& hiddenPrefix,
                                      const string& checkboxPrefix);
%ignore ncbi::CHTMLHelper::StoreIDList(CHTML_form *form,
                                       const TIDList&  ids,
                                       const string&  hiddenPrefix,
                                       const string&  checkboxPrefix) ;

%ignore ncbi::COptionDescription::COptionDescription();

// blast::CBlastHSPResults::DebugDump not defined in blast_aux.cpp
// (or anywhere else); it's a virtual function, so this class is unusable.
// Implementation committed 3/28/05
%ignore ncbi::blast::CBlastHSPResults;

// inline functions defined in .cpp
%ignore ncbi::objects::CAnnotObject_Ref::
    CAnnotObject_Ref(const CAnnotObject_Info& object);
%ignore ncbi::objects::CAnnotObject_Ref::
    CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_annot, TSeqPos index);
%ignore ncbi::objects::CAnnotObject_Ref::
    SetSNP_Point(const SSNP_Info& snp, CSeq_loc_Conversion* cvt);

// sometimes inline in .cpp
// (depending on preprocessor symbol CHECK_STREAM_INTEGRITY)
%ignore ncbi::CObjectOStreamAsnBinary::WriteByte;


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 22:23:14  jcherry
 * Initial version
 *
 * ===========================================================================
 */
