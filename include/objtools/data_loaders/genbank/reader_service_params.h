#ifndef GBLOADER__READER_SERVICE_PARAMS__H_INCLUDED
#define GBLOADER__READER_SERVICE_PARAMS__H_INCLUDED

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
*    GenBank reader id1/id2 common configuration parameters
*
* ===========================================================================
*/

/* Timeout of network connections in seconds */
#define NCBI_GBLOADER_READER_PARAM_TIMEOUT "timeout"
/* Timeout of network connections in seconds while opening and initializing */
#define NCBI_GBLOADER_READER_PARAM_OPEN_TIMEOUT "open_timeout"
/* Open timeout multiplier in case of errors */
#define NCBI_GBLOADER_READER_PARAM_OPEN_TIMEOUT_MULTIPLIER "open_timeout_multiplier"
#define NCBI_GBLOADER_READER_PARAM2_OPEN_TIMEOUT_MULTIPLIER "open_multiplier"
/* Open timeout increment in case of errors */
#define NCBI_GBLOADER_READER_PARAM_OPEN_TIMEOUT_INCREMENT "open_timeout_increment"
#define NCBI_GBLOADER_READER_PARAM2_OPEN_TIMEOUT_INCREMENT "open_increment"
/* Maximal open timeout of network connections in seconds */
#define NCBI_GBLOADER_READER_PARAM_OPEN_TIMEOUT_MAX "open_timeout_max"
#define NCBI_GBLOADER_READER_PARAM2_OPEN_TIMEOUT_MAX "open_max"

#endif
