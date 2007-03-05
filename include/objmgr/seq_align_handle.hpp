#ifndef SEQ_ALIGN_HANDLE__HPP
#define SEQ_ALIGN_HANDLE__HPP

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
*   Seq-align handle
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.h>
#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/seq_annot_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerHandles
 *
 * @{
 */


class CScope;
class CSeq_annot_Handle;
class CAlign_CI;
class CAnnotObject_Info;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_align_Handle --
///
///  Proxy to access seq-align objects data
///

template<typename Handle>
class CSeq_annot_Add_EditCommand;
template<typename Handle>
class CSeq_annot_Replace_EditCommand;
template<typename Handle>
class CSeq_annot_Remove_EditCommand;

class NCBI_XOBJMGR_EXPORT CSeq_align_Handle
{
public:
    CSeq_align_Handle(void);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_Annot && m_AnnotIndex != eNull && !IsRemoved());

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get handle to the seq-annot
    const CSeq_annot_Handle& GetAnnot(void) const;

    /// Get const reference to current seq-align
    CConstRef<CSeq_align> GetSeq_align(void) const;

    // Mappings for CSeq_align methods
    CSeq_align::EType GetType(void) const;
    bool IsSetDim(void) const;
    CSeq_align::TDim GetDim(void) const;
    bool IsSetScore(void) const;
    const CSeq_align::TScore& GetScore(void) const;
    const CSeq_align::TSegs& GetSegs(void) const;
    bool IsSetBounds(void) const;
    const CSeq_align::TBounds& GetBounds(void) const;

    /// Return true if this Seq-align was removed already
    bool IsRemoved(void) const;
    /// Remove the Seq-align from Seq-annot
    void Remove(void) const;
    /// Replace the Seq-align with new Seq-align object.
    /// All indexes are updated correspondingly.
    void Replace(const CSeq_align& new_obj) const;
    /// Update index after manual modification of the object
    void Update(void) const;

private:
    friend class CAlign_CI;
    friend class CSeq_annot_EditHandle;
    typedef Int4 TIndex;

    enum {
        eNull = kMax_I4
    };

    const CSeq_align& x_GetSeq_align(void) const;

    CSeq_align_Handle(const CSeq_annot_Handle& annot, TIndex index);

    CSeq_annot_Handle          m_Annot;
    TIndex                     m_AnnotIndex;

private:

    friend class CSeq_annot_Add_EditCommand<CSeq_align_Handle>;
    friend class CSeq_annot_Replace_EditCommand<CSeq_align_Handle>;
    friend class CSeq_annot_Remove_EditCommand<CSeq_align_Handle>;

    /// Remove the Seq-align from Seq-annot
    void x_RealRemove(void) const;
    /// Replace the Seq-align with new Seq-align object.
    /// All indexes are updated correspondingly.
    void x_RealReplace(const CSeq_align& new_obj) const;

};


inline
CSeq_align_Handle::CSeq_align_Handle(void)
    : m_AnnotIndex(eNull)
{
}


inline
const CSeq_annot_Handle& CSeq_align_Handle::GetAnnot(void) const
{
    return m_Annot;
}


inline
CScope& CSeq_align_Handle::GetScope(void) const
{
    return GetAnnot().GetScope();
}


inline
CSeq_align::EType CSeq_align_Handle::GetType(void) const
{
    return x_GetSeq_align().GetType();
}


inline
bool CSeq_align_Handle::IsSetDim(void) const
{
    return x_GetSeq_align().IsSetDim();
}


inline
CSeq_align::TDim CSeq_align_Handle::GetDim(void) const
{
    return x_GetSeq_align().GetDim();
}


inline
bool CSeq_align_Handle::IsSetScore(void) const
{
    return x_GetSeq_align().IsSetScore();
}


inline
const CSeq_align::TScore& CSeq_align_Handle::GetScore(void) const
{
    return x_GetSeq_align().GetScore();
}


inline
const CSeq_align::TSegs& CSeq_align_Handle::GetSegs(void) const
{
    return x_GetSeq_align().GetSegs();
}


inline
bool CSeq_align_Handle::IsSetBounds(void) const
{
    return x_GetSeq_align().IsSetBounds();
}


inline
const CSeq_align::TBounds& CSeq_align_Handle::GetBounds(void) const
{
    return x_GetSeq_align().GetBounds();
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_ALIGN_HANDLE__HPP
