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
#include <objects/seq/seq_id_handle.hpp>
#include <vector>
#include <utility>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq_ScopeInfo;
class CBioseq_Handle;

////////////////////////////////////////////////////////////////////
//
//  CSynonymsSet::
//
//    Set of seq-id synonyms for CScope cache
//

class NCBI_XOBJMGR_EXPORT CSynonymsSet : public CObject
{
public:
    typedef CSeq_id_Handle                                 value_type;
    typedef vector<value_type>                             TIdSet;
    typedef TIdSet::const_iterator                         const_iterator;

    CSynonymsSet(void);
    ~CSynonymsSet(void);

    const_iterator begin(void) const;
    const_iterator end(void) const;
    bool empty(void) const;

    static CSeq_id_Handle GetSeq_id_Handle(const const_iterator& iter);

    void AddSynonym(const value_type& syn);
    bool ContainsSynonym(const CSeq_id_Handle& id) const;

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
CSynonymsSet::const_iterator CSynonymsSet::begin(void) const
{
    return m_IdSet.begin();
}


inline
CSynonymsSet::const_iterator CSynonymsSet::end(void) const
{
    return m_IdSet.end();
}


inline
bool CSynonymsSet::empty(void) const
{
    return m_IdSet.empty();
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR_IMPL___SYNONYMS__HPP */
