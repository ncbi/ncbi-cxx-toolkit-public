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
*      implements a basic cache for Biostrucs
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/11/01 19:01:40  thiessen
* use meta key instead of ctrl on Mac
*
* Revision 1.1  2001/10/30 02:54:11  thiessen
* add Biostruc cache
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

// for file/directory manipulation stuff
#include <wx/wx.h>
#include <wx/datetime.h>
#include <wx/file.h>

#include "cn3d/cn3d_cache.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/asn_reader.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

static std::string GetCacheFilePath(int mmdbID, int modelType)
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

static bool GetBiostrucFromCacheFolder(int mmdbID, int modelType, CBiostruc *biostruc)
{
    // try to load from cache
    TESTMSG("looking for " << mmdbID << " (model type " << modelType << ") in cache:");
    std::string err, cacheFile = GetCacheFilePath(mmdbID, modelType);
    if (!ReadASNFromFile(cacheFile.c_str(), biostruc, true, &err)) {
        ERR_POST(Warning << "failed to load " << mmdbID
            << " (model type " << modelType << ") from cache: " << err);
        return false;
    }

    // if successful, 'touch' the file to mark it as recently used
    TESTMSG("loaded " << cacheFile);
    if (!WriteASNToFile(cacheFile.c_str(), *biostruc, true, &err))  // gotta be a better way!
        ERR_POST(Warning << "error touching " << cacheFile);

    return true;
}

static bool GetBiostrucViaHTTPAndAddToCache(int mmdbID, int modelType, CBiostruc *biostruc)
{
    bool gotBiostruc = false;

    // construct URL
    static const wxString host = "www.ncbi.nlm.nih.gov", path = "/Structure/mmdb/mmdbsrv.cgi";
    wxString args;
    args.Printf("uid=%i&form=6&db=t&save=Save&dopt=i&Complexity=", mmdbID);
    switch (modelType) {
        case eModel_type_ncbi_all_atom: args += "Cn3D%20Subset"; break;
        case eModel_type_pdb_model: args += "All%20Models"; break;
        case eModel_type_ncbi_backbone:
        default:
            args += "Virtual%20Bond%20Model"; break;
    }

    // load from network
    TESTMSG("Trying to load Biostruc data from " << host.c_str() << path.c_str() << '?' << args.c_str());
    std::string err;
    gotBiostruc = GetAsnDataViaHTTP(host.c_str(), path.c_str(), args.c_str(), biostruc, &err);
    if (!gotBiostruc)
        ERR_POST(Warning << "Failed to read Biostruc from network\nreason: " << err);

    if (gotBiostruc) {
        bool cacheEnabled;
        if (RegistryGetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, &cacheEnabled) && cacheEnabled) {
            // add to cache
            if (CreateCacheFolder() &&
                WriteASNToFile(GetCacheFilePath(mmdbID, modelType).c_str(), *biostruc, true, &err)) {
                TESTMSG("stored " << mmdbID << " (model type " << modelType << ") in cache");
                // trim cache to appropriate size if we've added a new file
                int size;
                if (RegistryGetInteger(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, &size))
                    TruncateCache(size);
            } else {
                ERR_POST(Warning << "Failed to write Biostruc to cache folder");
                if (err.size() > 0) ERR_POST(Warning << "reason: " << err);
            }
        }
    }

    return gotBiostruc;
}

bool LoadBiostrucViaCache(int mmdbID, int modelType, ncbi::objects::CBiostruc *biostruc)
{
    bool gotBiostruc = false;

    // clear existing data (probably done by object loader anyway, but just in case...)
    biostruc->ResetId();
    biostruc->ResetDescr();
    biostruc->ResetChemical_graph();
    biostruc->ResetFeatures();
    biostruc->ResetModel();

    // try loading from local cache folder first, if cache enabled in registry
    bool cacheEnabled;
    if (RegistryGetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, &cacheEnabled) && cacheEnabled)
        gotBiostruc = GetBiostrucFromCacheFolder(mmdbID, modelType, biostruc);

    // otherwise, load via HTTP (and save in cache folder)
    if (!gotBiostruc)
        gotBiostruc = GetBiostrucViaHTTPAndAddToCache(mmdbID, modelType, biostruc);

    return gotBiostruc;
}

void TruncateCache(int maxSize)
{
#ifdef __WXMAC__
    // doesn't work, until wxFindFirstFile() is fixed...
    return;
#endif

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
        wxString file = wxFindFirstFile(cacheFolderFiles, wxFILE);
        for (; file.size() > 0; file = wxFindNextFile())
            if (!wxRemoveFile(file))
                ERR_POST(Warning << "can't remove file " << file);
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

