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
 * Author: Mike DiCuccio
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objects/seqfeat/Feat_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>

#include <serial/iterator.hpp>

#include <algo/sequence/gene_model.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CGeneModelDemoApp : public CNcbiApplication {
public:
    CGeneModelDemoApp(void) {DisableArgDescriptions();};
    virtual void Init(void);
    virtual int Run(void);
};

/*---------------------------------------------------------------------------*/

void CGeneModelDemoApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("i", "InputFile",
                            "File containing seq-align to use",
                            CArgDescriptions::eInputFile,
                            "-");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "File containing seq-annot for gene model",
                            CArgDescriptions::eOutputFile,
                            "-");
    arg_desc->AddDefaultKey("ofmt", "OutputFormat",
                            "Format for output",
                            CArgDescriptions::eString,
                            "seq-annot");
    arg_desc->SetConstraint("ofmt",
                            &(*new CArgAllow_Strings,
                              "seq-annot", "seq-feat"));

    SetupArgDescriptions(arg_desc.release());
}


int CGeneModelDemoApp::Run(void)
{
    const CArgs& args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiOstream& ostr = args["o"].AsOutputFile();
    string ofmt = args["ofmt"].AsString();

    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();

    while (istr) {
        CSeq_align align;
        try {
            istr >> MSerial_AsnText >> align;
        }
        catch (CEofException&) {
            break;
        }

        CSeq_annot annot;
        CBioseq_set seqs;
        CFeatureGenerator gen(scope);
        gen.ConvertAlignToAnnot(align, annot, seqs);

        annot.SetNameDesc("Demo Gene Models");
        annot.SetTitleDesc("Demo Gene Models");

        /**
        CTypeIterator<CSeq_feat> feat_it(annot);
        for ( ;  feat_it;  ++feat_it) {
            feat_it->SetId().SetLocal().SetId(++counter);
        }
        **/

        if (ofmt == "seq-annot") {
            ostr << MSerial_AsnText << annot;
        }
        else if (ofmt == "seq-feat") {
            ITERATE (CSeq_annot::TData::TFtable, it,
                     annot.GetData().GetFtable()) {
                ostr << MSerial_AsnText << **it;
            }
        }
    }

    return 0;
}

//---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    CGeneModelDemoApp theApp;
    return theApp.AppMain(argc, argv, NULL, eDS_Default, 0);
}
