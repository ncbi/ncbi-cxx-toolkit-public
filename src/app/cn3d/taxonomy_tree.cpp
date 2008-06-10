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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include "remove_header_conflicts.hpp"

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/treectrl.h>

// the application icon (under Windows it is in resources)
#if defined(__WXGTK__) || defined(__WXMAC__)
    #include "cn3d42App.xpm"
#endif

#include "taxonomy_tree.hpp"
#include "cn3d_tools.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "molecule_identifier.hpp"
#include "messenger.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

class TaxonomyWindow : public wxFrame
{
    friend class TaxonomyTree;

public:
    TaxonomyWindow(wxFrame *parent, TaxonomyWindow **handle);
    ~TaxonomyWindow(void);

private:
    wxTreeCtrl *tree;
    TaxonomyWindow **handle;

    void OnActivate(wxTreeEvent& event);

    DECLARE_EVENT_TABLE()
};

class NodeData : public wxTreeItemData
{
public:
    NodeData(const Sequence *s) : sequence(s) { }

    const Sequence *sequence;
};

BEGIN_EVENT_TABLE(TaxonomyWindow, wxFrame)
    EVT_TREE_ITEM_ACTIVATED(-1, TaxonomyWindow::OnActivate)
END_EVENT_TABLE()

TaxonomyWindow::TaxonomyWindow(wxFrame *parent, TaxonomyWindow **thisHandle) :
    wxFrame(parent, -1, "Taxonomy Tree", wxPoint(75,75), wxSize(400,400),
        wxDEFAULT_FRAME_STYLE
#if defined(__WXMSW__)
                | wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR | wxFRAME_FLOAT_ON_PARENT
#endif
    ),
    handle(thisHandle)
{
    // for now, create simple wx window with the tree
    tree = new wxTreeCtrl(this, -1, wxPoint(0,0), GetClientSize());
    SetIcon(wxICON(cn3d));
}

TaxonomyWindow::~TaxonomyWindow(void)
{
    if (handle) *handle = NULL;
}


static void ExpandAll(wxTreeCtrl& tree, const wxTreeItemId& id, bool shouldExpand, int toLevel)
{
    if (toLevel == 0 || !tree.ItemHasChildren(id)) return;

    // shouldExpand/collapse this node
    bool isExpanded = tree.IsExpanded(id);
    if (shouldExpand && !isExpanded)
        tree.Expand(id);
    else if (!shouldExpand && isExpanded)
        tree.Collapse(id);

    // descend tree and shouldExpand/collapse all children
    wxTreeItemIdValue cookie = &tree;
    for (wxTreeItemId child=tree.GetFirstChild(id, cookie); child.IsOk(); child=tree.GetNextChild(id, cookie))
        ExpandAll(tree, child, shouldExpand, toLevel - 1);
}

void TaxonomyWindow::OnActivate(wxTreeEvent& event)
{
    const wxTreeItemId& itemID = event.GetItem();
    bool keyActivated = (event.GetPoint().x == 0 && event.GetPoint().y == 0);
    bool hasChildren = tree->ItemHasChildren(itemID);

    if (keyActivated && hasChildren) {
        // expand/collapse entire tree at internal node
        ExpandAll(*tree, itemID, !tree->IsExpanded(itemID), -1);
    }

    else if (!keyActivated && !hasChildren) {
        // highlight sequence on double-click
        NodeData *data = dynamic_cast<NodeData*>(tree->GetItemData(itemID));
        if (data)
            GlobalMessenger()->HighlightAndShowSequence(data->sequence);
    }

    event.Skip();
}


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
        INFOMSG("taxonomy server connection initialized");
    else
        ERRORMSG("Unable to initialize taxonomy server!");
    return status;
}

TaxonomyTree::~TaxonomyTree(void)
{
    taxonomyServer.Fini();

    TaxonomyWindowList::iterator w, we = taxonomyWindows.end();
    for (w=taxonomyWindows.begin(); w!=we; ++w) {
        if (**w) {
            (**w)->handle = NULL;
            (**w)->Destroy();
        }
        delete *w;
    }
}

class TaxonomyTreeNode
{
public:
    int taxid, parentTaxid, nDescendentLeaves;
    string name;
    // use maps to ensure uniqueness
    typedef map < int , bool > ChildTaxIDMap;
    ChildTaxIDMap childTaxids;
    // int here is to count occurrences
    typedef map < const Sequence * , int > SequenceMap;
    SequenceMap sequences;

    TaxonomyTreeNode(void) { taxid = parentTaxid = nDescendentLeaves = 0; }
};

// map taxid -> node
typedef map < int , TaxonomyTreeNode > TaxonomyTreeMap;

static void AppendChildrenToTree(wxTreeCtrl *tree, const TaxonomyTreeMap& treeMap,
    const TaxonomyTreeNode& node, const wxTreeItemId id, bool abbreviated)
{
    // add sequence nodes
    if (node.sequences.size() > 0) {
        TaxonomyTreeNode::SequenceMap::const_iterator s, se = node.sequences.end();
        for (s=node.sequences.begin(); s!=se; ++s) {
            wxString name(s->first->identifier->ToString().c_str());
            if (s->second > 1) {
	    	wxString tmp = name;
                name.Printf("%s (x%i)", tmp.c_str(), s->second);
	    }
            const wxTreeItemId& child = tree->AppendItem(id, name);
            tree->SetItemData(child, new NodeData(s->first));
        }
    }

    // add heirarchy nodes
    if (node.childTaxids.size() > 0) {
        TaxonomyTreeNode::ChildTaxIDMap::const_iterator c, ce = node.childTaxids.end();
        for (c=node.childTaxids.begin(); c!=ce; ++c) {
            const TaxonomyTreeNode *childNode = &(treeMap.find(c->first)->second);
            wxString name = childNode->name.c_str();
            if (abbreviated) {
                while (childNode->sequences.size() == 0 && childNode->childTaxids.size() == 1 &&
                       treeMap.find(childNode->childTaxids.begin()->first)->second.sequences.size() == 0)
                    childNode = &(treeMap.find(childNode->childTaxids.begin()->first)->second);
                if (childNode->name != name.c_str())
                    name += wxString(" . . . ") + childNode->name.c_str();
            }
	    wxString tmp = name;
            name.Printf("%s (%i)", tmp.c_str(), childNode->nDescendentLeaves);
            wxTreeItemId childId = tree->AppendItem(id, name);
            AppendChildrenToTree(tree, treeMap, *childNode, childId, abbreviated);
        }
    }
}

static void AddNode(TaxonomyTreeMap *taxTree, const Sequence *seq,
    int taxid, const CTaxon2_data *taxData, int parent)
{
    // set info for child node
    TaxonomyTreeNode& node = (*taxTree)[taxid];
    node.taxid = taxid;
    node.parentTaxid = parent;
    node.name = (taxData->IsSetOrg() && taxData->GetOrg().IsSetTaxname()) ?
        taxData->GetOrg().GetTaxname() : string("(error getting node name!)");
    if (seq) {
        ++(node.sequences[seq]);
        ++(node.nDescendentLeaves);
    }

    // set info for parent node
    TaxonomyTreeNode& parentNode = (*taxTree)[parent];
    parentNode.childTaxids[taxid] = true;
    ++(parentNode.nDescendentLeaves);
}

void TaxonomyTree::ShowTreeForAlignment(wxFrame *windowParent,
    const BlockMultipleAlignment *alignment, bool abbreviated)
{
    wxBeginBusyCursor();    // sometimes takes a while

    // holds tree structure
    TaxonomyTreeMap taxTree;

    // build a tree of all sequences with known taxonomy
    int taxid, parent;
    const CTaxon2_data *taxData;
    for (unsigned int row=0; row<alignment->NRows(); ++row) {
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
    taxTree[1].name = "Global Root";
    TRACEMSG("apparent # leaves in tree: " << taxTree[1].nDescendentLeaves);
    TaxonomyTreeNode *node = &(taxTree[1]);
    while (node->childTaxids.size() == 1) node = &(taxTree[node->childTaxids.begin()->first]);
    INFOMSG("deepest node containing all leaves: " << node->name);

    TaxonomyWindow *window;
    TaxonomyWindow **handle = new TaxonomyWindow*;
    *handle = window = new TaxonomyWindow(windowParent, handle);
    wxString name;
    name.Printf("%s (%i)", node->name.c_str(), node->nDescendentLeaves);
    AppendChildrenToTree(window->tree, taxTree, *node, window->tree->AddRoot(name), abbreviated);
    ExpandAll(*(window->tree), window->tree->GetRootItem(), true, 2);
    window->Show(true);
    taxonomyWindows.push_back(handle);

    wxEndBusyCursor();
}

int TaxonomyTree::GetTaxIDForSequence(const Sequence *seq)
{
    // check cache first
    TaxonomyIDMap::const_iterator id = taxonomyIDs.find(seq->identifier);
    if (id != taxonomyIDs.end()) return id->second;

    if (!Init()) return 0;
    int taxid = 0;
    string err = "no gi or source info";

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
        for (d=seq->bioseqASN->GetDescr().Get().begin(); d!=de; ++d) {
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
        WARNINGMSG("Unable to get taxonomy for " << seq->identifier->ToString()
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
        WARNINGMSG("Unable to get taxonomy data for taxid " << taxid
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
        WARNINGMSG("Unable to get parent for taxid " << taxid
            << "; reason: " << taxonomyServer.GetLastError());
//    else
//        TESTMSG("taxid " << parent << " is parent of " << taxid);
    taxonomyParents[taxid] = parent;
    return parent;
}

END_SCOPE(Cn3D)
