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


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

#include <objects/taxon1/taxon1.hpp>
#include <objects/id1/id1_client.hpp>

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

    arg_desc->AddDefaultKey("gi", "GI",
                            "gi to test",
                            CArgDescriptions::eInteger,
                            "0");

    arg_desc->AddOptionalKey("file", "InputFile",
                             "Input file to test, one gi or accession per line",
                             CArgDescriptions::eInputFile);

    arg_desc->AddFlag("show_acc",
                      "Show the passed accession as well as the gi");

    // Pass argument descriptions to the application
    //
    SetupArgDescriptions(arg_desc.release());
}


int CGi2TaxIdApp::Run()
{
    const CArgs& args = GetArgs();

    bool show = args["show_acc"];

    vector<string> id_list;
    if( args["gi"].AsInteger() ) {
        id_list.push_back( args["gi"].AsString() );
    }

    if (args["file"]) {
        CNcbiIstream& istr = args["file"].AsInputFile();
        string acc;
        while (istr >> acc) {
            id_list.push_back(acc);
        }
    }

    CID1Client id1_client;
    CTaxon1 tax;
    tax.Init();

    static const char* sc_ValidChars =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "_.|";

    ITERATE (vector<string>, iter, id_list) {
        string id_str = *iter;
        id_str = NStr::TruncateSpaces(id_str);
        string::size_type pos = id_str.find_first_not_of(sc_ValidChars);
        if (pos != string::npos) {
            id_str.erase(pos);
        }
        if ( id_str.empty() ) {
            LOG_POST(Info << "ignoring accession: " << *iter);
            continue;
        }

        // resolve the id to a gi
        TGi gi = ZERO_GI;
        try {
            gi = NStr::StringToNumeric<TGi>(id_str);
        }
        catch (...) {
            try {
                cout << "trying: " << *iter << " -> " << id_str << endl;
                CSeq_id id(id_str);
                gi = id1_client.AskGetgi(id);
            } catch (CException&) {
                // gi = 0;
            }
        }

        if (gi == ZERO_GI) {
            LOG_POST(Error << "don't know anything about accession/id: "
                << id_str);
            continue;
        }

        int tax_id = 0;
        tax.GetTaxId4GI(gi, tax_id);

        if (show) {
            cout << id_str << " ";
        }
        cout << gi << " " << tax_id << endl;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CGi2TaxIdApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
