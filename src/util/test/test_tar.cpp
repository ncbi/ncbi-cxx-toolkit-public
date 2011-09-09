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
 *                    basically, a very limited verion of a tar utility.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/rwstream.hpp>
#include <util/compress/tar.hpp>
#include <errno.h>
#ifdef NCBI_OS_MSWIN
#  include <io.h>     // For _setmode()
#  include <fcntl.h>  // For _O_BINARY
#endif // NCBI_OS_MSWIN

#include <common/test_assert.h>  // This header must go last


#ifndef   EOVERFLOW
#  define EOVERFLOW (-1)
#endif // EOVERFLOW


USING_NCBI_SCOPE;


const unsigned int fVerbose = CTar::fDumpEntryHeaders;


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTarTest : public CNcbiApplication
{
public:
    CTarTest();

protected:
    virtual void Init(void);
    virtual int  Run(void);

    string x_Pos(const CTarEntryInfo& info);

protected:
    CTar::TFlags m_Flags;

    enum EAction {
        eNone      =  0,
        eCreate    = (1 << 0),
        eAppend    = (1 << 1),
        eUpdate    = (1 << 2),
        eList      = (1 << 3),
        eExtract   = (1 << 4),
        eTest      = (1 << 5)
    };
    typedef unsigned int TAction;

    auto_ptr<CTar::TEntries> x_Append(CTar& tar, const string& name);
};


CTarTest::CTarTest()
    : m_Flags(0)
{
#if defined(__GLIBCPP__) || (defined(__GLIBCXX__)  &&  __GLIBCXX__ < 20060524)
    // a/ sync_with_stdio(false) is 100% buggy in GCC < 4.1.1; or
    // b/ sync_with_stdio(any) is 100% buggy in Solaris STL;
    // when stdin's positioning methods (failing or not) get called...
    SetStdioFlags(fDefault_SyncWithStdio);
#endif // __GLIBCPP__ || (__GLIBCXX__  &&  __GLIBCXX__ < 20060524)
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostAllFlags(eDPF_DateTime    | eDPF_Severity |
                        eDPF_OmitInfoSev | eDPF_ErrorID);
    DisableArgDescriptions(fDisableStdArgs);
    HideStdArgs(-1/*everything*/);
}


void CTarTest::Init(void)
{
    auto_ptr<CArgDescriptions> args(new CArgDescriptions);
    args->PrintUsageIfNoArgs();
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
    args->AddFlag("T", "Test archive [non-standard]");
    args->AddKey ("f", "archive_file_name",
                  "Archive file name;  use '-' for stdin/stdout",
                  CArgDescriptions::eString);
    args->AddOptionalKey("C", "directory",
                         "Set base directory", CArgDescriptions::eString);
    args->AddDefaultKey ("b", "blocking_factor",
                         "Archive block size in 512-byte units"
                         " (10K blocks in use by default)",
                         CArgDescriptions::eInteger, "20");
    args->SetConstraint ("b", new CArgAllow_Integers(1, (1 << 22) - 1));
    args->AddFlag("i", "Ignore zero blocks");
    args->AddFlag("L", "Follow links");
    args->AddFlag("p", "Preserve all permissions");
    args->AddFlag("m", "Don't extract modification times");
    args->AddFlag("O", "Don't extract file ownerships");
    args->AddFlag("M", "Don't extract permission masks");
    args->AddFlag("U", "Only existing files/entries in update/extract");
    args->AddFlag("B", "Create backup copies of destinations when extracting");
    args->AddFlag("E", "Maintain equal types of files and archive entries");
    args->AddFlag("S", "Skip unsupported entries instead of extracting them");
    args->AddFlag("Z", "No NCBI signature in headers [non-standard]");
    args->AddFlag("k", "Keep old files when extracting");
    args->AddFlag("v", "Turn on debugging information");
    args->AddFlag("s", "Use stream operations with archive");
    args->AddFlag("lfs", "Large File Support check [non-standard]");
    args->AddExtra(0/*no mandatory*/, kMax_UInt/*unlimited optional*/,
                   "List of files to process", CArgDescriptions::eString);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "Tar test suite: a simplified tar utility"
                          " [with some extras]");
    SetupArgDescriptions(args.release());
}


static int x_CheckLFS(const CArgs& args, const string& path)
{
    CDirEntry::SStat st;
    int  nolfs = sizeof(st.orig.st_size) <= 4 ? -1 : 0;
    bool verbose = args["v"].HasValue() ? true : false;
    string message = (string(path.empty() ? "Large File Support (LFS)" : "LFS")
                      + string(nolfs ? " is not present" : " is present"));
    if (!path.empty()) {
        CDirEntry file(path);
        if (file.Stat(&st, eFollowLinks)) {
            CDirEntry::EType type = CDirEntry::GetType(st.orig);
            if (type == CDirEntry::eFile) {
                ifstream ifs(path.c_str(), IOS_BASE::in | IOS_BASE::binary);
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
                message +=
                    string(nolfs ? " but" : " and") +
                    " TAR API should work for";
                nolfs = 0;
            }
        } else if (errno == EOVERFLOW) {
            message +=
                string(nolfs ? " and" : " but") +
                " TAR API may not work for";
            nolfs = -1;
        } else {
            message +=
                string(nolfs ? " and" : " but") +
                " nothing can be figured for";
        }
        message += " `" + path + '\'';
    }
    if (verbose) {
        LOG_POST(message);
    }
    return nolfs;
}


static string x_DataSize(Uint8 size, bool binary = true)
{
    const char* kSfx[] = {"", "K", "M", "G", "T", "P", "E", "Z", "Y", 0};
    if (!size)
        return "0";
    const Uint8 kilo = binary ? 1 << 10 : 1000;
    size_t idx = 0;
    int prec = 0;
    size *= 1000;
    for (idx = 0;  idx < sizeof(kSfx)/sizeof(kSfx[0]);  idx++) {
        if (size <= kilo * 1000)
            break;
        size /= kilo;
        if (prec < 3)
            prec++;
    }
    CNcbiOstrstream os;
    os << fixed << setprecision(prec) << (double) size / 1000.0
       << (kSfx[idx] ?           kSfx[idx]               : "?")
       << (kSfx[idx] ? "iB" + (!*kSfx[idx]  ||  !binary) : "");
    return CNcbiOstrstreamToString(os);
}


string CTarTest::x_Pos(const CTarEntryInfo& info)
{
    Uint8 header_pos = info.GetPosition(CTarEntryInfo::ePos_Header);
    Uint8 data_pos   = info.GetPosition(CTarEntryInfo::ePos_Data);
    Uint8 size       = info.GetSize();
    _ASSERT((header_pos & 0777) == 0  &&  (data_pos & 0777) == 0);
    if (!(m_Flags & fVerbose))
        return kEmptyStr;
    string pos(" at block "
               + NStr::UInt8ToString(header_pos >> 9, NStr::fWithCommas));
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
        ifstream ifs(name.c_str(), IOS_BASE::in | IOS_BASE::binary);
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

    TAction action = eNone;

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
                   "You must specify exactly one of c, r, u, t, x, T");
    }
    size_t n = args.GetNExtra();

    bool stream = args["s"].HasValue();

    size_t blocking_factor = args["b"].AsInteger();

    auto_ptr<CTar> tar;

    if (action != eExtract  ||  !stream  ||  n != 1) {
        if (file.empty()) {
            CNcbiIos* io = 0;
            if (action == eList ||  action == eExtract  ||  action == eTest) {
                io = &NcbiCin;
#ifdef NCBI_OS_MSWIN
                _setmode(_fileno(stdin), _O_BINARY);
#endif // NCBI_OS_MSWIN
            } else if (action == eCreate  ||  action == eAppend) {
                io = &NcbiCout;
#ifdef NCBI_OS_MSWIN
                _setmode(_fileno(stdout), _O_BINARY);
#endif // NCBI_OS_MSWIN
            } else {
                NCBI_THROW(CArgException, eInvalidArg, "Cannot update pipe");
            }
            _ASSERT(io);
            tar.reset(new CTar(*io,  blocking_factor));
        } else {
            tar.reset(new CTar(file, blocking_factor));
        }
        m_Flags = tar->GetFlags();
    } else {
        m_Flags = 0;
    }

    if (args["i"].HasValue()) {
        m_Flags |=  CTar::fIgnoreZeroBlocks;
    }
    if (args["L"].HasValue()) {
        m_Flags |=  CTar::fFollowLinks;
    }
    if (args["p"].HasValue()) {
        m_Flags |=  CTar::fPreserveAll;
    }
    if (args["m"].HasValue()) {
        m_Flags &= ~CTar::fPreserveTime;
    }
    if (args["O"].HasValue()) {
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
    if (args["E"].HasValue()) {
        m_Flags |=  CTar::fEqualTypes;
    }
    if (args["k"].HasValue()) {
        m_Flags &= ~CTar::fOverwrite;
    }
    if (args["S"].HasValue()) {
        m_Flags |=  CTar::fSkipUnsupported;
    }
    if (args["Z"].HasValue()) {
        m_Flags |=  CTar::fStandardHeaderOnly;
    }
    if (args["v"].HasValue()) {
        m_Flags |=  fVerbose;
    }

    if (tar.get()) {
        tar->SetFlags(m_Flags);

        if (args["C"].HasValue()) {
            tar->SetBaseDir(args["C"].AsString());
        }

        if (action == eCreate) {
            tar->Create();
        }
    }

    if (action == eCreate  ||  action == eAppend  ||  action == eUpdate) {
        if (!n) {
            NCBI_THROW(CArgException, eInvalidArg, "Must specify file(s)");
        }
        const string& what   =   action == eUpdate ? "Updating " : "Adding ";
        const string& prefix = (stream ?
                                (action == eUpdate ? "U "        : "A ") :
                                (action == eUpdate ? "u "        : "a "));
        CTar::TEntries entries;
        for (size_t i = 1;  i <= n;  ++i) {
            string name = args[i].AsString();
            auto_ptr<CTar::TEntries> add;
            LOG_POST(what << name);
            if (action == eUpdate) {
                _ASSERT(n);
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
                LOG_POST(prefix << it->GetName() + x_Pos(*it));
            }
        }
    } else if (action == eTest) {
        if (n) {
            NCBI_THROW(CArgException, eInvalidArg, "Extra args not allowed");
        }
        NcbiCerr << "Testing archive... " << flush;
        tar->Test();
        NcbiCerr << "Done." << endl;
    } else if (action == eExtract  &&  stream  &&  n == 1) {
        ifstream ifs;
        istream& is = file.empty() ? cin : dynamic_cast<istream&>(ifs);
        if (!file.empty()) {
            ifs.open(file.c_str(), IOS_BASE::in | IOS_BASE::binary);
        }
        if (!is.good()) {
            NCBI_THROW(CTarException, eOpen, "Archive not found");
        }
        IReader* ir = CTar::Extract(is, args[1].AsString(), m_Flags);
        if (!ir) {
            NCBI_THROW(CTarException, eBadName,
                       "Entry either not found or has a non-file type");
        }
        CRStream rs(ir, 0, 0, (CRWStreambuf::fOwnReader |
                               CRWStreambuf::fLogExceptions));
        NcbiStreamCopy(NcbiCout, rs);
    } else {
        if (n) {
            auto_ptr<CMaskFileName> mask(new CMaskFileName);
            for (size_t i = 1;  i <= n;  i++) {
                mask->Add(args[i].AsString());
            }
            tar->SetMask(mask.release(), eTakeOwnership);
        }
        if (action == eList) {
            auto_ptr<CTar::TEntries> entries = tar->List();
            ITERATE(CTar::TEntries, it, *entries.get()) {
                LOG_POST(*it << x_Pos(*it));
            }
        } else if (stream) {
            _ASSERT(action == eExtract);
            const CTarEntryInfo* info;
            while ((info = tar->GetNextEntryInfo()) != 0) {
                if (info->GetType() != CTarEntryInfo::eFile) {
                    continue;
                }
                LOG_POST("X " << info->GetName() + x_Pos(*info));
                IReader* ir = tar->GetNextEntryData();
                _ASSERT(ir);
                CRStream rs(ir, 0, 0, (CRWStreambuf::fOwnReader |
                                       CRWStreambuf::fLogExceptions));
                NcbiStreamCopy(NcbiCout, rs);
            }
        } else {
            _ASSERT(action == eExtract);
            auto_ptr<CTar::TEntries> entries = tar->Extract();
            if (m_Flags & fVerbose) {
                ITERATE(CTar::TEntries, it, *entries.get()) {
                    LOG_POST("x " << it->GetName() + x_Pos(*it));
                }
            }
        }
    }

    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTarTest().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
