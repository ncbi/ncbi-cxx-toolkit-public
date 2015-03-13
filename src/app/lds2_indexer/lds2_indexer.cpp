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
 * Author:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objtools/lds2/lds2.hpp>

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CLDS2IndexerApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

private:
private:
};


#define GB_RELEASE_MODE_NONE "none"
#define GB_RELEASE_MODE_GUESS "guess"
#define GB_RELEASE_MODE_FORCE "force"


void CLDS2IndexerApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        "LDS2 Indexer", false);

    arg_desc->AddKey("source", "path_name",
        "Paht to the directory with source data files",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("db", "db_name",
        "Path to the LDS2 database file",
        CArgDescriptions::eString);

    arg_desc->AddFlag("norecursive",
        "Do not search for source files recursively");

    arg_desc->AddOptionalKey("gb_release", "gb_release_mode",
        "Mode of GB release file detection",
        CArgDescriptions::eString);
    arg_desc->SetConstraint("gb_release",
                            &(*new CArgAllow_Strings,
                              GB_RELEASE_MODE_NONE,
                              GB_RELEASE_MODE_GUESS,
                              GB_RELEASE_MODE_FORCE));

/*
    arg_desc->AddFlag("abs_path",
        "Use absolute path to data files (default)");
    arg_desc->AddFlag("keep_path",
        "Keep original path to data files");
    arg_desc->AddFlag("keep_other",
        "Keep files outside source dir indexed.");

*/

    arg_desc->AddOptionalKey("group_aligns", "group_size",
        "Group standalone seq-aligns into blobs",
        CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("dump_table", "table_name",
        "Dump LDS2 table content",
        CArgDescriptions::eString);
    arg_desc->AddDefaultKey("dump_file", "file_name",
        "Dump destination", CArgDescriptions::eOutputFile, "-");
    SetupArgDescriptions(arg_desc.release());
}


int CLDS2IndexerApplication::Run(void)
{
    string lds2_section_name = "lds2";
    const CNcbiRegistry& reg = GetConfig();
    string db_path = reg.Get(lds2_section_name,  "Path",
        IRegistry::fTruncate);
    string source_path = reg.Get(lds2_section_name,  "Source",
        IRegistry::fTruncate);

    const CArgs& args = GetArgs();
    if (args["source"]) {
        source_path = args["source"].AsString();
    }

    if (args["db"]) {
        db_path = args["db"].AsString();
    }
    else {
        db_path = CDirEntry::ConcatPath(source_path, "lds2.db");
    }

    CLDS2_Manager mgr(db_path);

    if ( args["gb_release"] ) {
        string mode = args["gb_release"].AsString();
        if ( mode == GB_RELEASE_MODE_NONE ) {
            mgr.SetGBReleaseMode(CLDS2_Manager::eGB_Ignore);
        }
        if ( mode == GB_RELEASE_MODE_GUESS ) {
            mgr.SetGBReleaseMode(CLDS2_Manager::eGB_Guess);
        }
        if ( mode == GB_RELEASE_MODE_FORCE ) {
            mgr.SetGBReleaseMode(CLDS2_Manager::eGB_Force);
        }
    }

    if ( args["group_aligns"] ) {
        mgr.SetSeqAlignGroupSize(args["group_aligns"].AsInteger());
    }

    if ( args["dump_table"] ) {
        mgr.GetDatabase()->Dump(args["dump_table"].AsString(), args["dump_file"].AsOutputFile());
    }
    else {
        mgr.AddDataDir(source_path, args["norecursive"] ?
            CLDS2_Manager::eDir_NoRecurse : CLDS2_Manager::eDir_Recurse);
        mgr.UpdateData();
    }



/*
    if ( args["keep_path"] ) {
        if ( args["abs_path"] ) {
            ERR_FATAL("Conflicting options: -abs_path and -keep_path");
        }
        flags = (flags & ~CLDS_Manager::fPathMask) |
            CLDS_Manager::fOriginalPath;
    }
    if ( args["abs_path"] ) {
        flags = (flags & ~CLDS_Manager::fPathMask) |
            CLDS_Manager::fAbsolutePath;
    }

    if ( args["keep_other"] ) {
        flags = (flags & ~CLDS_Manager::fOtherFilesMask) |
            CLDS_Manager::fKeepOtherFiles;
    }

*/

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CLDS2IndexerApplication().AppMain(argc, argv);
}
