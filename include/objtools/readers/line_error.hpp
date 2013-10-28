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
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/readers/reader_exception.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJUTIL_EXPORT ILineError
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
        eProblem_QualifierBadValue,
        eProblem_BadScoreValue,
        eProblem_MissingContext,
        eProblem_BadTrackLine,
        eProblem_InternalPartialsInFeatLocation,
        eProblem_FeatMustBeInXrefdGene,
        eProblem_CreatedGeneFromMultipleFeats,
        eProblem_UnrecognizedSquareBracketCommand,
        eProblem_TooLong,
        eProblem_UnexpectedNucResidues,
        eProblem_UnexpectedAminoAcids,
        eProblem_TooManyAmbiguousResidues,
        eProblem_InvalidResidue,
        eProblem_ModifierFoundButNoneExpected,
        eProblem_ExtraModifierFound,
        eProblem_ExpectedModifierMissing,
        eProblem_Missing,
        eProblem_NonPositiveLength,
        eProblem_ParsingModifiers,
        eProblem_ContradictoryModifiers,
        eProblem_InvalidLengthAutoCorrected, // covers more general cases than eProblem_NonPositiveLength
        eProblem_IgnoredResidue,

        //vcf specific
        eProblem_BadInfoLine,
        eProblem_BadFormatLine,
        eProblem_BadFilterLine,

        // not a problem, actually, but used as
        // the default if PutProgress is
        // not overridden.
        eProblem_ProgressInfo, 

        eProblem_GeneralParsingError
    };

    /// This is here because the copy constructor may be protected
    /// eventually.
    virtual ILineError *Clone(void) const {
        /// throw instead of making it pure virtual for backward
        /// compatibility.
        NCBI_USER_THROW("not implemented: ILineError::Clone");
    }

    virtual ~ILineError(void) throw() {}

    virtual EProblem
    Problem(void) const = 0;

    virtual EDiagSev
    Severity(void) const =0;
    
    virtual const std::string &
    SeqId(void) const = 0;

    virtual unsigned int
    Line(void) const =0;

    typedef vector<unsigned int> TVecOfLines;
    virtual const TVecOfLines &
    OtherLines(void) const = 0;

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
        if( ! OtherLines().empty() ) {
            result << ", with other possibly relevant line(s):";
            ITERATE( TVecOfLines, line_it, OtherLines() ) {
                result << ' ' << *line_it;
            }
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

    virtual const std::string&
    ErrorMessage(void) const { 
        static string empty("");
        return empty; 
    }

    virtual std::string
    ProblemStr(void) const
    {
        return ProblemStr(Problem());
    }

    static
    std::string
    ProblemStr(EProblem eProblem)
    {
        switch(eProblem) {
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

    virtual void Dump( 
        std::ostream& out ) const
    {
        out << "                " << SeverityStr() << ":" << endl;
        out << "Problem:        " << ProblemStr() << endl;
        const string & seqid = SeqId();
        if (!seqid.empty()) {
            out << "SeqId:          " << seqid << endl;
        }
        out << "Line:           " << Line() << endl;
        const string & feature = FeatureName();
        if (!feature.empty()) {
            out << "FeatureName:    " << feature << endl;
        }
        const string & qualname = QualifierName();
        if (!qualname.empty()) {
            out << "QualifierName:  " << qualname << endl;
        }
        const string & qualval = QualifierValue();
        if (!qualval.empty()) {
            out << "QualifierValue: " << qualval << endl;
        }
        const TVecOfLines & vecOfLines = OtherLines();
        if( ! vecOfLines.empty() ) {
            out << "OtherLines:";
            ITERATE(TVecOfLines, line_it, vecOfLines) {
                out << ' ' << *line_it;
            }
            out << endl;
        }
        out << endl;
    };

    // dump the XML on one line since some tools assume that
    virtual void DumpAsXML(
        std::ostream& out ) const
    {
        out << "<message severity=\"" << NStr::XmlEncode(SeverityStr()) << "\" "
            << "problem=\"" << NStr::XmlEncode(ProblemStr()) << "\" ";
        const string & seqid = SeqId();
        if (!seqid.empty()) {
            out << "seqid\"" << NStr::XmlEncode(seqid) << "\" ";
        }
        out << "line=\"" << Line() << "\" ";
        const string & feature = FeatureName();
        if (!feature.empty()) {
            out << "feature_name=\"" << NStr::XmlEncode(feature) << "\" ";
        }
        const string & qualname = QualifierName();
        if (!qualname.empty()) {
            out << "qualifier_name=\"" << NStr::XmlEncode(qualname) << "\" ";
        }
        const string & qualval = QualifierValue();
        if (!qualval.empty()) {
            out << "qualifier_value=\"" << NStr::XmlEncode(qualval) << "\" ";
        }
        out << ">";

        // child nodes
        ITERATE(TVecOfLines, line_it, OtherLines()) {
            out << "<other_line>" << *line_it << "</other_line>";
        }

        out << "</message>" << endl;
    };
};
    
//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CLineError:
//  ============================================================================
    public ILineError
{
public:

    /// The CLineError constructor is deprecated and will be removed at some
    /// point so please use this instead.
    ///
    /// @returns
    ///   Caller is responsible for the return value.
    static CLineError* Create(
        EProblem eProblem,
        EDiagSev eSeverity,
        const std::string& strSeqId,
        unsigned int uLine,
        const std::string & strFeatureName = string(""),
        const std::string & strQualifierName = string(""),
        const std::string & strQualifierValue = string(""),
        const std::string & strErrorMessage = string(""),
        const TVecOfLines & vecOfOtherLines = TVecOfLines() );

    /// This is marked deprecated because it will eventually become
    /// protected instead of public.  Please use the Create function instead.
    NCBI_DEPRECATED_CTOR(CLineError(
        EProblem eProblem,
        EDiagSev eSeverity,
        const std::string& strSeqId,
        unsigned int uLine,
        const std::string & strFeatureName,
        const std::string & strQualifierName,
        const std::string & strQualifierValue,
        const std::string & strErrorMessage,
        const TVecOfLines & m_vecOfOtherLine));

    /// This is marked deprecated because it will eventually become
    /// protected instead of public.  Please use the Throw function to throw
    /// this exception and try to avoid using the copy constructor at all.
    NCBI_DEPRECATED_CTOR(CLineError(const CLineError & rhs ));

    virtual ILineError *Clone(void) const;

    virtual ~CLineError(void) throw() {}

    /// copy constructor will be hidden some day, so please use this function to
    /// make this class throw.
    NCBI_NORETURN void Throw(void) const;
       
    void PatchLineNumber(
        unsigned int uLine) { m_uLine = uLine; };

    // "OtherLines" not set in ctor because it's
    // use should be somewhat rare
    void AddOtherLine(unsigned int uOtherLine) {
        m_vecOfOtherLines.push_back(uOtherLine);
    }
 
    EProblem
    Problem(void) const { return m_eProblem; }

    EDiagSev
    Severity(void) const { return m_eSeverity; }
    
    const std::string &
    SeqId(void) const { return m_strSeqId; }

    unsigned int
    Line(void) const { return m_uLine; }

    const TVecOfLines &
    OtherLines(void) const { return m_vecOfOtherLines; }

    const std::string &
    FeatureName(void) const { return m_strFeatureName; }
    
    const std::string &
    QualifierName(void) const { return m_strQualifierName; }

    const std::string &
    QualifierValue(void) const { return m_strQualifierValue; }
        
    virtual std::string
    ProblemStr(void) const
    {
        if (m_eProblem == ILineError::eProblem_GeneralParsingError  &&
                !ErrorMessage().empty()) {
            return ErrorMessage();
        }
        return ILineError::ProblemStr(Problem());
    }

    const std::string &ErrorMessage(void) const { return m_strErrorMessage; }

protected:

    EProblem m_eProblem;
    EDiagSev m_eSeverity;
    std::string m_strSeqId;
    unsigned int m_uLine;
    std::string m_strFeatureName;
    std::string m_strQualifierName;
    std::string m_strQualifierValue;
    std::string m_strErrorMessage;
    TVecOfLines m_vecOfOtherLines;
};

//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CObjReaderLineException
//  ============================================================================
    // must inherit from ILineError first due to the way CObject detects
    // whether or not it's on the heap.
    : public ILineError, public CObjReaderParseException
{
public:

    using CObjReaderParseException::EErrCode;

    /// Please use this instead of the constructor because the ctor
    /// will be moved from public to protected at some point.
    ///
    /// @returns
    ///   Caller is responsible for the return value.
    static CObjReaderLineException* Create(
        EDiagSev eSeverity,
        unsigned int uLine,
        const std::string &strMessage,
        EProblem eProblem = eProblem_GeneralParsingError,
        const std::string& strSeqId = string(""),
        const std::string & strFeatureName = string(""),
        const std::string & strQualifierName = string(""),
        const std::string & strQualifierValue = string(""),
        CObjReaderLineException::EErrCode eErrCode = eFormat,
        const TVecOfLines & vecOfOtherLines = TVecOfLines()
        );

    virtual ILineError *Clone(void) const;

    // Copy constructor will eventually become protected, so please use 
    /// this function to make this function to throw.
    NCBI_NORETURN void Throw(void) const;

    ~CObjReaderLineException(void) throw() { }

    /// This is marked deprecated because it will eventually become
    /// protected instead of public.  Please use the Create function instead.
    NCBI_DEPRECATED_CTOR(
    CObjReaderLineException(
        EDiagSev eSeverity,
        unsigned int uLine,
        const std::string &strMessage,
        EProblem eProblem = eProblem_GeneralParsingError,
        const std::string& strSeqId = string(""),
        const std::string & strFeatureName = string(""),
        const std::string & strQualifierName = string(""),
        const std::string & strQualifierValue = string(""),
        CObjReaderLineException::EErrCode eErrCode = eFormat,
        const TVecOfLines & vecOfOtherLines = TVecOfLines()
        ));

    /// This will become protected at some point in the future, so please
    /// avoid using it.  To throw this exception, use the Throw function
    NCBI_DEPRECATED_CTOR(
    CObjReaderLineException(const CObjReaderLineException & rhs ));

    TErrCode GetErrCode(void) const 
    { 
        return (TErrCode) this->x_GetErrCode();
    } 

    EProblem Problem(void) const { return m_eProblem; }
    const std::string &SeqId(void) const { return m_strSeqId; }
    EDiagSev Severity(void) const { return GetSeverity(); }
    unsigned int Line(void) const { return m_uLineNumber; }
    const TVecOfLines & OtherLines(void) const { return m_vecOfOtherLines; }
    const std::string &FeatureName(void) const { return m_strFeatureName; }
    const std::string &QualifierName(void) const { return m_strQualifierName; }
    const std::string &QualifierValue(void) const { return m_strQualifierValue; }

    const std::string &ErrorMessage(void) const { return m_strErrorMessage; }

    std::string ProblemStr() const;

    std::string Message() const { return ( GetMsg().empty() ? ILineError::Message() : GetMsg()); }
    
    //
    //  Cludge alert: The line number may not be known at the time the exception
    //  is generated. In that case, the exception will be fixed up before being
    //  rethrown.
    //
    void 
    SetLineNumber(
        unsigned int uLineNumber ) { m_uLineNumber = uLineNumber; }

    // "OtherLines" not set in ctor because it's
    // use should be somewhat rare
    void AddOtherLine(unsigned int uOtherLine) {
        m_vecOfOtherLines.push_back(uOtherLine);
    }

protected:

    EProblem m_eProblem;
    std::string m_strSeqId;
    unsigned int m_uLineNumber;
    std::string m_strFeatureName;
    std::string m_strQualifierName;
    std::string m_strQualifierValue;
    std::string m_strErrorMessage;
    TVecOfLines m_vecOfOtherLines;
};

    
END_SCOPE(objects)

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___LINEERROR__HPP
