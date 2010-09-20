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
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_util.h>
#include <util/compress/stream_util.hpp>
#include <util/compress/tar.hpp>
#include <string.h>
#ifdef NCBI_OS_UNIX
#  include <signal.h>
#  include <unistd.h>  // isatty()
#endif // NCBI_OS_UNIX


BEGIN_NCBI_SCOPE


static const char kDefaultTestURL[] =
    "ftp://ftp:-none@ftp.ncbi.nlm.nih.gov/toolbox/ncbi_tools/ncbi.tar.gz";


static bool s_Signaled  = false;
static int  s_Throttler = 0;


#ifdef NCBI_OS_UNIX
extern "C" {
static void s_Interrupt(int /*signo*/)
{
    s_Signaled = 1;
}
}
#endif // NCBI_OS_UNIX


class CDownloadCallbackData {
public:
    CDownloadCallbackData(size_t size)
        : m_Sw(CStopWatch::eStart), m_Size(size), m_TotalSize(0)
    { memset(&m_Cb, 0, sizeof(m_Cb)); }
    SCONN_Callback*       CB(void)       { return &m_Cb;          }
    const SCONN_Callback& CB(void) const { return m_Cb;           }
    size_t           GetSize(void) const { return m_Size;         }
    double        GetElapsed(void) const { return m_Sw.Elapsed(); }
    void        AddTotalSize(size_t add) { m_TotalSize += add;    }
    size_t      GetTotalSize(void) const { return m_TotalSize;    }

private:
    CStopWatch     m_Sw;
    SCONN_Callback m_Cb;
    size_t         m_Size;
    size_t         m_TotalSize;
};


extern "C" {
static void s_FtpCallback(CONN conn, ECONN_Callback type, void* data)
{
    if (type != eCONN_OnClose  &&  !s_Signaled) {
        // Reinstate the callback right away
        SCONN_Callback cb = { s_FtpCallback, data };
        CONN_SetCallback(conn, type, &cb, 0);
    }

    const CDownloadCallbackData* dlcbdata
        = reinterpret_cast<const CDownloadCallbackData*>(data);
    size_t pos  = CONN_GetPosition(conn, eIO_Read);
    size_t size = dlcbdata->GetSize();
    double time = dlcbdata->GetElapsed();

#ifdef NCBI_OS_UNIX
    if (!isatty(STDERR_FILENO)) {
        return;
    }
#endif // NCBI_OS_UNIX

    cout.flush();
    if (!size) {
        cerr << "Downloaded " << pos << "/unknown"
            " in " << fixed << setprecision(2) << time
             << left << setw(16) << 's' << '\r' << flush;
    } else {
        double percent = (pos * 100.0) / size;
        cerr << "Downloaded " << pos << '/' << size
             << " (" << fixed << setprecision(2) << percent << "%)"
            " in " << fixed << setprecision(2) << time;
        if (time) {
            double kbps = pos / time / 1024.0;
            cerr << "s (" << fixed << setprecision(2) << kbps
                 << left << setw(16) << "KB/s)" << '\r' << flush;
        } else {
            cerr << left << setw(16) << 's'     << '\r' << flush;
        }
    }
    if (type == eCONN_OnClose) {
        if (dlcbdata->CB().func)
            dlcbdata->CB().func(conn, type, dlcbdata->CB().data);
        cerr << endl;
    } else if (s_Signaled) {
        CONN_Cancel(conn);
        cerr << endl << "Canceled" << endl;
    } else if (s_Throttler) {
        SleepMilliSec(s_Throttler);
    }
}
}


class CTarCheckpointed : public CTar {
public:
    CTarCheckpointed(istream& is, CDownloadCallbackData* dlcbdata)
        : CTar(is), m_DlCbData(dlcbdata) { }

protected:
    virtual bool Checkpoint(const CTarEntryInfo& current, bool /**/) const
    {
        m_DlCbData->AddTotalSize(current.GetSize());
        cerr.flush();
        cout << current << endl;
        return true;
    }

private:
    CDownloadCallbackData* m_DlCbData;
};


static size_t s_GetFtpFilesize(iostream& ios, const char* filename)
{
    size_t retval;
    ios << "SIZE " << filename << endl;
    if (!(ios >> retval)) {
        retval = 0;
    }
    ios.clear();
    return retval;
}


static void s_InitiateFtpFileDownload(iostream& ios, const char* filename)
{
    ios << "RETR " << filename << endl;
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;

    // Setup error posting
    SetDiagPostAllFlags(eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Info);

    CONNECT_Init(0);

    const char* url = argc > 1  &&  *argv[1] ? argv[1] : kDefaultTestURL;
    if (argc > 2) {
        s_Throttler = NStr::StringToInt(argv[2]);
    }

    SConnNetInfo* net_info = ConnNetInfo_Create(0);

    if (!ConnNetInfo_ParseURL(net_info, url)) {
        ERR_POST(Fatal << "Cannot parse URL \"" << url << '"');
    }
    if (net_info->scheme != eURL_Ftp) {
        ERR_POST(Fatal << "URL scheme must be FTP (ftp://)");
    }
    if (!*net_info->user) {
        ERR_POST(Warning << "Username not provided, defaulted to `ftp'");
        strcpy(net_info->user, "ftp");
    }
    CConn_FTPDownloadStream ftp(net_info->host, kEmptyStr/*file*/,
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
    size_t size = s_GetFtpFilesize(ftp, net_info->path);
    if (size) {
        ERR_POST(Info << "Downloading " << net_info->path
                 << " from " << net_info->host << ", "
                 << size << " byte" << &"s"[size == 1]);
    } else {
        ERR_POST(Info << "Downloading " << net_info->path
                 << " from " << net_info->host);
    }
    s_InitiateFtpFileDownload(ftp, net_info->path);

    ConnNetInfo_Destroy(net_info);

#ifdef NCBI_OS_UNIX
    signal(SIGINT,  s_Interrupt);
    signal(SIGTERM, s_Interrupt);
    signal(SIGQUIT, s_Interrupt);
#endif // NCBI_OS_UNIX
    // Reset I/O positions
    CONN_GetPosition(ftp.GetCONN(), eIO_Open);
    // Set both read and close callbacks
    CDownloadCallbackData dlcbdata(size);
    SCONN_Callback cb = { s_FtpCallback, &dlcbdata };
    CONN_SetCallback(ftp.GetCONN(), eCONN_OnRead,  &cb, 0);
    CONN_SetCallback(ftp.GetCONN(), eCONN_OnClose, &cb, dlcbdata.CB());
    // Build on-the-fly ungzip stream on top of ftp
    CDecompressIStream is(ftp, CCompressStream::eGZipFile);
    // Build streaming [un]tar on top of uncompressed data
    CTarCheckpointed tar(is, &dlcbdata);

    auto_ptr<CTar::TEntries> filelist;
    try {
        // NB: CTar *loves* exceptions, for some weird reason :-/
        filelist.reset(tar.List().release());  // can be tar.Extract() as well
    } NCBI_CATCH_ALL("TAR Error");

    // These should not matter, and can be issued in any order
    ftp.Close();  // ...so "wrong" order on purpose!
    tar.Close();

    // Conclude the test
    int exitcode;
    if (!(size = filelist.get() ? filelist->size() : 0)) {
        ERR_POST(Warning << "File list is emtpy");
        exitcode = 1;
    } else {
        ERR_POST(Info << "Total downloaded "
                 << size << " file" << &"s"[size == 1]
                 << " in " << fixed << setprecision(2) << dlcbdata.GetElapsed()
                 << "s; combined size " << dlcbdata.GetTotalSize());
        exitcode = 0;
    }
    CORE_SetLOG(0);
    CORE_SetLOCK(0);
    return exitcode;
}
