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
 *    base omssa application class
 *
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

#include "omssaapp.hpp"

#include <fstream>
#include <string>
#include <list>



USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  COMSSABase
//
//  Main application
//



COMSSABase::COMSSABase()
{
    SetVersion(CVersionInfo(2, 1, 8));
}


///
/// print out a list of modification that can be used in OMSSA
///
void COMSSABase::PrintMods(CRef <CMSModSpecSet> Modset)
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
void COMSSABase::PrintEnzymes(void)
{
    int i;
    for(i = 0; i < eMSEnzymes_max; i++)
	cout << i << ": " << kEnzymeNames[i] << endl;
}

///
/// print out a list of ions that can be used in OMSSA
///
void COMSSABase::PrintIons(void)
{
    int i;
    for(i = 0; i < eMSIonType_parent; i++)
	    cout << i << ": " << kIonLabels[i] << endl;
    for(i = eMSIonType_adot; i < eMSIonType_max; i++)
        cout << i << ": " << kIonLabels[i] << endl;
    
}


void COMSSABase::Init()
{

    auto_ptr<CArgDescriptions> argDesc(new CArgDescriptions);
    argDesc->PrintUsageIfNoArgs();

    argDesc->AddDefaultKey("pm", "param",
                           "search parameter input in xml format (overrides command line)",
                           CArgDescriptions::eString, "");

    argDesc->AddDefaultKey("d", "blastdb", "Blast sequence library to search. Do not include .p* filename suffixes.",
			   CArgDescriptions::eString, "nr");
    argDesc->AddFlag("umm", "use memory mapped sequence libraries");
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
    argDesc->AddDefaultKey("fbz2", "bz2infile", "omssa omx file compressed by bzip2",
                   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fxml", "omxinfile", "omssa xml search request file",
                   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("o", "textasnoutfile", "filename for text asn.1 formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("ob", "binaryasnoutfile", "filename for binary asn.1 formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("ox", "xmloutfile", "filename for xml formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("obz2", "bz2outfile", "filename for bzip2 compressed xml formatted search results",
                CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("op", "pepxmloutfile", "filename for pepXML formatted search results",
               CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("oc", "csvfile", "filename for csv formatted search summary",
                CArgDescriptions::eString, "");
    argDesc->AddFlag("w", "include spectra and search params in search results");
    argDesc->AddDefaultKey("to", "pretol", "product ion m/z tolerance in Da",
			   CArgDescriptions::eDouble, "0.8");
    argDesc->AddDefaultKey("te", "protol", "precursor ion m/z tolerance in Da (or ppm if -teppm flag set)",
			   CArgDescriptions::eDouble, "2.0");
    argDesc->AddDefaultKey("tom", "promass", "product ion search type (0 = mono, 1 = avg, 2 = N15, 3 = exact)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("tem", "premass", "precursor ion search type (0 = mono, 1 = avg, 2 = N15, 3 = exact, 4 = multiisotope)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("tez", "prozdep", "charge dependency of precursor mass tolerance (0 = none, 1 = linear)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("ti", "isotopes", "when doing multiisotope search, number of isotopic peaks to search.  0 = monoisotopic peak only",
                           CArgDescriptions::eInteger, "0");
    argDesc->AddFlag("teppm", "search precursor masses in units of ppm");
    argDesc->AddDefaultKey("ta", "autotol", 
                   "automatic mass tolerance adjustment fraction",
                   CArgDescriptions::eDouble, 
                   "1.0");
    argDesc->AddDefaultKey("tex", "exact", 
                    "threshold in Da above which the mass of neutron should be added in exact mass search",
                    CArgDescriptions::eDouble, 
                    "1446.94");
    argDesc->AddDefaultKey("i", "ions", 
               "id numbers of ions to search (comma delimited, no spaces)",
               CArgDescriptions::eString, 
               "1,4");
    argDesc->AddDefaultKey("cl", "cutlo", "low intensity cutoff as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.0");
    argDesc->AddDefaultKey("ch", "cuthi", "high intensity cutoff as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.2");
    argDesc->AddDefaultKey("ci", "cutinc", "intensity cutoff increment as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.0005");
    argDesc->AddDefaultKey("cp", "precursorcull", 
                "eliminate charge reduced precursors in spectra (0=no, 1=yes)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("v", "cleave", 
			   "number of missed cleavages allowed",
			   CArgDescriptions::eInteger, "1");
    argDesc->AddDefaultKey("x", "taxid", 
			   "comma delimited list of taxids to search (0 = all)",
			   CArgDescriptions::eString, "0");
    argDesc->AddDefaultKey("w1", "window1", 
			   "single charge window in Da",
			   CArgDescriptions::eInteger, "27");
    argDesc->AddDefaultKey("w2", "window2", 
			   "double charge window in Da",
			   CArgDescriptions::eInteger, "14");
    argDesc->AddDefaultKey("h1", "hit1", 
			   "number of peaks allowed in single charge window (0 = number of ion species)",
			   CArgDescriptions::eInteger, "2");
    argDesc->AddDefaultKey("h2", "hit2", 
			   "number of peaks allowed in double charge window (0 = number of ion species)",
			   CArgDescriptions::eInteger, "2");
    argDesc->AddDefaultKey("hl", "hitlist", 
			   "maximum number of hits retained per precursor charge state per spectrum during the search",
			   CArgDescriptions::eInteger, "30");
    argDesc->AddDefaultKey("hc", "hitcount", 
                           "maximum number of hits reported per spectrum (0 = all)",
                           CArgDescriptions::eInteger, "0");
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
    argDesc->AddFlag("mnm", "n-term methionine should not be cleaved");
    argDesc->AddDefaultKey("mm", "maxmod", 
			   "the maximum number of mass ladders to generate per database peptide",
			   CArgDescriptions::eInteger, /*NStr::IntToString(MAXMOD2)*/ "128");
    argDesc->AddDefaultKey("e", "enzyme", 
			   "id number of enzyme to use",
			   CArgDescriptions::eInteger, 
			   NStr::IntToString(eMSEnzymes_trypsin));
	argDesc->AddDefaultKey("zh", "maxcharge", 
				"maximum precursor charge to search when not 1+",
				CArgDescriptions::eInteger, 
				"3");
	argDesc->AddDefaultKey("zl", "mincharge", 
				"minimum precursor charge to search when not 1+",
				CArgDescriptions::eInteger, 
				"1");
    argDesc->AddDefaultKey("zoh", "maxprodcharge", 
                  "maximum product charge to search",
                  CArgDescriptions::eInteger, 
                  "2");
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
    argDesc->AddDefaultKey("zcc", "calccharge", 
                   "how should precursor charges be determined? (1=believe the input file, 2=use a range)",
                   CArgDescriptions::eInteger, 
                   "2");
    argDesc->AddDefaultKey("zn", "negions", 
                           "search using negative or positive ions (1=positive, -1=negative)",
                           CArgDescriptions::eInteger, 
                           "1");
    argDesc->AddDefaultKey("pc", "pseudocount", 
                  "minimum number of precursors that match a spectrum",
                  CArgDescriptions::eInteger, 
                  "1");
    argDesc->AddDefaultKey("sb1", "searchb1", 
                   "should first forward (b1) product ions be in search (1=no)",
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
    argDesc->AddDefaultKey("scorr", "corrscore", 
                     "turn off correlation correction to score (1=off, 0=use correlation)",
                     CArgDescriptions::eInteger, 
                     "0");
    argDesc->AddDefaultKey("scorp", "corrprob", 
                      "probability of consecutive ion (used in correlation correction)",
                      CArgDescriptions::eDouble, 
                      "0.5");
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
    argDesc->AddDefaultKey("p", "prolineruleions", 
                "id numbers of ion series to apply no product ions at proline rule at (comma delimited, no spaces)",
                CArgDescriptions::eString, 
                "");

    argDesc->AddFlag("il", "print a list of ions and their corresponding id number");
    argDesc->AddFlag("el", "print a list of enzymes and their corresponding id number");
    argDesc->AddFlag("ml", "print a list of modifications and their corresponding id number");

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


    AppInit(argDesc.get());

    SetupArgDescriptions(argDesc.release());

    // allow info posts to be seen
    SetDiagPostLevel(eDiag_Info);
}


void COMSSABase::AppInit(CArgDescriptions *argDesc)
{
}

void COMSSABase::SetOutFile(bool IncludeSpectra,
                        EMSSerialDataFormat FileFormat,
                        string FileName,
                        CRef<CMSSearchSettings> &Settings) 
{
    CRef <CMSOutFile> Outfile;
    Outfile = new CMSOutFile;

    Outfile->SetOutfile(FileName);
    Outfile->SetOutfiletype(FileFormat);

    if(IncludeSpectra) {
        Outfile->SetIncluderequest(true);
    }
    else {
        Outfile->SetIncluderequest(false);
    }

    Settings->SetOutfiles().push_back(Outfile);
}


void COMSSABase::SetSearchSettings(const CArgs& args, CRef<CMSSearchSettings> Settings)
{

    CRef <CMSInFile> Infile; 
    Infile = new CMSInFile;

    if (args["fx"].AsString().size() != 0) {
        Infile->SetInfile(args["fx"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_dtaxml);
    }
    else if (args["f"].AsString().size() != 0) {
        Infile->SetInfile(args["f"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_dta);
    }
    else if (args["fb"].AsString().size() != 0) {
        Infile->SetInfile(args["fb"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_dtablank);
    }
    else if (args["fp"].AsString().size() != 0) {
        Infile->SetInfile(args["fp"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_pkl);
    }
    else if (args["fm"].AsString().size() != 0) {
        Infile->SetInfile(args["fm"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_mgf);
    }
    else if (args["fomx"].AsString().size() != 0) {
        Infile->SetInfile(args["fomx"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_omx);
    }
    else if (args["fbz2"].AsString().size() != 0) {
         Infile->SetInfile(args["fbz2"].AsString());
         Infile->SetInfiletype(eMSSpectrumFileType_omxbz2);
     }
    else if (args["foms"].AsString().size() != 0) {
        Infile->SetInfile(args["foms"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_oms);
    }
    else if (args["fxml"].AsString().size() != 0) {
        Infile->SetInfile(args["fxml"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_xml);
    }
    else ERR_POST(Fatal << "no input file specified");

    Settings->SetInfiles().push_back(Infile);


    bool IncludeSpectra = args["w"];

    // set up output files
    if (args["o"].AsString() != "") {
        SetOutFile(IncludeSpectra, eMSSerialDataFormat_asntext, args["o"].AsString(), Settings);
    }
    if (args["ob"].AsString() != "") {
        SetOutFile(IncludeSpectra, eMSSerialDataFormat_asnbinary, args["ob"].AsString(), Settings);
    }
    if (args["ox"].AsString() != "") {
        SetOutFile(IncludeSpectra, eMSSerialDataFormat_xml, args["ox"].AsString(), Settings);
    }
    if (args["obz2"].AsString() != "") {
         SetOutFile(IncludeSpectra, eMSSerialDataFormat_xmlbz2, args["obz2"].AsString(), Settings);
     }
    if (args["op"].AsString() != "") {
        SetOutFile(IncludeSpectra, eMSSerialDataFormat_pepxml, args["op"].AsString(), Settings);
    }
    if (args["oc"].AsString() != "") {
        SetOutFile(IncludeSpectra, eMSSerialDataFormat_csv, args["oc"].AsString(), Settings);
    }

    // set up rest of settings

    Settings->SetPrecursorsearchtype(args["tem"].AsInteger());
    Settings->SetProductsearchtype(args["tom"].AsInteger());
    Settings->SetPeptol(args["te"].AsDouble());
    Settings->SetMsmstol(args["to"].AsDouble());
    Settings->SetZdep(args["tez"].AsInteger());
    Settings->SetExactmass(args["tex"].AsDouble());
    Settings->SetAutomassadjust(args["ta"].AsDouble());
    // new arguments set in spec only if turned on.  allows for backward compatibility with browser
    if(args["ti"].AsInteger() != 0)
        Settings->SetNumisotopes(args["ti"].AsInteger());
    if(args["teppm"]) 
        Settings->SetPepppm(true);

    InsertList(args["i"].AsString(), Settings->SetIonstosearch(), "unknown ion");
    Settings->SetCutlo(args["cl"].AsDouble());
    Settings->SetCuthi(args["ch"].AsDouble());
    Settings->SetCutinc(args["ci"].AsDouble());
    Settings->SetPrecursorcull(args["cp"].AsInteger());
    Settings->SetSinglewin(args["w1"].AsInteger());
    Settings->SetDoublewin(args["w2"].AsInteger());
    Settings->SetSinglenum(args["h1"].AsInteger());
    Settings->SetDoublenum(args["h2"].AsInteger());
    Settings->SetEnzyme(args["e"].AsInteger());
    Settings->SetMissedcleave(args["v"].AsInteger());
    InsertList(args["mv"].AsString(), Settings->SetVariable(), "unknown variable mod");
    InsertList(args["mf"].AsString(), Settings->SetFixed(), "unknown fixed mod");
    Settings->SetNmethionine(!args["mnm"]);
    Settings->SetDb(args["d"].AsString());
    Settings->SetHitlistlen(args["hl"].AsInteger());
    Settings->SetTophitnum(args["ht"].AsInteger());
    Settings->SetMinhit(args["hm"].AsInteger());
    Settings->SetMinspectra(args["hs"].AsInteger());
    Settings->SetScale(MSSCALE); // presently ignored
    Settings->SetCutoff(args["he"].AsDouble());
    if(args["hc"].AsInteger() != 0)
        Settings->SetReportedhitcount(args["hc"].AsInteger());
    Settings->SetMaxmods(args["mm"].AsInteger());
    Settings->SetPseudocount(args["pc"].AsInteger());
    Settings->SetSearchb1(args["sb1"].AsInteger());
    Settings->SetSearchctermproduct(args["sct"].AsInteger());
    Settings->SetMaxproductions(args["sp"].AsInteger());
    Settings->SetNocorrelationscore(args["scorr"].AsInteger());
    Settings->SetProbfollowingion(args["scorp"].AsDouble());

    Settings->SetMinnoenzyme(args["no"].AsInteger());
    Settings->SetMaxnoenzyme(args["nox"].AsInteger());
    if (args["x"].AsString() != "0") {
        InsertList(args["x"].AsString(), Settings->SetTaxids(), "unknown tax id");
    }

    Settings->SetChargehandling().SetCalcplusone(args["zc"].AsInteger());
    Settings->SetChargehandling().SetConsidermult(args["zt"].AsInteger());
    Settings->SetChargehandling().SetMincharge(args["zl"].AsInteger());
    Settings->SetChargehandling().SetMaxcharge(args["zh"].AsInteger());
    Settings->SetChargehandling().SetPlusone(args["z1"].AsDouble());
    Settings->SetChargehandling().SetMaxproductcharge(args["zoh"].AsInteger());
    Settings->SetChargehandling().SetCalccharge(args["zcc"].AsInteger());
    Settings->SetChargehandling().SetNegative(args["zn"].AsInteger());

    Settings->SetIterativesettings().SetResearchthresh(args["ii"].AsDouble());
    Settings->SetIterativesettings().SetSubsetthresh(args["is"].AsDouble());
    Settings->SetIterativesettings().SetReplacethresh(args["ir"].AsDouble());

    InsertList(args["p"].AsString(), Settings->SetNoprolineions(), "unknown ion for proline rule");

    return;
}


void COMSSABase::SetThreadCount(int NumThreads)
{
#if defined(_MT)
    SetnThreads() = NumThreads;
    if (GetnThreads() == 0) SetnThreads() = GetCpuCount();
    if (GetnThreads() > 1) {
      ERR_POST(Info << "Using " << GetnThreads() << " search threads");
    }
#else
    SetnThreads() = 1;
#endif
}

void COMSSABase::RunSearch(CRef <CSearch> SearchEngine)
{
    for (int i=1; i < GetnThreads(); i++) { 
       CRef <CSearch> SearchThread (new CSearch(i));
       SearchThread->CopySettings(SearchEngine);
       SetsearchThreads().push_back(SearchThread);
    }
    
    _TRACE("omssa: search begin");
    for (int i=0; i < GetnThreads(); i++) { 
       SetsearchThreads()[i]->Run(CThread::fRunAllowST);
    }

    bool* result;
    for (int i=0; i < GetnThreads(); i++) {
        SetsearchThreads()[i]->Join(reinterpret_cast<void**>(&result));
        //cout << "Returned value: " << *result << endl;
    }
    SetsearchThreads()[0]->SetResult(SetsearchThreads()[0]->SharedPeakSet);
}


