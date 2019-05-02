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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Test application for the CFormatGuess component
*
* ===========================================================================
*/

#ifndef IMPORT_ERROR__HPP
#define IMPORT_ERROR__HPP

#include <corelib/ncbistd.hpp>
#include <util/line_reader.hpp>

#undef ERROR

BEGIN_NCBI_SCOPE

//  ============================================================================
class NCBI_XOBJIMPORT_EXPORT CImportError:
    public CException
//  ============================================================================
{
public:
    enum  ErrorLevel {
        PROGRESS = -1,
        FATAL = 0,          // show stops here, discard all data
        CRITICAL = 1,       // show stops here, preserve data retrieved so far
        ERROR = 2,          // discard current unit of information
        WARNING = 3,        // fix up and use current unit of information
        DEBUG = 4,
        NONE = 10,
    };

    enum ErrorCode {
        eUNSPECIFIED = 0,
    };

public:
    CImportError(
        ErrorLevel,
        const std::string&,
        unsigned int =0,
        ErrorCode = eUNSPECIFIED); //line number

    CImportError&
    operator =(
        const CImportError& rhs) {
        mSeverity = rhs.mSeverity;
        mMessage = rhs.mMessage;
        mLineNumber = rhs.mLineNumber;
        return *this;
    };

    virtual ~CImportError() {};

    void
    SetLineNumber(
        unsigned int lineNumber) { mLineNumber = lineNumber; };

    void
    AmendMessage(
        const std::string& amend) { mAmend = amend; };

    ErrorLevel Severity() const { return mSeverity; };
    std::string Message() const;
    unsigned int LineNumber() const { return mLineNumber; };
    ErrorCode Code() const { return mCode; };

    string 
    SeverityStr() const;

    void
    Serialize(
        CNcbiOstream&);

protected:
    ErrorLevel mSeverity;
    ErrorCode mCode;
    string mMessage;
    string mAmend;
    unsigned int mLineNumber;
};

END_NCBI_SCOPE

#endif
