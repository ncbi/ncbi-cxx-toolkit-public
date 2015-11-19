#ifndef GBLOADER_PUBSEQ2_PARAMS__HPP_INCLUDED
#define GBLOADER_PUBSEQ2_PARAMS__HPP_INCLUDED

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
*    GenBank ID2 via PubSeqOS reader configuration parameters
*
* ===========================================================================
*/

/* Name of PUBSEQ2 reader driver */
#define NCBI_GBLOADER_READER_PUBSEQ2_DRIVER_NAME "pubseqos2"

/* PUBSEQ_OS server name */
#define NCBI_GBLOADER_READER_PUBSEQ2_PARAM_SERVER   "server"
/* PUBSEQ_OS login name */
#define NCBI_GBLOADER_READER_PUBSEQ2_PARAM_USER     "user"
/* PUBSEQ_OS login password */
#define NCBI_GBLOADER_READER_PUBSEQ2_PARAM_PASSWORD "password"
/* DBAPI driver name */
#define NCBI_GBLOADER_READER_PUBSEQ2_PARAM_DRIVER   "driver"
/* exclude WGS master descriptors */
#define NCBI_GBLOADER_READER_PUBSEQ2_PARAM_EXCL_WGS_MASTER "exclude_wgs_master"
/* timeout for requests */
#define NCBI_GBLOADER_READER_PUBSEQ2_PARAM_TIMEOUT  "timeout"
/* timeout for initial request */
#define NCBI_GBLOADER_READER_PUBSEQ2_PARAM_OPEN_TIMEOUT "open_timeout"

#endif
