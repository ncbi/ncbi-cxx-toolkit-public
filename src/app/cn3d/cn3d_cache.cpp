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
*      implements a basic cache for structures
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>

// for file/directory manipulation stuff
#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/datetime.h>
#include <wx/file.h>
#include <wx/filename.h>

#include "cn3d_cache.hpp"
#include "cn3d_tools.hpp"
#include "asn_reader.hpp"
#include "asn_converter.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

static string GetCacheFilePath(int mmdbID, EModel_type modelType)
{
    string cachePath;
    if (RegistryGetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, &cachePath)) {
        wxString cacheFile;
        cacheFile.Printf("%s%c%i.%i", cachePath.c_str(), wxFILE_SEP_PATH, mmdbID, modelType);
        cachePath = cacheFile.c_str();
    } else
        ERRORMSG("Can't get cache folder from registry");
    return cachePath;
}

static bool CreateCacheFolder(void)
{
    string cacheFolder;
    if (!RegistryGetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, &cacheFolder)) return false;
    if (wxDirExists(cacheFolder.c_str())) return true;
    bool okay = wxMkdir(cacheFolder.c_str());
    TRACEMSG((okay ? "created" : "failed to create") << " folder " << cacheFolder);
    return okay;
}

static void ExtractBioseqs(list < CRef < CSeq_entry > >& seqEntries, BioseqRefList *sequences)
{
    list < CRef < CSeq_entry > >::iterator e, ee = seqEntries.end();
    for (e=seqEntries.begin(); e!=ee; ++e) {
        if ((*e)->IsSeq())
            sequences->push_back(CRef<CBioseq>(&((*e)->SetSeq())));
        else
            ExtractBioseqs((*e)->SetSet().SetSeq_set(), sequences);
    }
}

bool ExtractBiostrucAndBioseqs(CNcbi_mime_asn1& mime,
    CRef < CBiostruc >& biostruc, BioseqRefList *sequences)
{
    if (!mime.IsStrucseq()) {
        ERRORMSG("ExtractBiostrucAndBioseqs() - expecting strucseq mime");
        return false;
    }

    // copy mime's biostruc into existing object
    biostruc.Reset(&(mime.SetStrucseq().SetStructure()));

    // extract Bioseqs
    if (sequences) {
        sequences->clear();
        ExtractBioseqs(mime.SetStrucseq().SetSequences(), sequences);
    }

    return true;
}

static CNcbi_mime_asn1 * GetStructureFromCacheFolder(int mmdbID, EModel_type modelType)
{
    // try to load from cache
    INFOMSG("looking for " << mmdbID << " (model type " << (int) modelType << ") in cache:");
    string err, cacheFile = GetCacheFilePath(mmdbID, modelType);
    CRef < CNcbi_mime_asn1 > mime(new CNcbi_mime_asn1());
    SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
    bool gotFile = ReadASNFromFile(cacheFile.c_str(), mime.GetPointer(), true, &err);
    SetDiagPostLevel(eDiag_Info);
    if (!gotFile) {
        WARNINGMSG("failed to load " << mmdbID
            << " (model type " << (int) modelType << ") from cache: " << err);
        return NULL;
    }

    // if successful, 'touch' the file to mark it as recently used
    INFOMSG("loaded " << cacheFile);
    wxFileName fn(cacheFile.c_str());
    if (!fn.Touch())
        WARNINGMSG("error touching " << cacheFile);

    return mime.Release();
}

static CNcbi_mime_asn1 * GetStructureViaHTTPAndAddToCache(
    const string& uid, int mmdbID, EModel_type modelType)
{
    // construct URL
    static const wxString host = "www.ncbi.nlm.nih.gov", path = "/Structure/mmdb/mmdbsrv.cgi";
    wxString args;
    if (mmdbID > 0)
        args.Printf("uid=%i&form=6&db=t&save=Save&dopt=j&Complexity=", mmdbID);
    else    // assume PDB id
        args.Printf("uid=%s&form=6&db=t&save=Save&dopt=j&Complexity=", uid.c_str());
    switch (modelType) {
        case eModel_type_ncbi_all_atom: args += "Cn3D%20Subset"; break;
        case eModel_type_pdb_model: args += "All%20Models"; break;
        case eModel_type_ncbi_backbone:
        default:
            args += "Virtual%20Bond%20Model"; break;
    }

    // load from network
    INFOMSG("Trying to load structure data from " << host.c_str() << path.c_str() << '?' << args.c_str());
    string err;
    CRef < CNcbi_mime_asn1 > mime(new CNcbi_mime_asn1());

    if (!GetAsnDataViaHTTP(host.c_str(), path.c_str(), args.c_str(), mime.GetPointer(), &err) ||
            !mime->IsStrucseq()) {
        ERRORMSG("Failed to read structure " << uid << " from network\nreason: " << err);
        return NULL;

    } else {
        // get MMDB ID from biostruc if not already known
        if (mmdbID == 0) {
            if (mime->GetStrucseq().GetStructure().GetId().front()->IsMmdb_id())
                mmdbID = mime->GetStrucseq().GetStructure().GetId().front()->GetMmdb_id().Get();
            else {
                ERRORMSG("Can't get MMDB ID from Biostruc!");
                return mime.Release();
            }
        }

        bool cacheEnabled;
        if (RegistryGetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, &cacheEnabled) && cacheEnabled) {
            // add to cache
            if (CreateCacheFolder() &&
                WriteASNToFile(GetCacheFilePath(mmdbID, modelType).c_str(), *mime, true, &err)) {
                INFOMSG("stored " << mmdbID << " (model type " << (int) modelType << ") in cache");
                // trim cache to appropriate size if we've added a new file
                int size;
                if (RegistryGetInteger(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, &size))
                    TruncateCache(size);
            } else {
                WARNINGMSG("Failed to write structure to cache folder");
                if (err.size() > 0) WARNINGMSG("reason: " << err);
            }
        }
    }

    return mime.Release();
}

CNcbi_mime_asn1 * LoadStructureViaCache(const std::string& uid, ncbi::objects::EModel_type modelType)
{
    // determine whether this is an integer MMDB ID or alphanumeric PDB ID
    int mmdbID = 0;
    if (uid.size() == 4 && (isalpha(uid[1]) || isalpha(uid[2]) || isalpha(uid[3]))) {
        TRACEMSG("Fetching PDB " << uid);
    } else {    // mmdb id
        unsigned long tmp;
        if (wxString(uid.c_str()).ToULong(&tmp)) {
            mmdbID = (int) tmp;
        } else {
            ERRORMSG("LoadStructureViaCache() - invalid uid " << uid);
            return false;
        }
        TRACEMSG("Fetching MMDB " << mmdbID);
    }

    // try loading from local cache folder first, if cache enabled in registry (but only with known mmdb id)
    bool cacheEnabled;
    CNcbi_mime_asn1 *mime = NULL;
    if (mmdbID > 0 &&
            RegistryGetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, &cacheEnabled) &&
            cacheEnabled)
        mime = GetStructureFromCacheFolder(mmdbID, modelType);

    // otherwise, load via HTTP (and save in cache folder)
    if (!mime)
        mime = GetStructureViaHTTPAndAddToCache(uid, mmdbID, modelType);

    return mime;
}

bool LoadStructureViaCache(const std::string& uid, ncbi::objects::EModel_type modelType,
    CRef < CBiostruc >& biostruc, BioseqRefList *sequences)
{
    CRef < CNcbi_mime_asn1 > mime(LoadStructureViaCache(uid, modelType));
    return (mime.NotEmpty() && ExtractBiostrucAndBioseqs(*mime, biostruc, sequences));
}

void TruncateCache(int maxSize)
{
    string cacheFolder;
    if (!RegistryGetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, &cacheFolder) ||
        !wxDirExists(cacheFolder.c_str())) {
        WARNINGMSG("can't find cache folder");
        return;
    }
    INFOMSG("truncating cache to " << maxSize << " MB");

    wxString cacheFolderFiles;
    cacheFolderFiles.Printf("%s%c*", cacheFolder.c_str(), wxFILE_SEP_PATH);

    // empty directory if maxSize <= 0
    if (maxSize <= 0) {
        wxString f;
        while ((f=wxFindFirstFile(cacheFolderFiles, wxFILE)).size() > 0) {
            if (!wxRemoveFile(f))
                WARNINGMSG("can't remove file " << f);
        }
        return;
    }

    // otherwise, add up file sizes and keep deleting oldest until total size <= max
    unsigned long totalSize = 0;
    wxString oldestFileName;
    do {

        // if totalSize > 0, then we've already scanned the folder and know it's too big,
        // so delete oldest file
        if (totalSize > 0 && !wxRemoveFile(oldestFileName))
            WARNINGMSG("can't remove file " << oldestFileName);

        // loop through files, finding oldest and calculating total size
        totalSize = 0;
        time_t oldestFileDate = wxDateTime::GetTimeNow(), date;
        wxString file = wxFindFirstFile(cacheFolderFiles, wxFILE);
        for (; file.size() > 0; file = wxFindNextFile()) {
            date = wxFileModificationTime(file);
            if (date < oldestFileDate) {
                oldestFileDate = date;
                oldestFileName = file;
            }
            wxFile wx_file(file, wxFile::read);
            if (wx_file.IsOpened()) {
                totalSize += wx_file.Length();
                wx_file.Close();
            } else
                WARNINGMSG("wxFile failed to open " << file);
        }
        INFOMSG("total size: " << totalSize << " oldest file: " << oldestFileName.c_str());

    } while (totalSize > maxSize * 1024 * 1024);
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.14  2004/03/15 18:23:01  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.13  2004/02/19 17:04:49  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.12  2004/01/17 00:17:29  thiessen
* add Biostruc and network structure load
*
* Revision 1.11  2003/04/02 17:49:18  thiessen
* allow pdb id's in structure import dialog
*
* Revision 1.10  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.9  2002/09/30 17:13:02  thiessen
* change structure import to do sequences as well; change cache to hold mimes; change block aligner vocabulary; fix block aligner dialog bugs
*
* Revision 1.8  2002/09/11 01:39:35  thiessen
* fix cache file touch
*
* Revision 1.7  2002/08/15 22:13:13  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.6  2002/03/07 15:45:45  thiessen
* compile fix ; extra file load messages
*
* Revision 1.5  2002/02/27 16:29:40  thiessen
* add model type flag to general mime type
*
* Revision 1.4  2002/01/11 15:48:49  thiessen
* update for Mac CW7
*
* Revision 1.3  2001/11/09 15:19:16  thiessen
* wxFindFirst file fixed on wxMac; call glViewport() from OnSize()
*
* Revision 1.2  2001/11/01 19:01:40  thiessen
* use meta key instead of ctrl on Mac
*
* Revision 1.1  2001/10/30 02:54:11  thiessen
* add Biostruc cache
*
*/
