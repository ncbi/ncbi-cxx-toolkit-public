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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/01/16 16:26:37  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:01  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/objmgr1/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDesc_CI
{
public:
    CDesc_CI(void);
    CDesc_CI(const CBioseqHandle& handle);
    CDesc_CI(const CDesc_CI& iter);
    ~CDesc_CI(void);

    CDesc_CI& operator= (const CDesc_CI& iter);

    CDesc_CI& operator++ (void);
    CDesc_CI& operator++ (int);
    operator bool (void) const;

    const CSeq_descr& operator*  (void) const;
    const CSeq_descr* operator-> (void) const;

private:
    // Move to the next entry containing a descriptor
    void x_Walk(void);

    CBioseqHandle         m_Handle;    // Source bioseq
    CConstRef<CSeq_entry> m_NextEntry; // Next Seq-entry to get descriptor from
    CConstRef<CSeq_descr> m_Current;   // Current descriptor
};


inline
CDesc_CI& CDesc_CI::operator++(int)
{
    return ++(*this);
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // DESC_CI__HPP
