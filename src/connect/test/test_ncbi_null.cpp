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
 *   HTTP submission w/soaking up the response, showing overall status only
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/rwstream.hpp>
#include <connect/ncbi_conn_stream.hpp>


BEGIN_NCBI_SCOPE


class CNullWriter : public IWriter
{
public:
    virtual ERW_Result Write(const void* /*buf*/,
                             size_t      count,
                             size_t*     bytes_written = 0)
    {
        if (bytes_written)
            *bytes_written = count;
        return eRW_Success;
    }

    virtual ERW_Result Flush(void) { return eRW_Success; }
};


EIO_Status Soaker(const string& url, const string& data,
                  const STimeout* read_tmo,
                  const STimeout* write_tmo)
{
    static const STimeout kZeroTimeout = { 0 };

    CConn_HttpStream http(url, fHTTP_AutoReconnect, write_tmo);
    http.SetTimeout(eIO_Close, read_tmo);
    http.SetTimeout(eIO_Read, read_tmo);
    http << data << flush;

    EIO_Status status = CONN_Wait(http.GetCONN(), eIO_Read, &kZeroTimeout);
    if (status != eIO_Success) {
        if (status != eIO_Timeout)
            return status;
        status = http.Status(eIO_Write);
        if (status != eIO_Success)
            return status;
    }

    CWStream null(new CNullWriter, 0, 0, CRWStreambuf::fOwnWriter);
    NcbiStreamCopy(null, http);

    status = http.Status();
    if (status != eIO_Success  &&  status != eIO_Closed)
        return status;
    status = http.Status(eIO_Read);
    if (status != eIO_Closed)
        return status;
    return eIO_Success;
}


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    // Setup error posting
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Trace);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    // Init the library explicitly (this sets up the log)
    CONNECT_Init(0);

    STimeout tmo = { 5, 0 };
    EIO_Status status = Soaker(argv[1], "Hello", &tmo, &tmo);
    cout << "Status = " << IO_StatusStr(status) << endl;

    return !(status == eIO_Success);
}
