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
 * File Description:
 *   NCBI service classes and functions for C++ diagnostic API
 *
 */

#include <ncbi_pch.hpp>
#include "ncbidiag_p.hpp"
#include <corelib/ncbidiag.hpp>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CDiagStrMatcher::

CDiagStrMatcher::~CDiagStrMatcher()
{
}


///////////////////////////////////////////////////////
//  CDiagStrEmptyMatcher::

bool CDiagStrEmptyMatcher::Match(const char* str) const
{
    return (!str  ||  *str == '\0');
}


void CDiagStrEmptyMatcher::Print(ostream& out) const
{
    out << '?';
}



///////////////////////////////////////////////////////
//  CDiagStrStringMatcher::

bool CDiagStrStringMatcher::Match(const char* str) const
{
    if ( !str )
        return false;
    return m_Pattern == str;
}


void CDiagStrStringMatcher::Print(ostream& out) const
{
    out << m_Pattern;
}


///////////////////////////////////////////////////////
//  CDiagStrPathMatcher::

bool CDiagStrPathMatcher::Match(const char* str) const
{
    if ( !str )
        return false;
    string lstr(str);
    size_t pos;
#   ifdef NCBI_OS_MSWIN
    // replace \ in windows path to /
    while ( (pos = lstr.find('\\'))  !=  string::npos )
        lstr[pos] = '/';
#   endif
#   ifdef NCBI_OS_MACOS
    // replace : in mac path to /
    while ( (pos = lstr.find(':'))  !=  string::npos )
        lstr[pos] = '/';
#   endif

    pos = lstr.find(m_Pattern);
    if (pos == string::npos)
        return false;

    // check if found pattern after src/ or include/
    if (  !(pos > 2  &&  lstr.substr(pos-3, 3)  ==  "src"     )  &&
         !(pos > 6  &&  lstr.substr(pos-7, 7)  ==  "include" ) )
        return false;

    // if pattern ends with / check that pattern matches all dirs
    if (m_Pattern[m_Pattern.size()-1] != '/')
        return true;

    // '/' should not be after place we found m_Pattern + 1
    return (lstr.substr(pos+m_Pattern.size()+1).find('/') == string::npos);
}


void CDiagStrPathMatcher::Print(ostream& out) const
{
    out << m_Pattern;
}


///////////////////////////////////////////////////////
//  CDiagMatcher::

EDiagFilterAction CDiagMatcher::Match(const char* module,
                                      const char* nclass,
                                      const char* function) const
{
    if( !m_Module  &&  !m_Class  &&  !m_Function )
        return eDiagFilter_None;

    EDiagFilterAction reverse = m_Action == eDiagFilter_Reject ? 
        eDiagFilter_Accept : eDiagFilter_None;

    if ( m_Module      &&  !m_Module  ->Match(module  ) )
        return reverse;
    if ( m_Class       &&  !m_Class   ->Match(nclass  ) )
        return reverse;
    if ( m_Function    &&  !m_Function->Match(function) )
        return reverse;

    return m_Action;
}

EDiagFilterAction CDiagMatcher::MatchFile(const char* file) const
{
    if(!m_File) 
        return eDiagFilter_None;

    if(m_File && m_File->Match(file))
        return m_Action;

    return m_Action == eDiagFilter_Reject ? 
        eDiagFilter_Accept : eDiagFilter_None;
}

inline
void s_PrintMatcher(ostream& out,
                  const AutoPtr<CDiagStrMatcher> &matcher, 
                  const string& desc)
{
    out << desc << "(";
    if(matcher)
        matcher->Print(out);
    else
        out << "NULL";
    out << ") ";
}
    
void CDiagMatcher::Print(ostream& out) const
{
    if (m_Action == eDiagFilter_Reject)
        out << '!';

    s_PrintMatcher(out, m_File,     "File"    );
    s_PrintMatcher(out, m_Module,   "Module"  );
    s_PrintMatcher(out, m_Class,    "Class"   );
    s_PrintMatcher(out, m_Function, "Function");
}



///////////////////////////////////////////////////////
//  CDiagFilter::

void CDiagFilter::Fill(const char* filter_string)
{
    try {
        CDiagSyntaxParser parser;
        CNcbiIstrstream in(filter_string);

        parser.Parse(in, *this);
    }
    catch (const CDiagSyntaxParser::TErrorInfo& err_info) {
        CNcbiOstrstream message;
        message << "Syntax error in string \"" << filter_string
                << "\" at position:"
                << err_info.second << " - " << err_info.first << ends;
        NCBI_THROW(CCoreException, eDiagFilter,
                   CNcbiOstrstreamToString(message));
    }
}


// Check if the filter accepts the message
EDiagFilterAction CDiagFilter::Check(const CNcbiDiag& message,
                                     EDiagSev         sev) const
{
    // if we do not have any filters accept
    if(m_Matchers.empty())
        return eDiagFilter_Accept;

    EDiagFilterAction action = CheckFile(message.GetFile());
    if (action == eDiagFilter_None) 
        action = x_Check(message.GetModule(),
                         message.GetClass(),
                         message.GetFunction(),
                         sev);

    if (action == eDiagFilter_None) {
        action = eDiagFilter_Reject;
    }

    return action;
}


// Check if the filter accepts the exception
EDiagFilterAction CDiagFilter::Check(const CException& ex,
                                     EDiagSev          sev) const
{
    // if we do not have any filters accept
    if(m_Matchers.empty())
        return eDiagFilter_Accept;

    const CException* pex;
    for (pex = &ex;  pex;  pex = pex->GetPredecessor()) {
        EDiagFilterAction action = CheckFile(pex->GetFile().c_str());
        if (action == eDiagFilter_None) 
            action = x_Check(pex->GetModule()  .c_str(),
                             pex->GetClass()   .c_str(),
                             pex->GetFunction().c_str(),
                             sev);
        if (action == eDiagFilter_Accept)
                return action;
    }
    return eDiagFilter_Reject;
}


EDiagFilterAction CDiagFilter::CheckFile(const char* file) const
{
    ITERATE(TMatchers, i, m_Matchers) {
        EDiagFilterAction action = (*i)->MatchFile(file);
        if( action != eDiagFilter_None )
            return action;
    }

    return eDiagFilter_None;
}

// Check if the filter accepts module, class and function
EDiagFilterAction CDiagFilter::x_Check(const char* module,
                                       const char* nclass,
                                       const char* function,
                                       EDiagSev    sev) const
{
    ITERATE(TMatchers, i, m_Matchers) {
        EDiagFilterAction action = (*i)->Match(module, nclass, function);
        if (action == eDiagFilter_Accept && 
            int(sev) < int((*i)->GetSeverity())) {
            continue;
        }
            if( action != eDiagFilter_None ) {
                if ( action == eDiagFilter_Reject && i != m_Matchers.end() ) {
                    continue;
                }
                return action;
            }
    }

    return eDiagFilter_None;
}


void CDiagFilter::Print(ostream& out) const
{
    int count = 0;
    ITERATE(TMatchers, i, m_Matchers) {
        out << "\tFilter " << count++ << " - ";
        (*i)->Print(out);
        out << endl;
    }
}



/////////////////////////////////////////////////////////////////////////////
/// CDiagLexParser::

CDiagLexParser::CDiagLexParser()
    : m_Pos(0)
{
}


// Takes next lexical symbol from the stream
CDiagLexParser::ESymbol CDiagLexParser::Parse(istream& in)
{
    CT_INT_TYPE symbol0;
    enum EState { 
        eStart, 
        eExpectColon, 
        eExpectClosePar, 
        eExpectCloseBracket,
        eInsideId,
        eInsidePath,
        eSpace,
    };
    EState state = eStart;

    while( true ){
        symbol0 = in.get();
        if (CT_EQ_INT_TYPE(symbol0, CT_EOF)) {
            break;
        }
        CT_CHAR_TYPE symbol = CT_TO_CHAR_TYPE(symbol0);
        m_Pos++;

        switch( state ) {
        case eStart:
            switch( symbol ) {
            case '?' :
                m_Str = '?';
                return eId;
            case '!' :
                return eExpl;
            case ':' :
                state = eExpectColon;
                break;
            case '(' :
                state = eExpectClosePar;
                break;
            case '[':
                m_Str = kEmptyStr;
                state = eExpectCloseBracket;
                break;
            case '/' :
                state = eInsidePath;
                m_Str = symbol;
                break;
            default :
                if ( isspace(symbol) )
                {
                    state = eSpace;
                    break;
                }
                if ( !isalpha(symbol)  &&  symbol != '_' )
                    throw CDiagSyntaxParser::TErrorInfo("wrong symbol", 
                                                        m_Pos);
                m_Str = symbol;
                state = eInsideId;
            }
            break;
        case eSpace :
            if ( !isspace(symbol) ) {
                in.putback( symbol );
                --m_Pos;
                return eDone;
            }
            break;
        case eExpectColon :
            if( isspace(symbol) )
                break;
            if( symbol == ':' )
                return eDoubleColon;
            throw CDiagSyntaxParser::TErrorInfo
                ( "wrong symbol, expected :", m_Pos );
        case eExpectClosePar :
            if( isspace( symbol ) )
                break;
            if( symbol == ')' )
                return ePars;
            throw CDiagSyntaxParser::TErrorInfo
                ( "wrong symbol, expected )", m_Pos );
        case eExpectCloseBracket:
            if (symbol == ']') {
                return eBrackets;
            }
            if( isspace( symbol ) )
                break;
            m_Str += symbol;
            break;
        case eInsideId :
            if(isalpha(symbol)  ||  isdigit(symbol)  ||  symbol == '_') {
                m_Str += symbol;
                break;
            }
            in.putback( symbol );
            m_Pos--;
            return eId;
        case eInsidePath :
            if( isspace(symbol) )
                return ePath;
            m_Str += symbol;
            break;
        }
    }

    switch ( state ) {
    case eExpectColon :
        throw CDiagSyntaxParser::TErrorInfo
            ( "unexpected end of input, ':' expected", m_Pos );
    case eExpectClosePar :
        throw CDiagSyntaxParser::TErrorInfo
            ( "unexpected end of input, ')' expected", m_Pos );
    case eExpectCloseBracket:
        throw CDiagSyntaxParser::TErrorInfo
            ( "unexpected end of input, ']' expected", m_Pos );
    case eInsideId :
        return eId;
    case eInsidePath :
        return ePath;
    case eStart :
        break;
    default:
        break;
    }

    return eEnd;
}



/////////////////////////////////////////////////////////////////////////////
/// CDiagSyntaxParser::

CDiagSyntaxParser::CDiagSyntaxParser()
    : m_Pos(0),
      m_Negative(false),
      m_DiagSev(eDiag_Info)
{
}


void CDiagSyntaxParser::x_PutIntoFilter(CDiagFilter& to, EInto into)
{
    CDiagMatcher* matcher = 0;
    switch ( m_Matchers.size() ) {
    case 0 :
        matcher = new CDiagMatcher
            (
             m_FileMatcher.release(),
             NULL,
             NULL,
             NULL,
             m_Negative ? eDiagFilter_Reject : eDiagFilter_Accept
             );
        break;
    case 1:
        matcher = new CDiagMatcher
            (
             m_FileMatcher.release(),
             // the matcher goes to module if function is not enforced
             into == eFunction ? NULL : m_Matchers[0].release(),
             NULL,
             into == eFunction ? m_Matchers[0].release() : NULL,
             // the matcher goes to function if function is enforced
             m_Negative ? eDiagFilter_Reject : eDiagFilter_Accept
             );
        break;
    case 2:
        matcher = new CDiagMatcher
            (
             m_FileMatcher.release(),
             // the first matcher goes to module
             m_Matchers[0].release(),
             // the second matcher goes to class if function is not enforced
             into == eFunction ? NULL : m_Matchers[1].release(),
             // the second matcher goes to function if function is enforced
             into == eFunction ? m_Matchers[1].release() : NULL,
             m_Negative ? eDiagFilter_Reject : eDiagFilter_Accept
             );
        break;
    case 3:
        matcher = new CDiagMatcher
            (
             m_FileMatcher.release(),
             // the first matcher goes to module
             m_Matchers[0].release(),
             // the second matcher goes to class
             m_Matchers[1].release(),
             // the third matcher goes to function
             m_Matchers[2].release(),
             m_Negative ? eDiagFilter_Reject : eDiagFilter_Accept 
             );
        break;
    default :
        _ASSERT( false );
    }
    m_Matchers.clear();
    m_FileMatcher = NULL;
    matcher->SetSeverity(m_DiagSev);

    _ASSERT( matcher );
    to.InsertMatcher(matcher);
}


CDiagStrMatcher* CDiagSyntaxParser::x_CreateMatcher(const string& str)
{
    _ASSERT( !str.empty() );

    if (str == "?")
        return new CDiagStrEmptyMatcher;

    return new CDiagStrStringMatcher(str);
}



EDiagSev CDiagSyntaxParser::x_GetDiagSeverity(const string& sev_str)
{
    if (NStr::CompareNocase(sev_str, "Info") == 0) {
        return eDiag_Info;
    }
    if (NStr::CompareNocase(sev_str, "Warning") == 0) {
        return eDiag_Warning;
    }
    if (NStr::CompareNocase(sev_str, "Error") == 0) {
        return eDiag_Error;
    }
    if (NStr::CompareNocase(sev_str, "Critical") == 0) {
        return eDiag_Critical;
    }
    if (NStr::CompareNocase(sev_str, "Fatal") == 0) {
        return eDiag_Fatal;
    }
    if (NStr::CompareNocase(sev_str, "Trace") == 0) {
        return eDiag_Trace;
    }
    throw TErrorInfo("Incorrect severity level", m_Pos);
    
}

void CDiagSyntaxParser::Parse(istream& in, CDiagFilter& to)
{
    enum EState {
        eStart,
        eGotExpl,
        eGotModule,
        eGotModuleOrFunction,
        eGotClass,
        eGotFunction,
        eGotClassOrFunction,
        eReadyForFunction
    };

    CDiagLexParser lexer;
    EState state = eStart;
    m_Negative = false;

    CDiagLexParser::ESymbol symbol = CDiagLexParser::eDone;
    try {
        to.Clean();

        for (;;) {
            if (symbol == CDiagLexParser::eDone)
                symbol = lexer.Parse(in);

            switch (state) {

            case eStart :
                switch (symbol) {
                case CDiagLexParser::eExpl:
                    m_Negative = true;
                    state = eGotExpl;
                    break;
                case CDiagLexParser::eDoubleColon:
                    m_Matchers.push_back(NULL); // push empty module
                    state = eGotModule;
                    break;
                case CDiagLexParser::eId:
                    m_Matchers.push_back( x_CreateMatcher(lexer.GetId()) );
                    state = eGotModuleOrFunction;
                    break;
                case CDiagLexParser::ePath:
                    m_FileMatcher = new CDiagStrPathMatcher(lexer.GetId());
                    x_PutIntoFilter(to, eModule);
                    break;
                case CDiagLexParser::eBrackets:
                    {
                    EDiagSev sev = x_GetDiagSeverity(lexer.GetId());
                    // trace is not controlled by this filtering
                    if (sev == eDiag_Trace) {
                        throw TErrorInfo("unexpected 'Trace' severity", m_Pos);
                    }                    
                    m_DiagSev = sev;
                    }
                    break;
                case CDiagLexParser::eEnd:
                    break;
                default :
                    throw TErrorInfo("'!' '::' '[]' or 'id' expected", m_Pos);
                }
                break;

            case eGotExpl :
                switch (symbol) {
                case CDiagLexParser::eId:
                    m_Matchers.push_back( x_CreateMatcher(lexer.GetId()) );
                    state = eGotModuleOrFunction;
                    break;
                case CDiagLexParser::eDoubleColon:
                    m_Matchers.push_back(NULL); // push empty module
                    state = eGotModule;
                    break;
                case CDiagLexParser::ePath:
                    m_FileMatcher = new CDiagStrPathMatcher(lexer.GetId());
                    x_PutIntoFilter(to, eModule);
                    state = eStart;
                    break;
                default :
                    throw TErrorInfo("'::' or 'id' expected", m_Pos);
                }
                break;

            case eGotModule :
                switch ( symbol ) {
                case CDiagLexParser::eId:
                    m_Matchers.push_back( x_CreateMatcher(lexer.GetId()) );
                    state = eGotClassOrFunction;
                    break;
                default :
                    throw TErrorInfo("'id' expected", m_Pos);
                }
                break;

            case eGotModuleOrFunction :
                switch( symbol ) {
                case CDiagLexParser::ePars:
                    state = eGotFunction;
                    break;
                case CDiagLexParser::eDoubleColon:
                    state = eGotModule;
                    break;
                default :
                    x_PutIntoFilter( to, eModule );
                    m_Negative = false;
                    state = eStart;
                    continue;
                }
                break;

            case eGotFunction :
                x_PutIntoFilter(to, eFunction);
                m_Negative = false;
                state = eStart;
                continue;

            case eGotClassOrFunction :
                switch( symbol ) {
                case CDiagLexParser::ePars:
                    state = eGotFunction;
                    break;
                case CDiagLexParser::eDoubleColon:
                    state = eGotClass;
                    break;
                default :
                    x_PutIntoFilter( to, eModule );
                    m_Negative = false;
                    state = eStart;
                    continue;
                }
                break;

            case eGotClass :
                switch( symbol ) {
                case CDiagLexParser::eId:
                    m_Matchers.push_back( x_CreateMatcher(lexer.GetId()) );
                    state = eReadyForFunction;
                    break;
                default :
                    throw TErrorInfo("'id' expected", m_Pos);
                }
                break;

            case eReadyForFunction :
                switch( symbol ) {
                case CDiagLexParser::ePars:
                    state = eGotFunction;
                    break;
                default :
                    x_PutIntoFilter(to, eModule);
                    m_Negative = false;
                    state = eStart;
                    continue;
                }
                break;
            }
            if( symbol == CDiagLexParser::eEnd ) break;
            symbol = CDiagLexParser::eDone;
            m_Pos = lexer.GetPos();

        }
    }
    catch (...) {
        to.Clean();
        throw;
    }
}



END_NCBI_SCOPE



/*
 * ==========================================================================
 * $Log$
 * Revision 1.8  2005/04/25 19:49:28  ivanov
 * Fixed compilation warning
 *
 * Revision 1.7  2005/04/25 16:44:33  ssikorsk
 * Fixed DIAG_FILTER parser and expression evaluation bugs
 *
 * Revision 1.6  2004/12/17 12:15:37  kuznets
 * A parser state situation tracked more carefully
 *
 * Revision 1.5  2004/12/13 14:38:32  kuznets
 * Implemented severity filtering
 *
 * Revision 1.4  2004/09/23 21:27:41  ucko
 * CDiagLexParser::Parse: properly detect end of input.
 *
 * Revision 1.3  2004/09/22 14:52:48  ucko
 * Unbroke non-MSVC compilation.
 *
 * Revision 1.2  2004/09/22 14:40:23  kuznets
 * Fixed MSVC compile
 *
 * Revision 1.1  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * 	CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * ==========================================================================
 */
