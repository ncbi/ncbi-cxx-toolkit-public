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

#ifndef FEAT_IMPORT_ERROR__HPP
#define FEAT_IMPORT_ERROR__HPP

#include <corelib/ncbifile.hpp>
#include <util/line_reader.hpp>

#undef ERROR

BEGIN_NCBI_SCOPE

//  ============================================================================
class CFeatureImportError:
    public CException
//  ============================================================================
{
public:
    enum  ErrorLevel{
        PROGRESS = -1,
        CRITICAL = 0,
        ERROR = 1,
        WARNING = 2,
        INFO = 3,
        DEBUG = 4,
    };

public:
    CFeatureImportError(
        ErrorLevel,
        const std::string,
        unsigned int =0); //line number

    virtual ~CFeatureImportError() {};

    void
    SetLineNumber(
        unsigned int lineNumber) { mLineNumber = lineNumber; };

    ErrorLevel Severity() const { return mSeverity; };
    const std::string& Message() const { return mMessage; };
    unsigned int LineNumber() const { return mLineNumber; };
    
    string 
    SeverityStr() const;

    void
    Serialize(
        CNcbiOstream&);

protected:
    ErrorLevel mSeverity;
    string mMessage;
    unsigned int mLineNumber;
};

END_NCBI_SCOPE

#endif
