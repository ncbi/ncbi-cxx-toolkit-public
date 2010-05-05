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
 * Authors:  Greg Boratyn
 *
 * File Description:
 *   Demo application for simple use of CGuideTree and CGuideTreeCalc
 *   clases.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiapp.hpp>

#include <serial/serialbase.hpp>
#include <objects/biotree/BioTreeContainer.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <algo/phy_tree/bio_tree.hpp>

#include "guide_tree.hpp"
#include "guide_tree_calc.hpp"


USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  CGuideTreeApplication::


class CGuideTreeApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CGuideTreeApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Guide tree demo application");


    arg_desc->AddKey("i", "filename", "File with Seq-annot ortree in ASN format",
                            CArgDescriptions::eInputFile);

    arg_desc->AddKey("o", "filename", "File name",
                     CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("type", "type", "Type of input data",
                            CArgDescriptions::eString, "seqalign");

    arg_desc->SetConstraint("type", &(*new CArgAllow_Strings, "seqalign",
                                      "seqalignset", "tree"));

    arg_desc->AddOptionalKey("query", "id", "Query sequence id",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("seqalign", "filename", "Write seq_align "
                             "correspoinding to the tree to a file",
                             CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("divergence", "value", "Maximum divergence"
                            " between sequences",
                            CArgDescriptions::eDouble, "0.85");

    arg_desc->AddDefaultKey("distance", "method", "Evolutionary correction for"
                            " sequence divergence - distance",
                            CArgDescriptions::eString, "grishin");

    arg_desc->SetConstraint("distance", &(*new CArgAllow_Strings,
                                          "jukes_cantor", "poisson", "kimura",
                                          "grishin", "grishin2"));
    
    arg_desc->AddDefaultKey("treemethod", "method", "Algorithm for phylogenetic"
                            " tree computation",
                            CArgDescriptions::eString, "fastme");

    arg_desc->SetConstraint("treemethod", &(*new CArgAllow_Strings, "fastme",
                                            "nj"));

    arg_desc->AddDefaultKey("labels", "labels", "Sequence labels in the tree",
                            CArgDescriptions::eString, "seqtitle");

    arg_desc->SetConstraint("labels", &(*new CArgAllow_Strings, "taxname",
                                        "seqtitle", "blastname", "seqid",
                                        "seqid_and_blastname"));

    arg_desc->AddDefaultKey("render", "method", "Tree rendering method",
                            CArgDescriptions::eString, "rect");

    arg_desc->SetConstraint("render", &(*new CArgAllow_Strings, "rect",
                                       "slanted", "radial", "force"));

    arg_desc->AddOptionalKey("simpl", "method", "Tree simplification method",
                             CArgDescriptions::eString);

    arg_desc->SetConstraint("simpl", &(*new CArgAllow_Strings, "none", 
                                       "blastname", "full"));

    arg_desc->AddOptionalKey("expcol", "num", "Expand or collapse node",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("reroot", "num", "Re-root tree",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("subtree", "num", "Show sub-tree",
                             CArgDescriptions::eInteger);

    arg_desc->AddFlag("no_dist", "Edge lengths of the rendered tree will not be"
                      " proportional to tree edge lengths");

    arg_desc->AddDefaultKey("outfmt", "format", "Format for saving tree",
                            CArgDescriptions::eString, "image");

    arg_desc->SetConstraint("outfmt", &(*new CArgAllow_Strings, "image", "asn",
                                     "newick", "nexus"));

    arg_desc->AddDefaultKey("width", "num", "Image width",
                            CArgDescriptions::eInteger, "800");

    arg_desc->AddDefaultKey("height", "num", "Image height",
                            CArgDescriptions::eInteger, "600");

    arg_desc->AddFlag("best_height", "Use minimal image height so that tree is"
                      " visible");


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)

enum EInput {
    eSeqAlignSet, eSeqAlign, eTree
};

int CGuideTreeApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CBioTreeContainer btc;
    CBioTreeDynamic dyntree;

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*object_manager);
    CRef<CScope> scope(new CScope(*object_manager));
    scope->AddDefaults();
    
    auto_ptr<CGuideTree> gtree;

    string query_id = "";
    if (args["query"]) {
        query_id = args["query"].AsString();
    }

    EInput input;
    if (args["type"].AsString() == "seqalignset") {
        input = eSeqAlignSet;
    } 
    else if (args["type"].AsString() == "seqalign") {
        input = eSeqAlign;
    } 
    else {
        input = eTree;
    }

    // If input is Seq-annot compute guide tree
    if (input == eSeqAlignSet || input == eSeqAlign) {

        CRef<CGuideTreeCalc> calc;
        
        if (input == eSeqAlignSet) {
            CRef<CSeq_align_set> seq_align_set(new CSeq_align_set());
            args["i"].AsInputFile() >> MSerial_AsnText >> *seq_align_set;
            calc.Reset(new CGuideTreeCalc(seq_align_set, scope));
        }
        else {
            CSeq_align seq_align;
            args["i"].AsInputFile() >> MSerial_AsnText >> seq_align;
            calc.Reset(new CGuideTreeCalc(seq_align, scope, query_id));
        }

        calc->SetMaxDivergence(args["divergence"].AsDouble());

        CGuideTree::ELabelType labels;
        if (args["labels"].AsString() == "taxname") {
            labels = CGuideTree::eTaxName;
        } else if (args["labels"].AsString() == "seqtitle") {
            labels = CGuideTree::eSeqTitle;
        } else if (args["labels"].AsString() == "blastname") {
            labels = CGuideTree::eBlastName;
        } else if (args["labels"].AsString() == "seqid") {
            labels = CGuideTree::eSeqId;
        } else if (args["labels"].AsString() == "seqid_and_blastname") {
            labels = CGuideTree::eSeqIdAndBlastName;
        } else {
            NcbiCerr << "Error: Unrecognised labels type." << NcbiEndl;
            return 1;
        }

        CGuideTreeCalc::EDistMethod dist;
        if (args["distance"].AsString() == "jukes_cantor") {
            dist = CGuideTreeCalc::eJukesCantor;
        } else if (args["distance"].AsString() == "poisson") {
            dist = CGuideTreeCalc::ePoisson;
        } else if (args["distance"].AsString() == "kimura") {
            dist = CGuideTreeCalc::eKimura;
        } else if (args["distance"].AsString() == "grishin") {
            dist = CGuideTreeCalc::eGrishin;
        } else if (args["distance"].AsString() == "grishin2") {
            dist = CGuideTreeCalc::eGrishinGeneral;
        } else {
            NcbiCerr << "Error: Unrecognized distance method." << NcbiEndl;
            return 1;
        }
        calc->SetDistMethod(dist);

        CGuideTreeCalc::ETreeMethod method;
        if (args["treemethod"].AsString() == "fastme") {
            method = CGuideTreeCalc::eFastME;
        } else if (args["treemethod"].AsString() == "nj") {
            method = CGuideTreeCalc::eNJ;
        } else {
            NcbiCerr << "Error: Unrecognized tree computation method."
                     << NcbiEndl;
            return 1;
        }
        calc->SetTreeMethod(method);

        if (calc->CalcBioTree()) {
            gtree.reset(new CGuideTree(*calc, labels));
        }
        else {
            ITERATE(vector<string>, it, calc->GetMessages()) {
                NcbiCerr << "Error: " << *it << NcbiEndl;
            }
            NcbiCerr << NcbiEndl;
            return 1;
        }

        // Write any warning messages
        if (calc->IsMessage()) {
            ITERATE(vector<string>, it, calc->GetMessages()) {
                NcbiCerr << "Warning: " << *it << NcbiEndl;
            }
            NcbiCerr << NcbiEndl;
        }

        // Write seq_align
        if (args["seqalign"]) {
            CRef<CSeq_align> seqalign = calc->GetSeqAlign();
            args["seqalign"].AsOutputFile() << MSerial_AsnText << *seqalign;
        }
    }
    else {

        // Otherwise load tree
        args["i"].AsInputFile() >> MSerial_AsnText >> btc;
        gtree.reset(new CGuideTree(btc));
    }

    // Simplify tree
    if (args["simpl"]) {
        if (args["simpl"].AsString() == "blastname") {
            gtree->SimplifyTree(CGuideTree::eByBlastName, false);
        } else if (args["simpl"].AsString() == "full") {
            gtree->SimplifyTree(CGuideTree::eFullyExpanded, false);
        }
    }

    // Expand or collapse selected subtree
    if (args["expcol"]) {
        gtree->ExpandCollapseSubtree(args["expcol"].AsInteger(), false);
    }

    // Reroot tree
    if (args["reroot"]) {
        gtree->RerootTree(args["reroot"].AsInteger(), false);
    }

    // Show subtree
    if (args["subtree"]) {
        gtree->ShowSubtree(args["subtree"].AsInteger(), false);
    }

    // Select tree rendering
    if (args["render"].AsString() == "rect") {
        gtree->SetRenderFormat(CGuideTree::eRect);
    } else if (args["render"].AsString() == "slanted") {
        gtree->SetRenderFormat(CGuideTree::eSlanted);
    } else if (args["render"].AsString() == "radial") {
        gtree->SetRenderFormat(CGuideTree::eRadial);
    } else if (args["redenr"].AsString() == "force") {
        gtree->SetRenderFormat(CGuideTree::eForce);
    }

    if (args["no_dist"]) {
        gtree->SetDistanceMode(false);
    }

    // Redo pre-rendering calculations
    gtree->Refresh();

    CGuideTree::ETreeFormat tree_format;

    if (args["outfmt"].AsString() == "image") {
        tree_format = CGuideTree::eImage;
    } else if (args["outfmt"].AsString() == "asn") {
        tree_format = CGuideTree::eASN;
    } else if (args["outfmt"].AsString() == "newick") {
        tree_format = CGuideTree::eNewick;
    } else if (args["outfmt"].AsString() == "nexus") {
        tree_format = CGuideTree::eNexus;
    } else {
        NcbiCerr << "Error: Unrecognised tree output format." << NcbiEndl;
        return 1;
    }

    if (tree_format == CGuideTree::eImage) {
        gtree->PreComputeImageDimensions();
        gtree->SetImageWidth(args["width"].AsInteger());

        if (args["best_height"]) {
            gtree->SetImageHeight(gtree->GetMinHeight());
        }
        else {
            gtree->SetImageHeight(args["height"].AsInteger());
        }
    }

    // Write tree in selected format
    gtree->SaveTreeAs(args["o"].AsOutputFile(), tree_format);

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CGuideTreeApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CGuideTreeApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
