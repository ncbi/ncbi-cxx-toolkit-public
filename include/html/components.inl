#if defined(COMPONENTS__HPP)  &&  !defined(COMPONENTS__INL)
#define COMPONENTS__INL

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
* Revision 1.1  1999/01/21 21:12:53  vasilche
* Added/used descriptions for HTML submit/select/text.
* Fixed some bugs in paging.
*
* ===========================================================================
*/

inline COptionDescription::COptionDescription(const string& value)
    : m_Value(value)
{
}

inline COptionDescription::COptionDescription(const string& value,
                                              const string& label)
    : m_Value(value), m_Label(label)
{
}

inline void CSelectDescription::Add(int value)
{
    Add(NStr::IntToString(value));
}

#endif /* def COMPONENTS__HPP  &&  ndef COMPONENTS__INL */
