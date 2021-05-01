/*
 * $Id$
 *
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
 * Authors: Frank Ludwig
 *
 */
#include <ncbi_pch.hpp>
#include <objtools/readers/aln_error_reporter.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

class ILineErrorListener;

const char* SShowStopper::what() const noexcept {
    return mDescr.c_str();
}


void
CAlnErrorReporter::Fatal(
        const SShowStopper& showStopper)
{
    Fatal(
        showStopper.mLineNumber,
        showStopper.mErrCode,
        showStopper.mDescr,
        showStopper.mSeqId);
};


void
CAlnErrorReporter::Fatal(
    int lineNumber,
    EAlnSubcode errorCode,
    const string& descr,
    const string& seqId)
{
    Report(
        lineNumber,
        EDiagSev::eDiag_Fatal,
        EReaderCode::eReader_Alignment,
        errorCode,
        descr,
        seqId);
};


void
CAlnErrorReporter::Error(
        const SShowStopper& showStopper)
{
    Error(
        showStopper.mLineNumber,
        showStopper.mErrCode,
        showStopper.mDescr,
        showStopper.mSeqId);
};


void
CAlnErrorReporter::Error(
    int lineNumber,
    EAlnSubcode errorCode,
    const string& descr,
    const string& seqId)
{
    Report(
        lineNumber,
        EDiagSev::eDiag_Error,
        EReaderCode::eReader_Alignment,
        errorCode,
        descr,
        seqId);
};


void
CAlnErrorReporter::Warn(
    int lineNumber,
    EAlnSubcode errorCode,
    const string& descr,
    const string& seqId)
{
    Report(
        lineNumber,
        EDiagSev::eDiag_Warning,
        EReaderCode::eReader_Alignment,
        errorCode,
        descr,
        seqId);
};


void
CAlnErrorReporter::Report(
        int lineNumber,
        EDiagSev severity,
        EReaderCode subsystem,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId)
{
    string message(descr);
    if (!seqId.empty()) {
        message = "At ID \'" + seqId + "\': " + descr;
    }
    if (!mpEl) {
        NCBI_THROW2(CObjReaderParseException, eFormat, message, 0);
    }
    if (lineNumber == -1) {
        lineNumber = 0;
    }
    AutoPtr<CLineErrorEx> pErr(
        CLineErrorEx::Create(
            ILineError::eProblem_GeneralParsingError,
            severity,
            subsystem,
            errorCode,
            seqId,
            lineNumber,
            message));
    mpEl->PutError(*pErr);
}


END_SCOPE(objects);
END_NCBI_SCOPE;

