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
 * Author:  Repackaged by Chris Lanczycki from Charlie Liu's algTaxDataSource
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

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

struct OrgNode
{
	OrgNode(int n, const CRef<CCdd_org_ref>& oref) : order(n), orgRef(oref) {}

	int order;   //  an index tracking the order of insertion into a particular TaxidOrgMap
	const CRef<CCdd_org_ref> orgRef;
};

typedef map<int, OrgNode > TaxidToOrgMap;

class NCBI_CDUTILS_EXPORT CPriorityTaxNodes 
{
    static const string PREF_TAXNODE_FILE;
public: 

    enum TaxNodeInputType {
        eCddPrefNodes = 1,      //  take only the 'preferred-nodes' field of Cdd-pref-nodes
        eCddModelOrgs = 2,      //  take only the 'model-organisms' field of Cdd-pref-nodes
        eCddOptional  = 4,      //  take only the 'optional-nodes' field of Cdd-pref-nodes
        eCddPrefNodesAll = 7,   //  treat all entries in Cdd-pref-nodes ASN.1 data structure as one set
        eRawTaxIds = 8          //  simply a set of taxonomy ids
    };

    //  All data read via the constructors are added to existing data
    //  unless reset = true, in which case existing data is removed first.

    //  Will look for a file under 'data/txnodes.asn' relative to current directory.
    //  Reads in an object of type CCdd_pref_nodes.
	CPriorityTaxNodes(TaxNodeInputType inputType);

    //  Pass the full path to the file containing preferred tax node info formatted 
    //  in ASN.1 as per the Cdd-pref-nodes spec.
    CPriorityTaxNodes(const string& prefTaxnodeFileName, TaxNodeInputType inputType);

    //  Use input to set up the internal maps.
    CPriorityTaxNodes(const CCdd_pref_nodes& prefNodes, TaxNodeInputType inputType);

    //  Given a list of taxids for each of the maps, construct the maps.
    CPriorityTaxNodes(const vector< int >& taxids, TaxClient& taxClient, TaxNodeInputType inputType = eRawTaxIds);

    virtual ~CPriorityTaxNodes();

    //  Contents of file is loaded:  it is added to existing data
    //  unless doReset = true, in which case existing data is erased.
    //  Cannot currently add to existing contents of pref nodes maps.
    //  Return true only if data was loaded from a file (false will be 
    //  returned if data already exists and doReset = false).
    bool LoadFromFile(const string& prefTaxnodeFileName, bool doReset = false);

    //  Loads nodes from 'prefNodes' using same type as m_inputType.
    unsigned int Load(const CCdd_pref_nodes& prefNodes, bool doReset = false);

    //  If want to also reset the node input type, pass a valid pointer.
    //  The selected tax node map is always cleared, but by default the ancestor map
    //  is not cleared unless a) the input type changes and doesn't include the current
    //  input type, or b) the 'forceClearAncestorMap' is true.
    void Reset(TaxNodeInputType* inputType = NULL, bool forceClearAncestorMap = false);

    bool   IsLoaded() {return m_loaded;}
	string GetLastError() {return m_err;}
    TaxNodeInputType GetNodeInputType() const {return m_inputType;}

    //  Looks if taxid is *exactly* one of the priority tax nodes.
    bool IsPriorityTaxnode(int taxid);

    //  If taxidIn is not an a priority taxnode, return its nearest ancestor that is.  
    //  Use taxClient to ascend the lineage of taxidIn in the full taxonomy tree.
    //  Return true if a priorityTaxid is found; otherwise return false and set priorityTaxid = 0.
    bool GetPriorityTaxid(int taxidIn, int& priorityTaxid, TaxClient& taxClient);
    bool GetPriorityTaxidAndName(int taxidIn, int& priorityTaxid, string& nodeName, TaxClient& taxClient);

    //  If not an exact match w/ a priority taxnode, and there is
    //  not an entry for the taxid in the corresponding ancestral map, use taxClient
    //  to ascend the tax tree to see if one of its ancestors is a match.  Return the 
    //  first such tax node's info.
    //  Return value itself is OrgNode.order, or -1 on failure or if taxid = 0.
    int GetPriorityTaxnode(int taxid, string& nodeName, TaxClient* taxClient = NULL);
    int GetPriorityTaxnode(int taxid, const OrgNode*& orgNode, TaxClient* taxClient = NULL);

    //  Extract fields from a CCdd_org_ref.
	static string getTaxName(const CRef< CCdd_org_ref >& orgRef);
	static int getTaxId(const CRef< CCdd_org_ref >& orgRef);
	static bool isActive(const CRef< CCdd_org_ref >& orgRef);

    //  Conversion functions between a collection of taxonomy ids and a CCdd_org_ref_set
    //  object, which ultimately packages COrg_ref objects.  Return value is the number
    //  of CCdd_org_ref/int added to the 2nd argument; last argument when present identifies
    //  those inputs which could not be converted.  In both, no sentinel placeholders
    //  are left in the output data structure for failures --> output index not guaranteed
    //  to correspond to input index.
    //  NOTE:  2nd argument is not cleared/reset before adding new data.
    static unsigned int CddOrgRefSetToTaxIds(const CCdd_org_ref_set& cddOrgRefSet, vector< int >& taxids, vector<int>* notAddedIndices = NULL);
    static unsigned int TaxIdsToCddOrgRefSet(const vector< int >& taxids, CCdd_org_ref_set& cddOrgRefSet, TaxClient& taxClient, vector<int>* notAddedTaxids = NULL);

protected:
	string m_err;

private:
    bool m_loaded;
    TaxNodeInputType m_inputType;

    //  Map not static so that can have several different collections of tax nodes.
	TaxidToOrgMap m_selectedTaxNodesMap;

    //  Use to map a taxid to the taxid found by findAncestor (mapped value may equal key value),
    //  which should have an entry in m_selectedTaxNodesMap.
    //  Maintain these to avoid use of TaxClient when possible.
    typedef map<int, int>      TAncestorMap;
    TAncestorMap   m_ancestralTaxNodeMap;

    //  Return m_selectedTaxNodesMap.end() if no ancestor found for taxid, or taxid = 0.  
    //  Use taxClient only if non-null and the corresponding ancestral map does not contain 
    //  non-zero taxid as a key, or if ancestralMap pointer is NULL (to force use of the TaxClient).
	TaxidToOrgMap::iterator findAncestor(int taxid, TaxClient* taxClient);

	void putIntoMap(const CCdd_org_ref_set& orgRefs);
    void BuildMap(const CCdd_pref_nodes& prefNodes, bool reset = false);
    bool ReadPreferredTaxnodes(const string& fileName, bool reset = false);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_PREFTAXNODES_HPP
