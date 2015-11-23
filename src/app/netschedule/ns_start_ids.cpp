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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetSchedule queue job start identifiers
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>

#include "ns_start_ids.hpp"
#include "ns_types.hpp"


BEGIN_NCBI_SCOPE



CNSStartIDs::CNSStartIDs(const string &  data_dir_name)
{
    m_FileName = CFile::MakePath(data_dir_name, kStartJobIDsFileName);
}


CNSStartIDs::~CNSStartIDs()
{}


void CNSStartIDs::Set(const string &  qname, unsigned int  value)
{
    TStartIDs::iterator     k;
    CFastMutexGuard         guard(m_Lock);

    k = m_IDs.find(qname);
    if (k != m_IDs.end()) {
        if (k->second != value) {
            k->second = value;
            x_SerializeNoLock();
        }
    } else {
        // Create a new entry and serialize the file
        m_IDs[ qname ] = value;
        x_SerializeNoLock();
    }
}


unsigned int CNSStartIDs::Get(const string &  qname)
{
    TStartIDs::const_iterator       k;
    CFastMutexGuard                 guard(m_Lock);

    k = m_IDs.find(qname);
    if (k != m_IDs.end())
        return k->second;

    // Create a new entry and serialize the file
    m_IDs[ qname ] = 1;
    x_SerializeNoLock();
    return 1;
}


void CNSStartIDs::Serialize(void) const
{
    CFastMutexGuard     guard(m_Lock);
    x_SerializeNoLock();
}


void CNSStartIDs::Load(void)
{
    // Loading is done at the startup time when there is no threaded access
    FILE *      f = fopen(m_FileName.c_str(), "r");
    if (f == NULL)
        return;

    char *      raw_line = NULL;
    size_t      raw_length = 0;
    ssize_t     read_size = 0;

    while ((read_size = getline(&raw_line, &raw_length, f)) != -1) {
        if (read_size == 0)
            continue;

        if (raw_line[read_size - 1] == '\n')
            --read_size;

        string      s(raw_line, read_size);
        s = NStr::TruncateSpaces(s);
        if (s.length() == 0)
            continue;
        if (s[0] == ';' || s[0] == '#')
            continue;

        // This is a meaningfull line
        list<string>        tokens;
        NStr::Split(s, "=", tokens, NStr::fSplit_NoMergeDelims);
        if (tokens.size() != 2) {
            ERR_POST("Poor format of the " + kStartJobIDsFileName +
                     " file. Line: " + s + ". Ignore and continue.");
            continue;
        }
        list<string>::const_iterator    item = tokens.begin();
        string                          qname = *item;
        ++item;
        m_IDs[qname] = NStr::StringToUInt(*item);
    }

    if (raw_line != NULL)
        free(raw_line);
    fclose(f);
}


void CNSStartIDs::x_SerializeNoLock(void) const
{
    TStartIDs::const_iterator       k;
    FILE *                          f = fopen(m_FileName.c_str(), "w");

    if (f != NULL) {
        for (k = m_IDs.begin(); k != m_IDs.end(); ++k)
            fprintf(f, "%s=%d\n", k->first.c_str(), k->second);
        fclose(f);
    } else
        ERR_POST("Cannot serialize start queue job ids into file " +
                 m_FileName);
}

END_NCBI_SCOPE

