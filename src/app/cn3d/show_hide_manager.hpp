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
* Revision 1.5  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.4  2000/12/01 19:34:43  thiessen
* better domain assignment
*
* Revision 1.3  2000/08/17 18:32:37  thiessen
* minor fixes to StyleManager
*
* Revision 1.2  2000/08/17 14:22:01  thiessen
* added working StyleManager
*
* Revision 1.1  2000/08/03 15:14:18  thiessen
* add skeleton of style and show/hide managers
*
* ===========================================================================
*/

#ifndef CN3D_SHOW_HIDE_MANAGER__HPP
#define CN3D_SHOW_HIDE_MANAGER__HPP

#include <corelib/ncbistl.hpp>

#include <vector>
#include <map>

#include "cn3d/structure_base.hpp"
#include "cn3d/show_hide_callback.hpp"


BEGIN_SCOPE(Cn3D)

class Residue;
class StructureSet;
class ShowHideInfo;

class ShowHideManager : public ShowHideCallback
{
public:
    ~ShowHideManager();

    // eventually this will be tied to a GUI element or something...
    bool OverlayConfEnsembles(void) const { return true; }

    // set show/hide status of an entity - must be StructureObject, Molecule, or Residue.
    void Show(const StructureBase *entity, bool isShown);

    void MakeAllVisible(void);

    // query whether an entity is visible
    bool IsHidden(const StructureBase *entity) const;
    bool IsVisible(const StructureBase *entity) const { return !IsHidden(entity); }

    // used for show/hide dialog to get list of names and visibility status
    void GetShowHideInfo(std::vector < std::string > *names, std::vector < bool > *visibilities) const;
    void SelectionCallback(const std::vector < bool >& itemsEnabled);
    void SelectionChangedCallback(const std::vector < bool >& original, std::vector < bool >& itemsEnabled);
    void ConstructShowHideArray(const StructureSet *structureSet);

    // functions to show/hide more complex groups of stuff
    void ShowAlignedDomains(const StructureSet *set);
    void ShowResidues(const StructureSet *set, bool showAligned);
    void ShowSelectedResidues(const StructureSet *set);

private:
    typedef std::map < const StructureBase *, bool > EntitiesHidden;
    EntitiesHidden entitiesHidden;

    // holds info on objects in this StructureSet
    std::vector < const ShowHideInfo * > structureInfo;

public:
};

END_SCOPE(Cn3D)

#endif // CN3D_SHOW_HIDE_MANAGER__HPP
