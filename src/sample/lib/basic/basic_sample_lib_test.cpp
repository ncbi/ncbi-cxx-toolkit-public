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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Test for sample library
 *
 */

#include <ncbi_pch.hpp>
#include <sample/lib/basic/sample_lib_basic.hpp>

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CSampleLibtestApplication::


class CSampleLibtestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
};


void CSampleLibtestApplication::Init(void)
{
    // Create command-line argument descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Test program: find files in PATH by mask");

    arg_desc->AddPositional(
        "mask", "File name mask to search for",
        CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CSampleLibtestApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    list<string> found;
    if (CSampleLibraryObject().FindInPath( found, args["mask"].AsString() )) {
        ITERATE( list<string>, f, found) {
            cout << *f << endl;
        }
        return 0;
    }
    return 1;
}

int main(int argc, const char* argv[])
{
    return CSampleLibtestApplication().AppMain(argc, argv);
}
