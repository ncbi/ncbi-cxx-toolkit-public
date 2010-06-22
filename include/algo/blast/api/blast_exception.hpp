/* $Id$
 * ===========================================================================
 *
 *                            public DOMAIN NOTICE                          
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
 * Author:  Christiam Camacho
 *
 */

/// @file blast_exception.hpp
/// Declares the BLAST exception class.

#ifndef ALGO_BLAST_API___BLAST_EXCEPTION__HPP
#define ALGO_BLAST_API___BLAST_EXCEPTION__HPP

#include <corelib/ncbiexpt.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Defines system exceptions occurred while running BLAST
class CBlastSystemException : public CException
{
public:
    /// Error types that BLAST can generate
    enum EErrCode {
        eOutOfMemory,   ///< Out-of-memory 
        eNetworkError   ///< Network error
    };

    /// Translate from the error code value to its string representation
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eOutOfMemory:          return "eOutOfMemory";
        default:                    return CException::GetErrCodeString();
        }
    }

#ifndef SKIP_DOXYGEN_PROCESSING
    NCBI_EXCEPTION_DEFAULT(CBlastSystemException, CException);
#endif /* SKIP_DOXYGEN_PROCESSING */
};

/// Defines BLAST error codes (user errors included)
class CBlastException : public CException
{
public:
    /// Error types that BLAST can generate
    enum EErrCode {
        eCoreBlastError,    ///< FIXME: need to interpret CORE errors
        eInvalidOptions,    ///< Invalid algorithm options
        eInvalidArgument,   ///< Invalid argument to some function/method
                            /// (could be programmer error - prefer assertions
                            /// in those cases unless API needs to be 
                            /// "bullet-proof")
        eNotSupported,      ///< Feature not supported
        eInvalidCharacter,  ///< Invalid character in sequence data
        eSeqSrcInit,        ///< Initialization error in BlastSeqSrc 
                            /// implementation
        eRpsInit,           ///< Error while initializing RPS-BLAST
        eSetup              ///< Error while setting up BLAST
    };

    /// Translate from the error code value to its string representation
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eCoreBlastError:       return "eCoreBlastError";
        case eInvalidOptions:       return "eInvalidOptions";
        case eInvalidArgument:      return "eInvalidArgument";
        case eNotSupported:         return "eNotSupported";
        case eInvalidCharacter:     return "eInvalidCharacter";
        case eSeqSrcInit:           return "eSeqSrcInit";
        case eRpsInit:              return "eRpsInit";
        default:                    return CException::GetErrCodeString();
        }
    }

#ifndef SKIP_DOXYGEN_PROCESSING
    NCBI_EXCEPTION_DEFAULT(CBlastException,CException);
#endif /* SKIP_DOXYGEN_PROCESSING */
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_EXCEPTION__HPP */
