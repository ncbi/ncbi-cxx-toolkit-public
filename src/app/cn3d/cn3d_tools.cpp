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
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#if defined(__WXMSW__)
#include <windows.h>
#include <shellapi.h>   // for ShellExecute, needed to launch browser

#elif defined(__WXGTK__)
#include <unistd.h>

#elif defined(__WXMAC__)
// full paths used to avoid adding extra -I option to point at FlatCarbon to compile flags for all modules...
// Under OSX 10.6 and earlier, /Developer was a root-level directory.  With 10.8, it is buried under XCode's tools.
//#include "/Developer/Headers/FlatCarbon/Types.h"
//#include "/Developer/Headers/FlatCarbon/InternetConfig.h"
#include "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk/Developer/Headers/FlatCarbon/InternetConfig.h"
#endif

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include "remove_header_conflicts.hpp"

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/file.h>
#include <wx/fileconf.h>

#include "cn3d_tools.hpp"
#include "asn_reader.hpp"

#include <memory>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

///// Registry stuff /////

static CMemoryRegistry registry;
static string registryFile;
static bool registryChanged = false;

static void SetRegistryDefaults(void)
{
    // default log window startup
    RegistrySetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, false);
    RegistrySetString(REG_CONFIG_SECTION, REG_FAVORITES_NAME, NO_FAVORITES_FILE);
    RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_POS_X, 50);
    RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_POS_Y, 50);
    RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_SIZE_W, 400);
    RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_SIZE_H, 400);

    // default animation controls
    RegistrySetInteger(REG_ANIMATION_SECTION, REG_SPIN_DELAY, 50);
    RegistrySetDouble(REG_ANIMATION_SECTION, REG_SPIN_INCREMENT, 2.0),
    RegistrySetInteger(REG_ANIMATION_SECTION, REG_FRAME_DELAY, 500);

    // default quality settings
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_SLICES, 10);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_STACKS, 8);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_BOND_SIDES, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SIDES, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SEGMENTS, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_HELIX_SIDES, 12);
    RegistrySetBoolean(REG_QUALITY_SECTION, REG_HIGHLIGHTS_ON, true);
    RegistrySetString(REG_QUALITY_SECTION, REG_PROJECTION_TYPE, "Perspective");

    if (IsWindowedMode()) {
        // default font for OpenGL (structure window)
        wxFont *font = wxFont::New(
#if defined(__WXMSW__)
            12,
#elif defined(__WXGTK__)
            14,
#elif defined(__WXMAC__)
            14,
#endif
            wxSWISS, wxNORMAL, wxBOLD, false);
        if (font && font->Ok())
            RegistrySetString(REG_OPENGL_FONT_SECTION, REG_FONT_NATIVE_FONT_INFO, WX_TO_STD(font->GetNativeFontInfoDesc()));
        else
            ERRORMSG("Can't create default structure window font");

        if (font) delete font;

        // default font for sequence viewers
        font = wxFont::New(
#if defined(__WXMSW__)
            10,
#elif defined(__WXGTK__)
            14,
#elif defined(__WXMAC__)
            12,
#endif
            wxROMAN, wxNORMAL, wxNORMAL, false);
        if (font && font->Ok())
            RegistrySetString(REG_SEQUENCE_FONT_SECTION, REG_FONT_NATIVE_FONT_INFO, WX_TO_STD(font->GetNativeFontInfoDesc()));
        else
            ERRORMSG("Can't create default sequence window font");
        if (font) delete font;
    }

    // default cache settings
    RegistrySetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, true);
    if (GetPrefsDir().size() > 0)
        RegistrySetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, GetPrefsDir() + "cache");
    else
        RegistrySetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, GetProgramDir() + "cache");
    RegistrySetInteger(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, 25);

    // default advanced options
    RegistrySetBoolean(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, true);
#ifdef __WXGTK__
    RegistrySetString(REG_ADVANCED_SECTION, REG_BROWSER_LAUNCH,
        // for launching netscape in a separate window
        "( netscape -noraise -remote 'openURL(<URL>,new-window)' || netscape '<URL>' ) >/dev/null 2>&1 &"
        // for launching netscape in an existing window
//        "( netscape -raise -remote 'openURL(<URL>)' || netscape '<URL>' ) >/dev/null 2>&1 &"
    );
#endif
    RegistrySetInteger(REG_ADVANCED_SECTION, REG_MAX_N_STRUCTS, 10);
    RegistrySetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, 0);

    // default stereo options
    RegistrySetDouble(REG_ADVANCED_SECTION, REG_STEREO_SEPARATION, 5.0);
    RegistrySetBoolean(REG_ADVANCED_SECTION, REG_PROXIMAL_STEREO, true);
}

void LoadRegistry(void)
{
    // first set up defaults, then override any/all with stuff from registry file
    SetRegistryDefaults();

    if (GetPrefsDir().size() > 0)
        registryFile = GetPrefsDir() + "Preferences";
    else
        registryFile = GetProgramDir() + "Preferences";
    auto_ptr<CNcbiIfstream> iniIn(new CNcbiIfstream(registryFile.c_str(), IOS_BASE::in | IOS_BASE::binary));
    if (*iniIn) {
        TRACEMSG("loading program registry " << registryFile);
        registry.Read(*iniIn, (CNcbiRegistry::ePersistent | CNcbiRegistry::eOverride));
    }

    registryChanged = false;
}

void SaveRegistry(void)
{
    if (registryChanged) {
        auto_ptr<CNcbiOfstream> iniOut(new CNcbiOfstream(registryFile.c_str(), IOS_BASE::out));
        if (*iniOut) {
//            TESTMSG("saving program registry " << registryFile);
            registry.Write(*iniOut);
        }
    }
}

bool RegistryIsValidInteger(const string& section, const string& name)
{
    long value;
    wxString regStr = registry.Get(section, name).c_str();
    return (regStr.size() > 0 && regStr.ToLong(&value));
}

bool RegistryIsValidDouble(const string& section, const string& name)
{
    double value;
    wxString regStr = registry.Get(section, name).c_str();
    return (regStr.size() > 0 && regStr.ToDouble(&value));
}

bool RegistryIsValidBoolean(const string& section, const string& name)
{
    string regStr = registry.Get(section, name);
    return (regStr.size() > 0 && (
        toupper((unsigned char) regStr[0]) == 'T' || toupper((unsigned char) regStr[0]) == 'F' ||
        toupper((unsigned char) regStr[0]) == 'Y' || toupper((unsigned char) regStr[0]) == 'N'));
}

bool RegistryIsValidString(const string& section, const string& name)
{
    string regStr = registry.Get(section, name);
    return (regStr.size() > 0);
}

bool RegistryGetInteger(const string& section, const string& name, int *value)
{
    wxString regStr = registry.Get(section, name).c_str();
    long l;
    if (regStr.size() == 0 || !regStr.ToLong(&l)) {
        WARNINGMSG("Can't get long from registry: " << section << ", " << name);
        return false;
    }
    *value = (int) l;
    return true;
}

bool RegistryGetDouble(const string& section, const string& name, double *value)
{
    wxString regStr = registry.Get(section, name).c_str();
    if (regStr.size() == 0 || !regStr.ToDouble(value)) {
        WARNINGMSG("Can't get double from registry: " << section << ", " << name);
        return false;
    }
    return true;
}

bool RegistryGetBoolean(const string& section, const string& name, bool *value)
{
    string regStr = registry.Get(section, name);
    if (regStr.size() == 0 || !(
            toupper((unsigned char) regStr[0]) == 'T' || toupper((unsigned char) regStr[0]) == 'F' ||
            toupper((unsigned char) regStr[0]) == 'Y' || toupper((unsigned char) regStr[0]) == 'N')) {
        WARNINGMSG("Can't get boolean from registry: " << section << ", " << name);
        return false;
    }
    *value = (toupper((unsigned char) regStr[0]) == 'T' || toupper((unsigned char) regStr[0]) == 'Y');
    return true;
}

bool RegistryGetString(const string& section, const string& name, string *value)
{
    string regStr = registry.Get(section, name);
    if (regStr.size() == 0) {
        WARNINGMSG("Can't get string from registry: " << section << ", " << name);
        return false;
    }
    *value = regStr;
    return true;
}

bool RegistrySetInteger(const string& section, const string& name, int value)
{
    bool okay = registry.Set(section, name, NStr::IntToString(value), CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetDouble(const string& section, const string& name, double value)
{
    bool okay = registry.Set(section, name, NStr::DoubleToString(value), CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetBoolean(const string& section, const string& name, bool value, bool useYesOrNo)
{
    string regStr;
    if (useYesOrNo)
        regStr = value ? "yes" : "no";
    else
        regStr = value ? "true" : "false";
    bool okay = registry.Set(section, name, regStr, CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetString(const string& section, const string& name, const string& value)
{
    bool okay = registry.Set(section, name, value, CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}


///// Misc stuff /////

// global strings for various directories - will include trailing path separator character
static string
    workingDir,     // current working directory
    programDir,     // directory where Cn3D executable lives
    dataDir,        // 'data' directory with external data files
    prefsDir;       // application preferences directory
const string& GetWorkingDir(void) { return workingDir; }
const string& GetProgramDir(void) { return programDir; }
const string& GetDataDir(void) { return dataDir; }
const string& GetPrefsDir(void) { return prefsDir; }

void SetUpWorkingDirectories(const char* argv0)
{
    // set up working directories
    workingDir = wxGetCwd().c_str();
#ifdef __WXGTK__
    if (getenv("CN3D_HOME") != NULL)
        programDir = getenv("CN3D_HOME");
    else
#endif
    if (wxIsAbsolutePath(argv0))
        programDir = wxPathOnly(argv0).c_str();
    else if (wxPathOnly(argv0) == "")
        programDir = workingDir;
    else
        programDir = workingDir + wxFILE_SEP_PATH + WX_TO_STD(wxPathOnly(argv0));
    workingDir = workingDir + wxFILE_SEP_PATH;
    programDir = programDir + wxFILE_SEP_PATH;

    // find or create preferences folder
    wxString localDir;
    wxFileName::SplitPath(wxFileConfig::GetLocalFileName("unused"), &localDir, NULL, NULL);
    wxString prefsDirLocal = localDir + wxFILE_SEP_PATH + "Cn3D_User";
    wxString prefsDirProg = wxString(programDir.c_str()) + wxFILE_SEP_PATH + "Cn3D_User";
    if (wxDirExists(prefsDirLocal))
        prefsDir = prefsDirLocal.c_str();
    else if (wxDirExists(prefsDirProg))
        prefsDir = prefsDirProg.c_str();
    else {
        // try to create the folder
        if (wxMkdir(prefsDirLocal) && wxDirExists(prefsDirLocal))
            prefsDir = prefsDirLocal.c_str();
        else if (wxMkdir(prefsDirProg) && wxDirExists(prefsDirProg))
            prefsDir = prefsDirProg.c_str();
    }
    if (prefsDir.size() == 0)
        WARNINGMSG("Can't create Cn3D_User folder at either:"
            << "\n    " << prefsDirLocal
            << "\nor  " << prefsDirProg);
    else
        prefsDir += wxFILE_SEP_PATH;

    // set data dir, and register the path in C toolkit registry (mainly for BLAST code)
#ifdef __WXMAC__
    dataDir = programDir + "../Resources/data/";
#else
    dataDir = programDir + "data" + wxFILE_SEP_PATH;
#endif

    TRACEMSG("working dir: " << workingDir.c_str());
    TRACEMSG("program dir: " << programDir.c_str());
    TRACEMSG("data dir: " << dataDir.c_str());
    TRACEMSG("prefs dir: " << prefsDir.c_str());
}

#ifdef __WXMSW__
// code borrowed (and modified) from Nlm_MSWin_OpenDocument() in vibutils.c
static bool MSWin_OpenDocument(const char* doc_name)
{
    int status = (int) ShellExecute(0, "open", doc_name, NULL, NULL, SW_SHOWNORMAL);
    if (status <= 32) {
        ERRORMSG("Unable to open document \"" << doc_name << "\", error = " << status);
        return false;
    }
    return true;
}
#endif

#ifdef __WXMAC__
//  CJL Hack ... pass the length of the string
static OSStatus MacLaunchURL(ConstStr255Param urlStr, long int len)
{
    OSStatus err;
    ICInstance inst;
    long int startSel;
    long int endSel;

    err = ICStart(&inst, 'Cn3D');
    if (err == noErr) {
#if !TARGET_CARBON
        err = ICFindConfigFile(inst, 0, nil);
#endif
        if (err == noErr) {
            startSel = 0;
//            endSel = strlen(urlStr);   //  OSX didn't like this:  invalid conversion from
//                                          'const unsigned char*' to 'const char*' compiler error.
// ConstStr255Param is an unsigned char*.  Mac developer docs do not seem to indicate the '255'
// means there are any length restrictions on such strings, and that implementations have some
// backing store for longer strings.   But to be safe, I'm truncating this to 255.
// As used in Cn3D none of the URLs are terribly long ... except when multiple annotations are selected.
// (Also see CoreFoundation header CFBase.h; used in ncbi_os_mac.hpp Pstrncpy)
            endSel = (len > 0 && len <= 255) ? len : 255;
            err = ICLaunchURL(inst, "\p", urlStr, endSel, &startSel, &endSel);
        }
        ICStop(inst);
    }
    return err;
}
#endif

void LaunchWebPage(const char *url)
{
    if(!url) return;
    INFOMSG("launching url " << url);

#if defined(__WXMSW__)
    if (!MSWin_OpenDocument(url)) {
        ERRORMSG("Unable to launch browser");
    }

#elif defined(__WXGTK__)
    string command;
    RegistryGetString(REG_ADVANCED_SECTION, REG_BROWSER_LAUNCH, &command);
    size_t pos = 0;
    while ((pos=command.find("<URL>", pos)) != string::npos)
        command.replace(pos, 5, url);
    TRACEMSG("launching browser with: " << command);
    system(command.c_str());

#elif defined(__WXMAC__)
    //  CJL:  hack of dubious generality to get the string length
    //        of a 'ConstStr255Param' type.
    //        Unclear if strings longer than 255 characters are safe.  See notes above in MacLaunchURL.
    unsigned int i = 0, l = strlen(url);
    unsigned char uc_url[l+1];
    for (; i < l && i < 255; ++i)  uc_url[i] = (unsigned char) *(url + i);
    uc_url[i] = '\0';
    MacLaunchURL(uc_url, l);
#endif
}

CRef < CBioseq > FetchSequenceViaHTTP(const string& id)
{
    CSeq_entry seqEntry;
    string err;
    static const string host("eutils.ncbi.nlm.nih.gov"), path("/entrez/eutils/efetch.fcgi");
    string args = string("rettype=asn.1&retmode=binary&maxplex=1&id=") + id;

    // efetch doesn't seem to care whether db is protein or nucleotide, when using gi or accession... but that may change in the future
    CRef < CBioseq > bioseq;
    for (unsigned int round=1; round<=2 && bioseq.Empty(); ++round) {
        string db = (round == 1) ? "protein" : "nucleotide";
        INFOMSG("Trying to load sequence from URL " << host << path << '?' << (args + "&db=" + db));
        bool ok = GetAsnDataViaHTTP(host, path, (args + "&db=" + db), &seqEntry, &err);
        if (ok) {
            if (seqEntry.IsSeq())
                bioseq.Reset(&(seqEntry.SetSeq()));
            else if (seqEntry.IsSet() && seqEntry.GetSet().GetSeq_set().front()->IsSeq())
                bioseq.Reset(&(seqEntry.SetSet().SetSeq_set().front()->SetSeq()));
            else
                WARNINGMSG("FetchSequenceViaHTTP() - confused by SeqEntry format");
        } else {
            WARNINGMSG("FetchSequenceViaHTTP() - HTTP Bioseq retrieval failed, err: " << err);
        }
    }
    return bioseq;
}

static const string NCBIStdaaResidues("-ABCDEFGHIKLMNPQRSTVWXYZU*OJ");

// gives NCBIStdaa residue number for a character (or value for 'X' if char not found)
unsigned char LookupNCBIStdaaNumberFromCharacter(char r)
{
    typedef map < char, unsigned char > Char2UChar;
    static Char2UChar charMap;

    if (charMap.size() == 0) {
        for (unsigned int i=0; i<NCBIStdaaResidues.size(); ++i)
            charMap[NCBIStdaaResidues[i]] = (unsigned char) i;
    }

    Char2UChar::const_iterator n = charMap.find(toupper((unsigned char) r));
    if (n != charMap.end())
        return n->second;
    else
        return charMap.find('X')->second;
}

char LookupCharacterFromNCBIStdaaNumber(unsigned char n)
{
    if (n <= 27)
        return NCBIStdaaResidues[n];
    ERRORMSG("LookupCharacterFromNCBIStdaaNumber() - valid values are 0 - 27");
    return '?';
}

bool Prosite2Regex(const string& prosite, string *regex, int *nGroups)
{
    try {
        // check allowed characters ('#' isn't ProSite, but is a special case used to match an 'X' residue character)
        static const string allowed = "-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[],(){}<>.#";
        unsigned int i;
        for (i=0; i<prosite.size(); ++i)
            if (allowed.find(toupper((unsigned char) prosite[i])) == string::npos) break;
        if (i != prosite.size()) throw "invalid ProSite character";
        if (prosite[prosite.size() - 1] != '.') throw "ProSite pattern must end with '.'";

        // translate into real regex syntax;
        regex->erase();
        *nGroups = 0;

        bool inGroup = false;
        for (unsigned int i=0; i<prosite.size(); ++i) {

            // handle grouping and termini
            bool characterHandled = true;
            switch (prosite[i]) {
                case '-': case '.': case '>':
                    if (inGroup) {
                        *regex += ')';
                        inGroup = false;
                    }
                    if (prosite[i] == '>') *regex += '$';
                    break;
                case '<':
                    *regex += '^';
                    break;
                default:
                    characterHandled = false;
                    break;
            }
            if (characterHandled) continue;
            if (!inGroup && (
                    (isalpha((unsigned char) prosite[i]) && toupper((unsigned char) prosite[i]) != 'X') ||
                    prosite[i] == '[' || prosite[i] == '{' || prosite[i] == '#')) {
                *regex += '(';
                ++(*nGroups);
                inGroup = true;
            }

            // translate syntax
            switch (prosite[i]) {
                case '(':
                    *regex += '{';
                    break;
                case ')':
                    *regex += '}';
                    break;
                case '{':
                    *regex += "[^";
                    break;
                case '}':
                    *regex += ']';
                    break;
                case 'X': case 'x':
                    *regex += '.';
                    break;
                case '#':
                    *regex += 'X';
                    break;
                default:
                    *regex += toupper((unsigned char) prosite[i]);
                    break;
            }
        }
    }

    catch (const char *err) {
        ERRORMSG("Prosite2Regex() - " << err);
        return false;
    }

    return true;
}

unsigned int PrositePatternLength(const string& prosite)
{
    //  ('#' isn't ProSite, but is a special case used to match an 'X' residue character)
    static const string allowed = "-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[],(){}<>.#";

    // check allowed characters 
    unsigned int i;
    for (i=0; i<prosite.size(); ++i)
        if (allowed.find(toupper((unsigned char) prosite[i])) == string::npos) break;
    if (i != prosite.size()) return 0;

    bool hasOnlyX = true, stopParsing = false;
    bool inBraces = false, inBrackets = false, inParens = false;
    unsigned int length = 0;
    int nFromParens;
    string betweenParens;

    for (i=0; i < prosite.size() && !stopParsing; ++i) {

        // handle grouping and termini
        bool characterHandled = true;
        switch (prosite[i]) {
            case '-': case '.': case '>': case '<':
                break;
            default:
                characterHandled = false;
                break;
        }
        if (inParens && prosite[i] != ')' && !characterHandled) betweenParens += prosite[i];
        if (characterHandled) continue;

        if (hasOnlyX && isalpha((unsigned char) prosite[i]) && toupper((unsigned char) prosite[i]) != 'X') {
            hasOnlyX = false;
        }

        // translate syntax
        switch (prosite[i]) {
            case '(':
                inParens = true;
                break;
            case ')':
                nFromParens = NStr::StringToInt(betweenParens, NStr::fConvErr_NoThrow);

                //  Do not allow a variable number of repetitions.
                //  Also, length has already been incremented by 1 for whatever the (...) references
                if (nFromParens > 0) 
                    length += nFromParens - 1;
                else   
                    stopParsing = true;

                inParens = false;
                betweenParens.erase();
                break;
            case '{':
                inBraces = true;
                break;
            case '}':
                ++length;
                inBraces = false;
                break;
            case '[':
                inBrackets = true;
                break;
            case ']':
                ++length;
                inBrackets = false;
                break;
            default:
                if (!inParens && !inBraces && !inBrackets) ++length;
                break;
        }
    }

    //  Invalid pattern:  Appear to have missed a closing parenthesis/brace/bracket.
    if (inParens || inBrackets || inBraces) length = 0;

    //  Invalid pattern:  Appear to have all 'X' characters.
    if (hasOnlyX) length = 0;

    //  If there was some parsing error or prosite pattern allowed
    //  a match of indeterminate length, return 0.
    if (stopParsing) length = 0;

    return length;
}

END_SCOPE(Cn3D)
