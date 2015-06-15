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
 * File Description: NetStorage server timing
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include "nst_timing.hpp"


BEGIN_NCBI_SCOPE


void CNSTTiming::Append(const string &  what,
                        const CNSTPreciseTime &  start)
{
    data.push_back(make_pair(what, CNSTPreciseTime::Current() - start));
}


// Automatically cleans the collected data to avoid double printing
string CNSTTiming::Serialize(void)
{
    string      retval;
    for ( vector< pair< string, CNSTPreciseTime > >::const_iterator
            k = data.begin(); k != data.end(); ++k ) {
        if (!retval.empty())
            retval += " ";
        retval += k->first + ": " + NST_FormatPreciseTimeAsSec(k->second);
    }

    data.clear();
    return retval;
}

string CNSTTiming::Serialize(CDiagContext_Extra  extra)
{
    string  retval;
    for ( vector< pair< string, CNSTPreciseTime > >::const_iterator
            k = data.begin(); k != data.end(); ++k ) {
        if (NStr::StartsWith(k->first, "MS SQL")) {
            extra.Print(NStr::Replace(k->first, " ", "_"), k->second);
            continue;
        }
        if (!retval.empty())
            retval += " ";
        retval += k->first + ": " + NST_FormatPreciseTimeAsSec(k->second);
    }

    data.clear();
    return retval;
}


bool  CNSTTiming::Empty(void) const
{
    return data.empty();
}


size_t  CNSTTiming::Size(void) const
{
    return data.size();
}


void  CNSTTiming::Clear(void)
{
    data.clear();
}


END_NCBI_SCOPE

