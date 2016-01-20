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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <util/file_manifest.hpp>
#include <util/stream_source.hpp>

BEGIN_NCBI_SCOPE


void CInputStreamSource::SetStandardInputArgs(CArgDescriptions& arg_desc,
                                              const string &prefix,
                                              const string &description,
                                              bool is_mandatory)
{
    arg_desc.SetCurrentGroup("Input Options for " + prefix);
    if (prefix == "input") {
        arg_desc.AddDefaultKey("input", "InputFile",
                               "Stream of " + description,
                               CArgDescriptions::eInputFile,
                               "-");
        arg_desc.AddAlias("i", "input");
    } else {
        if (is_mandatory) {
            arg_desc.AddKey(prefix, "InputFile",
                            "Stream of " + description,
                            CArgDescriptions::eInputFile);
        }
        else {
            arg_desc.AddOptionalKey(prefix, "InputFile",
                                    "Stream of " + description,
                                    CArgDescriptions::eInputFile);
        }
    }

    arg_desc.AddOptionalKey(prefix + "-path", "InputPath",
                            "Path to " + description,
                            CArgDescriptions::eString);
    arg_desc.AddOptionalKey(prefix + "-mask", "FileMask",
                            "File pattern to search for " + description,
                            CArgDescriptions::eString);
    arg_desc.SetDependency(prefix + "-mask",
                           CArgDescriptions::eRequires,
                           prefix + "-path");

    arg_desc.AddOptionalKey(prefix + "-manifest", "InputFile",
                            "File containing a list of files containing " + description,
                            CArgDescriptions::eInputFile);

    arg_desc.SetDependency(prefix,
                           CArgDescriptions::eExcludes,
                           prefix + "-manifest");

    arg_desc.SetDependency(prefix,
                           CArgDescriptions::eExcludes,
                           prefix + "-path");

    arg_desc.SetDependency(prefix + "-manifest",
                           CArgDescriptions::eExcludes,
                           prefix + "-path");

    if (prefix == "input") {
        arg_desc.AddAlias("I", "input-manifest");
    }
}

vector<string> CInputStreamSource::RecreateInputArgs(const CArgs& args, const string &prefix)
{
    vector<string> result;
    if (args[prefix + "-path"].HasValue()) {
        result.push_back("-" + prefix + "-path");
        result.push_back(args[prefix + "-path"].AsString());
        if (args[prefix + "-mask"]) {
            result.push_back("-" + prefix + "-mask");
            result.push_back(args[prefix + "-mask"].AsString());
        }
    }
    else if (args[prefix + "-manifest"].HasValue()) {
        result.push_back("-" + prefix + "-manifest");
        result.push_back(args[prefix + "-manifest"].AsString());
    }
    else {
        result.push_back("-" + prefix);
        result.push_back(args[prefix].AsString());
    }
    return result;
}


CInputStreamSource::CInputStreamSource()
    : m_Istr(NULL), m_CurrIndex(0)
{
}


CInputStreamSource::CInputStreamSource(const CArgs& args, const string& prefix)
    : m_Istr(NULL), m_CurrIndex(0)
{
    InitArgs(args, prefix);
}

void CInputStreamSource::InitArgs(const CArgs& args, const string &prefix)
{
    m_Args.Assign(args);
    m_Prefix = prefix;

    if (m_Args[prefix + "-path"].HasValue()) {
        string path = m_Args[prefix + "-path"].AsString();
        string mask;
        if (m_Args[prefix + "-mask"]) {
            mask = m_Args[prefix + "-mask"].AsString();
        }
        InitFilesInDirSubtree(path, mask);
    }
    else if (m_Args[prefix + "-manifest"].HasValue()) {
        InitManifest(m_Args[prefix + "-manifest"].AsString());
    }
    else if (m_Args[prefix].HasValue() && m_Args[prefix].AsString() == "-") {
        /// NOTE: this is ignored if either -input-path or -input-mask is
        /// provided
        InitStream(m_Args[prefix].AsInputFile(), m_Args[prefix].AsString());
    }
    else if (m_Args[prefix].HasValue()) {
        /// Input file; init as input file, so it can be opened multiple times
        InitFile(m_Args[prefix].AsString());
    }
}


/// Initialize from a given stream which is the sole content.
/// As precondition, expect that the stream is in a good condition
/// prior to being handed off to consumers.
///
void CInputStreamSource::InitStream(CNcbiIstream& istr, const string& fname)
{
    if (m_Istr  ||  ! m_Files.empty()) {
        NCBI_THROW(CException, eUnknown,
                   "CInputStreamSource::InitManifest(): "
                   "attempt to init already initted class");
    }
    if (! istr) {
        NCBI_THROW(CException, eUnknown,
                   "CInputStreamSource::InitStream(): "
                   "stream is bad");
    }
    m_Istr = &istr;
    m_CurrFile = fname;
    m_CurrIndex = 0;
}


/// Initialize from a single file path.
///
void CInputStreamSource::InitFile(const string& file_path)
{
    if (m_Istr  ||  ! m_Files.empty()) {
        NCBI_THROW(CException, eUnknown,
                   "CInputStreamSource::InitFile(): "
                   "attempt to init already initted class");
    }

    /**
     * commented out: this breaks stream processing
    if ( !CFile(file_path).Exists() ) {
        NCBI_THROW(CException, eUnknown,
                   "input file " + file_path + " does not exist");
    }
    **/

    m_Files.push_back(file_path);
    Rewind();
}


/// Initialize from a manifest file.
///
/// @see CFileManifest
void CInputStreamSource::InitManifest(const string& manifest)
{
    if (m_Istr  || !  m_Files.empty()) {
        NCBI_THROW(CException, eUnknown,
                   "CInputStreamSource::InitManifest(): "
                   "attempt to init already initted class");
    }

    CFileManifest src(manifest);
    vector<string> all(src.GetAllFilePaths());
    std::copy( all.begin(), all.end(), std::back_inserter(m_Files));

    _TRACE("Added " << m_Files.size() << " files from input manifest");

    Rewind();
}


/// Initialize from a file search path
///
void CInputStreamSource::InitFilesInDirSubtree(const string& file_path,
                                               const string& file_mask)
{
    if (m_Istr  ||  ! m_Files.empty()) {
        NCBI_THROW(CException, eUnknown,
                   "CInputStreamSource::InitFilesInDirSubtree(): "
                   "atemmpt to init already initted class");
    }

    CDir d(file_path);
    if ( !d.Exists() ) {
        NCBI_THROW(CException, eUnknown,
                   "input directory " + file_path + " does not exist");
    }

    vector<string> paths;
    paths.push_back(file_path);

    vector<string> masks;
    if ( !file_mask.empty() ) {
        masks.push_back(file_mask);
    } else {
        masks.push_back("*");
    }

    FindFiles(m_Files,
              paths.begin(), paths.end(),
              masks.begin(), masks.end(),
              fFF_File | fFF_Recursive);
    _TRACE("Added " << m_Files.size() << " files from input path");

    Rewind();
}


CNcbiIstream& CInputStreamSource::GetStream(string* fname)
{
    if (m_Istr) {
        if (fname) {
            *fname = m_CurrFile;
        }
        return *m_Istr;
    }

    if (m_IstrOwned.get()) {
        if (fname) {
            *fname = m_CurrFile;
        }
        return *m_IstrOwned;
    }

    NCBI_THROW(CException, eUnknown, "All input streams consumed");
}


CNcbiIstream& CInputStreamSource::operator*()
{
    return GetStream();
}


CInputStreamSource& CInputStreamSource::operator++()
{
    // The next stream can be held in either of two places. Clear both.

    // Clear first place.
    if (m_Istr) {
        if (m_Istr->bad()) {
            // Check that the stream, at the end, didn't go bad as might
            // happen if there was a disk read error. On the other hand,
            // ok if it has failbit set so ignore that, e.g. getline sets
            // failbit at the last line, if it has a teminator.
            NCBI_THROW(CException, eUnknown,
                       "CInputStreamSource::operator++(): "
                       "Unknown error in input stream, "
                       "which is in a bad state after use");
        }
        m_Istr = NULL;
    }

    // Clear second place.
    if (m_IstrOwned.get()) {
        if (m_IstrOwned->bad()) {
            // Samecheck  as for m_Istr.
            string msg("CInputStreamSource::operator++(): "
                       "Unknown error reading file, "
                       "which is in a bad state after use: ");
            NCBI_THROW(CException, eUnknown, msg + m_CurrFile);
        }
        m_IstrOwned.reset();
    }

    // The current filename currently applies to only the first source,
    // but someday might apply to others, so clear it here rather than
    // inside the above conditionals.
    m_CurrFile.erase();

    // Advance to the next stream, if there is any.
    if (m_CurrIndex < m_Files.size()) {
        m_CurrFile = m_Files[m_CurrIndex++];
        m_IstrOwned.reset(new CNcbiIfstream(m_CurrFile.c_str()));
        if (m_IstrOwned->fail()) {
            // Do not provide to clients with streams that are already
            // known not to be good (fail, meaning badbit or failbit).
            string msg("CInputStreamSource::operator++(): "
                       "File is not accessible: ");
            NCBI_THROW(CException, eUnknown, msg + m_CurrFile);
        }
    }
    return *this;
}

CInputStreamSource& CInputStreamSource::Rewind(void)
{
    m_CurrIndex = 0;
    ++(*this);
    return *this;
}

string CInputStreamSource::GetCurrFileName(void) const
{
    return m_CurrFile;
}

size_t CInputStreamSource::GetCurrFileIndex(size_t* count) const
{
    if (count) {
        *count = m_Files.size();
    }
    return m_CurrIndex;
}

CInputStreamSource::operator bool()
{
    // The stream contains data if it references a stream (given on input)
    // owns a stream (extracted from a manifest), or still has a non-empty
    // queued list of files.
    return (m_Istr  ||  m_IstrOwned.get()  ||  m_CurrIndex < m_Files.size());
}



END_NCBI_SCOPE

