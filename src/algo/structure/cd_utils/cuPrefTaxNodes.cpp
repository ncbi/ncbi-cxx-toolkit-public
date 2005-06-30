/* $Id$
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
 * Author:  Charlie
 *
 * File Description:
 *
 *       Class to maintain lists of preferred and model tax nodes,
 *       using the Cdd-pref-nodes ASN specification.
 *
 * ===========================================================================
 */


#include <ncbi_pch.hpp>
#include <objects/cdd/Cdd_org_ref_set.hpp>
#include <algo/structure/cd_utils/cuPrefTaxNodes.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

//extern string launchDirectory;
const string CJL_LaunchDirectory = "E:\\Users\\lanczyck\\CDTree\\Code\\c++\\compilers\\msvc710_prj\\static\\bin\\debug\\";

const string CPrefTaxNodes::PREF_TAXNODE_FILE  = "data/txnodes.asn";
//CCdd_pref_nodes CPrefTaxNodes::m_prefNodes;
TaxidToOrgMap CPrefTaxNodes::m_prefNodesMap;
TaxidToOrgMap CPrefTaxNodes::m_modelOrgsMap;
TaxidToOrgMap CPrefTaxNodes::m_optionalNodesMap;
bool CPrefTaxNodes::m_loaded = false;
   
CPrefTaxNodes::CPrefTaxNodes(bool reset)
{
//    string filename = PREF_TAXNODE_FILE;
    string filename = CJL_LaunchDirectory + PREF_TAXNODE_FILE;
    LoadFromFile(filename, reset);
}

CPrefTaxNodes::CPrefTaxNodes(const string& prefTaxnodeFileName, bool reset)
{
    LoadFromFile(prefTaxnodeFileName, reset);
}

CPrefTaxNodes::CPrefTaxNodes(const CCdd_pref_nodes& prefNodes, bool reset)
{
    BuildMaps(prefNodes, reset);
    m_loaded = true;
}


CPrefTaxNodes::~CPrefTaxNodes() 
{
}

void CPrefTaxNodes::Reset() {

	m_err = "";
    m_loaded = false;

//    m_prefNodes.Reset();
    m_prefNodesMap.clear();
	m_modelOrgsMap.clear();
	m_optionalNodesMap.clear();
}

    
bool CPrefTaxNodes::LoadFromFile(const string& prefTaxnodeFileName, bool doReset)
{
    bool result = false;
    if (doReset) {
        Reset();
    }

//    if (!m_loaded)
//	{
    if (ReadPreferredTaxnodes(prefTaxnodeFileName)) {
		m_loaded = true;
        result = true;
    } else 
		m_err = "Failed to read preferred Taxonomy nodes from file '" + prefTaxnodeFileName + "'.\n";

    return result;
}
   
bool CPrefTaxNodes::ReadPreferredTaxnodes(const string& filename) 
{
    CCdd_pref_nodes prefNodes;
//    if (!ReadASNFromFile(filename.c_str(), &m_prefNodes, false, &m_err))
    if (!ReadASNFromFile(filename.c_str(), &prefNodes, false, &m_err))
	{
		return false;
	}

    BuildMaps(prefNodes);
    return true;
}

void CPrefTaxNodes::BuildMaps(const CCdd_pref_nodes& prefNodes, bool reset) {
    if (reset)
        Reset();

	//build a taxId/taxName map
	if (prefNodes.CanGetPreferred_nodes())
		putIntoMap(prefNodes.GetPreferred_nodes(), m_prefNodesMap);
	if (prefNodes.CanGetModel_organisms())
		putIntoMap(prefNodes.GetModel_organisms(), m_modelOrgsMap);
	if (prefNodes.CanGetOptional_nodes())
		putIntoMap(prefNodes.GetOptional_nodes(), m_optionalNodesMap);
}

void CPrefTaxNodes::putIntoMap(const CCdd_org_ref_set& orgRefs, TaxidToOrgMap& taxidOrgMap)
{
	const list< CRef< CCdd_org_ref > >& orgList = orgRefs.Get();
	list< CRef< CCdd_org_ref > >::const_iterator cit = orgList.begin();
	int i = 0;
	for (; cit != orgList.end(); cit++)
	{
		taxidOrgMap.insert(TaxidToOrgMap::value_type(getTaxId(*cit), OrgNode(i,*cit)));
		i++;
	}
}

string CPrefTaxNodes::getTaxName(const CRef< CCdd_org_ref >& orgRef)
{
	if (orgRef->CanGetReference())
	{
		const COrg_ref& org = orgRef->GetReference();
		return org.GetTaxname();
	}
	else
		return kEmptyStr;
}

int CPrefTaxNodes::getTaxId(const CRef< CCdd_org_ref >& orgRef)
{
	if (orgRef->CanGetReference())
	{
		const COrg_ref& org = orgRef->GetReference();
		return org.GetTaxId();
	}
	else
		return 0;
}

bool CPrefTaxNodes::isActive(const CRef< CCdd_org_ref >& orgRef)
{
	return orgRef->GetActive();
}


TaxidToOrgMap::iterator  CPrefTaxNodes::findAncestor(TaxidToOrgMap& toMap, int taxid, TaxClient& taxClient)
{
	TaxidToOrgMap::iterator tit = toMap.begin();
	for(; tit != toMap.end(); tit++)
	{
		if (taxClient.IsTaxDescendant(tit->first, taxid))
			return tit;
	}
	return toMap.end();
}

bool CPrefTaxNodes::IsModelOrg(int taxid) 
{
	TaxidToOrgMap::iterator it = m_modelOrgsMap.find(taxid);	
    return it != m_modelOrgsMap.end();
}

//  return index into list; -1 if fails
int CPrefTaxNodes::GetModelOrg(int taxid, TaxClient& taxClient, string& nodeName) 
{
	nodeName.erase();
	TaxidToOrgMap::iterator it = m_modelOrgsMap.find(taxid);	
    if (it != m_modelOrgsMap.end())
	{
		nodeName.append(getTaxName(it->second.orgRef));
		return it->second.order;
	}
	else // fail to find exact match; try to find ancetral match
	{
		it = findAncestor(m_modelOrgsMap, taxid, taxClient);
		if (it != m_modelOrgsMap.end())
		{
			nodeName.append(getTaxName(it->second.orgRef));
			return it->second.order;
		}
	}
	return -1;
}
    
bool CPrefTaxNodes::IsPrefTaxnode(int taxid) 
{
	TaxidToOrgMap::iterator it = m_prefNodesMap.find(taxid);	
    return it != m_prefNodesMap.end();
}

//  return index into list; -1 if fails
int  CPrefTaxNodes::GetPrefTaxnode(int taxid, TaxClient& taxClient, string& nodeName) 
{
	nodeName.erase();
    TaxidToOrgMap::iterator it = findAncestor(m_prefNodesMap, taxid, taxClient);
	if (it != m_prefNodesMap.end())
	{
		nodeName.append(getTaxName(it->second.orgRef));
		return it->second.order;
	}
	else
		return -1;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE


/* 
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/06/30 23:53:16  lanczyck
 * extract preferred tax nodes/model organism code from CDTree's algTaxDataSource and repackage for use here
 *
 * ===========================================================================
 */