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
*    reads in an omssa search result, which is formatted in ASN.1,
*    iterates through the results and prints them.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
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
#include <objects/omssa/omssa__.hpp>

#include <string>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

// subclass of standard NCBI application class
class CReadresult : public CNcbiApplication {
public:
    virtual int Run();
    virtual void Init();
};


// initialize application
void CReadresult::Init()
{
    auto_ptr<CArgDescriptions> argDesc(new CArgDescriptions);
	
	// set up command line arguments
    argDesc->AddDefaultKey("o", "omssafile", 
		"file with results from omssa",
		CArgDescriptions::eString, "");
	
    SetupArgDescriptions(argDesc.release());
	
    SetDiagPostLevel(eDiag_Info);
}

int main(int argc, const char* argv[]) 
{
    CReadresult theTestApp;
    return theTestApp.AppMain(argc, argv);
}

// run application
int CReadresult::Run()
{    
    const CArgs& args = GetArgs();
	
    try {
		
		// read in omssa files
		if(args["o"].AsString().size() > 0) {
			// input stream (specialized for ASN.1)
			auto_ptr<CObjectIStream> 
				in(CObjectIStream::Open(args["o"].AsString().c_str(),
				eSerial_AsnText));
			// object to hold input
			CMSResponse Response;
			// read it in
			in->Read(ObjectInfo(Response));
			
			
			CMSResponse::THitsets::const_iterator iHits;
			iHits = Response.GetHitsets().begin();
			
			// iterate through sets of hits, where each set corresponds
			// to a spectrum
			for(; iHits != Response.GetHitsets().end(); iHits++) {
				CRef< CMSHitSet > HitSet =  *iHits;
				
				// ERR_POST macro writes to error log, which is usually cerr
				ERR_POST(Info << "Hitset: " << HitSet->GetNumber());
				if( HitSet-> CanGetError() && HitSet->GetError() ==
					eMSHitError_notenuffpeaks) {
					ERR_POST(Info << "Hitset Empty");
					continue;
				}
				
				// iterate through peptide hits for each spectrum
				CMSHitSet::THits::const_iterator iHit; 
				CMSHits::TPephits::const_iterator iPephit;
				for(iHit = HitSet->GetHits().begin();
				iHit != HitSet->GetHits().end(); iHit++) {
					ERR_POST(Info << (*iHit)->GetPepstring() << ": " << "P = " << 
						(*iHit)->GetPvalue() << " E = " <<
						(*iHit)->GetEvalue());    
					
					// iterate through each sequence that contains the peptide hit
					for(iPephit = (*iHit)->GetPephits().begin();
					iPephit != (*iHit)->GetPephits().end();
					iPephit++) {
						ERR_POST(Info << (*iPephit)->GetGi() << ": " << (*iPephit)->GetStart() << "-" << (*iPephit)->GetStop() << ":" << (*iPephit)->GetDefline());
					}
				}
			}	
		}
		
    } catch (exception& e) {
		ERR_POST(Fatal << e.what());
		return 1;
    }
	
    return 0;
}
