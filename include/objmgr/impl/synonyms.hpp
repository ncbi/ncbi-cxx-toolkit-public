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
struct SSeq_id_ScopeInfo;

////////////////////////////////////////////////////////////////////
//
//  CSynonymsSet::
//
//    Set of seq-id synonyms for CScope cache
//

class NCBI_XOBJMGR_EXPORT CSynonymsSet : public CObject
{
public:
    typedef pair<const CSeq_id_Handle, SSeq_id_ScopeInfo>* value_type;
    typedef vector<value_type>                             TIdSet;
    typedef TIdSet::const_iterator                         const_iterator;

    CSynonymsSet(void);
    ~CSynonymsSet(void);

    const_iterator begin(void) const;
    const_iterator end(void) const;
    bool empty(void) const;

    static CSeq_id_Handle GetSeq_id_Handle(const const_iterator& iter);
    static CBioseq_Handle GetBioseqHandle(const const_iterator& iter);

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

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.6  2004/07/12 15:05:31  grichenk
 * Moved seq-id mapper from xobjmgr to seq library
 *
 * Revision 1.5  2003/09/30 16:22:01  vasilche
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
 * Revision 1.4  2003/06/02 16:01:37  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.3  2003/05/12 19:18:28  vasilche
 * Fixed locking of object manager classes in multi-threaded application.
 *
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
