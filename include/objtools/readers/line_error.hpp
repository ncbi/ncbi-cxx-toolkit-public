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
 * Author: Frank Ludwig
 *
 * File Description:
 *   Basic reader interface
 *
 */

#ifndef OBJTOOLS_READERS___LINEERROR__HPP
#define OBJTOOLS_READERS___LINEERROR__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/reader_exception.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class ILineError
//  ============================================================================
{
public:
    // If you add to here, make sure to add to ProblemStr()
    enum EProblem {
        // useful when you have a problem variable, but haven't found a problem yet
        eProblem_Unset = 1, 

        eProblem_UnrecognizedFeatureName,
        eProblem_UnrecognizedQualifierName,
        eProblem_NumericQualifierValueHasExtraTrailingCharacters,
        eProblem_NumericQualifierValueIsNotANumber,
        eProblem_FeatureNameNotAllowed,
        eProblem_NoFeatureProvidedOnIntervals,
        eProblem_QualifierWithoutFeature,
        eProblem_FeatureBadStartAndOrStop,
        eProblem_BadFeatureInterval,
        eProblem_BadScoreValue,
        eProblem_MissingContext,

        eProblem_GeneralParsingError
    };

    virtual ~ILineError(void) throw() {}

    virtual EProblem
    Problem(void) const = 0;

    virtual EDiagSev
    Severity(void) const =0;
    
    virtual const std::string &
    SeqId(void) const = 0;

    virtual unsigned int
    Line(void) const =0;

    virtual const std::string &
    FeatureName(void) const = 0;
    
    virtual const std::string &
    QualifierName(void) const = 0;

    virtual const std::string &
    QualifierValue(void) const = 0;

    // combines the other fields to print a reasonable error message
    virtual std::string
    Message(void) const
    {
        CNcbiOstrstream result;
        result << "On SeqId '" << SeqId() << "', line " << Line() << ", severity " << SeverityStr() << ": '"
               << ProblemStr() << "'";
        if( ! FeatureName().empty() ) {
            result << ", with feature name '" << FeatureName() << "'";
        }
        if( ! QualifierName().empty() ) {
            result << ", with qualifier name '" << QualifierName() << "'";
        }
        if( ! QualifierValue().empty() ) {
            result << ", with qualifier value '" << QualifierValue() << "'";
        }
        return (string)CNcbiOstrstreamToString(result);
    }

    std::string
    SeverityStr(void) const
    {
        switch ( Severity() ) {
        default:
            return "Unknown";
        case eDiag_Info:
            return "Info";
        case eDiag_Warning:
            return "Warning";
        case eDiag_Error:
            return "Error";
        case eDiag_Critical:
            return "Critical";
        case eDiag_Fatal:
            return "Fatal";
        }
    };

    std::string
    ProblemStr(void) const
    {
        switch(Problem()) {
        case eProblem_Unset:
            return "Unset";
        case eProblem_UnrecognizedFeatureName:
            return "Unrecognized feature name";
        case eProblem_UnrecognizedQualifierName:
            return "Unrecognized qualifier name";
        case eProblem_NumericQualifierValueHasExtraTrailingCharacters:
            return "Numeric qualifier value has extra trailing characters after the number";
        case eProblem_NumericQualifierValueIsNotANumber:
            return "Numeric qualifier value should be a number";
        case eProblem_FeatureNameNotAllowed:
            return "Feature name not allowed";
        case eProblem_NoFeatureProvidedOnIntervals:
            return "No feature provided on intervals";
        case eProblem_QualifierWithoutFeature:
            return "No feature provided for qualifiers";
        case eProblem_FeatureBadStartAndOrStop:
            return "Feature bad start and/or stop";
        case eProblem_GeneralParsingError:
            return "General parsing error";
        case eProblem_BadFeatureInterval:
            return "Bad feature interval";
        case eProblem_BadScoreValue:
            return "Invalid score value";
        case eProblem_MissingContext:
            return "Value ignored due to missing context";

        default:
            return "Unknown problem";
        }
    }
};
    
//  ============================================================================
class CLineError:
//  ============================================================================
    public ILineError
{
public:
    CLineError(
        EProblem eProblem,
        EDiagSev eSeverity,
        const std::string& strSeqId,
        unsigned int uLine,
        const std::string & strFeatureName = string(""),
        const std::string & strQualifierName = string(""),
        const std::string & strQualifierValue = string("") )
    : m_eProblem(eProblem), m_eSeverity( eSeverity ), m_strSeqId(strSeqId), m_uLine( uLine ), 
      m_strFeatureName(strFeatureName), m_strQualifierName(strQualifierName), 
      m_strQualifierValue(strQualifierValue)
     { }
    
    virtual ~CLineError(void) throw() {}
       
    void PatchLineNumber(
        unsigned int uLine) { m_uLine = uLine; };
 
    EProblem
    Problem(void) const { return m_eProblem; }

    EDiagSev
    Severity(void) const { return m_eSeverity; }
    
    const std::string &
    SeqId(void) const { return m_strSeqId; }

    unsigned int
    Line(void) const { return m_uLine; }

    const std::string &
    FeatureName(void) const { return m_strFeatureName; }
    
    const std::string &
    QualifierName(void) const { return m_strQualifierName; }

    const std::string &
    QualifierValue(void) const { return m_strQualifierValue; }
    
    void Dump( 
        std::ostream& out )
    {
        out << "                " << SeverityStr() << ":" << endl;
        out << "Problem:        " << ProblemStr() << endl;
        string seqid = SeqId();
        if (!seqid.empty()) {
            out << "SeqId:          " << seqid << endl;
        }
        out << "Line:           " << Line() << endl;
        string feature = FeatureName();
        if (!feature.empty()) {
            out << "FeatureName:    " << feature << endl;
        }
        string qualname = QualifierName();
        if (!qualname.empty()) {
            out << "QualifierName:  " << qualname << endl;
        }
        string qualval = QualifierValue();
        if (!qualval.empty()) {
            out << "QualifierValue: " << qualval << endl;
        }
        out << endl;
    };
        
protected:
    EProblem m_eProblem;
    EDiagSev m_eSeverity;
    std::string m_strSeqId;
    unsigned int m_uLine;
    std::string m_strFeatureName;
    std::string m_strQualifierName;
    std::string m_strQualifierValue;
};

//  ============================================================================
class CObjReaderLineException
//  ============================================================================
    : public CObjReaderParseException, public ILineError
{
public:
    CObjReaderLineException(
        EDiagSev eSeverity,
        unsigned int uLine,
        const std::string &strMessage,
        EProblem eProblem = eProblem_GeneralParsingError,
        const std::string& strSeqId = string(""),
        const std::string & strFeatureName = string(""),
        const std::string & strQualifierName = string(""),
        const std::string & strQualifierValue = string("") )
    : CObjReaderParseException( DIAG_COMPILE_INFO, 0, eFormat, strMessage, uLine,
        eDiag_Info ), m_strSeqId(strSeqId), m_uLineNumber(uLine), 
        m_strFeatureName(strFeatureName), m_strQualifierName(strQualifierName), 
        m_strQualifierValue(strQualifierValue)
    {
        SetSeverity( eSeverity );
    };

    ~CObjReaderLineException(void) throw() { }

    EProblem Problem(void) const { return m_eProblem; }
    const std::string &SeqId(void) const { return m_strSeqId; }
    EDiagSev Severity(void) const { return GetSeverity(); }
    unsigned int Line(void) const { return m_uLineNumber; }
    const std::string &FeatureName(void) const { return m_strFeatureName; }
    const std::string &QualifierName(void) const { return m_strQualifierName; }
    const std::string &QualifierValue(void) const { return m_strQualifierValue; }

    std::string Message() const { return GetMsg(); }
    
    //
    //  Cludge alert: The line number may not be known at the time the exception
    //  is generated. In that case, the exception will be fixed up before being
    //  rethrown.
    //
    void 
    SetLineNumber(
        unsigned int uLineNumber ) { m_uLineNumber = uLineNumber; }
        
protected:
    EProblem m_eProblem;
    std::string m_strSeqId;
    unsigned int m_uLineNumber;
    std::string m_strFeatureName;
    std::string m_strQualifierName;
    std::string m_strQualifierValue;
};

    
END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___LINEERROR__HPP
