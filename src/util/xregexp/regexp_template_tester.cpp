/*$Id$
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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *      Regexp template tester based on the Perl Compatible Regular
 *      Expression (pcre) library. Allow to test delimited text data
 *      against template with regular expression rules.
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/tempstr.hpp>
#include <util/xregexp/regexp.hpp>
#include <util/xregexp/regexp_template_tester.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Macros for throwing an exceptions on error
//

#define FILE_NAME(name) \
    (name.empty() ? string("(stream)") : name)

#define ERROR_FILE(errcode, message, file) \
    NCBI_THROW(CRegexpTemplateTesterException, errcode, \
              FILE_NAME(file) + " -- " + message)

#define ERROR_TEMPLATE(errcode, message) \
    NCBI_THROW(CRegexpTemplateTesterException, errcode, "\n" + \
              FILE_NAME(m_FileName) + "(" + \
              NStr::NumericToString(m_FileLineNum) + ")\n" + \
              FILE_NAME(m_TemplateName) + "(" + \
              NStr::NumericToString(m_TemplateLineNum) + ")\n-- " + \
              message)



/////////////////////////////////////////////////////////////////////////////
///
/// CRegexpTemplateTester -- 
///


CRegexpTemplateTester::CRegexpTemplateTester(TFlags flags)
    : m_Flags(flags),
      m_VarStart("${"),
      m_VarEnd("}"),
      m_OpStart("#"),
      m_CommentStart("//"),
      m_EOLs("\r\n")
{
    x_Reset();
}


void CRegexpTemplateTester::x_Reset()
{
    // File/template
    m_FileName.clear();
    m_FileLineNum = 0;
    m_TemplateName.clear();
    m_TemplateLineNum = 0;

    // Variables
    m_Vars.clear();

    // Cached line
    m_FileLine.clear();
    m_ReprocessFileLine = false;
}


void CRegexpTemplateTester::SetVarScope(string& start, string& end)
{
    m_VarStart = start;
    m_VarEnd = end;
}


void CRegexpTemplateTester::SetCommentStart(string& str)
{
    m_CommentStart = str;
}


void CRegexpTemplateTester::SetCommandStart(string& str)
{
    m_OpStart = str;
}


void CRegexpTemplateTester::SetDelimiters(string& str)
{
    m_EOLs = str;
}


string CRegexpTemplateTester::GetVar(const string& name) const
{
    TVarMap::const_iterator var = m_Vars.find(name);
    if (var == m_Vars.end()) {
        ERROR_TEMPLATE(eVarNotFound, string("variable '") + name + "' is not defined");
    }
    return var->second;
}


const CRegexpTemplateTester::TVarMap& CRegexpTemplateTester::GetVars(void) const
{
    return m_Vars;
}


void CRegexpTemplateTester::PrintVar(const string& name) const
{
    string value = GetVar(name);
    cout << name << " = " << NStr::PrintableString(value) << endl;
}


void CRegexpTemplateTester::PrintVars(void) const
{
    ITERATE(TVarMap, it, m_Vars) {
        cout << it->first << " = " << NStr::PrintableString(it->second) << endl;
    }
}


void CRegexpTemplateTester::Compare(const string& file_path, const string& template_path)
{
    x_Reset();
    m_FileName = file_path;
    m_TemplateName = template_path;

    CNcbiIfstream file_stm(file_path.c_str());
    if (!file_stm.good()) {
        ERROR_FILE(eOpenFile, "cannot open file", file_path);
    }
    CNcbiIfstream template_stm(template_path.c_str());
    if (!template_stm.good()) {
        ERROR_FILE(eOpenFile, "cannot open file", template_path);
    }
    // Compare
    if ( x_Compare(file_stm, template_stm) == eStop ) {
        return;
    }
    // Compare number of lines in both files
    if (x_GetLine(file_stm, eFile)) {
        ERROR_TEMPLATE(eMismatchLength, "file/template length mismatch");
    }
    return;
}


void CRegexpTemplateTester::Compare(istream& file_stm, istream& template_stm)
{
    x_Reset();
    if (x_Compare(file_stm, template_stm) == eStop) {
        return;
    }
    // Compare number of lines in both streams
    if (x_GetLine(file_stm, eFile)) {
        ERROR_TEMPLATE(eMismatchLength, "stream/template length mismatch");
    }
    return;
}


CRegexpTemplateTester::EResult CRegexpTemplateTester::x_Compare(istream& file_stm, istream& template_stm)
{
    while (x_GetLine(template_stm, eTemplate)) 
    {
        CTempString str(m_TemplateLine);

        // Comment line
        if (NStr::StartsWith(str, m_CommentStart)) {
            continue;
        }
        // Operations
        if (NStr::StartsWith(str, m_OpStart)) {
            str = NStr::TruncateSpaces_Unsafe(str.substr(m_OpStart.length()));

            // stop
            if (str == "stop") {
                return eStop;
            }
            // set var=value
            if (NStr::StartsWith(str, "set ")) {
                x_Op_Set(str);
                continue;
            }
            // echo <string>
            if (NStr::StartsWith(str, "echo ")) {
                x_Op_Echo(str);
                continue;
            }
            // test <expression>
            if (NStr::StartsWith(str, "test ")) {
                x_Op_Test(str);
                continue;
            }
            // include <path>
            if (NStr::StartsWith(str, "include ")) {
                x_Op_Include(str, file_stm);
                continue;
            }
            // skip <expression>
            if (NStr::StartsWith(str, "skip ")) {
                x_Op_Skip(str, file_stm);
                continue;
            }
            SIZE_TYPE len = str.find(' ');
            ERROR_TEMPLATE(eOpUnknown, "unknown operation '" + string(str.substr(0, len)) + "'");
            // unreachable
        }

        if (!m_ReprocessFileLine) {
            // Get next line from the file
            if (!x_GetLine(file_stm, eFile)) {
                ERROR_TEMPLATE(eMismatchLength, "file/template length mismatch");
            }
            ++m_FileLineNum;
        }
        m_ReprocessFileLine = false;

        // Compare
        x_CompareLines(m_FileLine, str);
    }
    return eTemplateEOF;
}


istream& CRegexpTemplateTester::x_GetLine(istream& is, ESource src)
{
    string*     str = NULL;
    SIZE_TYPE*  num = NULL;
    bool skip_empty = false;;

    switch(src) {
        case eFile:
            str = &m_FileLine;
            num = &m_FileLineNum;
            skip_empty = (m_Flags & fSkipEmptySourceLines) > 0;
            break;
        case eTemplate:
            str = &m_TemplateLine;
            num = &m_TemplateLineNum;
            skip_empty = (m_Flags & fSkipEmptyTemplateLines) > 0;
            break;
        default:
            _TROUBLE;
    }

    while (NcbiGetline(is, *str, m_EOLs)) {
        (*num)++;
        if (!skip_empty  ||  !str->empty()) {
            break;
        }
    }
    return is;
}


void CRegexpTemplateTester::x_CompareLines(CTempString file_line, CTempString template_line)
{
    // Check template line on variables and substitute

    // Substitute known variables (constants) with values
    string s = x_SubstituteVars(template_line, NULL);
    // Now, replace inline variable definitions with regexp counterparts
    TVarList inline_vars;
    s = x_SubstituteVars(s, &inline_vars);

    // Compare lines

    CRegexp re(s);
    if ( !re.IsMatch(file_line) ) {
        ERROR_TEMPLATE(eMismatchContent, "content mismatch");
    }
    if ( inline_vars.size() != (SIZE_TYPE)(re.NumFound()-1) ) {
        ERROR_TEMPLATE(eMismatchContent, "cannot match all variables");
    }
    // Convert sub-patterns to variables
    int i = 1;
    ITERATE(TVarList, it, inline_vars) {
        m_Vars[*it] = re.GetSub(file_line, i++);
    }
    return;
}


SIZE_TYPE CRegexpTemplateTester::x_ParseVar(CTempString str, SIZE_TYPE pos) const
{
    SIZE_TYPE len = str.length();
    pos += m_VarStart.length();
    if (pos >= len) {
        return NPOS;
    }

    int counter = 1;

    for (; pos <= len - m_VarEnd.length(); pos++) {
        if (NStr::CompareCase(str, pos, m_VarStart.length(), m_VarStart) == 0) {
            counter++;
        } else 
        if (NStr::CompareCase(str, pos, m_VarEnd.length(), m_VarEnd) == 0) {
            counter--;
        }
        if (counter == 0) {
            return pos;
        }
    }
    return NPOS;
}


SIZE_TYPE CRegexpTemplateTester::x_ParseVarName(CTempString str, SIZE_TYPE pos) const
{
    SIZE_TYPE len = str.length();
    if (pos >= len) {
        return NPOS;
    }
    _ASSERT(!isspace((unsigned char)str[pos]));
    if (!isalpha((unsigned char)str[pos])) {
        ERROR_TEMPLATE(eVarErr, "variable name should start with alphabetic symbol");
    }
    SIZE_TYPE start = pos;
    ++pos;
    while (pos < len  &&  
           (isalnum((unsigned char)str[pos])  ||  str[pos] == '-'  ||  str[pos] == '_') ) ++pos;
    return pos - start;
}


string CRegexpTemplateTester::x_SubstituteVars(CTempString str, TVarList* inline_vars) const
{
    SIZE_TYPE start = NStr::Find(str, m_VarStart);
    if (start == NPOS) {
        return str;
    }
    string out;
    bool   out_have_value = false;  // allow to process empty variables
    SIZE_TYPE last = 0;
    do {
        SIZE_TYPE end = x_ParseVar(str, start);
        if (end == NPOS  ||  end == start) {
            ERROR_TEMPLATE(eVarErr, "cannot find closing tag for variable definition");
        }
        // Get variable definition
        CTempString vdef = str.substr(start + m_VarStart.length(),
                                      end - start - m_VarStart.length());

        // If variable definition looks like like "var=value", replace it with 
        // regexp sub-pattern "(value)" and add "var" to 'inline_vars' list.
        // Otherwise, just substitute variable with its value.

        SIZE_TYPE eqpos = vdef.find('=');

        if (inline_vars || (!inline_vars  &&  eqpos == NPOS)) {
            // Append string before variable definition
            if (start - last) {
                out.append(str.data() + last, start - last);
                out_have_value = true;
            }
            last = end + m_VarEnd.length();
        }
        if (eqpos == NPOS) {
            // Replace variable with its value 
            string s = x_SubstituteVars(vdef, NULL);
            out.append(GetVar(s));
            out_have_value = true;
        } else 

        if (inline_vars) {
            // Get variable name
            SIZE_TYPE n = x_ParseVarName(vdef, 0);
            CTempString inline_name = vdef.substr(0, n);
            if ( !n || (inline_name.length() != eqpos)) {
                ERROR_TEMPLATE(eVarErr, "wrong variable definition");
            }
            CTempString inline_val = vdef.substr(eqpos + 1);
            if (inline_val.empty()) {
                ERROR_TEMPLATE(eVarErr, "variable definition cannot have empty value");
            }
            string s = x_SubstituteVars(inline_val, NULL);
            // Replace with subpattern
            out.append("(" + s + ")");
            out_have_value = true;
           inline_vars->push_back(inline_name);
        }
        // Find next variable
        start = NStr::Find(str, m_VarStart, end + m_VarEnd.length());
    }
    while ( start != NPOS );

    if (out.empty() && !out_have_value) {
        return str;
    }
    // append string tail
    if (last < str.length()) {
        out.append(str.data() + last, str.length() - last);
    }
    // return result
    return out;
}


// Skip white spaces
#define SKIP_SPACES \
        while (i < len  &&  isspace((unsigned char)str[i])) ++i


void CRegexpTemplateTester::x_Op_Set(CTempString str)
{
    SIZE_TYPE len = str.length();
    SIZE_TYPE i = 4; // length of "set "
    _ASSERT(i < len);

    SKIP_SPACES;
    SIZE_TYPE n = x_ParseVarName(str, i);
    if ( !n ) {
        ERROR_TEMPLATE(eOpErr, "SET: variable not specified");
    }
    string name = str.substr(i, n);
    i += n;
    if (str[i] != '=') {
        ERROR_TEMPLATE(eOpErr, "SET: no assignment operator");
    }
    if (i >= len) {
        ERROR_TEMPLATE(eOpErr, "SET: variable cannot have empty value");
    }
    // Substitute all variables with its values
    m_Vars[name] = x_SubstituteVars(str.substr(++i), NULL);
}


void CRegexpTemplateTester::x_Op_Echo(CTempString str)
{
    SIZE_TYPE len = str.length();
    SIZE_TYPE i = 5; // length of "echo "
    SKIP_SPACES;
    string s = x_SubstituteVars(str.substr(i), NULL);
    cout << s << endl;
}


void CRegexpTemplateTester::x_Op_Test(CTempString str)
{
    enum ETestOp {
        eEqual,
        eNotEqual,
        eMatch,
        eNotMatch
    };
    SIZE_TYPE len = str.length();
    SIZE_TYPE i = 5; // length of "test "
    _ASSERT(i < len);

    SKIP_SPACES;

    ETestOp   op;
    SIZE_TYPE pop;

    if ((pop = NStr::Find(str, "==", i)) != NPOS) {
        op = eEqual;
    } else 
    if ((pop = NStr::Find(str, "!=", i)) != NPOS) {
        op = eNotEqual;
    } else 
    if ((pop = NStr::Find(str, "=~", i)) != NPOS) {
        op = eMatch;
    } else 
    if ((pop = NStr::Find(str, "!~", i)) != NPOS) {
        op = eNotMatch;
    } else {
        ERROR_TEMPLATE(eOpErr, "TEST: no comparison operator");
    }
    string sop = str.substr(pop, 2);
    string s1 = x_SubstituteVars(str.substr(i, pop - i), NULL);
    string s2 = x_SubstituteVars(str.substr(pop+2), NULL);

    bool res;
    switch (op) {
        case eEqual:
            res = (s1 == s2);
            break;
        case eNotEqual:
            res = (s1 != s2);
            break;
        case eMatch:
        case eNotMatch:
            {{
                CRegexp re(s2);
                if (re.IsMatch(s1)) {
                    res = (op == eMatch);
                } else {
                    res = (op == eNotMatch);
                }
            }}
            break;
    }
    if (!res) {
        ERROR_TEMPLATE(eOpTest, "TEST: result of comparison is FALSE: '" +
                                NStr::PrintableString(s1) + "' " + sop + " '" +
                                NStr::PrintableString(s2) + "'");
    }
    return;
}


void CRegexpTemplateTester::x_Op_Include(CTempString str, istream& file_stm)
{
    SIZE_TYPE len = str.length();
    SIZE_TYPE i = 8; // length of "include "
    _ASSERT(i < len);

    SKIP_SPACES;

    string path;
    CFile::SplitPath(m_TemplateName, &path);
    path = CFile::ConcatPath(path, str.substr(i));

    CNcbiIfstream template_stm(path.c_str());
    if (!template_stm.good()) {
        ERROR_TEMPLATE(eOpErr, "INCLUDE: cannot open file: " + path);
    }

    // Save context for current template
    string    saved_name = m_TemplateName;
    SIZE_TYPE saved_line = m_TemplateLineNum;

    // Reset
    m_TemplateName = path;
    m_TemplateLineNum = 0;

    // Continue comparing 
    x_Compare(file_stm, template_stm);

    // Restore saved context
    m_TemplateName = saved_name;
    m_TemplateLineNum = saved_line;

    return;
}


void CRegexpTemplateTester::x_Op_Skip(CTempString str, istream& file_stm)
{
    enum ESkipOp {
        eNumber,
        eWhile,
        eUntil
    };
    SIZE_TYPE len = str.length();
    SIZE_TYPE i = 5; // length of "skip "
    _ASSERT(i < len);

    SKIP_SPACES;
    CTempString op_str  = str.substr(i);
    CTempString op_name;

    ESkipOp     op;
    SIZE_TYPE   op_num = 0;
    CTempString re_str;

    if (NStr::StartsWith(op_str, "while")) {
        // # skip while <regexp>
        op = eWhile;
        op_name = op_str.substr(0, 5);
        i += 5;
        if (str[i] == ' ') {
            SKIP_SPACES;
            re_str = str.substr(i);
        }
    } 
    else 
    if (NStr::StartsWith(op_str, "until")) {
        // # skip until <regexp>
        op = eUntil;
        op_name = op_str.substr(0, 5);
        i += 5;
        if (str[i] == ' ') {
            SKIP_SPACES;
            re_str = str.substr(i);
        }
    } 
    else {
        // # skip <num>
        op = eNumber;
        try {
            op_num = NStr::StringToNumeric<SIZE_TYPE>(op_str);
        } 
        catch (CStringException&) {}
        if (!op_num) {
            ERROR_TEMPLATE(eOpErr, "SKIP: expected number of lines");
        }
    }

    AutoPtr<CRegexp> re;
    string re_pattern;

    if (op == eWhile || op == eUntil) {
        re_pattern = x_SubstituteVars(re_str, NULL);
        if (re_str.empty()) {
            ERROR_TEMPLATE(eOpErr, "SKIP: expected regular expression after '" +
                           (string)op_name + "'");
        }
        re.reset(new CRegexp(re_pattern));
    }

    // Skip lines in the input data

    SIZE_TYPE num_lines = 0;
    bool      matched = false;
    
    while (x_GetLine(file_stm, eFile))
    {
        ++num_lines;

        if (op == eNumber) {
            if (num_lines == op_num) {
                break;
            }
        } else {
            matched = re->IsMatch(m_FileLine);
            if (op == eWhile) {
                // revert
                matched = !matched;
            }
            if (matched) {
                break;
            }
        }
    }

    // Check that we skipped all requested number of lines or
    // found the line matched (or not) to the regular expression.
    if (op == eNumber) {
        if (num_lines != op_num) {
            ERROR_TEMPLATE(eMismatchLength, "SKIP: unable to skip " +
                           NStr::NumericToString(num_lines ) + " lines");
        }
    } else {
        if (!matched) {
            ERROR_TEMPLATE(eMismatchLength, "SKIP: unable to skip " +
                           (string)op_name + " '" + 
                           NStr::PrintableString(re_pattern) + "'");
        }
        // For the while/until cases we should be able to process current
        // 'stopper' file line also, it can be compared with the next template
        // 'data' line.
        m_ReprocessFileLine = true;
    }
    return;
}



/////////////////////////////////////////////////////////////////////////////
///
/// CRegexpTemplateTesterException --
///

const char* CRegexpTemplateTesterException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
        case eOpenFile:         return "eOpenFile";
        case eMismatchLength:   return "eMismatchLength";
        case eMismatchContent:  return "eMismatchContent";
        case eVarNotFound:      return "eVarNotFound";
        case eVarErr:           return "eVarErr";
        case eOpUnknown:        return "eOpUnknown";
        case eOpErr:            return "eOpErr";
        case eOpTest:           return "eOpTest";
        default:                return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
