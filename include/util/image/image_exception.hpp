#ifndef UTIL_IMAGE___IMAGE_EXCEPTION__HPP
#define UTIL_IMAGE___IMAGE_EXCEPTION__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *    CImageException -- exceptions for images, primarily for I/O
 */

#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE


//
// class CImage defines exceptions valid for images.  These exceptions are
// largely related to image reading
//
class CImageException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eInvalidDimension,
        eInvalidImage,
        eReadError,
        eUnsupported,
        eWriteError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eInvalidDimension: return "eInvalidDimension";
        case eInvalidImage:     return "eInvalidImage";
        case eReadError:        return "eReadError";
        case eUnsupported:      return "eUnsupported";
        case eWriteError:       return "eWriteError";
        default:                return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CImageException,CException);
};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/08/19 13:10:56  dicuccio
 * Dropped export specifier on inlined exceptions
 *
 * Revision 1.2  2003/08/27 16:43:48  ivanov
 * Added export specifier to class definition
 *
 * Revision 1.1  2003/06/03 15:17:41  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

#endif  // UTIL_IMAGE___IMAGE_EXCEPTION__HPP
