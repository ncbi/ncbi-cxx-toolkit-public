#ifndef SEQ_ID_MAPPER_REAL__HPP
#define SEQ_ID_MAPPER_REAL__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/11 19:06:23  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Global Seq-id mapper: convert seq-id to an integer value

class CSeqIdMapper_Real
{
public:
    typedef CBioseqHandle::THandle TKey;
    
    CSeqIdMapper_Real() : m_NextHandle(1) {};
    ~CSeqIdMapper_Real();
    
    // Get key for the seq-id (create a new key if not created yet)
    TKey SeqIdToHandle(const CSeq_id& id);
    // Reverse lookup of seq-id
    CSeq_id* HandleToSeqId(TKey key) const ;
    
private:
    typedef map<string, TKey> TIdMap;
    mutable CFastMutex m_Mutex;
    // Global seq-id to key map
    TIdMap     m_IdMap;
    // Next unused key value
    TKey       m_NextHandle;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_ID_MAPPER_REAL__HPP
