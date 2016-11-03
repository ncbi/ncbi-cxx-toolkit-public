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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>

#include "remove_header_conflicts.hpp"

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

//  If assemblyId = -1, use the predefined 'default' assembly.
//  Otherwise, get the specific assembly requested, where 
//  assemblyId = 0 means the ASU, and PDB-defined assemblies
//  are indexed sequentially from 1.
static CNcbi_mime_asn1 * GetStructureViaHTTPAndAddToCache(
    const string& uid, int mmdbID, EModel_type modelType, int assemblyId = 0)
{
    string host, path, args;
    
    if (assemblyId == 0)  {
        // construct URL [mmdbsrv.cgi]
        host = "www.ncbi.nlm.nih.gov";
        path = "/Structure/mmdb/mmdbsrv.cgi";
        args = "save=Save&dopt=j&uid=";
        if (mmdbID > 0)
            args += NStr::IntToString(mmdbID);
        else    // assume PDB id
            args += uid;
        args += "&Complexity=";
        switch (modelType) {
            case eModel_type_ncbi_all_atom: args += "3"; break;
            case eModel_type_pdb_model: args += "4"; break;
            case eModel_type_ncbi_backbone:
            default:
                args += "2"; break;
        }
    }
    
    else {
        // construct URL [mmdb_strview.cgi]
        host = "www.ncbi.nlm.nih.gov";
        path = "/Structure/mmdb/mmdb_strview.cgi";
        args = "program=cn3d&display=1&uid=";
        if (mmdbID > 0)
            args += NStr::IntToString(mmdbID);
        else    // assume PDB id
            args += uid;
        args += "&complexity=";
        switch (modelType) {
            case eModel_type_ncbi_vector: args += "1"; break;
            case eModel_type_ncbi_all_atom: args += "3"; break;
            case eModel_type_pdb_model: args += "4"; break;
            case eModel_type_ncbi_backbone:
            default:
                args += "2"; break;
        }
        args += "&buidx=" + NStr::IntToString(assemblyId);
    }

    // load from network
    INFOMSG("Trying to load structure data from " << host << path << '?' << args);
    string err;
    CRef < CNcbi_mime_asn1 > mime(new CNcbi_mime_asn1());

    if (!GetAsnDataViaHTTPS(host, path, args, mime.GetPointer(), &err) ||
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

CNcbi_mime_asn1 * LoadStructureViaCache(const std::string& uid, ncbi::objects::EModel_type modelType, int assemblyId)
{
    // determine whether this is an integer MMDB ID or alphanumeric PDB ID
    int mmdbID = 0;
    if (uid.size() == 4 && (isalpha((unsigned char) uid[1]) || isalpha((unsigned char) uid[2]) || isalpha((unsigned char) uid[3]))) {
        TRACEMSG("Fetching PDB " << uid);
    } else {    // mmdb id
        unsigned long tmp;
        if (wxString(uid.c_str()).ToULong(&tmp)) {
            mmdbID = (int) tmp;
        } else {
            ERRORMSG("LoadStructureViaCache() - invalid uid " << uid);
            return NULL;
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
        mime = GetStructureViaHTTPAndAddToCache(uid, mmdbID, modelType, assemblyId);

    return mime;
}

bool LoadStructureViaCache(const std::string& uid, ncbi::objects::EModel_type modelType, int assemblyId,
    CRef < CBiostruc >& biostruc, BioseqRefList *sequences)
{
    CRef < CNcbi_mime_asn1 > mime(LoadStructureViaCache(uid, modelType, assemblyId));
    return (mime.NotEmpty() && ExtractBiostrucAndBioseqs(*mime, biostruc, sequences));
}

void TruncateCache(unsigned int maxSize)
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
