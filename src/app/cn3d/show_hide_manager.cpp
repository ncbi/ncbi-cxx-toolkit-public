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
*      manager object to track show/hide status of objects at various levels
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2001/03/09 15:49:05  thiessen
* major changes to add initial update viewer
*
* Revision 1.8  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.7  2001/01/25 20:21:18  thiessen
* fix ostrstream memory leaks
*
* Revision 1.6  2000/12/29 19:23:39  thiessen
* save row order
*
* Revision 1.5  2000/12/19 16:39:09  thiessen
* tweaks to show/hide
*
* Revision 1.4  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.3  2000/12/01 19:35:57  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.2  2000/08/17 18:33:12  thiessen
* minor fixes to StyleManager
*
* Revision 1.1  2000/08/03 15:13:59  thiessen
* add skeleton of style and show/hide managers
*
* ===========================================================================
*/

#include "cn3d/show_hide_manager.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/residue.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/opengl_renderer.hpp"

#include <corelib/ncbistre.hpp>
#include <vector>

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)


// ShowHideInfo is a generic container class to help organize the list of things that can be
// shown/hidden; stuff derived from it allows various types of objects to be shown/hidden

std::string indent("     ");

class ShowHideInfo
{
protected:
    std::string label;
public:
    std::vector < int > parentIndexes;
    void GetLabel(std::string *str) const { *str = label; }
    virtual bool IsVisible(const ShowHideManager *shm) const = 0;
    virtual void Show(ShowHideManager *shm, bool isShown) const = 0;
};

class ShowHideObject : public ShowHideInfo
{
private:
    const StructureObject *object;
public:
    ShowHideObject(const StructureObject *o) : object(o) { label = o->pdbID; }
    bool IsVisible(const ShowHideManager *shm) const { return shm->IsVisible(object); }
    void Show(ShowHideManager *shm, bool isShown) const { shm->Show(object, isShown); }
};

class ShowHideMolecule : public ShowHideInfo
{
private:
    const Molecule *molecule;
public:
    ShowHideMolecule(const Molecule *m) : molecule(m)
    {
        label = indent + m->pdbID;
        if (m->pdbChain != ' ') {
            label += '_';
            label += (char) m->pdbChain;
        }
    }
    bool IsVisible(const ShowHideManager *shm) const { return shm->IsVisible(molecule); }
    void Show(ShowHideManager *shm, bool isShown) const { shm->Show(molecule, isShown); }
};

class ShowHideDomain : public ShowHideInfo
{
private:
    const Molecule *molecule;
    int domainID;
public:
    ShowHideDomain(const Molecule *m, int d, int labelNum) : molecule(m), domainID(d)
    {
        CNcbiOstrstream oss;
        oss << indent << indent << m->pdbID;
        if (m->pdbChain != ' ') oss << '_' << (char) m->pdbChain;
        oss << " d" << labelNum << '\0';
        label = oss.str();
        delete oss.str();
    }
    bool IsVisible(const ShowHideManager *shm) const
    {
        bool isVisible = false;
        for (int i=0; i<molecule->NResidues(); i++) {
            if (molecule->residueDomains[i] == domainID &&
                shm->IsVisible(molecule->residues.find(i+1)->second)) {
                isVisible = true;   // return true if any residue from this domain is visible
                break;
            }
        }
        return isVisible;
    }
    void Show(ShowHideManager *shm, bool isShown) const
    {
        for (int i=0; i<molecule->NResidues(); i++)
            if (molecule->residueDomains[i] == domainID)
                shm->Show(molecule->residues.find(i+1)->second, isShown);
    }
};


///// the ShowHideManager class /////

ShowHideManager::~ShowHideManager()
{
    for (int i=0; i<structureInfo.size(); i++) delete structureInfo[i];
}

static void PostRedrawEntity(const StructureObject *object, const Molecule *molecule, const Residue *residue)
{
    if (residue) {
        const Molecule *m;
        if (residue->GetParentOfType(&m))
            GlobalMessenger()->PostRedrawMolecule(m);
    }

    else if (molecule) {
        GlobalMessenger()->PostRedrawMolecule(molecule);
    }

    else if (object) {
        // redraw all prot/nuc molecules
        ChemicalGraph::MoleculeMap::const_iterator m, me = object->graph->molecules.end();
        for (m=object->graph->molecules.begin(); m!=me; m++) {
            if (m->second->IsProtein() || m->second->IsNucleotide())
                GlobalMessenger()->PostRedrawMolecule(m->second);
        }
    }

    GlobalMessenger()->PostRedrawAllSequenceViewers();
}

void ShowHideManager::Show(const StructureBase *entity, bool isShown)
{
    // make sure this is a valid entity
    const StructureObject *object = dynamic_cast<const StructureObject *>(entity);
    const Molecule *molecule = dynamic_cast<const Molecule *>(entity);
    const Residue *residue = dynamic_cast<const Residue *>(entity);
    if (!entity || !(object || molecule || residue)) {
        ERR_POST(Error << "ShowHideManager::Show() - must be a StructureObject, Molecule, or Residue");
        return;
    }

    EntitiesHidden::iterator e = entitiesHidden.find(entity);

    // hide an entity that's not already hidden
    if (!isShown && e == entitiesHidden.end()) {
        entitiesHidden[entity] = true;
        PostRedrawEntity(object, molecule, residue);
        entity->parentSet->renderer->ShowAllFrames();
    }

    // show an entity that's currently hidden
    else if (isShown && e != entitiesHidden.end()) {
        entitiesHidden.erase(e);
        PostRedrawEntity(object, molecule, residue);
        entity->parentSet->renderer->ShowAllFrames();
    }
}

bool ShowHideManager::IsHidden(const StructureBase *entity) const
{
    if (entitiesHidden.size() == 0) return false;

    const StructureObject *object;
    const Molecule *molecule;
    const Residue *residue;

    EntitiesHidden::const_iterator e = entitiesHidden.find(entity);

    if ((object = dynamic_cast<const StructureObject *>(entity)) != NULL) {
        return (e != entitiesHidden.end());
    }

    else if ((molecule = dynamic_cast<const Molecule *>(entity)) != NULL) {
        if (!molecule->GetParentOfType(&object)) return false;
        return (entitiesHidden.find(object) != entitiesHidden.end() ||
                e != entitiesHidden.end());
    }

    else if ((residue = dynamic_cast<const Residue *>(entity)) != NULL) {
        if (!residue->GetParentOfType(&molecule) ||
            !molecule->GetParentOfType(&object)) return false;
        return (entitiesHidden.find(object) != entitiesHidden.end() ||
                entitiesHidden.find(molecule) != entitiesHidden.end() ||
                e != entitiesHidden.end());
    }

    ERR_POST(Error << "ShowHideManager::IsHidden() - must be a StructureObject, Molecule, or Residue");
    return false;
}

void ShowHideManager::ConstructShowHideArray(const StructureSet *structureSet)
{
    ShowHideInfo *info;
    int objectIndex, moleculeIndex;

    StructureSet::ObjectList::const_iterator o, oe = structureSet->objects.end();
    for (o=structureSet->objects.begin(); o!=oe; o++) {

        objectIndex = structureInfo.size();
        structureInfo.push_back(new ShowHideObject(*o));

        // list interesting (prot/nuc) chains
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; m++) {
            int nDom = 1; // # domains in this chain

            if (m->second->IsProtein() || m->second->IsNucleotide()) {
                moleculeIndex = structureInfo.size();
                info = new ShowHideMolecule(m->second);
                info->parentIndexes.push_back(objectIndex);
                structureInfo.push_back(info);

                // if there is more than one domain, enumerate them
                if (m->second->nDomains > 1) {
                    StructureObject::DomainMap::const_iterator d, de = (*o)->domainMap.end();
                    for (d=(*o)->domainMap.begin(); d!=de; d++) {
                        if (d->second == m->second) {
                            info = new ShowHideDomain(m->second, d->first, nDom++);
                            info->parentIndexes.push_back(objectIndex);
                            info->parentIndexes.push_back(moleculeIndex);
                            structureInfo.push_back(info);
                        }
                    }
                }
            }
        }
    }
}

void ShowHideManager::GetShowHideInfo(
    std::vector < std::string > *names, std::vector < bool > *visibilities) const
{
    names->resize(structureInfo.size());
    visibilities->resize(structureInfo.size());
    for (int i=0; i<structureInfo.size(); i++) {
        structureInfo[i]->GetLabel(&(names->at(i)));
        visibilities->at(i) = structureInfo[i]->IsVisible(this);
    }
}

void ShowHideManager::ShowHideCallbackFunction(const std::vector < bool >& itemsEnabled)
{
    if (itemsEnabled.size() != structureInfo.size()) {
        ERR_POST(Error << "ShowHideManager::ShowHideCallbackFunction() - wrong size list");
        return;
    }

    for (int i=0; i<itemsEnabled.size(); i++)
        structureInfo[i]->Show(this, itemsEnabled[i]);
    TESTMSG("entities hidden: " << entitiesHidden.size());
}

void ShowHideManager::SelectionChangedCallback(
    const std::vector < bool >& original, std::vector < bool >& itemsEnabled)
{
    // count number of changes
    int i, nChanges = 0, itemChanged, nEnabled = 0, itemEnabled;
    for (i=0; i<itemsEnabled.size(); i++) {
        if (itemsEnabled[i] != original[i]) {
            nChanges++;
            itemChanged = i;
        }
        if (itemsEnabled[i]) {
            nEnabled++;
            itemEnabled = i;
        }
    }

    // if change was a single de/selection, then turn off/on the children of that item
    if (nChanges == 1 || nEnabled == 1) {
        int item = (nChanges == 1) ? itemChanged : itemEnabled;
        for (i=item+1; i<structureInfo.size(); i++) {
            for (int j=0; j<structureInfo[i]->parentIndexes.size(); j++) {
                if (structureInfo[i]->parentIndexes[j] == item)
                    itemsEnabled[i] = itemsEnabled[item];
            }
        }
    }

    // check all items to make sure that when an object is on, its parents are also on
    for (i=0; i<itemsEnabled.size(); i++) {
        if (itemsEnabled[i]) {
            for (int j=0; j<structureInfo[i]->parentIndexes.size(); j++) {
                itemsEnabled[structureInfo[i]->parentIndexes[j]] = true;
            }
        }
    }
}

void ShowHideManager::ShowObject(const StructureObject *object, bool isShown)
{
    // hide via normal mechanism
    if (!isShown) {
        Show(object, false);
    }

    // but if to be shown and isn't already visible, then unhide any children of this object
    else if (IsHidden(object)) {
        EntitiesHidden::iterator e, ee = entitiesHidden.end();
        for (e=entitiesHidden.begin(); e!=ee; ) {
            const StructureObject *eObj = dynamic_cast<const StructureObject *>(e->first);
            if (!eObj) e->first->GetParentOfType(&eObj);
            if (eObj == object) {
                EntitiesHidden::iterator d(e);
                e++;
                entitiesHidden.erase(d);
            } else {
                e++;
            }
        }
        PostRedrawEntity(object, NULL, NULL);
        object->parentSet->renderer->ShowAllFrames();
    }
}

void ShowHideManager::MakeAllVisible(void)
{
    while(entitiesHidden.size() > 0) Show(entitiesHidden.begin()->first, true);
}

void ShowHideManager::ShowAlignedDomains(const StructureSet *set)
{
    MakeAllVisible();
    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    for (o=set->objects.begin(); o!=oe; o++) {
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; m++) {

            if (!m->second->IsProtein()) continue;  // leave all non-protein visible
            if (!set->alignmentManager->IsInAlignment(m->second->sequence)) {
                Show(m->second, false);
                continue;
            }

            std::map < int, bool > domains;
            Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();

            // first pass determins which domains have any aligned residues
            for (r=m->second->residues.begin(); r!=re; r++)
                if (set->alignmentManager->IsAligned(m->second->sequence, r->first - 1))
                    domains[m->second->residueDomains[r->first - 1]] = true;

            // second pass does hides domains not represented
            for (r=m->second->residues.begin(); r!=re; r++)
                if (domains.find(m->second->residueDomains[r->first - 1]) == domains.end())
                    Show(r->second, false);
        }
    }
}

void ShowHideManager::ShowResidues(const StructureSet *set, bool showAligned)
{
    MakeAllVisible();
    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    for (o=set->objects.begin(); o!=oe; o++) {
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; m++) {
            if (!m->second->IsProtein()) continue;  // leave all non-protein visible

            if (!set->alignmentManager->IsInAlignment(m->second->sequence)) {
                if (showAligned) Show(m->second, false);
                continue;
            }

            Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();
            for (r=m->second->residues.begin(); r!=re; r++) {
                bool aligned = set->alignmentManager->IsAligned(m->second->sequence, r->first - 1);
                if ((showAligned && !aligned) || (!showAligned && aligned))
                    Show(r->second, false);
            }
        }
    }
}

void ShowHideManager::ShowSelectedResidues(const StructureSet *set)
{
    MakeAllVisible();
    if (!GlobalMessenger()->IsAnythingHighlighted()) return;

    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    for (o=set->objects.begin(); o!=oe; o++) {
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; m++) {
            Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();
            for (r=m->second->residues.begin(); r!=re; r++) {
                if (!GlobalMessenger()->IsHighlighted(m->second, r->first))
                    Show(r->second, false);
            }
        }
    }
}

END_SCOPE(Cn3D)
