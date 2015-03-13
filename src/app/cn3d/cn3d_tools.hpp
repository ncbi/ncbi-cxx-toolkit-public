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

#if !wxCHECK_VERSION(2,8,0)
#error Cn3D requires wxWindows version 2.8.0 or higher!
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
#define CN3D_VERSION_STRING "4.3.1"

// diagnostic streams
#define TRACEMSG(stream) ERR_POST(ncbi::Trace << stream)
#define INFOMSG(stream) ERR_POST(ncbi::Info << stream)
#define WARNINGMSG(stream) ERR_POST(ncbi::Warning << stream)
#define ERRORMSG(stream) ERR_POST(ncbi::Error << stream)
#define FATALMSG(stream) ERR_FATAL(stream)

// turn on/off dialog box for errors (on by default)
void SetDialogSevereErrors(bool status);

// strings for various directories - dirs will include trailing path separator character
extern const std::string& GetWorkingDir(void);  // current working directory
extern const std::string& GetUserDir(void);     // directory of latest user-selected file
extern const std::string& GetProgramDir(void);  // directory where Cn3D executable lives
extern const std::string& GetDataDir(void);     // 'data' directory with external data files
extern const std::string& GetWorkingFilename(void); // name of current working file
extern const std::string& GetPrefsDir(void);    // application preferences directory

// initial directory setup
void SetUpWorkingDirectories(const char* argv0);

// program mode
bool IsWindowedMode(void);

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

// parse a string as ProSite into a regular expression
bool Prosite2Regex(const std::string& prosite, std::string *regex, int *nGroups);

//  Returns 0 for invalid Prosite patterns, including patterns that
//  contain only 'X' characters, or those of indeterminate length.
//  Does not require that the prosite string have a terminal '.'.
unsigned int PrositePatternLength(const std::string& prosite);

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
void VectorRemoveElements(std::vector < T >& v, const std::vector < bool >& remove, unsigned int nToRemove)
{
    if (v.size() != remove.size()) {
#ifndef _DEBUG
        // MSVC gets internal compiler error here on debug builds... ugh!
        ERR_POST(ncbi::Error << "VectorRemoveElements() - size mismatch");
#endif
        return;
    }

    std::vector < T > copy(v.size() - nToRemove);
    unsigned int i, nRemoved = 0;
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


#if wxCHECK_VERSION(2,9,0)
#define WX_TO_STD(wxstring) ((wxstring).ToStdString())
#else
#define WX_TO_STD(wxstring) (std::string((wxstring).c_str()))
#endif


END_SCOPE(Cn3D)

#endif // CN3D_TOOLS__HPP
