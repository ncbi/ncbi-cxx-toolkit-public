#ifndef OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP
#define OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-id handle for Object Manager
*
*/


#include <objects/seqloc/Seq_id.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//
//    Handle to be used instead of CSeq_id. Supports different
//    methods of comparison: exact equality or match of seq-ids.
//

// forward declaration
class CSeq_id;
class CSeq_id_Handle;
class CSeq_id_Mapper;
class CSeq_id_Which_Tree;


class NCBI_XOBJMGR_EXPORT CSeq_id_Info : public CObject
{
public:
    explicit CSeq_id_Info(CSeq_id::E_Choice type);
    explicit CSeq_id_Info(const CConstRef<CSeq_id>& seq_id);
    ~CSeq_id_Info(void);

    CConstRef<CSeq_id> GetSeqId(void) const
        {
            return m_Seq_id;
        }
    CConstRef<CSeq_id> GetGiSeqId(int gi) const;

    void AddLock(void) const;
    void RemoveLock(void) const;

    int GetLockCounter(void) const;

    CSeq_id::E_Choice GetType(void) const;
    CSeq_id_Mapper& GetMapper(void) const;
    CSeq_id_Which_Tree& GetTree(void) const;

private:
    CSeq_id_Info(const CSeq_id_Info&);
    const CSeq_id_Info& operator=(const CSeq_id_Info&);

    void x_RemoveLastLock(void) const;

    mutable CAtomicCounter       m_LockCounter;
    CSeq_id::E_Choice            m_Seq_id_Type;
    CConstRef<CSeq_id>           m_Seq_id;
    mutable CRef<CSeq_id_Mapper> m_Mapper;
};


class NCBI_XOBJMGR_EXPORT CSeq_id_Handle
{
public:
    // 'ctors
    CSeq_id_Handle(void);
    ~CSeq_id_Handle(void);
    explicit CSeq_id_Handle(const CSeq_id_Info* info, int gi = 0);

    CSeq_id_Handle(const CSeq_id_Handle& handle);
    CSeq_id_Handle& operator=(const CSeq_id_Handle& handle);

    static CSeq_id_Handle GetGiHandle(int gi);
    static CSeq_id_Handle GetHandle(const CSeq_id& id);

    bool operator== (const CSeq_id_Handle& handle) const;
    bool operator!= (const CSeq_id_Handle& handle) const;
    bool operator<  (const CSeq_id_Handle& handle) const;
    bool operator== (const CSeq_id& id) const;

    // Check if the handle is a valid or an empty one
    operator bool(void) const;
    bool operator! (void) const;

    // Reset the handle (remove seq-id reference)
    void Reset(void);

    // True if "this" is a better bioseq than "h".
    bool IsBetter(const CSeq_id_Handle& h) const;

    string AsString(void) const;
    bool IsGi(void) const;
    int GetGi(void) const;
    unsigned GetHash(void) const;

    CConstRef<CSeq_id> GetSeqId(void) const;
    CConstRef<CSeq_id> GetSeqIdOrNull(void) const;

    static void DumpRegister(const char* msg);

private:
    void x_Register(void);
    void x_Deregister(void);
    
    friend class CSeq_id_Mapper;

    // Comparison methods
    // True if handles are strictly equal
    bool x_Equal(const CSeq_id_Handle& handle) const;
    // True if "this" may be resolved to "handle"
    bool x_Match(const CSeq_id_Handle& handle) const;

    // Seq-id info
    CConstRef<CSeq_id_Info>    m_Info;
    int                        m_Gi;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Info
/////////////////////////////////////////////////////////////////////////////


inline
void CSeq_id_Info::AddLock(void) const
{
    _VERIFY(m_LockCounter.Add(1) > 0);
}


inline
void CSeq_id_Info::RemoveLock(void) const
{
    if ( m_LockCounter.Add(-1) <= 0 ) {
        x_RemoveLastLock();
    }
}


inline
int CSeq_id_Info::GetLockCounter(void) const
{
    int counter = m_LockCounter.Get();
    _ASSERT(counter >= 0);
    return counter;
}


inline
CSeq_id::E_Choice CSeq_id_Info::GetType(void) const
{
    return m_Seq_id_Type;
}


inline
CSeq_id_Mapper& CSeq_id_Info::GetMapper(void) const
{
    return *m_Mapper;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Handle
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_id_Handle::operator bool (void) const
{
    return m_Info;
}


inline
bool CSeq_id_Handle::operator!(void) const
{
    return !m_Info;
}


inline
CSeq_id_Handle::CSeq_id_Handle(void)
    : m_Info(0), m_Gi(0)
{
}


inline
CSeq_id_Handle::CSeq_id_Handle(const CSeq_id_Info* info, int gi)
    : m_Info(info), m_Gi(gi)
{
    m_Info->AddLock();
    _ASSERT(info);
}


inline
CSeq_id_Handle::~CSeq_id_Handle(void)
{
    if ( m_Info ) {
        m_Info->RemoveLock();
    }
}


inline
CSeq_id_Handle::CSeq_id_Handle(const CSeq_id_Handle& handle)
{
    m_Info = handle.m_Info;
    m_Gi = handle.m_Gi;
    if ( m_Info ) {
        m_Info->AddLock();
    }
}


inline
CSeq_id_Handle& CSeq_id_Handle::operator=(const CSeq_id_Handle& handle)
{
    if (this != &handle) {
        if ( m_Info ) {
            m_Info->RemoveLock();
        }
        m_Info = handle.m_Info;
        m_Gi = handle.m_Gi;
        if ( m_Info ) {
            m_Info->AddLock();
        }
    }
    return *this;
}


inline
void CSeq_id_Handle::Reset(void)
{
    if ( m_Info ) {
        m_Info->RemoveLock();
    }
    m_Info.Reset();
    m_Gi = 0;
}


inline
bool CSeq_id_Handle::operator==(const CSeq_id_Handle& handle) const
{
    return m_Gi == handle.m_Gi && m_Info == handle.m_Info;
}


inline
bool CSeq_id_Handle::operator!=(const CSeq_id_Handle& handle) const
{
    return m_Gi != handle.m_Gi || m_Info != handle.m_Info;
}


inline
bool CSeq_id_Handle::operator<(const CSeq_id_Handle& handle) const
{
    return m_Gi < handle.m_Gi ||
        m_Gi == handle.m_Gi && m_Info < handle.m_Info;
}


inline
bool CSeq_id_Handle::IsGi(void) const
{
    return m_Gi != 0;
}


inline
int CSeq_id_Handle::GetGi(void) const
{
    return m_Gi;
}


inline
unsigned CSeq_id_Handle::GetHash(void) const
{
    unsigned hash = m_Gi;
    if ( !hash ) {
        hash = unsigned((unsigned long)(m_Info)>>3);
    }
    return hash;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  2004/06/17 18:28:38  vasilche
* Fixed null pointer exception in GI CSeq_id_Handle.
*
* Revision 1.24  2004/06/16 19:21:56  grichenk
* Fixed locking of CSeq_id_Info
*
* Revision 1.23  2004/06/14 13:57:09  grichenk
* CSeq_id_Info locks CSeq_id_Mapper with a CRef
*
* Revision 1.22  2004/02/19 17:25:33  vasilche
* Use CRef<> to safely hold pointer to CSeq_id_Info.
* CSeq_id_Info holds pointer to owner CSeq_id_Which_Tree.
* Reduce number of calls to CSeq_id_Handle.GetSeqId().
*
* Revision 1.21  2004/01/22 20:10:38  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.20  2004/01/07 20:42:00  grichenk
* Fixed matching of accession to accession.version
*
* Revision 1.19  2003/11/26 17:55:54  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.18  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.17  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.16  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.15  2003/06/19 18:23:44  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.14  2003/06/10 19:06:34  vasilche
* Simplified CSeq_id_Mapper and CSeq_id_Handle.
*
* Revision 1.13  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.12  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.11  2003/02/21 14:33:50  grichenk
* Display warning but don't crash on uninitialized seq-ids.
*
* Revision 1.10  2002/12/26 20:44:02  dicuccio
* Added Win32 export specifier.  Added #include for seq_id_mapper - at bottom of
* file to skirt circular include dependency.
*
* Revision 1.9  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/29 21:19:57  gouriano
* added debug dump
*
* Revision 1.7  2002/05/02 20:42:35  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.6  2002/03/18 17:26:32  grichenk
* +CDataLoader::x_GetSeq_id(), x_GetSeq_id_Key(), x_GetSeq_id_Handle()
*
* Revision 1.5  2002/03/15 18:10:05  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.4  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/02/12 19:41:40  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.2  2002/01/29 17:06:12  grichenk
* + operator !()
*
* Revision 1.1  2002/01/23 21:56:35  grichenk
* Splitted id_handles.hpp
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP */
