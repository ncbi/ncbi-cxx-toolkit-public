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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

#include <objects/taxon1/taxon1.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(ncbi::objects);


class CGi2TaxIdApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

};


void CGi2TaxIdApp::Init()
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("gi", "Accession",
                            "gi to test",
                            CArgDescriptions::eInteger,
                            "0");

    arg_desc->AddOptionalKey("file", "InputFile",
                             "Input file to test, one gi per line",
                             CArgDescriptions::eInputFile);

    // Pass argument descriptions to the application
    //
    SetupArgDescriptions(arg_desc.release());
}


int CGi2TaxIdApp::Run()
{
    CArgs args = GetArgs();

    vector<int> gi_list;
    gi_list.push_back(args["gi"].AsInteger());

    if (args["file"]) {
        CNcbiIstream& istr = args["file"].AsInputFile();
        int gi;
        while (istr >> gi) {
            gi_list.push_back(gi);
        }
    }

    CTaxon1 tax;
    tax.Init();

    ITERATE (vector<int>, gi_iter, gi_list) {
        if ( !*gi_iter ) {
            LOG_POST(Info << "ignoring NULL gi: " << *gi_iter);
            continue;
        }

        int tax_id = 0;
        tax.GetTaxId4GI(*gi_iter, tax_id);
        cout << *gi_iter << " " << tax_id << endl;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CGi2TaxIdApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/10/16 16:13:20  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
