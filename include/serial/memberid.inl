#if defined(MEMBERID__HPP)  &&  !defined(MEMBERID__INL)
#define MEMBERID__INL

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/06/30 16:04:25  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

inline
CMemberId::CMemberId(void)
    : m_Tag(-1)
{
}

inline
CMemberId::CMemberId(const string& name)
    : m_Name(name), m_Tag(-1)
{
}

inline
const string& CMemberId::GetName(void) const
{
    return m_Name;
}

inline
CMemberId::TTag CMemberId::GetTag(void) const
{
    return m_Tag;
}

#endif /* def MEMBERID__HPP  &&  ndef MEMBERID__INL */
