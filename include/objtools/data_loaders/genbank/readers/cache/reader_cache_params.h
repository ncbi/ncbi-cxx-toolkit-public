#ifndef GBLOADER_CACHE_PARAMS__HPP_INCLUDED
#define GBLOADER_CACHE_PARAMS__HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Eugene Vasilchenko
*
*  File Description:
*    GenBank cache configuration parameters
*
* ===========================================================================
*/

/* Name of cache reader driver */
#define NCBI_GBLOADER_READER_CACHE_DRIVER_NAME "cache"

/* Name of sub section with parameter of ICache driver for id cache */
#define NCBI_GBLOADER_READER_CACHE_PARAM_ID_SECTION "id_cache"
/* Name of sub section with parameter of ICache driver for blob cache */
#define NCBI_GBLOADER_READER_CACHE_PARAM_BLOB_SECTION "blob_cache"
/* Name of ICache driver to be used in cache */
#define NCBI_GBLOADER_READER_CACHE_PARAM_DRIVER "driver"

/* Name of cache writer driver */
#define NCBI_GBLOADER_WRITER_CACHE_DRIVER_NAME "cache"

/* Name of sub section with parameter of ICache driver for id cache */
#define NCBI_GBLOADER_WRITER_CACHE_PARAM_ID_SECTION "id_cache"
/* Name of sub section with parameter of ICache driver for blob cache */
#define NCBI_GBLOADER_WRITER_CACHE_PARAM_BLOB_SECTION "blob_cache"
/* Name of ICache driver to be used in cache */
#define NCBI_GBLOADER_WRITER_CACHE_PARAM_DRIVER "driver"

#endif
