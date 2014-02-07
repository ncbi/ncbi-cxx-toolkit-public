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
#include <corelib/ncbi_process.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_socket.hpp>
#include <stdlib.h>
#include <time.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */

#include "test_assert.h"  // This header must go last

#ifdef NCBI_OS_UNIX
#  define DEV_NULL "/dev/null"
#else
#  define DEV_NULL "NUL"
#endif // NCBI_OS_UNIX


static const size_t kBufferSize = 512*1024;


BEGIN_NCBI_SCOPE


static CNcbiRegistry* s_CreateRegistry(void)
{
    CNcbiRegistry* reg = new CNcbiRegistry;

    // Compose a test registry
    reg->Set("ID1", DEF_CONN_REG_SECTION "_" REG_CONN_HOST, DEF_CONN_HOST);
    reg->Set("ID1", DEF_CONN_REG_SECTION "_" REG_CONN_PATH, DEF_CONN_PATH);
    reg->Set("ID1", DEF_CONN_REG_SECTION "_" REG_CONN_ARGS, DEF_CONN_ARGS);
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_HOST,     "www.ncbi.nlm.nih.gov");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_PATH,      "/Service/bounce.cgi");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_ARGS,           "arg1+arg2+arg3");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_REQ_METHOD,     "POST");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_TIMEOUT,        "10.0");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "TRUE");
    return reg;
}


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


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;
    CNcbiRegistry* reg;
    TFTP_Flags flag = 0;
    SConnNetInfo* net_info;
    size_t i, j, k, l, m, n, size;

    reg = s_CreateRegistry();
    CONNECT_Init(reg);

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

    if (argc <= 1)
        g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    else
        g_NCBI_ConnectRandomSeed = atoi(argv[1]);
    CORE_LOGF(eLOG_Note, ("Random SEED = %u", g_NCBI_ConnectRandomSeed));
    srand(g_NCBI_ConnectRandomSeed);


    LOG_POST(Info << "Test 0 of 9: Checking error log setup");
    ERR_POST(Info << "Test log message using C++ Toolkit posting");
    CORE_LOG(eLOG_Note, "Another test message using C Toolkit posting");
    LOG_POST(Info << "Test 0 passed\n");


    LOG_POST(Info << "Test 1 of 9: Memory stream");
    // Testing memory stream out-of-sequence interleaving operations
    m = (rand() & 0x00FF) + 1;
    size = 0;
    for (n = 0;  n < m;  n++) {
        CConn_MemoryStream* ms = 0;
        string data, back;
        size_t sz = 0;
#if 0
        LOG_POST(Info << "  Micro-test " << (int) n << " of "
                 << (int) m << " start");
#endif
        k = (rand() & 0x00FF) + 1;
        for (i = 0;  i < k;  i++) {
            l = (rand() & 0x00FF) + 1;
            string bit;
            bit.resize(l);
            for (j = 0;  j < l;  j++) {
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
            assert(ms->good());
            SetDiagTrace(eDT_Disable);
            *ms >> ws;
            SetDiagTrace(eDT_Enable);
            ms->clear();
        } else
            ms->ToString(&back);
#if 0
        LOG_POST(Info << "  Data size=" << (unsigned long) data.size());
        LOG_POST(Info << "  Back size=" << (unsigned long) back.size());
        for (i = 0;  i < data.size()  &&  i < back.size(); i++) {
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


    LOG_POST(Info << "Test 2 of 9: FTP download");
    if (rand() & 1)
        flag |= fFTP_DelayRestart;
    if (!(net_info = ConnNetInfo_Create(0)))
        ERR_POST(Fatal << "Cannot create net info");
    if (net_info->debug_printout == eDebugPrintout_Some)
        flag |= fFTP_LogControl;
    else if (net_info->debug_printout == eDebugPrintout_Data) {
        char val[32];
        ConnNetInfo_GetValue(0, REG_CONN_DEBUG_PRINTOUT, val, sizeof(val),
                             DEF_CONN_DEBUG_PRINTOUT);
        flag |= strcasecmp(val, "all") == 0 ? fFTP_LogAll : fFTP_LogData;
    }
    CConn_FTPDownloadStream download("ftp.ncbi.nlm.nih.gov",
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
    LOG_POST(Info << "Test 2 passed: 1024+" << size << '=' << n
             << " byte(s) downloaded via FTP\n");


    LOG_POST(Info << "Test 3 of 9: FTP upload");
    string ftpuser, ftppass, ftpfile;
    if (s_GetFtpCreds(ftpuser, ftppass)) {
        CTime start(CTime::eCurrent);
        ftpfile  = "test_ncbi_conn_stream";
        ftpfile += '-';
        ftpfile += NStr::GetField(CSocketAPI::gethostname(), 0, '.');
        ftpfile += '-';
        ftpfile += NStr::UInt8ToString(CProcess::GetCurrentPid());
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
        CConn_FTPUploadStream upload("ftp-private.ncbi.nlm.nih.gov",
                                     ftpuser, ftppass, ftpfile,
                                     "test_upload",
                                     0/*port = default*/, flag,
                                     0/*offset*/, net_info->timeout);
        size = 0;
        while (size < (10<<20)  &&  upload.good()) {
            char buf[4096];
            size_t n = (size_t) rand() % sizeof(buf) + 1;
            for (size_t i = 0;  i < n;  i++)
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
                LOG_POST(Info << "Test 3 passed: " <<
                         speedstr << '\n');
            } else
                LOG_POST(speedstr);
        }
        upload.clear();
        upload << "REN " << ftpfile << '\t'
               << '"' << ftpfile << "~\"" << NcbiEndl;
        if (!upload  ||  upload.Status(eIO_Write) != eIO_Success)
            LOG_POST("REN failed");
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
        LOG_POST(Info << "Test 3 skipped\n");


    LOG_POST(Info << "Test 4 of 9: FTP peculiarities");
    if (!ftpuser.empty()  &&  !ftppass.empty()) {
        _ASSERT(!ftpfile.empty());
        // Note that FTP streams are not buffered for the sake of command
        // responses;  for file xfers the use of read() and write() does
        // the adequate buffering at user-level (and FTP connection).
        CConn_FtpStream ftp("ftp-private.ncbi.nlm.nih.gov",
                            ftpuser, ftppass, "test_download",
                            0/*port = default*/, flag, 0/*cmcb*/,
                            net_info->timeout);
        string temp;
        ftp << "SYST" << NcbiEndl;
        ftp >> temp;  // dangerous: may leave some putback behind
        if (temp.empty())
            ERR_POST(Fatal << "Test 4 failed in SYST");
        LOG_POST(Info << "SYST command returned: '" << temp << '\'');
        _ASSERT(ftp.Drain() == eIO_Success);
        ftp << "CWD \"dir\"ect\"ory\"" << NcbiEndl;
        if (!ftp  ||  ftp.Status(eIO_Write) != eIO_Success)
            ERR_POST(Fatal << "Test 4 failed in CWD");
        getline(ftp, temp);  // better: does not leave putback behind
        LOG_POST(Info << "CWD command returned: '" << temp << '\'');
        _ASSERT(temp == "250");
        _ASSERT(ftp.eof());
        ftp.clear();
        ftp << "PWD" << NcbiEndl;
        if (!ftp  ||  ftp.Status(eIO_Write) != eIO_Success)
            ERR_POST(Fatal << "Test 4 failed in PWD");
        ftp >> temp;
        LOG_POST(Info << "PWD command returned: '" << temp << '\'');
        if (temp != "/test_download/\"dir\"ect\"ory\"")
            ERR_POST(Fatal << "Test 4 failed in PWD response");
        ftp.clear();
        ftp << "XCUP" << NcbiEndl;
        if (!ftp  ||  ftp.Status(eIO_Write) != eIO_Success)
            ERR_POST(Fatal << "Test 4 failed in XCUP");
        ftp << "RETR \377\377 special file downloadable" << NcbiEndl;
        if (!ftp  ||  ftp.Status(eIO_Write) != eIO_Success) {
            ftp.clear();
            ftp << "RETR \377 special file downloadable" << NcbiEndl;
            if (!ftp  ||  ftp.Status(eIO_Write) != eIO_Success)
                ERR_POST(Fatal << "Test 4 failed in RETR IAC");
            ERR_POST(Critical << "\n\n***"
                     " BUGGY FTP (UNCLEAN IAC) SERVER DETECTED!!! "
                     "***\n");
        }
        ftp << "RETR \357\273\277\320\237\321\200\320"
            << "\270\320\262\320\265\321\202" << NcbiEndl;
        if (!ftp  ||  ftp.Status (eIO_Write) != eIO_Success)
            ERR_POST(Fatal << "Test 4 failed in RETR UTF-8");
        ftp << "STOR " << "../test_upload/" << ftpfile << ".0" << NcbiEndl;
        if (!ftp  ||  ftp.Status(eIO_Write) != eIO_Success)
            ERR_POST(Fatal << "Test 4 failed in STOR");
        if (ftp.Close() == eIO_Success)
            ERR_POST(Fatal << "Test 4 failed");
        LOG_POST(Info << "Test 4 done\n");
    } else
        LOG_POST(Info << "Test 4 skipped\n");

    ConnNetInfo_Destroy(net_info);


    {{
        // Silent test for timeouts and memory leaks in an unused stream
        static const STimeout tmo = {5, 0};
        CConn_IOStream* s  =
            new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmo);
        delete s;
    }}


    LOG_POST(Info << "Test 5 of 9: Big buffer bounce via HTTP");
    CConn_HttpStream ios(0, "User-Header: My header\r\n", 0, 0, 0, 0,
                         fHTTP_AutoReconnect | fHTTP_Flushable |
                         fHCC_UrlEncodeArgs/*obsolete now*/);

    char *buf1 = new char[kBufferSize + 1];
    char *buf2 = new char[kBufferSize + 2];

    for (j = 0; j < kBufferSize/1024; j++) {
        for (i = 0; i < 1024 - 1; i++)
            buf1[j*1024 + i] = "0123456789"[rand() % 10];
        buf1[j*1024 + 1024 - 1] = '\n';
    }
    buf1[kBufferSize] = '\0';

    if (!(ios << buf1))
        ERR_POST(Fatal << "Cannot send data");

    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(kBufferSize)));

    if (!ios.flush())
        ERR_POST(Fatal << "Cannot flush data");

    ios.read(buf2, kBufferSize + 1);
    streamsize buflen = ios.gcount();

    if (!ios.good() && !ios.eof())
        ERR_POST(Fatal << "Cannot receive data");

    LOG_POST(Info << buflen << " bytes obtained"
             << (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST(Fatal << "Not entirely bounced, mismatch position: " << i+1);
    if ((size_t) buflen > kBufferSize)
        ERR_POST(Fatal << "Sent: " << kBufferSize << ", bounced: " << buflen);

    LOG_POST(Info << "Test 5 passed\n");

    // Clear EOF condition
    ios.clear();


    LOG_POST(Info << "Test 6 of 9: Random bounce");

    if (!(ios << buf1))
        ERR_POST(Fatal << "Cannot send data");

    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(2*kBufferSize)));

    j = 0;
    buflen = 0;
    for (i = 0, l = 0; i < kBufferSize; i += j, l++) {
        k = rand() % 15 + 1;

        if (i + k > kBufferSize + 1)
            k = kBufferSize + 1 - i;
        ios.read(&buf2[i], k);
        j = (size_t) ios.gcount();
        if (!ios.good() && !ios.eof()) {
            ERR_POST("Cannot receive data");
            return 2;
        }
        if (j != k)
            LOG_POST(Info << "Bytes requested: " << k << ", received: " << j);
        buflen += j;
        l++;
        if (!j && ios.eof())
            break;
    }

    LOG_POST(Info << buflen << " bytes obtained in " << l << " iteration(s)"
             << (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST(Fatal << "Not entirely bounced, mismatch position: " << i+1);
    if ((size_t) buflen > kBufferSize)
        ERR_POST(Fatal << "Sent: " << kBufferSize << ", bounced: " << buflen);

    LOG_POST(Info << "Test 6 passed\n");

    // Clear EOF condition
    ios.clear();


    LOG_POST(Info << "Test 7 of 9: Truly binary bounce");

    for (i = 0; i < kBufferSize; i++)
        buf1[i] = (char)(255/*rand() % 256*/);

    ios.write(buf1, kBufferSize);

    if (!ios.good())
        ERR_POST(Fatal << "Cannot send data");

    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(3*kBufferSize)));

    ios.read(buf2, kBufferSize + 1);
    buflen = ios.gcount();

    if (!ios.good() && !ios.eof())
        ERR_POST(Fatal << "Cannot receive data");

    LOG_POST(Info << buflen << " bytes obtained"
             << (ios.eof() ? " (EOF)" : ""));

    for (i = 0; i < kBufferSize; i++) {
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST(Fatal << "Not entirely bounced, mismatch position: " << i+1);
    if ((size_t) buflen > kBufferSize)
        ERR_POST(Fatal << "Sent: " << kBufferSize << ", bounced: " << buflen);

    LOG_POST(Info << "Test 7 passed\n");

    delete[] buf1;
    delete[] buf2;


    LOG_POST(Info << "Test 8 of 9: NcbiStreamCopy()");

    ofstream null(DEV_NULL);
    assert(null);

    CConn_HttpStream http("www.ncbi.nlm.nih.gov",
                          "/cpp/network/dispatcher.html", kEmptyStr/*args*/,
                          "My-Header: Header", 0/*port*/, fHTTP_Flushable);
    http << "Sample input -- should be ignored";

    if (!http.good()  ||  !http.flush()  ||  !NcbiStreamCopy(null, http))
        ERR_POST(Fatal << "Test 8 failed");
    else
        LOG_POST(Info << "Test 8 passed\n");


    LOG_POST(Info << "Test 9 of 9: HTTP status code and text");

    CConn_HttpStream bad_http("http://www.ncbi.nlm.nih.gov/blah");
    bad_http >> ftpfile/*dummy*/;

    int    code = bad_http.GetStatusCode();
    string text = bad_http.GetStatusText();
    NcbiCout << "Status(bad) = " << code << ' ' << text << NcbiEndl;
    if (code != 404  ||  text.empty())
        ERR_POST(Fatal << "Test 9 failed");
    bad_http.Close();

    CConn_HttpStream good_http("http://www.ncbi.nlm.nih.gov/index.html");
    good_http >> ftpfile/*dummy*/;

    code = good_http.GetStatusCode();
    text = good_http.GetStatusText();
    NcbiCout << "Status(good) = " << code << ' ' << text << NcbiEndl;
    if (code != 200  ||  text.empty())
        ERR_POST(Fatal << "Test 9 failed");

    LOG_POST(Info << "Test 9 passed\n");


    CORE_SetREG(0);
    delete reg;

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOCK(0);
    CORE_SetLOG(0);
    return 0/*okay*/;
}
