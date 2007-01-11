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

BEGIN_NCBI_SCOPE

class CQueryParseTree;



/** @addtogroup QParser
 *
 * @{
 */


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
        eNotSet,      ///< Produced by the (private) default constructor
        eIdentifier,  ///< Identifier like db.field (Org, Fld12, etc.)
        eIntConst,    ///< Integer const
        eFloatConst,  ///< Floating point const
        eBoolConst,   ///< Boolean (TRUE or FALSE)
        eString,      ///< String ("free text")
        eUnaryOp,     ///< Unary operator (NOT)
        eBinaryOp     ///< Binary operator
    };

    /// Unary operator 
    ///
    enum EUnaryOp {
        eNot
    };
    
    /// Binary operator
    ///
    enum EBinaryOp {
        eAnd,
        eOr,
        eSub,
        eNot2,
        eXor,
        eEQ,
        eGT,
        eGE,
        eLT,
        eLE
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
    explicit CQueryParseNode(EBinaryOp op, const string& orig_text);
    explicit CQueryParseNode(EUnaryOp  op, const string& orig_text);
    
    /// @name Source reference accessors
    /// @{  
    
    /// Set node location in the query text (for error diagnostics)
    void SetLoc(const SSrcLoc& loc) { m_Location = loc; }
    const SSrcLoc& GetLoc() const   { return m_Location; }
    
    /// @}


    /// @name Value accessors
    /// @{  
    
    EType     GetType() const { return m_Type; }
    EBinaryOp GetBinaryOp() const;
    EUnaryOp  GetUnaryOp() const;
    const string& GetStrValue() const;
    const string& GetIdent() const;
    const string& GetOriginalText() const { return m_OrigText; }
    Int4 GetInt() const;
    bool GetBool() const;
    double GetDouble() const;
    
    /// For eIdentifier nodes we can assign identifier's index 
    /// (For database fields this could be a field index)
    ///
    void SetIdentIdx(int idx);  
        
    /// Get index of eIdentifier
    ///
    int GetIdentIdx() const;
    
    /// @}
    
    /// TRUE if node was created as explicitly 
    /// FALSE - node was created as a result of a default and the interpreter has
    ///         a degree of freedom in execution
    bool IsExplicit() const { return m_Explicit; }  
    void SetExplicit(bool expl=true) { m_Explicit = expl; }
    
    /// Check if node is marked with NOT flag (like != )
    bool IsNot() const { return m_Not; }
    void SetNot(bool n=true) { m_Not = n; }
private:
    // required for use with CTreeNode<>
    CQueryParseNode();
    friend class CTreeNode<CQueryParseNode>;
    
private:
    EType         m_Type;  
    union {
        EUnaryOp     m_UnaryOp;
        EBinaryOp    m_BinaryOp;
        Int4         m_IntConst;
        bool         m_BoolConst;
        double       m_DoubleConst;
        int          m_IdentIdx;
    };    
    string        m_Value;
    string        m_OrigText; 
    bool          m_Explicit;
    bool          m_Not;
    SSrcLoc       m_Location;
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


    enum ECase {
        eCaseSensitiveUpper, ///< Operators must come in upper case (AND)
        eCaseInsensitive     ///< Case insensitive parsing (AnD)
    };


    /// Query parser front-end function
    ///
    /// @param query_str
    ///    Query string subject of parsing
    /// @param case_sense
    ///    Case sensitivity (AND, AnD, etc.)
    /// @param verbose
    ///    Debug print switch
    ///
    void Parse(const char* query_str, 
               ECase       case_sense = eCaseInsensitive,
               bool        verbose    = false);

    
    /// Replace current query tree with the new one.
    /// CQueryParseTree takes ownership on the passed argument.
    ///
    void SetQueryTree(TNode* qtree);
    const TNode* GetQueryTree() const { return m_Tree.get(); }
    
        
    /// @name Static node creation functions - 
    ///       class factories working as virtual constructors
    /// @{

    /// Create Identifier node or string node
    static 
    TNode* CreateNode(const string&  value, 
                      const string&  orig_text, 
                      bool           isIdent);
    static TNode* CreateNode(Int4   value, const string&  orig_text);
    static TNode* CreateNode(bool   value, const string&  orig_text);
    static TNode* CreateNode(double value, const string&  orig_text);
                      
    static 
    TNode* CreateBinaryNode(CQueryParseNode::EBinaryOp op,
                            TNode*                     arg1, 
                            TNode*                     arg2,
                            const string&              orig_text="");
    static 
    TNode* CreateUnaryNode(CQueryParseNode::EUnaryOp op, 
                           TNode*                    arg,
                           const string&             orig_text="");

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
        eCompileError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eIncorrectNodeType:       return "eIncorrectNodeType";
        case eParserError:             return "eParserError";
        case eCompileError:            return "eCompileError";
            
        default: return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CQueryParseException, CException);
};


/* @} */

END_NCBI_SCOPE


#endif  // UTIL__QUERY_PARSER_HPP__


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2007/01/11 14:49:20  kuznets
 * Many cosmetic fixes and functional development
 *
 * Revision 1.2  2007/01/11 01:04:13  ucko
 * Give CQueryParseNode a private default constructor, as a formality for
 * CTreeNode<CQueryParseNode> (granting the latter friend-level access).
 * Rename "identificator" to the proper word "identifier".
 * Indent with spaces, not tabs.
 *
 * Revision 1.1  2007/01/10 16:11:38  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
