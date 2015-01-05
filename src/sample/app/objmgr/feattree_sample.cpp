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
 * Authors:  David McElhany
 *
 * File Description:
 *   Demonstrate using the CFeatTree class.  Specifically: create the feature
 *   tree, then iterate over the tree mapping each feature to the best gene for
 *   the feature.  In this demo, the output is simple information about the
 *   selected Bioseq, the features and genes found, and the size of the map.
 *   In real applications, something more useful would be done with the map.
 *   The point of this demo is just to show the mechanics of using the feature
 *   tree.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbi_param.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/create_defline.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/objistr.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);
USING_SCOPE(ncbi::objects::sequence);


/////////////////////////////////////////////////////////////////////////////
//  CFeatTreeSampleApp::


class CFeatTreeSampleApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    // Helper function to get a unique string for a Seq-feat (the ASN.1 data).
    static string FeatString(CConstRef<CSeq_feat> seq_feat)
    {
        CNcbiOstrstream oss;
        oss << MSerial_AsnText << *seq_feat;
        return CNcbiOstrstreamToString(oss);
    }

    TGi m_gi;
    int m_range_from;
    int m_range_to;
};


/////////////////////////////////////////////////////////////////////////////
//  Initialize configuration parameters


void CFeatTreeSampleApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Demonstrate CFeatTree class.");

    arg_desc->AddDefaultKey("gi", "gi",
                            "The gi for the Bioseq of interest, e.g. 455025.",
                            CArgDescriptions::eInteger, "455025");

    arg_desc->AddDefaultKey("from", "from",
                            "The starting position of the range.",
                            CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("to", "to",
                            "The ending position of the range.",
                            CArgDescriptions::eInteger, "0");

    // Setup arg.descriptions for this application.
    SetupArgDescriptions(arg_desc.release());

    // Get the arguments.
    const CArgs& args = GetArgs();

    // Get configuration.
    m_gi = GI_FROM(int, args["gi"].AsInteger());
    m_range_from = args["from"].AsInteger();
    m_range_to = args["to"].AsInteger();
}



/////////////////////////////////////////////////////////////////////////////
//  Run the application


int CFeatTreeSampleApp::Run(void)
{
    // Get access to the object manager.
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    // Register the GenBank data loader with the OM.
    CGBDataLoader::RegisterInObjectManager(*object_manager);

    // Create a new scope and add default loaders (GB loader).
    CScope scope(*object_manager);
    scope.AddDefaults();

    // Create a Seq-id and set it to the specified GI.
    CSeq_id seq_id;
    seq_id.SetGi(m_gi);

    // Get a Bioseq handle for the Seq-id.
    CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);
    if (bioseq_handle.GetState()) {
        // print blob state:
        NcbiCout << "Bioseq state: 0x"
                 << hex << bioseq_handle.GetState()
                 << dec << NcbiEndl;
        return 1;
    }
    CDeflineGenerator gen;
    NcbiCout << "Title: " << gen.GenerateDefline(bioseq_handle) << NcbiEndl;

    // Construct the Seq-loc to get features for.
    CSeq_loc seq_loc;
    if (m_range_from == 0 && m_range_to == 0) {
        seq_loc.SetWhole().SetGi(m_gi);
        NcbiCout << "Searching whole bioseq, gi|" << m_gi << "  length = "
            << bioseq_handle.GetBioseqLength() << NcbiEndl;
    } else {
        seq_loc.SetInt().SetId(seq_id);
        seq_loc.SetInt().SetFrom(m_range_from);
        seq_loc.SetInt().SetTo(m_range_to);
        NcbiCout << "Searching bioseq interval, gi|" << m_gi << ":"
            << seq_loc.GetInt().GetFrom() << "-"
            << seq_loc.GetInt().GetTo() << NcbiEndl;
    }

    // Make a selector to limit features to those of interest.
    SAnnotSelector sel;
    sel.SetResolveAll();
    sel.SetAdaptiveDepth(true);

    // Exclude SNP's and STS's since they won't add anything interesting
    // but could significantly degrade performance.
    sel.ExcludeNamedAnnots("SNP");
    sel.ExcludeNamedAnnots("STS");

    // Use a CFeat_CI iterator to iterate through all selected features.
    CFeat_CI feat_it(CFeat_CI(scope, seq_loc, sel));
    NcbiCout << feat_it.GetSize() << " features found." << NcbiEndl;

    // Create the feature tree and add to it the features found by the
    // feature iterator.
    feature::CFeatTree feat_tree;
    feat_tree.AddFeatures(feat_it);

    // Create some data structures to map features to genes
    // and track unique genes found.
    map< string, string >        feattree_map;
    set< string, less<string> >  gene_set;

    // Find the best gene for each feature using CFeatTree.
    // Loop through all the features, mapping each feature to the best
    // related gene, if any.
    CConstRef<CSeq_feat> gene;
    for (feat_it.Rewind(); feat_it; ++feat_it) {

        // Get the underlying Seq-feat.
        CConstRef<CSeq_feat> seq_feat_ref(feat_it->GetSeq_feat());

        // Get a unique string representation of this feature.
        string feat_str(FeatString(seq_feat_ref));

        // Add this feature and its best gene to the feature map.
        // First, find the best gene.

        // Is this feature itself a gene?
        if (seq_feat_ref->GetData().GetSubtype() ==
            CSeqFeatData::eSubtype_gene) {
            gene.Reset(seq_feat_ref);
        } else {
            // Does this feature have a parent that's a gene?
            CMappedFeat parent(feat_tree.GetParent(*feat_it,
                               CSeqFeatData::eSubtype_gene));
            if (parent) {
                gene.Reset(parent.GetSeq_feat());
            } else {
                // No gene found for this feature.
                continue;
            }
        }

        // Get a unique string representation for the best gene.
        string gene_str(FeatString(gene));

        // Now map this feature to its best gene.
        feattree_map[feat_str] = gene_str;
        gene_set.insert(gene_str);
    }

    // For this demo, just print the size of the map and number of genes.
    // For real applications, do something interesting with the results.
    NcbiCout << feattree_map.size() << " features mapped to genes." << NcbiEndl;
    NcbiCout << gene_set.size() << " unique genes found." << NcbiEndl;

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CFeatTreeSampleApp::Exit(void)
{
    // Do your after-Run() cleanup here
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CFeatTreeSampleApp().AppMain(argc, argv);
}
