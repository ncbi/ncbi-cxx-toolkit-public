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
* ===========================================================================
*/

#ifndef CN3D_TOOLS__HPP
#define CN3D_TOOLS__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbidiag.hpp>

#include <objects/cn3d/Cn3d_style_settings_set.hpp>
#include <objects/seq/Bioseq.hpp>

#include <string>
#include <vector>
#include <map>

class wxFrame;


/////
///// Check platform and wx option compatibility
/////
#include <wx/platform.h>
#include <wx/version.h>

#if !wxCHECK_VERSION(2,3,2)
#error Cn3D requires wxWindows version 2.3.2 or higher!
#endif

#if !defined(__WXMSW__) && !defined(__WXGTK__) && !defined(__WXMAC__)
#error Cn3D will only work with wxMSW, wxGTK, or wxMac!
#endif

#if defined(__WXMAC__) && !defined(__DARWIN__)
#error Cn3D compilation is no longer supported for Mac OS 8/9 (classic or Carbon)
#endif

#if !wxUSE_GLCANVAS
#error Please set wxUSE_GLCANVAS to 1 in <wx/setup.h>
#endif

#if !wxUSE_STREAMS || !wxUSE_ZIPSTREAM || !wxUSE_ZLIB
#error Must turn on wxUSE_STREAMS && wxUSE_ZIPSTREAM && wxUSE_ZLIB for the Cn3D help system!
#endif


BEGIN_SCOPE(Cn3D)

// program version number string
#define CN3D_VERSION_STRING "4.2"

// diagnostic streams
#define TRACEMSG(stream) ERR_POST(ncbi::Trace << stream)
#define INFOMSG(stream) ERR_POST(ncbi::Info << stream)
#define WARNINGMSG(stream) ERR_POST(ncbi::Warning << stream)
#define ERRORMSG(stream) ERR_POST(ncbi::Error << stream)
#define FATALMSG(stream) ERR_POST(ncbi::Fatal << stream)

// turn on/off dialog box for errors (on by default)
void SetDialogSevereErrors(bool status);

// strings for various directories - dirs will include trailing path separator character
extern const std::string& GetWorkingDir(void);  // current working directory
extern const std::string& GetUserDir(void);     // directory of latest user-selected file
extern const std::string& GetProgramDir(void);  // directory where Cn3D executable lives
extern const std::string& GetDataDir(void);     // 'data' directory with external data files
extern const std::string& GetWorkingFilename(void); // name of current working file
extern const std::string& GetPrefsDir(void);    // application preferences directory

// get working document title; bring the log window forward
extern const std::string& GetWorkingTitle(void);
extern void RaiseLogWindow(void);

// launch web browser on given URL
extern void LaunchWebPage(const char *url);

// retrieve a sequence via network; uid can be gi or accession
ncbi::CRef < ncbi::objects::CBioseq > FetchSequenceViaHTTP(const std::string& id);

// top-level window (the main structure window)
extern wxFrame * GlobalTopWindow(void);

// gives NCBIStdaa residue number for a character (or value for 'X' if char not found)
extern unsigned char LookupNCBIStdaaNumberFromCharacter(char r);
extern char LookupCharacterFromNCBIStdaaNumber(unsigned char n);

// return BLOSUM62 score for two residues (by character)
extern int GetBLOSUM62Score(char a, char b);

// standard probability for a residue (by character)
extern double GetStandardProbability(char ch);

// global program registry manipulation
extern void LoadRegistry(void);
extern void SaveRegistry(void);
extern bool RegistryIsValidInteger(const std::string& section, const std::string& name);
extern bool RegistryIsValidDouble(const std::string& section, const std::string& name);
extern bool RegistryIsValidBoolean(const std::string& section, const std::string& name);
extern bool RegistryIsValidString(const std::string& section, const std::string& name);
extern bool RegistryGetInteger(const std::string& section, const std::string& name, int *value);
extern bool RegistryGetDouble(const std::string& section, const std::string& name, double *value);
extern bool RegistryGetBoolean(const std::string& section, const std::string& name, bool *value);
extern bool RegistryGetString(const std::string& section, const std::string& name, std::string *value);
extern bool RegistrySetInteger(const std::string& section, const std::string& name, int value);
extern bool RegistrySetDouble(const std::string& section, const std::string& name, double value);
extern bool RegistrySetBoolean(const std::string& section, const std::string& name, bool value,
    bool useYesOrNo = false);
extern bool RegistrySetString(const std::string& section, const std::string& name,
    const std::string& value);

// registry section/entry names
static const std::string
    // configuration
    REG_CONFIG_SECTION = "Cn3D-4-Config",
    REG_FAVORITES_NAME = "Favorites",
        NO_FAVORITES_FILE = "(none)",   // to signal that no Favorites file is defined
    REG_SHOW_LOG_ON_START = "ShowLogOnStartup",
    REG_MT_DIALOG_POS_X = "MultiTextDialogPositionX",
    REG_MT_DIALOG_POS_Y = "MultiTextDialogPositionY",
    REG_MT_DIALOG_SIZE_W = "MultiTextDialogWidth",
    REG_MT_DIALOG_SIZE_H = "MultiTextDialogHeight",
    // animation
    REG_ANIMATION_SECTION = "Cn3D-4-Animation",
    REG_SPIN_DELAY = "SpinDelay",
    REG_SPIN_INCREMENT = "SpinIncrement",
    REG_FRAME_DELAY = "FrameDelay",
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
    REG_FONT_NATIVE_FONT_INFO = "NativeFontInfo",
    // cache settings
    REG_CACHE_SECTION = "Cn3D-4-Cache",
    REG_CACHE_ENABLED = "CacheEnabled",
    REG_CACHE_FOLDER = "CacheFolder",
    REG_CACHE_MAX_SIZE = "CacheSizeMax",
    // advanced options
    REG_ADVANCED_SECTION = "Cn3D-4-Advanced",
    REG_CDD_ANNOT_READONLY = "CDDAnnotationsReadOnly",
#ifdef __WXGTK__
    REG_BROWSER_LAUNCH = "BrowserLaunchCommand",
#endif
    REG_MAX_N_STRUCTS = "MaxNumStructures",
    REG_FOOTPRINT_RES = "FootprintExcessResidues",
    REG_STEREO_SEPARATION = "StereoSeparationDegrees",
    REG_PROXIMAL_STEREO = "ProxmimalStereo";


// utility function to remove some elements from a vector
template < class T >
void VectorRemoveElements(std::vector < T >& v, const std::vector < bool >& remove, int nToRemove)
{
    if (v.size() != remove.size()) {
#ifndef _DEBUG
        // MSVC gets internal compiler error here on debug builds... ugh!
        ERR_POST(ncbi::Error << "VectorRemoveElements() - size mismatch");
#endif
        return;
    }

    std::vector < T > copy(v.size() - nToRemove);
    int i, nRemoved = 0;
    for (i=0; i<v.size(); ++i) {
        if (remove[i])
            ++nRemoved;
        else
            copy[i - nRemoved] = v[i];
    }
    if (nRemoved != nToRemove) {
#ifndef _DEBUG
        ERR_POST(ncbi::Error << "VectorRemoveElements() - bad nToRemove");
#endif
        return;
    }

    v = copy;
}

// utility function to delete all elements from an STL container
#define DELETE_ALL_AND_CLEAR(container, ContainerType) \
do { \
    ContainerType::iterator i, ie = (container).end(); \
    for (i=(container).begin(); i!=ie; ++i) \
        delete *i; \
    (container).clear(); \
} while (0)

template < class T >
class TypeStringAssociator
{
private:
    typedef typename std::map < T , std::string > T2String;
    T2String type2string;
    typedef typename std::map < std::string , T > String2T;
    String2T string2type;

public:
    void Associate(const T& type, const std::string& name)
    {
        type2string[type] = name;
        string2type[name] = type;
    }
    const T * Find(const std::string& name) const
    {
        typename TypeStringAssociator::String2T::const_iterator i = string2type.find(name);
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
        typename TypeStringAssociator::T2String::const_iterator i = type2string.find(type);
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

END_SCOPE(Cn3D)

#endif // CN3D_TOOLS__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.38  2005/03/25 15:10:45  thiessen
* matched self-hit E-values with CDTree2
*
* Revision 1.37  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
* Revision 1.36  2005/01/04 16:06:59  thiessen
* make MultiTextDialog remember its position+size
*
* Revision 1.35  2004/05/28 21:01:45  thiessen
* namespace/typename fixes for GCC 3.4
*
* Revision 1.34  2004/03/15 17:17:56  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.33  2004/03/10 23:15:51  thiessen
* add ability to turn off/on error dialogs; group block aligner errors in message log
*
* Revision 1.32  2004/02/19 17:04:52  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.31  2004/01/17 00:17:30  thiessen
* add Biostruc and network structure load
*
* Revision 1.30  2003/12/03 15:07:10  thiessen
* add more sophisticated animation controls
*
* Revision 1.29  2003/11/15 16:08:35  thiessen
* add stereo
*
* Revision 1.28  2003/03/13 16:57:14  thiessen
* fix favorites load/save problem
*
* Revision 1.27  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
* Revision 1.26  2003/02/03 19:20:03  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.25  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.24  2002/12/19 15:56:16  thiessen
* use wxCHECK_VERSION
*
* Revision 1.23  2002/10/18 17:15:33  thiessen
* use wxNativeEncodingInfo to store fonts in registry
*
* Revision 1.22  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.21  2002/10/11 17:21:39  thiessen
* initial Mac OSX build
*
* Revision 1.20  2002/09/13 14:21:45  thiessen
* finish hooking up browser launch on unix
*
* Revision 1.19  2002/08/28 20:30:33  thiessen
* fix proximity sort bug
*
* Revision 1.18  2002/07/23 15:46:49  thiessen
* print out more BLAST info; tweak save file name
*
* Revision 1.17  2002/06/05 14:28:39  thiessen
* reorganize handling of window titles
*
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
*/
