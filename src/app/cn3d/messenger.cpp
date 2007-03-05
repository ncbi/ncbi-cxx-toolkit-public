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
*      Classes to handle messaging and communication between sequence
*      and structure windows
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <memory>

#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set_id.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Residue_interval_pntr.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>

#include "messenger.hpp"
#include "structure_window.hpp"
#include "cn3d_glcanvas.hpp"
#include "sequence_viewer.hpp"
#include "opengl_renderer.hpp"
#include "structure_set.hpp"
#include "chemical_graph.hpp"
#include "sequence_set.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// the global Messenger object
static Messenger messenger;

Messenger * GlobalMessenger(void)
{
    return &messenger;
}


void Messenger::PostRedrawAllStructures(void)
{
    redrawAllStructures = true;
    redrawMolecules.clear();
}

void Messenger::PostRedrawMolecule(const Molecule *molecule)
{
    if (!redrawAllStructures) redrawMolecules[molecule] = true;
}

void Messenger::PostRedrawAllSequenceViewers(void)
{
    redrawAllSequenceViewers = true;
    redrawSequenceViewers.clear();
}
void Messenger::PostRedrawSequenceViewer(ViewerBase *viewer)
{
    if (!redrawAllSequenceViewers) redrawSequenceViewers[viewer] = true;
}

void Messenger::UnPostRedrawAllSequenceViewers(void)
{
    redrawAllSequenceViewers = false;
    redrawSequenceViewers.clear();
}

void Messenger::UnPostRedrawSequenceViewer(ViewerBase *viewer)
{
    if (redrawAllSequenceViewers) {
        SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
        for (q=sequenceViewers.begin(); q!=qe; ++q) redrawSequenceViewers[*q] = true;
        redrawAllSequenceViewers = false;
    }
    RedrawSequenceViewerList::iterator f = redrawSequenceViewers.find(viewer);
    if (f != redrawSequenceViewers.end()) redrawSequenceViewers.erase(f);
}

void Messenger::UnPostStructureRedraws(void)
{
    redrawAllStructures = false;
    redrawMolecules.clear();
}

void Messenger::ProcessRedraws(void)
{
    if (redrawAllSequenceViewers) {
        SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
        for (q=sequenceViewers.begin(); q!=qe; ++q) (*q)->Refresh();
        redrawAllSequenceViewers = false;
    }
    else if (redrawSequenceViewers.size() > 0) {
        RedrawSequenceViewerList::const_iterator q, qe = redrawSequenceViewers.end();
        for (q=redrawSequenceViewers.begin(); q!=qe; ++q) q->first->Refresh();
        redrawSequenceViewers.clear();
    }

    if (redrawAllStructures) {
        if (structureWindow) {
            structureWindow->glCanvas->SetCurrent();
            structureWindow->glCanvas->renderer->Construct();
            structureWindow->glCanvas->renderer->NewView();
            structureWindow->glCanvas->Refresh(false);
        }
		redrawAllStructures = false;
    }
    else if (redrawMolecules.size() > 0) {
        map < const StructureObject * , bool > hetsRedrawn;
        RedrawMoleculeList::const_iterator m, me = redrawMolecules.end();
        for (m=redrawMolecules.begin(); m!=me; ++m) {
            const StructureObject *object;
            if (!m->first->GetParentOfType(&object)) continue;

            // hets/solvents are always redrawn with each molecule, so don't need to repeat
            if ((m->first->IsSolvent() || m->first->IsHeterogen()) &&
                hetsRedrawn.find(object) != hetsRedrawn.end()) continue;

            object->graph->RedrawMolecule(m->first->id);
            hetsRedrawn[object] = true;
        }
        if (structureWindow) {
            structureWindow->glCanvas->renderer->NewView();
            structureWindow->glCanvas->Refresh(false);
        }
        redrawMolecules.clear();
    }
}

void Messenger::RemoveStructureWindow(const StructureWindow *window)
{
    if (window != structureWindow)
        ERRORMSG("Messenger::RemoveStructureWindow() - window mismatch");
    structureWindow = NULL;
}

void Messenger::RemoveSequenceViewer(const ViewerBase *sequenceViewer)
{
    SequenceViewerList::iterator t, te = sequenceViewers.end();
    for (t=sequenceViewers.begin(); t!=te; ++t) {
        if (*t == sequenceViewer) sequenceViewers.erase(t);
        break;
    }
}

void Messenger::SequenceWindowsSave(bool prompt)
{
    SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; ++q)
        (*q)->SaveDialog(prompt);
}

void Messenger::NewSequenceViewerFont(void)
{
    SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; ++q)
        (*q)->NewFont();
}


///// highlighting functions /////

bool Messenger::IsHighlighted(const MoleculeIdentifier *identifier, int index) const
{
    if (highlightingSuspended) return false;

    MoleculeHighlightMap::const_iterator h = highlights.find(identifier);
    if (h == highlights.end()) return false;

    if (index == -1) return true;   // special check for highlight anywhere

    if (index < 0 || index >= (int)h->second.size()) {
        ERRORMSG("Messenger::IsHighlighted() - index out of range");
        return false;
    } else
        return h->second[index];
}

bool Messenger::IsHighlighted(const Molecule *molecule, int residueID) const
{
    return IsHighlighted(molecule->identifier, residueID - 1);  // assume index = id - 1
}

bool Messenger::IsHighlighted(const Sequence *sequence, unsigned int seqIndex) const
{
    return IsHighlighted(sequence->identifier, seqIndex);
}

bool Messenger::IsHighlightedAnywhere(const MoleculeIdentifier *identifier) const
{
    return IsHighlighted(identifier, -1);
}

bool Messenger::IsHighlightedAnywhere(const Molecule *molecule) const
{
    return IsHighlighted(molecule->identifier, -1);
}

void Messenger::RedrawMoleculesWithIdentifier(const MoleculeIdentifier *identifier, const StructureSet *set)
{
    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    ChemicalGraph::MoleculeMap::const_iterator m, me;
    for (o=set->objects.begin(); o!=oe; ++o) {
        for (m=(*o)->graph->molecules.begin(), me=(*o)->graph->molecules.end(); m!=me; ++m) {
            if (m->second->identifier == identifier)
                PostRedrawMolecule(m->second);
        }
    }
}

void Messenger::AddHighlights(const Sequence *sequence, unsigned int seqIndexFrom, unsigned int seqIndexTo)
{
    if (seqIndexFrom > seqIndexTo || seqIndexFrom >= sequence->Length() || seqIndexTo >= sequence->Length()) {
        ERRORMSG("Messenger::AddHighlights() - seqIndex out of range");
        return;
    }

    MoleculeHighlightMap::iterator h = highlights.find(sequence->identifier);
    if (h == highlights.end()) {
        highlights[sequence->identifier].resize(sequence->Length(), false);
        h = highlights.find(sequence->identifier);
    }

    for (unsigned int i=seqIndexFrom; i<=seqIndexTo; ++i) h->second[i] = true;

    PostRedrawAllSequenceViewers();
    RedrawMoleculesWithIdentifier(sequence->identifier, sequence->parentSet);
}

void Messenger::HighlightAndShowSequence(const Sequence *sequence)
{
    RemoveAllHighlights(true);
    AddHighlights(sequence, 0, sequence->Length() - 1);

    SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; ++q)
        (*q)->MakeSequenceVisible(sequence->identifier);
}

void Messenger::KeepHighlightsOnlyOnSequence(const Sequence *sequence)
{
    if (highlights.size() == 0 || (highlights.size() == 1 && highlights.begin()->first == sequence->identifier))
        return;

    MoleculeHighlightMap newHighlights;
    MoleculeHighlightMap::const_iterator h, he = highlights.end();
    for (h=highlights.begin(); h!=he; ++h) {
        if (h->first == sequence->identifier) {
            newHighlights[sequence->identifier] = h->second;
            break;
        }
    }
    if (h == he) {
        ERRORMSG("Selected sequence has no highlights!");
        return;
    }
    highlights.clear();
    highlights = newHighlights;
    PostRedrawAllStructures();
    PostRedrawAllSequenceViewers();
}

void Messenger::RemoveHighlights(const Sequence *sequence, unsigned int seqIndexFrom, unsigned int seqIndexTo)
{
    if (seqIndexFrom > seqIndexTo || seqIndexFrom >= sequence->Length() || seqIndexTo >= sequence->Length()) {
        ERRORMSG("Messenger::RemoveHighlights() - seqIndex out of range");
        return;
    }

    MoleculeHighlightMap::iterator h = highlights.find(sequence->identifier);
    if (h != highlights.end()) {
        unsigned int i;
        for (i=seqIndexFrom; i<=seqIndexTo; ++i) h->second[i] = false;

        // remove sequence from store if no highlights left
        for (i=0; i<sequence->Length(); ++i)
            if (h->second[i] == true) break;
        if (i == sequence->Length())
            highlights.erase(h);

        PostRedrawAllSequenceViewers();
        RedrawMoleculesWithIdentifier(sequence->identifier, sequence->parentSet);
    }
}

void Messenger::ToggleHighlights(const MoleculeIdentifier *identifier, unsigned int indexFrom, unsigned int indexTo,
    const StructureSet *set)
{
    if (indexFrom > indexTo || indexFrom >= identifier->nResidues || indexTo >= identifier->nResidues) {
        ERRORMSG("Messenger::ToggleHighlights() - index out of range");
        return;
    }

    MoleculeHighlightMap::iterator h = highlights.find(identifier);
    if (h == highlights.end()) {
        highlights[identifier].resize(identifier->nResidues, false);
        h = highlights.find(identifier);
    }

    unsigned int i;
    for (i=indexFrom; i<=indexTo; ++i) h->second[i] = !h->second[i];

    // remove sequence from store if no highlights left
    for (i=0; i<h->second.size(); ++i)
        if (h->second[i] == true) break;
    if (i == h->second.size())
        highlights.erase(h);

    PostRedrawAllSequenceViewers();
    RedrawMoleculesWithIdentifier(identifier, set);
}

void Messenger::ToggleHighlights(const Sequence *sequence, unsigned int seqIndexFrom, unsigned int seqIndexTo)
{
    ToggleHighlights(sequence->identifier, seqIndexFrom, seqIndexTo, sequence->parentSet);
}

void Messenger::ToggleHighlight(const Molecule *molecule, int residueID, bool scrollViewersTo)
{
    // assume index = id - 1
    ToggleHighlights(molecule->identifier, residueID - 1, residueID - 1, molecule->parentSet);

    if (scrollViewersTo) {
        // make selected residue visible in sequence viewers if residue is in displayed sequence
        SequenceViewerList::iterator t, te = sequenceViewers.end();
        for (t=sequenceViewers.begin(); t!=te; ++t)
            (*t)->MakeResidueVisible(molecule, residueID - 1);
    }
}

bool Messenger::RemoveAllHighlights(bool postRedraws)
{
    bool anyRemoved = highlights.size() > 0;

    if (postRedraws) {
        if (anyRemoved) PostRedrawAllSequenceViewers();

        if (structureWindow) {
            MoleculeHighlightMap::const_iterator h, he = highlights.end();
            for (h=highlights.begin(); h!=he; ++h)
                RedrawMoleculesWithIdentifier(h->first, structureWindow->glCanvas->structureSet);
        }
    }

    highlights.clear();

    return anyRemoved;
}

void Messenger::CacheHighlights(void)
{
    highlightCache = highlights;
}

void Messenger::RestoreCachedHighlights(void)
{
    highlights = highlightCache;
    PostRedrawAllSequenceViewers();
    PostRedrawAllStructures();
}

void Messenger::GetHighlights(MoleculeHighlightMap *copyHighlights)
{
    *copyHighlights = highlights;    // copy the lists
}

void Messenger::SetHighlights(const MoleculeHighlightMap& newHighlights)
{
    RemoveAllHighlights(true);
    highlights = newHighlights;

    PostRedrawAllSequenceViewers();
    if (structureWindow) {
        MoleculeHighlightMap::const_iterator h, he = highlights.end();
        for (h=highlights.begin(); h!=he; ++h)
            RedrawMoleculesWithIdentifier(h->first, structureWindow->glCanvas->structureSet);
    }
}

void Messenger::SuspendHighlighting(bool suspend)
{
    if (highlightingSuspended != suspend) {
        highlightingSuspended = suspend;
        if (IsAnythingHighlighted()) {
            PostRedrawAllStructures();
            PostRedrawAllSequenceViewers();
        }
    }
}

bool Messenger::GetHighlightedResiduesWithStructure(MoleculeHighlightMap *residues) const
{
    residues->clear();
    if (!IsAnythingHighlighted()) return false;

    MoleculeHighlightMap::const_iterator h, he = highlights.end();
    for (h=highlights.begin(); h!=he; ++h) {
        if (h->first->HasStructure())
            (*residues)[h->first] = h->second;
    }

    return (residues->size() > 0);
}

CBiostruc_annot_set * Messenger::CreateBiostrucAnnotSetForHighlightsOnSingleObject(void) const
{
    if (!IsAnythingHighlighted()) {
        ERRORMSG("Nothing highlighted");
        return NULL;
    }

    // check to see that all highlights are on a single structure object
    int mmdbID;
    MoleculeHighlightMap::const_iterator h, he = highlights.end();
    for (h=highlights.begin(); h!=he; ++h) {
        if (h == highlights.begin()) mmdbID = h->first->mmdbID;
        if (h->first->mmdbID == MoleculeIdentifier::VALUE_NOT_SET || h->first->mmdbID != mmdbID) {
            ERRORMSG("All highlights must be on a single PDB structure");
            return NULL;
        }
        if (h->first->moleculeID == MoleculeIdentifier::VALUE_NOT_SET) {
            ERRORMSG("internal error - MoleculeIdentifier has no moleculeID");
            return NULL;
        }
    }

    // create the Biostruc-annot-set
    CRef < CBiostruc_annot_set > bas(new CBiostruc_annot_set());

    // set id
    CRef < CBiostruc_id > bid(new CBiostruc_id());
    bas->SetId().push_back(bid);
    bid->SetMmdb_id().Set(mmdbID);

    // create feature set and feature
    CRef < CBiostruc_feature_set > bfs(new CBiostruc_feature_set());
    bas->SetFeatures().push_back(bfs);
    bfs->SetId().Set(1);
    CRef < CBiostruc_feature > bf(new CBiostruc_feature());
    bfs->SetFeatures().push_back(bf);

    // create Chem-graph-pntrs with residues
    CChem_graph_pntrs *cgp = new CChem_graph_pntrs();
    bf->SetLocation().SetSubgraph(*cgp);
    CResidue_pntrs *rp = new CResidue_pntrs();
    cgp->SetResidues(*rp);

    // add all residue intervals
    for (h=highlights.begin(); h!=he; ++h) {
        unsigned int first = 0, last = 0;
        while (first < h->second.size()) {

            // find first highlighted residue
            while (first < h->second.size() && !h->second[first]) ++first;
            if (first >= h->second.size()) break;
            // find last in contiguous stretch of highlighted residues
            last = first;
            while (last + 1 < h->second.size() && h->second[last + 1]) ++last;

            // add new interval to list
            CRef < CResidue_interval_pntr > rip(new CResidue_interval_pntr());
            rip->SetMolecule_id().Set(h->first->moleculeID);
            rip->SetFrom().Set(first + 1);  // assume residueID == index + 1
            rip->SetTo().Set(last + 1);
            rp->SetInterval().push_back(rip);

            first = last + 2;
        }
    }

    return bas.Release();
}

bool Messenger::GetHighlightsForSelectionMessage(string *data) const
{
    data->erase();
    if (!IsAnythingHighlighted()) return false;

    CNcbiOstrstream oss;

    MoleculeHighlightMap::const_iterator h, he = highlights.end();
    for (h=highlights.begin(); h!=he; ++h) {

        // add identifier
        string id;
        if (!SeqIdToIdentifier(h->first->seqIDs.front(), id)) {
            WARNINGMSG("Messenger::GetHighlightsForSelectionMessage() - SeqIdToIdentifier() failed");
            continue;
        }
        oss << id << '\t';  // separate id from intervals with tab

        // add range(s)
        unsigned int first = 0, last = 0;
        bool firstInterval = true;
        while (first < h->second.size()) {

            // find first highlighted residue
            while (first < h->second.size() && !h->second[first]) ++first;
            if (first >= h->second.size()) break;

            // find last in contiguous stretch of highlighted residues
            last = first;
            while (last + 1 < h->second.size() && h->second[last + 1]) ++last;

            // add new interval to list (separated by spaces)
            if (!firstInterval)
                oss << ' ';
            else
                firstInterval = false;
            oss << first;
            if (last > first)
                oss << '-' << last;

            first = last + 2;
        }

        oss << '\n';
    }

    *data = (string) CNcbiOstrstreamToString(oss);
    return true;
}

void Messenger::SetAllWindowTitles(void) const
{
    SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; ++q)
        (*q)->SetWindowTitle();
    if (structureWindow) structureWindow->SetWindowTitle();
}

bool Messenger::IsFileMessengerActive(void) const
{
    return (structureWindow && structureWindow->IsFileMessengerActive());
}

void Messenger::FileMessengerSend(const std::string& toApp,
    const std::string& command, const std::string& data)
{
    if (structureWindow) structureWindow->SendCommand(toApp, command, data);
}

END_SCOPE(Cn3D)
