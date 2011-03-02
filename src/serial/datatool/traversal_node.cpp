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

#include <ncbi_pch.hpp>

#include "aliasstr.hpp"
#include "blocktype.hpp"
#include "choicetype.hpp"
#include "enumtype.hpp"
#include "module.hpp"
#include "namespace.hpp"
#include "reftype.hpp"
#include "statictype.hpp"
#include "stdstr.hpp"
#include "traversal_node.hpp"
#include "typestr.hpp"
#include "unitype.hpp"

#include <serial/typeinfo.hpp>
#include <util/static_map.hpp>
#include <iterator>

BEGIN_NCBI_SCOPE

CTraversalNode::TASNNodeToNodeMap CTraversalNode::m_ASNNodeToNodeMap;
// Okay to dynamically allocate and never free, because it's needed for the entire
// duration of the program.
set<CTraversalNode*> *CTraversalNode::ms_EveryNode = new set<CTraversalNode*>;

CRef<CTraversalNode> CTraversalNode::Create( CRef<CTraversalNode> caller, const string &var_name, CDataType *asn_node )
{
    // in the future, maybe a real factory pattern should be done here
    return CRef<CTraversalNode>( new CTraversalNode( caller, var_name, asn_node ) );
}

CTraversalNode::CTraversalNode( CRef<CTraversalNode> caller, const string &var_name, CDataType *asn_node )
: m_Type(eType_Primitive), m_VarName(var_name), m_IsTemplate(false), m_DoStoreArg(false)
{
    if( m_VarName.empty() ) {
        throw runtime_error("No var_name given for CTraversalNode");
    }

    // If another node was previously constructed from the same asn_node, 
    // grab as much as we can from it to avoid as much work as possible.
    TASNNodeToNodeMap::iterator similar_node_iter = m_ASNNodeToNodeMap.find( asn_node );
    if( similar_node_iter == m_ASNNodeToNodeMap.end() ) {
        x_LoadDataFromASNNode( asn_node );
        m_ASNNodeToNodeMap.insert( TASNNodeToNodeMap::value_type(asn_node, Ref() ) );
    } else {
        CTraversalNode &other_node = *(similar_node_iter->second);
        m_TypeName = other_node.m_TypeName;
        m_InputClassName = other_node.m_InputClassName;
        m_IncludePath = other_node.m_IncludePath;
        m_Type = other_node.m_Type;
        m_IsTemplate = other_node.m_IsTemplate;
    }

    // function name is our caller's function name plus our variable
    m_FuncName = ( caller ? ( (caller->m_FuncName) + "_" ) : kEmptyStr ) + NStr::Replace(m_VarName, "-", "_" );

    // only attach to caller once we've successfully created the object
    if( caller ) {
        AddCaller( caller );
    }

    // add it to the list of all constructed nodes
    ms_EveryNode->insert( this );
}

CTraversalNode::~CTraversalNode(void)
{
    // remove it from the list of all constructed nodes
    ms_EveryNode->erase( this );
}

void CTraversalNode::x_LoadDataFromASNNode( CDataType *asn_node )
{
    m_TypeName = asn_node->GetFullName();

    // figure out m_InputClassName
    AutoPtr<CTypeStrings> c_type = asn_node->GetFullCType();
    const CNamespace& ns = c_type->GetNamespace();
    m_InputClassName = c_type->GetCType(ns);
    // handle inner classes
    if( (asn_node->IsEnumType() && ! dynamic_cast<CIntEnumDataType*>(asn_node) ) ||
        NStr::StartsWith( m_InputClassName, "C_" ) ) 
    {
        const CDataType *parent_asn_node = asn_node->GetParentType();
        while( parent_asn_node ) {
            AutoPtr<CTypeStrings> parent_type_strings = parent_asn_node->GetFullCType();
            const CNamespace& parent_ns = parent_type_strings->GetNamespace();
            string parent_class_name = parent_type_strings->GetCType(parent_ns);
            if( NStr::EndsWith( parent_class_name, m_InputClassName ) ) {
                break;
            }

            m_InputClassName = parent_class_name + "::" + m_InputClassName;

            parent_asn_node = parent_asn_node->GetParentType();
        }
    }

    if( ! asn_node->IsReference() ) {
        m_IncludePath = x_ExtractIncludePathFromFileName( asn_node->GetSourceFileName() );
    }

    if( asn_node->IsReference() ) {
        CReferenceDataType* ref = dynamic_cast<CReferenceDataType*>(asn_node);
        // get the module of the symbol we're referencing, not our module
        m_IncludePath = x_ExtractIncludePathFromFileName( ref->Resolve()->GetSourceFileName() );
        m_Type = eType_Reference;
    } else if( asn_node->IsContainer() ) {
        if( dynamic_cast<CChoiceDataType*>(asn_node) != 0 ) {
            m_Type = eType_Choice;
        } else {
            m_Type = eType_Sequence;
        }
    } else if( dynamic_cast<CUniSequenceDataType*>(asn_node) != 0 ) {
        m_Type = eType_UniSequence;
    } else if( dynamic_cast<CEnumDataType*>(asn_node) != 0 ) {
        m_Type = eType_Enum;
    } else if( dynamic_cast<CNullDataType *>(asn_node) != 0 ) {
        m_Type = eType_Null;
    } else if( asn_node->IsStdType() ) {
        // nothing to do 
    } else {
        throw runtime_error("possible bug in code generator: unknown type for '" + m_TypeName + "'");
    }

    // since lists and vectors seem to be used interchangeably in 
    // the datatool object code with no discernible pattern, we can't
    // know at this time what the type will be, so we just use
    // a template and let the compiler figure it out.
    if( NStr::Find( m_InputClassName, "std::list" ) != NPOS ) {
        m_IsTemplate = true;
        x_TemplatizeType( m_InputClassName );
    }
}

void CTraversalNode::AddCaller( CRef<CTraversalNode> caller )
{
    m_Callers.insert( caller );
    caller->m_Callees.insert( Ref() );
}

void CTraversalNode::GenerateCode( const string &func_class_name, CNcbiOstream& traversal_output_file, EGenerateMode generate_mode )
{
    // print start of function
    if( m_IsTemplate ) {
        traversal_output_file << "template< typename " << m_InputClassName << " >" << endl;
    }
    traversal_output_file << "void ";
    if( (! func_class_name.empty()) && (generate_mode == eGenerateMode_Definitions) ) {
        traversal_output_file << func_class_name << "::" ;
    }
    switch( m_Type ) {
    case eType_Null:
        traversal_output_file << m_FuncName << "( void )";
        break;
    default:
        traversal_output_file << m_FuncName << "( " << m_InputClassName << " & arg0 )";
        break;
    }

    // if we're just generating the prototypes, we end early
    if( generate_mode == eGenerateMode_Prototypes ) {
        traversal_output_file << ";" << endl;
        return;
    }

    traversal_output_file << endl;
    traversal_output_file << "{ // type " << GetTypeAsString() << endl;

    // store our arg if we're one of the functions that are supposed to
    if( m_DoStoreArg ) {
        traversal_output_file << "  " << GetStoredArgVariable() << " = &arg0;" << endl;
        traversal_output_file << endl;
    }

    // generate calls to pre-callees user functions
    ITERATE( TUserCallVec, func_iter, m_PreCalleesUserCalls ) {
        traversal_output_file << "  " << (*func_iter)->GetUserFuncName() << "( arg0";
        ITERATE( CTraversalNode::TNodeVec, extra_arg_iter, (*func_iter)->GetExtraArgNodes() ) {
            _ASSERT( (*extra_arg_iter)->GetDoStoreArg() );
            traversal_output_file << ", *" << (*extra_arg_iter)->GetStoredArgVariable();
        }
        traversal_output_file << " );" << endl;
    }

    // generate calls to the contained types
    if( ! m_Callees.empty() ) {
        switch( m_Type ) {
        case eType_Null:
        case eType_Enum:
        case eType_Primitive:
            _ASSERT( m_Callees.empty() );
            break;
        case eType_Choice:
            {
                traversal_output_file << "  switch( arg0.Which() ) {" << endl;
                ITERATE( TNodeSet, child_iter, m_Callees ) {
                    string case_name = (*child_iter)->GetVarName();
                    case_name[0] = toupper(case_name[0]);
                    NStr::ReplaceInPlace( case_name, "-", "_" );
                    traversal_output_file << "  case " << m_InputClassName << "::e_" << case_name << ":" << endl;;
                    string argString = string("arg0.Set") + case_name + "()";
                    x_GenerateChildCall( traversal_output_file, (*child_iter), argString );
                    traversal_output_file << "    break;" << endl;
                }
                traversal_output_file << "  default:" << endl;
                traversal_output_file << "    break;" << endl;
                traversal_output_file << "  }" << endl;
            }
            break;
        case eType_Sequence:
            {
                ITERATE( TNodeSet, child_iter, m_Callees ) {
                    string case_name = (*child_iter)->GetVarName();
                    case_name[0] = toupper(case_name[0]);
                    NStr::ReplaceInPlace( case_name, "-", "_" );
                    traversal_output_file << "  if( arg0.IsSet" << case_name << "() ) {" << endl;;
                    string argString = string("arg0.Set") + case_name + "()";
                    x_GenerateChildCall( traversal_output_file, (*child_iter), argString );
                    traversal_output_file << "  }" << endl;
                }
            }
            break;
        case eType_Reference:
            {
                _ASSERT( m_Callees.size() == 1 );
                CRef<CTraversalNode> child = *m_Callees.begin();
                string case_name = child->GetVarName();
                case_name[0] = toupper(case_name[0]);
                NStr::ReplaceInPlace( case_name, "-", "_" );

                // some reference functions pass their argument directly and others
                // have to call .Set() to get to it
                bool needs_set = ( (child->m_Type == eType_Primitive) ||
                    (child->m_Type == eType_UniSequence) );

                if( needs_set ) {
                    traversal_output_file << "  if( arg0.IsSet() ) {" << endl;
                }

                string argString = ( needs_set ? "arg0.Set()" : "arg0" );
                x_GenerateChildCall( traversal_output_file, child, argString );

                if( needs_set ) {
                    traversal_output_file << "  }" << endl;
                }
            }
            break;
        case eType_UniSequence:
            {
                _ASSERT( m_Callees.size() == 1 );
                CRef<CTraversalNode> child = *m_Callees.begin();
                string case_name = child->GetVarName();
                case_name[0] = toupper(case_name[0]);
                NStr::ReplaceInPlace( case_name, "-", "_" );
                const char *input_class_prefix = ( m_IsTemplate ? "typename " : kEmptyCStr );
                traversal_output_file << "  NON_CONST_ITERATE( " << input_class_prefix << m_InputClassName << ", iter, arg0 ) { " << endl;

                int levelsOfDereference = 1;
                if( NStr::FindNoCase(m_InputClassName, "CRef") != NPOS ) {
                    ++levelsOfDereference;
                }
                if( NStr::FindNoCase(m_InputClassName, "vector") != NPOS ) {
                    ++levelsOfDereference;
                }
                string argString = string(levelsOfDereference, '*') + "iter";
                x_GenerateChildCall( traversal_output_file, child, argString );
                traversal_output_file << "  }" << endl;
            }
            break;
        default:
            throw runtime_error("Unknown node type. Probably bug in code generator.");
        }
    }

    // generate calls to post-callees user functions
    ITERATE( TUserCallVec, a_func_iter, m_PostCalleesUserCalls ) {
        traversal_output_file << "  " << (*a_func_iter)->GetUserFuncName() << "( arg0";
        ITERATE( CTraversalNode::TNodeVec, extra_arg_iter, (*a_func_iter)->GetExtraArgNodes() ) {
            _ASSERT( (*extra_arg_iter)->GetDoStoreArg() );
            traversal_output_file << ", *" << (*extra_arg_iter)->GetStoredArgVariable();
        }
        traversal_output_file << " );" << endl;
    }

    // reset stored arg since it's now invalid
    if( m_DoStoreArg ) {
        traversal_output_file << endl;
        traversal_output_file << "  " << GetStoredArgVariable() << " = NULL;" << endl;
    }

    // end of function
    traversal_output_file << "} // end of " << m_FuncName << endl;
    traversal_output_file << endl;
}

CTraversalNode::CUserCall::CUserCall( const std::string &user_func_name,
    std::vector< CRef<CTraversalNode> > &extra_arg_nodes ) 
    : m_UserFuncName( user_func_name ), m_ExtraArgNodes(extra_arg_nodes) 
{
    NON_CONST_ITERATE( std::vector< CRef<CTraversalNode> >, extra_arg_iter, extra_arg_nodes ) {
        (*extra_arg_iter)->m_ReferencingUserCalls.push_back( CRef<CUserCall>(this) );
    }
}

void CTraversalNode::DepthFirst( CDepthFirstCallback &callback, TTraversalOpts traversal_opts )
{
    TNodeVec node_path;
    TNodeSet nodesSeen;
    x_DepthFirst( callback, node_path, nodesSeen, traversal_opts );
}

const string &CTraversalNode::GetTypeAsString(void) const
{
    static const string kSequence = "Sequence";
    static const string kChoice = "Choice";
    static const string kPrimitive = "Primitive";
    static const string kNull = "Null";
    static const string kEnum = "Enum";
    static const string kReference = "Reference";
    static const string kUniSequence = "UniSequence";

    static const string kUnknown = "???";

    switch( m_Type ) {
    case CTraversalNode::eType_Sequence:
        return kSequence;
    case CTraversalNode::eType_Choice:
        return kChoice;
    case CTraversalNode::eType_Primitive:
        return kPrimitive;
    case CTraversalNode::eType_Null:
        return kNull;
    case CTraversalNode::eType_Enum:
        return kEnum;
    case CTraversalNode::eType_Reference:
        return kReference;
    case CTraversalNode::eType_UniSequence:
        return kUniSequence;
    default:
        // shouldn't happen
        return kUnknown;
    }
}

void CTraversalNode::AddPreCalleesUserCall( CRef<CUserCall> user_call )
{
    m_PreCalleesUserCalls.push_back( user_call );
}

void CTraversalNode::AddPostCalleesUserCall( CRef<CUserCall> user_call )
{
    m_PostCalleesUserCalls.push_back( user_call );
}

void CTraversalNode::RemoveCallee( CRef<CTraversalNode> callee )
{
    m_Callees.erase( callee );
    callee->m_Callers.erase( Ref() );
}

void CTraversalNode::RemoveXFromFuncName(void)
{
    if( NStr::StartsWith(m_FuncName, "x_") ) {
        m_FuncName = m_FuncName.substr(2);
    }
}

bool CTraversalNode::Merge( CRef<CTraversalNode> node_to_merge_into_this )
{
    // a node can't merge into itself
    if( this == node_to_merge_into_this.GetPointerOrNull() ) {
        return false;
    }

    // if either had to store the argument, we also have to store our argument
    m_DoStoreArg = ( m_DoStoreArg || node_to_merge_into_this->m_DoStoreArg );

    // add others' functions to us (as long as we don't already have them )
    TUserCallSet funcsWeHave;
    copy( m_PreCalleesUserCalls.begin(), m_PreCalleesUserCalls.end(),
          inserter(funcsWeHave, funcsWeHave.end() ) );
    copy( m_PostCalleesUserCalls.begin(), m_PostCalleesUserCalls.end(),
          inserter(funcsWeHave, funcsWeHave.end() ) );
    ITERATE( TUserCallVec, pre_call_iter, node_to_merge_into_this->m_PreCalleesUserCalls ) {
        if( funcsWeHave.find(*pre_call_iter) == funcsWeHave.end() ) {
            m_PreCalleesUserCalls.push_back( *pre_call_iter );
        }
    }
    ITERATE( TUserCallVec, post_call_iter, node_to_merge_into_this->m_PostCalleesUserCalls ) {
        if( funcsWeHave.find(*post_call_iter) == funcsWeHave.end() ) {
            m_PostCalleesUserCalls.push_back( *post_call_iter );
        }
    }

    // find any user calls that depended on the other node's value and reassign them to us
    m_ReferencingUserCalls.insert( m_ReferencingUserCalls.end(),
        node_to_merge_into_this->m_ReferencingUserCalls.begin(),
        node_to_merge_into_this->m_ReferencingUserCalls.end() );
    NON_CONST_ITERATE( TUserCallVec, call_iter, node_to_merge_into_this->m_ReferencingUserCalls ) {
        NON_CONST_ITERATE( TNodeVec, node_iter, (*call_iter)->m_ExtraArgNodes ) {
            if( *node_iter == node_to_merge_into_this->Ref() ) {
                *node_iter = Ref();
            }
        }
    }

    // add their callers as our callers and their
    // callees as our callees
    NON_CONST_SET_ITERATE( TNodeSet, their_caller, node_to_merge_into_this->m_Callers ) {
        AddCaller( *their_caller );
    }
    NON_CONST_SET_ITERATE( TNodeSet, their_callee, node_to_merge_into_this->m_Callees ) {
        (*their_callee)->AddCaller( Ref() );
    }

    // change our name.
    x_MergeNames( m_FuncName, m_FuncName, node_to_merge_into_this->m_FuncName );

    // wipe out all data in the other node so it becomes a hollow shell and nothing
    // calls it and it calls nothing. (Possibly garbage collected after this)
    node_to_merge_into_this->Clear();

    return true;
}

void CTraversalNode::Clear(void)
{
    // don't let anyone call us
    CRef<CTraversalNode> my_ref = Ref();
    NON_CONST_SET_ITERATE( TNodeSet, caller_iter, m_Callers ) {
        (*caller_iter)->m_Callees.erase( my_ref );
    }
    m_Callers.clear();

    // don't call anyone
    NON_CONST_SET_ITERATE( TNodeSet, callee_iter, m_Callees ) {
        (*callee_iter)->m_Callers.erase( my_ref );
    }
    m_Callees.clear();

    // no user calls
    m_PreCalleesUserCalls.clear();
    m_PostCalleesUserCalls.clear();

    // No one should depend on our value now
    m_ReferencingUserCalls.clear();
}

void CTraversalNode::x_DepthFirst( CDepthFirstCallback &callback, TNodeVec &node_path, TNodeSet &nodesSeen, 
    TTraversalOpts traversal_opts )
{
    node_path.push_back( Ref() );

    const bool post_traversal =
      ( ( traversal_opts & fTraversalOpts_Post ) != 0);
    const bool allow_cycles =
      ( ( traversal_opts & fTraversalOpts_AllowCycles ) != 0 );

    const bool seen_before = ( nodesSeen.find(Ref()) != nodesSeen.end() );
    // must avoid cyclic calls on post-traversal or we will get infinite loops
    // also if the user explicitly forbids it
    if( seen_before && (post_traversal || ! allow_cycles)  ) {
        node_path.pop_back();
        return;
    }

    const CDepthFirstCallback::ECallType is_cyclic = 
        ( seen_before ? CDepthFirstCallback::eCallType_Cyclic : CDepthFirstCallback::eCallType_NonCyclic );

    // call callback before for pre-traversal
    if( ! post_traversal ) {
        if( ! callback.Call( *this, node_path, is_cyclic ) ) {
            node_path.pop_back();
            return;
        }
    }

    const bool up_callers =
      ( ( traversal_opts & fTraversalOpts_UpCallers ) != 0 );
    TNodeSet & set_to_traverse = ( up_callers ? m_Callers : m_Callees );

    // traverse
    nodesSeen.insert( Ref() );    
    NON_CONST_SET_ITERATE( TNodeSet, child_iter, set_to_traverse ) {
        (*child_iter)->x_DepthFirst( callback, node_path, nodesSeen, traversal_opts );
    }
    nodesSeen.erase( Ref() );

    // call callback after for post-traversal
    if( post_traversal ) {
        // ignore return value since we're going to return anyway
        callback.Call( *this, node_path, is_cyclic );
    }

    node_path.pop_back();
}

void CTraversalNode::x_GenerateChildCall( CNcbiOstream& traversal_output_file, CRef<CTraversalNode> child, const string &arg )
{
    if( child->m_Type == eType_Null ) {
        // NULL functions take no arguments
        traversal_output_file << "    " << child->m_FuncName << "();" << endl;
    } else if( child->m_InputClassName == "int" ) {
        // necessary hack, unfortunately
        traversal_output_file << "    _ASSERT( sizeof(int) == sizeof(" << arg << ") );" << endl;
        traversal_output_file << "    // the casting, etc. is a hack to get around the fact that we sometimes use TSeqPos " << endl;
        traversal_output_file << "    // instead of int, but we can't tell where by looking at the .asn file." << endl;
        traversal_output_file << "    " << child->m_FuncName << "( *(int*)&" << arg << " );" << endl;
    } else {
        traversal_output_file << "    " << child->m_FuncName << "( " << arg << " );" << endl;
    }
}

struct CIsAlnum {
    bool operator()( const char &ch ) { return isalnum(ch) != 0; }
};
struct CIsNotAlnum {
    bool operator()( const char &ch ) { return isalnum(ch) == 0; }
};

void CTraversalNode::x_TemplatizeType( string &type_name )
{
    NStr::ReplaceInPlace( type_name, "std::list", "container" );
    string result = "T";

    // Replace clumps of more than one non-alphanumeric character by one
    // underscore
    string::iterator pos = type_name.begin();
    while( pos != type_name.end() ) {
        string::iterator next_bad_char = find_if( pos, type_name.end(), CIsNotAlnum() );
        // add the stuff before the next bad char straight to the result
        result.insert( result.end(), pos, next_bad_char );
        if( next_bad_char != type_name.end() ) {
            result += '_';
        }
        // find the next good character after the bad one
        pos = find_if( next_bad_char, type_name.end(), CIsAlnum() );
    }

    NStr::ToLower( result );
    result[0] = toupper(result[0]);

    type_name.swap( result );
}

void 
CTraversalNode::x_MergeNames( string &result, const string &name1, const string &name2 )
{
    // the other names are ignored, but maybe in the future, we'll use them
    if( ! NStr::EndsWith(result, "_ETC" ) ) {
        result += "_ETC";
    }
}

string CTraversalNode::x_ExtractIncludePathFromFileName( const string &asn_file_name )
{
    static const string kObjectsStr = "objects";

    string::size_type objects_pos = asn_file_name.find( kObjectsStr );
    string::size_type slash_after_objects = objects_pos + kObjectsStr.length();
    string::size_type last_backslash_pos = asn_file_name.find_last_of("\\/");
    if( (objects_pos == string::npos) || 
        (slash_after_objects >= asn_file_name.length() ) ||
        ( asn_file_name[slash_after_objects] != '\\' && asn_file_name[slash_after_objects] != '/') ||
        ( last_backslash_pos == string::npos ) ||
        ( last_backslash_pos <= slash_after_objects ) ) 
    {
        string msg;
        msg += "All ASN file names must contain 'objects' in their path so ";
        msg += "we can extract the location of .hpp files in the ";
        msg += "include/objects directory.  Example: C:/foo/bar/objects/seqloc/seqloc.asn. ";
        msg += "You gave this file name: '" + asn_file_name + "'";
        throw runtime_error(msg);
    }

    // give the part after objects but before the last [back]slash
    string result( 
        // The "+1" is to skip any backslash which might be after kObjectsStr
        asn_file_name.begin() + slash_after_objects + 1,
        asn_file_name.begin() + last_backslash_pos );

    // turn backslashes into forward slashes, though
    NStr::ReplaceInPlace( result, "\\", "/" );
    
    return result;
}

END_NCBI_SCOPE
