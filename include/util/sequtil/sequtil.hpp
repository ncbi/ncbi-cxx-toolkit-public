#ifndef UTIL_SEQUTIL___SEQUTIL__HPP
#define UTIL_SEQUTIL___SEQUTIL__HPP

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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   
 */   
#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE


class NCBI_XUTIL_EXPORT CSeqUtil
{
public:
    enum ECoding {
        e_not_set = 0,

        // NA coding
        e_Iupacna,
        e_Ncbi2na,
        e_Ncbi2na_expand,  // 2 bits per char
        e_Ncbi4na,
        e_Ncbi4na_expand,  // 4 bits per char (same as 8na)
        e_Ncbi8na,
        
        // AA coding
        e_Iupacaa,
        e_Ncbi8aa,
        e_Ncbieaa,
        e_Ncbistdaa
    };
    static const size_t kNumCodings;

    enum ECodingType {
        e_CodingType_Na,
        e_CodingType_Aa
    };

    // types
    typedef ECoding      TCoding;
    typedef ECodingType  TCodingType;

    static ECodingType GetCodingType(TCoding coding);
};



END_NCBI_SCOPE


#endif  /* UTIL_SEQUTIL___SEQUTIL__HPP */

 /*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/08/17 13:21:58  dicuccio
* Added export specifier
*
* Revision 1.2  2003/11/06 16:10:09  shomrat
* added e_not_set as a coding option
*
* Revision 1.1  2003/10/08 13:28:41  shomrat
* Initial version.
*
*
* ===========================================================================
*/
