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
 *   Standard test for the CONN-based streams
 *
 */

#include <ncbi_pch.hpp>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_process.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <stdlib.h>
#include <time.h>

#include "test_assert.h"  // This header must go last

#ifdef NCBI_OS_UNIX
#  define DEV_NULL "/dev/null"
#else
#  define DEV_NULL "NUL"
#endif // NCBI_OS_UNIX

#define CONN_NCBI_FTP_PUBLIC_HOST   "ftp-ext.ncbi.nlm.nih.gov"
#define CONN_NCBI_FTP_PRIVATE_HOST  "ftp-private.ncbi.nlm.nih.gov"
#define N                           12

#define _STR(X)                         #X
#define  STR(X)                     _STR(X)


static const size_t kBufferSize = 512 * 1024;


USING_NCBI_SCOPE;


static inline unsigned long udiff(unsigned long a, unsigned long b)
{
    return a > b ? a - b : b - a;
}


static bool s_GetFtpCreds(string& user, string& pass)
{
    user.clear();
    pass.clear();
    ifstream ifs("/am/ncbiapdata/test_data/ftp/test_ncbi_ftp_upload");
    if (ifs) {
        string src;
        ifs >> src;
        ifs.close();
        string dst(src.size(), '\0');
        size_t n_read, n_written;
        BASE64_Decode(src.c_str(), src.size(), &n_read,
                      &dst[0], dst.size(), &n_written);
        dst.resize(n_written);
        NStr::SplitInTwo(dst, ":", user, pass);
    }
    return !user.empty()  &&  !pass.empty();
}


class CNCBITestConnStreamApp : public CNcbiApplication
{
public:
    CNCBITestConnStreamApp(void);

public:
    void Init(void);
    int  Run (void);
};


CNCBITestConnStreamApp::CNCBITestConnStreamApp(void)
{
    DisableArgDescriptions();
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));
}


void CNCBITestConnStreamApp::Init(void)
{
    // Init the library explicitly (this sets up the registry)
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        } conn_initer;  /*NCBI_FAKE_WARNING*/
    }
    // NB: CONNECT_Init() can be used alternatively for applications
    
    // Create and populate test registry
    CNcbiRegistry& reg = GetRWConfig();

    reg.Set("ID1",    DEF_CONN_REG_SECTION "_" REG_CONN_PATH, DEF_CONN_PATH);
    reg.Set("ID1",    DEF_CONN_REG_SECTION "_" REG_CONN_ARGS, DEF_CONN_ARGS);
    reg.Set("bounce", DEF_CONN_REG_SECTION "_" REG_CONN_PATH, DEF_CONN_PATH);
    reg.Set("bounce", DEF_CONN_REG_SECTION "_" REG_CONN_DEBUG_PRINTOUT, "DATA");
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_PORT,           "443");
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_PATH,      "/Service/bounce.cgi");
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_ARGS,           "arg1+arg2+arg3");
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_REQ_METHOD,     "POST");
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_TIMEOUT,        "10.0");
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_MAX_TRY,        "1");
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "TRUE");
}


static void s_FTPStat(iostream& ftp)
{
    // Print out some server info
    if (!(ftp << "STAT" << NcbiFlush)) {
        ERR_POST(Fatal << "Cannot connect to ftp server");
    }
    string status;
    do {
        string line;
        getline(ftp, line);
        size_t linelen = line.size();
        if (linelen /*!line.empty()*/  &&  NStr::EndsWith(line, '\r')) {
            line.resize(--linelen);
        }
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) {
            continue;
        }
        if (status.empty()) {
            status  = "Server status:\n\t" + line;
        } else {
            status += "\n\t";
            status += line;
        }
    } while (ftp);
    if (!status.empty()) {
        ERR_POST(Info << status);
    }
    ftp.clear();
}


int CNCBITestConnStreamApp::Run(void)
{
    TFTP_Flags flag = 0;
    SConnNetInfo* net_info;
    size_t i, j, k, l, m, n, size;

    if (GetArguments().Size() > 1) {
        g_NCBI_ConnectRandomSeed
            = (unsigned int) atoi(GetArguments()[1].c_str());
    } else {
        g_NCBI_ConnectRandomSeed
            = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    }
    CORE_LOGF(eLOG_Note, ("Random SEED = %u", g_NCBI_ConnectRandomSeed));
    srand(g_NCBI_ConnectRandomSeed);


    LOG_POST(Info << "Test 0 of " STR(N) ": Checking error log setup");

    ERR_POST(Info << "Test log message using C++ Toolkit posting");
    CORE_LOG(eLOG_Note, "Another test message using C Toolkit posting");
    LOG_POST(Info << "Test 0 passed\n");


    LOG_POST(Info << "Test 1 of " STR(N) ": Memory stream");

    // Testing memory stream out-of-sequence interleaving operations
    m = (rand() & 0x00FF) + 1;
    size = 0;
    for (n = 0;  n < m;  ++n) {
        CConn_MemoryStream* ms = 0;
        string data, back;
        size_t sz = 0;
#if 0
        LOG_POST(Info << "  Micro-test " << (int) n << " of "
                 << (int) m << " start");
#endif
        k = (rand() & 0x00FF) + 1;
        for (i = 0;  i < k;  ++i) {
            l = (rand() & 0x00FF) + 1;
            string bit;
            bit.resize(l);
            for (j = 0;  j < l;  ++j) {
                bit[j] = "0123456789"[rand() % 10];
            }
#if 0
            LOG_POST(Info << "    Data bit at " << (unsigned long) sz << ", "
                     << (unsigned long) l << " byte(s) long: " << bit);
#endif
            sz += l;
            data += bit;
            if (ms)
                assert(*ms << bit);
            else if (i == 0) {
                switch (n % 4) {
                case 0:
#if 0
                    LOG_POST(Info << "  CConn_MemoryStream()");
#endif
                    ms = new CConn_MemoryStream;
                    ms->exceptions(NcbiBadbit);
                    assert(*ms << bit);
                    break;
                case 1:
                    {{
                        BUF buf = 0;
                        assert(BUF_Write(&buf, bit.data(), l));
#if 0
                        LOG_POST(Info << "  CConn_MemoryStream(BUF)");
#endif
                        ms = new CConn_MemoryStream(buf, eTakeOwnership);
                        ms->exceptions(NcbiBadbit);
                        break;
                     
                    }}
                default:
                    break;
                }
            }
        }
        switch (n % 4) {
        case 2:
#if 0
            LOG_POST(Info << "  CConn_MemoryStream("
                     << (unsigned long) data.size() << ')');
#endif
            ms = new CConn_MemoryStream(data.data(),data.size(), eNoOwnership);
            break;
        case 3:
            {{
                BUF buf = 0;
                assert(BUF_Append(&buf, data.data(), data.size()));
#if 0
                LOG_POST(Info << "  CConn_MemoryStream(BUF, "
                         << (unsigned long) data.size() << ')');
#endif
                ms = new CConn_MemoryStream(buf, eTakeOwnership);
                break;
            }}
        default:
            break;
        }
        assert(ms);
        if (!(rand() & 1)) {
            assert(*ms << endl);
            *ms >> back;
            IOS_BASE::iostate state = ms->rdstate();
            assert(state == NcbiGoodbit);
            SetDiagTrace(eDT_Disable);
            ms->exceptions(NcbiBadbit | NcbiEofbit);
            try {
                *ms >> ws;
            } catch (IOS_BASE::failure& ) {
                state = ms->rdstate();
            }
#if   defined(NCBI_COMPILER_GCC)
#  if defined(NCBI_OS_CYGWIN)                                           \
    ||  (NCBI_COMPILER_VERSION > 500  &&  NCBI_COMPILER_VERSION < 600   \
         &&  (!defined(_GLIBCXX_USE_CXX11_ABI) || _GLIBCXX_USE_CXX11_ABI != 0))
            catch (...) {
                // WORKAROUND:
                //   At least GCC 5.1.0 and 5.3.0 and using new ABI:
                //   fails to catch "IOS_BASE::failure" above.
                // Looks like ios_base::failure's actual type in the
                // std lib is pre-C++11 in this case.
                state = ms->rdstate();
            }
#  endif
#elif defined(NCBI_COMPILER_ICC)
            catch (...) {
                // WORKAROUND:
                // ICC 17.04 fails to catch "IOS_BASE::failure" above
                // because we're still using old ABI libraries.
                state = ms->rdstate();
            }
#endif /*NCBI_COMPILER_...*/
            _ASSERT(state & NcbiEofbit);
            SetDiagTrace(eDT_Enable);
            ms->clear();
        } else
            ms->ToString(&back);
#if 0
        LOG_POST(Info << "  Data size=" << (unsigned long) data.size());
        LOG_POST(Info << "  Back size=" << (unsigned long) back.size());
        for (i = 0;  i < data.size()  &&  i < back.size();  ++i) {
            if (data[i] != back[i]) {
                LOG_POST("  Data differ at pos " << (unsigned long) i
                         << ": '" << data[i] << "' vs '" << back[i] << '\'');
                break;
            }
        }
#endif
        assert(back == data);
        size += sz;
        delete ms;
#if 0
        LOG_POST(Info << "  Micro-test " << (int) n << " of "
                 << (int) m << " finish");
#endif
    }
    LOG_POST(Info << "Test 1 passed in "
             << (int) m    << " iteration(s) with "
             << (int) size << " byte(s) transferred\n");


    LOG_POST(Info << "Test 2 of " STR(N) ": Socket stream");

    bool secure = rand() & 1 ? true : false;
    if (!(net_info = ConnNetInfo_Create(0)))
        ERR_POST(Fatal << "Cannot create net info");

    unique_ptr<CSocket>        socket(new CSocket
                                      (net_info->host,    secure
                                       ? CONN_PORT_HTTPS
                                       : CONN_PORT_HTTP,
                                       net_info->timeout, secure
                                       ? fSOCK_LogDefault | fSOCK_Secure
                                       : fSOCK_LogDefault));
    unique_ptr<CConn_IOStream> stream(new CConn_SocketStream
                                      (socket->GetSOCK(), eNoOwnership,
                                       net_info->timeout));
    if (!stream->good())
        ERR_POST(Fatal << "Test 2 failed");

    stream.reset();
    socket.reset();
    ConnNetInfo_Destroy(net_info);
    LOG_POST(Info << "Test 2 passed\n");

    if (rand() & 1)
        flag |= fFTP_DelayRestart;
    if (!(net_info = ConnNetInfo_Create("_FTP")))
        ERR_POST(Fatal << "Cannot create net info");
    if (net_info->debug_printout == eDebugPrintout_Some)
        flag |= fFTP_LogControl;
    else if (net_info->debug_printout == eDebugPrintout_Data) {
        char val[32];
        ConnNetInfo_GetValue(0, REG_CONN_DEBUG_PRINTOUT, val, sizeof(val),
                             DEF_CONN_DEBUG_PRINTOUT);
        flag |= strcasecmp(val, "all") == 0 ? fFTP_LogAll : fFTP_LogData;
    }


    LOG_POST(Info << "Test 3 of " STR(N) ": FTP download");

    CConn_FTPDownloadStream download(CONN_NCBI_FTP_PUBLIC_HOST,
                                     "Misc/test_ncbi_conn_stream.FTP.data",
                                     "ftp"/*default*/, "-none"/*default*/,
                                     "/toolbox/ncbi_tools++/DATA",
                                     0/*port = default*/, flag, 0/*cmcb*/,
                                     1024/*offset*/, net_info->timeout);
    size_t pos1 = (size_t) download.tellg();
    size = NcbiStreamToString(0, download);
    if (!size)
        download.clear(NcbiBadbit);
    else if (download.eof()  &&  !download.fail())
        download.clear();
    size_t pos2 = (size_t) download.tellg();
    download.clear();
    download << "SIZE " <<
        "/toolbox/ncbi_tools++/DATA/Misc/test_ncbi_conn_stream.FTP.data";
    n = 0;
    download >> n;
    download.Close();
    if (!size)
        ERR_POST(Fatal << "No file downloaded");
    if (n  &&  n != size + 1024)
        ERR_POST(Fatal << "File size mismatch: 1024+" << size << "!=" << n);
    else if (pos1 == (size_t)(-1L)  ||  pos2 == (size_t)(-1L)) 
        ERR_POST(Fatal << "Bad stream posision(s): " << pos1 << ',' << pos2);
    else if (size != pos2 - pos1) {
        ERR_POST(Fatal << "Size mismatch: " << size << "!="
                 << pos2 << '-' <<  pos1);
    }
    LOG_POST(Info << "Test 3 passed: 1024+" << size << '=' << n
             << " byte(s) downloaded via FTP\n");


    LOG_POST(Info << "Test 4 of " STR(N) ": FTP upload");

    EIO_Status status;
    string ftpuser, ftppass, ftpfile;
    if (s_GetFtpCreds(ftpuser, ftppass)) {
        CTime start(CTime::eCurrent);
        ftpfile  = "test_ncbi_conn_stream";
        ftpfile += '-';
        ftpfile += NStr::GetField(CSocketAPI::gethostname(), 0, '.');
        ftpfile += '-';
        ftpfile += NStr::UInt8ToString(CCurrentProcess::GetPid());
        ftpfile += '-';
        ftpfile += start.AsString("YMDhms");
        ftpfile += ".tmp";
        // to use advanced xfer modes if available
        if (rand() & 1)
            flag |= fFTP_UncorkUpload;
        if (rand() & 1)
            flag |= fFTP_UseFeatures;
        if (rand() & 1)
            flag |= fFTP_UseActive;
        CConn_FTPUploadStream upload(CONN_NCBI_FTP_PRIVATE_HOST,
                                     ftpuser, ftppass, ftpfile,
                                     "test_upload",
                                     0/*port = default*/, flag,
                                     0/*offset*/, net_info->timeout);
        size = 0;
        while (size < (10<<20)  &&  upload.good()) {
            char buf[4096];
            n = (size_t) rand() % sizeof(buf) + 1;
            for (i = 0;  i < n;  i++)
                buf[i] = rand() & 0xFF;
            if (upload.write(buf, n))
                size += n;
        }
        if (!upload)
            ERR_POST(Fatal << "FTP upload error");
        _ASSERT(size != 0);
        time_t delta = 0;
        unsigned long val = 0;
        upload >> val;
        if (val == (unsigned long) size) {
            unsigned long filesize = 0;
            upload.clear();
            upload << "SIZE " << ftpfile << NcbiEndl;
            upload >> filesize;
            string filetime;
            upload.clear();
            upload << "MDTM " << ftpfile << NcbiEndl;
            upload >> filetime;
            string speedstr = (NStr::UInt8ToString(Uint8(size))
                               + " bytes uploaded via FTP");
            if (val == filesize)
                speedstr += " and verified";
            if (!filetime.empty()) {
                time_t stop = (time_t) NStr::StringToUInt(filetime);
                time_t time = (time_t) udiff((long) stop,
                                             (long) start.GetTimeT());
                double rate = (val / 1024.0) / (time ? time : 1);
                speedstr += (" in "
                             + NStr::ULongToString((unsigned long) time)
                             + " sec @ "
                             + NStr::DoubleToString(rate,2, NStr::fDoubleFixed)
                             + " KB/s");
                delta = (time_t)udiff((long)stop,
                                      (long)CTime(CTime::eCurrent).GetTimeT());
            }
            if (delta < 1800) {
                LOG_POST(Info << "Test 4 passed: " <<
                         speedstr << '\n');
            } else
                LOG_POST(speedstr);
        }
        upload.clear();
        upload << "REN " << ftpfile << '\t'
               << '"' << ftpfile << "~\"" << NcbiEndl;
        status = upload.Status(eIO_Write); 
        if (!upload  ||  status != eIO_Success) {
            string reason = status ? IO_StatusStr(status) : "I/O error";
            LOG_POST("REN failed: " + reason);
        }
        upload << "DELE " << ftpfile        << NcbiEndl;
        upload << "DELE " << ftpfile << '~' << NcbiEndl;
        if (val != (unsigned long) size) {
            ERR_POST(Fatal << "FTP upload incomplete: " <<
                     val << " out of " << size << " byte(s) uploaded");
        } else if (delta >= 1800) {
            ERR_POST(Fatal << "FTP file time is off by " <<
                     NStr::UIntToString((unsigned int) delta) <<
                     " seconds");
        }
    } else
        LOG_POST(Info << "Test 4 skipped\n");


    LOG_POST(Info << "Test 5 of " STR(N) ": FTP peculiarities");

    if (!ftpuser.empty()  &&  !ftppass.empty()) {
        _ASSERT(!ftpfile.empty());
        // Note that FTP streams are not buffered for the sake of command
        // responses;  for file xfers the use of read() and write() does
        // the adequate buffering at user-level (and FTP connection).
        CConn_FtpStream ftp(CONN_NCBI_FTP_PRIVATE_HOST,
                            ftpuser, ftppass, "test_download",
                            0/*port = default*/, flag, 0/*cmcb*/,
                            net_info->timeout);
        s_FTPStat(ftp);
        string temp;
        ftp << "SYST" << NcbiEndl;
        ftp >> temp;  // dangerous: may leave some putback behind
        if (temp.empty())
            ERR_POST(Fatal << "Test 5 failed in SYST");
        LOG_POST(Info << "SYST command returned: '" << temp << '\'');
        _ASSERT(ftp.Drain() == eIO_Success);
        ftp << "CWD \"dir\"ect\"ory\"" << NcbiEndl;
        status = ftp.Status(eIO_Write);
        if (!ftp  ||  status != eIO_Success) {
            string reason = status ? IO_StatusStr(status) : "I/O error";
            ERR_POST(Fatal << "Test 4 failed in CWD: " + reason);
        }
        getline(ftp, temp);  // better: does not leave putback behind
        LOG_POST(Info << "CWD command returned: '" << temp << '\'');
        _ASSERT(temp == "250");
        _ASSERT(ftp.eof());
        ftp.clear();
        ftp << "PWD" << NcbiEndl;
        status = ftp.Status(eIO_Write);
        if (!ftp  ||  status != eIO_Success) {
            string reason = status ? IO_StatusStr(status) : "I/O error";
            ERR_POST(Fatal << "Test 5 failed in PWD: " + reason);
        }
        ftp >> temp;
        LOG_POST(Info << "PWD command returned: '" << temp << '\'');
        if (temp != "/test_download/\"dir\"ect\"ory\"")
            ERR_POST(Fatal << "Test 5 failed in PWD response");
        ftp.clear();
        ftp << "XCUP" << NcbiEndl;
        status = ftp.Status(eIO_Write);
        if (!ftp  ||  status != eIO_Success) {
            string reason = status ? IO_StatusStr(status) : "I/O error";
            ERR_POST(Fatal << "Test 5 failed in XCUP: " + reason);
        }
        //ftp.clear();
        ftp << "RETR \377\377 special file downloadable" << NcbiEndl;
        status = ftp.Status(eIO_Write);
        if (!ftp  ||  status != eIO_Success) {
            ftp.clear();
            ftp << "RETR \377 special file downloadable" << NcbiEndl;
            status = ftp.Status(eIO_Write);
            if (!ftp  ||  status != eIO_Success) {
                string reason = status ? IO_StatusStr(status) : "I/O error";
                ERR_POST(Fatal << "Test 4 failed in RETR IAC: " + reason);
            }
            ERR_POST(Critical << "\n\n***"
                     " BUGGY FTP (UNCLEAN IAC) SERVER DETECTED!!! "
                     "***\n");
        }
        ftp << "RETR "
            << "\320\237\321\200\320\270"
            << "\320\262\320\265\321\202" << NcbiEndl;
        status = ftp.Status (eIO_Write);
        if (!ftp  ||  status != eIO_Success) {
            string reason = status ? IO_StatusStr(status) : "I/O error";
            ERR_POST(Fatal << "Test 5 failed in RETR UTF-8: " + reason);
        }
        ftp << "STOR " << "../test_upload/" << ftpfile << ".0" << NcbiEndl;
        status = ftp.Status(eIO_Write);
        if (!ftp  ||  status != eIO_Success) {
            string reason = status ? IO_StatusStr(status) : "I/O error";
            ERR_POST(Fatal << "Test 5 failed in STOR: " + reason);
        }
        if (ftp.Close() == eIO_Success)
            ERR_POST(Fatal << "Test 5 failed");
        LOG_POST(Info << "Test 5 done\n");
    } else
        LOG_POST(Info << "Test 5 skipped\n");


    {{
        // Silent test for timeouts and memory leaks in an unused stream
        static const STimeout tmo = {5, 0};
        CConn_IOStream* s  =
            new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmo);
        delete s;
    }}


    LOG_POST(Info << "Test 6 of " STR(N) ": Big buffer bounce via HTTP");

    ConnNetInfo_Destroy(net_info);
    if (!(net_info = ConnNetInfo_Create(0)))
        ERR_POST(Fatal << "Cannot re-create net info");
    if (net_info->port == CONN_PORT_HTTPS)
        net_info->scheme = eURL_Https;
    CConn_HttpStream ios(net_info, "User-Header: My header\r\n", 0, 0, 0, 0,
                         fHTTP_AutoReconnect | fHTTP_Flushable |
                         fHCC_UrlEncodeArgs/*obsolete now*/);
    ConnNetInfo_Destroy(net_info);

    AutoPtr< char, ArrayDeleter<char> >  buf1(new char[kBufferSize + 1]);
    AutoPtr< char, ArrayDeleter<char> >  buf2(new char[kBufferSize + 2]);

    for (j = 0;  j < kBufferSize/1024;  j++) {
        for (i = 0; i < 1024 - 1; i++)
            buf1.get()[j*1024 + i] = "0123456789"[rand() % 10];
        buf1.get()[j*1024 + 1024 - 1] = '\n';
    }
    buf1.get()[kBufferSize] = '\0';

    if (!(ios << buf1.get()))
        ERR_POST(Fatal << "Cannot send data");

    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(kBufferSize)));

    if (!ios.flush())
        ERR_POST(Fatal << "Cannot flush data");

    ios.read(buf2.get(), kBufferSize + 1);
    streamsize buflen = ios.gcount();

    if (!ios.good()  &&  !ios.eof())
        ERR_POST(Fatal << "Cannot receive data");

    LOG_POST(Info << buflen << " bytes obtained"
             << (ios.eof() ? " (EOF)" : ""));
    buf2.get()[buflen] = '\0';

    for (i = 0;  i < kBufferSize;  i++) {
        if (!buf2.get()[i])
            break;
        if (buf2.get()[i] != buf1.get()[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST(Fatal << "Not entirely bounced, mismatch position: " << i+1);
    if ((size_t) buflen > kBufferSize)
        ERR_POST(Fatal << "Sent: " << kBufferSize << ", bounced: " << buflen);

    LOG_POST(Info << "Test 6 passed\n");

    // Clear EOF condition
    ios.clear();


    LOG_POST(Info << "Test 7 of " STR(N) ": Random bounce");

    if (!(ios << buf1.get()))
        ERR_POST(Fatal << "Cannot send data");

    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(2*kBufferSize)));

    buflen = 0;
    for (i = 0, l = 0;  i < kBufferSize;  i += j, l++) {
        k = rand() % 15 + 1;
        if (i + k > kBufferSize + 1)
            k = kBufferSize + 1 - i;
        ios.read(&buf2.get()[i], k);
        j = (size_t) ios.gcount();
        if (!ios.good()  &&  !ios.eof())
            ERR_POST(Fatal << "Cannot receive data");
        if (j != k)
            LOG_POST(Info << "Bytes requested: " << k << ", received: " << j);
        buflen += j;
        l++;
        if (!j  &&  ios.eof())
            break;
    }

    LOG_POST(Info << buflen << " bytes obtained in " << l << " iteration(s)"
             << (ios.eof() ? " (EOF)" : ""));
    buf2.get()[buflen] = '\0';

    for (i = 0;  i < kBufferSize;  i++) {
        if (!buf2.get()[i])
            break;
        if (buf2.get()[i] != buf1.get()[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST(Fatal << "Not entirely bounced, mismatch position: " << i+1);
    if ((size_t) buflen > kBufferSize)
        ERR_POST(Fatal << "Sent: " << kBufferSize << ", bounced: " << buflen);

    LOG_POST(Info << "Test 7 passed\n");

    // Clear EOF condition
    ios.clear();


    LOG_POST(Info << "Test 8 of " STR(N) ": Truly binary bounce");

    for (i = 0;  i < kBufferSize;  i++)
        buf1.get()[i] = (char)(255/*rand() % 256*/);

    ios.write(buf1.get(), kBufferSize);

    if (!ios.good())
        ERR_POST(Fatal << "Cannot send data");

    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(3*kBufferSize)));

    ios.read(buf2.get(), kBufferSize + 1);
    buflen = ios.gcount();

    if (!ios.good()  &&  !ios.eof())
        ERR_POST(Fatal << "Cannot receive data");

    LOG_POST(Info << buflen << " bytes obtained"
             << (ios.eof() ? " (EOF)" : ""));

    for (i = 0;  i < kBufferSize;  i++) {
        if (buf2.get()[i] != buf1.get()[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST(Fatal << "Not entirely bounced, mismatch position: " << i+1);
    if ((size_t) buflen > kBufferSize)
        ERR_POST(Fatal << "Sent: " << kBufferSize << ", bounced: " << buflen);
    ios.Close();

    LOG_POST(Info << "Test 8 passed\n");

    buf1.reset(0);
    buf2.reset(0);


    LOG_POST(Info << "Test 9 of " STR(N) ": NcbiStreamCopy()");

    ofstream null(DEV_NULL);
    assert(null);

    CConn_HttpStream http("https://www.ncbi.nlm.nih.gov/cpp/network/"
                          "dispatcher.html", eReqMethod_Any,
                          "My-Header: Header", fHTTP_Flushable);
    http << "Sample input -- should be ignored";

    if (!http.good()  ||  !http.flush()  ||  !NcbiStreamCopy(null, http))
        ERR_POST(Fatal << "Test 9 failed");
    else
        LOG_POST(Info << "Test 9 passed\n");
    /* http will be closed by return from main() */


    LOG_POST(Info << "Test 10 of " STR(N) ": HTTP status code and text");

    CConn_HttpStream bad_http("https://www.ncbi.nlm.nih.gov/blah");
    int    code = bad_http.GetStatusCode();
    assert(!code);
    bad_http >> ftpfile/*dummy*/;

    code        = bad_http.GetStatusCode();
    string text = bad_http.GetStatusText();
    NcbiCout << "Status(bad) = " << code << ' ' << text << NcbiEndl;
    if (code != 404  ||  text.empty())
        ERR_POST(Fatal << "Test 10 failed");
    bad_http.Close();

    CConn_HttpStream good_http("https://www.ncbi.nlm.nih.gov/index.html");
    good_http >> ftpfile/*dummy*/;

    code = good_http.GetStatusCode();
    text = good_http.GetStatusText();
    NcbiCout << "Status(good) = " << code << ' ' << text << NcbiEndl;
    if (code != 200  ||  text.empty())
        ERR_POST(Fatal << "Test 10 failed");
    good_http.Close();

    LOG_POST(Info << "Test 10 passed\n");


    LOG_POST(Info << "Test 11 of " STR(N) ": HTTP If-Modified-Since");

    CConn_HttpStream modified("https://www.ncbi.nlm.nih.gov/Service"
                              "/index.html",
                              eReqMethod_Head,
                              "If-Modified-Since: "
                              "Fri, 25 Dec 2015 16:17:18 GMT");

    if (!(modified >> ftpfile/*dummy*/))
        ftpfile.erase();

    if (!ftpfile.empty())
        ERR_POST(Fatal << "Non-empty response\n" << ftpfile);
    if (modified.GetStatusCode() != 304)
        ERR_POST(Fatal << "Non-304 response");
    modified.Close();

    LOG_POST(Info << "Test 11 passed\n");


    LOG_POST(Info << "Test 12 of " STR(N) ": Service connector with SOCK");

    SOCK sock;
    string hello("Hello, World!");
    CConn_ServiceStream echo("bounce", fSERV_Standalone);
    if (!(sock = echo.GetSOCK()))
        ERR_POST(Fatal << "Unable to get SOCK. Test 12 failed");
    if (!(echo << hello << NcbiEndl))
        ERR_POST(Fatal << "Unable to send. Test 12 failed");
    if ((status = SOCK_Status(sock, eIO_Write)) != eIO_Success) {
        ERR_POST(Fatal << "SOCK write status " << IO_StatusStr(status)
                 << ". Test 12 failed");
    }
    if (!getline(echo, ftpfile))
        ERR_POST(Fatal << "Unable to receive. Test 12 failed");
    if (ftpfile != hello) {
        ERR_POST(Fatal << '"' << hello << "\" != \"" << ftpfile
                 << "\". Test 12 failed");
    }
    if (hello != ftpfile)
        ERR_POST(Fatal << "Stream data mismatch. Test 12 failed");
    hello += '\n';
    status = SOCK_Write(sock, hello.c_str(), hello.size(),
                        &size, eIO_WritePersist);
    if (status != eIO_Success) {
        ERR_POST(Fatal << "SOCK write failed (" << IO_StatusStr(status)
                 << "). Test 12 failed");
    }
    char smallbuf[80];
    status = SOCK_Read(sock, smallbuf, sizeof(smallbuf) - 1,
                       &size, eIO_ReadPlain);
    if (status != eIO_Success  ||  size > hello.size()) {
        ERR_POST(Fatal << "SOCK read failed ("
                 << (status != eIO_Success ? IO_StatusStr(status) : "too long")
                 << "). Test 12 failed");
    }
    smallbuf[size] = '\0';
    if (strncasecmp(hello.c_str(), smallbuf, strlen(smallbuf)) != 0)
        ERR_POST(Fatal << "SOCK data mismatch. Test 12 failed");
    // NB:  put positions may be not always comparable because for a server of
    // the STANDALONE or NCBID type, "sock" could have been also used to push a
    // 4-byte ticket prior to any actual user data...
    m = size_t(echo.tellg()) << 1;
    n = size_t(SOCK_GetPosition(sock, eIO_Read));
    if (n != m)
        ERR_POST(Fatal << "Position mismatch. Test 12 failed");
    echo.Close();

    LOG_POST(Info << "Test 12 passed\n");

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    return 0/*okay*/;
}


int main(int argc, const char* argv[])
{
    return CNCBITestConnStreamApp().AppMain(argc, argv);
}
