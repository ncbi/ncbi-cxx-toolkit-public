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
 * Authors: Mike Dicuccio, Andrea Asztalos
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <util/qparse/parse_utils.hpp>


BEGIN_NCBI_SCOPE

void Flatten_ParseTree(CQueryParseTree::TNode& node)
{
    CQueryParseNode::EType type = node->GetType();
    switch (type) {
    case CQueryParseNode::eAnd:
    case CQueryParseNode::eOr:
    {{
        CQueryParseTree::TNode::TNodeList_I iter;
        for (iter = node.SubNodeBegin(); iter != node.SubNodeEnd(); ) {
            CQueryParseTree::TNode& sub_node = **iter;
            if (sub_node->GetType() == type) {
                /// hoist this node's children
                CQueryParseTree::TNode::TNodeList_I sub_iter =
                    sub_node.SubNodeBegin();
                
                for (; sub_iter != sub_node.SubNodeEnd(); ) {
                    node.AddNode(sub_node.DetachNode(*sub_iter++));
                }

                node.RemoveNode(iter++);
            }
            else {
                ++iter;
            }
        }
    }}
    break;

    default:
        break;
    }

    CQueryParseTree::TNode::TNodeList_I iter;
    for (iter = node.SubNodeBegin();
        iter != node.SubNodeEnd();  ++iter) {
        Flatten_ParseTree(**iter);
    }
}


END_NCBI_SCOPE

