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
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/cobalt/kmercounts.hpp>
#include <algo/cobalt/clusterer.hpp>
#include <algo/cobalt/links.hpp>

#include <objtools/align_format/showdefline.hpp>

#include "cobalt_app_util.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(align_format);
USING_SCOPE(cobalt);

/////////////////////////////////////////////////////////////////////////////
//  CClustererApplication::


class CClustererApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    CRef<CObjectManager> m_ObjMgr;

    int x_RunBinary(const CArgs& args);
    int x_RunSparse(const CArgs& args);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CClustererApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion | fHideFullVersion
                | fHideXmlHelp | fHideDryRun);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "K-mer based sequence clustering");

    // Describe the expected command-line arguments
    arg_desc->AddKey("i", "infile", "Query file name",
                     CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("k", "num", "K-mer length",
                            CArgDescriptions::eInteger, "3");

    arg_desc->AddDefaultKey("alph", "alphabet", "Compressed alphabet",
                            CArgDescriptions::eString, "regular");

    arg_desc->SetConstraint("alph", &(*new CArgAllow_Strings, "regular",
                                      "se-v10", "se-b15"));

    arg_desc->AddDefaultKey("dist_method", "method", "Distance method",
                            CArgDescriptions::eString, "global");

    arg_desc->SetConstraint("dist_method", &(*new CArgAllow_Strings, "local", 
                                             "global"));

    arg_desc->AddDefaultKey("max_dist", "val", 
                            "Maximum distance between sequences in a cluster",
                            CArgDescriptions::eDouble, "0.8");

    arg_desc->AddDefaultKey("clust_method", "method", "Clustering method",
                            CArgDescriptions::eString, "clique");

    arg_desc->SetConstraint("clust_method", &(*new CArgAllow_Strings, "dist",
                                              "clique"));

    arg_desc->AddDefaultKey("impl", "method", "K-mer implementation method",
                            CArgDescriptions::eString, "bin");

    arg_desc->SetConstraint("impl", &(*new CArgAllow_Strings, "bin", "int"));


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)

int CClustererApplication::x_RunBinary(const CArgs& args)
{
    typedef CBinaryKmerCounts TKmerCounts;
    typedef TKmerMethods<TKmerCounts> TKMethods;

    m_ObjMgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*m_ObjMgr));
    scope->AddDefaults();
    vector< CRef<CSeq_loc> > queries;
    GetSeqLocFromStream(args["i"].AsInputFile(), queries, scope,
                        CFastaReader::fAssumeProt | 
                        CFastaReader::fForceType);

    vector<TKmerCounts> kmer_counts;
    unsigned int kmer_len = (unsigned int)args["k"].AsInteger();

    TKMethods::ECompressedAlphabet alph = TKMethods::eRegular;
    if (args["alph"].AsString() == "regular") {
        alph = TKMethods::eRegular;
    }
    else if (args["alph"].AsString() == "se-v10") {
        alph = TKMethods::eSE_V10;
    }
    else if (args["alph"].AsString() == "se-b15") {
        alph = TKMethods::eSE_B15;
    }
    else {
        NcbiCerr << (string)"Error: unrecognised alphabet: "
            + args["alph"].AsString();

        Exit();
    }
    TKMethods::SetParams(kmer_len, alph);

    // compute k-mer counts
    TKMethods::ComputeCounts(queries, *scope, kmer_counts);

    TKMethods::EDistMeasures dist_method = TKMethods::eFractionCommonKmersLocal;
    if (args["dist_method"].AsString() == "local") {
        dist_method = TKMethods::eFractionCommonKmersLocal;
    }
    else if (args["dist_method"].AsString() == "global") {
        dist_method = TKMethods::eFractionCommonKmersGlobal;
    }
    else {
        NcbiCerr << (string)"Error: unrecognised distance measure: "
            + args["dist_method"].AsString();

        Exit();
    }

    double max_diameter = args["max_dist"].AsDouble();
    CClusterer::EClustMethod clust_method = CClusterer::eDist;
    if (args["clust_method"].AsString() == "clique") {
        clust_method = CClusterer::eClique;
    }

    // compute distances between sequences
    CRef<CLinks> links = TKMethods::ComputeDistLinks(kmer_counts, dist_method,
                                                     max_diameter);

    // compute clusters
    CClusterer clusterer;
    clusterer.SetLinks(links);
    clusterer.SetClustMethod(clust_method);
    clusterer.SetMakeTrees(false);
    clusterer.Run();

    // print output: sequence id  cluster id
    for (int i=0;i < (int)queries.size();i++) {

        CBioseq_Handle bhandle = scope->GetBioseqHandle(queries[i]->GetWhole(),
                                                        CScope::eGetBioseq_All);

        list< CRef<CSeq_id> > id;
        CShowBlastDefline::GetSeqIdList(bhandle, id);
        string id_str = CShowBlastDefline::GetSeqIdListString(id, true);
        NcbiCout << id_str << "\t" << "Cluster_" 
                 << clusterer.GetClusterId(i) << NcbiEndl;
    }

    return 0;
}


int CClustererApplication::x_RunSparse(const CArgs& args)
{
    typedef CSparseKmerCounts TKmerCounts;
    typedef TKmerMethods<TKmerCounts> TKMethods;

    m_ObjMgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*m_ObjMgr));
    scope->AddDefaults();
    vector< CRef<CSeq_loc> > queries;
    GetSeqLocFromStream(args["i"].AsInputFile(), queries, scope,
                        CFastaReader::fAssumeProt | 
                        CFastaReader::fForceType);

    vector<TKmerCounts> kmer_counts;
    unsigned int kmer_len = (unsigned int)args["k"].AsInteger();

    TKMethods::ECompressedAlphabet alph = TKMethods::eRegular;
    if (args["alph"].AsString() == "regular") {
        alph = TKMethods::eRegular;
    }
    else if (args["alph"].AsString() == "se-v10") {
        alph = TKMethods::eSE_V10;
    }
    else if (args["alph"].AsString() == "se-b15") {
        alph = TKMethods::eSE_B15;
    }
    else {
        NcbiCerr << (string)"Error: unrecognised alphabet: "
            + args["alph"].AsString();

        Exit();
    }
    TKMethods::SetParams(kmer_len, alph);

    // compute k-mer counts
    TKMethods::ComputeCounts(queries, *scope, kmer_counts);

    TKMethods::EDistMeasures dist_method = TKMethods::eFractionCommonKmersLocal;
    if (args["dist_method"].AsString() == "local") {
        dist_method = TKMethods::eFractionCommonKmersLocal;
    }
    else if (args["dist_method"].AsString() == "global") {
        dist_method = TKMethods::eFractionCommonKmersGlobal;
    }
    else {
        NcbiCerr << (string)"Error: unrecognised distance measure: "
            + args["dist_method"].AsString();

        Exit();
    }

    double max_diameter = args["max_dist"].AsDouble();
    CClusterer::EClustMethod clust_method = CClusterer::eDist;
    if (args["clust_method"].AsString() == "clique") {
        clust_method = CClusterer::eClique;
    }

    // compute distances between sequences
    CRef<CLinks> links = TKMethods::ComputeDistLinks(kmer_counts, dist_method,
                                                     max_diameter);

    // compute clusters
    CClusterer clusterer;
    clusterer.SetLinks(links);
    clusterer.SetClustMethod(clust_method);
    clusterer.SetMakeTrees(false);
    clusterer.Run();

    // print output: sequence id  cluster id
    for (int i=0;i < (int)queries.size();i++) {

        CBioseq_Handle bhandle = scope->GetBioseqHandle(queries[i]->GetWhole(),
                                                        CScope::eGetBioseq_All);

        list< CRef<CSeq_id> > id;
        CShowBlastDefline::GetSeqIdList(bhandle, id);
        string id_str = CShowBlastDefline::GetSeqIdListString(id, true);
        NcbiCout << id_str << "\t" << "Cluster_" 
                 << clusterer.GetClusterId(i) << NcbiEndl;
    }

    return 0;
}


int CClustererApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    if (args["impl"].AsString() == "bin") {
        return x_RunBinary(args);
    }
    else {
        return x_RunSparse(args);
    }
}



/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CClustererApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CClustererApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
