/* $Id$
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
 */

#ifndef OBJTOOLS_BLASTDB_FORMAT__INVALID_DATA_EXCEPTION__HPP
#define OBJTOOLS_BLASTDB_FORMAT__INVALID_DATA_EXCEPTION__HPP

#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

// Note: This exception class replaces CInputException from blastinput
//       since we are getting rid of all dependencies from algo/blast.

/// Defines invalid user input exceptions
class NCBI_BLASTDB_FORMAT_EXPORT CInvalidDataException : public CException
{
public:
    /// Error types
    enum EErrCode {
        eInvalidRange,      ///< Invalid range specification
        eInvalidInput       ///< Invalid input data
    };

    /// Translate from the error code value to its string representation
    virtual const char* GetErrCodeString(void) const override {
        switch ( GetErrCode() ) {
        case eInvalidRange:         return "eInvalidRange";
        case eInvalidInput:         return "eInvalidInput";
        default:                    return CException::GetErrCodeString();
        }
    }

#ifndef SKIP_DOXYGEN_PROCESSING
    NCBI_EXCEPTION_DEFAULT(CInvalidDataException, CException);
#endif /* SKIP_DOXYGEN_PROCESSING */
};

END_NCBI_SCOPE

#endif  /* OBJTOOLS_BLASTDB_FORMAT__INVALID_DATA_EXCEPTION__HPP */
