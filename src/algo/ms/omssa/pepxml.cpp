/*
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
 * Author:  Douglas J. Slotta
 *
 * File Description:
 *   Code for converting OMSSA to PepXML
 *
 */

// standard includes
#include <ncbi_pch.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <util/xregexp/regexp.hpp>

#include "pepxml.hpp"
#include "omssa.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)

const double PROTON_MASS = 1.007276466;
const double HYDROGEN_MASS = 1.00794;
const double OH_MASS = 17.00734;

// char const * const kMolNames[5] = {
//     "not set",
//     "DNA",
//     "RNA",
//     "AA",
//     "NA"
// };

string CPepXML::ConvertDouble(double n) {
    string val = NStr::DoubleToString(n,15);
    int len = val.length();
    while (NStr::EndsWith(val,"0")) {
        val.erase(--len);
    }
    if (NStr::EndsWith(val,".")) {
        val.append("0");
    }
    return val;
}


char CPepXML::ConvertAA(char in) {
    string out;
    CSeqConvert::Convert(&in, CSeqUtil::e_Ncbistdaa, 0, 1, out, CSeqUtil::e_Ncbieaa);
    return out[0];
}

typedef pair<int, string> TAAModPair;
typedef map<int, string> TAAModMap;

CRef<CModification_info> CPepXML::ConvertModifications(CRef<CMSHits> msHits, CRef<CMSModSpecSet> Modset, set<int>& vModSet, CMSSearch& inOMSSA) {

    CRef<CModification_info> modInfo(new CModification_info); // modification_info is parent mod element.  attributes include
                                                              // modified_peptide and mod_[nc]term_mass

    TAAModMap modMap;
    string pep = msHits->GetPepstring();
    bool hasMod(false);

    ITERATE(CMSHits::TMods, iMod, msHits->GetMods()) {  // iterate through list of modifications
        int pos = (*iMod)->GetSite();
        int num = (*iMod)->GetModtype();  // poorly named in OMSSA, is actually MSMod, not MSModType
        vModSet.insert(num);
        EMSModType type = Modset->GetModType(num);  // aa specific, nterminal, etc.
        double mdiff = MSSCALE2DBL(Modset->GetModMass(num));
        char aa = pep[pos];
        double aaMass = m_aaMassMap.find(aa)->second;
        double mass = aaMass + mdiff;
        string iMass = "[" + NStr::IntToString(static_cast<int>(mass)) + "]";
        
        modMap.insert(TAAModPair(pos,iMass));

        CRef<CMod_aminoacid_mass> modaaMass(new CMod_aminoacid_mass);  // child tag of modInfo used for aa specific mods

        switch (type) {
        case eMSModType_modaa:
            modaaMass->SetAttlist().SetPosition(pos+1); // fill out subelement mod_aminoacid_mass
            modaaMass->SetAttlist().SetMass(mass);
            modInfo->SetMod_aminoacid_mass().push_back(modaaMass);
            hasMod = true;
            break;
        case eMSModType_modn:
        case eMSModType_modnaa:
        case eMSModType_modnp:
        case eMSModType_modnpaa:
            modInfo->SetAttlist().SetMod_nterm_mass(mass);
            hasMod = true;
            break;
        case eMSModType_modc:
        case eMSModType_modcaa:
        case eMSModType_modcp:
        case eMSModType_modcpaa:
            modInfo->SetAttlist().SetMod_cterm_mass(mass);
            hasMod = true;
            break;
        default:
            // perhaps some error handling here
            break;
        }
        
    }
    
    // iterate through peptide looking for aa specific mods.  If found, insert mass into peptide string
    
    string modPep;
    
    for (unsigned int i=0; i<pep.length(); i++) {
        char p = pep[i];
        modPep.append(1, p);
        TAAModMap::iterator it;
        it = modMap.find(i);
        if (it != modMap.end()) {
            modPep.append(it->second);           // see if AA has corresponding mod, if so, append mass text to peptide string
        } else if (m_staticModSet.count(p)>0) {  // else if there is a static modification associated with the AA, then
                                                 // create a mod_aminoacid_mass subelement 
            CRef<CMod_aminoacid_mass> modaaMass(new CMod_aminoacid_mass);
            modaaMass->SetAttlist().SetPosition(i+1);
            double staticMass = m_aaMassMap.find(p)->second;
            modaaMass->SetAttlist().SetMass(staticMass);
            modInfo->SetMod_aminoacid_mass().push_back(modaaMass);
            hasMod = true;
        }
    }

//  todo: does not return n or c term peptide or protein fixed mods.
//  to do this, iterate through the mod set for the search and print them out.
    
    CMSSearchSettings::TFixed::const_iterator iterF;
    for (iterF = inOMSSA.GetRequest().front()->GetSettings().GetFixed().begin();
         iterF != inOMSSA.GetRequest().front()->GetSettings().GetFixed().end(); ++iterF) {
        int type = Modset->GetModType(*iterF);
        double mass = MSSCALE2DBL(Modset->GetModMass(*iterF));
        if (type % 2 != 0) {
            switch (type) {
                case eMSModType_modn:
                case eMSModType_modnaa:
                case eMSModType_modnp:
                case eMSModType_modnpaa:
                    modInfo->SetAttlist().SetMod_nterm_mass(mass + HYDROGEN_MASS);
                    hasMod = true;
                    break;
                case eMSModType_modc:
                case eMSModType_modcaa:
                case eMSModType_modcp:
                case eMSModType_modcpaa:
                    modInfo->SetAttlist().SetMod_cterm_mass(mass + OH_MASS);
                    hasMod = true;
                    break;
            }
        }
    }
    
//  only return if we have declared a mod    
    if(!hasMod) return null;
    modInfo->SetAttlist().SetModified_peptide(modPep);    

    return modInfo;
}


void CPepXML::ConvertModSetting(CRef<CSearch_summary> sSum, CRef<CMSModSpecSet> Modset, int modnum, bool fixed) {
    // NB: pepXML does not seem to allow for modification to the terminus of a protein at particular amino acids
    int type = Modset->GetModType(modnum);
    if ( type % 2 == 0) {  // Must apply to a paticular amino acid
        for (int i=0; i< Modset->GetModNumChars(modnum); i++) {
            CRef<CAminoacid_modification> aaMod(new CAminoacid_modification);
            int modchar = Modset->GetModChar(modnum, i);
            char aa = ConvertAA(modchar);
            string aaStr(1, aa);
            aaMod->SetAttlist().SetAminoacid(aaStr);
            double mdiff = MSSCALE2DBL(Modset->GetModMass(modnum));
            double aaMass = m_aaMassMap.find(aa)->second;
            double mass = aaMass + mdiff;
            aaMod->SetAttlist().SetMassdiff(ConvertDouble(mdiff));
            aaMod->SetAttlist().SetMass(mass);
            if (fixed) {
                aaMod->SetAttlist().SetVariable("N");
                m_aaMassMap.erase(aa);
                m_aaMassMap.insert(TAminoAcidMassPair(aa, aaMass + mdiff));
                m_staticModSet.insert(aa);
            } else {
                aaMod->SetAttlist().SetVariable("Y");
            }
            if (type > 0) {
                if (type == eMSModType_modnpaa) aaMod->SetAttlist().SetPeptide_terminus("N");
                if (type == eMSModType_modcpaa) aaMod->SetAttlist().SetPeptide_terminus("C");
            }
            aaMod->SetAttlist().SetDescription(Modset->GetUnimodName(modnum));
            sSum->SetAminoacid_modification().push_back(aaMod);
        }
    } else {
        CRef<CTerminal_modification> termMod(new CTerminal_modification);
        double mass = MSSCALE2DBL(Modset->GetModMass(modnum));
        termMod->SetAttlist().SetMassdiff(ConvertDouble(mass));
        if (fixed) {
            termMod->SetAttlist().SetVariable("N");
        } else {
            termMod->SetAttlist().SetVariable("Y");
        }
        termMod->SetAttlist().SetDescription(Modset->GetUnimodName(modnum));
        switch (type) {
        case eMSModType_modn:
            termMod->SetAttlist().SetTerminus("n");
            termMod->SetAttlist().SetProtein_terminus("Y");
            termMod->SetAttlist().SetMass(mass + HYDROGEN_MASS);
            break;
        case eMSModType_modnp:
            termMod->SetAttlist().SetTerminus("n");
            termMod->SetAttlist().SetProtein_terminus("N");
            termMod->SetAttlist().SetMass(mass + HYDROGEN_MASS);
            break;
        case eMSModType_modc:
            termMod->SetAttlist().SetTerminus("c");
            termMod->SetAttlist().SetProtein_terminus("Y");
            termMod->SetAttlist().SetMass(mass + OH_MASS);
            break;
        case eMSModType_modcp:
            termMod->SetAttlist().SetTerminus("c");
            termMod->SetAttlist().SetProtein_terminus("N");
            termMod->SetAttlist().SetMass(mass + OH_MASS);
            break;
        }
        sSum->SetTerminal_modification().push_back(termMod);
    }
}

// Parses a spectrum identifier string
// SpecID: the string to parse
// field:  0 = whole string
//         1 = dta file name
//         2 = start scan
//         3 = end scan
//         4 = charge state
//         5 = file extension (.dta)
// query:  string to return if SpecID is not a dta filename
void CPepXML::ConvertScanID(CRef<CSpectrum_query> sQuery, string SpecID, int query, int charge) {
    string specFile, startScan, stopScan, dtaCharge;

    dtaCharge = NStr::IntToString(charge);
    
	CRegexp RxpLocus("^locus", CRegexp::fCompile_ignore_case | CRegexp::fCompile_newline );
	if (RxpLocus.IsMatch(SpecID)) {
        specFile = SpecID;
        startScan = NStr::IntToString(query);
        stopScan = startScan;
    } else {
        CRegexp RxpParse("(.*)\\.(\\d+)\\.(\\d+)\\.(\\d+)(\\.dta)?", CRegexp::fCompile_ignore_case);
        specFile = RxpParse.GetMatch(SpecID, 0, 1);
        if (specFile == "") {
            specFile = SpecID;
        }
        startScan= RxpParse.GetMatch(SpecID, 0, 2);
        if (startScan == "") {
            startScan = NStr::IntToString(query);
            stopScan = startScan;
        } else {
            stopScan = RxpParse.GetMatch(SpecID, 0, 3);
            if (stopScan == "") {
                stopScan = startScan;
            }
        }
    }
    
    sQuery->SetAttlist().SetSpectrum(specFile + "." + startScan + "." + stopScan + "." + dtaCharge);
    sQuery->SetAttlist().SetStart_scan(NStr::StringToInt(startScan));
    sQuery->SetAttlist().SetEnd_scan(NStr::StringToInt(stopScan));
}

string CPepXML::GetProteinName(CRef<CMSPepHit> pHit) {
    if (pHit->CanGetAccession()) {
        return pHit->GetAccession();
    } else if (pHit->CanGetGi()) {
        return "gi:" + NStr::IntToString(pHit->GetGi());
    }
    return pHit->GetDefline();
}


void CPepXML::ConvertMSHitSet(CRef<CMSHitSet> pHitSet, 
                              CMsms_run_summary::TSpectrum_query& sQueries, 
                              CRef<CMSModSpecSet> Modset, 
                              set<int>& variableMods,
                              CMSSearch& inOMSSA)
{
    if (pHitSet->GetHits().empty())
        return;

    CMSHitSet::THits::const_iterator iHit;
    set<int> charges;
    
    // First, find all possible charge states
    for(iHit = pHitSet->GetHits().begin(); iHit != pHitSet->GetHits().end(); iHit++) {
        charges.insert((*iHit)->GetCharge());
    }


    ITERATE(set<int>, iCharge, charges) {
        iHit = pHitSet->GetHits().begin();
        int charge = (*iHit)->GetCharge();

        // advance to the first instance with a matching charge
        while ( charge != *iCharge ) {
            iHit++;
            charge = (*iHit)->GetCharge();
        }
        
        CRef<CSpectrum_query> sQuery(new CSpectrum_query);
        string spectrumID;
        if(!(pHitSet->GetIds().empty())) {
            spectrumID = *(pHitSet->GetIds().begin());
        }
        //string query = NStr::IntToString(pHitSet->GetNumber());
        
        ConvertScanID(sQuery, spectrumID, pHitSet->GetNumber(), charge);
        
        //double neutral_precursor_mass = ((*iHit)->GetMass()/m_scale)/charge - (charge * PROTON_MASS);
        double neutral_precursor_mass = (*iHit)->GetMass()/m_scale;
        sQuery->SetAttlist().SetPrecursor_neutral_mass(neutral_precursor_mass);
        sQuery->SetAttlist().SetAssumed_charge(charge);
        sQuery->SetAttlist().SetIndex(m_index++);
        
        // Only one search_result per query (for now)
        CRef<CSearch_result> sResult(new CSearch_result);
        
        CMSHits::TPephits::const_iterator iPephit;
        int hitRank = 1;
        //double prevEValue = (*iHit)->GetEvalue();
        for( ; iHit != pHitSet->GetHits().end(); iHit++) {
            // skip this hit if it is not the right charge
            charge = (*iHit)->GetCharge();
            if ( charge != *iCharge ) {
                continue;  
            }
            
            // First protein is associated with search_hit, the rest go into alternative_proteins
            iPephit = (*iHit)->GetPephits().begin();
            // Each set of MSHits is a search_hit
            CRef<CSearch_hit> sHit(new CSearch_hit);
            //if (prevEValue < (*iHit)->GetEvalue()) hitRank++; // This sets those hits with the same score to have the same rank 
            sHit->SetAttlist().SetHit_rank(hitRank);
            hitRank++; // Arbitrarily advances the rank, ever if the scores are the same
            sHit->SetAttlist().SetPeptide((*iHit)->GetPepstring());
            if((*iHit)->CanGetPepstart())
                sHit->SetAttlist().SetPeptide_prev_aa((*iHit)->GetPepstart());
            if((*iHit)->CanGetPepstop())
                sHit->SetAttlist().SetPeptide_next_aa((*iHit)->GetPepstop());
            
            sHit->SetAttlist().SetProtein(GetProteinName(*iPephit));
        
            sHit->SetAttlist().SetNum_tot_proteins((*iHit)->GetPephits().size());
            sHit->SetAttlist().SetNum_matched_ions((*iHit)->GetMzhits().size());
            int tot_num_ions = ((*iHit)->GetPepstring().length()-1) * 2;
            sHit->SetAttlist().SetTot_num_ions(tot_num_ions);
            sHit->SetAttlist().SetCalc_neutral_pep_mass((*iHit)->GetTheomass()/m_scale);
            sHit->SetAttlist().SetMassdiff(ConvertDouble(neutral_precursor_mass - ((*iHit)->GetTheomass())/m_scale));
            //sHit->SetSearch_hit().SetAttlist().SetNum_tol_term("42"); //skip
            //sHit->SetSearch_hit().SetAttlist().SetNum_missed_cleavages("42"); //skip
            sHit->SetAttlist().SetIs_rejected(CSearch_hit::C_Attlist::eAttlist_is_rejected_0);
            sHit->SetAttlist().SetProtein_descr((*iPephit)->GetDefline());
            //sHit->SetSearch_hit().SetAttlist().SetCalc_pI("42"); //skip
            //sHit->SetSearch_hit().SetAttlist().SetProtein_mw("42"); //skip
            CRef<CSearch_score> pValue(new CSearch_score);
            pValue->SetAttlist().SetName("pvalue");
            pValue->SetAttlist().SetValue(ConvertDouble((*iHit)->GetPvalue()));
            CRef<CSearch_score> eValue(new CSearch_score);
            eValue->SetAttlist().SetName("expect");
            eValue->SetAttlist().SetValue(ConvertDouble((*iHit)->GetEvalue()));
            sHit->SetSearch_score().push_back(pValue);
            sHit->SetSearch_score().push_back(eValue);
            if ((*iHit)->CanGetScores()) {
                ITERATE(CMSHits::TScores, iScore, (*iHit)->GetScores()) {
                    CRef<CSearch_score> score(new CSearch_score);
                    score->SetAttlist().SetName((*iScore)->GetName());
                    score->SetAttlist().SetValue(ConvertDouble((*iScore)->GetValue()));
                    sHit->SetSearch_score().push_back(score);
                }
            }
            // Generate alternative_proteins
            for (iPephit++ ; iPephit != (*iHit)->GetPephits().end(); iPephit++) {
                CRef<CAlternative_protein> altPro(new CAlternative_protein);
                altPro->SetAttlist().SetProtein(GetProteinName(*iPephit));
                altPro->SetAttlist().SetProtein_descr((*iPephit)->GetDefline());
                //altPro->SetAlternative_protein().SetAttlist().SetNum_tol_term(); //skip
                //altPro->SetAlternative_protein().SetAttlist().SetProtein_mw();  //skip
                sHit->SetAlternative_protein().push_back(altPro);
            }
            CRef<CModification_info> modInfo = ConvertModifications(*iHit, Modset, variableMods, inOMSSA);
            if (modInfo) sHit->SetModification_info(*modInfo);
        
            sResult->SetSearch_hit().push_back(sHit);
        }
        sQuery->SetSearch_result().push_back(sResult);
        sQueries.push_back(sQuery);
    }
}


void CPepXML::ConvertFromOMSSA(CMSSearch& inOMSSA, CRef <CMSModSpecSet> Modset, string basename, string newname) {

    m_scale = static_cast<float>(inOMSSA.GetRequest().front()->GetSettings().GetScale());

    // set up m_aaMassMap for modifications
    for (int modchar=0; modchar < 29; modchar++) {
        char aa = ConvertAA(modchar);
        double aaMass = MonoMass[modchar];
        m_aaMassMap.insert(TAminoAcidMassPair(aa, aaMass));
    }


    CTime datetime(CTime::eCurrent);
    datetime.SetFormat("Y-M-DTh:m:s");
    this->SetAttlist().SetDate(datetime.AsString());
    this->SetAttlist().SetSummary_xml(newname);

    // Create the Run Summary (need to generalize)
    CRef<CMsms_run_summary> rSum(new CMsms_run_summary);
    rSum->SetAttlist().SetBase_name(basename);
    rSum->SetAttlist().SetRaw_data_type("raw");
    rSum->SetAttlist().SetRaw_data(".mzXML");
    EMSEnzymes enzyme = static_cast <EMSEnzymes>(inOMSSA.GetRequest().front()->GetSettings().GetEnzyme());
    string enzymeName = kEnzymeNames[enzyme];
    CRef<CCleave> cleave = CCleaveFactory::CleaveFactory(enzyme);
    rSum->SetSample_enzyme().SetAttlist().SetName(enzymeName);

    CRef<CSpecificity> specificity(new CSpecificity);
    specificity->SetAttlist().SetCut(cleave->GetCleaveAt());
    switch (cleave->GetCleaveSense()[0]) {
    case 'c':
    case 'C':
        specificity->SetAttlist().SetSense(CSpecificity::C_Attlist::eAttlist_sense_C);
        break;
    case 'n':
    case 'N':
        specificity->SetAttlist().SetSense(CSpecificity::C_Attlist::eAttlist_sense_N);
        break;
    default:
        // Should be some sort of error here
        cerr << "Hmm, a cleavage with no sense, how odd." << endl;
    }
    if (cleave->GetCheckProline()) {
        specificity->SetAttlist().SetNo_cut("P");
    }
    rSum->SetSample_enzyme().SetSpecificity().push_back(specificity);

    // Create the Search Summary
    CRef<CSearch_summary> sSum(new CSearch_summary);
    //sSum->SetAttlist().SetBase_name(baseFile.GetName());
    sSum->SetAttlist().SetBase_name(basename);
    sSum->SetAttlist().SetSearch_engine("OMSSA");
    sSum->SetAttlist().SetOut_data_type("n/a");
    sSum->SetAttlist().SetOut_data("n/a");

    EMSSearchType searchType = static_cast <EMSSearchType>(inOMSSA.GetRequest().front()->GetSettings().GetPrecursorsearchtype());
    //string searchTypeName = kSearchType[searchType];
    switch (searchType) {
    case eMSSearchType_average:
        sSum->SetAttlist().SetPrecursor_mass_type(CSearch_summary::C_Attlist::eAttlist_precursor_mass_type_average);
        break;
    case eMSSearchType_monoisotopic:
    case eMSSearchType_monon15:
    case eMSSearchType_exact:
    case eMSSearchType_multiisotope:
        sSum->SetAttlist().SetPrecursor_mass_type(CSearch_summary::C_Attlist::eAttlist_precursor_mass_type_monoisotopic);
        break;
    default:
        // Should be some sort of error here
        cerr << "Hmm, a typeless search, how odd." << endl;
        sSum->SetAttlist().SetPrecursor_mass_type(CSearch_summary::C_Attlist::eAttlist_precursor_mass_type_monoisotopic);
    }


    searchType = static_cast <EMSSearchType>(inOMSSA.GetRequest().front()->GetSettings().GetProductsearchtype());
    //searchTypeName = kSearchType[searchType];
    switch (searchType) {
    case eMSSearchType_average:
        sSum->SetAttlist().SetFragment_mass_type(CSearch_summary::C_Attlist::eAttlist_fragment_mass_type_average);
        break;
    case eMSSearchType_monoisotopic:
    case eMSSearchType_monon15:
    case eMSSearchType_exact:
    case eMSSearchType_multiisotope:
        sSum->SetAttlist().SetFragment_mass_type(CSearch_summary::C_Attlist::eAttlist_fragment_mass_type_monoisotopic);
        break;
    default:
        // Should be some sort of error here
        cerr << "Hmm, a typeless search, how odd." << endl;
        sSum->SetAttlist().SetFragment_mass_type(CSearch_summary::C_Attlist::eAttlist_fragment_mass_type_monoisotopic);
    }
    //sSum->SetAttlist().SetFragment_mass_type(searchTypeName);
    sSum->SetAttlist().SetSearch_id(1); // Should be count based upon search number

    string dbname = inOMSSA.GetRequest().front()->GetSettings().GetDb();
    sSum->SetSearch_database().SetAttlist().SetLocal_path(dbname);

    int dbtype(3);
    if(inOMSSA.GetResponse().front()->IsSetBioseqs() && inOMSSA.GetResponse().front()->GetBioseqs().Get().size() > 0)
        dbtype = inOMSSA.GetResponse().front()->GetBioseqs().Get().front()->GetSeq().GetInst().GetMol();
    switch (dbtype) {
    case 3:
        sSum->SetSearch_database().SetAttlist().SetType(CSearch_database::C_Attlist::eAttlist_type_AA);
        break;
    default:
        sSum->SetSearch_database().SetAttlist().SetType(CSearch_database::C_Attlist::eAttlist_type_NA);
    }
    
    sSum->SetSearch_database().SetAttlist().SetSize_in_db_entries(inOMSSA.GetResponse().front()->GetDbversion());

    sSum->SetEnzymatic_search_constraint().SetAttlist().SetEnzyme(enzymeName);
    sSum->SetEnzymatic_search_constraint().SetAttlist().SetMax_num_internal_cleavages(inOMSSA.GetRequest().front()->GetSettings().GetMissedcleave()); //check this
    sSum->SetEnzymatic_search_constraint().SetAttlist().SetMin_number_termini(cleave->GetCleaveNum()); //check this

    // Fixed mods
    CMSSearchSettings::TFixed::const_iterator iterF;
    for (iterF = inOMSSA.GetRequest().front()->GetSettings().GetFixed().begin();
         iterF != inOMSSA.GetRequest().front()->GetSettings().GetFixed().end(); ++iterF) {
        ConvertModSetting(sSum, Modset, *iterF, true);
    }

    // Variable mods
    // Delay processing until all hits are examined, in case the spectral library search 
    // adds extra mods not seen here.
    set<int> variableMods;
    CMSSearchSettings::TVariable::const_iterator iterV;
    for (iterV = inOMSSA.GetRequest().front()->GetSettings().GetVariable().begin();
         iterV != inOMSSA.GetRequest().front()->GetSettings().GetVariable().end(); ++iterV) {
        //ConvertModSetting(sSum, Modset, *iterV, false);
        variableMods.insert(*iterV);
    }
    
    // Now for the Spectrum Queries
    CMSResponse::THitsets::const_iterator iHits;
    m_index = 1;
    for (iHits = inOMSSA.GetResponse().front()->GetHitsets().begin(); 
         iHits != inOMSSA.GetResponse().front()->GetHitsets().end(); iHits++) {
        //CRef< CMSHitSet > HitSet =  *iHits;
        ConvertMSHitSet(*iHits, rSum->SetSpectrum_query(), Modset, variableMods, inOMSSA);
    }
    
    ITERATE(set<int>, iVMod, variableMods) {
        ConvertModSetting(sSum, Modset, *iVMod, false);
    }

    rSum->SetSearch_summary().push_back(sSum);
    this->SetMsms_run_summary().push_back(rSum);

}

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE
