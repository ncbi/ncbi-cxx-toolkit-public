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
 * Author:  Charlie Liu
 *
 * File Description:
 *
 *       Class to maintain lists of preferred and model tax nodes,
 *       using the Cdd-pref-nodes ASN specification.
 *
 * ===========================================================================
 */

#ifndef CU_PREFTAXNODES_HPP
#define CU_PREFTAXNODES_HPP
#include <objects/cdd/Cdd_pref_nodes.hpp>
#include <objects/cdd/Cdd_org_ref.hpp>
#include <algo/structure/cd_utils/cuTaxClient.hpp>
//#include "cuCppNCBI.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

struct OrgNode
{
	OrgNode(int n, const CRef<CCdd_org_ref>& oref) : order(n), orgRef(oref) {}

	const CRef<CCdd_org_ref> orgRef;
	int order;
};

typedef map<int, OrgNode > TaxidToOrgMap;

class CPrefTaxNodes 
{
    static const string PREF_TAXNODE_FILE;
public: 

    //  All data read via the constructors are added to existing data
    //  unless reset = true, in which case existing data is removed first.

    //  Will look for a file under 'data/txnodes.asn' relative to directory of executable.
	CPrefTaxNodes(bool reset = false);

    //  Pass the full path to the file containing preferred tax node info formatted 
    //  in ASN.1 as per the Cdd-pref-nodes spec.
    CPrefTaxNodes(const string& prefTaxnodeFileName, bool reset = false);

    //  Use input to set up the internal maps.
    CPrefTaxNodes(const CCdd_pref_nodes& prefNodes, bool reset = false);

    //  Given a list of taxids for each of the maps, construct the maps.
//    CPrefTaxNodes(const vector< int >& prefTaxids, const vector< int >& modelOrgTaxids, const vector< int >& optionalTaxids, bool reset = false);

    virtual ~CPrefTaxNodes();

    //  Contents of file is loaded:  it is added to existing data
    //  unless doReset = true, in which case existing data is erased.
    //  Cannot currently add to existing contents of pref nodes maps.
    //  Return true only if data was loaded from a file (false will be 
    //  returned if data already exists and doReset = false).
    bool LoadFromFile(const string& prefTaxnodeFileName, bool doReset = false);

    void Reset();

    bool   IsLoaded() {return m_loaded;}
	string GetLastError() {return m_err;};

    //  Looks if taxid is *exactly* one of the model orgs or pref taxnodes.
    bool IsModelOrg(int taxid);
    bool IsPrefTaxnode(int taxid);

    //  If not an exact match w/ a model org or pref taxnode, then ascend the
    //  tax tree to see if one of its ancestors is.  Return the first such tax node's info.
    //  Return value itself is the 'order'.
    int GetModelOrg(int taxid, TaxClient& taxClient, string& orgName); 
    int GetPrefTaxnode(int taxid, TaxClient& taxClient, string& nodeName);

    //  Extract fields from a CCdd_org_ref.
	static string getTaxName(const CRef< CCdd_org_ref >& orgRef);
	static int getTaxId(const CRef< CCdd_org_ref >& orgRef);
	static bool isActive(const CRef< CCdd_org_ref >& orgRef);
//    Use methods from cuSequence instead...
//    static int GetTaxIDInBioseq(const CBioseq& bioseq);
//    static bool isEnvironmentalSeq(const CBioseq& bioseq);
	//string GetTaxNameInBioseq(const CBioseq& bioseq);

protected:
	string m_err;

private:
//	static CCdd_pref_nodes m_prefNodes;
	static TaxidToOrgMap m_prefNodesMap;
	static TaxidToOrgMap m_modelOrgsMap;
	static TaxidToOrgMap m_optionalNodesMap;
	static bool m_loaded;

	TaxidToOrgMap::iterator findAncestor(TaxidToOrgMap& toMap, int taxid, TaxClient& taxClient);

	void putIntoMap(const CCdd_org_ref_set& orgRefs, TaxidToOrgMap& taxidOrgMap);
    void BuildMaps(const CCdd_pref_nodes& prefNodes, bool reset = false);
    bool ReadPreferredTaxnodes(const string& fileName);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_PREFTAXNODES_HPP

/* 
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/06/30 23:53:42  lanczyck
 * extract preferred tax nodes/model organism code from CDTree's algTaxDataSource and repackage for use here
 *
 * ===========================================================================
 */