#ifndef OBJECTS_OBJMGR_IMPL___SYNONYMS__HPP
#define OBJECTS_OBJMGR_IMPL___SYNONYMS__HPP

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
 *   Set of seq-id synonyms for CScope cache
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objects/objmgr/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  CSynonymsSet::
//
//    Set of seq-id synonyms for CScope cache
//

class CSynonymsSet : public CObject
{
public:
    typedef set<CSeq_id_Handle>    TIdSet;
    typedef TIdSet::iterator       iterator;
    typedef TIdSet::const_iterator const_iterator;

    CSynonymsSet(void) {}
    ~CSynonymsSet(void) {}

    iterator begin(void);
    const_iterator begin(void) const;
    iterator end(void);
    const_iterator end(void) const;
    iterator find(const CSeq_id_Handle& id);
    const_iterator find(const CSeq_id_Handle& id) const;
    bool empty(void) const;
    size_t size(void) const;

    void AddSynonym(const CSeq_id_Handle& id);
    void RemoveSynonym(const CSeq_id_Handle& id);
    bool ContainsSynonym(const CSeq_id_Handle& id);

private:
    // Prohibit copy functions
    CSynonymsSet(const CSynonymsSet&);
    CSynonymsSet& operator=(const CSynonymsSet&);

    TIdSet m_IdSet;
};

/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////

inline
CSynonymsSet::iterator CSynonymsSet::begin(void)
{
    return m_IdSet.begin();
}

inline
CSynonymsSet::const_iterator CSynonymsSet::begin(void) const
{
    return m_IdSet.begin();
}

inline
CSynonymsSet::iterator CSynonymsSet::end(void)
{
    return m_IdSet.end();
}

inline
CSynonymsSet::const_iterator CSynonymsSet::end(void) const
{
    return m_IdSet.end();
}

inline
CSynonymsSet::iterator CSynonymsSet::find(const CSeq_id_Handle& id)
{
    return m_IdSet.find(id);
}

inline
CSynonymsSet::const_iterator CSynonymsSet::find
    (const CSeq_id_Handle& id) const
{
    return m_IdSet.find(id);
}

inline
size_t CSynonymsSet::size(void) const
{
    return m_IdSet.size();
}

inline
bool CSynonymsSet::empty(void) const
{
    return m_IdSet.empty();
}

inline
void CSynonymsSet::AddSynonym(const CSeq_id_Handle& id)
{
    m_IdSet.insert(id);
}

inline
void CSynonymsSet::RemoveSynonym(const CSeq_id_Handle& id)
{
    m_IdSet.erase(id);
}

inline
bool CSynonymsSet::ContainsSynonym(const CSeq_id_Handle& id)
{
    return find(id) != end();
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.2  2003/02/28 21:54:16  grichenk
 * +CSynonymsSet::empty(), removed _ASSERT() in CScope::GetSynonyms()
 *
 * Revision 1.1  2003/02/28 20:02:03  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif  /* OBJECTS_OBJMGR_IMPL___SYNONYMS__HPP */
