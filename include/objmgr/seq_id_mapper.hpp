#ifndef OBJECTS_OBJMGR___SEQ_ID_MAPPER__HPP
#define OBJECTS_OBJMGR___SEQ_ID_MAPPER__HPP

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
*   Seq-id mapper for Object Manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/01/23 21:59:32  grichenk
* Redesigned seq-id handles and mapper
*
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>
#include <objects/objmgr1/seq_id_handle.hpp>

#include <map>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_id;


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
    // Get seq-id handle. Create new handle if not found and
    // do_not_create is false. Get only the exactly equal seq-id handle.
    CSeq_id_Handle GetHandle(const CSeq_id& id, bool do_not_create = false);
    // Get the list of matching handles, do not create new handles
    void GetMatchingHandles(const CSeq_id& id, TSeq_id_HandleSet& h_set);
    // Get the list of string-matching handles, do not create new handles
    void GetMatchingHandlesStr(string sid, TSeq_id_HandleSet& h_set);
    // Get seq-id for the given handle
    static const CSeq_id& GetSeq_id(const CSeq_id_Handle& handle);

    // References to each handle must be tracked to re-use their values
    void AddHandleReference(const CSeq_id_Handle& handle);
    void ReleaseHandleReference(const CSeq_id_Handle& handle);

private:
    // Constructor available for CObjectManager only
    CSeq_id_Mapper(void);
    friend class CObjectManager;

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



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR___SEQ_ID_MAPPER__HPP */
