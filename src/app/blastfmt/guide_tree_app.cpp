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
#include <algo/phy_tree/phytree_calc.hpp>
#include <algo/phy_tree/phytree_format/phytree_format.hpp>

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
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion | fHideFullVersion
                | fHideXmlHelp | fHideDryRun);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Application for computing and printing, "
                              "phylogenetic trees");


    arg_desc->AddKey("i", "filename", "File with Seq-annot or tree in ASN format",
                            CArgDescriptions::eInputFile);

    arg_desc->AddKey("o", "filename", "File name",
                     CArgDescriptions::eOutputFile);


    // input options
    arg_desc->SetCurrentGroup("Input options");
    arg_desc->AddDefaultKey("type", "type", "Type of input data",
                            CArgDescriptions::eString, "seqalign");

    arg_desc->SetConstraint("type", &(*new CArgAllow_Strings, "seqalign",
                                      "seqalignset", "tree"));


    // compute tree options
    arg_desc->SetCurrentGroup("Tree computation options");

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



    // tree manipulation options
    arg_desc->SetCurrentGroup("Tree manipulation options");

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


    // output options
    arg_desc->SetCurrentGroup("Output options");

    arg_desc->AddDefaultKey("outfmt", "format", "Format for saving tree",
                            CArgDescriptions::eString, "newick");
                              
    arg_desc->SetConstraint("outfmt", &(*new CArgAllow_Strings, "asn",
                                     "newick", "nexus"));

    arg_desc->AddDefaultKey("labels", "labels", "Sequence labels in the tree",
                            CArgDescriptions::eString, "seqtitle");

    arg_desc->SetConstraint("labels", &(*new CArgAllow_Strings, "taxname",
                                        "seqtitle", "blastname", "seqid",
                                        "seqid_and_blastname"));

    arg_desc->AddOptionalKey("seqalign", "filename", "Write seq_align "
                             "correspoinding to the tree to a file",
                             CArgDescriptions::eOutputFile);

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
    
    auto_ptr<CPhyTreeFormatter> gtree;

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

        CRef<CPhyTreeCalc> calc;
        
        if (input == eSeqAlignSet) {
            CRef<CSeq_align_set> seq_align_set(new CSeq_align_set());
            args["i"].AsInputFile() >> MSerial_AsnText >> *seq_align_set;
            calc.Reset(new CPhyTreeCalc(*seq_align_set, scope));
        }
        else {
            CSeq_align seq_align;
            args["i"].AsInputFile() >> MSerial_AsnText >> seq_align;
            calc.Reset(new CPhyTreeCalc(seq_align, scope));
        }

        calc->SetMaxDivergence(args["divergence"].AsDouble());

        CPhyTreeFormatter::ELabelType labels;
        if (args["labels"].AsString() == "taxname") {
            labels = CPhyTreeFormatter::eTaxName;
        } else if (args["labels"].AsString() == "seqtitle") {
            labels = CPhyTreeFormatter::eSeqTitle;
        } else if (args["labels"].AsString() == "blastname") {
            labels = CPhyTreeFormatter::eBlastName;
        } else if (args["labels"].AsString() == "seqid") {
            labels = CPhyTreeFormatter::eSeqId;
        } else if (args["labels"].AsString() == "seqid_and_blastname") {
            labels = CPhyTreeFormatter::eSeqIdAndBlastName;
        } else {
            NcbiCerr << "Error: Unrecognised labels type." << NcbiEndl;
            return 1;
        }

        CPhyTreeCalc::EDistMethod dist;
        if (args["distance"].AsString() == "jukes_cantor") {
            dist = CPhyTreeCalc::eJukesCantor;
        } else if (args["distance"].AsString() == "poisson") {
            dist = CPhyTreeCalc::ePoisson;
        } else if (args["distance"].AsString() == "kimura") {
            dist = CPhyTreeCalc::eKimura;
        } else if (args["distance"].AsString() == "grishin") {
            dist = CPhyTreeCalc::eGrishin;
        } else if (args["distance"].AsString() == "grishin2") {
            dist = CPhyTreeCalc::eGrishinGeneral;
        } else {
            NcbiCerr << "Error: Unrecognized distance method." << NcbiEndl;
            return 1;
        }
        calc->SetDistMethod(dist);

        CPhyTreeCalc::ETreeMethod method;
        if (args["treemethod"].AsString() == "fastme") {
            method = CPhyTreeCalc::eFastME;
        } else if (args["treemethod"].AsString() == "nj") {
            method = CPhyTreeCalc::eNJ;
        } else {
            NcbiCerr << "Error: Unrecognized tree computation method."
                     << NcbiEndl;
            return 1;
        }
        calc->SetTreeMethod(method);

        if (calc->CalcBioTree()) {
            gtree.reset(new CPhyTreeFormatter(*calc, labels));
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
        gtree.reset(new CPhyTreeFormatter(btc));
    }

    // Simplify tree
    if (args["simpl"]) {
        if (args["simpl"].AsString() == "blastname") {
            gtree->SimplifyTree(CPhyTreeFormatter::eByBlastName);
        } else if (args["simpl"].AsString() == "full") {
            gtree->SimplifyTree(CPhyTreeFormatter::eFullyExpanded);
        }
    }

    // Reroot tree
    if (args["reroot"]) {
        gtree->RerootTree(args["reroot"].AsInteger());
    }

    // Expand or collapse selected subtree
    if (args["expcol"]) {
        gtree->ExpandCollapseSubtree(args["expcol"].AsInteger());
    }

    // Show subtree
    if (args["subtree"]) {
        gtree->ShowSubtree(args["subtree"].AsInteger());
    }

    // print tree in text format

    CPhyTreeFormatter::ETreeFormat tree_format;
    if (args["outfmt"].AsString() == "asn") {
        tree_format = CPhyTreeFormatter::eASN;
    } else if (args["outfmt"].AsString() == "newick") {
        tree_format = CPhyTreeFormatter::eNewick;
    } else if (args["outfmt"].AsString() == "nexus") {
        tree_format = CPhyTreeFormatter::eNexus;
    } else {
        NcbiCerr << "Error: Unrecognised tree output format." << NcbiEndl;
        return 1;
    }

    gtree->WriteTreeAs(args["o"].AsOutputFile(), tree_format);

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
    return CGuideTreeApplication().AppMain(argc, argv, 0, eDS_Default, "");
}
