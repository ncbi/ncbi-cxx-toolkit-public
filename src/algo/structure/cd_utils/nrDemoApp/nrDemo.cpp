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
 * Authors:  Chris Lanczycki
 *
 * File Description:
 *   
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <objects/cdd/Cdd_pref_nodes.hpp>
#include <algo/structure/cd_utils/cuTaxNRCriteria.hpp>
#include "cuSimpleNonRedundifier.hpp"
#include "cuSimpleClusterer.hpp"
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include "nrDemo.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(cd_utils);


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments

NRDemo::NRDemo()
{
	SetVersion(CVersionInfo(2,0));
}

void NRDemo::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> argDescr(new CArgDescriptions);

    // Specify USAGE context
    argDescr->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Taxonomic Non-redundification Demo");


    argDescr->AddOptionalKey("o", "FilenameOut", "filename for output redirection (cerr used if not specified)", argDescr->eString);

    argDescr->AddDefaultKey("tax", "FilenameIn", "filename (relative to executable) containing a list of taxonomy ids to non-redundify", argDescr->eString, "taxIds.txt");

    argDescr->AddDefaultKey("dm", "FilenameIn", "filename (relative to executable) containing upper triangle + diagonal elements of a distance matrix.\nFirst entry is dimension.\nPosition in list of -i arg is index into this matrix.", argDescr->eString, "dists.txt");

    argDescr->AddDefaultKey("asn", "FilenameIn", "filename (relative to executable) containing a Cdd-pref-nodes ASN object that gives priority tax nodes (ascii)", argDescr->eString, "taxNodes.asn");

    argDescr->AddDefaultKey("t", "clusteringThreshold", "threshold at which clusters are built; enter a number on same scale as the *distance* used\n(i.e., a %identity threshold of 0.25 is a distance threshold of 0.75)\n", argDescr->eDouble, "0.5");


    // Setup arg.descriptions for this application
    SetupArgDescriptions(argDescr.release());
	
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int NRDemo::Run(void)
{
//    vector< int > priorityTaxids;

	const CArgs& args = GetArgs();

    int taxId;    
    unsigned int nElements = 0, nTaxidsRead = 0;
    unsigned int dim = 0;
    double thold = args["t"].AsDouble();
    double* dm;

    string path = CDir::AddTrailingPathSeparator(CDir::GetCwd());
//    path = "E:\\Users\\lanczyck\\CDTree\\Code\\c++\\compilers\\msvc710_prj\\static\\bin\\debug\\";
    string err, outfile, priorityNodes = path + args["asn"].AsString();

    //  Set up output stream
    CNcbiOstream* outStr = &cerr; 
    if (args["o"]) {
        outStr = new CNcbiOfstream((path + args["o"].AsString()).c_str(), IOS_BASE::out);
        if (outStr) {
            outStr->setf(IOS_BASE::fixed,
                         IOS_BASE::floatfield);
            outStr->precision(4);
            SetDiagStream(outStr, true);
        } else {
            outStr = &cerr;
        }
    }

    //  Set up the priority tax node objects from the CCdd_pref_nodes ASN
    //  in the file specified in "asn" argument.
    
    CCdd_pref_nodes ccddPrefNodes;
    bool readOK = ReadASNFromFile(priorityNodes.c_str(), &ccddPrefNodes, false, &err);
    if (!readOK) {
        ERR_POST(ncbi::Info << "Error reading CCdd_pref_nodes object from file " << priorityNodes << ":\n" << err << ncbi::Endl());
        return 1;
    }
    CPriorityTaxNodes* ptn1 = new CPriorityTaxNodes(ccddPrefNodes, CPriorityTaxNodes::eCddPrefNodes);
    CPriorityTaxNodes* ptn2 = new CPriorityTaxNodes(ccddPrefNodes, CPriorityTaxNodes::eCddModelOrgs);
    if (!ptn1 && !ptn2) {
        ERR_POST(ncbi::Info << "No priority tax nodes loaded from data in file " << priorityNodes << ncbi::Endl());
        return 2;
    }



    //  Read in a list of taxonomy ids from a second file.  These will be non-redundified
    //  according to the rules in the criteria, and the priority tax nodes.

    CBaseClusterer::TId i = 0;
    CTaxNRCriteria::TId2TaxidMap id2TaxidMap;
    CNcbiIfstream taxIdStr((path + args["tax"].AsString()).c_str(), IOS_BASE::in);
//    CNcbiIstream& taxIdStr = args["tax"].AsInputFile();
    while (taxIdStr >> taxId) {        
        id2TaxidMap.insert(CTaxNRCriteria::TId2TaxidMap::value_type(i, taxId));
        ++i;
    }
    taxIdStr.close();
//    args["tax"].CloseFile();

    nTaxidsRead = i;
    if (id2TaxidMap.size() == 0) {
        ERR_POST(ncbi::Info << "Error reading redundant tax ids from file " << args["tax"].AsString() << ncbi::Endl());
        return 3;
    }


    //  Build the criteria objects; they assume ownership of CPriorityTaxNodes objects.

    CTaxNRCriteria* prefNodes = new CTaxNRCriteria(ptn1, id2TaxidMap);
    CTaxNRCriteria* modelOrgs = new CTaxNRCriteria(ptn2, id2TaxidMap);


    //  Read in the distance matrix; build the clusterer.  (Clusterer can be built
    //  automatically by one form of non redundifier's MakeClusters method.)
    CNcbiIfstream distStr((path + args["dm"].AsString()).c_str(), IOS_BASE::in);
//    CNcbiIstream& distStr = args["dm"].AsInputFile();

    if (distStr >> dim) {
        if (nTaxidsRead > dim) {
            ERR_POST(ncbi::Info << "Distance matrix declared too small (dim = " << dim << ") for the " << nTaxidsRead << " tax ids to non-redundify.\n");
//            args["dm"].CloseFile();
            distStr.close();
            return 4;
        } else if (nTaxidsRead < dim) {
            ERR_POST(ncbi::Info << "Warning:  Distance matrix declared larger (dim = " << dim << ") for the " << nTaxidsRead << " tax ids to non-redundify.\nContinuing but distance data may not be properly indexed to your tax ids.\n");
        }
    }


    nElements = dim*(dim + 1)/2;
    dm = (dim > 0) ? new double[nElements] : NULL;
    i = 0;
    while (dm && i < nElements && distStr >> dm[i]) {
        ++i;
    }
//    args["dm"].CloseFile();
    distStr.close();

    if (i != nElements) {
        ERR_POST(ncbi::Info << "Stopping:  Not as many distances read from file " << args["dm"].AsString() << " as expected.\nFound " << i << ", expected " << nElements << ncbi::Endl());
        return 5;
    } else if (!dm) {
        ERR_POST(ncbi::Info << "Problem reading distance data from file " << args["dm"].AsString() << ncbi::Endl());
        return 6;
    }


    //  clusterer does not own the distance matrix.
    CSimpleClusterer* clusterer = new CSimpleClusterer(dm, dim, thold);

    if (!clusterer || !prefNodes || !modelOrgs) {
        ERR_POST(ncbi::Error << "Problem creating clusterer or criteria objects.\n");
        return 7;
    }

    //  Finally, perform the non-redundification....
    //  In each cluster, mark items under a pref tax node and but not model orgs.

    CSimpleNonRedundifier simpleNR;
    prefNodes->ShouldMatch();
    simpleNR.AddCriteria(prefNodes, true);  //  set verbose output; nr owns object
    modelOrgs->ShouldNotMatch();
    simpleNR.AddCriteria(modelOrgs, true);  //  set verbose output; nr owns object
    simpleNR.SetClusterer(clusterer);       //  nr object assumes ownership

    unsigned int nRedundant = simpleNR.ComputeRedundancies();

    //  List of final redundant indices (those not kept by each criteria)
    CBaseClusterer::TClusterId cid;
    ERR_POST(ncbi::Info << "\n\nFound " << nRedundant << " redundancies.\n\n");
    for (unsigned int i = 0; i < dim; ++i) {
        if (! simpleNR.GetItemStatus(i, &cid)) {
            ERR_POST(ncbi::Info << "ID " << setw(4) << i << ";  Cluster " << cid << "\n");
        }
    }

    //  Cleaning up (other ptrs declared cleaned up when simpleNR is destroyed)

    delete [] dm;
    if (args["o"]) ((CNcbiOfstream*)outStr)->close();

    return 0;
}



void NRDemo::Exit(void)
{
    SetDiagStream(0);
}

void NRDemo::test()
{
	return;
}


int main(int argc, const char* argv[])
{
    //  Diagnostics code from Paul's struct_util demo.
    SetDiagStream(&NcbiCerr); // send all diagnostic messages to cerr
    SetDiagPostLevel(eDiag_Info);   
//    SetupCToolkitErrPost(); // reroute C-toolkit err messages to C++ err streams

    SetDiagTrace(eDT_Default);      // trace messages only when DIAG_TRACE env. var. is set
//#ifdef _DEBUG
//    SetDiagPostFlag(eDPF_File);
//    SetDiagPostFlag(eDPF_Line);
//#else
    UnsetDiagTraceFlag(eDPF_File);
    UnsetDiagTraceFlag(eDPF_Line);
//#endif

    // Execute main application function
    NRDemo nrDemo;
    int result = nrDemo.AppMain(argc, argv, 0, eDS_Default, 0);
    return result;
}


