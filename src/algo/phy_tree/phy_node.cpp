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
 * Authors:  Josh Cherry
 *
 * File Description:  Things for representing and manipulating
 *          phylogenetic trees
 *
 */


#include <ncbi_pch.hpp>
#include <algo/phy_tree/phy_node.hpp>


BEGIN_NCBI_SCOPE

static string s_EncodeLabel(const string& label);

// recursive function
void PrintNode(CNcbiOstream& os, const TPhyTreeNode& node)
{
    if (!node.IsLeaf()) {
        os << '(';
        for (TPhyTreeNode::TNodeList_CI it = node.SubNodeBegin();
             it != node.SubNodeEnd();  ++it) {
            if (it != node.SubNodeBegin()) {
                os << ", ";
            }
            PrintNode(os, **it);
        }
        os << ')';
    }

    if (node.IsLeaf() || !node.GetValue().GetLabel().empty()) {
        os << s_EncodeLabel(node.GetValue().GetLabel());
    }

    if (node.GetValue().IsSetDist()) {
        os << ':' << node.GetValue().GetDist();
    }
}


CNcbiOstream& operator<<(CNcbiOstream& os, const TPhyTreeNode& tree)
{
    PrintNode(os, tree);
    os << ';' << endl;
    return os;
};


void WriteNexusTree(CNcbiOstream& os, const TPhyTreeNode& tree,
                    const string& tree_name)
{
    os << "#nexus\n\nbegin trees;\ntree " << tree_name << " = "
       << tree << "\nend;" << endl;
};


// Encode a label for Newick format:
// If necessary, enclose it in single quotes,
// but first escape any single quotes by doubling them.
// e.g., "This 'label'" -> "'This ''label'''"
static string s_EncodeLabel(const string& label) {
    if (label.find_first_of("()[]':;,_") == string::npos) {
        // No need to quote, but any spaces must be changed to underscores
        string unquoted = label;
        for (size_t i = 0; i < label.size(); ++i) {
            if (unquoted[i] == ' ') {
                unquoted[i] = '_';
            }
        }
        return unquoted;
    }
    if (label.find_first_of("'") == string::npos) {
        return '\'' + label + '\'';
    }
    string rv;
    rv.reserve(label.size() + 2);
    rv.append(1, '\'');
    for (unsigned int i = 0;  i < label.size();  ++i) {
        rv.append(1, label[i]);
        if (label[i] == '\'') {
            // "'" -> "''"
            rv.append(1, label[i]);
        }
    }
    rv.append(1, '\'');

    return rv;
}


END_NCBI_SCOPE
