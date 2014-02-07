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
 * File Description:  Test case for CTar API:
 *                    basically, a limited verion of the tar utility.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/stream_utils.hpp>
#include <util/compress/tar.hpp>
#include <util/compress/stream_util.hpp>
#ifdef TEST_CONN_TAR
#  define NCBI_CONN_STREAM_EXPERIMENTAL_API
#  include <connect/ncbi_conn_stream.hpp>
#  include <connect/ncbi_gnutls.h>
#endif // TEST_CONN_TAR
#include <errno.h>
#ifdef NCBI_OS_MSWIN
#  include <io.h>     // For _setmode()
#  include <fcntl.h>  // For _O_BINARY
#  include <conio.h>
#  define  DEVNULL  "NUL"
#endif // NCBI_OS_MSWIN
#ifdef NCBI_OS_UNIX
#  include <signal.h>
#  define  DEVNULL  "/dev/null"
#endif // NCBI_OS_UNIX

#include <common/test_assert.h>  // This header must go last


#ifndef   EOVERFLOW
#  define EOVERFLOW  (-1)
#endif // EOVERFLOW


USING_NCBI_SCOPE;


static const unsigned int fVerbose = CTar::fDumpEntryHeaders;


#ifdef TEST_CONN_TAR

static volatile bool s_Canceled = false;


class CCanceled : public CObject, public ICanceled
{
public:
    virtual bool IsCanceled(void) const { return s_Canceled; }
};


#  if   defined(NCBI_OS_MSWIN)
static BOOL WINAPI s_Interrupt(DWORD type)
{
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        s_Canceled = true;
        return TRUE;  // handled
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    default:
        break;
    }
    return FALSE;  // unhandled
}
#  elif defined(NCBI_OS_UNIX)
extern "C" {
static void s_Interrupt(int /*signo*/)
{
    s_Canceled = true;
}
}
#  endif // NCBI_OS

#endif // TEST_CONN_TAR


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTarTest : public CNcbiApplication
{
public:
    CTarTest(void);

protected:
    virtual void Init(void);
    virtual int  Run(void);

    auto_ptr<CTar::TEntries> x_Append(CTar& tar, const string& name);

    string x_Pos(const CTarEntryInfo& info);

    CTar::TFlags m_Flags;
};


CTarTest::CTarTest(void)
    : m_Flags(0)
{
#if defined(__GLIBCPP__) || (defined(__GLIBCXX__)  &&  __GLIBCXX__ < 20060524)
    // a/ sync_with_stdio(false) is 100% buggy in GCC < 4.1.1; or
    // b/ sync_with_stdio(any) is 100% buggy in Solaris STL;
    // when stdin's positioning methods (failing or not) get called...
    SetStdioFlags(fDefault_SyncWithStdio);
#endif // __GLIBCPP__ || (__GLIBCXX__  &&  __GLIBCXX__ < 20060524)
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_DateTime    | eDPF_Severity |
                        | eDPF_OmitInfoSev | eDPF_ErrorID);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));
    DisableArgDescriptions(fDisableStdArgs);
    HideStdArgs(-1/*everything*/);
}


void CTarTest::Init(void)
{
#ifdef TEST_CONN_TAR
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        } conn_initer;
    }
#  if   defined(NCBI_OS_MSWIN)
    SetConsoleCtrlHandler(s_Interrupt, TRUE);
#  elif defined(NCBI_OS_UNIX)
    signal(SIGINT,  s_Interrupt);
    signal(SIGTERM, s_Interrupt);
    signal(SIGQUIT, s_Interrupt);
#  endif // NCBI_OS
#endif // TEST_CONN_TAR

    auto_ptr<CArgDescriptions> args(new CArgDescriptions);
    args->SetMiscFlags(CArgDescriptions::fUsageIfNoArgs);
    if (args->Exist ("h")) {
        args->Delete("h");
    }
    if (args->Exist ("xmlhelp")) {
        args->Delete("xmlhelp");
    }
    args->AddFlag("c", "Create archive");
    args->AddFlag("r", "Append archive");
    args->AddFlag("u", "Update archive");
    args->AddFlag("t", "Table of contents");
    args->AddFlag("x", "Extract archive");
    args->AddFlag("T", "Test archive via walk-through [non-standard]");
    args->AddKey ("f", "archive_filename"
#ifdef TEST_CONN_TAR
                  "_or_url"
#endif // TEST_CONN_TAR
                  , "Archive file name to operate on"
#ifdef TEST_CONN_TAR
                  ", or URL to list/extract/test/stream-through"
#endif // TEST_CONN_TAR
                  ";\nuse '-' for stdin/stdout",
                  CArgDescriptions::eString);
    args->AddOptionalKey("C", "directory",
                         "Set base directory", CArgDescriptions::eString);
    args->AddDefaultKey ("b", "blocking_factor",
                         "Archive block size in 512-byte units\n"
                         "(10K blocks in use by default)",
                         CArgDescriptions::eInteger, "20");
    args->AddOptionalKey("X", "exclude",
                         "Exclude pattern", CArgDescriptions::eString,
                         CArgDescriptions::fAllowMultiple);
    args->SetConstraint ("b", new CArgAllow_Integers(1, (1 << 22) - 1));
    args->AddFlag("i", "Ignore zero blocks");
    args->AddFlag("h", "Follow links");
    args->AddFlag("p", "Preserve all permissions");
    args->AddFlag("m", "Don't extract modification times");
    args->AddFlag("o", "Don't extract file ownerships");
    args->AddFlag("M", "Don't extract permission masks");
    args->AddFlag("O", "Extract to stdout (rather than files) when streaming");
    args->AddFlag("U", "Only existing files/entries in update/extract");
    args->AddFlag("B", "Create backup copies of destinations when extracting");
    args->AddFlag("k", "Don't overwrite[keep] existing files when extracting");
    args->AddFlag("E", "Maintain equal types of files and archive entries");
    args->AddFlag("I", "Ignore unsupported entries (w/o extracting them)");
    args->AddFlag("z", "Use GZIP compression (aka tgz), subsumes NOT -r / -u");
    args->AddFlag("s", "Use stream operations with archive [non-standard]");
    args->AddFlag("S", "Stream archive through [non-standard]");
    args->AddFlag("Z", "No NCBI signature in headers [non-standard]");
    args->AddFlag("v", "Turn on debugging information");
    args->AddFlag("lfs", "Large File Support check [non-standard]");
    args->AddExtra(0/*no mandatory*/, kMax_UInt/*unlimited optional*/,
                   "List of entries to process", CArgDescriptions::eString);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "Tar test suite: a simplified tar utility"
                          " [with some extras]");
    SetupArgDescriptions(args.release());
}


static int x_CheckLFS(const CArgs& args, string& path)
{
    CDirEntry::SStat st;
    int    nolfs = sizeof(st.orig.st_size) <= 4 ? -1 : 0;
    bool   verbose = args["v"].HasValue() ? true : false;
    string message = ((path.empty() ? "Large File Support (LFS)" : "LFS")
                      + string(nolfs ? " is not present" : " is present"));
    if (!path.empty()) {
        CDirEntry file(path);
        if (file.Stat(&st, eFollowLinks)) {
            CDirEntry::EType type = CDirEntry::GetType(st.orig);
            if (type == CDirEntry::eFile) {
                CNcbiIfstream ifs(path.c_str(),
                                  IOS_BASE::in | IOS_BASE::binary);
                if (!ifs) {
                    message +=
                        string(nolfs ? " and" : " but") +
                        " C++ run-time cannot work with";
                    nolfs = -1;
                } else {
                    message +=
                        string(nolfs ? " but" : " and") +
                        " TAR API should work for";
                    nolfs = 0;
                }
            } else {
                message += " for ";
                nolfs = 0;
            }
        } else if (errno == EOVERFLOW) {
            message +=
                string(nolfs ? " and" : " but") +
                " TAR API may not work";
            path.erase();
            nolfs = -1;
        } else {
            message +=
                string(nolfs ? " and" : " but") +
                " nothing can be figured out for";
        }
        if (!path.empty())
            message += " `" + path + '\'';
    }
    if (verbose) {
        NcbiCerr << message << NcbiEndl;
    }
    return nolfs;
}


static string x_DataSize(Uint8 size, bool binary = true)
{
    static const char* kSfx[] = {"", "K", "M", "G", "T", "P", "E", "Z", "Y",0};
    if (!size)
        return "0";
    const Uint8 kilo = binary ? 1 << 10 : 1000;
    size_t idx = 0;
    int prec = 0;
    size *= 1000;
    for (idx = 0;  idx < sizeof(kSfx) / sizeof(kSfx[0]);  ++idx) {
        if (size <= kilo * 1000)
            break;
        size /= kilo;
        if (prec < 3)
            prec++;
    }
    CNcbiOstrstream os;
    os << NcbiFixed << NcbiSetprecision(prec) << (double) size / 1000.0
       << (kSfx[idx] ?           kSfx[idx]               : "?")
       << (kSfx[idx] ? "iB" + (!*kSfx[idx]  ||  !binary) : "");
    return CNcbiOstrstreamToString(os);
}


string CTarTest::x_Pos(const CTarEntryInfo& info)
{
    Uint8 header_pos = info.GetPosition(CTarEntryInfo::ePos_Header);
    Uint8 data_pos   = info.GetPosition(CTarEntryInfo::ePos_Data);
    _ASSERT((header_pos & 0777) == 0  &&  (data_pos & 0777) == 0);
    if (!(m_Flags & fVerbose))
        return kEmptyStr;
    string pos(" at block "
               + NStr::UInt8ToString(header_pos >> 9, NStr::fWithCommas));
    Uint8 size = info.GetSize();
    if (size) {
        pos += (" (data at "
                + NStr::UInt8ToString(data_pos >> 9, NStr::fWithCommas)
                + ": " + x_DataSize(size) + ')');
    }
    return pos;
}


auto_ptr<CTar::TEntries> CTarTest::x_Append(CTar& tar, const string& name)
{
    CDirEntry entry(name);
    CDirEntry::EType type = entry.GetType(eFollowLinks);
    if (type == CDirEntry::eDir) {
        auto_ptr<CTar::TEntries> entries(new CTar::TEntries);
        CDir::TEntries dir = CDir(name).GetEntries("*",CDir::eIgnoreRecursive);
        ITERATE(CDir::TEntries, e, dir) {
            auto_ptr<CTar::TEntries> add = x_Append(tar, (*e)->GetPath());
            entries->splice(entries->end(), *add);
        }
        return entries;
    }
    if (type == CDirEntry::eFile) {
        Uint8 size = CFile(name).GetLength();
        CNcbiIfstream ifs(name.c_str(), IOS_BASE::in | IOS_BASE::binary);
        return tar.Append(CTarUserEntryInfo(name, size), ifs);
    }
    return tar.Append(name);
}


int CTarTest::Run(void)
{
    const CArgs& args = GetArgs();

    _ASSERT(args["f"].HasValue());
    string file = args["f"].AsString();
    if (file == "-") {
        file.erase();
    }

    if (args["lfs"].HasValue()) {
        // Special priority treatment of this flag
        return x_CheckLFS(args, file);
    }

    enum EAction {
        eNone    =  0,
        eCreate  = (1 << 0),
        eAppend  = (1 << 1),
        eUpdate  = (1 << 2),
        eList    = (1 << 3),
        eExtract = (1 << 4),
        eTest    = (1 << 5)
    };
    unsigned int action = eNone;

    if (args["c"].HasValue()) {
        action |= eCreate;
    }
    if (args["r"].HasValue()) {
        action |= eAppend;
    }
    if (args["u"].HasValue()) {
        action |= eUpdate;
    }
    if (args["t"].HasValue()) {
        action |= eList;
    }
    if (args["x"].HasValue()) {
        action |= eExtract;
    }
    if (args["T"].HasValue()) {
        action |= eTest;
    }
    if (!action  ||  (action & (action - 1))) {
        NCBI_THROW(CArgException, eInvalidArg,
                   "You must specify exactly one of -c, -r, -u, -t, -x, -T");
    }

    bool zip = args["z"].HasValue();
    if (zip  &&  (action == eAppend  ||  action == eUpdate)) {
        NCBI_THROW(CArgException, eInvalidArg,
                   "Sorry, -z is not supported with either -r or -u");
    }

    size_t blocking_factor = args["b"].AsInteger();
    bool   pipethru        = args["S"].HasValue();
    bool   stream          = args["s"].HasValue();
    bool   verbose         = args["v"].HasValue();
    size_t n               = args.GetNExtra();

    if (verbose) {
        SetDiagPostLevel(eDiag_Info);
#if defined(_DEBUG)  &&  !defined(NDEBUG)
        SetDiagTrace(eDT_Enable);
#endif /*_DEBUG && !NDEBUG*/
    }

    auto_ptr<CTar>     tar;
    auto_ptr<CNcbiIos> zs;
    CNcbiIstream* in = 0;
    CNcbiIfstream ifs;
    CNcbiOfstream ofs;
    CNcbiIos* io;

#ifdef TEST_CONN_TAR
    CCanceled canceled;
    auto_ptr<CConn_IOStream> conn;
    if (NStr::FindNoCase(file, "://", 3, 5) != NPOS) {
#  ifdef HAVE_LIBGNUTLS
        SOCK_SetupSSL(NcbiSetupGnuTls);
#  endif // HAVE_LIBGNUTLS
        SOCK_SetInterruptOnSignalAPI(eOn);
        if (action == eList  ||  action == eExtract  ||  action == eTest
            ||  (pipethru  &&  action != eCreate)) {
            conn.reset(NcbiOpenURL(file));
            file.erase();
        } else {
            NCBI_THROW(CArgException, eInvalidArg,
                       "Sorry, URL is not supported with this action/mode");
        }
        if (!(in = conn.get())) {
            in = &ifs;
            in->clear(NcbiBadbit);
        } else {
            conn->SetCanceledCallback(&canceled);
        }
    } else
#endif // TEST_CONN_TAR
        if (pipethru  &&  !file.empty()) {
            ifs.open(file.c_str(), IOS_BASE::in | IOS_BASE::binary);
            file.erase();
            in = &ifs;
        }

#ifdef NCBI_OS_MSWIN
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif // NCBI_OS_MSWIN

    CRWStream stdio(!in  ||  in->good()
                    ? new CStreamReader(in ? *in : NcbiCin)
                    : 0,
                    !in  ||  in->good()
                    ? new CStreamWriter(           NcbiCout)
                    : 0,
                    0, 0, CRWStreambuf::fOwnAll | CRWStreambuf::fUntie);

    if (file.empty()  ||  zip) {
        if (action == eCreate  ||  action == eAppend  ||  action == eUpdate) {
            CNcbiOstream* os;
            if (!file.empty()) {
                ofs.open(file.c_str(),
                         IOS_BASE::trunc | IOS_BASE::out | IOS_BASE::binary);
                os = &ofs;
            } else if (action != eUpdate  ||  n < 2) {
                os = &stdio;
            } else {
                NCBI_THROW(CArgException, eInvalidArg, "2+fold pipe update");
            }
            if (!os->good()) {
                NCBI_THROW(CTarException, eOpen, "Archive not found");
            }
            if (zip) {
                // Very hairy :-)
                CZipCompressor* zc = new CZipCompressor;
                zc->SetFlags(CZipCompression::fWriteGZipFormat);
                if (!file.empty()) {
                    CZipCompression::SFileInfo info;
                    string tarfile = CDirEntry(file).GetBase();
                    if (!NStr::EndsWith(tarfile, ".tar"))
                        tarfile += ".tar";
                    info.name  = tarfile;
                    info.mtime = CTime(CTime::eCurrent).GetTimeT();
                    zc->SetFileInfo(info);
                }
                os = new CCompressionOStream
                    (*os, new CCompressionStreamProcessor
                     (zc, CCompressionStreamProcessor::eDelete),
                     CCompressionStream::fOwnProcessor);
                zs.reset(os);
            }
            io = os;
        } else {
            _ASSERT(action == eList || action == eExtract || action == eTest);
            CNcbiIstream* is;
            if (!file.empty()) {
                _ASSERT(!ifs.is_open());
                ifs.open(file.c_str(), IOS_BASE::in | IOS_BASE::binary);
                is = &ifs;
            } else {
                is = &stdio;
            }
            if (!is->good()) {
                NCBI_THROW(CTarException, eOpen, "Archive not found");
            }
            if (zip) {
                is = new CDecompressIStream(*is, CCompressStream::eGZipFile);
                zs.reset(is);
            }
            io = is;
        }
    } else {
        io = 0;
    }
    if (action != eExtract  ||  !stream  ||  n != 1) {
        if (io) {
            if (zip  &&  pipethru) {
                NCBI_THROW(CArgException, eInvalidArg, "Write-thru zip pipe");
            }
            tar.reset(new CTar(*io,  blocking_factor));
        } else {
            tar.reset(new CTar(file, blocking_factor));
        }
        m_Flags = tar->GetFlags();
    } else {
        m_Flags = 0;
    }

    if (verbose) {
        m_Flags |=  fVerbose;
    }
    if (args["i"].HasValue()) {
        m_Flags |=  CTar::fIgnoreZeroBlocks;
    }
    if (args["h"].HasValue()) {
        m_Flags |=  CTar::fFollowLinks;
    }
    if (args["p"].HasValue()) {
        m_Flags |=  CTar::fPreserveAll;
    }
    if (args["m"].HasValue()) {
        m_Flags &= ~CTar::fPreserveTime;
    }
    if (args["o"].HasValue()) {
        m_Flags &= ~CTar::fPreserveOwner;
    }
    if (args["M"].HasValue()) {
        m_Flags &= ~CTar::fPreserveMode;
    }
    if (args["U"].HasValue()) {
        m_Flags |=  CTar::fUpdate;
    }
    if (args["B"].HasValue()) {
        m_Flags |=  CTar::fBackup;
    }
    if (args["k"].HasValue()) {
        m_Flags &= ~CTar::fOverwrite;
    }
    if (args["E"].HasValue()) {
        m_Flags |=  CTar::fEqualTypes;
    }
    if (args["I"].HasValue()) {
        m_Flags |=  CTar::fSkipUnsupported;
    }
    if (args["Z"].HasValue()) {
        m_Flags |=  CTar::fStandardHeaderOnly;
    }
    if (pipethru) {
        m_Flags |=  CTar::fStreamPipeThrough;
    }
    auto_ptr<CMask> exclude;
    if (args["X"].HasValue()) {
        exclude.reset(new CMaskFileName);
        const CArgValue::TStringArray& values = args["X"].GetStringList();
        ITERATE(CArgValue::TStringArray, it, values) {
            exclude->Add(*it);
        }
    }

    CStopWatch sw(CStopWatch::eStart);
    if (!tar.get()) {
        _ASSERT(action == eExtract  &&  stream  &&  n == 1);
        if (!io) {
            _ASSERT(!file.empty()  &&  !zip  &&  !ifs.is_open());
            ifs.open(file.c_str(), IOS_BASE::in | IOS_BASE::binary);
            if (!ifs.good()) {
                NCBI_THROW(CTarException, eOpen, "Archive not found");
            }
        }
        CNcbiIstream& is = io ? dynamic_cast<CNcbiIstream&>(*io) : ifs;
        IReader* ir = CTar::Extract(is, args[1].AsString(), m_Flags);
        if (!ir) {
            NCBI_THROW(CTarException, eBadName,
                       "Entry either not found or has a non-file type");
        }
        CRStream rs(ir, 0, 0, (CRWStreambuf::fOwnReader |
                               CRWStreambuf::fLogExceptions));
        CNcbiOfstream of;
        bool tocout = args["O"].HasValue();
        if (!tocout  ||  pipethru) {
            of.open(!tocout ? args[1].AsString().c_str() : DEVNULL,
                    IOS_BASE::trunc | IOS_BASE::out | IOS_BASE::binary);
        }
        if (!NcbiStreamCopy(!tocout  ||  pipethru ? of : NcbiCout, rs)) {
            // NB: CTar would throw archive read exception
            string errmsg("Write error");
            if (!tocout) {
                errmsg += " in \"";
                errmsg += args[1].AsString();
                errmsg += '"';
            }
            NCBI_THROW(CTarException, eWrite, errmsg);
        }
    } else {
        tar->SetFlags(m_Flags);
        if (args["C"].HasValue()) {
            tar->SetBaseDir(args["C"].AsString());
        }
        tar->SetMask(exclude.get(), eNoOwnership, CTar::eExcludeMask);
        if (action == eCreate  ||  action == eAppend  ||  action == eUpdate) {
            if (!n) {
                NCBI_THROW(CArgException, eInvalidArg, "Must specify file(s)");
            }
            if (action == eCreate) {
                tar->Create();
            }
            const string& what =   action == eUpdate ? "Updating " : "Adding ";
            const string& pfx  = (stream ?
                                  (action == eUpdate ? "U "        : "A ") :
                                  (action == eUpdate ? "u "        : "a "));
            CTar::TEntries entries;
            for (size_t i = 1;  i <= n;  ++i) {
                string name = args[i].AsString();
                auto_ptr<CTar::TEntries> add;
                NcbiCerr << what << name << NcbiEndl;
                if (action == eUpdate) {
                    _ASSERT(n  &&  !io);
                    add = tar->Update(name);
                } else if (stream) {
                    add = x_Append(*tar, name);
                } else {
                    add = tar->Append(name);
                }
                entries.splice(entries.end(), *add);
            }
            if (m_Flags & fVerbose) {
                ITERATE(CTar::TEntries, it, entries) {
                    NcbiCerr << pfx << it->GetName() + x_Pos(*it) << NcbiEndl;
                }
            }
            tar->Close();  // finalize TAR file before streams close (below)
        } else if (action == eList  ||  action == eExtract) {
            if (n) {
                auto_ptr<CMaskFileName> mask(new CMaskFileName);
                for (size_t i = 1;  i <= n;  ++i) {
                    mask->Add(args[i].AsString());
                }
                tar->SetMask(mask.release(), eTakeOwnership);
            }
            if (action == eList) {
                if (stream) {
                    const CTarEntryInfo* info;
                    while ((info = tar->GetNextEntryInfo()) != 0) {
                        NcbiCerr << *info << x_Pos(*info) << NcbiEndl;
                    }
                } else {
                    auto_ptr<CTar::TEntries> entries = tar->List();
                    ITERATE(CTar::TEntries, it, *entries.get()) {
                        NcbiCerr << *it << x_Pos(*it) << NcbiEndl;
                    }
                }
            } else if (stream) {
                const CTarEntryInfo* info;
                while ((info = tar->GetNextEntryInfo()) != 0) {
                    if (info->GetType() != CTarEntryInfo::eFile) {
                        continue;
                    }
                    if (m_Flags & fVerbose) {
                        NcbiCerr << "X " << info->GetName() + x_Pos(*info)
                                 << NcbiEndl;
                    }
                    IReader* ir = tar->GetNextEntryData();
                    _ASSERT(ir);
                    CRStream rs(ir, 0, 0, (CRWStreambuf::fOwnReader |
                                           CRWStreambuf::fLogExceptions));
                    CNcbiOfstream of;
                    bool tocout = args["O"].HasValue();
                    if (!tocout  ||  pipethru) {
                        of.open(!tocout ? info->GetName().c_str() : DEVNULL,
                                IOS_BASE::trunc |
                                IOS_BASE::out   |
                                IOS_BASE::binary);
                    }
                    if (!NcbiStreamCopy(!tocout || pipethru ? of:NcbiCout,rs)){
                        // NB: CTar would throw archive read exception
                        string errmsg("Write error");
                        if (!tocout) {
                            errmsg += " in \"";
                            errmsg += info->GetName();
                            errmsg += '"';
                        }
                        NCBI_THROW(CTarException, eWrite, errmsg);
                    }
                }
            } else {
                auto_ptr<CTar::TEntries> entries = tar->Extract();
                if (m_Flags & fVerbose) {
                    ITERATE(CTar::TEntries, it, *entries.get()) {
                        NcbiCerr << "x " << it->GetName() + x_Pos(*it)
                                 << NcbiEndl;
                    }
                }
            }
            if (pipethru) {
                tar->Close();
            }
        } else {
            _ASSERT(action == eTest);
            if (n) {
                NCBI_THROW(CArgException, eInvalidArg, "Args not allowed");
            }
            NcbiCerr << "Testing archive... " << NcbiFlush;
            tar->Test();
            NcbiCerr << "Done." << NcbiEndl;
            if (pipethru) {
                tar->Close();
            }
        }
    }
    Uint8 pos = tar.get() ? tar->GetCurrentPosition() : 0;

    zs.reset(0);       // make sure zip stream gets finalized...
    if (ofs.is_open()) {
        ofs.close();   // ...before the output file gets closed
    }

    if (pos  &&  (m_Flags & fVerbose)) {
        double elapsed = sw.Elapsed();
        Uint8  rate = elapsed ? (Uint8)(pos / elapsed) : 0; 
        NcbiCerr << NStr::UInt8ToString(pos, NStr::fWithCommas)
                 << " archive byte" << &"s"[pos == 1] << " processed in "
                 << CTimeSpan(elapsed + 0.5).AsString("h:m:s")
                 << (rate ? " " + x_DataSize(rate, false) + "/s" : kEmptyStr)
                 << NcbiEndl;
    }
    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
#ifdef NCBI_OS_UNIX
    signal(SIGPIPE, SIG_IGN);
#endif // NCBI_OS_UNIX
    // Execute main application function
    return CTarTest().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
