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
 *  Government have not placed any restriction on its use or requeryion.
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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <misc/xmlreaders/rfam_tool.hpp>
#include <corelib/ncbistd.hpp>
#include <misc/xmlwrapp/stylesheet.hpp>

BEGIN_NCBI_SCOPE
// BEGIN_SCOPE(objects)

////////////////////////////
//
// CRfamTool
//
///////////////////////////
CRfamTool::CRfamTool(const string& xmlfile)
{
    xml::document doc(xmlfile.c_str(), NULL );
    m_doc.swap(doc);
}

bool 
CRfamTool::GetModelByAccession(const string& accession, CRfamModel& model)
{

    string xpath = "//Entry/Accession[.='"+accession+"']/..";
    xml::node_set attrNodes = m_doc.get_root_node().run_xpath_query(xpath.c_str());
    xml::node_set::iterator it = attrNodes.begin();
    if(attrNodes.size() != 1) {
        LOG_POST(Error << "doc xpath=" << xpath << " did not return required number (1) of nodes, it returned " << attrNodes.size() << " nodes");
        return false;
    } else {
        return sx_GetModel(*it, model);
    }
    return true;
    
    
}

bool 
CRfamTool::GetModelByIdentification(const string& model_identification, CRfamModel& model)
{

    string xpath = "//Entry[@identification='"+model_identification+"']";
    xml::node_set attrNodes = m_doc.get_root_node().run_xpath_query(xpath.c_str());
    xml::node_set::iterator it = attrNodes.begin();
    if(attrNodes.size() != 1) {
        LOG_POST(Error << "doc xpath=" << xpath << " did not return required number (1) of nodes, it returned " << attrNodes.size() << " nodes");
        return false;
    } else {
        return sx_GetModel(*it, model);
    }
    return true;
    
    
}


bool
CRfamTool::sx_GetModel(xml::node& node, CRfamModel& model)
{
    bool result=false;
    NON_CONST_ITERATE(xml::node, child, node) {
        string parameter_name = child->get_name();
        string content = child->get_content();
        if ( content == "") { 
            continue;
        }
        if ( parameter_name == "") { 
            continue;
        }
        result=true;
        model.AddParameter(parameter_name, content);
    }
    return result;
}

////////////////////////////
//
// CRfamModel
//
///////////////////////////

void 
CRfamModel::AddParameter(const string& parameter_name, const string& content)
{
    m_parameters[parameter_name]=content;
}

string 
CRfamModel::Get(const string& parameter_name)
{
    string result;
    if ( m_parameters.find(parameter_name)==m_parameters.end() ) {
        return result;
    }
    result = m_parameters[parameter_name];
    return result;
}

END_NCBI_SCOPE
// END_SCOPE(objects)

