/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>

#include "wgs_filelist.hpp"

USING_NCBI_SCOPE;

namespace wgsparse
{

static const char PATH_SEPARATOR = CDirEntry::GetPathSeparator();

static void GetFiles(const string& top_dir, const string& cur_dir, const string& mask, bool recursive, list<string>& files)
{
    string full_dir = top_dir + cur_dir;
    CDir dir(full_dir);
    CDir::TEntries entries = dir.GetEntries(mask, CDir::fIgnoreRecursive);

    for (auto entry : entries) {

        if (!entry->IsDir()) {
            string fname = full_dir + PATH_SEPARATOR + entry->GetName();
            files.push_back(fname);
        }
    }

    if (recursive) {

        entries = dir.GetEntries("", CDir::fIgnoreRecursive);
        for (auto entry : entries) {

            if (entry->IsDir()) {
                GetFiles(top_dir, cur_dir + PATH_SEPARATOR + entry->GetName(), mask, recursive, files);
            }
        }
    }
}

bool GetFilesFromDir(const string& mask, list<string>& files)
{
    string top_dir_str = CDirEntry::GetNearestExistingParentDir(mask);
    CDirEntry top_dir(top_dir_str);

    if (top_dir.IsDir()) {
        string pure_mask = mask.substr(top_dir_str.size() + 1);

        size_t slash = pure_mask.find_last_of('\\');
        if (slash == string::npos) {
            slash = pure_mask.find_last_of('/');
        }

        bool recursive = false;
        if (slash != string::npos) {
            recursive = true;
            pure_mask = pure_mask.substr(slash + 1);
        }

        GetFiles(top_dir_str, "", pure_mask, recursive, files);
    }
    else {
        files.push_back(top_dir_str); // the only file
    }

    return !files.empty();
}

bool GetFilesFromFile(const string& file_name, list<std::string>& files)
{
    ifstream in(file_name);

    static const size_t BUF_SIZE = 1024;
    vector<char> buf(BUF_SIZE);
    while (in) {
        in.getline(&buf[0], BUF_SIZE);
        if (in) {
            files.push_back(&buf[0]);
        }
    }

    return !files.empty();
}

bool IsDupFileNames(const std::list<std::string>& files, string& dup_name)
{
    set<string> fnames;
    for (auto& filename : files) {

        string cur_name = filename;
        size_t slash = cur_name.find_last_of(PATH_SEPARATOR);
        if (slash != string::npos) {
            cur_name = cur_name.substr(slash + 1);
        }

        if (fnames.find(cur_name) != fnames.end()) {
            dup_name = cur_name;
            return true;
        }

        fnames.insert(cur_name);
    }

    return false;
}

bool MakeDir(const std::string& dir_path)
{
    CDir dir(dir_path);
    return dir.Create(CDir::fCreate_Default);
}


}
