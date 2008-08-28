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
#include <util/regexp.hpp>

#include "pepxml.hpp"
#include "omssa.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)

char const * const kMolNames[5] = {
    "not set",
    "DNA",
    "RNA",
    "AA",
    "NA"
};

char CPepXML::ConvertAA(char in) {
    string out;
    CSeqConvert::Convert(&in, CSeqUtil::e_Ncbistdaa, 0, 1, out, CSeqUtil::e_Ncbieaa);
    return out[0];
}

typedef pair<int, string> TAAModPair;
typedef map<int, string> TAAModMap;

CRef<CModification_info> CPepXML::ConvertModifications(CRef<CMSHits> msHits, CRef<CMSModSpecSet> Modset, set<int>& vModSet) {
    //if (msHits->GetMods().empty()) {
    //    return null;
    //}
    CRef<CModification_info> modInfo(new CModification_info);

    TAAModMap modMap;
    string pep = msHits->GetPepstring();

    ITERATE(CMSHits::TMods, iMod, msHits->GetMods()) {
        int pos = (*iMod)->GetSite();
        int num = (*iMod)->GetModtype();  // poorly named in OMSSA, is actually MSMod, not MSModType
        vModSet.insert(num);
        EMSModType type = Modset->GetModType(num);
        double mdiff = MSSCALE2DBL(Modset->GetModMass(num));
        char aa = pep[pos];
        double aaMass = m_aaMassMap.find(aa)->second;
        double mass = aaMass + mdiff;
        string sMass = NStr::DoubleToString(mass, 6);
        string iMass = "[" + NStr::IntToString(static_cast<int>(mass)) + "]";
        
        modMap.insert(TAAModPair(pos,iMass));

        CRef<CMod_aminoacid_mass> modaaMass(new CMod_aminoacid_mass);

        switch (type) {
        case eMSModType_modaa:
            modaaMass->SetAttlist().SetPosition(NStr::IntToString(pos+1));
            modaaMass->SetAttlist().SetMass(sMass);
            modInfo->SetMod_aminoacid_mass().push_back(modaaMass);
            break;
        case eMSModType_modn:
        case eMSModType_modnaa:
        case eMSModType_modnp:
        case eMSModType_modnpaa:
            modInfo->SetAttlist().SetMod_nterm_mass(sMass);
            break;
        case eMSModType_modc:
        case eMSModType_modcaa:
        case eMSModType_modcp:
        case eMSModType_modcpaa:
            modInfo->SetAttlist().SetMod_cterm_mass(sMass);
            break;
        default:
            // perhaps some error handling here
            break;
        }
        
    }
    string modPep;
    
    for (unsigned int i=0; i<pep.length(); i++) {
        char p = pep[i];
        modPep.append(1, p);
        TAAModMap::iterator it;
        it = modMap.find(i);
        if (it != modMap.end()) {
            modPep.append(it->second);
        } else if (m_staticModSet.count(p)>0) {
            CRef<CMod_aminoacid_mass> modaaMass(new CMod_aminoacid_mass);
            modaaMass->SetAttlist().SetPosition(NStr::IntToString(i+1));
            string staticMass = NStr::DoubleToString(m_aaMassMap.find(p)->second,6);
            modaaMass->SetAttlist().SetMass(staticMass);
            modInfo->SetMod_aminoacid_mass().push_back(modaaMass);
        }
    }
    if (modInfo->GetMod_aminoacid_mass().empty()) return null;

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
            string mass = NStr::DoubleToString(aaMass + mdiff, 6);
            aaMod->SetAttlist().SetMassdiff(NStr::DoubleToString(mdiff, 6));
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
        double mdiff = MSSCALE2DBL(Modset->GetModMass(modnum));
        string mass = NStr::DoubleToString(mdiff);
        termMod->SetAttlist().SetMassdiff(mass);
        termMod->SetAttlist().SetMass(mass);
        if (fixed) {
            termMod->SetAttlist().SetVariable("N");
        } else {
            termMod->SetAttlist().SetVariable("Y");
        }
        termMod->SetAttlist().SetDescription(Modset->GetUnimodName(modnum));
        switch (type) {
        case eMSModType_modn:
            termMod->SetAttlist().SetTerminus("N");
            termMod->SetAttlist().SetProtein_terminus("Y");
            break;
        case eMSModType_modnp:
            termMod->SetAttlist().SetTerminus("N");
            termMod->SetAttlist().SetProtein_terminus("N");
            break;
        case eMSModType_modc:
            termMod->SetAttlist().SetTerminus("C");
            termMod->SetAttlist().SetProtein_terminus("Y");
            break;
        case eMSModType_modcp:
            termMod->SetAttlist().SetTerminus("C");
            termMod->SetAttlist().SetProtein_terminus("N");
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
    sQuery->SetAttlist().SetStart_scan(startScan);
    sQuery->SetAttlist().SetEnd_scan(stopScan);
}

string CPepXML::GetProteinName(CRef<CMSPepHit> pHit) {
    if (pHit->CanGetAccession()) {
        return pHit->GetAccession();
    } else if (pHit->CanGetGi()) {
        return "gi:" + NStr::IntToString(pHit->GetGi());
    }
    return pHit->GetDefline();
}


void CPepXML::ConvertFromOMSSA(CMSSearch& inOMSSA, CRef <CMSModSpecSet> Modset, string basename, string newname) {

    float scale = static_cast<float>(inOMSSA.GetRequest().front()->GetSettings().GetScale());

    CTime datetime(CTime::eCurrent);

    // set up m_aaMassMap for modifications
    for (int modchar=0; modchar < 29; modchar++) {
        char aa = ConvertAA(modchar);
        double aaMass = MonoMass[modchar];
        m_aaMassMap.insert(TAminoAcidMassPair(aa, aaMass));
    }

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
    specificity->SetAttlist().SetSense(cleave->GetCleaveSense()); 
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
    string searchTypeName = kSearchType[searchType];
    sSum->SetAttlist().SetPrecursor_mass_type(searchTypeName);

    searchType = static_cast <EMSSearchType>(inOMSSA.GetRequest().front()->GetSettings().GetProductsearchtype());
    searchTypeName = kSearchType[searchType];
    sSum->SetAttlist().SetFragment_mass_type(searchTypeName);
    //sSum->SetSearch_summary().SetAttlist().SetOut_data_type("out");
    //sSum->SetSearch_summary().SetAttlist().SetOut_data("tgz");
    sSum->SetAttlist().SetSearch_id("1"); // Should be count based upon search number

    string dbname = inOMSSA.GetRequest().front()->GetSettings().GetDb();
    string dbsize = NStr::IntToString(inOMSSA.GetResponse().front()->GetDbversion());
    string dbtype = kMolNames[inOMSSA.GetResponse().front()->GetBioseqs().Get().front()->GetSeq().GetInst().GetMol()];
    sSum->SetSearch_database().SetAttlist().SetLocal_path(dbname);
    sSum->SetSearch_database().SetAttlist().SetType(dbtype);
    sSum->SetSearch_database().SetAttlist().SetSize_in_db_entries(dbsize);

    sSum->SetEnzymatic_search_constraint().SetAttlist().SetEnzyme(enzymeName);
    sSum->SetEnzymatic_search_constraint().SetAttlist().SetMax_num_internal_cleavages(NStr::IntToString(inOMSSA.GetRequest().front()->GetSettings().GetMissedcleave())); //check this
    sSum->SetEnzymatic_search_constraint().SetAttlist().SetMin_number_termini(NStr::IntToString(cleave->GetCleaveNum())); //check this

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
    int index = 1;
    for (iHits = inOMSSA.GetResponse().front()->GetHitsets().begin(); 
         iHits != inOMSSA.GetResponse().front()->GetHitsets().end(); iHits++) {
        CRef< CMSHitSet > HitSet =  *iHits;
        if (!HitSet->GetHits().empty()) {
            CMSHitSet::THits::const_iterator iHit;
            iHit = HitSet->GetHits().begin();
            CRef<CSpectrum_query> sQuery(new CSpectrum_query);
            string spectrumID;
            if(!(HitSet->GetIds().empty())) {
                spectrumID = *(HitSet->GetIds().begin());
            }
            //string query = NStr::IntToString(HitSet->GetNumber());

            int charge = (*iHit)->GetCharge();
            ConvertScanID(sQuery, spectrumID, HitSet->GetNumber(), charge);
            //sQuery->SetAttlist().SetSpectrum(spectrumID);
            //sQuery->SetAttlist().SetStart_scan(ParseScan(spectrumID, 1, query));
            //sQuery->SetAttlist().SetEnd_scan(ParseScan(spectrumID, 2, query));

            double neutral_precursor_mass = ((*iHit)->GetMass()/scale)/charge;
            sQuery->SetAttlist().SetPrecursor_neutral_mass(NStr::DoubleToString(neutral_precursor_mass));
            sQuery->SetAttlist().SetAssumed_charge(NStr::IntToString(charge));
            sQuery->SetAttlist().SetIndex(NStr::IntToString(index++));

            // Only one search_result per query (for now)
            CRef<CSearch_result> sResult(new CSearch_result);

            CMSHits::TPephits::const_iterator iPephit;
            int hitRank = 1;
            //double prevEValue = (*iHit)->GetEvalue();
            for( ; iHit != HitSet->GetHits().end(); iHit++) {
                // First protein is associated with search_hit, the rest go into alternative_proteins
                iPephit = (*iHit)->GetPephits().begin();
                // Each set of MSHits is a search_hit
                CRef<CSearch_hit> sHit(new CSearch_hit);
                //if (prevEValue < (*iHit)->GetEvalue()) hitRank++; // This sets those hits with the same score to have the same rank 
                sHit->SetAttlist().SetHit_rank(NStr::IntToString(hitRank));
                hitRank++; // Arbitrarily advances the rank, ever if the scores are the same
                sHit->SetAttlist().SetPeptide((*iHit)->GetPepstring());
                if((*iHit)->CanGetPepstart())
                    sHit->SetAttlist().SetPeptide_prev_aa((*iHit)->GetPepstart());
                if((*iHit)->CanGetPepstop())
                    sHit->SetAttlist().SetPeptide_next_aa((*iHit)->GetPepstop());

                sHit->SetAttlist().SetProtein(GetProteinName(*iPephit));

                sHit->SetAttlist().SetNum_tot_proteins(NStr::IntToString((*iHit)->GetPephits().size()));
                sHit->SetAttlist().SetNum_matched_ions(NStr::IntToString((*iHit)->GetMzhits().size()));
                // skip calculating the theoretical number of ions for now
                //int tot_num_ions = (*iHit)->GetPepstring().size() * inOMSSA.GetRequest().front()->GetSettings().GetIonstosearch().size();
				int tot_num_ions = ((*iHit)->GetPepstring().length()-1) * 2;
				sHit->SetAttlist().SetTot_num_ions(NStr::IntToString(tot_num_ions));
                sHit->SetAttlist().SetCalc_neutral_pep_mass(NStr::DoubleToString((*iHit)->GetTheomass()/scale));
                sHit->SetAttlist().SetMassdiff(NStr::DoubleToString(neutral_precursor_mass - ((*iHit)->GetTheomass())/scale));
                //sHit->SetSearch_hit().SetAttlist().SetNum_tol_term("42"); //skip
                //sHit->SetSearch_hit().SetAttlist().SetNum_missed_cleavages("42"); //skip
                sHit->SetAttlist().SetIs_rejected("0");
                sHit->SetAttlist().SetProtein_descr((*iPephit)->GetDefline());
                //sHit->SetSearch_hit().SetAttlist().SetCalc_pI("42"); //skip
                //sHit->SetSearch_hit().SetAttlist().SetProtein_mw("42"); //skip
                CRef<CSearch_score> pValue(new CSearch_score);
                pValue->SetAttlist().SetName("pvalue");
                pValue->SetAttlist().SetValue(NStr::DoubleToString((*iHit)->GetPvalue()));
                CRef<CSearch_score> eValue(new CSearch_score);
                eValue->SetAttlist().SetName("expect");
                eValue->SetAttlist().SetValue(NStr::DoubleToString((*iHit)->GetEvalue()));
                sHit->SetSearch_score().push_back(pValue);
                sHit->SetSearch_score().push_back(eValue);
                if ((*iHit)->CanGetScores()) {
                    ITERATE(CMSHits::TScores, iScore, (*iHit)->GetScores()) {
                        CRef<CSearch_score> score(new CSearch_score);
                        score->SetAttlist().SetName((*iScore)->GetName());
                        score->SetAttlist().SetValue(NStr::DoubleToString((*iScore)->GetValue()));
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
                CRef<CModification_info> modInfo = ConvertModifications(*iHit, Modset, variableMods);
                if (modInfo) sHit->SetModification_info(*modInfo);
                
                sResult->SetSearch_hit().push_back(sHit);
            }
            sQuery->SetSearch_result().push_back(sResult);
            rSum->SetSpectrum_query().push_back(sQuery);
        }
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
