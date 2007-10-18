#ifndef OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP
#define OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP

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
*   Seq-id handle for Object Manager
*
*/


#include <objects/seqloc/Seq_id.hpp>

#include <set>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup OBJECTS_Seqid
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////
///
///  CSeq_id_Handle::
///
///    Handle to be used instead of CSeq_id. Supports different
///    methods of comparison: exact equality or match of seq-ids.
///

// forward declaration
class CSeq_id;
class CSeq_id_Handle;
class CSeq_id_Mapper;
class CSeq_id_Which_Tree;


class CSeq_id_Info : public CObject
{
public:
    NCBI_SEQ_EXPORT CSeq_id_Info(CSeq_id::E_Choice type,
                                 CSeq_id_Mapper* mapper);
    NCBI_SEQ_EXPORT CSeq_id_Info(const CConstRef<CSeq_id>& seq_id,
                                 CSeq_id_Mapper* mapper);
    NCBI_SEQ_EXPORT ~CSeq_id_Info(void);

    CConstRef<CSeq_id> GetSeqId(void) const
        {
            return m_Seq_id;
        }
    NCBI_SEQ_EXPORT CConstRef<CSeq_id> GetGiSeqId(int gi) const;

    // locking
    void AddLock(void) const
        {
            m_LockCounter.Add(1);
        }
    void RemoveLock(void) const
        {
            if ( m_LockCounter.Add(-1) <= 0 ) {
                x_RemoveLastLock();
            }
        }
    int GetLockCounter(void) const
        {
            return m_LockCounter.Get();
        }

    CSeq_id::E_Choice GetType(void) const
        {
            return m_Seq_id_Type;
        }
    CSeq_id_Mapper& GetMapper(void) const
        {
            return *m_Mapper;
        }
    NCBI_SEQ_EXPORT CSeq_id_Which_Tree& GetTree(void) const;

private:
    friend class CSeq_id_Which_Tree;

    NCBI_SEQ_EXPORT void x_RemoveLastLock(void) const;

    mutable CAtomicCounter       m_LockCounter;
    CSeq_id::E_Choice            m_Seq_id_Type;
    CConstRef<CSeq_id>           m_Seq_id;
    mutable CRef<CSeq_id_Mapper> m_Mapper;

private:
    // to prevent copying
    CSeq_id_Info(const CSeq_id_Info&);
    const CSeq_id_Info& operator=(const CSeq_id_Info&);
};


class CSeq_id_InfoLocker : protected CObjectCounterLocker
{
public:
    void Lock(const CSeq_id_Info* info) const
        {
            CObjectCounterLocker::Lock(info);
            info->AddLock();
        }
    void Relock(const CSeq_id_Info* info) const
        {
            Lock(info);
        }
    void Unlock(const CSeq_id_Info* info) const
        {
            info->RemoveLock();
            CObjectCounterLocker::Unlock(info);
        }
};


class CSeq_id_Handle
{
public:
    // 'ctors
    CSeq_id_Handle(void)
        : m_Info(null), m_Gi(0)
        {
        }
    explicit CSeq_id_Handle(const CSeq_id_Info* info, int gi = 0)
        : m_Info(info), m_Gi(gi)
        {
            _ASSERT(info);
        }

    /// Faster way to create a handle for a gi.
    static NCBI_SEQ_EXPORT CSeq_id_Handle GetGiHandle(int gi);

    /// Normal way of getting a handle, works for any seq-id.
    static NCBI_SEQ_EXPORT CSeq_id_Handle GetHandle(const CSeq_id& id);

    bool operator== (const CSeq_id_Handle& handle) const
        {
            return m_Gi == handle.m_Gi && m_Info == handle.m_Info;
        }
    bool operator!= (const CSeq_id_Handle& handle) const
        {
            return m_Gi != handle.m_Gi || m_Info != handle.m_Info;
        }
    bool operator<  (const CSeq_id_Handle& handle) const
        {
            return m_Gi > handle.m_Gi || // gi != 0 first
                m_Gi == handle.m_Gi && m_Info < handle.m_Info;
        }
    bool NCBI_SEQ_EXPORT operator== (const CSeq_id& id) const;

    /// Check if the handle is a valid or an empty one
    DECLARE_OPERATOR_BOOL_REF(m_Info);

    /// Reset the handle (remove seq-id reference)
    void Reset(void)
        {
            m_Info.Reset();
            m_Gi = 0;
        }

    //
    bool NCBI_SEQ_EXPORT HaveMatchingHandles(void) const;
    bool NCBI_SEQ_EXPORT HaveReverseMatch(void) const;

    //
    typedef set<CSeq_id_Handle> TMatches;
    void NCBI_SEQ_EXPORT GetMatchingHandles(TMatches& matches) const;
    void NCBI_SEQ_EXPORT GetReverseMatchingHandles(TMatches& matches) const;

    /// True if *this matches to h.
    /// This mean that *this is either the same as h,
    /// or more generic version of h.
    bool NCBI_SEQ_EXPORT MatchesTo(const CSeq_id_Handle& h) const;

    /// True if "this" is a better bioseq than "h".
    bool NCBI_SEQ_EXPORT IsBetter(const CSeq_id_Handle& h) const;

    string NCBI_SEQ_EXPORT AsString(void) const;

    bool IsGi(void) const
        {
            return m_Gi != 0;
        }
    int GetGi(void) const
        {
            return m_Gi;
        }
    unsigned NCBI_SEQ_EXPORT GetHash(void) const;

    CSeq_id::E_Choice Which(void) const
        {
            return m_Info->GetType();
        }
    CSeq_id::EAccessionInfo IdentifyAccession(void) const
        {
            return m_Info->GetSeqId()->IdentifyAccession();
        }

    CConstRef<CSeq_id> NCBI_SEQ_EXPORT GetSeqId(void) const;
    CConstRef<CSeq_id> NCBI_SEQ_EXPORT GetSeqIdOrNull(void) const;

    CSeq_id_Mapper& GetMapper(void) const
        {
            return m_Info->GetMapper();
        }

    void Swap(CSeq_id_Handle& idh)
        {
            m_Info.Swap(idh.m_Info);
            swap(m_Gi, idh.m_Gi);
        }

private:
    friend class CSeq_id_Mapper;

    // Seq-id info
    CConstRef<CSeq_id_Info, CSeq_id_InfoLocker> m_Info;
    int m_Gi;
};

/// Get CConstRef<CSeq_id> from a seq-id handle (for container
/// searching template functions)
template<>
inline
CConstRef<CSeq_id> Get_ConstRef_Seq_id(const CSeq_id_Handle& id)
{
    return id.GetSeqId();
}


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////

/* @} */


/// Return best label for a sequence from single Seq-id, or set of Seq-ids.
/// Return empty string if the label cannot be determined.
/// GetDirectLabel() will return non-empty string only if the Seq-id is
/// very likely enough to get good label without loading full set of
/// sequence Seq-ids.
NCBI_SEQ_EXPORT string GetDirectLabel(const CSeq_id& id);
NCBI_SEQ_EXPORT string GetDirectLabel(const CSeq_id_Handle& id);
NCBI_SEQ_EXPORT string GetLabel(const CSeq_id& id);
NCBI_SEQ_EXPORT string GetLabel(const CSeq_id_Handle& id);
NCBI_SEQ_EXPORT string GetLabel(const vector<CSeq_id_Handle>& ids);
NCBI_SEQ_EXPORT string GetLabel(const vector<CRef<CSeq_id> >& ids);


END_SCOPE(objects)
END_NCBI_SCOPE

BEGIN_STD_SCOPE
inline 
void swap(NCBI_NS_NCBI::objects::CSeq_id_Handle& idh1,
          NCBI_NS_NCBI::objects::CSeq_id_Handle& idh2)
{
    idh1.Swap(idh2);
}

END_STD_SCOPE

#endif  /* OBJECTS_OBJMGR___SEQ_ID_HANDLE__HPP */
