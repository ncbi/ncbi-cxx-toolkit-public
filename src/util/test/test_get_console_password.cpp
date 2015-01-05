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
 * Author:  Sergey Satskiy
 *
 * File Description:
 *   Test of the get password functionality.
 *   The test should not be run automatically because it requires user
 *   interactions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <util/util_misc.hpp>


USING_NCBI_SCOPE;


class CGetPasswordApplication : public CNcbiApplication
{
private:
    virtual int Run(void);
    virtual void Init(void);
};


int CGetPasswordApplication::Run(void)
{
    string userInput = g_GetPasswordFromConsole("Type your password: ");
    cout << endl << "User input: '" << userInput << "'" << endl;

    return 0;
}


void CGetPasswordApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideDryRun);

    auto_ptr<CArgDescriptions> args(new CArgDescriptions);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "g_GetPasswordFromConsole(...) function test");
    SetupArgDescriptions(args.release());
}


int main(int argc, char* argv[])
{
    return CGetPasswordApplication().AppMain(argc, argv);
}
