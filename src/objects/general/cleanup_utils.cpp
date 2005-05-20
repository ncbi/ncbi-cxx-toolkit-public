/* $Id$
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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   General utilities for data cleanup.
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/cleanup_utils.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CleanString(string& str)
{
    NStr::TruncateSpacesInPlace(str, NStr::eTrunc_Begin);
    while (!str.empty()) {
        NStr::TruncateSpacesInPlace(str, NStr::eTrunc_End);
        if (str[str.length() - 1] == ';') {
            bool remove = true;
            size_t semicolon = str.length() - 1;
            size_t amp = str.find_last_of('&');
            if (amp != NPOS) {
                remove = false;
                for (size_t i = amp + 1; i < semicolon; ++i) {
                    if (isspace(str.data()[i])) {
                        remove = true;
                        break;
                    }
                }
            }
            if (remove) {
                str.resize(semicolon);
            } else {
                break;
            }
        } else {
            break;
        }
    }
}


void RemoveSpaces(string& str)
{
    if (str.empty()) {
        return;
    }

    size_t next = 0;

    NON_CONST_ITERATE(string, it, str) {
        if (!isspace(*it)) {
            str[next++] = *it;
        }
    }
    if (next < str.length()) {
        str.resize(next);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 6.1  2005/05/20 13:31:29  shomrat
* Added BasicCleanup()
*
*
* ===========================================================================
*/
