#if defined(SERIALDEF__HPP)  &&  !defined(SERIALDEF__INL)
#define SERIALDEF__INL

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
* Revision 1.1  1999/06/24 14:44:44  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

inline
TObjectPtr Add(TObjectPtr object, int offset)
{
    return static_cast<char*>(object) + offset;
}

inline
TConstObjectPtr Add(TConstObjectPtr object, int offset)
{
    return static_cast<const char*>(object) + offset;
}

inline
int Sub(TConstObjectPtr first, TConstObjectPtr second)
{
    return static_cast<const char*>(first) - static_cast<const char*>(second);
}

#endif /* def SERIALDEF__HPP  &&  ndef SERIALDEF__INL */
