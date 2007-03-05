#ifndef BIOSEQ_CI__HPP
#define BIOSEQ_CI__HPP

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
*   Iterates over bioseqs from a given seq-entry and scope
*
*/


#include <corelib/ncbiobj.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <stack>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CScope;
class CBioseq_Handle;
class CSeq_entry;


/////////////////////////////////////////////////////////////////////////////
///
///  CBioseq_CI --
///
///  Enumerate bioseqs in a given seq-entry
///

class NCBI_XOBJMGR_EXPORT CBioseq_CI
{
public:
    /// Class of bioseqs to iterate
    enum EBioseqLevelFlag {
        eLevel_All,          ///< Any bioseq
        eLevel_Mains,        ///< Main bioseq only
        eLevel_Parts,        ///< Parts only
        eLevel_IgnoreClass   ///< Search for bioseqs in any bioseq-set
                             ///< regardless of types and classes.
    };

    // 'ctors
    CBioseq_CI(void);

    /// Create an iterator that enumerates bioseqs
    /// from the entry taken from the scope. Use optional
    /// filter to iterate over selected bioseq types only.
    /// Filter value eMol_na may be used to include both
    /// dna and rna bioseqs.
    CBioseq_CI(const CSeq_entry_Handle& entry,
               CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
               EBioseqLevelFlag level = eLevel_All);

    CBioseq_CI(const CBioseq_set_Handle& bioseq_set,
               CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
               EBioseqLevelFlag level = eLevel_All);

    /// Create an iterator that enumerates bioseqs
    /// from the entry taken from the given scope. Use optional
    /// filter to iterate over selected bioseq types only.
    /// Filter value eMol_na may be used to include both
    /// dna and rna bioseqs.
    CBioseq_CI(CScope& scope, const CSeq_entry& entry,
               CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
               EBioseqLevelFlag level = eLevel_All);

    CBioseq_CI(const CBioseq_CI& bioseq_ci);
    ~CBioseq_CI(void);

    /// Get the current scope for the iterator
    CScope& GetScope(void) const;

    CBioseq_CI& operator= (const CBioseq_CI& bioseq_ci);

    /// Move to the next object in iterated sequence
    CBioseq_CI& operator++ (void);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(m_CurrentBioseq);

    const CBioseq_Handle& operator* (void) const;
    const CBioseq_Handle* operator-> (void) const;

private:
    void x_Initialize(const CSeq_entry_Handle& entry);
    void x_SetEntry(const CSeq_entry_Handle& entry);
    void x_Settle(void);
    bool x_IsValidMolType(const CBioseq_Info& seq) const;
    bool x_SkipClass(CBioseq_set::TClass set_class);

    typedef stack<CSeq_entry_CI> TEntryStack;

    CHeapScope          m_Scope;
    CSeq_inst::EMol     m_Filter;
    EBioseqLevelFlag    m_Level;
    CSeq_entry_Handle   m_CurrentEntry;
    CBioseq_Handle      m_CurrentBioseq;
    TEntryStack         m_EntryStack;
    int                 m_InParts;
};


inline
const CBioseq_Handle& CBioseq_CI::operator* (void) const
{
    return m_CurrentBioseq;
}


inline
const CBioseq_Handle* CBioseq_CI::operator-> (void) const
{
    return &m_CurrentBioseq;
}


inline
CScope& CBioseq_CI::GetScope(void) const
{
    return m_Scope.GetScope();
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // BIOSEQ_CI__HPP
