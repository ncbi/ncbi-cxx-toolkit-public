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
 * Authors:  Victor Joukov
 *
 * File Description: Utility functions for NetSchedule
 *
 */

#include <ncbi_pch.hpp>

#include "ns_util.hpp"
#include "ns_queue.hpp"
#include <util/bitset/bmalgo.h>
#include <connect/ncbi_socket.hpp>


BEGIN_NCBI_SCOPE


void    NS_ValidateConfigFile(const CNcbiRegistry &  reg)
{
    // Walk all the qclass_... and queue_... sections and check that
    // they do not have dangling references to the NC sections
    list<string>        sections;

    reg.EnumerateSections(&sections);
    ITERATE(list<string>, it, sections) {
        string              queue_or_class;
        string              prefix;
        const string &      section_name = *it;

        NStr::SplitInTwo(section_name, "_", prefix, queue_or_class);
        if (queue_or_class.empty())
            continue;
        if (NStr::CompareNocase(prefix, "qclass") == 0 ||
            NStr::CompareNocase(prefix, "queue") == 0) {
            // This section should be checked
            string      ref_section = reg.GetString(section_name,
                                                    "netcache_api", kEmptyStr);
            if (ref_section.empty())
                continue;

            // It refers to another section. Check that it exists.
            if (find(sections.begin(), sections.end(), ref_section) ==
                sections.end())
                NCBI_THROW(CNetScheduleException, eInvalidParameter,
                           "Value [" + section_name +
                           "].netcache_api refers to non-existing section '" +
                           ref_section + "'.");
        }
    }
}

END_NCBI_SCOPE

