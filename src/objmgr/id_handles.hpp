#ifndef OBJECTS_OBJMGR___HANDLES__HPP
#define OBJECTS_OBJMGR___HANDLES__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-id and Bioseq handles for Object Manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/16 15:51:34  grichenk
* Initial revision
*
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <map>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CTextseq_id;


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//
//    Handle to be used instead of CSeq_id. Supports different
//    methods of comparison: exact equality or match of seq-ids.
//


typedef size_t TSeq_id_Key;

class CSeq_id_Handle
{
public:
    // 'ctors
    CSeq_id_Handle(void); // Create an empty handle
    CSeq_id_Handle(const CSeq_id_Handle& handle);
    ~CSeq_id_Handle(void);

    CSeq_id_Handle& operator= (const CSeq_id_Handle& handle);
    bool operator== (const CSeq_id_Handle& handle) const;
    bool operator<  (const CSeq_id_Handle& handle) const;

    // Check if the handle is a valid or an empty one
    operator bool(void) const;

    // Reset the handle (remove seq-id reference)
    void Reset(void);

private:
    // This constructor should be used by mappers only
    CSeq_id_Handle(const CSeq_id& id, TSeq_id_Key key);

    // Comparison methods
    // True if handles are strictly equal
    bool x_Equal(const CSeq_id_Handle& handle) const;
    // True if "this" may be resolved to "handle"
    bool x_Match(const CSeq_id_Handle& handle) const;

    // Handle value
    TSeq_id_Key m_Value;
    // Reference to the seq-id used by a group of equal handles
    CRef<CSeq_id> m_SeqId;

    friend class CSeq_id_Mapper;
    friend class CSeq_id_Which_Tree;
};



////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Mapper::
//
//    Allows fast convertions between CSeq_id and CSeq_id_Handle,
//    including searching for multiple matches for a given seq-id.
//


typedef pair< CConstRef<CSeq_id>, TSeq_id_Key > TSeq_id_Info;
typedef set<CSeq_id_Handle>                     TSeq_id_HandleSet;


// forward declaration
class CSeq_id_Which_Tree;


const size_t kKeyUsageTableSize = 65536;


class CSeq_id_Mapper : public CObject
{
public:
    // 'ctors
    CSeq_id_Mapper(void);
    ~CSeq_id_Mapper(void);

    // Get seq-id handle. Create new handle if not found and
    // do_not_create is false. Get only the exactly equal seq-id handle.
    CSeq_id_Handle GetHandle(const CSeq_id& id, bool do_not_create = false);
    // Get the list of matching handles, do not create new handles
    void GetMatchingHandles(const CSeq_id& id, TSeq_id_HandleSet& h_set);
    // Get the list of string-matching handles, do not create new handles
    void GetMatchingHandlesStr(string sid, TSeq_id_HandleSet& h_set);
    // Get seq-id for the given handle
    const CSeq_id& GetSeq_id(const CSeq_id_Handle& handle) const;

    // References to each handle must be tracked to re-use their values
    void AddHandleReference(const CSeq_id_Handle& handle);
    void ReleaseHandleReference(const CSeq_id_Handle& handle);

private:
    // Hide copy constructor and operator
    CSeq_id_Mapper(const CSeq_id_Mapper&);
    CSeq_id_Mapper& operator= (const CSeq_id_Mapper&);

    // Get next available key for CSeq_id_Handle
    TSeq_id_Key GetNextKey(void);

    // Table for id reference counters. The keys are grouped by
    // kKeyUsageTableSize elements. When a group is completely
    // unreferenced, it may be deleted and its keys re-used.
    size_t m_KeyUsageTable[kKeyUsageTableSize];

    // Next available key value -- must be read through GetNextKey() only.
    TSeq_id_Key m_NextKey;

    // Some map entries may point to the same subtree (e.g. gb, dbj, emb).
    typedef map<CSeq_id::E_Choice, CRef<CSeq_id_Which_Tree> > TIdMap;
    TIdMap m_IdMap;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//

inline
CSeq_id_Handle::CSeq_id_Handle(void)
    : m_Value(0), m_SeqId(0)
{
}

inline
CSeq_id_Handle::CSeq_id_Handle(const CSeq_id_Handle& handle)
    : m_Value(handle.m_Value), m_SeqId(handle.m_SeqId)
{
}

inline
CSeq_id_Handle::~CSeq_id_Handle(void)
{
}

inline
CSeq_id_Handle& CSeq_id_Handle::operator= (const CSeq_id_Handle& handle)
{
    m_Value = handle.m_Value;
    m_SeqId = handle.m_SeqId;
    return *this;
}

inline
bool CSeq_id_Handle::operator== (const CSeq_id_Handle& handle) const
{
    return m_Value == handle.m_Value;
}

inline
bool CSeq_id_Handle::operator< (const CSeq_id_Handle& handle) const
{
    return m_Value < handle.m_Value;
}

inline
CSeq_id_Handle::operator bool (void) const
{
    return !m_SeqId.Empty();
}

inline
void CSeq_id_Handle::Reset(void)
{
    m_Value = 0;
    m_SeqId.Reset();
}



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR___HANDLES__HPP */
