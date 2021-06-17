#ifndef TRAVERSALSPECFILEPARSER__HPP
#define TRAVERSALSPECFILEPARSER__HPP

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
* Author: Michael Kornbluh
*
* File Description:
*   Parses the description files used by the cleanup_code_generator.
*   See the .cpp file for a detailed description of the format 
*   of the specification file.
*/

#include <vector>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistre.hpp>

BEGIN_NCBI_SCOPE

class CTraversalSpecFileParser {
public:

    // parses on construction
    CTraversalSpecFileParser( CNcbiIstream &istream );

    typedef std::vector<std::string> TPattern;
    typedef std::vector<TPattern> TPatternVec;

    // represents one unit of a "use-clause"
    class CDescFileNode : public CObject {
    public:
        enum EWhen {
            eWhen_afterCallees = 1,
            eWhen_beforeCallees            
        };

        CDescFileNode( const string &func, const string &pattern, 
            const std::vector<std::string> &except_patterns,
            const std::vector<std::string> &arg_patterns,
            const std::vector<std::string> &constant_args,
            EWhen when );            

        const std::string& GetFunc(void) const { return m_Func; }
        const TPattern & GetPattern(void) const { return m_Pattern; }
        const TPatternVec & GetExceptPatterns(void) const { return m_ExceptPatterns; }
        const TPatternVec & GetArgPatterns(void) const { return m_ArgPatterns; }
        const vector<string> & GetConstantArgs(void) const { return m_ConstantArgs; }
        EWhen GetWhen(void) const { return m_When; }
        int GetID(void) const { return m_ID; }

        string ToString(void);

        void ConvertToMemberMacro(void);

    private:
        const std::string m_Func;
        TPattern m_Pattern;
        TPatternVec m_ExceptPatterns;
        TPatternVec m_ArgPatterns;
        vector<string> m_ConstantArgs;
        const int m_ID; // unique ID.  Needed for proper ordering
        const EWhen m_When;

        static int ms_HighestID; // help assign next ID
    };

    typedef CRef<CDescFileNode> CDescFileNodeRef;

    // represents a "root" clause
    struct SRootInfo : public CObject {
        SRootInfo( const std::string &root_type_name,
            const std::string &root_func_name ) :
        m_Root_type_name(root_type_name), m_Root_func_name(root_func_name) { }

        const std::string m_Root_type_name; // e.g. "Seq-id"
        const std::string m_Root_func_name; // e.g. "BasicCleanupSeqEntry"
    };
    typedef CConstRef<SRootInfo> TRootInfoRef;
    typedef std::vector<TRootInfoRef> TRootInfoRefVec;

    // represents a "member" clause
    struct SMember : public CObject {
        SMember( const std::string &type_name, const std::string &variable_name )
            : m_Type_name(type_name), m_Variable_name(variable_name) { }

        const std::string m_Type_name;
        const std::string m_Variable_name;
    };
    typedef CConstRef<SMember> TMemberRef;
    typedef std::vector<TMemberRef> TMemberRefVec;

    const std::string &GetOutputFileHeader(void) const { return m_OutputFileHeader; }
    const std::string &GetOutputFileSource(void) const { return m_OutputFileSource; }
    const std::string &GetOutputClassName(void) const { return m_OutputClassName; }
    const TRootInfoRefVec &GetRootTypes(void) const { return m_RootTypes; }
    const std::vector< CRef<CDescFileNode> > &GetDescFileNodes(void) const { return m_DescFileNodes; }
    const std::vector<std::string> &GetHeaderIncludes(void) const { return m_HeaderIncludes; }
    const std::vector<std::string> &GetSourceIncludes(void) const { return m_SourceIncludes; }
    const TMemberRefVec &GetMembers(void) const { return m_Members; }
    const std::vector<std::string> &GetHeaderForwardDeclarations(void) const { return m_HeaderForwardDeclarations; }
    const TPatternVec & GetDeprecatedPatterns(void) { return m_DeprecatedPatterns; }
    const std::vector<std::string> &GetNamespace(void) const { return m_Namespace; }

    bool IsPruningAllowed(void) const { return m_IsPruningAllowed; }
    bool IsMergingAllowed(void) const { return m_IsMergingAllowed; }

private:
    // thrown when we reach the end, if we're in "throw" mode
    class CNoMoreTokens : public std::exception { 
    };

    // thrown on parsing error (e.g. mismatched braces, invalid func name, etc. )
    class CParseError : public std::runtime_error { 
    public:
        CParseError( const string &msg ) : std::runtime_error(msg.c_str()) { }
    };

    // grabs tokens from istream
    // should this keep track of line numbers, too?
    class CTokenizer {
    public:
        CTokenizer( CNcbiIstream &istream ) : m_Istream(istream) { }

        // returns false if it couldn't get any
        // (only used at top level when running out of tokens is legitimate)
        bool GetNextNoThrow( string &out_next_token );
        // throws if it couldn't get any
        void GetNextOrThrow( string &out_next_token );

        // returns true if the next token will be the expected_token
        // (throws if no more tokens available )
        bool NextWillBe( const string &expected_token );

        // Discards one token.  Throws if the token is not what it should_be.
        // (should_be ignored if NULL)
        void DiscardOne( const char* should_be = NULL );

        // loads tokens into out_tokens until we get expected_token 
        // (expected_token is not loaded, but thrown away)
        void GetUntil( std::vector<std::string> &out_tokens, const string &expected_token );

    private: 

        // reloads m_NextTokens if possible (and needed)
        // returns true if m_NextTokens has items when done
        bool x_TryToGetTokensIfNone(void);

        // loads from m_IsStream into next_word until last_char is found
        // (does not add last character)
        // Backslashes not yet supported (treated same as any character).
        void x_ReadUntil( string &next_word, char last_char );

        // when we read a line, we will often get extra tokens, so we store them here
        std::list<string> m_NextTokens;
        CNcbiIstream &m_Istream;
    };

    void x_ParseOutputFiles( CTokenizer &tokenizer );
    void x_ParseOutputClassNameClause( CTokenizer &tokenizer );
    void x_ParseUseClause( CTokenizer &tokenizer );
    void x_ParseUseExceptClause( CTokenizer &tokenizer, 
        std::vector<std::string> &out_except_patterns );
    void x_ParseUseArgClause( CTokenizer &tokenizer, 
        const string &main_pattern,
        std::vector<std::string> &out_arg_patterns );
    void x_ParseRootClause( CTokenizer &tokenizer );
    void x_ParseMemberClause( CTokenizer &tokenizer );
    void x_ParseInclude( std::vector< std::string > &include_list, 
        CTokenizer &tokenizer );
    void x_ParseHeaderForwardDeclarationClause( CTokenizer &tokenizer );
    void x_ParseMemberMacro( CTokenizer &tokenizer );
    void x_ParseDeprecated( CTokenizer &tokenizer );

    bool x_IsValidPattern( const std::string & pattern );

    std::string m_OutputFileHeader;
    std::string m_OutputFileSource;
    std::string m_OutputClassName;
    TRootInfoRefVec m_RootTypes;
    std::vector< CRef<CDescFileNode> > m_DescFileNodes;
    std::vector< std::string > m_HeaderIncludes;
    std::vector< std::string > m_SourceIncludes;
    TMemberRefVec m_Members;
    std::vector< std::string > m_HeaderForwardDeclarations;
    TPatternVec m_DeprecatedPatterns;
    std::vector< std::string > m_Namespace;

    bool m_IsPruningAllowed;
    bool m_IsMergingAllowed;
};

END_NCBI_SCOPE

#endif /* TRAVERSALSPECFILEPARSER__HPP */
