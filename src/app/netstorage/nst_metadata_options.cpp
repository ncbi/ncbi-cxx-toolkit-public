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
 * File Description: Metadata options
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "nst_metadata_options.hpp"


BEGIN_NCBI_SCOPE


struct MetadataOptionToId
{
    enum EMetadataOption    option;
    string                  id;
};


const MetadataOptionToId    metadataOptionToIdMap[] = {
                                { eMetadataNotSpecified, "not set" },
                                { eMetadataRequired,     "required" },
                                { eMetadataDisabled,     "disabled" },
                                { eMetadataMonitoring,   "monitoring" }
                                                      };

const size_t    metadataOptionToIdMapSize = sizeof(metadataOptionToIdMap) /
                                            sizeof(MetadataOptionToId);



enum EMetadataOption  g_IdToMetadataOption(const string &  option_id)
{
    for (size_t  k = 0; k < metadataOptionToIdMapSize; ++k) {
        if (NStr::CompareNocase(option_id, metadataOptionToIdMap[k].id) == 0)
            return metadataOptionToIdMap[k].option;
    }
    return eMetadataUnknown;
}


string  g_MetadataOptionToId(enum EMetadataOption  option)
{
    for (size_t  k = 0; k < metadataOptionToIdMapSize; ++k) {
        if (metadataOptionToIdMap[k].option == option)
            return metadataOptionToIdMap[k].id;
    }
    return "unknown";
}


vector<string>  g_GetSupportedMetadataOptions(void)
{
    vector<string>      result;
    for (size_t  k = 0; k < metadataOptionToIdMapSize; ++k)
        result.push_back(metadataOptionToIdMap[k].id);
    return result;
}

END_NCBI_SCOPE


