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
*      structure window object for Cn3D
*
* ===========================================================================
*/

#ifndef CN3D_STRUCTURE_WINDOW__HPP
#define CN3D_STRUCTURE_WINDOW__HPP

#include <corelib/ncbistd.hpp>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>

#include <string>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/html/helpctrl.h>
#include <wx/fileconf.h>

#include "multitext_dialog.hpp"
#include "file_messaging.hpp"


BEGIN_SCOPE(Cn3D)

class Cn3DGLCanvas;
class CDDAnnotateDialog;
class MultiTextDialog;
class CDDRefDialog;
class CDDBookRefDialog;
class CDDSplashDialog;
class CommandProcessor;

class StructureWindow: public wxFrame, public MultiTextDialogOwner, public ncbi::MessageResponder
{
public:
    StructureWindow(const wxString& title, const wxPoint& pos, const wxSize& size);
    ~StructureWindow();

    // public data
    Cn3DGLCanvas *glCanvas;

    // MessageResponder callbacks and setup
    void ReceivedCommand(const std::string& fromApp, unsigned long id,
        const std::string& command, const std::string& data);
    void ReceivedReply(const std::string& fromApp, unsigned long id,
		ncbi::MessageResponder::ReplyStatus status, const std::string& data);
    void SetupFileMessenger(const std::string& messageFilename,
        const std::string& messageApp, bool readOnly);
    bool IsFileMessengerActive(void) const { return (fileMessenger != NULL); }
    void SendCommand(const std::string& toApp, const std::string& command, const std::string& data);

    // public methods
    bool LoadData(const char *filename, bool force, ncbi::objects::CNcbi_mime_asn1 *mimeData = NULL);
    bool SaveDialog(bool prompt, bool canCancel);
    void SetWindowTitle(void);
    void DialogTextChanged(const MultiTextDialog *changed);
    void DialogDestroyed(const MultiTextDialog *destroyed);
    void SetPreferredFavoriteStyle(const std::string& s) { preferredFavoriteStyle = s; }

    enum {
        // File menu
            MID_OPEN,
            MID_NETWORK_OPEN,
            MID_SAVE_SAME,
            MID_SAVE_AS,
            MID_PNG,
            MID_REFIT_ALL,
            MID_PREFERENCES,
            MID_FONTS,
                MID_OPENGL_FONT,
                MID_SEQUENCE_FONT,
            MID_EXIT,
        // View menu
            MID_ZOOM_IN,
            MID_ZOOM_OUT,
            MID_RESET,
            MID_RESTORE,
            MID_NEXT_FRAME,
            MID_PREV_FRAME,
            MID_FIRST_FRAME,
            MID_LAST_FRAME,
            MID_ALL_FRAMES,
            MID_STEREO,
            MID_ANIMATE,
                MID_PLAY,
                MID_SPIN,
                MID_STOP,
                MID_ANIM_CONTROLS,
        // Show/Hide menu
            MID_SHOW_HIDE,
            MID_SHOW_ALL,
            MID_SHOW_DOMAINS,
            MID_SHOW_ALIGNED,
            MID_SHOW_UNALIGNED,
                MID_SHOW_UNALIGNED_ALL,
                MID_SHOW_UNALIGNED_ALN_DOMAIN,
            MID_SHOW_SELECTED_RESIDUES,
            MID_SHOW_SELECTED_DOMAINS,
            MID_SELECT_MOLECULE,
            MID_DIST_SELECT,
                MID_DIST_SELECT_RESIDUES,
                MID_DIST_SELECT_ALL,
                MID_DIST_SELECT_OTHER_RESIDUES,
                MID_DIST_SELECT_OTHER_ALL,
        // Structure Alignments menu
        // Style menu
            MID_EDIT_STYLE,
            MID_RENDER, // rendering shortcuts
                MID_WORM,
                MID_TUBE,
                MID_WIRE,
                MID_BNS,
                MID_SPACE,
                MID_SC_TOGGLE,
            MID_COLORS, // color shortcuts
                MID_SECSTRUC,
                MID_ALIGNED,
                MID_CONS,
                    MID_IDENTITY,
                    MID_VARIETY,
                    MID_WGHT_VAR,
                    MID_INFO,
                    MID_FIT,
                    MID_BLOCK_FIT,
                    MID_BLOCK_Z_FIT,
                    MID_BLOCK_ROW_FIT,
                MID_OBJECT,
                MID_DOMAIN,
                MID_MOLECULE,
                MID_RAINBOW,
                MID_HYDROPHOB,
                MID_CHARGE,
                MID_TEMP,
                MID_ELEMENT,
            MID_ANNOTATE,
            MID_FAVORITES, // favorites submenu
                MID_ADD_FAVORITE,
                MID_REMOVE_FAVORITE,
                MID_FAVORITES_FILE,
        // Window menu
            MID_SHOW_LOG,
            MID_SHOW_LOG_START,
            MID_SHOW_SEQ_V,
            MID_SEND_SELECTION,
        // CDD menu
            MID_CDD_OVERVIEW,
            MID_EDIT_CDD_NAME,
            MID_EDIT_CDD_DESCR,
            MID_EDIT_CDD_NOTES,
            MID_EDIT_CDD_REFERENCES,
            MID_EDIT_CDD_BOOK_REFERENCES,
            MID_ANNOT_CDD,
            MID_CDD_REJECT_SEQ,
            MID_CDD_SHOW_REJECTS,
        // Messaging menu (only when file messenger specified on command line)
            MID_MESSAGING,
        // Help menu
            MID_HELP_COMMANDS,
            MID_ONLINE_HELP,
            MID_ABOUT = wxID_ABOUT,     // special case so that this comes under Apple menu on Mac

        // not actually menu items, but used to enumerate style favorites list
            MID_FAVORITES_BEGIN = 100,
            MID_FAVORITES_END   = 149
    };

    void ShowCDDOverview(void);
    void ShowCDDAnnotations(void);
    void ShowCDDReferences(void);
    void ShowCDDBookReferences(void);

private:

    // non-modal dialogs owned by this object
    CDDAnnotateDialog *cddAnnotateDialog;
    MultiTextDialog *cddDescriptionDialog, *cddNotesDialog;
    CDDRefDialog *cddRefDialog;
    CDDBookRefDialog *cddBookRefDialog;
    CDDSplashDialog *cddOverview;
    void DestroyNonModalDialogs(void);

    void OnExit(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);
    void OnShowWindow(wxCommandEvent& event);

    void SetRenderingMenuFlag(int which);
    void SetColoringMenuFlag(int which);

    // menu-associated methods
    void OnOpen(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnPNG(wxCommandEvent& event);
    void OnAlignStructures(wxCommandEvent& event);
    void OnAdjustView(wxCommandEvent& event);
    void OnShowHide(wxCommandEvent& event);
    void OnDistanceSelect(wxCommandEvent& event);
    void OnSetStyle(wxCommandEvent& event);
    void OnEditFavorite(wxCommandEvent& event);
    void OnSelectFavorite(wxCommandEvent& event);
    void OnCDD(wxCommandEvent& event);
    void OnPreferences(wxCommandEvent& event);
    void OnSetFont(wxCommandEvent& event);
    void OnAnimate(wxCommandEvent& event);
    void OnAnimationTimer(wxTimerEvent& event);
    void OnHelp(wxCommandEvent& event);
    void OnSendSelection(wxCommandEvent& event);

    wxMenuBar *menuBar;
    wxMenu *fileMenu, *favoritesMenu, *windowMenu;
    void SetupFavoritesMenu(void);
    std::string preferredFavoriteStyle;

    wxTimer animationTimer;
    enum { ANIM_FRAMES, ANIM_SPIN };    // animation modes
    int animationMode;
    double spinIncrement;

    // help window and associated config file (for saving bookmarks, etc.)
    wxHtmlHelpController *helpController;
    wxFileConfig *helpConfig;

    // MessageResponder stuff
    ncbi::FileMessagingManager fileMessagingManager;
    wxTimer fileMessagingTimer;
    ncbi::FileMessenger *fileMessenger;
    void OnFileMessagingTimer(wxTimerEvent& event);
    std::string messageTargetApp;

    CommandProcessor *commandProcessor;

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTURE_WINDOW__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2004/08/04 18:58:30  thiessen
* add -s command line option for preferred favorite style
*
* Revision 1.13  2004/04/22 00:05:04  thiessen
* add seclect molecule
*
* Revision 1.12  2004/02/19 17:05:17  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.11  2004/01/17 01:47:27  thiessen
* add network load
*
* Revision 1.10  2004/01/17 00:17:32  thiessen
* add Biostruc and network structure load
*
* Revision 1.9  2004/01/08 15:31:03  thiessen
* remove hard-coded CDTree references in messaging; add Cn3DTerminated message upon exit
*
* Revision 1.8  2003/12/03 15:07:10  thiessen
* add more sophisticated animation controls
*
* Revision 1.7  2003/11/15 16:08:37  thiessen
* add stereo
*
* Revision 1.6  2003/09/26 17:12:46  thiessen
* add book reference dialog
*
* Revision 1.5  2003/07/17 18:47:01  thiessen
* add -f option to force save to same file
*
* Revision 1.4  2003/07/10 18:47:29  thiessen
* add CDTree->Select command
*
* Revision 1.3  2003/07/10 13:47:22  thiessen
* add LoadFile command
*
* Revision 1.2  2003/03/14 19:22:59  thiessen
* add CommandProcessor to handle file-message commands; fixes for GCC 2.9
*
* Revision 1.1  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
*/
