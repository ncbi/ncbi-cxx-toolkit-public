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
* Authors:  Aaron Ucko, NCBI
*
* File Description:
*   Basic reader interface
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/readers/line_error.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// static
CLineError* CLineError::Create(
        EProblem eProblem,
        EDiagSev eSeverity,
        const std::string& strSeqId,
        unsigned int uLine,
        const std::string & strFeatureName,
        const std::string & strQualifierName,
        const std::string & strQualifierValue,
        const std::string & strErrorMessage,
        const TVecOfLines & vecOfOtherLines)
{
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    return new CLineError(
            eProblem,
            eSeverity,
            strSeqId,
            uLine,
            strFeatureName,
            strQualifierName,
            strQualifierValue,
            strErrorMessage,
            vecOfOtherLines);
}

void
CLineError::Throw(void) const {
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    throw *this;
}

CLineError::CLineError(
    EProblem eProblem,
    EDiagSev eSeverity,
    const std::string& strSeqId,
    unsigned int uLine,
    const std::string & strFeatureName,
    const std::string & strQualifierName,
    const std::string & strQualifierValue,
    const std::string & strErrorMessage,
    const TVecOfLines & vecOfOtherLines )
    : m_eProblem(eProblem), m_eSeverity( eSeverity ), m_strSeqId(strSeqId), m_uLine( uLine ),
    m_strFeatureName(strFeatureName), m_strQualifierName(strQualifierName),
    m_strQualifierValue(strQualifierValue), m_strErrorMessage(strErrorMessage),
    m_vecOfOtherLines(vecOfOtherLines)
{ }

CLineError::CLineError(const CLineError & rhs ) :
m_eProblem(rhs.m_eProblem), m_eSeverity(rhs.m_eSeverity ), m_strSeqId(rhs.m_strSeqId), m_uLine(rhs.m_uLine ),
    m_strFeatureName(rhs.m_strFeatureName), m_strQualifierName(rhs.m_strQualifierName),
    m_strQualifierValue(rhs.m_strQualifierValue), m_strErrorMessage(rhs.m_strErrorMessage),
    m_vecOfOtherLines(rhs.m_vecOfOtherLines)
{ }

ILineError *CLineError::Clone(void) const
{
    return new CLineError(*this);
}


CLineErrorEx* CLineErrorEx::Create(
        EProblem eProblem,
        EDiagSev eSeverity,
        int code,
        int subcode,
        const std::string& strSeqId,
        unsigned int uLine,
        const std::string & strErrorMessage,
        const std::string & strFeatureName,
        const std::string & strQualifierName,
        const std::string & strQualifierValue,
        const TVecOfLines & vecOfOtherLines)
{
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    return new CLineErrorEx(
            eProblem,
            eSeverity,
            code,
            subcode,
            strSeqId,
            uLine,
            strErrorMessage,
            strFeatureName,
            strQualifierName,
            strQualifierValue,
            vecOfOtherLines);
}


void
CLineErrorEx::Throw(void) const {
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    throw *this;
}


CLineErrorEx::CLineErrorEx(
    EProblem eProblem,
    EDiagSev eSeverity,
    int code,
    int subcode,
    const std::string& strSeqId,
    unsigned int uLine,
    const std::string & strErrorMessage,
    const std::string & strFeatureName,
    const std::string & strQualifierName,
    const std::string & strQualifierValue,
    const TVecOfLines & vecOfOtherLines )
    : m_eProblem(eProblem), m_eSeverity( eSeverity ),
      m_Code(code), m_Subcode(subcode),
    m_strSeqId(strSeqId), m_uLine( uLine ),
    m_strFeatureName(strFeatureName), m_strQualifierName(strQualifierName),
    m_strQualifierValue(strQualifierValue),
    m_strErrorMessage(strErrorMessage),
    m_vecOfOtherLines(vecOfOtherLines)
{ }


CLineErrorEx::CLineErrorEx(const CLineErrorEx & rhs ) :
    CLineErrorEx(rhs.m_eProblem,
                 rhs.m_eSeverity,
                 rhs.m_Code,
                 rhs.m_Subcode,
                 rhs.m_strSeqId,
                 rhs.m_uLine,
                 rhs.m_strErrorMessage,
                 rhs.m_strFeatureName,
                 rhs.m_strQualifierName,
                 rhs.m_strQualifierValue,
                 rhs.m_vecOfOtherLines)
{}

/*
m_eProblem(rhs.m_eProblem), m_eSeverity(rhs.m_eSeverity ), m_strSeqId(rhs.m_strSeqId), m_uLine(rhs.m_uLine ),
    m_strFeatureName(rhs.m_strFeatureName), m_strQualifierName(rhs.m_strQualifierName),
    m_strQualifierValue(rhs.m_strQualifierValue), m_strErrorMessage(rhs.m_strErrorMessage),
    m_vecOfOtherLines(rhs.m_vecOfOtherLines)
{ }

*/

ILineError *CLineErrorEx::Clone(void) const
{
    return new CLineErrorEx(*this);
}



CObjReaderLineException *
    CObjReaderLineException::Create(
    EDiagSev eSeverity,
    unsigned int uLine,
    const std::string &strMessage,
    EProblem eProblem,
    const std::string& strSeqId,
    const std::string & strFeatureName,
    const std::string & strQualifierName,
    const std::string & strQualifierValue,
    CObjReaderLineException::EErrCode eErrCode,
    const TVecOfLines & vecOfOtherLines
    )
{
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    return new CObjReaderLineException(
        eSeverity, uLine, strMessage, eProblem,
        strSeqId, strFeatureName, strQualifierName, strQualifierValue,
        eErrCode, vecOfOtherLines );
}

ILineError *CObjReaderLineException::Clone(void) const
{
    return new CObjReaderLineException(*this);
}

void
CObjReaderLineException::Throw(void) const {
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    throw *this;
}

CObjReaderLineException::CObjReaderLineException(
    EDiagSev eSeverity,
    unsigned int uLine,
    const std::string &strMessage,
    EProblem eProblem,
    const std::string& strSeqId,
    const std::string & strFeatureName,
    const std::string & strQualifierName,
    const std::string & strQualifierValue,
    CObjReaderLineException::EErrCode eErrCode,
    const TVecOfLines & vecOfOtherLines
    )
    : CObjReaderParseException( DIAG_COMPILE_INFO, 0, static_cast<CObjReaderParseException::EErrCode>(CException::eInvalid), strMessage, uLine,
    eDiag_Info ),
    m_eProblem(eProblem), m_strSeqId(strSeqId), m_uLineNumber(uLine),
    m_strFeatureName(strFeatureName), m_strQualifierName(strQualifierName),
    m_strQualifierValue(strQualifierValue), m_strErrorMessage(strMessage),
    m_vecOfOtherLines(vecOfOtherLines)
{
    SetSeverity( eSeverity );
    x_InitErrCode(static_cast<CException::EErrCode>(eErrCode));
}

CObjReaderLineException::CObjReaderLineException(const CObjReaderLineException & rhs ) :
CObjReaderParseException( rhs ),
    m_eProblem(rhs.Problem()), m_strSeqId(rhs.SeqId()), m_uLineNumber(rhs.Line()),
    m_strFeatureName(rhs.FeatureName()), m_strQualifierName(rhs.QualifierName()),
    m_strQualifierValue(rhs.QualifierValue()), m_strErrorMessage(rhs.ErrorMessage()),
    m_vecOfOtherLines(rhs.m_vecOfOtherLines)
{
    SetSeverity( rhs.Severity() );
    x_InitErrCode(static_cast<CException::EErrCode>(rhs.x_GetErrCode()));
}

// this is here because of an apparent compiler bug
std::string
CObjReaderLineException::ProblemStr() const
{
    if (!m_strErrorMessage.empty()) {
        return m_strErrorMessage;
    }
    return ILineError::ProblemStr();
}

END_SCOPE(objects)
END_NCBI_SCOPE

