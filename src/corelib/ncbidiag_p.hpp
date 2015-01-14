#ifndef CORELIB___NCBIDIAG_P__HPP
#define CORELIB___NCBIDIAG_P__HPP

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
 * Author:  Vyacheslav Kononenko
 *
 *
 */

/// @file ncbidiag_p.hpp
///
///   Defines NCBI C++ service classes and functions for diagnostic APIs,
///   classes, and macros.
///
///   More elaborate documentation could be found in:
///     http://www.ncbi.nlm.nih.gov/toolkit/doc/book/ch_core/#ch_core.diag

#include <corelib/ncbiutil.hpp>
#include <deque>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
/// Forward declaration

class CNcbiDiag;

enum EDiagFilterAction {
    eDiagFilter_None,
    eDiagFilter_Accept,
    eDiagFilter_Reject
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrMatcher --
///
/// Abstract class that matches a char* string.
///

class CDiagStrMatcher
{
public:
    virtual ~CDiagStrMatcher();

    /// Return true if str matches for this object
    virtual bool Match(const char* str) const = 0;

    // Print state
    virtual void Print(ostream& out) const = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrEmptyMatcher --
///
/// Specialization of CDiagStrMatcher that matches an empty string.
///

class CDiagStrEmptyMatcher : public CDiagStrMatcher
{
public:
    /// Return true if str is empty
    virtual bool Match(const char* str) const;

    // Print state
    virtual void Print(ostream& out) const;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrStringMatcher --
///
/// Specialization of CDiagStrMatcher that matches a given string.
///

class CDiagStrStringMatcher : public CDiagStrMatcher
{
public:
    // Constructor - set the pattern to match
    CDiagStrStringMatcher(const string& pattern)
        : m_Pattern(pattern)
    {}

    /// Return true if str is equal to pattern
    virtual bool Match(const char* str) const;

    // Print state
    virtual void Print(ostream& out) const;

private:
    string m_Pattern;
};

/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrPathMatcher --
///
/// Specialization of CDiagStrMatcher that matches a path to the source file
///

class CDiagStrPathMatcher : public CDiagStrMatcher
{
public:
    // Constructor - set the pattern to match. The pattern is converted
    // to UNIX format ('/' rather than '\' or ':').
    CDiagStrPathMatcher(const string& pattern);

    /// Return true if str is equal to pattern
    virtual bool Match(const char* str) const;

    // Print state
    virtual void Print(ostream& out) const;

private:
    string m_Pattern;
};

/////////////////////////////////////////////////////////////////////////////
///
/// CDiagStrErrCodeMatcher --
///
/// Specialization of CDiagStrMatcher that matches error codes
///

class CDiagStrErrCodeMatcher : public CDiagStrMatcher
{
public:
    // Constructor - set the pattern to match.
    // Pattern ::= coderange '.' coderange
    // coderange ::= (int | intpair) (',' (int | intpair))*
    // intpair ::= int '-' int
    // int ::= integer
    CDiagStrErrCodeMatcher(const string& pattern);

    /// Return true if str matches pattern
    /// str ::= errcode ':' subcode
    virtual bool Match(const char* str) const;

    // Print state
    virtual void Print(ostream& out) const;

private:
    typedef int  TCode;
    typedef vector< pair<TCode, TCode> >  TPattern;
    
    static void x_Parse(TPattern& pattern, const string& str);
    static bool x_Match(const TPattern& pattern, TCode code);
    static void x_Print(const TPattern& pattern, ostream& out);
    TPattern m_Code;
    TPattern m_SubCode;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDiagMatcher --
///
/// Matcher of CNcbiDiag for module, class and function name if any
///

class CDiagMatcher
{
public:
    // Takes ownership of module, nclass and function.
    // If NULL pointer passed as CDiagStrMatcher*, then it accepts anything
    CDiagMatcher(CDiagStrMatcher*  errcode,
                 CDiagStrMatcher*  file,
                 CDiagStrMatcher*  module, 
                 CDiagStrMatcher*  nclass, 
                 CDiagStrMatcher*  func,
                 EDiagFilterAction action)
        : m_ErrCode(errcode), m_File(file),
          m_Module(module), m_Class(nclass), m_Function(func), 
          m_Action(action), m_DiagSev(eDiag_Info)
    {}

    EDiagFilterAction MatchErrCode(int code, int subcode) const;

    // Check if the filter accepts a filename
    EDiagFilterAction MatchFile(const char* file) const;

    // Match whole set
    EDiagFilterAction Match(const char* module,
                            const char* nclass,
                            const char* function) const;

    // Print state
    void Print(ostream& out) const;

    EDiagSev GetSeverity() const { return m_DiagSev; }
    void SetSeverity(EDiagSev sev) { m_DiagSev = sev; }

    bool IsErrCodeMatcher(void) const {return m_ErrCode.get() != 0;}

private:
    AutoPtr<CDiagStrMatcher> m_ErrCode;
    AutoPtr<CDiagStrMatcher> m_File;
    AutoPtr<CDiagStrMatcher> m_Module;
    AutoPtr<CDiagStrMatcher> m_Class;
    AutoPtr<CDiagStrMatcher> m_Function;
    EDiagFilterAction        m_Action;
    EDiagSev                 m_DiagSev;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagFilter --
///
/// Checks if SDiagMessage should be passed.
/// Contains 0..N CDiagMatcher objects
///

class NCBI_XNCBI_EXPORT CDiagFilter
{
public:
    // 'tors
    CDiagFilter(void);
    ~CDiagFilter(void);

    /// Check if the filter accepts message.
    /// In addition check an exception and all its backlog if specified.
    EDiagFilterAction Check(const CNcbiDiag&  msg, const CException* ex = NULL) const;

    /// Fill the filter from a string
    void Fill(const char* filter_string);

    /// Print state
    void Print(ostream& out) const;

private:
    /// Check if the filter accepts errcode
    EDiagFilterAction x_CheckErrCode(int code, int subcode, EDiagSev sev) const;

    /// Check if the filter accepts path
    EDiagFilterAction x_CheckFile(const char* file, EDiagSev sev) const;

    // Check if the filter accepts the location
    EDiagFilterAction x_CheckLocation(const char* module,
                                      const char* nclass,
                                      const char* function,
                                      EDiagSev    sev) const;

    // CSyntaxParser can insert CDiagStrMatcher and clean
    friend class CDiagSyntaxParser;

    // Insert a new matcher into the end of list
    // Take ownership of the matcher
    void InsertMatcher(CDiagMatcher *matcher)
    {
        m_Matchers.push_back(matcher);
    }

    // Negative matchers should be processed first
    // because they are *AND NOT* matchers, not just *NOT*.
    void InsertNegativeMatcher(CDiagMatcher *matcher)
    { 
        m_Matchers.push_front(matcher); 
        ++m_NotMatchersNum;
    }

    // Remove and destroy all matchers, if any
    void Clean(void);

private:
    typedef deque< AutoPtr<CDiagMatcher> >  TMatchers;
    TMatchers m_Matchers;
    size_t    m_NotMatchersNum;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagLexParser --
///
/// Lexical parser
///

class CDiagLexParser
{
public:
    // Symbols
    enum ESymbol {
        eDone,           // to mark that symbol was processed
        eExpl,           // ! processed
        ePath,           // path started with /
        eId,             // id and ?
        eDoubleColon,    // ::
        ePars,           // ()
        eBrackets,       // []
        eErrCode,        // (code.subcode)
        eEnd             // end of stream
    };

    // Constructor
    CDiagLexParser();

    // Takes next lexical symbol from the stream
    ESymbol Parse(istream& in);

    // Returns id or ? string when eId is returned from parse()
    const string& GetId() const  { return m_Str; }

    // Returns current position
    int GetPos() const { return m_Pos; }

private:
    string  m_Str;
    int     m_Pos;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDiagSyntaxParser --
///
/// Parses string and fills CDiagFilter with CDiagStrMatchers
///

class CDiagSyntaxParser
{
public:
    // Error info type
    typedef pair<const char*, int> TErrorInfo;

    // Constructor
    CDiagSyntaxParser();

    // Parses stream and fills CDiagFilter to with CDiagStrMatchers
    void Parse(istream& in, CDiagFilter& to);

private:
    enum EInto {
        eModule,
        eFunction
    };

    // Puts CDiagMessageMatcher into CDiagFilter
    void x_PutIntoFilter(CDiagFilter& to, EInto into);

    // Creates a CDiagStrMatcher by string
    static CDiagStrMatcher* x_CreateMatcher(const string& str);

    /// "Info", "Warning", etc converted to enum value
    /// Throws an excpetion if incorrect value passed
    EDiagSev x_GetDiagSeverity(const string& sev_str);

private:
    typedef vector< AutoPtr<CDiagStrMatcher> >  TMatchers;
    TMatchers                 m_Matchers;
    AutoPtr<CDiagStrErrCodeMatcher>  m_ErrCodeMatcher;
    AutoPtr<CDiagStrMatcher>  m_FileMatcher;
    int                       m_Pos;
    bool                      m_Negative;
    EDiagSev                  m_DiagSev;
};


END_NCBI_SCOPE

#endif  /* CORELIB___NCBIDIAG_P__HPP */
