#ifndef UTIL_SEQUTIL___SEQUTIL_EXPT__HPP
#define UTIL_SEQUTIL___SEQUTIL_EXPT__HPP

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
 *      Exception class for sequence utilities.
 */   
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE


class CSeqUtilException : public CException
{
public:
    enum EErrCode {
        eNotSupported,
        eInvalidCoding,
        eBadConversion,
        eBadParameter
    };
    virtual const char* GetErrCodeString(void) const {
          switch ( GetErrCode() ) {
          case eNotSupported:
              return "Operation not supported";
          case eInvalidCoding:
              return "Invalid coding";
          case eBadConversion:
              return "Attempt to perform illegal conversion";
          case eBadParameter:
              return "One or more parameters passed are invalid";
          default:
              return CException::GetErrCodeString();
          }
    }

    NCBI_EXCEPTION_DEFAULT(CSeqUtilException, CException);
};


END_NCBI_SCOPE


#endif  /* UTIL_SEQUTIL___SEQUTIL_EXPT__HPP */

 /*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/08/19 13:11:49  dicuccio
* Dropped export specifier on inlined exception
*
* Revision 1.1  2003/10/08 13:29:12  shomrat
* Initial version.
*
*
* ===========================================================================
*/
