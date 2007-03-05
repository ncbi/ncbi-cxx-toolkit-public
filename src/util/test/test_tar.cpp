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
#include <util/compress/tar.hpp>
#include <test/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CNcbiApplication
{
public:
    CTest();

protected:
    enum EAction {
        fNone               =  0,
        fCreate             = (1 << 0),
        fAppend             = (1 << 1),
        fUpdate             = (1 << 2),
        fUpdateExistingOnly = (1 << 3),
        fList               = (1 << 4),
        fExtract            = (1 << 5),
        fTest               = (1 << 6)
    };
    typedef unsigned int TAction;

protected:
    virtual void Init(void);
    virtual int  Run(void);
};


CTest::CTest()
{
    DisableArgDescriptions(fDisableStdArgs);
    HideStdArgs(-1/*everything*/);
}


void CTest::Init(void)
{
    auto_ptr<CArgDescriptions> args(new CArgDescriptions);
    if (args->Exist("h")) {
        args->Delete("h");
    }
    args->AddFlag("c", "Create archive");
    args->AddFlag("r", "Append archive");
    args->AddFlag("u", "Update archive");
    args->AddFlag("U", "Update archive (existing only) [non-standard]");
    args->AddFlag("t", "Table of contents");
    args->AddFlag("x", "Extract archive");
    args->AddFlag("T", "Test archive [non-standard]");
    args->AddOptionalKey("f", "filename",
                         "Default = cin/cout", CArgDescriptions::eString);
    args->AddOptionalKey("C", "directory",
                         "Set base directory", CArgDescriptions::eString);
    args->AddDefaultKey("b", "blocking_factor",
                        "Archive block size in 512-byte units"
                        " (default corresponds to 10K blocks to use)",
                        CArgDescriptions::eInteger, "20");
    args->SetConstraint("b", new CArgAllow_Integers(1, (1 << 22) - 1));
    args->AddFlag("i", "Ignore zero blocks");
    args->AddExtra(0, 1<<20, "List of files to process",
                   CArgDescriptions::eString);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "Tar test suite: VERY simplified tar utility");
    SetupArgDescriptions(args.release());

    SetDiagPostLevel(eDiag_Warning);
}


int CTest::Run(void)
{
    TAction action = fNone;

    const CArgs& args = GetArgs();

    if (args["c"].HasValue())
        action |= fCreate;
    if (args["r"].HasValue())
        action |= fAppend;
    if (args["u"].HasValue())
        action |= fUpdate;
    if (args["U"].HasValue())
        action |= fUpdateExistingOnly;
    if (args["t"].HasValue())
        action |= fList;
    if (args["x"].HasValue())
        action |= fExtract;
    if (args["T"].HasValue())
        action |= fTest;

    if (!action  ||  (action & (action - 1))) {
        NCBI_THROW(CArgException, eInvalidArg,
                   "You have to specify exactly one of "
                   "c, r, u, U, t, x, or T");
    }

    size_t blocking_factor = args["b"].AsInteger();

    string file;
    if (args["f"].HasValue()) {
        file = args["f"].AsString();
        if (file == "-") {
            file.erase();
        }
    }

    auto_ptr<CTar> tar;

    if (file.empty()) {
        CNcbiIos* io = 0;
        if (action == fList  ||  action == fExtract  ||  action == fTest) {
            io = &cin;
        } else if (action == fCreate  ||  action == fAppend) {
            io = &cout;
        } else {
            NCBI_THROW(CArgException, eInvalidArg, "Cannot update in a pipe");
        }
        _ASSERT(io);
        tar.reset(new CTar(*io, blocking_factor));
    } else {
        tar.reset(new CTar(file, blocking_factor));
    }

    if (args["C"].HasValue()) {
        tar->SetBaseDir(args["C"].AsString());
    }

    if (args["i"].HasValue()) {
        tar->SetFlags(tar->GetFlags() | CTar::fIgnoreZeroBlocks);
    }

    if (args["U"].HasValue()) {
        tar->SetFlags(tar->GetFlags() | CTar::fUpdateExistingOnly);
    }

    if (action == fCreate) {
        tar->Create();
    }
    auto_ptr<CTar::TEntries> entries;
    if (action == fCreate  ||  action == fAppend  ||
        action == fUpdate  ||  action == fUpdateExistingOnly) {
        size_t n = args.GetNExtra();
        if (n == 0) {
            NCBI_THROW(CArgException, eInvalidArg, "Must specify file(s)");
        }
        for (size_t i = 1;  i <= n;  i++) {
            const string& name   = args[i].AsString();
            const string& what   =
                action == fUpdate  ||  action == fUpdateExistingOnly
                ? "Updating " : "Adding ";
            const string& prefix =
                action == fUpdate  ||  action == fUpdateExistingOnly
                ? "u "        : "a ";
            LOG_POST(what << name);
            if (action == fUpdate  ||  action == fUpdateExistingOnly) {
                entries.reset(tar->Update(name).release());
            } else {
                entries.reset(tar->Append(name).release());
            }
            if (!entries->empty()) {
                ITERATE(CTar::TEntries, it, *entries.get()) {
                    LOG_POST(prefix << it->GetName());
                }
            }
        }
    } else if (action == fTest) {
        if (args.GetNExtra()) {
            NCBI_THROW(CArgException, eInvalidArg, "Extra args not allowed");
        }
        cerr << "Testing archive... ";
        cerr.flush();
        tar->Test();
        cerr << "Done." << endl;
    } else {
        size_t n = args.GetNExtra();
        if (n) {
            auto_ptr<CMaskFileName> mask(new CMaskFileName);
            for (size_t i = 1;  i <= n;  i++) {
                mask->Add(args[i].AsString());
            }
            tar->SetMask(mask.release(), eTakeOwnership);
        }
        if (action == fList) {
            entries.reset(tar->List().release());
            ITERATE(CTar::TEntries, it, *entries.get()) {
                LOG_POST(*it);
            }
        } else {
            entries.reset(tar->Extract().release());
            ITERATE(CTar::TEntries, it, *entries.get()) {
                LOG_POST("x " << it->GetName());
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
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
