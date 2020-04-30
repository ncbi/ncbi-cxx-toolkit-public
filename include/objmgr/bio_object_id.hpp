#ifndef OBJECTS_OBJMGR_IMPL___BIOOBJID__HPP
#define OBJECTS_OBJMGR_IMPL___BIOOBJID__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <corelib/ncbiobj.hpp>

#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioObjectId
{
public:
    enum EType {
        eSeqId,
        eSetId,
        eUniqNumber,
        eUnSet
    };
    CBioObjectId() : m_Id(eUnSet, CSeq_id_Handle()) {}
    explicit CBioObjectId(const CSeq_id_Handle& id) : m_Id(eSeqId,id) {}
    CBioObjectId(EType type, TIntId id)
        : m_Id(type, CSeq_id_Handle::GetGiHandle(GI_FROM(TIntId, id))) 
    {
        _ASSERT(type == eSeqId || type == eSetId || type == eUniqNumber);
    }

    const CSeq_id_Handle& GetSeqId() const { 
        _ASSERT(m_Id.first == eSeqId); return m_Id.second; 
    }

    TIntId GetSetId() const  { 
        _ASSERT(m_Id.first == eSetId); return GI_TO(TIntId, m_Id.second.GetGi());
    }
    
    TIntId GetUniqNumber() const  { 
        _ASSERT(m_Id.first == eUniqNumber); return GI_TO(TIntId, m_Id.second.GetGi());
    }
    
    bool operator == (const CBioObjectId& other) const {
        return m_Id == other.m_Id;
    }
    bool operator != (const CBioObjectId& other) const {
        return m_Id != other.m_Id;
    }
    bool operator < (const CBioObjectId& other) const {
        return m_Id < other.m_Id; 
    }

    EType GetType() const { return m_Id.first; }
    const CSeq_id_Handle& x_GetSeqIdNoCheck() const { return m_Id.second; }

private:
    pair<EType, CSeq_id_Handle> m_Id;
};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___BIOOBJID__HPP
