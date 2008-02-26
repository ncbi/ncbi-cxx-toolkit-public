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
 *  Please cite the authors in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Lewis Y. Geer, Douglas J. Slotta
 *  
 * File Description:
 *    command line OMSSA search
 *
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_system.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>
#include <objects/omssa/omssa__.hpp>
#include <serial/objostrxml.hpp>
#include <corelib/ncbifile.hpp>

#include <fstream>
#include <string>
#include <list>
#include <stdio.h>

#include "omssa.hpp"
#include "SpectrumSet.hpp"
#include "Mod.hpp"
#include "omssaapp.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  COMSSA
//
//  Main application
//


class COMSSA : public COMSSABase {
public:
    virtual int Run();
    virtual void AppInit(CArgDescriptions *argDesc);
};


void COMSSA::AppInit(CArgDescriptions *argDesc)
{
    if(!argDesc) return;
    argDesc->AddDefaultKey("mx", "modinputfile", 
                "file containing modification data",
                CArgDescriptions::eString,
                "mods.xml");
     argDesc->AddDefaultKey("mux", "usermodinputfile", 
                 "file containing user modification data",
                 CArgDescriptions::eString,
                 "usermods.xml");
     argDesc->AddDefaultKey("nt", "numthreads",
			    "number of search threads to use, 0=autodetect",
			    CArgDescriptions::eInteger, 
			    "0");
     argDesc->AddFlag("ni", "don't print informational messages");
     argDesc->AddFlag("ns", "depreciated flag"); // to be deprecated
     argDesc->AddFlag("os", "use omssa 1.0 scoring"); // to be deprecated
     argDesc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Search engine for identifying MS/MS peptide spectra");
}


int main(int argc, const char* argv[]) 
{
    COMSSA theTestApp;
    return theTestApp.AppMain(argc, argv, 0, eDS_Default, 0);
}


/** progress callback */
static void OMSSACallback(int TotalSeq, int Completed, void* Anything)
{
    ERR_POST(Info << "Sequence=" << Completed << " Percent=" << (double)Completed/TotalSeq*100.0 <<
             "%");
}

int COMSSA::Run()
{    

    try {

	CArgs args = GetArgs();
    CRef <CMSModSpecSet> Modset(new CMSModSpecSet);

    // turn off informational messages if requested
    if(args["ni"])
       SetDiagPostLevel(eDiag_Warning);

    // read in modifications
    if(CSearchHelper::ReadModFiles(args["mx"].AsString(), args["mux"].AsString(),
                            GetProgramExecutablePath(), Modset))
        return 1;

	// print out the modification list
	if(args["ml"]) {
        Modset->CreateArrays();
	    PrintMods(Modset);
	    return 0;
	}

	// print out the enzymes list
	if(args["el"]) {
	    PrintEnzymes();
	    return 0;
	}

    // print out the ions list
    if(args["il"]) {
        PrintIons();
        return 0;
    }

    CMSSearch MySearch;

    // which search settings to use
    CRef <CMSSearchSettings> SearchSettings;

    // if search settings to be loaded from param file, create and load
    if(args["pm"].AsString().size() != 0) {
        SearchSettings.Reset(new CMSSearchSettings);
        CSearchHelper::CreateSearchSettings(args["pm"].AsString(),
                                            SearchSettings);
    }
    else if (args["fxml"].AsString().size() != 0) {
        // load in MSRequest
        CSearchHelper::ReadSearchRequest(args["fxml"].AsString(),
                                         eSerial_Xml,
                                         MySearch);
        // todo: SearchSettings needs to be set or will be overwritten!
        SearchSettings.Reset(&((*(MySearch.SetRequest().begin()))->SetSettings()));
    }
    else {
        // use command line to set up search settings if no param file
        SearchSettings.Reset(new CMSSearchSettings);
        SetSearchSettings(args, SearchSettings);
    }

    CSearchHelper::ValidateSearchSettings(SearchSettings);

    int nThreads;
#if defined(_MT)
    nThreads = args["nt"].AsInteger();
    if (nThreads == 0) nThreads = GetCpuCount();
    if (nThreads > 1) {
      ERR_POST(Info << "Using " << nThreads << " search threads");
    }
#else
    nThreads = 1;
#endif

    vector < CRef<CSearch> > searchThreads;
    CRef <CSearch> SearchEngine (new CSearch(0));
    searchThreads.push_back(SearchEngine);
    
    //CSearch* SearchEngine = new CSearch();

    // set up rank scoring
    if(args["os"]) SearchEngine->SetRankScore() = false;
    else SearchEngine->SetRankScore() = true;

    int FileRetVal(1);

    if(args["fxml"].AsString().size() == 0) {
        // load in files only if infile specified and not a loaded MSRequest
        if(SearchSettings->GetInfiles().size() == 1) {
            FileRetVal = 
                CSearchHelper::LoadAnyFile(MySearch,
                                           *(SearchSettings->GetInfiles().begin()),
                                            &(SearchEngine->SetIterative()));
            if(FileRetVal == -1) {
                ERR_POST(Fatal << "omssacl: too many spectra in input file");
                return 1;
            }
            else if(FileRetVal == 1) {
                ERR_POST(Fatal << "omssacl: unable to read spectrum file -- incorrect file type?");
                return 1;
            }
        }
        else {
            ERR_POST(Fatal << "omssacl: input file not given or too many input files given.");
            return 1;
        }
        
        // place search settings in search object
        MySearch.SetUpSearchSettings(SearchSettings, 
                                     SearchEngine->GetIterative());
    }

    try {
        SearchEngine->InitBlast(SearchSettings->GetDb().c_str(),
	args["umm"]);
    }
    catch (const NCBI_NS_STD::exception &e) {
        ERR_POST(Fatal << "Unable to open blast library " << SearchSettings->GetDb() << " with error:" <<
                 e.what());
    }
    catch (...) {
        ERR_POST(Fatal << "Unable to open blast library " << SearchSettings->GetDb());
    }

    // set up the response object
    if(MySearch.SetResponse().empty()) {
        CRef <CMSResponse> Response (new CMSResponse);
        MySearch.SetResponse().push_back(Response);
    }

    // Used to be a call to SearchEngine.Search(...)
    SearchEngine->SetupSearch(*MySearch.SetRequest().begin(),
                              *MySearch.SetResponse().begin(), 
                              Modset,
                              SearchSettings,
                              &OMSSACallback);

    for (int i=1; i < nThreads; i++) { 
       CRef <CSearch> SearchThread (new CSearch(i));
       SearchThread->CopySettings(SearchEngine);
       searchThreads.push_back(SearchThread);
    }
    
    _TRACE("omssa: search begin");
    for (int i=0; i < nThreads; i++) { 
       searchThreads[i]->Run(CThread::fRunAllowST);
    }

    bool* result;
    for (int i=0; i < nThreads; i++) {
        searchThreads[i]->Join(reinterpret_cast<void**>(&result));
        //cout << "Returned value: " << *result << endl;
    }
    searchThreads[0]->SetResult(searchThreads[0]->SharedPeakSet);
    _TRACE("omssa: search end");
    

#if _DEBUG
	// read out hits
	CMSResponse::THitsets::const_iterator iHits;
	iHits = (*MySearch.SetResponse().begin())->GetHitsets().begin();
	for(; iHits != (*MySearch.SetResponse().begin())->GetHitsets().end(); iHits++) {
	    CRef< CMSHitSet > HitSet =  *iHits;
	    ERR_POST(Info << "Hitset: " << HitSet->GetNumber());
	    if( HitSet-> CanGetError() && HitSet->GetError() ==
		eMSHitError_notenuffpeaks) {
		ERR_POST(Info << "Hitset Empty");
		continue;
	    }
	    CRef< CMSHits > Hit;
	    CMSHitSet::THits::const_iterator iHit; 
	    CMSHits::TPephits::const_iterator iPephit;
	    for(iHit = HitSet->GetHits().begin();
		iHit != HitSet->GetHits().end(); iHit++) {
		ERR_POST(Info << (*iHit)->GetPepstring() << ": " << "P = " << 
			 (*iHit)->GetPvalue() << " E = " <<
			 (*iHit)->GetEvalue());    
		for(iPephit = (*iHit)->GetPephits().begin();
		    iPephit != (*iHit)->GetPephits().end();
		    iPephit++) {
		    ERR_POST(Info << ((*iPephit)->CanGetGi()?(*iPephit)->GetGi():0) << 
                     ": " << (*iPephit)->GetStart() << "-" << (*iPephit)->GetStop() << 
                     ":" << (*iPephit)->GetDefline());
		}
    
	    }
	}
#endif

	// Check to see if there is a hitset

	if(!(*MySearch.SetResponse().begin())->CanGetHitsets()) {
	  ERR_POST(Fatal << "No results found");
	}

    CSearchHelper::SaveAnyFile(MySearch, 
                               SearchSettings->GetOutfiles(),
                               Modset);
    
    } catch (NCBI_NS_STD::exception& e) {
	ERR_POST(Fatal << "Exception in COMSSA::Run: " << e.what());
    }

    return 0;
}



