#ifndef SEQ_ANNOT_CI__HPP
#define SEQ_ANNOT_CI__HPP

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
*   Seq-annot iterator
*
*/


#include <corelib/ncbiobj.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>

#include <vector>
#include <stack>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CSeq_annot_Handle;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CBioseq_set_Info;
class CBioseq_Handle;
class CBioseq_set_Handle;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_annot_CI --
///
///  enumerate CSeq_annot objects - packs of annotations 
///  (features, graphs, alignments etc.)
///

class NCBI_XOBJMGR_EXPORT CSeq_annot_CI
{
public:
    enum EFlags {
        eSearch_entry,      //< Search only in this entry
        eSearch_recursive   //< Search recursively
    };

    /// Create an empty iterator
    CSeq_annot_CI(void);

    /// Create an iterator that enumerates CSeq_annot objects 
    /// related to the given seq-entry
    explicit CSeq_annot_CI(const CSeq_entry_Handle& entry,
                           EFlags flags = eSearch_recursive);

    /// Create an iterator that enumerates CSeq_annot objects 
    /// related to the given seq-set
    explicit CSeq_annot_CI(const CBioseq_set_Handle& bioseq_set,
                           EFlags flags = eSearch_recursive);

    /// Create an iterator that enumerates CSeq_annot objects 
    /// related to the given seq-entry from different scope
    CSeq_annot_CI(CScope& scope, const CSeq_entry& entry,
                  EFlags flags = eSearch_recursive);

    /// Create an iterator that enumerates CSeq_aannot objects 
    /// related to the given bioseq up to the TSE
    explicit CSeq_annot_CI(const CBioseq_Handle& bioseq);

    CSeq_annot_CI(const CSeq_annot_CI& iter);
    ~CSeq_annot_CI(void);

    CSeq_annot_CI& operator=(const CSeq_annot_CI& iter);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(m_CurrentAnnot);

    /// Move to the next object in iterated sequence
    CSeq_annot_CI& operator++(void);

    /// Get the current scope of the iterator
    CScope& GetScope(void) const;

    const CSeq_annot_Handle& operator*(void) const;
    const CSeq_annot_Handle* operator->(void) const;

private:
    void x_Initialize(const CSeq_entry_Handle& entry_handle, EFlags flags);

    void x_SetEntry(const CSeq_entry_Handle& entry);
    void x_Push(void);
    void x_Settle(void);

    typedef vector< CRef<CSeq_annot_Info> > TAnnots;
    typedef TAnnots::const_iterator TAnnot_CI;
    typedef stack<CSeq_entry_CI> TEntryStack;

    const TAnnots& x_GetAnnots(void) const;

    CSeq_entry_Handle           m_CurrentEntry;
    TAnnot_CI                   m_AnnotIter;
    CSeq_annot_Handle           m_CurrentAnnot;
    TEntryStack                 m_EntryStack;
    // Used when initialized with a bioseq handle to iterate upwards
    bool                        m_UpTree;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CScope& CSeq_annot_CI::GetScope(void) const
{
    return m_CurrentEntry.GetScope();
}


inline
const CSeq_annot_Handle& CSeq_annot_CI::operator*(void) const
{
    _ASSERT(*this);
    return m_CurrentAnnot;
}


inline
const CSeq_annot_Handle* CSeq_annot_CI::operator->(void) const
{
    _ASSERT(*this);
    return &m_CurrentAnnot;
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_ANNOT_CI__HPP
