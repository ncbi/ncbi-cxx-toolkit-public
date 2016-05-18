#ifndef ASN_CACHE_EXCEPTION_HPP__
#define ASN_CACHE_EXCEPTION_HPP__

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
 * Authors:  Cheinan Marks
 *
 * File Description:
 *   Produce an ASN.1 cache index from CSeq_entry blobs passed from ID. 
 *   This module is built into a library designed to be linked into an
 *   ID team machine with direct access to the databases.
 */

BEGIN_NCBI_SCOPE


#include <corelib/ncbiexpt.hpp>


class CASNCacheException : public CException
{
public:
    enum EErrCode {
        eMaxBadBlobCountExceeded
        , eRootDirectoryCreationFailed
        , eCantOpenChunkFile
        , eCantCopyChunkFile
        , eCantFindChunkFile
    };  

    virtual const char* GetErrCodeString() const
    {   
        switch (GetErrCode()) {
            case eMaxBadBlobCountExceeded: return "Bad blob count exceeds maximum.";
            case eRootDirectoryCreationFailed: return "Could not create root directory.";
            case eCantOpenChunkFile: return "Unable to open a cache chunk file.";
            case eCantCopyChunkFile: return "Unable to copy a cache chunk file.";
            case eCantFindChunkFile: return "Unable to find a cache chunk file.";
            default:     return CException::GetErrCodeString();
        }   
    }   

    NCBI_EXCEPTION_DEFAULT(CASNCacheException, CException);
};


END_NCBI_SCOPE


#endif  // ASN_CACHE_EXCEPTION_HPP__

