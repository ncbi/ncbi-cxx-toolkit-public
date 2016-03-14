#ifndef OBJMGR_IMPL_SEQ_ID_SORT__HPP
#define OBJMGR_IMPL_SEQ_ID_SORT__HPP

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
* Author:
*           Aleksey Grichenko
*
* File Description:
*           API for sorting seq-ids to improve the performance of data loaders.
*
*/

#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CSortableSeq_id
/////////////////////////////////////////////////////////////////////////////


class NCBI_XOBJMGR_EXPORT CSortableSeq_id : public CObject
{
public:
    CSortableSeq_id(const CSeq_id_Handle& idh, size_t idx);

    bool operator<(const CSortableSeq_id& id) const;

    const CSeq_id_Handle& GetId(void) const { return m_Id; }
    size_t GetIndex(void) const { return m_Index; }

private:
    CSortableSeq_id(const CSortableSeq_id&);
    CSortableSeq_id& operator=(const CSortableSeq_id&);

    void x_ParseParts(const string& s);

    struct SIdPart
    {
        SIdPart(Uint8 i)
            : m_IsInt(true), m_Int(i) {}

        SIdPart(const string& value)
            : m_IsInt(false), m_Int(0)
        {
            for (size_t p = 0; p < value.size(); ++p) {
                char c = value[p];
                if (c < '0' || c > '9') {
                    m_Str = value;
                    return;
                }
                m_Int = m_Int*10 + c - '0';
            }
            m_IsInt = true;
        }

        bool m_IsInt;
        string m_Str;
        Uint8 m_Int;
    };

    typedef vector<SIdPart> TIdParts;

    CSeq_id_Handle m_Id;
    size_t m_Index;
    TIdParts m_Parts;
};


inline
bool operator<(const CRef<CSortableSeq_id>& id1, const CRef<CSortableSeq_id>& id2)
{
    return *id1 < *id2;
}


class NCBI_XOBJMGR_EXPORT CSortedSeq_ids
{
public:
    typedef vector<CSeq_id_Handle> TIds;

    CSortedSeq_ids(const TIds& ids);

    void GetSortedIds(TIds& ids) const;

    template<class TValue> void RestoreOrder(vector<TValue>& values) const
    {
        _ASSERT(values.size() == m_SortedIds.size());
        vector<TValue> tmp = values;
        for (size_t i = 0; i < m_SortedIds.size(); ++i) {
            values[m_SortedIds[i]->GetIndex()] = tmp[i];
        }
    }

private:
    typedef vector< CRef<CSortableSeq_id> > TSortedIds;

    TSortedIds  m_SortedIds;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJMGR_IMPL_SEQ_ID_SORT__HPP
