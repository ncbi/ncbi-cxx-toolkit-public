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


%ignore ncbi::objects::CAutoDefSourceModifierInfo::AddQual;
%ignore ncbi::objects::CAutoDefSourceModifierInfo::RemoveQual;
%ignore ncbi::objects::CAutoDefModifierCombo::GetNumUniqueDescriptions;
%ignore ncbi::objects::CAutoDefModifierCombo::AllUnique;
%ignore ncbi::objects::CAutoDef::GetBestModifierList;

%ignore *::CRemoteBlastDataLoader_CCachedSeqData::AddDelta;

%ignore ncbi::objects::CAutoDef::DoAutoDef;

%ignore ncbi::CQueryParseNode::GetIdentIdx;

%ignore ncbi::IServer_StreamHandler::GetStream;

%ignore ncbi::blast::kMinRawGappedScore;

%ignore ncbi::CDiagCollectGuard::SetPrintSeverity;
%ignore ncbi::CDiagCollectGuard::SetCollectSeverity;

%ignore ncbi::objects::CAnnotObject_Ref::IsTableFeat;
%ignore ncbi::objects::CSeq_annot_EditHandle::Update;

%ignore ncbi::CWriteDB::AddDefline;

%ignore ncbi::blast::CBlastOptionsHandle::ClearFilterOptions;

%ignore ncbi::DataLoaders_Register_BlastDB;

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
