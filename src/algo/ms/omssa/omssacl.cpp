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
 * Author:  Lewis Y. Geer
 *  
 * File Description:
 *    command line OMSSA search
 *
 *
 * ===========================================================================
 */

#include <stdio.h>
#include <readdb.h>

#include <fstream>
#include <string>
#include <list>

#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>

#include <msms.hpp>
#include <mspeak.hpp>
#include <omssa.hpp>
#include <objects/omssa/omssa__.hpp>
#include <SpectrumSet.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  COMSSA
//
//  Main application
//

class COMSSA : public CNcbiApplication {
public:
    virtual int Run();
    virtual void Init();
};


void COMSSA::Init()
{
    auto_ptr<CArgDescriptions> argDesc(new CArgDescriptions);

    argDesc->AddDefaultKey("d", "blastdb", "Blast database to parse",
			   CArgDescriptions::eString, "yeast");
    argDesc->AddDefaultKey("f", "infile", "dta file to read in",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("m", "xmlinfile", "xml encapsulated dta file to read in",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("o", "outfile", "search results file",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("g", "graph", "graph file to write out",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("t", "tol", "msms tolerance",
			   CArgDescriptions::eDouble, "1.5");
    argDesc->AddDefaultKey("p", "ptol", "peptide mass tolerance",
			   CArgDescriptions::eDouble, "20.0");
    argDesc->AddDefaultKey("i", "baseline", "baseline cutoff by fraction of max intensity",
			   CArgDescriptions::eDouble, "0.025");
    argDesc->AddDefaultKey("c", "cull", 
			   "number of peaks to leave in each 100Da bin",
			   CArgDescriptions::eInteger, "3");
    argDesc->AddDefaultKey("l", "cleave", 
			   "number of missed cleavages allowed",
			   CArgDescriptions::eInteger, "1");
    argDesc->AddDefaultKey("n", "numseq", 
			   "number of sequences to search (0 = all)",
			   CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("x", "taxid", 
			   "taxid to search (0 = all)",
			   CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("w1", "window1", 
			   "single charge window",
			   CArgDescriptions::eInteger, "26");
    argDesc->AddDefaultKey("w2", "window2", 
			   "double charge window",
			   CArgDescriptions::eInteger, "14");

    SetupArgDescriptions(argDesc.release());

    // allow info posts to be seen
    SetDiagPostLevel(eDiag_Info);
}

int main(int argc, const char* argv[]) 
{
    COMSSA theTestApp;
    return theTestApp.AppMain(argc, argv);
}


int COMSSA::Run()
{    
    CArgs args = GetArgs();
    _TRACE("omssa: initializing score");
    CSearch Search(false);
    int retval = Search.InitBlast(args["d"].AsString().c_str(), true);
    if(retval) {
	ERR_POST(Fatal << "ommsatest: unable to initialize blastdb, error " <<
	    retval);
	return 1;
    }
    _TRACE("ommsa: score initialized");
    CRef <CSpectrumSet> Spectrumset(new CSpectrumSet);

    if(args["m"].AsString().size() != 0) {
	ifstream PeakFile(args["m"].AsString().c_str());
	if(!PeakFile) {
	    ERR_POST(Fatal <<" omssatest: not able to open spectrum file " <<
		args["m"].AsString());
	    return 1;
	}
	Spectrumset->LoadMultDTA(PeakFile);
    }
    else if(args["f"].AsString().size() != 0) {
	ifstream PeakFile(args["f"].AsString().c_str());
	if(!PeakFile) {
	    ERR_POST(Fatal << "omssatest: not able to open spectrum file " <<
		args["f"].AsString());
	    return 1;
	}
	Spectrumset->LoadDTA(PeakFile);
    }
    else {
	ERR_POST(Fatal << "omssatest: input file not given.");
	return 1;
    }

    CMSResponse Response;
    CMSRequest Request;

    Request.SetPeptol(args["p"].AsDouble());
    Request.SetMsmstol(args["t"].AsDouble());
    Request.SetHaircut(args["i"].AsDouble());
    Request.SetEnzyme(eMSEnzymes_trypsin);
    Request.SetMissedcleave(args["l"].AsInteger());
    Request.SetSpectra(*Spectrumset);
    Request.SetFixed().push_back(eMSMod_moxy);
    Request.SetVariable().push_back(eMSMod_moxy);
    Request.SetDb(args["d"].AsString());
    Request.SetCull(args["c"].AsInteger());
    if(args["x"].AsInteger() != 0) 
	Request.SetTaxids().push_back(args["x"].AsInteger());


    _TRACE("omssa: search begin");
    Search.Search(Request, Response, 50, 
		  args["w1"].AsInteger(), 
		  args["w2"].AsInteger());
    _TRACE("omssa: search end");

    // read out hits
    CMSResponse::THitsets::const_iterator iHits;
    iHits = Response.GetHitsets().begin();
    for(; iHits != Response.GetHitsets().end(); iHits++) {
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
	    //	Hit = *iHit;
	    ERR_POST(Info << (*iHit)->GetPepstring() << ": " << "P = " << 
		(*iHit)->GetPvalue() << " E = " <<
		(*iHit)->GetEvalue());    
	    for(iPephit = (*iHit)->GetPephits().begin();
		iPephit != (*iHit)->GetPephits().end();
		iPephit++) {
		ERR_POST(Info << (*iPephit)->GetGi() << ": " << (*iPephit)->GetStart() << "-" << (*iPephit)->GetStop() << ":" << (*iPephit)->GetDefline());
	    }
    
	}
	cerr << endl << endl;
    }


    if(args["o"].AsString() != "") {
	auto_ptr<CObjectOStream>
	    txt_out(CObjectOStream::Open(args["o"].AsString().c_str(),
					 eSerial_AsnText));
	txt_out->Write(ObjectInfo(Response));
    }
#if MSGRAPH
    if(args["g"].AsString() != "") {
	CMSDrawSpectra DrawIt(kMSWIDTH, kMSHEIGHT);
	DrawIt.Init(Request);
	DrawIt.DrawHit(Response, 0, Search, true, true);
	DrawIt.DrawSpectra();
	
	FILE *os;
	os = fopen(args["g"].AsString().c_str(), "wb");
	DrawIt.Output(os);
	fclose(os);
    }
#endif
    return 0;
}



/*
  $Log$
  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.1  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.10  2003/10/06 18:14:17  lewisg
  threshold vary

  Revision 1.9  2003/07/21 20:25:03  lewisg
  fix missing peak bug

  Revision 1.8  2003/07/17 18:45:50  lewisg
  multi dta support

  Revision 1.7  2003/07/07 16:17:51  lewisg
  new poisson distribution and turn off histogram

  Revision 1.6  2003/05/01 14:52:10  lewisg
  fixes to scoring

  Revision 1.5  2003/04/18 20:46:52  lewisg
  add graphing to omssa

  Revision 1.4  2003/04/04 18:43:51  lewisg
  tax cut

  Revision 1.3  2003/04/02 18:49:51  lewisg
  improved score, architectural changes

  Revision 1.2  2003/02/10 19:37:56  lewisg
  perf and web page cleanup

  Revision 1.1  2003/02/03 20:39:06  lewisg
  omssa cgi



*/
