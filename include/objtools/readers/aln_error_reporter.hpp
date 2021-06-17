#ifndef _ALN_ERROR_REPORTER_HPP_
#define _ALN_ERROR_REPORTER_HPP_

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
#include <objtools/readers/reader_error_codes.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class ILineErrorListener;

/////////////////////////////////////////////////////////////////////
/// SShowStopper
///
/// Throw SShowStopper to
/// 1) log a fatal error to the error listener
/// specified when calling the CAlnReader::Read() methods
/// AND
/// 2) to immediately exit the Read() method WITHOUT generating an
/// alignment.
////////////////////////////////////////////////////////////////////
//  ============================================================================
struct NCBI_XOBJREAD_EXPORT SShowStopper : std::exception
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

    const char* what() const noexcept;

    int mLineNumber;
    EAlnSubcode mErrCode;
    string mDescr;
    string mSeqId;
};

//////////////////////////////////////////////////////////////////////////////
/// CAlnErrorReporter
///
/// Use CAlnErrorReporter to log messages to the error listener passed
/// to the CAlnReader::Read() methods.
/// CAlnErrorReporter simply logs messages and provides no other guarantee
/// on the behavior of the code.
/// In particular, CAlnErrorReporter makes no guarantee that invoking Fatal()
/// will cause a read to terminate before an alignment is generated.
/// That behavior is dependent on the underlying error-listener implementation.
//////////////////////////////////////////////////////////////////////////////
//  ============================================================================
class NCBI_XOBJREAD_EXPORT CAlnErrorReporter
//  ============================================================================
{
public:
    CAlnErrorReporter(
        ILineErrorListener* pEl = nullptr) : mpEl(pEl) {};

    ~CAlnErrorReporter() {};

    void
    Fatal(const SShowStopper& showStopper);

    void
    Fatal(
        int lineNumber,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId = "");

    void
    Error(const SShowStopper& showStopper);

    void
    Error(
        int lineNumber,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId = "");

    void
    Warn(
        int lineNumber,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId = "");

    void
    Report(
        int lineNumber,
        EDiagSev severity,
        EReaderCode subsystem,
        EAlnSubcode errorCode,
        const string& descr,
        const string& seqId = "");

protected:
    ILineErrorListener* mpEl;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_ERROR_REPORTER_HPP_
