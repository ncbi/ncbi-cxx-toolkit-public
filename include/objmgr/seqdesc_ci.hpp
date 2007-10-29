#ifndef SEQDESC_CI__HPP
#define SEQDESC_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Object manager iterators
*
*/


#include <objmgr/seq_descr_ci.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seq/Seqdesc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CBioseq_Base_Info;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeqdesc_CI --
///
///  Another type of descriptor 
///  Enumerate individual descriptors (CSeqdesc) rather than sets of them
///
///  @sa
///    CSeq_descr_CI

class NCBI_XOBJMGR_EXPORT CSeqdesc_CI
{
public:
    typedef vector<CSeqdesc::E_Choice> TDescChoices;

    CSeqdesc_CI(void);
    // Old method, should not be used.
    CSeqdesc_CI(const CSeq_descr_CI& desc_it,
                CSeqdesc::E_Choice choice = CSeqdesc::e_not_set);

    /// Create an iterator that enumerates CSeqdesc objects 
    /// from a bioseq with limit number of seq-entries
    /// to "search_depth" (0 = unlimited) for specific type
    CSeqdesc_CI(const CBioseq_Handle& handle,
                CSeqdesc::E_Choice choice = CSeqdesc::e_not_set,
                size_t search_depth = 0);

    /// Create an iterator that enumerates CSeqdesc objects 
    /// from a seq-entry, limit number of seq-entries
    /// to "search_depth" (0 = unlimited) for specific type
    CSeqdesc_CI(const CSeq_entry_Handle& entry,
                CSeqdesc::E_Choice choice = CSeqdesc::e_not_set,
                size_t search_depth = 0);

    /// Create an iterator that enumerates CSeqdesc objects 
    /// from a bioseq with limit number of seq-entries
    /// to "search_depth" (0 = unlimited) for set of types
    CSeqdesc_CI(const CBioseq_Handle& handle,
                const TDescChoices& choices,
                size_t search_depth = 0);

    /// Create an iterator that enumerates CSeqdesc objects 
    /// from a seq-entry, limit number of seq-entries
    /// to "search_depth" (0 = unlimited) for set of types
    CSeqdesc_CI(const CSeq_entry_Handle& entry,
                const TDescChoices& choices,
                size_t search_depth = 0);

    CSeqdesc_CI(const CSeqdesc_CI& iter);
    ~CSeqdesc_CI(void);

    CSeqdesc_CI& operator= (const CSeqdesc_CI& iter);

    /// Move to the next object in iterated sequence
    CSeqdesc_CI& operator++ (void); // prefix

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(m_Entry);

    const CSeqdesc& operator*  (void) const;
    const CSeqdesc* operator-> (void) const;

    CSeq_entry_Handle GetSeq_entry_Handle(void) const;

private:
    CSeqdesc_CI operator++ (int); // prohibit postfix

    typedef unsigned TDescTypeMask;

    typedef list< CRef<CSeqdesc> >        TDescList;
    typedef TDescList::const_iterator     TDescList_CI;

    void x_AddChoice(CSeqdesc::E_Choice choice);
    void x_SetChoice(CSeqdesc::E_Choice choice);
    void x_SetChoices(const TDescChoices& choices);

    bool x_RequestedType(void) const;

    bool x_Valid(void) const; // for debugging
    bool x_ValidDesc(void) const;

    void x_FirstDesc(void);
    void x_NextDesc(void);
    
    void x_SetEntry(const CSeq_descr_CI& entry);

    void x_Next(void);
    void x_Settle(void);

    const CBioseq_Base_Info& x_GetBaseInfo(void) const;

    TDescTypeMask       m_Choice;
    CSeq_descr_CI       m_Entry;
    TDescList_CI        m_Desc_CI;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQDESC_CI__HPP
