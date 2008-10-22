static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: cobalt_app.cpp

Author: Jason Papadopoulos

Contents: C++ driver for COBALT multiple alignment algorithm

******************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <serial/iterator.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <algo/cobalt/cobalt.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(cobalt);

class CMultiApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int Run(void);
    virtual void Exit(void);

    CRef<CObjectManager> m_ObjMgr;
};

void CMultiApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                             "COBALT multiple alignment utility");

    arg_desc->AddKey("i", "infile", "Query file name",
                     CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("p", "patternfile", 
                     "filename containing search patterns",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("db", "database", "domain database name",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("b", "blockfile", 
                     "filename containing conserved blocks",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("f", "freqfile", 
                     "filename containing residue frequencies",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("seqalign", "file", 
                     "destination filename for text seqalign",
                     CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("c", "constraintfile", 
                     "filename containing pairwise alignment constraints, "
                     "one per line, each of the form:\n"
                     "seq1_idx seq1_start seq1_end "
                     "seq2_idx seq2_start seq2_end",
                     CArgDescriptions::eString);
    arg_desc->AddDefaultKey("evalue", "evalue", 
                     "E-value threshold for selecting conserved domains",
                     CArgDescriptions::eDouble, "0.01");
    arg_desc->AddDefaultKey("evalue2", "evalue2", 
                     "E-value threshold for aligning filler segments",
                     CArgDescriptions::eDouble, "0.01");
    arg_desc->AddDefaultKey("g0", "penalty", 
                     "gap open penalty for initial/terminal gaps",
                     CArgDescriptions::eInteger, "5");
    arg_desc->AddDefaultKey("e0", "penalty", 
                     "gap extend penalty for initial/terminal gaps",
                     CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("g1", "penalty", 
                     "gap open penalty for middle gaps",
                     CArgDescriptions::eInteger, "11");
    arg_desc->AddDefaultKey("e1", "penalty", 
                     "gap extend penalty for middle gaps",
                     CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("ccc", "conserved_cutoff", 
                     "Minimum average score needed for a multiple alignment "
                     "column to be considered as conserved",
                     CArgDescriptions::eDouble, "0.67");
    arg_desc->AddDefaultKey("dfb", "domain_res_boost", 
                     "When assigning domain residue frequencies, the amount of "
                     "extra weight (0..1) to give the actual sequence letter "
                     "at that position",
                     CArgDescriptions::eDouble, "0.5");
    arg_desc->AddDefaultKey("ffb", "filler_res_boost", 
                     "When assigning filler residue frequencies, the amount of "
                     "extra weight (0..1) to give the actual sequence letter "
                     "at that position",
                     CArgDescriptions::eDouble, "1.0");
    arg_desc->AddDefaultKey("norps", "norps", 
                     "do not perform initial RPS blast search",
                     CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("v", "verbose", 
                     "verbose output",
                     CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("iter", "iterate", 
                     "look for conserved columns and iterate if "
                     "any are found",
                     CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("matrix", "matrix", 
                     "score matrix to use",
                     CArgDescriptions::eString, "BLOSUM62");
    arg_desc->AddDefaultKey("pseudo", "pseudocount", 
                     "Pseudocount constant",
                     CArgDescriptions::eDouble, "2.0");
    arg_desc->AddDefaultKey("fastme", "fastme", 
                     "Use FastME tree generation algorithm instead of "
                     "neighbor joining",
                     CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("clusters", "clusters", 
                     "Use query clustering to minimize number of"
                     "RPSBlast searches",
                     CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("kmer_len", "length", 
                     "Kmer length for query clustering",
                     CArgDescriptions::eInteger, "4");

    arg_desc->AddDefaultKey("max_dist", "distance",
                     "Maximum distance between sequences in a cluster",
                     CArgDescriptions::eDouble, "0.5");

    arg_desc->AddDefaultKey("similarity", "measure",
                     "Similarity measure to be used for clustering"
                     "k-mer counts vectors",
                     CArgDescriptions::eString, "kfraction_global");
    arg_desc->SetConstraint("similarity", &(*new CArgAllow_Strings,
                     "kfraction_local", "kfraction_global"));

    arg_desc->AddOptionalKey("comp_alph", "name",
                     "Compressed alphabet for kmer counting",
                     CArgDescriptions::eString);
    arg_desc->SetConstraint("comp_alph", &(*new CArgAllow_Strings, "se-v10", 
                       "se-b15"));

    SetupArgDescriptions(arg_desc.release());
}

void
x_GetSeqLocFromStream(CNcbiIstream& instream, CObjectManager& objmgr,
                      vector< CRef<objects::CSeq_loc> >& seqs,
                      CRef<objects::CScope>& scope)
{
    seqs.clear();
    scope.Reset(new CScope(objmgr));

    // read one query at a time, and use a separate seq_entry,
    // scope, and lowercase mask for each query. This lets different
    // query sequences have the same ID. Later code will distinguish
    // between queries by using different elements of retval[]

    CStreamLineReader line_reader(instream);
    CFastaReader fasta_reader(line_reader, 
                              CFastaReader::fAssumeProt |
                              CFastaReader::fForceType |
                              CFastaReader::fNoParseID);

    while (!line_reader.AtEOF()) {

        scope->AddDefaults();
        CRef<CSeq_entry> entry = fasta_reader.ReadOneSeq();

        if (entry == 0) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                        "Could not retrieve seq entry");
        }
        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
        CRef<CSeq_loc> seqloc(new CSeq_loc());
        seqloc->SetWhole().Assign(*itr->GetId().front());
        seqs.push_back(seqloc);
    }
}


static void
x_LoadConstraints(string constraintfile,
                  vector<CMultiAlignerOptions::SConstraint>& constr)
{
    CNcbiIfstream f(constraintfile.c_str());
    if (f.bad() || f.fail())
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Cannot open file with pairwise constraints");

    int seq1, seq1_start, seq1_end;
    int seq2, seq2_start, seq2_end;

    constr.clear();

    f >> seq1 >> seq1_start >> seq1_end;
    f >> seq2 >> seq2_start >> seq2_end;
    CMultiAlignerOptions::SConstraint 
        c(seq1, seq1_start, seq1_end, seq2, seq2_start, seq2_end);
    constr.push_back(c);

    while (!f.eof()) {
        seq1 = -1;

        f >> seq1 >> seq1_start >> seq1_end;
        f >> seq2 >> seq2_start >> seq2_end;
        if (seq1 >= 0) {
            constr.push_back(CMultiAlignerOptions::SConstraint(seq1,
                          seq1_start, seq1_end, seq2, seq2_start, seq2_end));
        }
    }
}


static void
x_LoadPatterns(string patternsfile,
               vector<CMultiAlignerOptions::CPattern>& patterns)
{
    CNcbiIfstream f(patternsfile.c_str());
    if (f.bad() || f.fail())
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Cannot open patterns file");

    patterns.clear();

    while (!f.eof()) {
        string single_pattern;

        f >> single_pattern;

        if (!single_pattern.empty()) {
            patterns.push_back(single_pattern);
        }
    }
}


int CMultiApplication::Run(void)
{
    // Allow the fasta reader to complain on 
    // invalid sequence input
    SetDiagPostLevel(eDiag_Warning);

    // Process command line args
    const CArgs& args = GetArgs();
    
    // Set up data loaders
    m_ObjMgr = CObjectManager::GetInstance();

    CRef<CMultiAlignerOptions> opts(
               new CMultiAlignerOptions(CMultiAlignerOptions::fNoQueryClusters
                                        | CMultiAlignerOptions::fNoIterate
                                        | CMultiAlignerOptions::fNoPatterns
                                        | CMultiAlignerOptions::fNoRpsBlast));


    // PSSM aligner parameters
    opts->SetGapOpenPenalty(-args["g1"].AsInteger());
    opts->SetGapExtendPenalty(-args["e1"].AsInteger());
    opts->SetEndGapOpenPenalty(-args["g0"].AsInteger());
    opts->SetEndGapExtendPenalty(-args["e0"].AsInteger());
    opts->SetScoreMatrixName(args["matrix"].AsString());

    // RPS Blast parameters
    if (args["db"]) {
        opts->SetRpsDb(args["db"].AsString());
    }
    opts->SetRpsEvalue(args["evalue"].AsDouble());
    opts->SetDomainResFreqBoost(args["dfb"].AsDouble());

    // Blastp parameters
    opts->SetBlastpEvalue(args["evalue2"].AsDouble());
    opts->SetLocalResFreqBoost(args["ffb"].AsDouble());

    // Patterns
    if (args["p"]) {
        x_LoadPatterns(args["p"].AsString(), opts->SetCddPatterns());
    }

    // User constraints
    if (args["c"]) {
        x_LoadConstraints(args["c"].AsString(), opts->SetUserConstraints());
    }

    // Progressive alignmenet params
    if (args["fastme"].AsBoolean()) {
        opts->SetTreeMethod(CMultiAlignerOptions::eFastME);
    }

    // Iterative alignment params
    opts->SetIterate(args["iter"].AsBoolean());
    opts->SetConservedCutoffScore(args["ccc"].AsDouble());
    opts->SetPseudocount(args["pseudo"].AsDouble());

    // Query clustering params
    opts->SetUseQueryClusters(args["clusters"].AsBoolean());
    opts->SetKmerLength(args["kmer_len"].AsInteger());
    opts->SetMaxInClusterDist(args["max_dist"].AsDouble());
    CMultiAligner::TKMethods::EDistMeasures dist_measure 
               = CMultiAligner::TKMethods::eFractionCommonKmersGlobal;
    if (args["similarity"]) {
        string dist_arg = args["similarity"].AsString();
        if (dist_arg == "kfraction_local") {
            dist_measure = CMultiAligner::TKMethods::eFractionCommonKmersLocal;
        }
    }
    opts->SetKmerDistMeasure(dist_measure);
    CMultiAligner::TKMethods::ECompressedAlphabet alph 
                                 = CMultiAligner::TKMethods::eRegular;
    if (args["comp_alph"]) {
        if (args["comp_alph"].AsString() == "se-v10") {
            alph = CMultiAligner::TKMethods::eSE_V10;
        } else if (args["comp_alph"].AsString() == "se-b15") {
            alph = CMultiAligner::TKMethods::eSE_B15;
        }
    }
    opts->SetKmerAlphabet(alph);

    // Verbose level
    opts->SetVerbose(args["v"].AsBoolean());

    opts->Validate();

    CMultiAligner aligner(opts);

    vector< CRef<objects::CSeq_loc> > queries;
    CRef<objects::CScope> scope;
    x_GetSeqLocFromStream(args["i"].AsInputFile(), *m_ObjMgr, queries, scope);
    _ASSERT(!scope.Empty());

    aligner.SetQueries(queries, scope);



    aligner.Run();

    const vector<CSequence>& results(aligner.GetSeqResults());
    for (int i = 0; i < (int)results.size(); i++) {
        CBioseq_Handle bhandle = scope->GetBioseqHandle(queries[i]->GetWhole(),
                                                       CScope::eGetBioseq_All);

        printf(">%s\n", sequence::GetTitle(bhandle).c_str());
        for (int j = 0; j < results[i].GetLength(); j++) {
            printf("%c", results[i].GetPrintableLetter(j));
        }
        printf("\n");
    }
    if (args["seqalign"]) {
        CRef<CSeq_align> sa = aligner.GetResults();
        CNcbiOstream& out = args["seqalign"].AsOutputFile();
        out << MSerial_AsnText << *sa;
    }

    return 0;
}

void CMultiApplication::Exit(void)
{
    SetDiagStream(0);
}

int main(int argc, const char* argv[])
{
    return CMultiApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
