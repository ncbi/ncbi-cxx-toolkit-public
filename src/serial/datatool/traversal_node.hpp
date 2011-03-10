#ifndef TRAVERSALNODE__HPP
#define TRAVERSALNODE__HPP

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
*   Represents one node in the traversal code (this gets translated into one function).
*/

#include <corelib/ncbiobj.hpp>
#include <vector>

#include "type.hpp"

BEGIN_NCBI_SCOPE

class CTraversalNode : public CObject {
public:

    class CNodeCall : public CObject {
    public:
        CNodeCall( const std::string &var_name,
            CRef<CTraversalNode> node ) :
            m_VarName( var_name ), m_Node( node )
        {
        }

        const std::string &GetVarName(void) const { return m_VarName; }
        CRef<CTraversalNode>GetNode(void) const { return m_Node; }

    private:
        const std::string m_VarName;
        const CRef<CTraversalNode> m_Node;
    };

    struct SRefNodeCallLessthan {
        bool operator()( const CRef<CNodeCall> ref1, const CRef<CNodeCall> ref2 ) const;
    };

    typedef std::set< CRef<CTraversalNode> > TNodeSet;
    typedef std::set< CRef<CNodeCall>, SRefNodeCallLessthan > TNodeCallSet;
    typedef std::vector< CRef<CTraversalNode> > TNodeVec;

    // factory
    static CRef<CTraversalNode> Create( CRef<CTraversalNode> caller, const string &var_name, CDataType *asn_node );
   
    // needed when we find a cycle.  Usually the parent caller is
    // just specified at creation-time.
    void AddCaller( const std::string &var_name, CRef<CTraversalNode> caller );

    // For depth-first iteration of the nodes
    class CDepthFirstCallback {
    public:
        virtual ~CDepthFirstCallback() { }

        enum ECallType {
            eCallType_NonCyclic = 0,
            eCallType_Cyclic
        };
        // should return false if we should not traverse farther (recommended if the call is cyclic, to avoid infinite loops )
        // node_path shows how we got here
        virtual bool Call( CTraversalNode& node, const TNodeVec &node_path, ECallType is_cyclic ) = 0;
    };
    // "or" these together
    enum FTraversalOpts {
        fTraversalOpts_Post        = 1<<0, // pre: call callback, then traverse. post: traverse, then call callback.
        fTraversalOpts_UpCallers   = 1<<1, // traverse backwards up callers, instead of downwards to callees
        fTraversalOpts_AllowCycles = 1<<2,  // if set, user has to check for cyclic calls
    };
    typedef unsigned int TTraversalOpts;
    void DepthFirst( CDepthFirstCallback &callback, TTraversalOpts traversal_opts = 0 );

    // writes the code for this function out to the given stream
    enum EGenerateMode {
        eGenerateMode_Prototypes = 0,
        eGenerateMode_Definitions
    };
    void GenerateCode( const string &func_class_name, CNcbiOstream& traversal_output_file, EGenerateMode generate_mode );

    void SplitByVarName(void);

    enum EType {
        eType_Sequence = 0,
        eType_Choice,
        eType_Primitive,
        eType_Null,
        eType_Enum,
        eType_Reference,
        eType_UniSequence
    };

    // represents a user function called by this node.
    class CUserCall : public CObject {
    public:
        CUserCall( const std::string &user_func_name,
            std::vector< CRef<CTraversalNode> > &extra_arg_nodes );

        const std::string &GetUserFuncName() const { return m_UserFuncName; }
        const TNodeVec &GetExtraArgNodes(void) const { return m_ExtraArgNodes; }

        bool operator==( const CUserCall & rhs ) const { return Compare(rhs) == 0; }
        bool operator!=( const CUserCall & rhs ) const { return Compare(rhs) != 0; }
        bool operator< ( const CUserCall & rhs ) const { return Compare(rhs) < 0; }
        int  Compare( const CUserCall &rhs ) const { return NStr::Compare(m_UserFuncName, rhs.m_UserFuncName); }
    private:
        const std::string m_UserFuncName;
        TNodeVec m_ExtraArgNodes;

        friend class CTraversalNode;
    };

    CRef<CTraversalNode> Ref() { return CRef<CTraversalNode>(this); }

    typedef std::vector< CRef<CUserCall> > TUserCallVec;

    const std::string &GetFuncName(void) const { return m_FuncName; }
    const TNodeCallSet & GetCallers(void) const { return m_Callers; }
    const TNodeCallSet & GetCallees(void) const { return m_Callees; }
    const std::string &GetTypeName() const { return m_TypeName; }
    EType GetType() const { return m_Type; }
    const string &GetTypeAsString(void) const;
    const std::string &GetIncludePath(void)  const { return m_IncludePath;  }
    const std::string &GetInputClassName(void)   const { return m_InputClassName;   }
    bool IsTemplate(void) const { return m_IsTemplate; }
    const TUserCallVec &GetPostCalleesUserCalls() const { return m_PostCalleesUserCalls; }
    const TUserCallVec &GetPreCalleesUserCalls() const { return m_PreCalleesUserCalls; }
    
    const string GetStoredArgVariable(void) const { return ( m_DoStoreArg ? "m_LastArg_" + m_FuncName : kEmptyStr); };

    bool GetDoStoreArg(void) const { return m_DoStoreArg; }
    void SetDoStoreArg(void) { m_DoStoreArg = true; }

    void AddPreCalleesUserCall( CRef<CUserCall> user_call );
    void AddPostCalleesUserCall( CRef<CUserCall> user_call );

    void RemoveXFromFuncName(void);

    // returns false if couldn't merge
    enum EMergeNameAllowed {
        // Can *this have its name changed when the other node is
        // merged into it?
        eMergeNameAllowed_AllowNameChange = 1,
        eMergeNameAllowed_ForbidNameChange
    };
    bool Merge( CRef<CTraversalNode> node_to_merge_into_this, 
        EMergeNameAllowed merge_name_allowed = eMergeNameAllowed_AllowNameChange );

    // erases all connections to and from this node and its calls.
    // It's possible that it will be garbage-collected soon after this point.
    void Clear(void);


    typedef set<CTraversalNode*> TNodeRawSet;
    static const TNodeRawSet &GetEveryNode(void) { return *ms_EveryNode; };

private:

    // Make sure the CTraversalNode depends on asn_node ONLY in the constructor
    // to make it easier if we need to transition the code to another parsing system.
    CTraversalNode( CRef<CTraversalNode> parent, const string &var_name, CDataType *asn_node );
    ~CTraversalNode(void);

    // Each CTraversalNode should be unique, so assignment doesn't make sense
    // ( no-arg constructor is implemented and used
    //   for internal purposes )
    CTraversalNode(void);
    CTraversalNode( const CTraversalNode &other );
    CTraversalNode& operator = (const CTraversalNode& other);

    void x_LoadDataFromASNNode( CDataType *asn_node );

    struct CCRefUserCallLessThan {
        bool operator()( const CConstRef<CUserCall> & call1, const CConstRef<CUserCall> & call2 ) const {
            return *call1 < *call2;
        }
    };
    typedef set< CConstRef<CUserCall>, CCRefUserCallLessThan > TUserCallSet;

    // helper for DepthFirst which hides some implementation details
    void x_DepthFirst( CDepthFirstCallback &callback, TNodeVec &node_path, TNodeSet &nodesSeen, 
        TTraversalOpts traversal_opts );

    // generates a call to a child node, taking care of a couple details
    // like not passing an arg for null children.
    void x_GenerateChildCall( CNcbiOstream& traversal_output_file, CRef<CTraversalNode> child, const string &arg );

    // Turn type-name into a potential template typename
    void x_TemplatizeType( std::string &type_name );

    // When we merge nodes, this function merges the names of the two functions
    static void x_MergeNames( string &result, const string &name1, const string &name2 );

    CRef<CTraversalNode> x_CloneWithoutCallers(const string &var_name) const;

    // e.g. C:\foo\bar\src\objects\seqloc\seqlocl.asn" returns "seqloc"
    string x_ExtractIncludePathFromFileName( const string &asn_file_name );

    // due to cycles, we can have many callers
    TNodeCallSet m_Callees;
    TNodeCallSet m_Callers;

    TUserCallVec m_PreCalleesUserCalls;
    TUserCallVec m_PostCalleesUserCalls;
    // The user calls that need the value of this node.
    TUserCallVec m_ReferencingUserCalls;

    EType m_Type;

    std::string m_TypeName;        // e.g. "Seq-loc"
    std::string m_InputClassName;  // e.g. "CSeq_loc"
    std::string m_IncludePath;      // e.g. "seqloc"

    std::string m_FuncName;   // name of the C++ function that will be produced

    bool m_IsTemplate;        // is classname really a templated name?

    // true if this function should store its argument
    // every time it's called. (Stored value used by nodes lower down the chain)
    bool m_DoStoreArg;

    // Hold the first CTraversalNode created from a given asn_node.
    // We can often save data calculating time by loading the data
    // that the other node already calculated.
    typedef std::map<CDataType *, CRef<CTraversalNode> > TASNNodeToNodeMap;
    static TASNNodeToNodeMap m_ASNNodeToNodeMap;

    // holds all CTraversalNodes that have been created but not destroyed
    // (doesn't use CRef because then destructor could never be called)
    // This is a pointer because there is actually a potential undefined behavior
    // issue: If ms_EveryNode is destroyed before other static structures such as
    // m_ASNNodeToNodeMap are destroyed, it's possible for CRef<CTraversalNodes>
    // to become destroyed, which destorys a CTraversalNode.  However,
    // CTraversalNodes remove themselves from ms_EveryNode when they're
    // destroyed, which could cause memory corruption issues.
    static set<CTraversalNode*> *ms_EveryNode;

    // When we need to create a new function name, we append with the
    // next number, just to force uniqueness.
    static int ms_FuncUniquerInt;
};

END_NCBI_SCOPE

#endif /* TRAVERSALNODE__HPP */
