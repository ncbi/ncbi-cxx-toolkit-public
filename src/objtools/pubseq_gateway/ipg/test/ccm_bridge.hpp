#ifndef TESTS__STORAGE__CCM_BRIDGE_HPP
#define TESTS__STORAGE__CCM_BRIDGE_HPP
/* ===========================================================================
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
* Author:  Dmitrii Saprykin, NCBI
*
* ===========================================================================
*/

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiexpt.hpp>

#include <cstdio>

BEGIN_NCBI_SCOPE

class CCCMBridge final
{
public:
    static unique_ptr<CNcbiRegistry> GetConfigRegistry(string const& config_basename)
    {
        char * config_dir = getenv("INI_CONFIG_LOCATION");
        if (!config_dir) {
            return nullptr;
        }
        string config = string(config_dir) + config_basename;
        filebuf fb;
        fb.open(config.c_str(), ios::in | ios::binary);
        CNcbiIstream i(&fb);
        auto r = make_unique<CNcbiRegistry>(i, 0);
        fb.close();

        string section = "CASSANDRA";
        r->Set(section, "service", GetClusterConnection(), IRegistry::fPersistent);
        return r;
    }

private:
    static string GetClusterConnection()
    {
        string address_list;
        char * address = getenv("CASSANDRA_CLUSTER");
        if (address) {
            address_list.assign(address);
        }
        return address_list + "," + address_list;
    }
};

END_NCBI_SCOPE

#endif  // TESTS__STORAGE__CCM_BRIDGE_HPP
