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

#include <objtools/lds/lds_manager.hpp>

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CLDSIndexerApplication : public CNcbiApplication
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


void CLDSIndexerApplication::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);


    // Program description
    string prog_description = "LDS Indexer";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    arg_desc->AddKey
        ("source", "path_name",
         "Paht to the directory with source data files",
         CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("db_path", "path_name",
         "Path to the directory where LDS database will be created",
         CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("alias", "alias_name",
         "Alias for LDS database",
         CArgDescriptions::eString);

    arg_desc->AddFlag
        ("norecursive",
         "Search for source files recursively");

    arg_desc->AddFlag
        ("crc32",
         "Turn on control sums calculation");
    arg_desc->AddFlag
        ("nocrc32",
         "Turn off control sums calculation");

    arg_desc->AddOptionalKey
        ("gb_release", "gb_release_mode",
         "Mode of GB release file detection",
         CArgDescriptions::eString);
    arg_desc->SetConstraint("gb_release",
                            &(*new CArgAllow_Strings,
                              GB_RELEASE_MODE_NONE,
                              GB_RELEASE_MODE_GUESS,
                              GB_RELEASE_MODE_FORCE));

    arg_desc->AddFlag
        ("abs_path",
         "Use absolute path to data files (default)");
    arg_desc->AddFlag
        ("keep_path",
         "Keep original path to data files");

    arg_desc->AddFlag
        ("keep_other",
         "Keep files outside source dir indexed.");

    arg_desc->AddOptionalKey
        ("dump_table", "table_name",
         "Dump LDS table content",
         CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}


int CLDSIndexerApplication::Run(void)
{
    string lds_section_name = "lds";

    const CNcbiRegistry& reg = GetConfig();

    string lds_path = reg.Get(lds_section_name,  "Path",
                                     IRegistry::fTruncate);
    string lds_alias = reg.Get(lds_section_name, "Alias",
                                      IRegistry::fTruncate);

    string source_path = reg.Get(lds_section_name,  "Source",
                                        IRegistry::fTruncate);
   

    const CArgs& args = GetArgs();
    if (args["source"])
        source_path = args["source"].AsString();

    if (args["db_path"] )
        lds_path = args["db_path"].AsString();
        
    CLDS_Manager::TFlags flags = CLDS_Manager::fDefaultFlags;

    bool recurse_sub_dir = reg.GetBool(lds_section_name, "SubDir", true);
    if ( args["norecursive"] ) {
        recurse_sub_dir = false;
    }
    flags = (flags & ~CLDS_Manager::fRecurseMask) |
        (recurse_sub_dir? CLDS_Manager::fRecurseSubDirs: CLDS_Manager::fDontRecurse);

    bool crc32 = 
        reg.GetBool(lds_section_name, "ControlSum", true);
    // if ControlSum key is true (default), try an alternative "CRC32" key
    // (more straightforward synonym for the same setting)
    if (crc32) {
        crc32 = reg.GetBool(lds_section_name, "CRC32", true);
    }
    if ( args["crc32"] ) {
        crc32 = true;
    }
    if ( args["nocrc32"] ) {
        crc32 = false;
    }
    flags = (flags & ~CLDS_Manager::fControlSumMask) |
        (crc32? CLDS_Manager::fComputeControlSum: CLDS_Manager::fNoControlSum);
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

    if ( args["gb_release"] ) {
        string mode = args["gb_release"].AsString();
        flags &= ~CLDS_Manager::fGBReleaseMask;
        if ( mode == GB_RELEASE_MODE_NONE ) {
            flags |= CLDS_Manager::fNoGBRelease;
        }
        if ( mode == GB_RELEASE_MODE_GUESS ) {
            flags |= CLDS_Manager::fGuessGBRelease;
        }
        if ( mode == GB_RELEASE_MODE_FORCE ) {
            flags |= CLDS_Manager::fForceGBRelease;
        }
    }

    if ( args["keep_other"] ) {
        flags = (flags & ~CLDS_Manager::fOtherFilesMask) |
            CLDS_Manager::fKeepOtherFiles;
    }

    CLDS_Manager manager( source_path, lds_path, lds_alias);
    if ( args["dump_table"] ) {
        manager.DumpTable(args["dump_table"].AsString());
    }
    else {
        manager.Index(flags);
    }

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return 
        CLDSIndexerApplication().AppMain(
                    argc, argv, 0, eDS_Default );
}
