#ifndef WRITER_CACHE_ENTRY__HPP_INCLUDED
#define WRITER_CACHE_ENTRY__HPP_INCLUDED

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
*
*  File Description: Cached extension of data reader from ID1
*
*/

#include <objtools/data_loaders/genbank/writer_interface.hpp>

BEGIN_NCBI_SCOPE

extern "C" 
{

NCBI_XREADER_CACHE_EXPORT
void NCBI_EntryPoint_CacheWriter(
     CPluginManager<objects::CWriter>::TDriverInfoList&   info_list,
     CPluginManager<objects::CWriter>::EEntryPointRequest method);

NCBI_XREADER_CACHE_EXPORT
void NCBI_EntryPoint_xwriter_cache(
     CPluginManager<objects::CWriter>::TDriverInfoList&   info_list,
     CPluginManager<objects::CWriter>::EEntryPointRequest method);

} // extern C

END_NCBI_SCOPE

#endif//WRITER_CACHE_ENTRY__HPP_INCLUDED
