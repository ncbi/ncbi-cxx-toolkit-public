#ifndef OBJSTRB__HPP
#define OBJSTRB__HPP

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
* Revision 1.4  1999/06/15 16:20:05  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.3  1999/06/10 21:06:41  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.2  1999/06/04 20:51:36  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:27  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CObjectStreamBinaryDefs
{
public:
    enum {
        eNull = 0,

        eStd_char = 0xC0, // char
        eStd_ubyte,       // unsigned char or any with sizeof() == 1
        eStd_sbyte,       // signed char or any with sizeof() == 1
        eStd_uordinal,    // any unsigned
        eStd_sordinal,    // any signed
        eStd_string,      // string
        eStd_float,       // float, double, long double

        eBlock = 0xE0,
        eObjectReference,
        eMemberReference,
        eThisClass,
        eOtherClass,
        eElement,
        eEndOfElements
    };
};

//#include <objstrb.inl>

END_NCBI_SCOPE

#endif  /* OBJSTRB__HPP */
