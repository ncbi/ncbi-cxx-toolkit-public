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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#if defined(__WXMSW__)
#include <windows.h>
#include <shellapi.h>   // for ShellExecute, needed to launch browser

#elif defined(__WXGTK__)
#include <unistd.h>

#elif defined(__WXMAC__)
// full paths needed to void having to add -I/Developer/Headers/FlatCarbon to all modules...
#include "/Developer/Headers/FlatCarbon/Types.h"
#include "/Developer/Headers/FlatCarbon/InternetConfig.h"
#endif

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/file.h>

#include "cn3d_tools.hpp"
#include "asn_reader.hpp"

#include <memory>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

///// Registry stuff /////

static CNcbiRegistry registry;
static string registryFile;
static bool registryChanged = false;

void LoadRegistry(void)
{
    if (GetPrefsDir().size() > 0)
        registryFile = GetPrefsDir() + "Preferences";
    else
        registryFile = GetProgramDir() + "Preferences";
    auto_ptr<CNcbiIfstream> iniIn(new CNcbiIfstream(registryFile.c_str(), IOS_BASE::in));
    if (*iniIn) {
        INFOMSG("loading program registry " << registryFile);
        registry.Read(*iniIn);
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
        toupper(regStr[0]) == 'T' || toupper(regStr[0]) == 'F' ||
        toupper(regStr[0]) == 'Y' || toupper(regStr[0]) == 'N'));
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
            toupper(regStr[0]) == 'T' || toupper(regStr[0]) == 'F' ||
            toupper(regStr[0]) == 'Y' || toupper(regStr[0]) == 'N')) {
        WARNINGMSG("Can't get boolean from registry: " << section << ", " << name);
        return false;
    }
    *value = (toupper(regStr[0]) == 'T' || toupper(regStr[0]) == 'Y');
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
    wxString regStr;
    regStr.Printf("%i", value);
    bool okay = registry.Set(section, name, regStr.c_str(), CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetDouble(const string& section, const string& name, double value)
{
    wxString regStr;
    regStr.Printf("%g", value);
    bool okay = registry.Set(section, name, regStr.c_str(), CNcbiRegistry::ePersistent);
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
static OSStatus MacLaunchURL(ConstStr255Param urlStr)
{
    OSStatus err;
    ICInstance inst;
    SInt32 startSel;
    SInt32 endSel;

    err = ICStart(&inst, 'Cn3D');
    if (err == noErr) {
#if !TARGET_CARBON
        err = ICFindConfigFile(inst, 0, nil);
#endif
        if (err == noErr) {
            startSel = 0;
            endSel = strlen(urlStr);
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
    MacLaunchURL(url);
#endif
}

CRef < CBioseq > FetchSequenceViaHTTP(const string& id)
{
    CSeq_entry seqEntry;
    string err;
    static const string host("www.ncbi.nlm.nih.gov"), path("/entrez/viewer.cgi");
    string args = string("view=0&maxplex=1&save=idf&val=") + id;
    INFOMSG("Trying to load sequence from URL " << host << path << '?' << args);

    CRef < CBioseq > bioseq;
    if (GetAsnDataViaHTTP(host, path, args, &seqEntry, &err)) {
        if (seqEntry.IsSeq())
            bioseq.Reset(&(seqEntry.SetSeq()));
        else if (seqEntry.IsSet() && seqEntry.GetSet().GetSeq_set().front()->IsSeq())
            bioseq.Reset(&(seqEntry.SetSet().SetSeq_set().front()->SetSeq()));
        else
            ERRORMSG("FetchSequenceViaHTTP() - confused by SeqEntry format");
    }
    if (bioseq.Empty())
        ERRORMSG("FetchSequenceViaHTTP() - HTTP Bioseq retrieval failed");

    return bioseq;
}


// gives NCBIStdaa residue number for a character (or value for 'X' if char not found)
unsigned char LookupNCBIStdaaNumberFromCharacter(char r)
{
    typedef map < char, unsigned char > Char2UChar;
    static Char2UChar charMap;

    if (charMap.size() == 0) {
        const string NCBIStdaaResidues("-ABCDEFGHIKLMNPQRSTVWXYZU*");
        for (unsigned int i=0; i<NCBIStdaaResidues.size(); ++i)
            charMap[NCBIStdaaResidues[i]] = (unsigned char) i;
    }

    Char2UChar::const_iterator n = charMap.find(toupper(r));
    if (n != charMap.end())
        return n->second;
    else
        return charMap.find('X')->second;
}

extern char LookupCharacterFromNCBIStdaaNumber(unsigned char n)
{
    static const string NCBIStdaaResidues("-ABCDEFGHIKLMNPQRSTVWXYZU*");
    if (n <= 25)
        return NCBIStdaaResidues[n];
    ERRORMSG("LookupCharacterFromNCBIStdaaNumber() - valid values are 0 - 25");
    return '?';
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
* Revision 1.9  2004/05/24 15:56:33  gorelenk
* Changed position of PCH include
*
* Revision 1.8  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2004/02/19 17:04:52  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.6  2004/01/17 00:17:30  thiessen
* add Biostruc and network structure load
*
* Revision 1.5  2003/12/16 02:16:49  ucko
* +<memory> for auto_ptr<>
*
* Revision 1.4  2003/11/15 16:08:35  thiessen
* add stereo
*
* Revision 1.3  2003/03/19 14:44:56  thiessen
* remove header dependency
*
* Revision 1.2  2003/03/13 22:48:43  thiessen
* fixes for Mac/OSX/gcc
*
* Revision 1.1  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
*/
