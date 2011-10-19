#ifndef TRAVERSALPATTERN__HPP
#define TRAVERSALPATTERN__HPP

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
*   Used to attach user functions to be called by the traversal.
*/

#include <vector>

#include <corelib/ncbiobj.hpp>

#include "traversal_spec_file_parser.hpp"
#include "traversal_node.hpp"

BEGIN_NCBI_SCOPE

class CTraversalPatternMatchCallback {
public:
    typedef map<std::string, CTraversalNode::TNodeVec> TLeafToNodeMap;

    // This will attach the given user functions to any 
    // nodes they belong to, and add any nodes with functions
    // to out_nodesWithFunctions
    CTraversalPatternMatchCallback( 
        CTraversalSpecFileParser &spec_file_parser, 
        CTraversalNode::TNodeSet &out_nodesWithFunctions );

private:

    // attach the given pattern to all relevant nodes
    void x_TryToAttachPattern( CTraversalSpecFileParser::CDescFileNodeRef pattern );

    // mark as deprecated any nodes we can
    void x_TryToDeprecatePatternMatchers( 
        const CTraversalSpecFileParser::TPattern & deprec_pattern, 
        CTraversalNode::TNodeSet & nodes_to_destroy );

    typedef std::vector<std::string>::const_reverse_iterator TPatternIter;
    // Check if the given node matches the given pattern
    bool x_PatternMatches( CRef<CTraversalNode> node, TPatternIter pattern_start, TPatternIter pattern_end );

    // Check if at least one of any of the given patterns matches the given node
    bool x_AnyPatternMatches( CRef<CTraversalNode> node, const CTraversalSpecFileParser::TPatternVec &patterns );

    // This actually does the pattern attachment.  It can throw an exception if that's impossible, such
    // as due to specifying an ancestor pattern that doesn't work (shouldn't usually happen, since
    // such problems are usually caught earlier.
    void x_DoAttachment( CRef<CTraversalNode> node, CTraversalSpecFileParser::CDescFileNodeRef pattern );
    CRef<CTraversalNode> x_TranslateArgToNode( CRef<CTraversalNode> node, 
        const CTraversalSpecFileParser::TPattern &main_pattern,
        const CTraversalSpecFileParser::TPattern &extra_arg_pattern );

    // returns true if the node can't get a pattern attached to it
    bool x_NodeIsUnmatchable( const CTraversalNode &node );

    // This assumes that the node has the same variable name regardless of
    // which caller is used.
    const string &x_GetNodeVarName( const CTraversalNode &node );

    enum ERefChoice {
        eRefChoice_RefOnly = 1,
        eRefChoice_ChildOnly,
        eRefChoice_Both
    };

    ERefChoice x_UseRefOrChild(
        const CTraversalNode& parent_ref,
        const CTraversalNode& child );

    // maps the given type or label or class to all the nodes that contain it as the last part
    // e.g. "location" maps to any nodes that ar named "location" in their last part
    TLeafToNodeMap m_LeafToPossibleNodes;
    CTraversalNode::TNodeSet &m_NodesWithFunctions;
};

END_NCBI_SCOPE

#endif /* TRAVERSALPATTERN__HPP */
