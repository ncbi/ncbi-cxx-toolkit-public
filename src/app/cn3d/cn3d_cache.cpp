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
* ---------------------------------------------------------------------------
* $Log$
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
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

// for file/directory manipulation stuff
#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/datetime.h>
#include <wx/file.h>
#include <wx/filename.h>

#include "cn3d/cn3d_cache.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/asn_reader.hpp"
#include "cn3d/asn_converter.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

static std::string GetCacheFilePath(int mmdbID, EModel_type modelType)
{
    std::string cachePath;
    if (RegistryGetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, &cachePath)) {
        wxString cacheFile;
        cacheFile.Printf("%s%c%i.%i", cachePath.c_str(), wxFILE_SEP_PATH, mmdbID, modelType);
        cachePath = cacheFile.c_str();
    } else
        ERR_POST(Error << "Can't get cache folder from registry");
    return cachePath;
}

static bool CreateCacheFolder(void)
{
    std::string cacheFolder;
    if (!RegistryGetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, &cacheFolder)) return false;
    if (wxDirExists(cacheFolder.c_str())) return true;
    bool okay = wxMkdir(cacheFolder.c_str());
    TESTMSG((okay ? "created" : "failed to create") << " folder " << cacheFolder);
    return okay;
}

static void ExtractBioseqs(std::list < CRef < CSeq_entry > >& seqEntries, BioseqRefList *sequences)
{
    std::list < CRef < CSeq_entry > >::iterator e, ee = seqEntries.end();
    for (e=seqEntries.begin(); e!=ee; e++) {
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
        ERR_POST(Error << "ExtractBiostrucAndBioseqs() - expecting strucseq mime");
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

static bool GetStructureFromCacheFolder(int mmdbID, EModel_type modelType,
    CRef < CBiostruc >& biostruc, BioseqRefList *sequences)
{
    // try to load from cache
    TESTMSG("looking for " << mmdbID << " (model type " << (int) modelType << ") in cache:");
    std::string err, cacheFile = GetCacheFilePath(mmdbID, modelType);
    CNcbi_mime_asn1 mime;
    SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
    bool gotFile = ReadASNFromFile(cacheFile.c_str(), &mime, true, &err);
    SetDiagPostLevel(eDiag_Info);
    if (!gotFile || !ExtractBiostrucAndBioseqs(mime, biostruc, sequences)) {
        ERR_POST(Warning << "failed to load " << mmdbID
            << " (model type " << (int) modelType << ") from cache: " << err);
        return false;
    }

    // if successful, 'touch' the file to mark it as recently used
    TESTMSG("loaded " << cacheFile);
    wxFileName fn(cacheFile.c_str());
    if (!fn.Touch())
        ERR_POST(Warning << "error touching " << cacheFile);

    return true;
}

static bool GetStructureViaHTTPAndAddToCache(int mmdbID, EModel_type modelType,
    CRef < CBiostruc >& biostruc, BioseqRefList *sequences)
{
    bool gotStructure = false;

    // construct URL
    static const wxString host = "www.ncbi.nlm.nih.gov", path = "/Structure/mmdb/mmdbsrv.cgi";
    wxString args;
    args.Printf("uid=%i&form=6&db=t&save=Save&dopt=j&Complexity=", mmdbID);
    switch (modelType) {
        case eModel_type_ncbi_all_atom: args += "Cn3D%20Subset"; break;
        case eModel_type_pdb_model: args += "All%20Models"; break;
        case eModel_type_ncbi_backbone:
        default:
            args += "Virtual%20Bond%20Model"; break;
    }

    // load from network
    TESTMSG("Trying to load structure data from " << host.c_str() << path.c_str() << '?' << args.c_str());
    std::string err;
    CNcbi_mime_asn1 mime;
    gotStructure = (GetAsnDataViaHTTP(host.c_str(), path.c_str(), args.c_str(), &mime, &err) &&
        ExtractBiostrucAndBioseqs(mime, biostruc, sequences));
    if (!gotStructure)
        ERR_POST(Warning << "Failed to read structure from network\nreason: " << err);

    if (gotStructure) {
        bool cacheEnabled;
        if (RegistryGetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, &cacheEnabled) && cacheEnabled) {
            // add to cache
            if (CreateCacheFolder() &&
                WriteASNToFile(GetCacheFilePath(mmdbID, modelType).c_str(), mime, true, &err)) {
                TESTMSG("stored " << mmdbID << " (model type " << (int) modelType << ") in cache");
                // trim cache to appropriate size if we've added a new file
                int size;
                if (RegistryGetInteger(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, &size))
                    TruncateCache(size);
            } else {
                ERR_POST(Warning << "Failed to write structure to cache folder");
                if (err.size() > 0) ERR_POST(Warning << "reason: " << err);
            }
        }
    }

    return gotStructure;
}

bool LoadStructureViaCache(int mmdbID, ncbi::objects::EModel_type modelType,
    CRef < CBiostruc >& biostruc, BioseqRefList *sequences)
{
    bool gotStructure = false;

    // try loading from local cache folder first, if cache enabled in registry
    bool cacheEnabled;
    if (RegistryGetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, &cacheEnabled) && cacheEnabled)
        gotStructure = GetStructureFromCacheFolder(mmdbID, modelType, biostruc, sequences);

    // otherwise, load via HTTP (and save in cache folder)
    if (!gotStructure)
        gotStructure = GetStructureViaHTTPAndAddToCache(mmdbID, modelType, biostruc, sequences);

    return gotStructure;
}

void TruncateCache(int maxSize)
{
    std::string cacheFolder;
    if (!RegistryGetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, &cacheFolder) ||
        !wxDirExists(cacheFolder.c_str())) {
        ERR_POST(Warning << "can't find cache folder");
        return;
    }
    TESTMSG("truncating cache to " << maxSize << " MB");

    wxString cacheFolderFiles;
    cacheFolderFiles.Printf("%s%c*", cacheFolder.c_str(), wxFILE_SEP_PATH);

    // empty directory if maxSize <= 0
    if (maxSize <= 0) {
        wxString f;
        while ((f=wxFindFirstFile(cacheFolderFiles, wxFILE)).size() > 0) {
            if (!wxRemoveFile(f))
                ERR_POST(Warning << "can't remove file " << f);
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
            ERR_POST(Warning << "can't remove file " << oldestFileName);

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
                ERR_POST(Warning << "wxFile failed to open " << file);
        }
        TESTMSG("total size: " << totalSize << " oldest file: " << oldestFileName.c_str());

    } while (totalSize > maxSize * 1024 * 1024);
}

END_SCOPE(Cn3D)

