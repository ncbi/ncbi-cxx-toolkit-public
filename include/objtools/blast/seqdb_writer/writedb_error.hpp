#ifndef OBJTOOLS_BLAST_SEQDB_WRITER___WRITEDB_ERROR__HPP
#define OBJTOOLS_BLAST_SEQDB_WRITER___WRITEDB_ERROR__HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_error.hpp
/// Defines exception class for WriteDB.
/// 
/// Defines classes:
///     CWriteDBException
/// 
/// Implemented for: UNIX, MS-Windows

#include <ncbiconf.h>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

/// CWriteDBException
/// 
/// This exception class is thrown for WriteDB related errors such as
/// configuration, parameter, and file errors.

class NCBI_XOBJWRITE_EXPORT CWriteDBException : public CException {
public:
    /// Errors are classified into one of two types.
    enum EErrCode {
        /// Argument validation failed.
        eArgErr,
        
        /// Files were missing or contents were incorrect.
        eFileErr
    };
    
    /// Get a message describing the situation leading to the throw.
    virtual const char* GetErrCodeString() const
    {
        switch ( GetErrCode() ) {
        case eArgErr:  return "eArgErr";
        case eFileErr: return "eFileErr";
        default:       return CException::GetErrCodeString();
        }
    }
    
    /// Include standard NCBI exception behavior.
    NCBI_EXCEPTION_DEFAULT(CWriteDBException,CException);
};

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_WRITER___WRITEDB_ERROR__HPP

