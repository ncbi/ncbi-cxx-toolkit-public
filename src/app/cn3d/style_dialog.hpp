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
* ===========================================================================
*/

#ifndef CN3D_STYLE_DIALOG__HPP
#define CN3D_STYLE_DIALOG__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/spinbutt.h>

#include <map>
#include <string>

#include "style_manager.hpp"
#include "cn3d_tools.hpp"
#include <algo/structure/wx_tools/wx_tools.hpp>


BEGIN_SCOPE(Cn3D)

class StructureSet;

class StyleDialog : public wxDialog
{
public:
    StyleDialog(wxWindow* parent, StyleSettings *settingsToEdit, const StructureSet *set);
    ~StyleDialog(void);

    // set the StyleSettings from values in the panel; returns true if all values are valid
    bool GetValues(StyleSettings *settings);

    // set the controls according to the given StyleSettings
    bool SetControls(const StyleSettings& settings);

private:

    StyleSettings *editedSettings;
    const StyleSettings originalSettings;
    const StructureSet *structureSet;
    bool changedSinceApply, changedEver, initialized;

    ncbi::FloatingPointSpinCtrl *fpSpaceFill, *fpBallRadius, *fpStickRadius, *fpTubeRadius,
        *fpTubeWormRadius, *fpHelixRadius, *fpStrandWidth, *fpStrandThickness;

    bool HandleColorButton(int bID);

    static TypeStringAssociator < StyleSettings::eBackboneType > BackboneTypeStrings;
    static TypeStringAssociator < StyleSettings::eDrawingStyle > DrawingStyleStrings;
    static TypeStringAssociator < StyleSettings::eColorScheme > ColorSchemeStrings;
    static TypeStringAssociator < StyleSettings::eLabelType > LabelTypeStrings;
    static TypeStringAssociator < StyleSettings::eNumberType > NumberTypeStrings;

    static void SetupStyleStrings(void);
    bool GetBackboneStyle(StyleSettings::BackboneStyle *bbStyle,
        int showID, int renderID, int colorID, int userID);
    bool SetBackboneStyle(const StyleSettings::BackboneStyle& bbStyle,
        int showID, int renderID, int colorID, int userID);
    bool GetGeneralStyle(StyleSettings::GeneralStyle *gStyle,
        int showID, int renderID, int colorID, int userID);
    bool SetGeneralStyle(const StyleSettings::GeneralStyle& gStyle,
        int showID, int renderID, int colorID, int userID);
    bool GetLabelStyle(StyleSettings::LabelStyle *lStyle,
        int spacingID, int typeID, int numberingID, int terminiID, int whiteID);
    bool SetLabelStyle(const StyleSettings::LabelStyle& lStyle,
        int spacingID, int typeID, int numberingID, int terminiID, int whiteID);

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnChange(wxCommandEvent& event);
    void OnSpin(wxSpinEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_STYLE_DIALOG__HPP
