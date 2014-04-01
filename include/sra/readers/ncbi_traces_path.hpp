#ifndef SRA__READER__NCBI_TRACES_PATH__HPP
#define SRA__READER__NCBI_TRACES_PATH__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Standard path to NCBI SRA & BAM files.
 *
 */

#include <ncbiconf.h>

#ifdef NCBI_OS_DARWIN
# define PANFS_TRACES_PATH(num)  "/net/traces" num
# define NETAPP_TRACES_PATH(num) "/net/traces" num
#elif defined(NCBI_OS_MSWIN)
# define PANFS_TRACES_PATH(num)  "//panfs/traces" num
# define NETAPP_TRACES_PATH(num) "//traces" num
#else
# define PANFS_TRACES_PATH(num)  "/panfs/traces" num ".be-md.ncbi.nlm.nih.gov"
# define NETAPP_TRACES_PATH(num) "/netmnt/traces" num
#endif

#define NCBI_TRACES01_PATH PANFS_TRACES_PATH("01")
#define NCBI_TRACES02_PATH PANFS_TRACES_PATH("02")
#define NCBI_TRACES03_PATH PANFS_TRACES_PATH("03")
#define NCBI_TRACES04_PATH NETAPP_TRACES_PATH("04")

#define NCBI_SRA_REP_PATH NCBI_TRACES04_PATH ":" NCBI_TRACES01_PATH
#define NCBI_SRA_VOL_PATH "sra18:sra17:sra16:sra15:sra14:sra13:sra12:sra11:sra10:sra9:sra8:sra7:sra6:sra5:sra4:sra3:sra2:sra1:sra0:era8:era7:era6:era5:era4:era3:era2:era1:era0:dra0:refseq"

#define NCBI_SRZ_REP_PATH NCBI_TRACES04_PATH ":" NCBI_TRACES01_PATH
#define NCBI_SRZ_VOL_PATH "sra8:srz0"

#ifdef NCBI_OS_DARWIN
# define NCBI_TEST_TRACES_PATH(num) "/netopt/toolkit_test_data/traces" num ":/net/snowman/vol/projects/toolkit_test_data/traces" num
#elif defined(NCBI_OS_MSWIN)
# define NCBI_TEST_TRACES_PATH(num) "//snowman/toolkit_test_data/traces" num
#else
# define NCBI_TEST_TRACES_PATH(num) "/net/snowman/vol/projects/toolkit_test_data/traces" num
#endif

#define NCBI_TEST_BAM_FILE_PATH NCBI_TEST_TRACES_PATH("02") ":" NCBI_TEST_TRACES_PATH("04")

#endif // SRA__READER__NCBI_TRACES_PATH__HPP
