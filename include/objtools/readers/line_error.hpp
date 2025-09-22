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
#include <objtools/logging/message.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <serial/serialbase.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJUTIL_EXPORT ILineError : public IObjtoolsMessage
//  ============================================================================
{
public:

    using TLineNum = unsigned long;

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
        eProblem_IncompleteFeature,
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
        eProblem_DiscouragedFeatureName,
        eProblem_DiscouragedQualifierName,
        eProblem_InvalidQualifier,
        eProblem_InconsistentQualifiers, // qualifiers should match across features, but don't
        eProblem_DuplicateIDs,

        // vcf specific
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
    virtual ILineError* Clone(void) const = 0;

    virtual ~ILineError(void) noexcept {}

    virtual EProblem
    Problem(void) const = 0;

    virtual const string& SeqId(void) const = 0;

    virtual TLineNum Line(void) const = 0;

    typedef vector<TLineNum> TVecOfLines;

    virtual const TVecOfLines& OtherLines(void) const = 0;

    virtual const string& FeatureName(void) const = 0;

    virtual const string& QualifierName(void) const = 0;

    virtual const string& QualifierValue(void) const = 0;

    // combines the other fields to print a reasonable error message
    virtual string Message() const;

    string SeverityStr() const;

    virtual const string& ErrorMessage() const;

    virtual string ProblemStr() const;

    static string ProblemStr(EProblem eProblem);

    virtual void Write(CNcbiOstream& out) const;

    // dump the XML on one line since some tools assume that
    virtual void WriteAsXML(CNcbiOstream& out) const;

    // IMessage methods - default implementations or wrappers for ILineError
    // methods (for backward compatibility).
    virtual string      GetText(void) const { return Message(); }
    virtual EDiagSev    GetSeverity(void) const { return Severity(); }
    virtual int         GetCode(void) const { return 0; }
    virtual int         GetSubCode(void) const { return 0; }
    virtual void        Dump(CNcbiOstream& out) const { Write(out); }
    virtual std::string Compose(void) const
    {
        return Message();
    }

    // This is required to disambiguate IMessage::GetSeverity() and
    // CException::GetSeverity() in CObjReaderLineException
    virtual EDiagSev Severity(void) const { return eDiag_Error; }

    virtual void DumpAsXML(CNcbiOstream& out) const { WriteAsXML(out); }
};

//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CLineError :
    //  ============================================================================
    public ILineError
{
public:
    /// Use this because the constructor is protected.
    ///
    /// @returns
    ///   Caller is responsible for the return value.
    static CLineError* Create(
        EProblem           eProblem,
        EDiagSev           eSeverity,
        const std::string& strSeqId,
        TLineNum       uLine,
        const std::string& strFeatureName    = string(""),
        const std::string& strQualifierName  = string(""),
        const std::string& strQualifierValue = string(""),
        const std::string& strErrorMessage   = string(""),
        const TVecOfLines& vecOfOtherLines   = TVecOfLines());

    /// Use this because copy ctor is protected.
    virtual ILineError* Clone(void) const;

    virtual ~CLineError(void) noexcept {}

    /// copy constructor is protected so please use this function to
    /// throw the object.
    NCBI_NORETURN void Throw(void) const;

    void PatchLineNumber(
        TLineNum uLine) { m_uLine = uLine; };

    void PatchErrorMessage(
        const string& errorMessage)
    {
        m_strErrorMessage = errorMessage;
    };

    // "OtherLines" not set in ctor because it's
    // use should be somewhat rare
    void AddOtherLine(TLineNum uOtherLine)
    {
        m_vecOfOtherLines.push_back(uOtherLine);
    }

    EProblem
    Problem(void) const { return m_eProblem; }

    virtual EDiagSev
    Severity(void) const { return m_eSeverity; }

    const std::string&
    SeqId(void) const { return m_strSeqId; }

    TLineNum
    Line(void) const { return m_uLine; }

    const TVecOfLines&
    OtherLines(void) const { return m_vecOfOtherLines; }

    const std::string&
    FeatureName(void) const { return m_strFeatureName; }

    const std::string&
    QualifierName(void) const { return m_strQualifierName; }

    const std::string&
    QualifierValue(void) const { return m_strQualifierValue; }

    virtual std::string
    ProblemStr(void) const
    {
        if (const auto& msg = ErrorMessage(); ! msg.empty()) {
            return msg;
        }
        return ILineError::ProblemStr(Problem());
    }

    const std::string& ErrorMessage(void) const { return m_strErrorMessage; }

protected:
    EProblem     m_eProblem;
    EDiagSev     m_eSeverity;
    std::string  m_strSeqId;
    TLineNum m_uLine;
    std::string  m_strFeatureName;
    std::string  m_strQualifierName;
    std::string  m_strQualifierValue;
    std::string  m_strErrorMessage;
    TVecOfLines  m_vecOfOtherLines;

    /// protected instead of public.  Please use the Create function instead.
    CLineError(
        EProblem           eProblem,
        EDiagSev           eSeverity,
        const std::string& strSeqId,
        TLineNum       uLine,
        const std::string& strFeatureName,
        const std::string& strQualifierName,
        const std::string& strQualifierValue,
        const std::string& strErrorMessage,
        const TVecOfLines& m_vecOfOtherLine);

    /// protected instead of public.  Please use the Throw function to throw
    /// this exception and try to avoid using the copy constructor at all.
    CLineError(const CLineError& rhs);
};


//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CLineErrorEx :
    //  ============================================================================
    public ILineError
{
public:
    /// Use this because the constructor is protected.
    ///
    /// @returns
    ///   Caller is responsible for the return value.
    static CLineErrorEx* Create(
        EProblem           eProblem,
        EDiagSev           eSeverity,
        int                code,
        int                subcode,
        const std::string& strSeqId,
        TLineNum       uLine,
        const std::string& strErrorMessage   = string(""),
        const std::string& strFeatureName    = string(""),
        const std::string& strQualifierName  = string(""),
        const std::string& strQualifierValue = string(""),
        const TVecOfLines& vecOfOtherLines   = TVecOfLines());

    /// Use this because copy ctor is protected.
    virtual ILineError* Clone(void) const override;

    virtual ~CLineErrorEx(void) {}

    /// copy constructor is protected so please use this function to
    /// throw the object.
    NCBI_NORETURN void Throw(void) const;

    void PatchLineNumber(
        TLineNum uLine) { m_uLine = uLine; };

    // "OtherLines" not set in ctor because it's
    // use should be somewhat rare
    void AddOtherLine(TLineNum uOtherLine)
    {
        m_vecOfOtherLines.push_back(uOtherLine);
    }

    EProblem
    Problem(void) const override { return m_eProblem; }

    virtual EDiagSev
    Severity(void) const override { return m_eSeverity; }

    const std::string&
    SeqId(void) const override { return m_strSeqId; }

    TLineNum
    Line(void) const override { return m_uLine; }

    const TVecOfLines&
    OtherLines(void) const override { return m_vecOfOtherLines; }

    const std::string&
    FeatureName(void) const override { return m_strFeatureName; }

    const std::string&
    QualifierName(void) const override { return m_strQualifierName; }

    const std::string&
    QualifierValue(void) const override { return m_strQualifierValue; }

    virtual std::string
    ProblemStr(void) const override
    {
        if (m_eProblem == ILineError::eProblem_GeneralParsingError &&
            ! ErrorMessage().empty()) {
            return ErrorMessage();
        }
        return ILineError::ProblemStr(Problem());
    }

    const std::string& ErrorMessage(void) const override { return m_strErrorMessage; }

    virtual string Message(void) const override
    {
        return (m_strErrorMessage.empty() ? ILineError::Message() : m_strErrorMessage);
    }

    virtual int GetCode(void) const override { return m_Code; };
    virtual int GetSubCode(void) const override { return m_Subcode; }

protected:
    EProblem     m_eProblem;
    EDiagSev     m_eSeverity;
    int          m_Code;
    int          m_Subcode;
    std::string  m_strSeqId;
    TLineNum m_uLine;
    std::string  m_strFeatureName;
    std::string  m_strQualifierName;
    std::string  m_strQualifierValue;
    std::string  m_strErrorMessage;
    TVecOfLines  m_vecOfOtherLines;

    /// protected instead of public.  Please use the Create function instead.
    CLineErrorEx(
        EProblem           eProblem,
        EDiagSev           eSeverity,
        int                code,
        int                subcode,
        const std::string& strSeqId,
        TLineNum       uLine,
        const std::string& strErrorMessage,
        const std::string& strFeatureName,
        const std::string& strQualifierName,
        const std::string& strQualifierValue,
        const TVecOfLines& m_vecOfOtherLine);

    /// protected instead of public.  Please use the Throw function to throw
    /// this exception and try to avoid using the copy constructor at all.
    CLineErrorEx(const CLineErrorEx& rhs);
};


//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CObjReaderLineException
    //  ============================================================================
    // must inherit from ILineError first due to the way CObject detects
    // whether or not it's on the heap.
    : public ILineError,
      public CObjReaderParseException
{
public:
    using CObjReaderParseException::EErrCode;

    /// Please use this instead of the constructor because the ctor
    /// is protected.
    ///
    /// @returns
    ///   Caller is responsible for the return value.
    static CObjReaderLineException* Create(
        EDiagSev                          eSeverity,
        TLineNum                      uLine,
        const std::string&                strMessage,
        EProblem                          eProblem          = eProblem_GeneralParsingError,
        const std::string&                strSeqId          = string(""),
        const std::string&                strFeatureName    = string(""),
        const std::string&                strQualifierName  = string(""),
        const std::string&                strQualifierValue = string(""),
        CObjReaderLineException::EErrCode eErrCode          = eFormat,
        const TVecOfLines&                vecOfOtherLines   = TVecOfLines());

    /// Use instead of copy constructor, which is private.
    virtual ILineError* Clone(void) const;

    // Copy constructor is private, so please use
    /// this function to throw this object.
    NCBI_NORETURN void Throw(void) const;

    ~CObjReaderLineException(void) noexcept {}

    TErrCode GetErrCode(void) const
    {
        return (TErrCode)this->x_GetErrCode();
    }

    EProblem           Problem(void) const { return m_eProblem; }
    const std::string& SeqId(void) const { return m_strSeqId; }
    EDiagSev           Severity(void) const { return CObjReaderParseException::GetSeverity(); }
    TLineNum       Line(void) const { return m_uLineNumber; }
    const TVecOfLines& OtherLines(void) const { return m_vecOfOtherLines; }
    const std::string& FeatureName(void) const { return m_strFeatureName; }
    const std::string& QualifierName(void) const { return m_strQualifierName; }
    const std::string& QualifierValue(void) const { return m_strQualifierValue; }

    const std::string& ErrorMessage(void) const { return m_strErrorMessage; }

    std::string ProblemStr() const;

    std::string Message() const { return (GetMsg().empty() ? ILineError::Message() : GetMsg()); }

    //
    //  Cludge alert: The line number may not be known at the time the exception
    //  is generated. In that case, the exception will be fixed up before being
    //  rethrown.
    //
    void
    SetLineNumber(
        TLineNum uLineNumber) { m_uLineNumber = uLineNumber; }

    // "OtherLines" not set in ctor because it's
    // use should be somewhat rare
    void AddOtherLine(TLineNum uOtherLine)
    {
        m_vecOfOtherLines.push_back(uOtherLine);
    }

    void SetObject(CRef<CSerialObject> pObject);

    CConstRef<CSerialObject> GetObject() const;

private:
    EProblem            m_eProblem;
    std::string         m_strSeqId;
    TLineNum        m_uLineNumber;
    std::string         m_strFeatureName;
    std::string         m_strQualifierName;
    std::string         m_strQualifierValue;
    std::string         m_strErrorMessage;
    TVecOfLines         m_vecOfOtherLines;
    CRef<CSerialObject> m_pObject;

    /// private instead of public.  Please use the Create function instead.
    CObjReaderLineException(
        EDiagSev                          eSeverity,
        TLineNum                      uLine,
        const std::string&                strMessage,
        EProblem                          eProblem          = eProblem_GeneralParsingError,
        const std::string&                strSeqId          = string(""),
        const std::string&                strFeatureName    = string(""),
        const std::string&                strQualifierName  = string(""),
        const std::string&                strQualifierValue = string(""),
        CObjReaderLineException::EErrCode eErrCode          = eFormat,
        const TVecOfLines&                vecOfOtherLines   = TVecOfLines());

    /// Private, so use Clone or Throw instead.
    CObjReaderLineException(const CObjReaderLineException& rhs);
};


END_SCOPE(objects)

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___LINEERROR__HPP
