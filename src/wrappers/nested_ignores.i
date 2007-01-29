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
 * File Description:  SWIG %ignore's connected to nested classes
 *
 */

%ignore *::CBioTree_CBioNode;

%ignore *::CSeqConvert_IPackTarget::NewSegment;

%ignore *::CTSE_Info_SFeatIdIndex::m_Chunks;
%ignore *::CTSE_Info_SFeatIdInfo::CTSE_Info_SFeatIdInfo;

%ignore *::CVecscreen_AlnInfo::type;

%ignore *::CReaderCacheManager_SReaderCacheInfo;

// hide util/thread_pool.hpp due to classes nested in CBlockingQueue
#define THREAD_POOL__HPP
// fallout from that
%ignore CPrefetchManager_Impl;

%ignore *::CFileUtil_SFileSystemInfo::fs_type;

%ignore *::I_DriverContext_SConnAttr::mode;

%ignore *::CNetScheduleClient_SJobBatch::job_list;

%ignore *::CStlClassInfoFunctions_set_SInitializer;
%ignore *::CStlClassInfoFunctions_multiset_SInitializer;

// Hide util/bitset/bm.h from SWIG
#define BM__H__INCLUDED__

// Data member whose type is an enum of the enclosing class.
%ignore *::CEventHandler_SPostRequest::m_DispHow;

// For some reason SWIG thinks that this nested struct has
// a default ctor.
%ignore *::CSeqMaskerIstat_optimization_data;

// Temporary class
%ignore ncbi::NStr_TStringToNumFlags;

// Copy ctors cause trouble
%ignore *::CSeqMasker_CSeqMaskerException;
%ignore *::CSeqMaskerUsetSimple_Exception;
%ignore *::CSeqMaskerIstatAscii_Exception;
%ignore *::CSeqMaskerUsetArray_Exception;
%ignore *::CSeqMaskerIstatBin_Exception;
%ignore *::CSeqMaskerIstatFactory_Exception;
%ignore *::CSeqMaskerUsetHash_Exception;
%ignore *::CSeqMaskerIstatOAscii_Exception;
%ignore *::CSeqMaskerIstatOBinary_Exception;
%ignore *::CSeqMaskerOstat_CSeqMaskerOstatException;
%ignore *::CSeqMaskerOstatAscii_CSeqMaskerOstatAsciiException;
%ignore *::CSeqMaskerOstatFactory_CSeqMaskerOstatFactoryException;
%ignore *::CSeqMaskerOstatOpt_Exception;

%ignore *::CSymDustMasker_perfect;  // shouldn't be public anyway

%ignore *::CTSE_Info_SIdAnnotInfo;  // in objmgr/impl

%ignore ncbi::objects::CSeqVector_CI_CTempValue;

%ignore *::CAlignInfo_CExon::GetGenomicLocation();
%ignore *::CAlignInfo_CExon::GetTranscriptLocation();
%ignore *::CAlignInfo_CExon::CreateSeqAlign();
%ignore *::CAlignInfo_CAlignSegment::GetGenomicLocation();
%ignore *::CAlignInfo_CAlignSegment::GetTranscriptLocation();
%ignore *::CAlignInfo_CAlignSegment::CreateSeqAlign();

%ignore *::CAlignInfo_CExon;
%ignore *::CAlignInfo_CAlignSegment;

%ignore *::CDataSource_ScopeInfo_STSE_Key::operator<;
%ignore *::CDataSource_ScopeInfo_STSE_Key;

%ignore *::CMemoryRegistry_SSection;

%ignore ncbi::CSplign_SAlignedCompartment::m_segments;

%ignore *::CDisplaySeqalign_SeqlocInfo;

%ignore *::CBDB_Cache_CacheKey;
%ignore *::CMetaRegistry_SEntry;
%ignore *::CDll_TEntryPoint;
%ignore *::CDllResolver_SResolvedEntry;
%ignore *::IClassFactory_SDriverInfo;
%ignore *::CPluginManager_SDriverInfo;

// were fixed (MSVC6)
%ignore *::CIntervalTreeTraits_SNodeMapValue;
%ignore *::CIntervalTreeTraits_STreeMapValue;

%ignore *::CPackString_SNode;

%ignore *::CGBLGuard_SLeveledMutex;

// was fixed (MSVC6)
%ignore *::ITreeIterator_I4Each;

%ignore ncbi::objects::SAlignment_Segment_SAlignment_Row::SameStrand(const SAlignment_Row&) const;
%ignore *::CObjectsSniffer_SCandidateInfo;
%ignore *::CSeqSearch_IClient;
%ignore *::CNCBINode_TMode;
%ignore *::CCgiCookie_PLessCPtr;
%ignore *::CSerializable_CProxy;
%ignore *::CSeqportUtil_CBadIndex;
// copy ctor
%ignore *::CSeq_annot_CI_SEntryLevel_CI;

%ignore *::CGBDataLoader_SGBLoaderParam;
%ignore *::CUsrFeatDataLoader_SUsrFeatParam;

// problem with TRange
%ignore *::CFlatPrimary_SPiece;


/*
 * ===========================================================================
 * $Log$
 * Revision 1.17  2006/11/17 18:18:44  jcherry
 * Deal with CTSE_Info substructures
 *
 * Revision 1.16  2006/10/10 18:33:31  jcherry
 * %ignore CVecscreen::AlnInfo::type
 *
 * Revision 1.15  2006/05/30 18:36:08  jcherry
 * %ignore CReaderCacheManager::SReaderCacheInfo
 *
 * Revision 1.14  2006/02/09 19:42:51  jcherry
 * Work around problem with classes nested in CBlockingQueue (a template)
 *
 * Revision 1.13  2006/01/12 17:03:02  jcherry
 * %ignore CFileUtil::SFileSystemInfo::fs_type
 *
 * Revision 1.12  2006/01/05 16:00:56  jcherry
 * %ignore I_DriverContext::SConnAttr::mode
 *
 * Revision 1.11  2006/01/03 14:37:44  jcherry
 * %ignore CNetScheduleClient::SJobBatch::job_list
 *
 * Revision 1.10  2005/11/19 21:27:26  jcherry
 * %ignore CStlClassInfoFunctions_set::SInitializer and
 * CStlClassInfoFunctions_multiset::SInitializer
 *
 * Revision 1.9  2005/11/01 15:51:24  jcherry
 * CSymDustMasker::lcr changed to CSymDustMasker::perfect
 *
 * Revision 1.8  2005/10/07 17:12:31  jcherry
 * Hide util/bitset/bm.h from SWIG
 *
 * Revision 1.7  2005/09/13 15:23:24  jcherry
 * Remove %ignore's related to CCompartmentFinder, which no longer exists
 *
 * Revision 1.6  2005/08/31 17:02:55  jcherry
 * %ignore CSeqMaskerIstat::optimization_data and
 * CEventHandler::SPostRequest::m_DispHow
 *
 * Revision 1.5  2005/08/06 15:00:34  jcherry
 * %ignore temporary class NStr::TStringToNumFlags
 *
 * Revision 1.4  2005/08/03 14:57:10  jcherry
 * Added headers
 *
 * Revision 1.3  2005/07/15 15:25:02  jcherry
 * %ignore CTSE_Info::SIdAnnotInfo
 *
 * Revision 1.2  2005/07/08 16:31:50  jcherry
 * %ignore ncbi::CCompartmentFinder_CCompartment::SetMembers
 *
 * Revision 1.1  2005/05/11 22:23:14  jcherry
 * Initial version
 *
 * ===========================================================================
 */
