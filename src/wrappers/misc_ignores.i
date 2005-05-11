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
 * File Description:  Miscellaneous SWIG %ignore's
 *
 */


// Declared as friend; we don't want to wrap it,
// plus SWIG tries to wrap it for CRefs too
%ignore *::s_CleanupThreads;
// A friend, no export specifier
%ignore s_CleanupThreadsTls;

%{
// An ugly thing having to do with classes nested in
// CIntervalTreeTraits and SWIG more recent than 1.3.21
#define TMapValue CIntervalTreeTraits::TMapValue
%}

// non-const version causes non-const methods of
// pointee to be neglected
%ignore ncbi::CRef::operator->() const;

%ignore *::DebugDump;

// problem with struct stat
%ignore ncbi::CDirEntry_SStat::orig;

// Private typedef problem (g++ OK, MSVC not OK)
%ignore ncbi::CTreeIterator::CTreeIterator(const TBeginInfo &beginInfo);
%ignore ncbi::CTreeIterator::CTreeIterator(const TBeginInfo &beginInfo,
                                           const string &filter);


// Smart pointer, char* member in pointee, something bad happens
%ignore ncbi::blast::CBlast_Message;

// A weird thing where the generated Python code uses
// __swig_setmethods__, and tries to access that of the
// base class, which has no such member
%ignore ncbi::objects::CObjmgrUtilException;

// These public data members have private assignment operators,
// which swig tries to use in its "set" functions
%ignore ncbi::CObjectStreamCopier::m_ClassMemberHookKey;
%ignore ncbi::CObjectStreamCopier::m_ChoiceVariantHookKey;
%ignore ncbi::CObjectStreamCopier::m_ObjectHookKey;
%ignore ncbi::CObjectOStream::m_ObjectHookKey;
%ignore ncbi::CObjectOStream::m_ChoiceVariantHookKey;
%ignore ncbi::CObjectOStream::m_ClassMemberHookKey;
%ignore ncbi::CObjectIStream::m_ObjectHookKey;
%ignore ncbi::CObjectIStream::m_ChoiceVariantHookKey;
%ignore ncbi::CObjectIStream::m_ClassMemberHookKey;
%ignore ncbi::CObjectIStream::m_ObjectSkipHookKey;
%ignore ncbi::CObjectIStream::m_ChoiceVariantSkipHookKey;
%ignore ncbi::CObjectIStream::m_ClassMemberSkipHookKey;

// These somehow use undefined stuff
%ignore ncbi::CObjectTypeInfo::BeginVariants;
%ignore ncbi::CObjectTypeInfo::FindVariant;
%ignore ncbi::CObjectTypeInfo::FindVariantByTag;
%ignore ncbi::CObjectTypeInfo::GetVariantIterator;
%ignore ncbi::CSerializable::Dump;

// private new and delete, which swig tries to use
%ignore ncbi::CDelayBuffer::CDelayBuffer();
%ignore ncbi::CDiagRestorer::CDiagRestorer();
%ignore ncbi::CDiagRestorer::~CDiagRestorer;
%ignore ncbi::COStreamFrame::COStreamFrame;
%ignore ncbi::COStreamFrame::~COStreamFrame;
%ignore ncbi::COStreamClassMember::COStreamClassMember;
%ignore ncbi::COStreamClassMember::~COStreamClassMember;
%ignore ncbi::COStreamContainer::COStreamContainer;
%ignore ncbi::COStreamContainer::~COStreamContainer;
%ignore ncbi::CIStreamFrame::CIStreamFrame;
%ignore ncbi::CIStreamFrame::~CIStreamFrame;
%ignore ncbi::CIStreamClassMemberIterator::CIStreamClassMemberIterator;
%ignore ncbi::CIStreamClassMemberIterator::~CIStreamClassMemberIterator;
%ignore ncbi::CIStreamContainerIterator::CIStreamContainerIterator;
%ignore ncbi::CIStreamContainerIterator::~CIStreamContainerIterator;

// default argument is expression involving member constants
%ignore ncbi::CRotatingLogStream::CRotatingLogStream;

// swig not figuring out how to qualify "TPool"
%ignore ncbi::CStdThreadInPool::CStdThreadInPool;
// inherited type
%ignore ncbi::CStlOneArgTemplate::SetMemFunctions;

// function reference/pointer weirdness
%ignore ncbi::CObjectOStream::operator<<;

// swig bug involving smart pointers and char * data members
%ignore ncbi::blast::CQuerySetUpOptions;
%ignore ncbi::blast::CLookupTableOptions;
%ignore ncbi::blast::CBlastScoreBlk;
%ignore ncbi::blast::CBlastScoringOptions;

// some weird thing
%ignore ncbi::blast::CDbBlast::GetQueryInfo;

// some funny problem with __swig_set_methods__
%ignore ncbi::blast::CBlastException;

#ifdef SWIGPYTHON
// Python keywords
%ignore ncbi::objects::CSeqref::print;
%ignore ncbi::blast::CPSIMatrix::lambda;
%ignore *::lambda;
#endif

// uses someone's non-public operator=
%ignore ncbi::objects::SSeq_id_ScopeInfo::m_Bioseq_Info;
%ignore ncbi::objects::SSeq_id_ScopeInfo::m_AllAnnotRef_Info;

// uses someone's non-public constructor
%ignore ncbi::objects::CSeqMap_CI_SegmentInfo::x_GetSegment;
%ignore ncbi::objects::CSeqMap_CI_SegmentInfo::x_GetNextSegment;

// redeclared non-public in subclass
%ignore ncbi::objects::CBioseq_Base_Info::x_SetObjAnnot;
%ignore ncbi::objects::CBioseq_Base_Info::x_ResetObjAnnot;
%ignore ncbi::objects::CBioseq_Base_Info::x_ParentDetach;
%ignore ncbi::objects::CBioseq_Base_Info::x_ParentAttach;
%ignore ncbi::objects::CTSE_Info_Object::x_UpdateAnnotIndexContents;
%ignore ncbi::objects::CTSE_Info_Object::x_DoUpdate;

// uses someone's non-existent default constructor
%ignore ncbi::objects::CSeq_id::DumpAsFasta() const;

// global operators defined twice, and won't do much useful in Python anyway
%ignore ncbi::operator+;
%ignore ncbi::operator-;
%ignore ncbi::operator<;

// pub
// causes multiple definitions of __lshift__ in Python wrappers
%ignore ncbi::objects::operator<<(CNcbiOstream& os, EError_val error);
%ignore ncbi::objects::operator<<(IFlatItemOStream& out, const IFlatItem* i);

// bdb
// Public data, assignment operator not callable
%ignore ncbi::CBDB_FileCursor::From;
%ignore ncbi::CBDB_FileCursor::To;

// dbapi_driver
%ignore ncbi::DBAPI_RegisterDriver_MSDBLIB;
%ignore ncbi::DBAPI_RegisterDriver_MYSQL;
%ignore ncbi::DBAPI_RegisterDriver_ODBC;

// lds
// Public data, private assignment operator
%ignore ncbi::objects::SLDS_TablesCollection::file_db;
%ignore ncbi::objects::SLDS_TablesCollection::object_type_db;
%ignore ncbi::objects::SLDS_TablesCollection::object_db;
%ignore ncbi::objects::SLDS_TablesCollection::annot_db;
%ignore ncbi::objects::SLDS_TablesCollection::annot2obj_db;
%ignore ncbi::objects::SLDS_TablesCollection::seq_id_list;


// various
%ignore ncbi::CTreeNode::RemoveAllSubNodes;
%ignore TTreeNode::RemoveAllSubNodes;

// Inheritance from template leads to problems with
// inherited typedef (TThread)
%ignore ncbi::CStdPoolOfThreads::Register;
%ignore ncbi::CStdPoolOfThreads::UnRegister;

// Public data with private assignment operator
%ignore ncbi::objects::CStandaloneRequestResult::m_MutexPool;


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 22:23:13  jcherry
 * Initial version
 *
 * ===========================================================================
 */
