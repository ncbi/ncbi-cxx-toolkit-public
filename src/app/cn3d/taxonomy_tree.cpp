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
* Authors:  Paul Thiessen
*
* File Description:
*      taxonomy tree stuff
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/09/10 17:02:26  thiessen
* show count for repeated sequences
*
* Revision 1.1  2002/09/09 22:51:19  thiessen
* add basic taxonomy tree viewer
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include <wx/wx.h>
#include <wx/treectrl.h>

// the application icon (under Windows it is in resources)
#if defined(__WXGTK__) || defined(__WXMAC__)
    #include "cn3d/cn3d.xpm"
#endif

#include "cn3d/taxonomy_tree.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/molecule_identifier.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

TaxonomyTree::TaxonomyTree(void)
{
}

bool TaxonomyTree::Init(void)
{
    if (taxonomyServer.IsAlive()) return true;

    wxBeginBusyCursor();    // sometimes takes a while
    bool status = taxonomyServer.Init();
    wxEndBusyCursor();
    if (status && taxonomyServer.IsAlive())
        TESTMSG("taxonomy server connection initialized");
    else
        ERR_POST(Error << "Unable to initialize taxonomy server!");
    return status;
}

TaxonomyTree::~TaxonomyTree(void)
{
    taxonomyServer.Fini();
}

class TaxonomyTreeNode
{
public:
    int taxid, parentTaxid, nDescendentLeaves;
    std::string name;
    // use maps to ensure uniqueness
    typedef std::map < int , bool > ChildTaxIDMap;
    ChildTaxIDMap childTaxids;
    typedef std::map < const MoleculeIdentifier * , int > IdentifierList;
    IdentifierList identifiers;

    TaxonomyTreeNode(void) { taxid = parentTaxid = nDescendentLeaves = 0; }
};

// map taxid -> node
typedef std::map < int , TaxonomyTreeNode > TaxonomyTreeMap;

static void AppendChildrenToTree(wxTreeCtrl *tree,
    const TaxonomyTreeMap& treeMap, const TaxonomyTreeNode& node, const wxTreeItemId id)
{
    // add child nodes
    if (node.childTaxids.size() > 0 && node.identifiers.size() == 0) {
        TaxonomyTreeNode::ChildTaxIDMap::const_iterator c, ce = node.childTaxids.end();
        for (c=node.childTaxids.begin(); c!=ce; c++) {
            const TaxonomyTreeNode& childNode = treeMap.find(c->first)->second;
            wxString name;
            name.Printf("%s (%i)", childNode.name.c_str(), childNode.nDescendentLeaves);
            wxTreeItemId childId = tree->AppendItem(id, name);
            AppendChildrenToTree(tree, treeMap, childNode, childId);
        }
    }

    else if (node.childTaxids.size() == 0 && node.identifiers.size() > 0) {
        TaxonomyTreeNode::IdentifierList::const_iterator i, ie = node.identifiers.end();
        for (i=node.identifiers.begin(); i!=ie; i++) {
            wxString name(i->first->ToString().c_str());
            if (i->second > 1)
                name.Printf("%s (x%i)", name.c_str(), i->second);
            tree->AppendItem(id, name);
        }
    }

    else
        ERR_POST(Error << "AppendChildrenToTree() got node with both children and identifiers, or neither!");
}

static void AddNode(TaxonomyTreeMap *taxTree, const Sequence *seq,
    int taxid, const CTaxon2_data *taxData, int parent)
{
    // set info for child node
    TaxonomyTreeNode& node = (*taxTree)[taxid];
    node.taxid = taxid;
    node.parentTaxid = parent;
    node.name = (taxData->IsSetOrg() && taxData->GetOrg().IsSetTaxname()) ?
        taxData->GetOrg().GetTaxname() : std::string("(error getting node name!)");
    if (seq) {
        node.identifiers[seq->identifier]++;
        node.nDescendentLeaves++;
    }

    // set info for parent node
    TaxonomyTreeNode& parentNode = (*taxTree)[parent];
    parentNode.childTaxids[taxid] = true;
    parentNode.nDescendentLeaves++;
}

void TaxonomyTree::ShowTreeForAlignment(const BlockMultipleAlignment *alignment)
{
    wxBeginBusyCursor();    // sometimes takes a while

    // holds tree structure
    TaxonomyTreeMap taxTree;

    // build a tree of all sequences with known taxonomy
    int row, taxid, parent;
    const CTaxon2_data *taxData;
    for (row=0; row<alignment->NRows(); row++) {
        const Sequence *seq = alignment->GetSequenceOfRow(row);
        taxid = GetTaxIDForSequence(seq);
        taxData = (taxid != 0) ? GetTaxInfoForTaxID(taxid) : NULL;
        if (!taxData) continue;

        // add node to tree
        do {
            // find parent
            parent = GetParentTaxID(taxid);
            if (parent == 0) break;

            // add node to tree
            AddNode(&taxTree, seq, taxid, taxData, parent);
            seq = NULL; // only add sequence to first node (leaf)

            // on to next level up
            taxid = parent;
            if (taxid > 1) {    // no tax info for root
                taxData = GetTaxInfoForTaxID(taxid);
                if (!taxData) break;
            }
        } while (taxid > 1);    // 1 is root tax node
    }
    TESTMSG("apparent # leaves in tree: " << taxTree[1].nDescendentLeaves);
    TaxonomyTreeNode *node = &(taxTree[1]);
    while (node->childTaxids.size() == 1) node = &(taxTree[node->childTaxids.begin()->first]);
    TESTMSG("deepest node containing all leaves: " << node->name);

    // for now, create simple wx window with the tree
    wxFrame *frame = new wxFrame(NULL, -1, "Taxonomy Tree", wxDefaultPosition, wxSize(200,200));
    wxTreeCtrl *tree = new wxTreeCtrl(frame, -1);
    wxString name;
    name.Printf("%s (%i)", node->name.c_str(), node->nDescendentLeaves);
    AppendChildrenToTree(tree, taxTree, *node, tree->AddRoot(name));
    frame->Layout();
    frame->SetIcon(wxICON(cn3d));
    frame->Show(true);

    wxEndBusyCursor();
}

int TaxonomyTree::GetTaxIDForSequence(const Sequence *seq)
{
    // check cache first
    TaxonomyIDMap::const_iterator id = taxonomyIDs.find(seq->identifier);
    if (id != taxonomyIDs.end()) return id->second;

    if (!Init()) return 0;
    int taxid = 0;
    std::string err = "no gi or source info";

    // try to get "official" tax info from gi
    if (seq->identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
        if (!taxonomyServer.GetTaxId4GI(seq->identifier->gi, taxid)) {
            taxid = 0;
            err = taxonomyServer.GetLastError();
        }
    }

    // otherwise, try to get it from org info in Bioseq
    if (taxid == 0 && seq->bioseqASN->IsSetDescr()) {
        CBioseq::TDescr::Tdata::const_iterator d, de = seq->bioseqASN->GetDescr().Get().end();
        for (d=seq->bioseqASN->GetDescr().Get().begin(); d!=de; d++) {
            const COrg_ref *org = NULL;
            if ((*d)->IsOrg())
                org = &((*d)->GetOrg());
            else if ((*d)->IsSource())
                org = &((*d)->GetSource().GetOrg());
            if (org) {
                if ((taxid=taxonomyServer.GetTaxIdByOrgRef(*org)) != 0)
                    break;
                else
                    err = taxonomyServer.GetLastError();
            }
        }
    }

    // add taxid to cache
    if (taxid == 0)
        ERR_POST(Warning << "Unable to get taxonomy for " << seq->identifier->ToString()
            << "; reason: " << err);
//    else
//        TESTMSG(seq->identifier->ToString() << " is from taxid " << taxid);
    taxonomyIDs[seq->identifier] = taxid;
    return taxid;
}

const ncbi::objects::CTaxon2_data * TaxonomyTree::GetTaxInfoForTaxID(int taxid)
{
    // check cache first
    TaxonomyInfoMap::const_iterator i = taxonomyInfo.find(taxid);
    if (i != taxonomyInfo.end()) return i->second.GetPointer();

    // if not present, query server
    if (!Init()) return NULL;
    CRef < CTaxon2_data > data = taxonomyServer.GetById(taxid);

    // add to cache
    if (data.Empty())
        ERR_POST(Warning << "Unable to get taxonomy data for taxid " << taxid
            << "; reason: " << taxonomyServer.GetLastError());
//    else if (data->IsSetOrg() && data->GetOrg().IsSetTaxname())
//        TESTMSG("taxid " << taxid << " is " << data->GetOrg().GetTaxname());
    taxonomyInfo[taxid] = data;
    return data.GetPointer();
}

int TaxonomyTree::GetParentTaxID(int taxid)
{
    // check cache first
    TaxonomyParentMap::const_iterator p = taxonomyParents.find(taxid);
    if (p != taxonomyParents.end()) return p->second;

    // if not present, query server
    if (!Init()) return 0;
    int parent = taxonomyServer.GetParent(taxid);

    // add to cache
    if (parent == 0)
        ERR_POST(Warning << "Unable to get parent for taxid " << taxid
            << "; reason: " << taxonomyServer.GetLastError());
//    else
//        TESTMSG("taxid " << parent << " is parent of " << taxid);
    taxonomyParents[taxid] = parent;
    return parent;
}

END_SCOPE(Cn3D)

