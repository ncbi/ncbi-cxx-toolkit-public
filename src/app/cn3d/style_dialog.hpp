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
*      dialog for setting styles
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2001/08/06 20:22:48  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.3  2001/06/08 14:46:47  thiessen
* fully functional (modal) render settings panel
*
* Revision 1.2  2001/06/07 19:04:50  thiessen
* functional (although incomplete) render settings panel
*
* Revision 1.1  2001/05/31 18:46:28  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* ===========================================================================
*/

#ifndef CN3D_STYLE_DIALOG__HPP
#define CN3D_STYLE_DIALOG__HPP

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

#include <wx/wx.h>

#include <map>
#include <string>

#include "cn3d/style_manager.hpp"


BEGIN_SCOPE(Cn3D)

class StyleSettings;
class StructureSet;
class FloatingPointSpinCtrl;

template < class T >
class TypeStringAssociator
{
private:
    std::map < T , std::string > type2string;
    std::map < std::string , T > string2type;
public:
    void Associate(const T& type, const std::string& name)
    {
        type2string[type] = name;
        string2type[name] = type;
    }
    const T * Find(const std::string& name) const
    {
        std::map < std::string , T >::const_iterator i = string2type.find(name);
        return ((i != string2type.end()) ? &(i->second) : NULL);
    }
    bool Get(const std::string& name, T *type) const
    {
        const T *found = Find(name);
        if (found) *type = *found;
        return (found != NULL);
    }
    const std::string * Find(const T& type) const
    {
        std::map < T , std::string >::const_iterator i = type2string.find(type);
        return ((i != type2string.end()) ? &(i->second) : NULL);
    }
    bool Get(const T& type, std::string *name) const
    {
        const std::string *found = Find(type);
        if (found) *name = *found;
        return (found != NULL);
    }
    int Size(void) const { return type2string.size(); }
};

class StyleDialog : public wxDialog
{
public:
    StyleDialog(wxWindow* parent, StyleSettings *settingsToEdit, const StructureSet *set);

    // set the StyleSettings from values in the panel; returns true if all values are valid
    bool GetValues(StyleSettings *settings);

    // set the controls according to the given StyleSettings
    bool SetControls(const StyleSettings& settings);

private:

    StyleSettings *editedSettings;
    const StyleSettings originalSettings;
    const StructureSet *structureSet;
    bool changedSinceApply, changedEver;

    FloatingPointSpinCtrl *fpSpaceFill, *fpBallRadius, *fpStickRadius, *fpTubeRadius,
        *fpTubeWormRadius, *fpHelixRadius, *fpStrandWidth, *fpStrandThickness;

    bool HandleColorButton(int bID);

    static TypeStringAssociator < StyleSettings::eBackboneType > BackboneTypeStrings;
    static TypeStringAssociator < StyleSettings::eDrawingStyle > DrawingStyleStrings;
    static TypeStringAssociator < StyleSettings::eColorScheme > ColorSchemeStrings;

    static void SetupStyleStrings(void);
    bool GetBackboneStyle(StyleSettings::BackboneStyle *bbStyle,
        int showID, int renderID, int colorID, int userID);
    bool SetBackboneStyle(const StyleSettings::BackboneStyle& bbStyle,
        int showID, int renderID, int colorID, int userID);
    bool GetGeneralStyle(StyleSettings::GeneralStyle *gStyle,
        int showID, int renderID, int colorID, int userID);
    bool SetGeneralStyle(const StyleSettings::GeneralStyle& gStyle,
        int showID, int renderID, int colorID, int userID);

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnChange(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_STYLE_DIALOG__HPP
