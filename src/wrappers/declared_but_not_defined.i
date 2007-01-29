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


%ignore ncbi::objects::CSeq_feat_Handle::GetSNPQualityCodeStr;
%ignore ncbi::objects::CSeq_feat_Handle::GetSNPQualityCodeOs;

%ignore ncbi::CNetScheduleAdmin::Logging;
%ignore ncbi::CNetScheduleSubmitter::GetJobDetails;
%ignore ncbi::CNetScheduleExecuter::GetJobDetails;
%ignore ncbi::CNetScheduleAdmin::StatusSnapshot;

%ignore ncbi::objects::CLDS_Manager::DeleteDB;

%ignore ncbi::objects::SAnnotObjectsIndex::ClearIndex();

%ignore ncbi::objects::CPrefetchToken::GetPriority;
%ignore ncbi::objects::CPrefetchToken::SetPriority;
%ignore ncbi::objects::CPrefetchManager::GetThreadCount;
%ignore ncbi::objects::CPrefetchManager::SetThreadCount;
%ignore ncbi::objects::CPrefetchToken_Impl::SetListener;

%ignore *::CHandleRangeMap::AddRange(const CSeq_id&, TSeqPos, TSeqPos,
                                     ENa_strand);
%ignore *::CHandleRangeMap::AddRange(const CSeq_id&, TSeqPos, TSeqPos);

%ignore ncbi::objects::CAnnotObject_Info::GetObject;
%ignore ncbi::objects::CAnnotObject_Info::GetObjectPointer;

%ignore ncbi::objects::SAnnotObjectsIndex::ReserveInfoSize;

%ignore ncbi::objects::CConstSageData::GetMethod() const;

%ignore ncbi::objects::CCddDataLoader::CCddDataLoader();

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

%ignore ncbi::x_SwapPointers(void* volatile*, void*);

%ignore ncbi::CHTMLHelper::LoadIDList(TIDList& ids,  
                                      const TCgiEntries& values,
                                      const string& hiddenPrefix,
                                      const string& checkboxPrefix);
%ignore ncbi::CHTMLHelper::StoreIDList(CHTML_form *form,
                                       const TIDList&  ids,
                                       const string&  hiddenPrefix,
                                       const string&  checkboxPrefix);

%ignore ncbi::COptionDescription::COptionDescription();


/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2006/09/06 16:06:47  jcherry
 * Added %ignore (CFlatFileConfig::SetCodonRecognizedToNote) and removed an
 * obsolete one (CThreadNonStop::RequestDoJob)
 *
 * Revision 1.18  2006/08/17 20:22:22  jcherry
 * %ignore ncbi::CThreadNonStop::RequestDoJob
 *
 * Revision 1.17  2006/08/01 13:47:13  jcherry
 * Removed %ignore's of some CAlnMap methods, which have been removed
 * from the class declaration
 *
 * Revision 1.16  2006/02/09 19:43:56  jcherry
 * %ignore's for Object Manager prefetch stuff
 *
 * Revision 1.15  2006/01/11 16:49:27  jcherry
 * %ignore some CHandleRangeMap::AddRange signatures.
 * Drop %ignore of CAlignVec::GetScore.
 *
 * Revision 1.14  2005/12/15 20:30:11  jcherry
 * %ignore ncbi::gnomon::CAlignVec::GetScore
 *
 * Revision 1.13  2005/11/14 15:10:40  jcherry
 * Don't %ignore CObjectIStreamAsnBinary methods (problem fixed)
 *
 * Revision 1.12  2005/11/10 19:06:00  jcherry
 * %ignore some CObjectIStreamAsnBinary methods that are inline in a .cpp
 *
 * Revision 1.11  2005/09/23 19:18:56  jcherry
 * %ignore CAnnotObject_Info::GetObject and CAnnotObject_Info::GetObjectPointer
 *
 * Revision 1.10  2005/09/16 15:06:43  jcherry
 * Removed some obsolete %ignore's.
 *
 * Revision 1.9  2005/09/15 15:13:48  jcherry
 * Removed some obsolete %ignore's.  Rearranged a bit.
 *
 * Revision 1.8  2005/08/31 17:01:47  jcherry
 * %ignore SAnnotObjectsIndex::ReserveInfoSize
 *
 * Revision 1.7  2005/08/12 20:26:53  jcherry
 * Added objtools/format
 *
 * Revision 1.6  2005/07/05 19:46:06  jcherry
 * Restored %ignore that was accidentally removed
 *
 * Revision 1.5  2005/07/05 14:54:06  jcherry
 * Removed commented-out %ignore's altogether
 *
 * Revision 1.4  2005/06/29 16:12:24  vasilche
 * Commented out fixed %ignore statements.
 *
 * Revision 1.3  2005/06/28 14:50:31  jcherry
 * Restore %ignore for BLAST_RPSSearchEngine
 *
 * Revision 1.2  2005/06/27 17:11:38  jcherry
 * Ignore CScope_Impl::x_GetBioseq_Lock; remove some obsolete %ignore's
 *
 * Revision 1.1  2005/05/11 22:23:14  jcherry
 * Initial version
 *
 * ===========================================================================
 */
