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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Special FTP download test
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <util/compress/stream_util.hpp>
#include <util/compress/tar.hpp>


static const char kNcbiTestLocation[] =
    "ftp://ftp:-none@ftp.ncbi.nlm.nih.gov/toolbox/ncbi_tools/ncbi.tar.gz";


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;

    CONNECT_Init(0);
   
    SConnNetInfo* net_info = ConnNetInfo_Create(0);

    const char* url = argc > 1 ? argv[1] : kNcbiTestLocation;

    if (!ConnNetInfo_ParseURL(net_info, url)) {
        ERR_POST(Fatal << "Cannot parse URL \"" << url << '"');
    }
    if (net_info->scheme != eURL_Ftp) {
        ERR_POST(Fatal << "URL scheme must be FTP");
    }
    if (!*net_info->user) {
        ERR_POST(Warning << "Username not provided, defaulted to `ftp'");
        strcpy(net_info->user, "ftp");
    }
    CConn_FTPDownloadStream ftp(net_info->host,
                                net_info->path,
                                net_info->user,
                                net_info->pass, kEmptyStr/*path*/,
                                net_info->port,
                                net_info->debug_printout
                                == eDebugPrintout_Data
                                ? fFCDC_LogAll :
                                net_info->debug_printout
                                == eDebugPrintout_Some
                                ? fFCDC_LogControl
                                : 0);
    ConnNetInfo_Destroy(net_info);

    CDecompressIStream is(ftp, CCompressStream::eGZipFile);
    CTar tar(is);

    auto_ptr<CTar::TEntries> filelist;
    try {
        filelist.reset(tar.List().release());
    } NCBI_CATCH_ALL("TAR error");

    if (!filelist->empty()) {
        ITERATE(CTar::TEntries, it, *filelist.get()) {
            LOG_POST(*it);
        }
    } else {
        ERR_POST(Warning << "File list is emtpy");
    }

    return 0;
}
