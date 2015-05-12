#ifndef UTIL___REGEXP_TEMPLATE_TESTER__HPP
#define UTIL___REGEXP_TEMPLATE_TESTER__HPP

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
 * Author: Vladimir Ivanov
 *
 */

/// @file regexp_template_tester.hpp
/// Regexp template tester based on the Perl Compatible Regular Expression
/// (pcre) library. Allow to test delimited text data against template
/// with regular expression rules.

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup Regexp
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////
///
/// CRegexpTemplateTester -- 
///
/// Match delimited text data against template with regular expression rules.
///
/// Template file can have some types of lines:
///    1) comment line, started with // symbols (can be changed);
///    2) command lines, started with # symbol (can be changed);
///    3) regular expressions, that are used to match corresponding line
///       in the tested file/stream. Usually number of lines in the tested file
///       should be the same as the number of rule lines in the template,
///       unless you use 'stop' or 'skip' template commands (see below).
///
/// Template commands allow to use variables. Each variable should be surrounded
/// with figure brackets with preceding $ symbol, like ${var}. Except 'set'
/// command where we define variable. Note that this can be changed and you can use
/// any other symbols to separate variables.
///
/// Commands:
///
///    1) set var=value
///          Define variable 'var', set it to 'value'.
///          'value' can be string, any other variable or combination of it.
///          This type of variable works almost as constants in the C++,
///          they can be useful to predefine some frequently used values
///          in the regular expression rules. All variable names starts with
///          a letter and can have numbers, letters, '-' or '_'.
///          Example:
///              # set fruit=apple
///              # set color=red
///              # set str=${color} ${fruit}
///              // variable 'str' have value 'red apple' now
///              This is a rule line with a variable ${str}
///          
///    2) echo <string>
///          Prints string/variables to stdout. Can be useful for debug purposes.
///          Example:
///              # set fruit=apple
///              # echo some string
///              # echo ${fruit}
///              # echo We have an ${fruit}
///
///    3) test <expression>
///          Compare variable with a string value.
///          'value' can be string, any other variable or combination of it.
///          Allowed comparing operators: 
///              ==  - left value         equal to right value
///              !=  - left value doesn't equal to right value.
///              =~  - left value         match to regexp on right
///              !~  - left value doesn't match to regexp on right.
///          Spaces around the comparison operator counts as part of 
///          the comparing strings on left and right accordingly.
///          Example:
///              # set fruit=apple
///              # set color=red
///              # test ${fruit}==apple
///              # test ${fruit}!=orange
///              # test red ${fruit}==red apple
///              # test ${color} ${fruit}==red apple
///              # test ${fruit}=~\w{3,}
///              # set re=\w{3,}
///              # test ${fruit}=~${re}
///
///    4) include path
///          Include sub-template with name 'path' and process it.
///          This allow to split big templates to parts, or separate common 
///          part of many templates. 'path' defines a relative path to
///          the sub-template from the directory of the template that includes it.
///          Sub-templates can include any other templates.
///          Example:
///              # include common_header.tpl
///              # include tests/test1
///              # include tests/test2
///
///   5) inline variables
///          One more way to define variables based on the data in the matching
///          string. For example, comparing file can have such line:
///              We have 5 apples.
///
///          Corresponding template file may have next rules:
///              We have ${count=[1-9]+} ${fruit=[a-z]+}\.
///              # test ${count}==5
///              # test ${fruit}==apples
///
///   6) skip <expression>
///          Allow to skip lines in the tested file/stream.
///          It is possible to skip:
///            - fixed number of lines;
///            - while lines match specified regular expression (find not);
///            - until some line will match specified regular expression (find);
///          Regular expressions can use variables here as well.
///          Note, that for while/until cases, testing stops on a line that
///          don't/meet specified criteria. So you can parse it with a different
///          rule again. If you don't need that line you can use 'skip 1' command
///          on the next template line or a regular expression that matches any
///          string, like '.*'.
///          Example:
///              // Skip next 5 lines in the tested file/stream
///              # skip 5
///              // Skip all next lines started with a capital letter
///              # skip while ^[A-Z]+
///              // Skip all next lines until we find a string that have
///              // some digits followed with 'apple' word.
///              # set fruit=apple
///              # skip until \d+ ${fruit}
///              We have ${count=\d+} ${fruit}
///              # test ${count}==1
///
///   7) stop
///          Stops processing the current template. If located in the main template,
///          it forces Compare() methods to return immediatle. Can be useful for
///          debug purposes or if you want to check only some part of the file,
///          ignoring any lines below some point. Note, that if 'stop' command have
///          used in the sub-template, testing engine will stop processing that
///          sub-template only and continue processing from the next line in
///          the parent template.
///          Example:
///              # stop
///
/// Nested variables.
///
///   You can use nested variables in any expression and every command that support it.
///   They can be useful in some cases,  especially if you want to use the same symbols
///   used to define variables. For example, you can write next template line:
///
///          We have ${count=[1-9]+} ${fruit=[a-z]+}\.
///
///   It have correct syntax, but what if you want to make it a bit complex or strict?
///   For example next template line should generate parsing errors:
///
///          // Match 2 or more digit numbers and fruits names with 4 letters at least
///          We have ${count=\d{2,}} ${fruit=\w{4,}}\.
///
///   This is because {...} is used inside variable defnition. As solution you can
///   choice different symbols that will define the start and end sequences for all
///   variables, or use nested variables as specified below:
///
///          # set d2=\d{2,}
///          # set w4=\w{4,}
///          We have ${count=${d2}} ${fruit=${w4}}\.
///
///   Also, you can construct variable names on the fly:
///
///          # set color=red
///          # echo ${${color}_${apple}}
///
///
/// Note, that all methods throw CRegexpTemplateTesterException on errors,
/// problems with parsing template commands or data mismatch.
///

class CRegexpTemplateTester
{
public:
    /// 
    enum EFlags {
        fSkipEmptySourceLines   = (1 << 0),  ///< Skip empty lines in the source
        fSkipEmptyTemplateLines = (1 << 1),  ///< Skip empty lines in the template
        fSkipEmptyLines = fSkipEmptySourceLines | fSkipEmptyTemplateLines 
    };
    typedef unsigned int TFlags;    ///< Binary OR of "EFlags"

    /// Default constructor.
    CRegexpTemplateTester(TFlags flags = 0);


    /// Compare file against template (file version).
    ///
    /// @param file_path
    ///   Path to the checking file.
    /// @param template_path
    ///   Path to the corresponding template.
    /// @return
    ///   Nothing on success. 
    ///   Throw CRegexpTemplateTesterException on error or mismatch.
    ///
    void Compare(const string& file_path, const string& template_path);

    /// Compare file against template (stream version).
    ///
    /// @param file_stream
    ///   Input stream with checking data.
    /// @param template_stream
    ///   Input stream with corresponding template data.
    /// @return
    ///   Nothing on success. 
    ///   Throw CRegexpTemplateTesterException on error or mismatch.
    /// @note
    ///   Due to file-oriented nature of the 'include' command in the
    ///   templates, it works a little different that in the file-based
    ///   version.  It is better do not use 'include' with streams at all,
    ///   but if you want it, be aware.  We don't have path to the directory
    ///   with the original template, so included sub-template should be
    ///   located in the current directory.
    ///
    void Compare(istream& file_stream, istream& template_stream);


    // --- Auxiliary methods used to tune up parsing for specific needs.

    /// Change strings defining start/end of variables.
    /// By default use next syntax: ${var}
    void SetVarScope(string& start, string& end);

    /// Change string defining start of comments line in templates.
    /// Default value: "//"
    void SetCommentStart(string& str);

    /// Change string defining start of template commands and operations.
    /// Default value: "#"
    void SetCommandStart(string& str);

    /// Change delimiters string, used for comparing data and templates.
    /// Default value: "\r\n"
    void SetDelimiters(string& str);


    // --- Debug functions

    void   PrintVars(void) const;
    void   PrintVar (const string& name) const;
    string GetVar   (const string& name) const;

    typedef map<string, string> TVarMap;
    const TVarMap& GetVars(void) const;

private:
    // Operations
    void  x_Op_Set     (CTempString str);
    void  x_Op_Echo    (CTempString str);
    void  x_Op_Test    (CTempString str);
    void  x_Op_Include (CTempString str, istream& file_stm);
    void  x_Op_Skip    (CTempString str, istream& file_stm);

private:
    // Internal methods

    /// Reset object state
    void x_Reset(void);

    /// Processing source
    enum ESource {
        eFile,     ///< source file/stream
        eTemplate  ///< template
    };

    /// The reason of stopping x_Compare(), if no error.
    enum EResult {
        eTemplateEOF,
        eStop
    };
    typedef list<string> TVarList;

    /// Main compare method, compare streams.
    /// Can be used recursively to process includes.
    /// Return TRUE if 'stop' command found.
    EResult x_Compare(istream& file_stream, istream& template_stream);

    /// Process/compare lines.
    void x_CompareLines(CTempString file_line, CTempString template_line);

    /// Parse variable from string, return its length.
    /// Can process nested variables.
    SIZE_TYPE x_ParseVar(CTempString str, SIZE_TYPE pos) const;

    /// Parse/check variable name from string, return its length.
    SIZE_TYPE x_ParseVarName (CTempString str, SIZE_TYPE pos) const;

    /// Replace all variables in the string with corresponding values.
    /// Also used for preparing inline variables for consecutive regexp matching.
    string x_SubstituteVars(CTempString str, TVarList* inline_vars) const;

    /// Get line from the stream 'is'.
    istream& x_GetLine(istream& is, ESource src);

private:
    TFlags    m_Flags;           ///< Processing flags
    string    m_VarStart;        ///< Variable definition start
    string    m_VarEnd;          ///< Variable definition end
    string    m_OpStart;         ///< Start of the template command line
    string    m_CommentStart;    ///< Start of the comment line
    string    m_EOLs;            ///< Lines delimiters
    TVarMap   m_Vars;            ///< Map of variables/values

    // Variables for currently processing file/(sub-)template
    string    m_FileName;        ///< Current file name (if any)
    string    m_FileLine;        ///< Currently processing file line
    SIZE_TYPE m_FileLineNum;     ///< Current file/stream line number
    string    m_TemplateName;    ///< Current template name
    string    m_TemplateLine;    ///< Currently processing template line
    SIZE_TYPE m_TemplateLineNum; ///< Current template line number

    bool  m_ReprocessFileLine;  ///< TRUE if m_FileLine should be reprocessed
                                ///< with next template data line
};



/////////////////////////////////////////////////////////////////////////////
///
/// CRegexpTemplateTesterException --
///
/// Define exceptions generated by CRegexpTemplateTester.

class NCBI_XNCBI_EXPORT CRegexpTemplateTesterException : public CCoreException
{
public:
    /// Error types that tester can generate.
    enum EErrCode {
        eOpenFile,         ///< file open error
        eMismatchLength,   ///< file/template line number mismatch
        eMismatchContent,  ///< file/template lines do not match
        eVarNotFound,      ///< variable not found
        eVarErr,           ///< variable definition error
        eOpUnknown,        ///< unknown operation
        eOpErr,            ///< operation definition error
        eOpTest            ///< 'test' operation return FALSE
    } ;
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CRegexpTemplateTesterException, CCoreException);
};


/* @} */

END_NCBI_SCOPE


#endif /* UTIL___REGEXP_TEMPLATE_TESTER__HPP */
