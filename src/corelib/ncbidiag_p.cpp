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

CDiagStrPathMatcher::CDiagStrPathMatcher(const string& pattern)
    : m_Pattern(pattern)
{
#   ifdef NCBI_OS_MSWIN
    size_t pos;
    // replace \ in windows path to /
    while ( (pos = m_Pattern.find('\\'))  !=  string::npos )
        m_Pattern[pos] = '/';
#   endif
}


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

    // '/' should not be after place we found m_Pattern
    return (lstr.find('/', pos + m_Pattern.size()) == string::npos);
}


void CDiagStrPathMatcher::Print(ostream& out) const
{
    out << m_Pattern;
}

/////////////////////////////////////////////////////////////////////////////
//  CDiagStrErrCodeMatcher

CDiagStrErrCodeMatcher::CDiagStrErrCodeMatcher(const string& pattern)
{
    string code, subcode;
    NStr::SplitInTwo(pattern,".",code, subcode);
    x_Parse(m_Code,code);
    x_Parse(m_SubCode,subcode);
}

bool CDiagStrErrCodeMatcher::Match(const char* str) const
{
    string first, second;
    NStr::SplitInTwo(str,".", first, second);
    if (!first.empty() && !second.empty()) {
        TCode code    = NStr::StringToInt( first);
        TCode subcode = NStr::StringToInt( second);
        return x_Match(m_Code,code) && x_Match(m_SubCode, subcode);
    }
    return false;
}

void CDiagStrErrCodeMatcher::Print(ostream& out) const
{
    x_Print(m_Code,out);
    out << '.';
    x_Print(m_SubCode,out);
}

void CDiagStrErrCodeMatcher::x_Parse(TPattern& pattern, const string& str)
{
    list<string> loc;
    NStr::Split( str,",",loc);
    list<string>::iterator it_loc;
    for (it_loc = loc.begin(); it_loc != loc.end(); ++it_loc) {
        string first, second;
        const string& sloc = *it_loc;
        size_t shift = 0;
        if (sloc[0] == '-') {
            shift = 1;
        }
        NStr::SplitInTwo( sloc.c_str() + shift,"-",first,second);
        if (!first.empty()) {
            TCode from, to;
            to = from = NStr::StringToInt( first);
            if (shift != 0) {
                to = from = -from;
            }
            if (!second.empty()) {
                to = NStr::StringToInt( second);
            }
            pattern.push_back( make_pair(from,to) );
        }
    }
}

bool CDiagStrErrCodeMatcher::x_Match(const TPattern& pattern, TCode code)
{
    ITERATE( TPattern, c, pattern) {
        if (code >= c->first && code <= c->second) {
            return true;
        }
    }
    return pattern.empty();
}

void CDiagStrErrCodeMatcher::x_Print(const TPattern& pattern, ostream& out)
{
    bool first = true;
    ITERATE( TPattern, c, pattern) {
        if (!first) {
            out << ',';
        }
        if (c->first != c->second) {
            out << c->first << '-' << c->second;
        } else {
            out << c->first;
        }
        first = false;
    }
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

EDiagFilterAction CDiagMatcher::MatchErrCode(int code, int subcode) const
{
    if (!m_ErrCode)
        return eDiagFilter_None;
    string str = NStr::IntToString(code);
    str += '.';
    str += NStr::IntToString(subcode);
    if (m_ErrCode->Match(str.c_str())) {
        return m_Action;
    }
    return m_Action == eDiagFilter_Reject ? 
        eDiagFilter_Accept : eDiagFilter_None;
}

EDiagFilterAction CDiagMatcher::MatchFile(const char* file) const
{
    if(!m_File) 
        return eDiagFilter_None;

    if(m_File->Match(file))
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

    s_PrintMatcher(out, m_ErrCode,  "ErrCode" );
    s_PrintMatcher(out, m_File,     "File"    );
    s_PrintMatcher(out, m_Module,   "Module"  );
    s_PrintMatcher(out, m_Class,    "Class"   );
    s_PrintMatcher(out, m_Function, "Function");
}



///////////////////////////////////////////////////////
//  CDiagFilter::

CDiagFilter::CDiagFilter(void) 
: m_NotMatchersNum(0)
{
}

CDiagFilter::~CDiagFilter(void)  
{ 
    Clean(); 
}

void CDiagFilter::Clean(void)  
{ 
    m_Matchers.clear(); 
    m_NotMatchersNum = 0;
}

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

EDiagFilterAction CDiagFilter::Check(const CNcbiDiag&  msg,
                                     const CException* ex) const
{
    // if we do not have any filters -- accept
    if (m_Matchers.empty()) {
        return eDiagFilter_Accept;
    }

    // Check errcode, file and location for the message
    EDiagFilterAction action;
    EDiagSev sev = msg.GetSeverity();
    action = x_CheckErrCode(msg.GetErrorCode(), msg.GetErrorSubCode(), sev);
    if (action == eDiagFilter_None) {
        action = x_CheckFile(msg.GetFile(), sev);
    }
    if (action == eDiagFilter_None) {
        action = x_CheckLocation(msg.GetModule(), msg.GetClass(), msg.GetFunction(), sev);
    }

    // If specified, additionally check an exception and its backlog.
    // Accept if one of the exceptions match.

    if (action == eDiagFilter_None  &&  ex) {

        // Fast check on non errcode matchers.
        // We already did errcode check above, exceptions have file/location only.
        bool found = false;
        ITERATE(TMatchers, i, m_Matchers) {
            if (!(*i)->IsErrCodeMatcher()) {
                found = true;
                break;
            }
        }
        // If found at least one, process with check
        if (found) {
            const CException* pex;
            for (pex = ex;  pex;  pex = pex->GetPredecessor()) {
                action = x_CheckFile(pex->GetFile().c_str(), sev);
                if (action == eDiagFilter_None) {
                    action = x_CheckLocation(pex->GetModule().c_str(),
                                             pex->GetClass().c_str(),
                                             pex->GetFunction().c_str(),
                                             sev);
                }
                if (action == eDiagFilter_Accept) {
                    return action;
                }
            }
        }
    }

    // Reject if not accepted
    if (action == eDiagFilter_None) {
        action = eDiagFilter_Reject;
    }
    return action;
}


// same logic as in CheckFile()
EDiagFilterAction CDiagFilter::x_CheckErrCode(int code, int subcode, EDiagSev sev) const
{
    size_t not_matchers_processed = 0;
    size_t curr_ind = 0;

    ITERATE(TMatchers, i, m_Matchers) {
        ++curr_ind;
        EDiagFilterAction action = (*i)->MatchErrCode(code, subcode);

        switch( action )
        {
        case eDiagFilter_Accept:
            if ( not_matchers_processed < m_NotMatchersNum ) {
                ++not_matchers_processed;
                // Check severity ...
                if (int(sev) < int((*i)->GetSeverity())) {
                    return eDiagFilter_Reject;
                }
                if ( curr_ind != m_Matchers.size() ) {
                    continue;
                } else {
                    return eDiagFilter_Accept;
                }
            }
            // Process *OR* conditions *PLUS* severity
            if (int(sev) < int((*i)->GetSeverity())) {
                continue;
            }
            return eDiagFilter_Accept;

        case eDiagFilter_Reject:
            if ( not_matchers_processed < m_NotMatchersNum ) {
                ++not_matchers_processed;
                return eDiagFilter_Reject;
            }
            if ( curr_ind != m_Matchers.size() ) {
                continue;
            }
            return eDiagFilter_Reject;

        case eDiagFilter_None:
            if ( not_matchers_processed < m_NotMatchersNum ) {
                ++not_matchers_processed;
            }
            break;
        }
    }

    return eDiagFilter_None;
}

EDiagFilterAction CDiagFilter::x_CheckFile(const char* file, EDiagSev sev) const
{
    size_t not_matchers_processed = 0;
    size_t curr_ind = 0;

    ITERATE(TMatchers, i, m_Matchers) {
        ++curr_ind;
        EDiagFilterAction action = (*i)->MatchFile(file);
 
        switch( action )
        {
        case eDiagFilter_Accept:
            // Process all *AND NOT* conditions.
            if ( not_matchers_processed < m_NotMatchersNum ) {
                // Not all *AND* conditions are still processed. 
                // Continue to check.
                ++not_matchers_processed;
                // Check severity ...
                if (int(sev) < int((*i)->GetSeverity())) {
                    return eDiagFilter_Reject;
                }
                if ( curr_ind != m_Matchers.size() ) {
                    continue;
                }
                return eDiagFilter_Accept;
            }
            // Process *OR* conditions *PLUS* severity
            if (int(sev) < int((*i)->GetSeverity())) {
                continue;
            }
            return eDiagFilter_Accept;

        case eDiagFilter_Reject:
            // Process all *AND NOT* and *OR* conditions.
            if ( not_matchers_processed < m_NotMatchersNum ) {
                // *AND* failed ...
                ++not_matchers_processed;
                return eDiagFilter_Reject;
            }
            if ( curr_ind != m_Matchers.size() ) {
                // It is still not the end of a list of the *OR* matchers.
                // Continue to check for a success.
                continue;
            }
            return eDiagFilter_Reject;

        case eDiagFilter_None:
            // Continue the loop.
            if ( not_matchers_processed < m_NotMatchersNum ) {
                ++not_matchers_processed;
            }
            break;
        }
    }

    return eDiagFilter_None;
}

// Check if the filter accepts module, class and function
EDiagFilterAction CDiagFilter::x_CheckLocation(const char* module,
                                               const char* nclass,
                                               const char* function,
                                               EDiagSev    sev) const
{
    size_t not_matchers_processed = 0;
    size_t curr_ind = 0;

    ITERATE(TMatchers, i, m_Matchers) {
        ++curr_ind;
        EDiagFilterAction action = (*i)->Match(module, nclass, function);

        switch( action )
        {
        case eDiagFilter_Accept:
            // Process all *AND NOT* conditions.
            if ( not_matchers_processed < m_NotMatchersNum ) {
                // Not all *AND* conditions are still processed. 
                // Continue to check.
                ++not_matchers_processed;
                // Check severity ...
                if ( int(sev) < int((*i)->GetSeverity()) ) {
                    return eDiagFilter_Reject;
                } 
                if ( curr_ind != m_Matchers.size() ) {
                    continue;
                }
                return eDiagFilter_Accept;
            }
            // Process *OR* conditions *PLUS* severity
            if ( int(sev) < int((*i)->GetSeverity()) ) {
                continue;
            } 
            return eDiagFilter_Accept;

        case eDiagFilter_Reject:
            // Process all *AND NOT* and *OR* conditions.
            if ( not_matchers_processed < m_NotMatchersNum ) {
                // *AND* failed ...
                ++not_matchers_processed;
                return eDiagFilter_Reject;
            }
            if ( curr_ind != m_Matchers.size() ) {
                // It is still not the end of a list of the *OR* matchers.
                // Continue to check for a success.
                continue;
            }
            return eDiagFilter_Reject;

        case eDiagFilter_None:
            // Continue the loop.
            if ( not_matchers_processed < m_NotMatchersNum ) {
                ++not_matchers_processed;
            }
            break;
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
        eInsideErrCode,
        eSpace
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
            case '\\':
            case '/' :
                state = eInsidePath;
                m_Str = symbol;
                break;
            default :
                if ( isspace((unsigned char) symbol) )
                {
                    state = eSpace;
                    break;
                }
                if ( !isalpha((unsigned char) symbol)  &&  symbol != '_' )
                    throw CDiagSyntaxParser::TErrorInfo("wrong symbol", m_Pos);
                m_Str = symbol;
                state = eInsideId;
            }
            break;
        case eSpace :
            if ( !isspace((unsigned char) symbol) ) {
                if ( symbol == '[' ||
                     symbol == '(' ||
                    (symbol == '!' && CT_TO_CHAR_TYPE(in.peek()) == '(')) {
                    in.putback( symbol );
                    --m_Pos;
                    state = eStart;
                    break;
                }
                in.putback( symbol );
                --m_Pos;
                return eDone;
            }
            break;
        case eExpectColon :
            if( isspace((unsigned char) symbol) )
                break;
            if( symbol == ':' )
                return eDoubleColon;
            throw CDiagSyntaxParser::TErrorInfo
                ( "wrong symbol, expected :", m_Pos );
        case eExpectClosePar :
            if( isspace((unsigned char) symbol) )
                break;
            if( symbol == ')' )
                return ePars;
            if( symbol == '+' || symbol == '-' ||
                symbol == '.' ||
                isdigit((unsigned char) symbol)) {
                state = eInsideErrCode;
                m_Str = symbol;
                break;
            }
            throw CDiagSyntaxParser::TErrorInfo
                ( "wrong symbol, expected )", m_Pos );
        case eExpectCloseBracket:
            if (symbol == ']') {
                return eBrackets;
            }
            if( isspace((unsigned char) symbol) )
                break;
            m_Str += symbol;
            break;
        case eInsideId :
            if(isalpha((unsigned char) symbol)  ||
               isdigit((unsigned char) symbol)  ||  symbol == '_') {
                m_Str += symbol;
                break;
            }
            in.putback( symbol );
            m_Pos--;
            return eId;
        case eInsidePath :
            if( isspace((unsigned char) symbol) )
                return ePath;
            m_Str += symbol;
            break;
        case eInsideErrCode:
            if( symbol == '+' || symbol == '-' ||
                symbol == '.' || symbol == ',' ||
                isdigit((unsigned char) symbol)) {
                m_Str += symbol;
                break;
            }
            if( symbol == ')' )
                return eErrCode;
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
             m_ErrCodeMatcher.release(),
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
             m_ErrCodeMatcher.release(),
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
             m_ErrCodeMatcher.release(),
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
             m_ErrCodeMatcher.release(),
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
    m_ErrCodeMatcher = NULL;
    m_FileMatcher = NULL;
    matcher->SetSeverity(m_DiagSev);
    // reset severity for next matcher
    m_DiagSev = eDiag_Info;

    _ASSERT( matcher );

    if ( m_Negative ) {
        to.InsertNegativeMatcher( matcher );
    } else {
        to.InsertMatcher( matcher );
    }
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
                    m_Negative = false;
                    break;
                case CDiagLexParser::eErrCode:
                    m_ErrCodeMatcher = new CDiagStrErrCodeMatcher(lexer.GetId());
                    x_PutIntoFilter(to, eModule);
                    m_Negative = false;
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
                    m_Negative = false;
                    state = eStart;
                    break;
                case CDiagLexParser::eErrCode:
                    m_ErrCodeMatcher = new CDiagStrErrCodeMatcher(lexer.GetId());
                    x_PutIntoFilter(to, eModule);
                    m_Negative = false;
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
