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
 * File Description: Utility functions for NetStorage
 *
 */

#include <ncbi_pch.hpp>

#include "nst_util.hpp"
#include <util/checksum.hpp>

#include <unistd.h>


BEGIN_NCBI_SCOPE


static const string     s_ErrorGettingChecksum = "error detected";


string NST_GetConfigFileChecksum(const string &  file_name,
                                 vector<string> &  warnings)
{
    // Note: at the time of writing the ComputeFileCRC32() function does not
    //       generate exceptions at least in some error cases e.g. if there
    //       is no file. Therefore some manual checks are introduced here:
    //       - the file exists
    //       - there are read permissions for it
    // Technically speaking it is not 100% guarantee because the file could be
    // replaced between the checks and the sum culculation but it is better
    // than nothing.

    if (access(file_name.c_str(), F_OK) != 0) {
        warnings.push_back("Error computing config file checksum, "
                           "the file does not exist: " + file_name);
        return s_ErrorGettingChecksum;
    }

    if (access(file_name.c_str(), R_OK) != 0) {
        warnings.push_back("Error computing config file checksum, "
                           "there are no read permissions: " + file_name);
        return s_ErrorGettingChecksum;
    }

    try {
        string      checksum_as_string;
        CChecksum   checksum(CChecksum::eMD5);

        checksum.AddFile(file_name);
        checksum.GetMD5Digest(checksum_as_string);
        return checksum_as_string;
    } catch (const exception &  ex) {
        warnings.push_back("Error computing config file checksum. " +
                           string(ex.what()));
        return s_ErrorGettingChecksum;
    } catch (...) {
        warnings.push_back("Unknown error of computing config file checksum");
        return s_ErrorGettingChecksum;
    }
    return s_ErrorGettingChecksum;
}


END_NCBI_SCOPE

