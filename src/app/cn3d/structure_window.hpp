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
#include <objects/mmdb2/Model_type.hpp>

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
class OpenGLRenderer;
class StructureSet;

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
    bool LoadData(const char *filename, bool force, bool noAlignmentWindow, ncbi::objects::CNcbi_mime_asn1 *mimeData = NULL);
    bool SaveDialog(bool prompt, bool canCancel);
    void SetWindowTitle(void);
    void DialogTextChanged(const MultiTextDialog *changed);
    void DialogDestroyed(const MultiTextDialog *destroyed);
    void SetPreferredFavoriteStyle(const std::string& s) { preferredFavoriteStyle = s; }

    enum {
        // File menu
            MID_OPEN = 1,    //  wxMac doesn't like MID_OPEN = 0 on Mac OSX 10.4 and higher
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
            MID_SHOW_CHAINS,
            MID_SHOW_UNALIGNED,
                MID_SHOW_UNALIGNED_ALL,
                MID_SHOW_UNALIGNED_ALN_DOMAIN,
            MID_SHOW_SELECTED_RESIDUES,
            MID_SHOW_SELECTED_DOMAINS,
            MID_DIST_SELECT,
            MID_SELECT_CHAIN,
            MID_SELECT_MOLECULE,
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
                MID_RESIDUE,
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

    void SetRenderingMenuFlag(int which);
    void SetColoringMenuFlag(int which);

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

    // menu-associated methods
    void OnOpen(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnPNG(wxCommandEvent& event);
    void OnAlignStructures(wxCommandEvent& event);
    void OnAdjustView(wxCommandEvent& event);
    void OnShowHide(wxCommandEvent& event);
    void OnSelect(wxCommandEvent& event);
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

bool LoadDataOnly(StructureSet **sset, OpenGLRenderer *renderer, const char *filename,  
    ncbi::objects::CNcbi_mime_asn1 *mimeData = NULL, const std::string& favoriteStyle = kEmptyStr, 
    ncbi::objects::EModel_type model = ncbi::objects::eModel_type_ncbi_all_atom, StructureWindow *window = NULL);

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTURE_WINDOW__HPP
