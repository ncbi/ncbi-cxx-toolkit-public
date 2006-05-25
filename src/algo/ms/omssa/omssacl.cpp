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
#include <serial/objostrxml.hpp>
#include <corelib/ncbifile.hpp>

#include <fstream>
#include <string>
#include <list>
#include <stdio.h>

#include "omssa.hpp"
#include "SpectrumSet.hpp"
#include "Mod.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  COMSSA
//
//  Main application
//
typedef list <string> TStringList;

class COMSSA : public CNcbiApplication {
public:
    COMSSA();
private:
    virtual int Run();
    virtual void Init();
    void PrintMods(CRef <CMSModSpecSet> Modset);
    void PrintEnzymes(void);
    void PrintIons(void);

    //! reads in modification files
    int ReadModFiles(CArgs& args, CRef <CMSModSpecSet> Modset);
    template <class T> void InsertList(const string& List, T& ToInsert, string error); 

    /**
     * Read in a spectrum file
     * 
     * @param Filename name of file
     * @param FileType type of file to be read in
     * @param MySearch the search
     * @return 1, -1 = error, 0 = ok
     */
    int ReadFile(const string& Filename, 
                 const EFileType FileType, 
                 CMSSearch& MySearch);

    /**
     * Read in a complete search
     * @param Filename name of file
     * @param Dataformat xml or asn.1
     * @param MySearch the search
     * @return 0 if OK
     */
    int ReadCompleteSearch(const string& Filename,
                           const ESerialDataFormat DataFormat,
                           CMSSearch& MySearch);
    
    /**
     * Set search settings given args
     */
    void SetSearchSettings(CArgs& args, CRef<CMSSearchSettings> Settings);
};

COMSSA::COMSSA()
{
    SetVersion(CVersionInfo(2, 0, 0));
}


int COMSSA::ReadFile(const string& Filename,
                     const EFileType FileType,
                     CMSSearch& MySearch)
{
    CNcbiIfstream PeakFile(Filename.c_str());
    if(!PeakFile) {
        ERR_POST(Fatal <<" omssacl: not able to open spectrum file " <<
                 Filename);
        return 1;
    }
    // create request and response objects if necessary
    if(!MySearch.CanGetRequest() || MySearch.SetRequest().empty()) {
        CRef <CMSRequest> Request (new CMSRequest);
        MySearch.SetRequest().push_back(Request);
    }    
    if(!MySearch.CanGetResponse() || MySearch.SetResponse().empty()) {
        CRef <CMSResponse> Response (new CMSResponse);
        MySearch.SetResponse().push_back(Response);
    }

    CRef <CSpectrumSet> SpectrumSet(new CSpectrumSet);
    (*MySearch.SetRequest().begin())->SetSpectra(*SpectrumSet);
    return SpectrumSet->LoadFile(FileType, PeakFile);
}   


int COMSSA::ReadCompleteSearch(const string& Filename,
                               const ESerialDataFormat DataFormat,
                               CMSSearch& MySearch)
{
    auto_ptr<CObjectIStream> 
        in(CObjectIStream::Open(Filename.c_str(), DataFormat));
    in->Open(Filename.c_str(), DataFormat);
    if(in->fail()) {	    
        ERR_POST(Warning << "ommsacl: unable to search file" << 
                 Filename);
        return 1;
    }
    in->Read(ObjectInfo(MySearch));
    in->Close();
    return 0;
}
    

template <class T> void COMSSA::InsertList(const string& Input, T& ToInsert, string error) 
{
    TStringList List;
    NStr::Split(Input, ",", List);

    TStringList::iterator iList(List.begin());
    int Num;

    for(;iList != List.end(); iList++) {
	try {
	    Num = NStr::StringToInt(*iList);
	} catch (CStringException &e) {
	    ERR_POST(Info << error);
	    continue;
	}
	ToInsert.push_back(Num);
    }
}


///
/// print out a list of modification that can be used in OMSSA
///
void COMSSA::PrintMods(CRef <CMSModSpecSet> Modset)
{
    typedef multimap <string, int> TModMap;
    TModMap ModMap;
    int i;
    for(i = 0; i < eMSMod_max; i++) {
        if(Modset->GetModMass(i) != 0.0)
            ModMap.insert(TModMap::value_type(Modset->GetModName(i), i));
    }

    cout << " # : Name" << endl;
    ITERATE (TModMap, iModMap, ModMap) {
        cout << setw(3) << iModMap->second << ": " << iModMap->first << endl;
    }
}


///
/// print out a list of enzymes that can be used in OMSSA
///
void COMSSA::PrintEnzymes(void)
{
    int i;
    for(i = 0; i < eMSEnzymes_max; i++)
	cout << i << ": " << kEnzymeNames[i] << endl;
}

///
/// print out a list of ions that can be used in OMSSA
///
void COMSSA::PrintIons(void)
{
    int i;
    for(i = 0; i < eMSIonType_max; i++)
	cout << i << ": " << kIonLabels[i] << endl;
}


void COMSSA::Init()
{

    auto_ptr<CArgDescriptions> argDesc(new CArgDescriptions);
    argDesc->PrintUsageIfNoArgs();

    argDesc->AddDefaultKey("d", "blastdb", "Blast sequence library to search. Do not include .p* filename suffixes.",
			   CArgDescriptions::eString, "nr");
    argDesc->AddDefaultKey("f", "infile", "single dta file to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fx", "xmlinfile", "multiple xml-encapsulated dta files to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fb", "dtainfile", "multiple dta files separated by blank lines to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fp", "pklinfile", "pkl formatted file",
                CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fm", "pklinfile", "mgf formatted file",
                CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("foms", "omsinfile", "omssa oms file",
                 CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fomx", "omxinfile", "omssa omx file",
                  CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("o", "textasnoutfile", "filename for text asn.1 formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("ob", "binaryasnoutfile", "filename for binary asn.1 formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("ox", "xmloutfile", "filename for xml formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("oc", "csvfile", "filename for csv formatted search summary",
                CArgDescriptions::eString, "");
    argDesc->AddFlag("w", "include spectra and search params in search results");
    argDesc->AddDefaultKey("to", "pretol", "product ion mass tolerance in Da",
			   CArgDescriptions::eDouble, "0.8");
    argDesc->AddDefaultKey("te", "protol", "precursor ion  mass tolerance in Da",
			   CArgDescriptions::eDouble, "2.0");
    argDesc->AddDefaultKey("tom", "promass", "product ion search type (0 = mono, 1 = avg, 2 = N15, 3 = exact)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("tem", "premass", "precursor ion search type (0 = mono, 1 = avg, 2 = N15, 3 = exact)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("tez", "prozdep", "charge dependency of precursor mass tolerance (0 = none, 1 = linear)",
                CArgDescriptions::eInteger, "1");
    argDesc->AddDefaultKey("tex", "exact", 
                   "threshold in Da above which the mass of neutron should be added in exact mass search",
                   CArgDescriptions::eDouble, 
                   "1446.94");

    argDesc->AddDefaultKey("i", "ions", 
               "id numbers of ions to search (comma delimited, no spaces)",
               CArgDescriptions::eString, 
               "1,4");
    argDesc->AddFlag("il", "print a list of ions and their corresponding id number");


    argDesc->AddDefaultKey("cl", "cutlo", "low intensity cutoff as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.0");
    argDesc->AddDefaultKey("ch", "cuthi", "high intensity cutoff as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.2");
    argDesc->AddDefaultKey("ci", "cutinc", "intensity cutoff increment as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.0005");
    argDesc->AddDefaultKey("v", "cleave", 
			   "number of missed cleavages allowed",
			   CArgDescriptions::eInteger, "1");
    argDesc->AddDefaultKey("x", "taxid", 
			   "comma delimited list of taxids to search (0 = all)",
			   CArgDescriptions::eString, "0");
    argDesc->AddDefaultKey("w1", "window1", 
			   "single charge window in Da",
			   CArgDescriptions::eInteger, "20");
    argDesc->AddDefaultKey("w2", "window2", 
			   "double charge window in Da",
			   CArgDescriptions::eInteger, "14");
    argDesc->AddDefaultKey("h1", "hit1", 
			   "number of peaks allowed in single charge window",
			   CArgDescriptions::eInteger, "2");
    argDesc->AddDefaultKey("h2", "hit2", 
			   "number of peaks allowed in double charge window",
			   CArgDescriptions::eInteger, "2");
    argDesc->AddDefaultKey("hl", "hitlist", 
			   "maximum number of hits retained per precursor charge state per spectrum",
			   CArgDescriptions::eInteger, "30");
    argDesc->AddDefaultKey("ht", "tophitnum", 
			   "number of m/z values corresponding to the most intense peaks that must include one match to the theoretical peptide",
			   CArgDescriptions::eInteger, "6");
    argDesc->AddDefaultKey("hm", "minhit", 
			   "the minimum number of m/z matches a sequence library peptide must have for the hit to the peptide to be recorded",
			   CArgDescriptions::eInteger, "2");
    argDesc->AddDefaultKey("hs", "minspectra", 
			   "the minimum number of m/z values a spectrum must have to be searched",
			   CArgDescriptions::eInteger, "4");
    argDesc->AddDefaultKey("he", "evalcut", 
			   "the maximum evalue allowed in the hit list",
			   CArgDescriptions::eDouble, "1");
    argDesc->AddDefaultKey("mf", "fixedmod", 
			   "comma delimited (no spaces) list of id numbers for fixed modifications",
			   CArgDescriptions::eString,
			   "");
    argDesc->AddDefaultKey("mv", "variablemod", 
			   "comma delimited (no spaces) list of id numbers for variable modifications",
			   CArgDescriptions::eString,
			   "");
    argDesc->AddFlag("ml", "print a list of modifications and their corresponding id number");
    argDesc->AddDefaultKey("mx", "modinputfile", 
			   "file containing modification data",
			   CArgDescriptions::eString,
			   "mods.xml");
    argDesc->AddDefaultKey("mux", "usermodinputfile", 
                "file containing user modification data",
                CArgDescriptions::eString,
                "usermods.xml");
    argDesc->AddDefaultKey("mm", "maxmod", 
			   "the maximum number of mass ladders to generate per database peptide",
			   CArgDescriptions::eInteger, /*NStr::IntToString(MAXMOD2)*/ "128");
    argDesc->AddDefaultKey("e", "enzyme", 
			   "id number of enzyme to use",
			   CArgDescriptions::eInteger, 
			   NStr::IntToString(eMSEnzymes_trypsin));
    argDesc->AddFlag("el", "print a list of enzymes and their corresponding id number");
	argDesc->AddDefaultKey("zh", "maxcharge", 
				"maximum precursor charge to search when not 1+",
				CArgDescriptions::eInteger, 
				"3");
	argDesc->AddDefaultKey("zl", "mincharge", 
				"minimum precursor charge to search when not 1+",
				CArgDescriptions::eInteger, 
				"1");
	argDesc->AddDefaultKey("zt", "chargethresh", 
				 "minimum precursor charge to start considering multiply charged products",
				 CArgDescriptions::eInteger, 
				 "3");
    argDesc->AddDefaultKey("z1", "plusone", 
                  "fraction of peaks below precursor used to determine if spectrum is charge 1",
                  CArgDescriptions::eDouble, 
                  "0.95");
    argDesc->AddDefaultKey("zc", "calcplusone", 
                  "should charge plus one be determined algorithmically? (1=yes)",
                  CArgDescriptions::eInteger, 
                  NStr::IntToString(eMSCalcPlusOne_calc));
    argDesc->AddDefaultKey("pc", "pseudocount", 
                  "minimum number of precursors that match a spectrum",
                  CArgDescriptions::eInteger, 
                  "1");
    argDesc->AddDefaultKey("sb1", "searchb1", 
                   "should first forward (b1) product ions be in search (1 = no)",
                   CArgDescriptions::eInteger, 
                   "1");
    argDesc->AddDefaultKey("sct", "searchcterm", 
                   "should c terminus ions be searched (1=no)",
                   CArgDescriptions::eInteger, 
                   "0");
    argDesc->AddDefaultKey("sp", "productnum", 
                    "max number of ions in each series being searched (0=all)",
                    CArgDescriptions::eInteger, 
                    "100");
    argDesc->AddDefaultKey("no", "minno", 
                     "minimum size of peptides for no-enzyme and semi-tryptic searches",
                     CArgDescriptions::eInteger, 
                     "4");
    argDesc->AddDefaultKey("nox", "maxno", 
                      "maximum size of peptides for no-enzyme and semi-tryptic searches (0=none)",
                      CArgDescriptions::eInteger, 
                      "40");
    argDesc->AddDefaultKey("is", "subsetthresh", 
                           "evalue threshold to include a sequence in the iterative search, 0 = all",
                           CArgDescriptions::eDouble, 
                           "0.0");
    argDesc->AddDefaultKey("ir", "replacethresh", 
                            "evalue threshold to replace a hit, 0 = only if better",
                            CArgDescriptions::eDouble, 
                            "0.0");
    argDesc->AddDefaultKey("ii", "iterativethresh", 
                            "evalue threshold to iteratively search a spectrum again, 0 = always",
                            CArgDescriptions::eDouble, 
                            "0.01");
    argDesc->AddFlag("ns", "depreciated flag"); // no longer has an effect, replaced by "os"
    argDesc->AddFlag("os", "use omssa 1.0 scoring");


    SetupArgDescriptions(argDesc.release());

    // allow info posts to be seen
    SetDiagPostLevel(eDiag_Info);
}

int main(int argc, const char* argv[]) 
{
    COMSSA theTestApp;
    return theTestApp.AppMain(argc, argv, 0, eDS_Default, 0);
}


//! reads in modification files

int COMSSA::ReadModFiles(CArgs& args, CRef <CMSModSpecSet> Modset)
{  
    CDirEntry DirEntry(GetProgramExecutablePath());
    string ModFileName;
    if(args["mx"].AsString() == "")
        ERR_POST(Critical << "modification filename is blank!");
    if(!CDirEntry::IsAbsolutePath(args["mx"].AsString()))
        ModFileName = DirEntry.GetDir() + args["mx"].AsString();
    auto_ptr<CObjectIStream> 
    modsin(CObjectIStream::Open(ModFileName.c_str(), eSerial_Xml));
    if(modsin->fail()) {	    
        ERR_POST(Fatal << "ommsacl: unable to open modification file" << 
                 ModFileName);
        return 1;
    }
    modsin->Read(ObjectInfo(*Modset));
    modsin->Close();

    // read in user mod file, if any
    if(args["mux"].AsString() != "") {
        CRef <CMSModSpecSet> UserModset(new CMSModSpecSet);
        if(!CDirEntry::IsAbsolutePath(args["mux"].AsString()))
            ModFileName = DirEntry.GetDir() + args["mux"].AsString();
        auto_ptr<CObjectIStream> 
         usermodsin(CObjectIStream::Open(ModFileName.c_str(), eSerial_Xml));
        usermodsin->Open(ModFileName.c_str(), eSerial_Xml);
        if(usermodsin->fail()) {	    
             ERR_POST(Warning << "ommsacl: unable to open user modification file" << 
                      ModFileName);
             return 0;
         }
        usermodsin->Read(ObjectInfo(*UserModset));
        usermodsin->Close();
        Modset->Append(*UserModset);
    }
    return 0;
}


void COMSSA::SetSearchSettings(CArgs& args, CRef<CMSSearchSettings> Settings)
{
	Settings->SetPrecursorsearchtype(args["tem"].AsInteger());
	Settings->SetProductsearchtype(args["tom"].AsInteger());
	Settings->SetPeptol(args["te"].AsDouble());
	Settings->SetMsmstol(args["to"].AsDouble());
    Settings->SetZdep(args["tez"].AsInteger());
    Settings->SetExactmass(args["tex"].AsDouble());

	InsertList(args["i"].AsString(), Settings->SetIonstosearch(), "unknown ion");
	Settings->SetCutlo(args["cl"].AsDouble());
	Settings->SetCuthi(args["ch"].AsDouble());
	Settings->SetCutinc(args["ci"].AsDouble());
	Settings->SetSinglewin(args["w1"].AsInteger());
	Settings->SetDoublewin(args["w2"].AsInteger());
	Settings->SetSinglenum(args["h1"].AsInteger());
	Settings->SetDoublenum(args["h2"].AsInteger());
	Settings->SetEnzyme(args["e"].AsInteger());
	Settings->SetMissedcleave(args["v"].AsInteger());
	InsertList(args["mv"].AsString(), Settings->SetVariable(), "unknown variable mod");
	InsertList(args["mf"].AsString(), Settings->SetFixed(), "unknown fixed mod");
	Settings->SetDb(args["d"].AsString());
	Settings->SetHitlistlen(args["hl"].AsInteger());
	Settings->SetTophitnum(args["ht"].AsInteger());
	Settings->SetMinhit(args["hm"].AsInteger());
	Settings->SetMinspectra(args["hs"].AsInteger());
    Settings->SetScale(MSSCALE); // presently ignored
	Settings->SetCutoff(args["he"].AsDouble());
	Settings->SetMaxmods(args["mm"].AsInteger());
    Settings->SetPseudocount(args["pc"].AsInteger());
    Settings->SetSearchb1(args["sb1"].AsInteger());
    Settings->SetSearchctermproduct(args["sct"].AsInteger());
    Settings->SetMaxproductions(args["sp"].AsInteger());
    Settings->SetMinnoenzyme(args["no"].AsInteger());
    Settings->SetMaxnoenzyme(args["nox"].AsInteger());

	if(args["x"].AsString() != "0") {
	    InsertList(args["x"].AsString(), Settings->SetTaxids(), "unknown tax id");
	}

	Settings->SetChargehandling().SetCalcplusone(args["zc"].AsInteger());
	Settings->SetChargehandling().SetConsidermult(args["zt"].AsInteger());
	Settings->SetChargehandling().SetMincharge(args["zl"].AsInteger());
	Settings->SetChargehandling().SetMaxcharge(args["zh"].AsInteger());
    Settings->SetChargehandling().SetPlusone(args["z1"].AsDouble());

    Settings->SetIterativesettings().SetResearchthresh(args["ii"].AsDouble());
    Settings->SetIterativesettings().SetSubsetthresh(args["is"].AsDouble());
    Settings->SetIterativesettings().SetReplacethresh(args["ir"].AsDouble());

	// validate the input
        list <string> ValidError;
	if(Settings->Validate(ValidError) != 0) {
	    list <string>::iterator iErr;
	    for(iErr = ValidError.begin(); iErr != ValidError.end(); iErr++)
		ERR_POST(Warning << *iErr);
	    ERR_POST(Fatal << "Unable to validate settings");
	}
     return;
}


int COMSSA::Run()
{    

    try {

	CArgs args = GetArgs();
    CRef <CMSModSpecSet> Modset(new CMSModSpecSet);

    // read in modifications
    if(ReadModFiles(args, Modset))
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

	CSearch Search;

    // set up rank scoring
    if(args["os"]) Search.SetRankScore() = false;
    else Search.SetRankScore() = true;

	int retval = Search.InitBlast(args["d"].AsString().c_str());
	if(retval) {
	    ERR_POST(Fatal << "ommsacl: unable to initialize blastdb, error " 
		     << retval);
	    return 1;
	}

    CMSSearch MySearch;

    int FileRetVal(1);

	if(args["fx"].AsString().size() != 0) 
        FileRetVal = ReadFile(args["fx"].AsString(), eDTAXML, MySearch);
	else if(args["f"].AsString().size() != 0)
        FileRetVal = ReadFile(args["f"].AsString(), eDTA, MySearch);
	else if(args["fb"].AsString().size() != 0) 
        FileRetVal = ReadFile(args["fb"].AsString(), eDTABlank, MySearch);
 	else if(args["fp"].AsString().size() != 0) 
        FileRetVal = ReadFile(args["fp"].AsString(), ePKL, MySearch);
    else if(args["fm"].AsString().size() != 0)
        FileRetVal = ReadFile(args["fm"].AsString(), eMGF, MySearch);
    else if(args["fomx"].AsString().size() != 0) {
        FileRetVal = ReadCompleteSearch(args["fomx"].AsString(), eSerial_Xml, MySearch);
        Search.SetIterative() = true;
    }
    else if(args["foms"].AsString().size() != 0) {
         FileRetVal = ReadCompleteSearch(args["foms"].AsString(), eSerial_AsnBinary, MySearch);
         Search.SetIterative() = true;
    }
	else {
	    ERR_POST(Fatal << "omssacl: input file not given.");
	    return 1;
	}

    if(FileRetVal == -1) {
        ERR_POST(Fatal << "omssacl: too many spectra in input file");
        return 1;
    }
    else if(FileRetVal == 1) {
        ERR_POST(Fatal << "omssacl: unable to read spectrum file -- incorrect file type?");
        return 1;
    }

    // which search settings to use
    CRef <CMSSearchSettings> SearchSettings;
    // the ordinal number of the SearchSettings
    int Settingid(0);

    // set up search settings
    if(Search.GetIterative()) {
        Settingid = 1 + (*MySearch.SetRequest().begin())->SetMoresettings().Set().size();
        SearchSettings = new CMSSearchSettings;
        (*MySearch.SetRequest().begin())->SetMoresettings().Set().push_back(SearchSettings);
    }
    else {
        SearchSettings.Reset(&((*MySearch.SetRequest().begin())->SetSettings()));
    }
    SearchSettings->SetSettingid() = Settingid;
    SetSearchSettings(args, SearchSettings);

	_TRACE("omssa: search begin");
	Search.Search(*MySearch.SetRequest().begin(),
                  *MySearch.SetResponse().begin(), 
                  Modset,
                  SearchSettings);
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

	// output
    auto_ptr <CObjectOStream> txt_out;  
    CNcbiOfstream os;

	if(args["o"].AsString() != "") {
		txt_out.reset(CObjectOStream::Open(args["o"].AsString().c_str(),
					     eSerial_AsnText));
	}
	else if(args["ob"].AsString() != "") {
		txt_out.reset(CObjectOStream::Open(args["ob"].AsString().c_str(),
					     eSerial_AsnBinary));
	}
	else if(args["ox"].AsString() != "") {
        txt_out.reset(CObjectOStream::Open(args["ox"].AsString().c_str(), eSerial_Xml));
        // turn on xml schema
        CObjectOStreamXml *xml_out = dynamic_cast <CObjectOStreamXml *> (txt_out.get());
        xml_out->SetReferenceSchema();
        // turn off names in named integers
        xml_out->SetWriteNamedIntegersByValue(true);
	}

    if(txt_out.get() != 0) {
        if(args["w"]) {
            // write out
            txt_out->Write(ObjectInfo(MySearch));
        }
        else {
            txt_out->Write(ObjectInfo(**MySearch.SetResponse().begin()));
        }
    }

    // print csv if requested
    if(args["oc"].AsString() != "") {
        CNcbiOfstream oscsv;
        oscsv.open(args["oc"].AsString().c_str());
        (*MySearch.SetResponse().begin())->PrintCSV(oscsv, Modset);
        oscsv.close();
    }


    } catch (NCBI_NS_STD::exception& e) {
	ERR_POST(Fatal << "Exception in COMSSA::Run: " << e.what());
    }

    return 0;
}



/*
  $Log$
  Revision 1.48  2006/05/25 17:11:56  lewisg
  one filtered spectrum per precursor charge state

  Revision 1.47  2005/11/16 20:01:13  lewisg
  turn off attribute tag in xml

  Revision 1.46  2005/11/07 19:57:20  lewisg
  iterative search

  Revision 1.45  2005/10/24 21:46:13  lewisg
  exact mass, peptide size limits, validation, code cleanup

  Revision 1.44  2005/09/22 14:58:03  ucko
  Tweak PrintMods to compile with WorkShop's ultra-strict STL; take
  advantage of ITERATE along the way.

  Revision 1.43  2005/09/21 20:08:50  lewisg
  make PTMs consistent

  Revision 1.42  2005/09/20 21:07:57  lewisg
  get rid of c-toolkit dependencies and nrutil

  Revision 1.41  2005/09/14 15:30:17  lewisg
  neutral loss

  Revision 1.40  2005/08/15 14:24:56  lewisg
  new mod, enzyme; stat test

  Revision 1.39  2005/08/01 13:44:18  lewisg
  redo enzyme classes, no-enzyme, fix for fixed mod enumeration

  Revision 1.38  2005/07/20 20:32:24  lewisg
  new version

  Revision 1.37  2005/06/16 21:22:11  lewisg
  fix n dependence

  Revision 1.36  2005/05/27 20:23:38  lewisg
  top-down charge handling

  Revision 1.35  2005/05/19 16:59:17  lewisg
  add top-down searching, fix variable mod bugs

  Revision 1.34  2005/05/13 17:57:17  lewisg
  one mod per site and bug fixes

  Revision 1.33  2005/04/21 21:54:03  lewisg
  fix Jeri's mem bug, split off mod file, add aspn and gluc

  Revision 1.32  2005/04/05 21:26:34  lewisg
  adjust ht parameter

  Revision 1.31  2005/04/05 21:02:52  lewisg
  increase number of mods, fix gi problem, fix empty scan bug

  Revision 1.30  2005/03/25 22:02:34  lewisg
  fix code to honor lower charge bound

  Revision 1.29  2005/03/22 22:22:34  lewisg
  search executable path for mods.xml

  Revision 1.28  2005/03/15 20:47:55  lewisg
  fix bug in pkl import

  Revision 1.27  2005/03/14 22:29:54  lewisg
  add mod file input

  Revision 1.26  2005/01/31 17:30:57  lewisg
  adjustable intensity, z dpendence of precursor mass tolerance

  Revision 1.25  2005/01/11 21:08:43  lewisg
  average mass search

  Revision 1.24  2004/12/06 22:57:34  lewisg
  add new file formats

  Revision 1.23  2004/12/03 21:14:16  lewisg
  file loading code

  Revision 1.22  2004/11/30 23:39:57  lewisg
  fix interval query

  Revision 1.21  2004/11/22 23:10:36  lewisg
  add evalue cutoff, fix fixed mods

  Revision 1.20  2004/11/01 22:04:12  lewisg
  c-term mods

  Revision 1.19  2004/10/20 22:24:48  lewisg
  neutral mass bugfix, concatenate result and response

  Revision 1.18  2004/09/29 19:43:09  lewisg
  allow setting of ions

  Revision 1.17  2004/09/15 18:35:00  lewisg
  cz ions

  Revision 1.16  2004/07/06 22:38:05  lewisg
  tax list input and user settable modmax

  Revision 1.15  2004/06/23 22:34:36  lewisg
  add multiple enzymes

  Revision 1.14  2004/06/08 19:46:21  lewisg
  input validation, additional user settable parameters

  Revision 1.13  2004/05/27 20:52:15  lewisg
  better exception checking, use of AutoPtr, command line parsing

  Revision 1.12  2004/05/21 21:41:03  gorelenk
  Added PCH ncbi_pch.hpp

  Revision 1.11  2004/03/30 19:36:59  lewisg
  multiple mod code

  Revision 1.10  2004/03/16 22:09:11  gorelenk
  Changed include for private header.

  Revision 1.9  2004/03/01 18:24:08  lewisg
  better mod handling

  Revision 1.8  2003/12/04 23:39:09  lewisg
  no-overlap hits and various bugfixes

  Revision 1.7  2003/11/20 15:40:53  lewisg
  fix hitlist bug, change defaults

  Revision 1.6  2003/11/18 18:16:04  lewisg
  perf enhancements, ROCn adjusted params made default

  Revision 1.5  2003/11/13 19:07:38  lewisg
  bugs: iterate completely over nr, don't initialize blastdb by iteration

  Revision 1.4  2003/11/10 22:24:12  lewisg
  allow hitlist size to vary

  Revision 1.3  2003/10/27 20:10:55  lewisg
  demo program to read out omssa results

  Revision 1.2  2003/10/21 21:12:17  lewisg
  reorder headers

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
