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
 * Author:  Anton Lavrentiev
 *
 * File Description:  Test case for CTar API:
 *                    basically, a very limited verion of a tar utility.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/rwstream.hpp>
#include <util/compress/tar.hpp>
#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


const unsigned int fVerbose = CTar::fDumpBlockHeaders;


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
        eNone    =  0,
        eCreate  = (1 << 0),
        eAppend  = (1 << 1),
        eUpdate  = (1 << 2),
        eList    = (1 << 3),
        eExtract = (1 << 4),
        eStream  = (1 << 5),
        eTest    = (1 << 6)
    };
    typedef unsigned int TAction;
};


CTarTest::CTarTest()
    : m_Flags(0)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostAllFlags(eDPF_DateTime | eDPF_Severity | eDPF_ErrorID);
    DisableArgDescriptions(fDisableStdArgs);
    HideStdArgs(-1/*everything*/);
}


void CTarTest::Init(void)
{
    auto_ptr<CArgDescriptions> args(new CArgDescriptions);
    args->PrintUsageIfNoArgs();
    if (args->Exist("h")) {
        args->Delete("h");
    }
    if (args->Exist("xmlhelp")) {
        args->Delete("xmlhelp");
    }
    args->AddFlag("c", "Create archive");
    args->AddFlag("r", "Append archive");
    args->AddFlag("u", "Update archive");
    args->AddFlag("t", "Table of contents");
    args->AddFlag("x", "Extract archive");
    args->AddFlag("X", "Stream extract single entry [non-standard]");
    args->AddFlag("T", "Test archive [non-standard]");
    args->AddKey("f", "archive_file_name",
                 "Archive file name;  use '-' for stdin/stdout",
                 CArgDescriptions::eString);
    args->AddOptionalKey("C", "directory",
                         "Set base directory", CArgDescriptions::eString);
    args->AddDefaultKey("b", "blocking_factor",
                        "Archive block size in 512-byte units"
                        " (10K blocks in use by default)",
                        CArgDescriptions::eInteger, "20");
    args->SetConstraint("b", new CArgAllow_Integers(1, (1 << 22) - 1));
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
    args->AddFlag("k", "Keep old files when extracting");
    args->AddFlag("v", "Turn on debugging information");
    args->AddExtra(0/*no mandatory*/, kMax_UInt/*unlimited optional*/,
                   "List of files to process", CArgDescriptions::eString);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "Tar test suite: a simplified tar utility");
    SetupArgDescriptions(args.release());
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
    if (info.GetSize()) {
        pos += (" (data at "
                + NStr::UInt8ToString(data_pos >> 9, NStr::fWithCommas)
                + ')');
    }
    return pos;
}


int CTarTest::Run(void)
{

    TAction action = eNone;

    const CArgs& args = GetArgs();

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
    if (args["X"].HasValue()) {
        action |= eStream;
    }
    if (args["T"].HasValue()) {
        action |= eTest;
    }
    if (!action  ||  (action & (action - 1))) {
        NCBI_THROW(CArgException, eInvalidArg,
                   "You have to specify exactly one of c, r, u, t, x, X, T");
    }

    _ASSERT(args["f"].HasValue());
    string file = args["f"].AsString();
    if (file == "-") {
        file.erase();
    }

    size_t blocking_factor = args["b"].AsInteger();

    auto_ptr<CTar> tar;

    if (action != eStream) {
        if (file.empty()) {
            CNcbiIos* io = 0;
            if (action == eList  ||  action == eExtract ||  action == eTest) {
                io = &cin;
            } else if (action == eCreate  ||  action == eAppend) {
                io = &cout;
            } else {
                NCBI_THROW(CArgException, eInvalidArg, "Cannot update pipe");
            }
            _ASSERT(io);
            tar.reset(new CTar(*io, blocking_factor));
        } else {
            tar.reset(new CTar(file, blocking_factor));
        }
    }

    m_Flags = action != eStream ? tar->GetFlags() : 0;

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
    if (args["v"].HasValue()) {
        m_Flags |=  fVerbose;
    }

    if (action != eStream) {
        tar->SetFlags(m_Flags);

        if (args["C"].HasValue()) {
            tar->SetBaseDir(args["C"].AsString());
        }

        if (action == eCreate) {
            tar->Create();
        }
    }

    auto_ptr<CTar::TEntries> entries;
    if (action == eCreate  ||  action == eAppend  ||  action == eUpdate) {
        size_t n = args.GetNExtra();
        if (n == 0) {
            NCBI_THROW(CArgException, eInvalidArg, "Must specify file(s)");
        }
        for (size_t i = 1;  i <= n;  i++) {
            const string& name   = args[i].AsString();
            const string& what   = action == eUpdate ? "Updating " : "Adding ";
            const string& prefix = action == eUpdate ? "u "        : "a ";
            LOG_POST(what << name);
            if (action == eUpdate) {
                entries.reset(tar->Update(name).release());
            } else {
                entries.reset(tar->Append(name).release());
            }
            if (m_Flags & fVerbose) {
                ITERATE(CTar::TEntries, it, *entries.get()) {
                    LOG_POST(prefix << it->GetName() + x_Pos(*it));
                }
            }
        }
    } else if (action == eTest) {
        if (args.GetNExtra()) {
            NCBI_THROW(CArgException, eInvalidArg, "Extra args not allowed");
        }
        cerr << "Testing archive... " << flush;
        tar->Test();
        cerr << "Done." << endl;
    } else {
        size_t n = args.GetNExtra();
        if (action == eStream) {
            if (n != 1) {
                NCBI_THROW(CArgException, eInvalidArg,
                           "Must specify a single entry");
            }
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
            NcbiStreamCopy(cout, rs);
        } else {
            if (n) {
                auto_ptr<CMaskFileName> mask(new CMaskFileName);
                for (size_t i = 1;  i <= n;  i++) {
                    mask->Add(args[i].AsString());
                }
                tar->SetMask(mask.release(), eTakeOwnership);
            }
            if (action == eList) {
                entries.reset(tar->List().release());
                ITERATE(CTar::TEntries, it, *entries.get()) {
                    LOG_POST(*it << x_Pos(*it));
                }
            } else {
                entries.reset(tar->Extract().release());
                if (m_Flags & fVerbose) {
                    ITERATE(CTar::TEntries, it, *entries.get()) {
                        LOG_POST("x " << it->GetName() + x_Pos(*it));
                    }
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
    return CTarTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
