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
*      Miscellaneous utility functions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2002/04/09 23:59:10  thiessen
* add cdd annotations read-only option
*
* Revision 1.15  2002/03/04 15:52:13  thiessen
* hide sequence windows instead of destroying ; add perspective/orthographic projection choice
*
* Revision 1.14  2001/12/20 21:41:46  thiessen
* create/use user preferences directory
*
* Revision 1.13  2001/11/27 16:26:07  thiessen
* major update to data management system
*
* Revision 1.12  2001/10/30 02:54:12  thiessen
* add Biostruc cache
*
* Revision 1.11  2001/09/06 21:38:33  thiessen
* tweak message log / diagnostic system
*
* Revision 1.10  2001/08/31 22:24:14  thiessen
* add timer for animation
*
* Revision 1.9  2001/08/24 18:53:13  thiessen
* add filename to sequence viewer window titles
*
* Revision 1.8  2001/08/16 19:21:16  thiessen
* add face name info to fonts
*
* Revision 1.7  2001/08/14 17:17:48  thiessen
* add user font selection, store in registry
*
* Revision 1.6  2001/08/13 22:30:51  thiessen
* add structure window mouse drag/zoom; add highlight option to render settings
*
* Revision 1.5  2001/08/06 20:22:48  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.4  2001/08/03 13:41:24  thiessen
* add registry and style favorites
*
* Revision 1.3  2001/07/19 19:12:46  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.2  2001/05/31 18:46:26  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.1  2001/05/15 14:57:48  thiessen
* add cn3d_tools; bring up log window when threading starts
*
* ===========================================================================
*/

#ifndef CN3D_TOOLS__HPP
#define CN3D_TOOLS__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbireg.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <string>

class wxFrame;


BEGIN_SCOPE(Cn3D)

// strings for various directories - dirs will include trailing path separator character
// (implemented in cn3d_main_wxwin.cpp)
extern const std::string& GetWorkingDir(void);  // current working directory
extern const std::string& GetUserDir(void);     // directory of latest user-selected file
extern const std::string& GetProgramDir(void);  // directory where Cn3D executable lives
extern const std::string& GetDataDir(void);     // 'data' directory with external data files
extern const std::string& GetWorkingFilename(void); // name of current working file
extern const std::string& GetPrefsDir(void);    // application preferences directory

// bring the log window forward (implemented in cn3d_main_wxwin.cpp)
extern void RaiseLogWindow(void);

// launch web browser on given URL (implemented in sequence_set.cpp)
extern void LaunchWebPage(const char *url);

// top-level window (the main structure window) (implemented in cn3d_main_wxwin.cpp)
extern wxFrame * GlobalTopWindow(void);

// diagnostic output (mainly for debugging)
#define TESTMSG(stream) ERR_POST(Info << stream)
//#define TESTMSG(stream)

// global program registry (cn3d.ini) (implemented in cn3d_main_wxwin.cpp)
extern bool RegistryIsValidInteger(const std::string& section, const std::string& name);
extern bool RegistryIsValidBoolean(const std::string& section, const std::string& name);
extern bool RegistryIsValidString(const std::string& section, const std::string& name);
extern bool RegistryGetInteger(const std::string& section, const std::string& name, int *value);
extern bool RegistryGetBoolean(const std::string& section, const std::string& name, bool *value);
extern bool RegistryGetString(const std::string& section, const std::string& name, std::string *value);
extern bool RegistrySetInteger(const std::string& section, const std::string& name, int value);
extern bool RegistrySetBoolean(const std::string& section, const std::string& name, bool value,
    bool useYesOrNo = false);
extern bool RegistrySetString(const std::string& section, const std::string& name,
    const std::string& value);

// registry section/entry names
static const std::string
    // configuration
    REG_CONFIG_SECTION = "Cn3D-4-Config",
    REG_FAVORITES_NAME = "Favorites",
    REG_ANIMATION_DELAY = "AnimationDelay",
    REG_SHOW_LOG_ON_START = "ShowLogOnStartup",
    // quality settings
    REG_QUALITY_SECTION = "Cn3D-4-Quality",
    REG_QUALITY_ATOM_SLICES = "AtomSlices",
    REG_QUALITY_ATOM_STACKS = "AtomStacks",
    REG_QUALITY_BOND_SIDES = "BondSides",
    REG_QUALITY_WORM_SIDES = "WormSides",
    REG_QUALITY_WORM_SEGMENTS = "WormSegments",
    REG_QUALITY_HELIX_SIDES = "HelixSides",
    REG_HIGHLIGHTS_ON = "HighlightsOn",
    REG_PROJECTION_TYPE = "ProjectionType",
    // font settings
    REG_OPENGL_FONT_SECTION = "Cn3D-4-Font-OpenGL",
    REG_SEQUENCE_FONT_SECTION = "Cn3D-4-Font-Sequence",
    REG_FONT_SIZE = "FontPointSize",
    REG_FONT_FAMILY = "FontFamily",
    REG_FONT_STYLE = "FontStyle",
    REG_FONT_WEIGHT = "FontWeight",
    REG_FONT_UNDERLINED = "FontUnderlined",
    REG_FONT_FACENAME = "FontFaceName",
    FONT_FACENAME_UNKNOWN = "unknown",
    // cache settings
    REG_CACHE_SECTION = "Cn3D-4-Cache",
    REG_CACHE_ENABLED = "CacheEnabled",
    REG_CACHE_FOLDER = "CacheFolder",
    REG_CACHE_MAX_SIZE = "CacheSizeMax",
    // advanced options
    REG_ADVANCED_SECTION = "Cn3D-4-Advanced",
    REG_CDD_ANNOT_READONLY = "CDDAnnotationsReadOnly";

END_SCOPE(Cn3D)

#endif // CN3D_TOOLS__HPP
