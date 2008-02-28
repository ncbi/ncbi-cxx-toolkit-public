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

string CPepXML::ConvertAA(char in) {
    string out;
    CSeqConvert::Convert(&in, CSeqUtil::e_Ncbistdaa, 0, 1, out, CSeqUtil::e_Ncbieaa);
    return out;
}

void CPepXML::ConvertModification(CRef<CSearch_summary> sSum, CRef <CMSModSpecSet> Modset, int modnum, bool fixed) {
//     eMSModType_modaa   = 0,  ///< at particular amino acids
//     eMSModType_modn    = 1,  ///< at the N terminus of a protein
//     eMSModType_modnaa  = 2,  ///< at the N terminus of a protein at particular amino acids
//     eMSModType_modc    = 3,  ///< at the C terminus of a protein
//     eMSModType_modcaa  = 4,  ///< at the C terminus of a protein at particular amino acids
//     eMSModType_modnp   = 5,  ///< at the N terminus of a peptide
//     eMSModType_modnpaa = 6,  ///< at the N terminus of a peptide at particular amino acids
//     eMSModType_modcp   = 7,  ///< at the C terminus of a peptide
//     eMSModType_modcpaa = 8,  ///< at the C terminus of a peptide at particular amino acids
//     eMSModType_modmax  = 9  ///< the max number of modification types
    // NB: pepXML does not seem to allow for modification to the terminus of a protein at particular amino acids
    int type = Modset->GetModType(modnum);
    if ( type % 2 == 0) {  // Must apply to a paticular amino acid
        for (int i=0; i< Modset->GetModNumChars(modnum); i++) {
            CRef<CAminoacid_modification> aaMod(new CAminoacid_modification);
            int modchar = Modset->GetModChar(modnum, i);
            aaMod->SetAttlist().SetAminoacid(ConvertAA(modchar));
            double mdiff = MSSCALE2DBL(Modset->GetModMass(modnum));
            string mass = NStr::DoubleToString(MonoMass[modchar] - mdiff);
            aaMod->SetAttlist().SetMassdiff(NStr::DoubleToString(mdiff));
            aaMod->SetAttlist().SetMass(mass);
            if (fixed) {
                aaMod->SetAttlist().SetVariable("N");
            } else {
                aaMod->SetAttlist().SetVariable("Y");
            }
            if (type > 0) {
                if (type == eMSModType_modnpaa) aaMod->SetAttlist().SetPeptide_terminus("N");
                if (type == eMSModType_modcpaa) aaMod->SetAttlist().SetPeptide_terminus("C");
            }
            aaMod->SetAttlist().SetDescription(Modset->GetModName(modnum));
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
        termMod->SetAttlist().SetDescription(Modset->GetModName(modnum));
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
//         1 = start scan
//         2 = end scan
//         3 = charge state
//         4 = file extension (.dta)
// def:    string to return if SpecID is not a dta filename
string CPepXML::ParseScan(string SpecID, int field, string def) {
	CRegexp RxpParse(".*\\.(\\d+)\\.(\\d+)\\.(\\d+)(\\.dta)?", CRegexp::fCompile_ignore_case);
	string match = RxpParse.GetMatch(SpecID, 0, field);
	if (match == "") return def;
	
	return match;
	
 //string start = "0";
 //   if (NStr::EndsWith(SpecID, "dta", NStr::eNocase)) {
 //       list<string> result;
 //       NStr::Split(SpecID, ".", result);
 //       list<string>::reverse_iterator rIter = result.rbegin();
 //       for (int i = 0; i < field; i++) { rIter++; }
 //       start = (*rIter);
 //   } else {
 //       start = def;
 //   }
 //   return start;
}

void CPepXML::ConvertFromOMSSA(CMSSearch& inOMSSA, CRef <CMSModSpecSet> Modset, string basename) {
    string newname = basename + ".pep.xml";

    float scale = static_cast<float>(inOMSSA.GetRequest().front()->GetSettings().GetScale());

    CTime datetime(CTime::eCurrent);

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
    sSum->SetAttlist().SetBase_name(basename);
    sSum->SetAttlist().SetSearch_engine("OMSSA");

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
        ConvertModification(sSum, Modset, *iterF, true);
    }

    // Variable mods
    CMSSearchSettings::TVariable::const_iterator iterV;
    for (iterV = inOMSSA.GetRequest().front()->GetSettings().GetVariable().begin();
         iterV != inOMSSA.GetRequest().front()->GetSettings().GetVariable().end(); ++iterV) {
        ConvertModification(sSum, Modset, *iterV, false);
    }
    rSum->SetSearch_summary().push_back(sSum);
    
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
            string spectrumID = HitSet->GetIds().front();
            string query = NStr::IntToString(HitSet->GetNumber());
            sQuery->SetAttlist().SetSpectrum(spectrumID);
            sQuery->SetAttlist().SetStart_scan(ParseScan(spectrumID, 1, query));
            sQuery->SetAttlist().SetEnd_scan(ParseScan(spectrumID, 2, query));
            sQuery->SetAttlist().SetPrecursor_neutral_mass(NStr::DoubleToString((*iHit)->GetMass()/scale));
            sQuery->SetAttlist().SetAssumed_charge(NStr::IntToString((*iHit)->GetCharge()));
            sQuery->SetAttlist().SetIndex(NStr::IntToString(index++));

            // Only one search_result per query (for now)
            CRef<CSearch_result> sResult(new CSearch_result);

            CMSHits::TPephits::const_iterator iPephit;
            int hitRank = 1;
            double prevEValue = (*iHit)->GetEvalue();
            for( ; iHit != HitSet->GetHits().end(); iHit++) {
                // First protein is associated with search_hit, the rest go into alternative_proteins
                iPephit = (*iHit)->GetPephits().begin();
                // Each set of MSHits is a search_hit
                CRef<CSearch_hit> sHit(new CSearch_hit);
                if (prevEValue < (*iHit)->GetEvalue()) hitRank++; 
                sHit->SetAttlist().SetHit_rank(NStr::IntToString(hitRank));
                sHit->SetAttlist().SetPeptide((*iHit)->GetPepstring());
                sHit->SetAttlist().SetPeptide_prev_aa((*iHit)->GetPepstart());
                sHit->SetAttlist().SetPeptide_next_aa((*iHit)->GetPepstop());
                if ((*iPephit)->CanGetAccession()) {
                    sHit->SetAttlist().SetProtein((*iPephit)->GetAccession());
                } else if ((*iPephit)->GetGi()) {
                    sHit->SetAttlist().SetProtein("gi:" + NStr::IntToString((*iPephit)->GetGi()));
                } else {
                    sHit->SetAttlist().SetProtein((*iPephit)->GetDefline());
                }
                sHit->SetAttlist().SetNum_tot_proteins(NStr::IntToString((*iHit)->GetPephits().size()));
                sHit->SetAttlist().SetNum_matched_ions(NStr::IntToString((*iHit)->GetMzhits().size()));
                // skip calculating the theoretical number of ions for now
                //int tot_num_ions = (*iHit)->GetPepstring().size() * inOMSSA.GetRequest().front()->GetSettings().GetIonstosearch().size();
				int tot_num_ions = ((*iHit)->GetPepstring().length()-1) * 2;
				sHit->SetAttlist().SetTot_num_ions(NStr::IntToString(tot_num_ions));
                sHit->SetAttlist().SetCalc_neutral_pep_mass(NStr::DoubleToString((*iHit)->GetTheomass()/scale));
                sHit->SetAttlist().SetMassdiff(NStr::DoubleToString(((*iHit)->GetTheomass() - (*iHit)->GetMass())/scale));
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
                // Generate alternative_proteins
                for (iPephit++ ; iPephit != (*iHit)->GetPephits().end(); iPephit++) {
                    CRef<CAlternative_protein> altPro(new CAlternative_protein);
                    altPro->SetAttlist().SetProtein((*iPephit)->GetAccession());
	                altPro->SetAttlist().SetProtein_descr((*iPephit)->GetDefline());
	                //altPro->SetAlternative_protein().SetAttlist().SetNum_tol_term(); //skip
	                //altPro->SetAlternative_protein().SetAttlist().SetProtein_mw();  //skip
                    sHit->SetAlternative_protein().push_back(altPro);
                }
                sResult->SetSearch_hit().push_back(sHit);
            }
            sQuery->SetSearch_result().push_back(sResult);
            rSum->SetSpectrum_query().push_back(sQuery);
        }
    }
    this->SetMsms_run_summary().push_back(rSum);

}


END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE
