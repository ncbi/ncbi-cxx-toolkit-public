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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-id mapper for Object Manager
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <objects/seq/seq_id_handle.hpp>

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup OBJECTS_Seqid
 *
 * @{
 */


class CSeq_id;
class CSeq_id_Which_Tree;


/////////////////////////////////////////////////////////////////////
///
///  CSeq_id_Mapper::
///
///    Allows fast convertions between CSeq_id and CSeq_id_Handle,
///    including searching for multiple matches for a given seq-id.
///


typedef set<CSeq_id_Handle>                     TSeq_id_HandleSet;


class NCBI_SEQ_EXPORT CSeq_id_Mapper : public CObject
{
public:
    static CRef<CSeq_id_Mapper> GetInstance(void);
    
    virtual ~CSeq_id_Mapper(void);
    
    /// Get seq-id handle. Create new handle if not found and
    /// do_not_create is false. Get only the exactly equal seq-id handle.
    CSeq_id_Handle GetGiHandle(TGi gi);
    CSeq_id_Handle GetHandle(const CSeq_id& id, bool do_not_create = false);

    /// Get the list of matching handles, do not create new handles
    bool HaveMatchingHandles(const CSeq_id_Handle& id);
    void GetMatchingHandles(const CSeq_id_Handle& id,
                            TSeq_id_HandleSet& h_set);
    bool HaveReverseMatch(const CSeq_id_Handle& id);
    void GetReverseMatchingHandles(const CSeq_id_Handle& id,
                                   TSeq_id_HandleSet& h_set);
    bool HaveMatchingHandles(const CSeq_id_Handle& id,
                             EAllowWeakMatch allow_weak_match);
    void GetMatchingHandles(const CSeq_id_Handle& id,
                            TSeq_id_HandleSet& h_set,
                            EAllowWeakMatch allow_weak_match);
    bool HaveReverseMatch(const CSeq_id_Handle& id,
                          EAllowWeakMatch allow_weak_match);
    void GetReverseMatchingHandles(const CSeq_id_Handle& id,
                                   TSeq_id_HandleSet& h_set,
                                   EAllowWeakMatch allow_weak_match);
    /// Get the list of string-matching handles, do not create new handles
    void GetMatchingHandlesStr(string sid,
                               TSeq_id_HandleSet& h_set);
    
    /// Get seq-id for the given handle
    static CConstRef<CSeq_id> GetSeq_id(const CSeq_id_Handle& handle);

    enum EDumpDetails {
        eCountTotalBytes, // no output, only return estimated memory
        eDumpTotalBytes,  // print and return estimated memory
        eDumpStatistics,  // print estimated memory for various Seq-id types
        eDumpAllIds       // print all mapped Seq-ids
    };
    size_t Dump(CNcbiOstream& out,
                EDumpDetails details = eDumpTotalBytes) const;
    
private:
    CSeq_id_Mapper(void);
    
    friend class CSeq_id_Handle;
    friend class CSeq_id_Info;

    // References to each handle must be tracked to re-use their values
    // Each CSeq_id_Handle locks itself in the constructor and
    // releases in the destructor.


    bool x_Match(const CSeq_id_Handle& h1, const CSeq_id_Handle& h2);
    bool x_IsBetter(const CSeq_id_Handle& h1, const CSeq_id_Handle& h2);


    CSeq_id_Which_Tree& x_GetTree(CSeq_id::E_Choice type);
    CSeq_id_Which_Tree& x_GetTree(const CSeq_id& id);
    CSeq_id_Which_Tree& x_GetTree(const CSeq_id_Handle& idh);

    // Hide copy constructor and operator
    CSeq_id_Mapper(const CSeq_id_Mapper&);
    CSeq_id_Mapper& operator= (const CSeq_id_Mapper&);

    // Some map entries may point to the same subtree (e.g. gb, dbj, emb).
    typedef vector<CRef<CSeq_id_Which_Tree> >                 TTrees;

    TTrees          m_Trees;
    mutable CMutex  m_IdMapMutex;
};


/////////////////////////////////////////////////////////////////////////////
//
// Inline methods
//
/////////////////////////////////////////////////////////////////////////////


inline
CConstRef<CSeq_id> CSeq_id_Mapper::GetSeq_id(const CSeq_id_Handle& h)
{
    return h.GetSeqId();
}


inline
CSeq_id_Which_Tree& CSeq_id_Mapper::x_GetTree(CSeq_id::E_Choice type)
{
    _ASSERT(size_t(type) < m_Trees.size());
    return *m_Trees[type];
}


inline
CSeq_id_Which_Tree& CSeq_id_Mapper::x_GetTree(const CSeq_id& seq_id)
{
    return x_GetTree(seq_id.Which());
}

/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR___SEQ_ID_MAPPER__HPP */
