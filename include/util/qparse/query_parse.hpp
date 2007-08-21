#ifndef UTIL__QUERY_PARSER_HPP__
#define UTIL__QUERY_PARSER_HPP__

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
 * Authors: Anatoliy Kuznetsov, Mike DiCuccio, Maxim Didenko
 *
 * File Description: Query parser implementation
 *
 */

/// @file query_parse.hpp
/// Query string parsing components

#include <corelib/ncbi_tree.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class CQueryParseTree;



/** @addtogroup QParser
 *
 * @{
 */

/// Base class for query node user defined object
///
/// User object used to carry field dependent data, metainformation,
/// execution time data, etc. It can be a bridge between parser and a query
/// execution engine.
///

class NCBI_XUTIL_EXPORT IQueryParseUserObject : public CObject
{
public:
    
    /// Reset user object (for reuse without reallocation)
    virtual void Reset() = 0;
    
    /// String value for debuging
    virtual string GetVisibleValue() const { return ""; };
};


/// Query node class
/// 
/// Query node describes element of the recursive parsing tree 
/// for the query language. 
/// (The tree is then interpreted by the execution machine)
///

class NCBI_XUTIL_EXPORT CQueryParseNode 
{
public:

   /// Query node type
   ///
    enum EType {
        eNotSet     = 0,   ///< Produced by the (private) default constructor
        eIdentifier,       ///< Identifier like db.field (Org, Fld12, etc.)
        eIntConst,         ///< Integer const
        eFloatConst,       ///< Floating point const
        eBoolConst,        ///< Boolean (TRUE or FALSE)
        eString,           ///< String ("free text")
        eFunction,         ///< Function

        // Operation codes:
        eNot,
        eFieldSearch,
        eLike,
        eBetween,
        eIn,
        eAnd,
        eOr,
        eSub,
        eXor,
        eRange,
        eEQ,
        eGT,
        eGE,
        eLT,
        eLE,
        
        // SQL specific components
        eSelect,
        eFrom,
        eWhere,
        
        eList,
        
        eMaxType
    };
    
    /// Source location (points to the position in the original src)
    /// All positions are 0 based
    ///
    struct SSrcLoc
    {
        unsigned   line;     ///< Src line number
        unsigned   pos;      ///< Position in the src line
        unsigned   length;   ///< Token length (optional)
        
        SSrcLoc(unsigned src_line = 0, unsigned src_pos = 0, unsigned len = 0)
        : line(src_line), pos(src_pos), length(len)
        {}
    };
    
public:
    /// Construct the query node
    /// @param value      Node value
    /// @param orig_text  Value as it appears in the original program
    /// @param isIdent    true whe the string is identifier (no quoting)
    ///
    CQueryParseNode(const string& value, const string& orig_text, bool isIdent);

    explicit CQueryParseNode(Int4   val, const string& orig_text);
    explicit CQueryParseNode(bool   val, const string& orig_text);
    explicit CQueryParseNode(double val, const string& orig_text);
    explicit CQueryParseNode(EType op_type, const string& orig_text);
    
    /// @name Source reference accessors
    /// @{  
    
    /// Set node location in the query text (for error diagnostics)
    void SetLoc(const SSrcLoc& loc) { m_Location = loc; }
    void SetLoc(unsigned line, unsigned pos) 
    { 
        m_Location.line = line;
        m_Location.pos = pos;
    }
    const SSrcLoc& GetLoc() const   { return m_Location; }
    
    /// @}


    /// @name Value accessors
    /// @{  
    
    EType     GetType() const { return m_Type; }
    const string& GetStrValue() const;
    const string& GetIdent() const;
    const string& GetOriginalText() const { return m_OrigText; }
    Int4 GetInt() const;
    bool GetBool() const;
    double GetDouble() const;
    
    int GetIdentIdx() const;    
    const string& GetOrig() const { return m_OrigText; }
    
    /// @}
    
    /// TRUE if node was created as explicitly 
    /// FALSE - node was created as a result of a default and the interpreter has
    ///         a degree of freedom in execution
    bool IsExplicit() const { return m_Explicit; }  
    void SetExplicit(bool expl=true) { m_Explicit = expl; }
    
    /// Check if node is marked with NOT flag (like != )
    bool IsNot() const { return m_Not; }
    void SetNot(bool n=true) { m_Not = n; }
    
    /// Returns TRUE if node describes logical operation (AND, OR, etc.)
    bool IsLogic() const 
    { 
        return m_Type == eNot || m_Type == eAnd || m_Type == eOr ||
               m_Type == eSub || m_Type == eXor; 
    }

    /// Returns TRUE if node is value (INT, String, etc.)
    bool IsValue() const 
    { 
        return m_Type == eIdentifier || m_Type == eIntConst   || 
               m_Type == eString     || m_Type == eFloatConst || 
               m_Type == eBoolConst; 
    }
    
    /// Elapsed time in seconds
    double Elapsed() const { return m_Elapsed; }
    /// Elapsed time in seconds
    double GetElapsed() const { return Elapsed(); }
    
    /// Set node timing
    void SetElapsed(double e) { m_Elapsed = e; }
    
    
    /// @name User object operations
    ///
    /// Methods to associate application specific data with
    /// parsing tree node.
    /// Data should be encapsulated into a user object derived
    /// from CQueryParseBaseUserObject.
    ///
    /// @{
    
    /// Get user object
    const IQueryParseUserObject* GetUserObject() const
                                    { return m_UsrObj.GetPointer(); }  
    IQueryParseUserObject* GetUserObject()
                                    { return m_UsrObj.GetPointer(); }  
    
    /// Set user object. Query node takes ownership.    
    void AttachUserObject(IQueryParseUserObject* obj);
    void SetUserObject(IQueryParseUserObject* obj)
                                    { AttachUserObject(obj); }

    /// Reset the associated user object 
    ///   (see IQueryParseUserObject::Reset())
    ///                                  
    void ResetUserObject();
    
    /// @}
    
private:
    // required for use with CTreeNode<>
    CQueryParseNode();
    friend class CTreeNode<CQueryParseNode>;
    
private:
    EType         m_Type;  
    union {
        Int4         m_IntConst;
        bool         m_BoolConst;
        double       m_DoubleConst;
    };    
    string        m_Value;
    string        m_OrigText; 
    bool          m_Explicit;
    bool          m_Not;
    SSrcLoc       m_Location;   ///< Reference to original location in query
    double        m_Elapsed;    ///< Execution timing
    
    CRef<IQueryParseUserObject>  m_UsrObj;
};


/// Query tree and associated utility methods
///
class NCBI_XUTIL_EXPORT CQueryParseTree
{
public:
    typedef CTreeNode<CQueryParseNode> TNode;
public:
    /// Contruct the query. Takes the ownership of the clause.
    explicit CQueryParseTree(TNode *clause=0);
    virtual ~CQueryParseTree();


    /// Case sensitive parsing
    ///
    enum ECase {
        eCaseSensitiveUpper, ///< Operators must come in upper case (AND)
        eCaseInsensitive     ///< Case insensitive parsing (AnD)
    };

    /// Level of tolerance to syntax errors and problems
    ///
    enum ESyntaxCheck {
        eSyntaxCheck,      ///< Best possible check for errors
        eSyntaxRelax       ///< Relaxed parsing rules
    };
    
    /// List of keywords recognised as functions
    typedef vector<string> TFunctionNames;

    /// Query parser front-end function
    ///
    /// @param query_str
    ///    Query string subject of parsing
    /// @param case_sense
    ///    Case sensitivity (AND, AnD, etc.)
    /// @param syntax_check
    ///    Sensitivity to syntax errors
    /// @param verbose
    ///    Debug print switch
    /// @param functions
    ///    List of names recognised as functions
    ///
    void Parse(const char*   query_str, 
               ECase         case_sense        = eCaseInsensitive,
               ESyntaxCheck  syntax_check      = eSyntaxCheck,
               bool          verbose           = false,
               const TFunctionNames& functions =  TFunctionNames(0));

    
    /// Replace current query tree with the new one.
    /// CQueryParseTree takes ownership on the passed argument.
    ///
    void SetQueryTree(TNode* qtree);
    const TNode* GetQueryTree() const { return m_Tree.get(); }
    TNode* GetQueryTree() { return m_Tree.get(); }
    
    /// Reset all user objects attached to the parsing tree
    void ResetUserObjects();
    
        
    /// @name Static node creation functions - 
    ///       class factories working as virtual constructors
    /// @{

    /// Create Identifier node or string node
    virtual 
    TNode* CreateNode(const string&  value, 
                      const string&  orig_text, 
                      bool           isIdent);
    virtual TNode* CreateNode(Int4   value, const string&  orig_text);
    virtual TNode* CreateNode(bool   value, const string&  orig_text);
    virtual TNode* CreateNode(double value, const string&  orig_text);
    virtual 
    TNode* CreateNode(CQueryParseNode::EType op,
                      TNode*                 arg1, 
                      TNode*                 arg2,
                      const string&          orig_text="");
    /// Create function node
    virtual 
    TNode* CreateFuncNode(const string&  func_name);

    /// @}
    
    /// Print the query tree (debugging)
    void Print(CNcbiOstream& os) const;
    
private:
    CQueryParseTree(const CQueryParseTree&);
    CQueryParseTree& operator=(const CQueryParseTree&); 
private:
    auto_ptr<TNode> m_Tree;
};

/// Query parser exceptions
///
class NCBI_XUTIL_EXPORT CQueryParseException : public CException
{
public:
    enum EErrCode {
        eIncorrectNodeType,
        eParserError,
        eCompileError,
        eUnknownFunction
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eIncorrectNodeType:       return "eIncorrectNodeType";
        case eParserError:             return "eParserError";
        case eCompileError:            return "eCompileError";
        case eUnknownFunction:         return "eUnknownFunction";
            
        default: return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CQueryParseException, CException);
};


/* @} */

END_NCBI_SCOPE


#endif  // UTIL__QUERY_PARSER_HPP__


