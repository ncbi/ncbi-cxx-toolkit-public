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
* Author: Anton Golikov
*
* File Description: Resolve host name to ip address using preset ini-file
*
* ===========================================================================
*/
#ifndef SMALLDNS__HPP
#define SMALLDNS__HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

// Validate if "ipname" contains a legal IP address in a dot notation
// (a la "255.255.255.255").
extern bool IsValidIP(const string& ipname);

// Given host name "hostname", return its IP address in dot notation.
// If the "hostname" is not in the dot notation, then look it up in
// the registry-like file named by "local_hosts_file" (default -- './hosts.ini')
// in section "[LOCAL_DNS]".
// Return empty string if "hostname" cannot be resolved
// using the "local_hosts_file" file.
extern string LocalResolveDNS(const string& hostname,
                              const string& local_hosts_file = kEmptyStr);

END_NCBI_SCOPE

#endif  /* SMALLDNS__HPP */
