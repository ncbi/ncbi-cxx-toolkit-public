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

// A friend defined in the CComparmentFinder::CCompartment body
%ignore PLowerSubj;
%ignore *::PLowerSubj;
%ignore ncbi::PLowerSubj;
%ignore ncbi::CCompartmentFinder_CCompartment::PLowerSubj;

%ignore ncbi::CSplign_SAlignedCompartment::m_segments;
%ignore ncbi::CCompartmentFinder_CCompartment::GetMembers;
%ignore ncbi::CCompartmentFinder_CCompartment::operator<;

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
 * Revision 1.1  2005/05/11 22:23:14  jcherry
 * Initial version
 *
 * ===========================================================================
 */
