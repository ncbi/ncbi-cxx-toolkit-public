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

    TDescTypeMask       m_Choice;
    CSeq_descr_CI       m_Entry;
    TDescList_CI        m_Desc_CI;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.12  2004/10/07 14:03:32  vasilche
* Use shared among TSEs CTSE_Split_Info.
* Use typedefs and methods for TSE and DataSource locking.
* Load split CSeqdesc on the fly in CSeqdesc_CI.
*
* Revision 1.11  2004/10/01 16:47:00  kononenk
* Added doxygen formatting
*
* Revision 1.10  2004/04/28 14:14:39  grichenk
* Added filtering by several seqdesc types.
*
* Revision 1.9  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.8  2004/02/09 19:18:50  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.7  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.6  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.5  2002/12/05 19:28:30  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.4  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/02/21 19:27:01  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/01/11 19:04:04  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // SEQDESC_CI__HPP
