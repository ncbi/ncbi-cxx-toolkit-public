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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/09/11 01:46:14  thiessen
* working messenger for sequence<->structure window communication
*
* ===========================================================================
*/

#include <wx/string.h> // name conflict kludge

#include "cn3d/messenger.hpp"
#include "cn3d/sequence_viewer.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/chemical_graph.hpp"

#include "cn3d/cn3d_main_wxwin.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

void Messenger::PostRedrawAllStructures(void)
{
    redrawAllStructures = true;
    redrawMolecules.empty();
}

void Messenger::PostRedrawMolecule(const StructureObject *object, int moleculeID)
{
    if (!redrawAllStructures) {
        MoleculeIdentifier mid = std::make_pair(object, moleculeID);
        RedrawMoleculeList::const_iterator m, me = redrawMolecules.end();
        for (m=redrawMolecules.begin(); m!=me; m++)
            if (*m == mid) break;
        if (m == me) redrawMolecules.push_back(mid);
    }
}

void Messenger::PostRedrawSequenceViewers(void)
{
    redrawSequenceViewers = true;
}

void Messenger::ProcessRedraws(void)
{
    if (redrawSequenceViewers) {
        SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
        for (q=sequenceViewers.begin(); q!=qe; q++)
            (*q)->Refresh();
        redrawSequenceViewers = false;
    }

    if (redrawAllStructures) {
        StructureWindowList::const_iterator t, te = structureWindows.end();
        for (t=structureWindows.begin(); t!=te; t++) {
            (*t)->glCanvas->renderer.Construct();
            (*t)->glCanvas->Refresh(false);
        }
        redrawAllStructures = false;
    }

    else if (redrawMolecules.size() > 0) {
        RedrawMoleculeList::const_iterator m, me = redrawMolecules.end();
        for (m=redrawMolecules.begin(); m!=me; m++)
            m->first->graph->RedrawMolecule(m->second);
        redrawMolecules.clear();

        StructureWindowList::const_iterator t, te = structureWindows.end();
        for (t=structureWindows.begin(); t!=te; t++)
            (*t)->glCanvas->Refresh(false);
    }
}

void Messenger::RemoveStructureWindow(const Cn3DMainFrame *structureWindow)
{
    StructureWindowList::iterator t, te = structureWindows.end();
    for (t=structureWindows.begin(); t!=te; t++) {
        if (*t == structureWindow) structureWindows.erase(t);
        break;
    }

    if (structureWindows.size() == 0) {
        SequenceViewerList::iterator q, qe = sequenceViewers.end();
        for (q=sequenceViewers.begin(); q!=qe; q++)
            delete *q;
        sequenceViewers.empty();
    }
}

void Messenger::DisplaySequences(const SequenceList *sequences)
{
    SequenceViewerList::iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; q++)
        (*q)->DisplaySequences(sequences);
}
   
void Messenger::DisplayAlignment(const BlockMultipleAlignment *alignment)
{
    SequenceViewerList::iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; q++)
        (*q)->DisplayAlignment(alignment);
}

void Messenger::ClearSequenceViewers(void)
{
    SequenceViewerList::iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; q++)
        (*q)->ClearGUI();
}

END_SCOPE(Cn3D)

