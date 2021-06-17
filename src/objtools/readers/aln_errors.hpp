#ifndef _ALN_ERRORS_HPP_
#define _ALN_ERRORS_HPP_

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
#include <corelib/ncbistd.hpp>
#include <objtools/readers/aln_error_reporter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

/*
class ILineErrorListener;

//  ============================================================================
struct SShowStopper : std::exception
    //  ============================================================================
{
public:
    SShowStopper(
        int lineNumber,
        EAlnSubcode errCode,
        const string& descr,
        const string& seqId = "") :
        mLineNumber(lineNumber),
        mErrCode(errCode),
        mDescr(descr),
        mSeqId(seqId) {};

    const char* what() const noexcept {
        return mDescr.c_str();
    }

    int mLineNumber;
    EAlnSubcode mErrCode;
    string mDescr;
    string mSeqId;
};

//  ============================================================================
class CAlnErrorReporter
    //  ============================================================================
{
public:
    CAlnErrorReporter(
        ILineErrorListener* pEl = nullptr) : mpEl(pEl) {};

    ~CAlnErrorReporter() {};

    void
    Fatal(
        const SShowStopper& showStopper)
    {
        Fatal(
            showStopper.mLineNumber,
            showStopper.mErrCode,
            showStopper.mDescr,
            showStopper.mSeqId);
    };

    void
    Fatal(
        int lineNumber,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId = "")
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
    Error(
        const SShowStopper& showStopper)
    {
        Error(
            showStopper.mLineNumber,
            showStopper.mErrCode,
            showStopper.mDescr,
            showStopper.mSeqId);
    };

    void
    Error(
        int lineNumber,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId = "")
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
    Warn(
        int lineNumber,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId = "")
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
    Report(
        int lineNumber,
        EDiagSev severity,
        EReaderCode subsystem,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId = "")
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
                "",
                lineNumber,
                message));
        mpEl->PutError(*pErr);
    }

protected:
    ILineErrorListener* mpEl;
};
*/

extern thread_local unique_ptr<CAlnErrorReporter> theErrorReporter;

string ErrorPrintf(const char *format, ...);

string BadCharCountPrintf(int expectedCount, int actualCount);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_ERRORS_HPP_
