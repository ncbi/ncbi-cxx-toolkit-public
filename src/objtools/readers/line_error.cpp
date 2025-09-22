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
    EProblem           eProblem,
    EDiagSev           eSeverity,
    const std::string& strSeqId,
    TLineNum           uLine,
    const std::string& strFeatureName,
    const std::string& strQualifierName,
    const std::string& strQualifierValue,
    const std::string& strErrorMessage,
    const TVecOfLines& vecOfOtherLines)
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

void CLineError::Throw(void) const
{
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    throw *this;
}

CLineError::CLineError(
    EProblem           eProblem,
    EDiagSev           eSeverity,
    const std::string& strSeqId,
    TLineNum           uLine,
    const std::string& strFeatureName,
    const std::string& strQualifierName,
    const std::string& strQualifierValue,
    const std::string& strErrorMessage,
    const TVecOfLines& vecOfOtherLines) :
    m_eProblem(eProblem),
    m_eSeverity(eSeverity),
    m_strSeqId(strSeqId),
    m_uLine(uLine),
    m_strFeatureName(strFeatureName),
    m_strQualifierName(strQualifierName),
    m_strQualifierValue(strQualifierValue),
    m_strErrorMessage(strErrorMessage),
    m_vecOfOtherLines(vecOfOtherLines)
{
}

CLineError::CLineError(const CLineError& rhs) :
    m_eProblem(rhs.m_eProblem),
    m_eSeverity(rhs.m_eSeverity),
    m_strSeqId(rhs.m_strSeqId),
    m_uLine(rhs.m_uLine),
    m_strFeatureName(rhs.m_strFeatureName),
    m_strQualifierName(rhs.m_strQualifierName),
    m_strQualifierValue(rhs.m_strQualifierValue),
    m_strErrorMessage(rhs.m_strErrorMessage),
    m_vecOfOtherLines(rhs.m_vecOfOtherLines)
{
}


string ILineError::Message() const
{
    ostringstream result;
    result << "On SeqId '" << SeqId() << "', line " << Line() << ", severity " << SeverityStr() << ": '"
           << ProblemStr() << "'";
    if (! FeatureName().empty()) {
        result << ", with feature name '" << FeatureName() << "'";
    }
    if (! QualifierName().empty()) {
        result << ", with qualifier name '" << QualifierName() << "'";
    }
    if (! QualifierValue().empty()) {
        result << ", with qualifier value '" << QualifierValue() << "'";
    }
    if (! OtherLines().empty()) {
        result << ", with other possibly relevant line(s):";
        ITERATE (TVecOfLines, line_it, OtherLines()) {
            result << ' ' << *line_it;
        }
    }
    return result.str();
}


string ILineError::SeverityStr() const
{
    return CNcbiDiag::SeverityName(Severity());
}


const string& ILineError::ErrorMessage() const
{
    return kEmptyStr;
}


string ILineError::ProblemStr() const
{
    return ProblemStr(Problem());
}


std::string
ILineError::ProblemStr(EProblem eProblem)
{
    switch (eProblem) {
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
    case eProblem_QualifierBadValue:
        return "Qualifier had bad value";
    case eProblem_BadScoreValue:
        return "Invalid score value";
    case eProblem_MissingContext:
        return "Value ignored due to missing context";
    case eProblem_BadTrackLine:
        return "Bad track line: Expected \"track key1=value1 key2=value2 ...\"";
    case eProblem_InternalPartialsInFeatLocation:
        return "Feature's location has internal partials";
    case eProblem_FeatMustBeInXrefdGene:
        return "Feature has xref to a gene, but that gene does NOT contain the feature.";
    case eProblem_CreatedGeneFromMultipleFeats:
        return "Feature is trying to create a gene that conflicts with the gene created by another feature.";
    case eProblem_UnrecognizedSquareBracketCommand:
        return "Unrecognized square bracket command";
    case eProblem_TooLong:
        return "Feature is too long";
    case eProblem_UnexpectedNucResidues:
        return "Nucleotide residues unexpectedly found in feature";
    case eProblem_UnexpectedAminoAcids:
        return "Amino acid residues unexpectedly found in feature";
    case eProblem_TooManyAmbiguousResidues:
        return "Too many ambiguous residues";
    case eProblem_InvalidResidue:
        return "Invalid residue(s)";
    case eProblem_ModifierFoundButNoneExpected:
        return "Modifiers were found where none were expected";
    case eProblem_ExtraModifierFound:
        return "Extraneous modifiers found";
    case eProblem_ExpectedModifierMissing:
        return "Expected modifier missing";
    case eProblem_Missing:
        return "Feature is missing";
    case eProblem_NonPositiveLength:
        return "Feature's length must be greater than zero.";
    case eProblem_ParsingModifiers:
        return "Could not parse modifiers.";
    case eProblem_ContradictoryModifiers:
        return "Multiple different values for modifier";
    case eProblem_InvalidLengthAutoCorrected:
        return "Feature had invalid length, but this was automatically corrected.";
    case eProblem_IgnoredResidue:
        return "An invalid residue has been ignored";
    case eProblem_InvalidQualifier:
        return "Invalid qualifier for feature";

    case eProblem_BadInfoLine:
        return "Broken ##INFO line";
    case eProblem_BadFormatLine:
        return "Broken ##FORMAT line";
    case eProblem_BadFilterLine:
        return "Broken ##FILTER line";

    case eProblem_ProgressInfo:
        return "Just a progress info message (no error)";
    default:
        return "Unknown problem";
    }
}


void ILineError::Write(
    CNcbiOstream& out) const
{

    out << "                " << SeverityStr() << ":" << endl;
    out << "Problem:        " << ProblemStr() << endl;
    if (GetCode()) {
        out << "Code:           " << GetCode();
        if (GetSubCode()) {
            out << "." << GetSubCode();
        }
        out << endl;
    }
    const string& seqid = SeqId();
    if (! seqid.empty()) {
        out << "SeqId:          " << seqid << endl;
    }
    if (Line()) {
        out << "Line:           " << Line() << endl;
    }
    const string& feature = FeatureName();
    if (! feature.empty()) {
        out << "FeatureName:    " << feature << endl;
    }
    const string& qualname = QualifierName();
    if (! qualname.empty()) {
        out << "QualifierName:  " << qualname << endl;
    }
    const string& qualval = QualifierValue();
    if (! qualval.empty()) {
        out << "QualifierValue: " << qualval << endl;
    }
    const TVecOfLines& vecOfLines = OtherLines();
    if (! vecOfLines.empty()) {
        out << "OtherLines:";
        ITERATE (TVecOfLines, line_it, vecOfLines) {
            out << ' ' << *line_it;
        }
        out << endl;
    }
    out << endl;
}


void ILineError::WriteAsXML(
    CNcbiOstream& out) const
{
    out << "<message severity=\"" << NStr::XmlEncode(SeverityStr()) << "\" "
        << "problem=\"" << NStr::XmlEncode(ProblemStr()) << "\" ";
    if (GetCode()) {
        string code = NStr::IntToString(GetCode());
        if (GetSubCode()) {
            code += "." + NStr::IntToString(GetSubCode());
        }
        out << "code=\"" << NStr::XmlEncode(code) << "\" ";
    }
    const string& seqid = SeqId();
    if (! seqid.empty()) {
        out << "seqid=\"" << NStr::XmlEncode(seqid) << "\" ";
    }
    out << "line=\"" << Line() << "\" ";
    const string& feature = FeatureName();
    if (! feature.empty()) {
        out << "feature_name=\"" << NStr::XmlEncode(feature) << "\" ";
    }
    const string& qualname = QualifierName();
    if (! qualname.empty()) {
        out << "qualifier_name=\"" << NStr::XmlEncode(qualname) << "\" ";
    }
    const string& qualval = QualifierValue();
    if (! qualval.empty()) {
        out << "qualifier_value=\"" << NStr::XmlEncode(qualval) << "\" ";
    }
    out << ">";

    // child nodes
    ITERATE (TVecOfLines, line_it, OtherLines()) {
        out << "<other_line>" << *line_it << "</other_line>";
    }

    out << "</message>" << endl;
}


ILineError* CLineError::Clone() const
{
    return new CLineError(*this);
}

CLineErrorEx* CLineErrorEx::Create(
    EProblem           eProblem,
    EDiagSev           eSeverity,
    int                code,
    int                subcode,
    const std::string& strSeqId,
    TLineNum           uLine,
    const std::string& strErrorMessage,
    const std::string& strFeatureName,
    const std::string& strQualifierName,
    const std::string& strQualifierValue,
    const TVecOfLines& vecOfOtherLines)
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


void CLineErrorEx::Throw(void) const
{
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    throw *this;
}


CLineErrorEx::CLineErrorEx(
    EProblem           eProblem,
    EDiagSev           eSeverity,
    int                code,
    int                subcode,
    const std::string& strSeqId,
    TLineNum           uLine,
    const std::string& strErrorMessage,
    const std::string& strFeatureName,
    const std::string& strQualifierName,
    const std::string& strQualifierValue,
    const TVecOfLines& vecOfOtherLines) :
    m_eProblem(eProblem),
    m_eSeverity(eSeverity),
    m_Code(code),
    m_Subcode(subcode),
    m_strSeqId(strSeqId),
    m_uLine(uLine),
    m_strFeatureName(strFeatureName),
    m_strQualifierName(strQualifierName),
    m_strQualifierValue(strQualifierValue),
    m_strErrorMessage(strErrorMessage),
    m_vecOfOtherLines(vecOfOtherLines)
{
}


CLineErrorEx::CLineErrorEx(const CLineErrorEx& rhs) :
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
{
}


ILineError* CLineErrorEx::Clone(void) const
{
    return new CLineErrorEx(*this);
}


CObjReaderLineException*
CObjReaderLineException::Create(
    EDiagSev                          eSeverity,
    TLineNum                          uLine,
    const std::string&                strMessage,
    EProblem                          eProblem,
    const std::string&                strSeqId,
    const std::string&                strFeatureName,
    const std::string&                strQualifierName,
    const std::string&                strQualifierValue,
    CObjReaderLineException::EErrCode eErrCode,
    const TVecOfLines&                vecOfOtherLines)
{
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    return new CObjReaderLineException(
        eSeverity, uLine, strMessage, eProblem, strSeqId, strFeatureName, strQualifierName, strQualifierValue, eErrCode, vecOfOtherLines);
}

ILineError* CObjReaderLineException::Clone(void) const
{
    return new CObjReaderLineException(*this);
}

void CObjReaderLineException::Throw(void) const
{
    // this triggers a deprecated-call warning, which should disappear
    // once the constructors become protected instead of deprecated.
    throw *this;
}

CObjReaderLineException::CObjReaderLineException(
    EDiagSev                          eSeverity,
    TLineNum                          uLine,
    const std::string&                strMessage,
    EProblem                          eProblem,
    const std::string&                strSeqId,
    const std::string&                strFeatureName,
    const std::string&                strQualifierName,
    const std::string&                strQualifierValue,
    CObjReaderLineException::EErrCode eErrCode,
    const TVecOfLines&                vecOfOtherLines) :
    CObjReaderParseException(DIAG_COMPILE_INFO, 0, static_cast<CObjReaderParseException::EErrCode>(CException::eInvalid), strMessage, uLine, eDiag_Info),
    m_eProblem(eProblem),
    m_strSeqId(strSeqId),
    m_uLineNumber(uLine),
    m_strFeatureName(strFeatureName),
    m_strQualifierName(strQualifierName),
    m_strQualifierValue(strQualifierValue),
    m_strErrorMessage(strMessage),
    m_vecOfOtherLines(vecOfOtherLines)
{
    SetSeverity(eSeverity);
    x_InitErrCode(static_cast<CException::EErrCode>(eErrCode));
}

CObjReaderLineException::CObjReaderLineException(const CObjReaderLineException& rhs) :
    CObjReaderParseException(rhs),
    m_eProblem(rhs.Problem()),
    m_strSeqId(rhs.SeqId()),
    m_uLineNumber(rhs.Line()),
    m_strFeatureName(rhs.FeatureName()),
    m_strQualifierName(rhs.QualifierName()),
    m_strQualifierValue(rhs.QualifierValue()),
    m_strErrorMessage(rhs.ErrorMessage()),
    m_vecOfOtherLines(rhs.m_vecOfOtherLines),
    m_pObject(rhs.m_pObject)
{
    SetSeverity(rhs.Severity());
    x_InitErrCode(static_cast<CException::EErrCode>(rhs.x_GetErrCode()));
}

// this is here because of an apparent compiler bug
std::string
CObjReaderLineException::ProblemStr() const
{
    if (! m_strErrorMessage.empty()) {
        return m_strErrorMessage;
    }
    return ILineError::ProblemStr();
}

void CObjReaderLineException::SetObject(CRef<CSerialObject> pObject)
{
    m_pObject = pObject;
}

CConstRef<CSerialObject> CObjReaderLineException::GetObject() const
{
    return m_pObject;
}

END_SCOPE(objects)
END_NCBI_SCOPE
