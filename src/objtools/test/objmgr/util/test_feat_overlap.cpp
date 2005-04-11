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
 * Author:  Mike DiCuccio
 *
 * File Description:
 *   test code for various flavors of feature overlap testing
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers/id2/reader_id2.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CFeatOverlapTester : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CFeatOverlapTester::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Show a sequence's title", false);

    arg_desc->AddKey("id", "SeqId",
                     "Seq-id of sequence to test",
                     CArgDescriptions::eString);
    arg_desc->AddDefaultKey("iters", "Iterations",
                            "Number of iterations to test",
                            CArgDescriptions::eInteger,
                            "10");

    arg_desc->AddFlag("complete", "Check completeness");
    arg_desc->AddFlag("perf",     "Check performance");

    SetupArgDescriptions(arg_desc.release());
}


static void s_GetFeatLabel(const CSeq_feat& feat,
                           string* label, CScope* scope)
{
    if (feat.IsSetProduct()) {
        feat.GetProduct().GetLabel(label);
        *label += ": ";
    }

    feature::GetLabel(feat, label, feature::eBoth, scope);
    *label += " (";
    feat.GetLocation().GetLabel(label);
    *label += ")";
}


int CFeatOverlapTester::Run(void)
{
    const CArgs&   args = GetArgs();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objmgr, new CId2Reader(),
                                           CObjectManager::eDefault);

    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_id id(args["id"].AsString());
    if (id.Which() == CSeq_id::e_not_set) {
        LOG_POST(Fatal << "can't understand ID: " << args["id"].AsString());
    }

    CBioseq_Handle handle = scope.GetBioseqHandle(id);
    if ( !handle ) {
        LOG_POST(Fatal << "can't retrieve sequence: " << args["id"].AsString());
    }


    SAnnotSelector sel;
    sel.SetResolveAll()
        .SetAdaptiveDepth(true)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion)
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_operon);

    sequence::EOverlapType ov = sequence::eOverlap_CheckIntervals;
    if (args["complete"]) {
        CFeat_CI feat_iter(handle, sel);

        for ( ;  feat_iter;  ++feat_iter) {
            const CSeq_feat& feat = feat_iter->GetOriginalFeature();
            CConstRef<CSeq_feat> best_gene =
                sequence::GetBestOverlappingFeat(feat,
                                                 CSeqFeatData::eSubtype_gene,
                                                 ov, scope);

            CConstRef<CSeq_feat> best_mrna =
                sequence::GetBestOverlappingFeat(feat,
                                                 CSeqFeatData::eSubtype_mRNA,
                                                 ov, scope);

            CConstRef<CSeq_feat> best_cdregion =
                sequence::GetBestOverlappingFeat(feat,
                                                 CSeqFeatData::eSubtype_cdregion,
                                                 ov, scope);

            CConstRef<CSeq_feat> best_operon =
                sequence::GetBestOverlappingFeat(feat,
                                                 CSeqFeatData::eSubtype_operon,
                                                 ov, scope);

            string feat_label;
            s_GetFeatLabel(feat_iter->GetOriginalFeature(),
                           &feat_label, &scope);

            cout << "feature: " << feat_label << endl;

            string gene_label;
            if (best_gene) {
                s_GetFeatLabel(*best_gene, &gene_label, &scope);
                cout << "  best gene: " << gene_label << endl;
            } else {
                cout << "  no best gene" << endl;
            }

            string mrna_label;
            if (best_mrna) {
                s_GetFeatLabel(*best_mrna, &mrna_label, &scope);
                cout << "  best mrna: " << mrna_label << endl;
            } else {
                cout << "  no best mrna" << endl;
            }

            string cdregion_label;
            if (best_cdregion) {
                s_GetFeatLabel(*best_cdregion, &cdregion_label, &scope);
                cout << "  best cdregion: " << cdregion_label << endl;
            } else {
                cout << "  no best cdregion" << endl;
            }

            string operon_label;
            if (best_operon) {
                s_GetFeatLabel(*best_operon, &operon_label, &scope);
                cout << "  best operon: " << operon_label << endl;
            } else {
                cout << "  no best operon" << endl;
            }
        }
    }

    if (args["perf"]) {
        cout << "Timing tests...";
        cout.flush();

        const size_t iters = args["iters"].AsInteger();
        CStopWatch sw;
        for (size_t i = 0;  i <= iters;  ++i) {
            if (i == 1) {
                sw.Start();
            }
            CFeat_CI feat_iter(handle, sel);
            for ( ;  feat_iter;  ++feat_iter) {
                const CSeq_feat& feat = feat_iter->GetOriginalFeature();
                CConstRef<CSeq_feat> best_gene =
                    sequence::GetBestOverlappingFeat(feat,
                                                     CSeqFeatData::eSubtype_gene,
                                                     ov, scope);

                CConstRef<CSeq_feat> best_mrna =
                    sequence::GetBestOverlappingFeat(feat,
                                                     CSeqFeatData::eSubtype_mRNA,
                                                     ov, scope);

                CConstRef<CSeq_feat> best_cdregion =
                    sequence::GetBestOverlappingFeat(feat,
                                                     CSeqFeatData::eSubtype_cdregion,
                                                     ov, scope);

                CConstRef<CSeq_feat> best_operon =
                    sequence::GetBestOverlappingFeat(feat,
                                                     CSeqFeatData::eSubtype_operon,
                                                     ov, scope);
            }
        }
        double e = sw.Elapsed();
        cout << "...done" << endl;
        cout << "finished " << iters << " iterations in "
            << e << " seconds = " << e * 1000.0 / iters << " msec / iter"
            << endl;
    }
    
    return 0;
}


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return CFeatOverlapTester().AppMain(argc, argv);
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/11 14:43:27  dicuccio
 * Added test for feature overlap variants
 *
 *
 * ===========================================================================
 */
