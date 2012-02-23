#ifndef TRAVERSALCODEGENERATOR__HPP
#define TRAVERSALCODEGENERATOR__HPP

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
*   Creates the generated code.
*/

#include "moduleset.hpp"
#include "traversal_node.hpp"
#include "traversal_spec_file_parser.hpp"
#include <corelib/ncbistre.hpp>

BEGIN_NCBI_SCOPE

class CTraversalCodeGenerator {
public:
    // all work done in constructor
    CTraversalCodeGenerator( CFileSet& mainModules, 
        CNcbiIstream& traversal_spec_file );

private:
    typedef map<CDataType*, CRef<CTraversalNode> > TASNToTravMap;
    // faster than calling CFileSet::ResolveInAnyModule
    typedef map<std::string, CDataType*> TNameToASNMap;

    // fills in nameToASNMap from mainModules
    void x_BuildNameToASNMap( CFileSet& mainModules, TNameToASNMap &nameToASNMap );

    // recurse to create the node
    CRef<CTraversalNode> x_CreateNode( 
        const TNameToASNMap &nameToASNMap,
        TASNToTravMap &asn_nodes_seen,
        const std::string &var_name,
        CDataType *asn_node, 
        CRef<CTraversalNode> parent );

    // traverses the code and removing all code paths that do nothing
    void x_PruneEmptyNodes( 
        vector< CRef<CTraversalNode> > &rootTraversalNodes, 
        CTraversalNode::TNodeSet &nodesWithFunctions );

    // write out the header file
    void x_GenerateHeaderFile( 
        const std::vector<std::string> & output_class_namespace,
        const std::string &output_class_name,
        const std::string &headerFileName,
        CNcbiOstream& traversal_header_file, 
        vector< CRef<CTraversalNode> > &rootTraversalNodes,
        const CTraversalSpecFileParser::TMemberRefVec & members,
        const std::vector<std::string> &header_includes,
        const std::vector<std::string> &header_forward_declarations );

    // gives the include guard to use given the header file name.
    void x_GetIncludeGuard( std::string& include_guard_define, const std::string& headerFileName );

    // write out the source file
    void x_GenerateSourceFile(
        const std::vector<std::string> & output_class_namespace,
        const std::string &output_class_name,
        const std::string &headerFileName,
        CNcbiOstream& traversal_source_file,
        std::vector< CRef<CTraversalNode> > &rootTraversalNodes,
        const std::vector<std::string> &source_includes );

    // given a file name, returns just the file name with no path
    // e.g. C:\foo\bar\something.txt becomes just something.txt
    std::string x_StripPath( const std::string &file_name );

    // e.g. "m_SomeMemberVar" becomes "someMemberVar"
    std::string x_MemberVarNameToArg(const std::string &member_var_name );

    void x_SplitNodesByVar(void);
};

END_NCBI_SCOPE

#endif /* TRAVERSALCODEGENERATOR__HPP */
