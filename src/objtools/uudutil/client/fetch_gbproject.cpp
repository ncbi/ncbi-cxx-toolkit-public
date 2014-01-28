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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors: Peter Meric
 *
 * File Description: check for availability of NetCache blobs
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/gbproj/GBProject_ver2.hpp>
#include <objtools/uudutil/project_storage.hpp>


USING_NCBI_SCOPE;


class CFetchGBProjectApp : public CNcbiApplication
{
public:
    CFetchGBProjectApp()
    {
    }

    virtual ~CFetchGBProjectApp()
    {
    }

    virtual void Init(void);
    virtual int Run(void);
};

void
CFetchGBProjectApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "UUDUtil FetchGBProject"
                             );

    arg_desc->AddKey("k",
                     "nckey",
                     "NetCache key for GBProject",
                     CArgDescriptions::eString
                    );

    arg_desc->AddFlag("p", "print project");

    SetupArgDescriptions(arg_desc.release());
}

int
CFetchGBProjectApp::Run(void)
{
    static const CArgs& args = GetArgs();
    static const string nckey = args["k"].AsString();

    CProjectStorage prjstorage("FetchGBProjectApp");
    if (args["p"].AsBoolean()) {
        NcbiCout << MSerial_AsnText << *prjstorage.GetObject(nckey);
    }
    else {
        CProjectStorage::TAnnots annots = prjstorage.GetAnnots(nckey);
        ITERATE (CProjectStorage::TAnnots, annot_it, annots) {
            const objects::CSeq_annot& sa(annot_it->GetObject());
            NcbiCout << MSerial_AsnText << sa;
        }
    }

    return 0;
}

int
main(int argc, const char* argv[])
{
    SetSplitLogFile(true);
    SetDiagPostLevel(eDiag_Info);
    GetDiagContext().SetOldPostFormat(false);
    return CFetchGBProjectApp().AppMain(argc,
                                        argv,
                                        NULL,
                                        eDS_ToStdlog,
                                        NcbiEmptyCStr,
                                        "UUD-Util-FetchGBProject"
                                       );
}
