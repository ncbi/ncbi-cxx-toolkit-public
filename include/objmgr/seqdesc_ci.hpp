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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/11 19:04:04  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/desc_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeqdesc_CI
{
public:
    CSeqdesc_CI(void);
    CSeqdesc_CI(const CDesc_CI& desc_it,
                CSeqdesc::E_Choice choice = CSeqdesc::e_not_set);
    CSeqdesc_CI(const CSeqdesc_CI& iter);
    ~CSeqdesc_CI(void);

    CSeqdesc_CI& operator= (const CSeqdesc_CI& iter);

    CSeqdesc_CI& operator++ (void); // prefix
    CSeqdesc_CI  operator++ (int);  // postfix
    operator bool (void) const;

    const CSeqdesc& operator*  (void) const;
    const CSeqdesc* operator-> (void) const;

private:
    typedef list< CRef<CSeqdesc> >::const_iterator TRawIterator;

    CDesc_CI            m_Outer;
    TRawIterator        m_Inner;
    TRawIterator        m_InnerEnd;
    CConstRef<CSeqdesc> m_Current;
    CSeqdesc::E_Choice  m_Choice;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQDESC_CI__HPP
