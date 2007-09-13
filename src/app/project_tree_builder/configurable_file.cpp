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
 * Author:  Vladimir Ivanov
 *
 */

#include <ncbi_pch.hpp>
#include <app/project_tree_builder/configurable_file.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/ptb_err_codes.hpp>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


bool CreateConfigurableFile(const string& src_path, const string& dst_path,
                            const string& config_name)
{
    string dst(dst_path);
    dst += ".candidate";

    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(dst_path, &dir);
    CDir project_dir(dir);
    if ( !project_dir.Exists() ) {
        CDir(dir).CreatePath();
    }

    CNcbiIfstream is(src_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !is ) {
        NCBI_THROW(CProjBulderAppException, eFileOpen, src_path);
    }
    CNcbiOfstream os(dst.c_str(),
                     IOS_BASE::out | IOS_BASE::binary | IOS_BASE::trunc);
    if ( !os ) {
        NCBI_THROW(CProjBulderAppException, eFileCreation, dst_path);
    }

    // Read source file
    string str;
    char   buf[1024*16];
    while ( is ) {
        is.read(buf, sizeof(buf));
        if ( is.gcount() > 0  &&  str.size() == str.capacity()) {
            str.reserve(str.size() +
                         max((SIZE_TYPE)is.gcount(), str.size() / 2));
        }
        str.append(buf, is.gcount());
    }

    // Replace some variables:

    // ---------- @ncbi_runpath@ ----------

    string run_path = GetApp().GetEnvironment().Get("NCBI_INSTALL_PATH");
    // For installing toolkit the path like to
    // "$NCBI_INSTALL_PATH/lib/static/DebugDLL",
    // otherwise the path like to 
    // ".../compilers/msvc710_prj/static/bin/DebugDLL".
    if ( run_path.empty() ) {
        run_path = GetApp().GetProjectTreeRoot();
        run_path = CDirEntry::ConcatPath(run_path,
                                         GetApp().GetBuildType().GetTypeStr());
        run_path = CDirEntry::ConcatPath(run_path, "bin");
    } else {
        run_path = CDirEntry::ConcatPath(run_path, "lib");
        run_path = CDirEntry::ConcatPath(run_path,
                                         GetApp().GetBuildType().GetTypeStr());
    }
    run_path = CDirEntry::ConcatPath(run_path, config_name);
    run_path = NStr::Replace(run_path, "\\", "\\\\");
    str = NStr::Replace(str, "@ncbi_runpath@", run_path);

    // ------------------------------------

    // Write result
    os.write(str.data(), str.size());
    if ( !os ) {
        NCBI_THROW(CProjBulderAppException, eFileCreation, dst_path);
    }
    os.close();

    if (PromoteIfDifferent(dst_path,dst)) {
        PTB_WARNING_EX(dst_path, ePTB_FileModified,
                       "Configuration file modified");
    } else {
        PTB_INFO_EX(dst_path, ePTB_NoError,
                    "Configuration file unchanged");
    }
    return true;
}


string ConfigurableFileSuffix(const string& config_name)
{
    return config_name;
    //string delim = "__";
    //return delim + config_name + delim;
}


END_NCBI_SCOPE
