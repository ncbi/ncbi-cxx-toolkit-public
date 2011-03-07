#ifndef NCBI_OBJMGR_SPLIT_PLACE_ID__HPP
#define NCBI_OBJMGR_SPLIT_PLACE_ID__HPP

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
* Author:  Eugene Vasilchenko
*
* File Description:
*   CPlaceId class to specify the place of split data in original blob.
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CPlaceId
{
public:
    typedef CSeq_id_Handle TBioseqId;
    typedef int TBioseq_setId;

    CPlaceId(void)
        : m_Bioseq_setId(0)
        {
        }
    explicit CPlaceId(const TBioseqId& id)
        : m_Bioseq_setId(0), m_BioseqId(id)
        {
        }
    explicit CPlaceId(TBioseq_setId id)
        : m_Bioseq_setId(id)
        {
        }

    bool IsNull(void) const
        {
            return !IsBioseq_set() && !IsBioseq();
        }
    bool IsBioseq(void) const
        {
            return m_BioseqId;
        }
    bool IsBioseq_set(void) const
        {
            return m_Bioseq_setId != 0;
        }

    const TBioseqId& GetBioseqId(void) const
        {
            _ASSERT(IsBioseq());
            return m_BioseqId;
        }
    TBioseq_setId GetBioseq_setId(void) const
        {
            _ASSERT(IsBioseq_set());
            return m_Bioseq_setId;
        }
    
    bool operator<(const CPlaceId& id) const
        {
            if ( m_Bioseq_setId != id.m_Bioseq_setId ) {
                return m_Bioseq_setId < id.m_Bioseq_setId;
            }
            return m_BioseqId < id.m_BioseqId;
        }
    bool operator==(const CPlaceId& id) const
        {
            return m_Bioseq_setId == id.m_Bioseq_setId &&
                m_BioseqId == id.m_BioseqId;
        }
    bool operator!=(const CPlaceId& id) const
        {
            return !(*this == id);
        }

private:
    TBioseq_setId   m_Bioseq_setId;
    TBioseqId       m_BioseqId;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//NCBI_OBJMGR_SPLIT_PLACE_ID__HPP
