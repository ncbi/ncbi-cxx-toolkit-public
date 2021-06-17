#ifndef DESC_CI__HPP
#define DESC_CI__HPP

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

#include <objects/seq/Seq_descr.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/bioseq_base_info.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CBioseq_Handle;
class CSeqdesc_CI;
class CBioseq_Base_Info;

/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_descr_CI --
///
///  Enumerate CSeq_descr objects from a Bioseq or Seq-entry handle
///

class NCBI_XOBJMGR_EXPORT CSeq_descr_CI
{
public:
    /// Create an empty iterator
    CSeq_descr_CI(void);

    /// Create an iterator that enumerates CSeq_descr objects 
    /// from a bioseq with limit number of seq-entries
    /// to "search_depth" (0 = unlimited).
    explicit CSeq_descr_CI(const CBioseq_Handle& handle,
                           size_t search_depth = 0);

    /// Create an iterator that enumerates CSeq_descr objects 
    /// from a bioseq with limit number of seq-entries
    /// to "search_depth" (0 = unlimited).
    explicit CSeq_descr_CI(const CBioseq_set_Handle& handle,
                           size_t search_depth = 0);

    /// Create an iterator that enumerates CSeq_descr objects 
    /// from a seq-entry, limit number of seq-entries
    /// to "search_depth" (0 = unlimited).
    explicit CSeq_descr_CI(const CSeq_entry_Handle& entry,
                           size_t search_depth = 0);

    CSeq_descr_CI(const CSeq_descr_CI& iter);
    ~CSeq_descr_CI(void);

    CSeq_descr_CI& operator= (const CSeq_descr_CI& iter);

    /// Move to the next object in iterated sequence
    CSeq_descr_CI& operator++ (void);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL_REF(m_CurrentBase);

    const CSeq_descr& operator*  (void) const;
    const CSeq_descr* operator-> (void) const;

    CSeq_entry_Handle GetSeq_entry_Handle(void) const;

private:
    friend class CSeqdesc_CI;

    // Move to the next entry containing a descriptor
    void x_Next(void);
    void x_Step(void);
    void x_Settle(void);

    const CBioseq_Base_Info& x_GetBaseInfo(void) const;

    CConstRef<CBioseq_Base_Info> m_CurrentBase;
    CBioseq_Handle        m_CurrentSeq;
    CBioseq_set_Handle    m_CurrentSet;
    size_t                m_ParentLimit;
};


inline
CSeq_descr_CI& CSeq_descr_CI::operator++(void)
{
    x_Next();
    return *this;
}


inline
const CBioseq_Base_Info& CSeq_descr_CI::x_GetBaseInfo(void) const
{
    return *m_CurrentBase;
}


inline
const CSeq_descr& CSeq_descr_CI::operator* (void) const
{
    return x_GetBaseInfo().GetDescr();
}


inline
const CSeq_descr* CSeq_descr_CI::operator-> (void) const
{
    return &x_GetBaseInfo().GetDescr();
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // DESC_CI__HPP
