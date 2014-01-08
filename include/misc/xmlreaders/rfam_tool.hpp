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
 * Authors: Azat Badretdin
 *
 * File Description:
 *
 */

#ifndef BACTERIAL_PIPELINE_RFAM_TOOL_HPP
#define BACTERIAL_PIPELINE_RFAM_TOOL_HPP

#include <corelib/ncbistl.hpp>
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/node.hpp>
#include <map>

BEGIN_NCBI_SCOPE
// BEGIN_SCOPE(objects)


class CRfamModel;
class CRfamTool;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CRfamTool::
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
class CRfamTool
{
public: // constructors
    CRfamTool() {}
    CRfamTool(const string& xmlfile);

    
public:    // enum
public: // member functions  
/*  
 *
 *  example of usage:
 *
 *  CRfamTool tool(path_to_xml_file);
 *  // read cmsearch output, obtain current model_identification
 *  CRfamModel model;
 *  tool.GetModelByIdentification(model_identification, model)
 *  // use parameter of the model
 *  string my_parameter_value = model.Get("my_parameter");
*/

    
    bool GetModelByAccession(const string& accession, CRfamModel& model);
    bool GetModelByIdentification(const string& model_identification, CRfamModel& model);
private: //member functions
    static bool sx_GetModel(xml::node& node, CRfamModel& model);

private: // data members
    xml::document m_doc;    
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  CRfamModel::
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
class CRfamModel
{
    typedef map<string, string> TMap;
public: // constructors
    CRfamModel() {}
    
public:    // enum
public: // member functions   
    void AddParameter(const string& parameter_name, const string& content);
    string Get(const string& parameter_name);
    string GetNcRNAClass(void) { return Get("ncRNA_class"); }
private:
    TMap m_parameters;
};
    
// END_SCOPE(objects)
END_NCBI_SCOPE     

#endif /// BACTERIAL_PIPELINE_RFAM_TOOL_HPP