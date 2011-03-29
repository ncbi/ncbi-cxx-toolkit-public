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
 *           Functions and classes to handle CD annotations and biostruc features.
 *
 */

#include <ncbi_pch.hpp>
//#include <corelib/ncbistd.hpp>

#include <list>

#include <objects/cdd/Align_annot.hpp>
#include <objects/cdd/Align_annot_set.hpp>
#include <objects/cdd/Cdd_id.hpp>
#include <objects/cdd/Cdd_id_set.hpp>
#include <objects/cdd/Feature_evidence.hpp>
#include <objects/cdd/Global_id.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/mmdb1/Biostruc_descr.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set_descr.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Residue_interval_pntr.hpp>

#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuBookRefUtils.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>

#include <algo/structure/cd_utils/cuCdAnnotation.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

unsigned int GetAlignAnnotFromToPairs(const CAlign_annot& feature, vector<FromToPair>& pairs)
{
    unsigned int from, to;
    pairs.clear();

    const CAlign_annot::TLocation& location = feature.GetLocation();
    if (location.IsPacked_int()) {
        const CPacked_seqint::Tdata& intervals = location.GetPacked_int().Get();
        CPacked_seqint::Tdata::const_iterator intervalCit;
        for (intervalCit = intervals.begin(); intervalCit != intervals.end(); ++intervalCit) {
            from = (*intervalCit)->GetFrom();
            to   = (*intervalCit)->GetTo();
            pairs.push_back(FromToPair(from, to));            
        }
    } else if (location.IsInt()) {
        from = location.GetInt().GetFrom();
        to   = location.GetInt().GetTo();
        pairs.push_back(FromToPair(from, to));            
    }
    return pairs.size();
}

void MapAlignAnnotUsingFromToPairs(const vector<FromToPair>& pairs, CAlign_annot& feature) 
{
    unsigned int i;
    unsigned int nPairs = pairs.size();

    CAlign_annot::TLocation& location = feature.SetLocation();
    if (location.IsPacked_int()) {
        CPacked_seqint::Tdata& intervals = location.SetPacked_int().Set();
        CPacked_seqint::Tdata::iterator intervalIt;
        for (i = 0, intervalIt = intervals.begin(); i < nPairs && intervalIt != intervals.end(); ++i, ++intervalIt) {
            (*intervalIt)->SetFrom() = pairs[i].first;
            (*intervalIt)->SetTo() = pairs[i].second;
        }
    } else if (location.IsInt() && nPairs == 1) {
        location.SetInt().SetFrom() = pairs[0].first;
        location.SetInt().SetTo() = pairs[0].second;
    } else {
        cerr << "MapAlignAnnotUsingFromToPairs:  unexpected situation encountered.  " << nPairs << " pairs; IsInt = " << location.IsInt() << endl;
    }
   
}

unsigned int GetAlignAnnotEvidenceFromToPairs(const CAlign_annot& feature, vector<FromToPair>& pairs)
{

    CAlign_annot::TEvidence::const_iterator evidenceCit;
    CBiostruc_annot_set::TFeatures::const_iterator featureCit;
    CBiostruc_feature_set::TFeatures::const_iterator bsFeatureCit;
    CResidue_pntrs::TInterval::const_iterator intervalCit;
    TSeqPos from, to;

    pairs.clear();

    if (feature.IsSetEvidence()) {
        const CAlign_annot::TEvidence& evidence = feature.GetEvidence();
        for (evidenceCit = evidence.begin(); evidenceCit != evidence.end(); ++evidenceCit) {
            if ((*evidenceCit)->IsBsannot()) {
                const CBiostruc_annot_set::TFeatures& features = (*evidenceCit)->GetBsannot().GetFeatures();
                for (featureCit = features.begin(); featureCit != features.end(); ++featureCit) {
                    const CBiostruc_feature_set::TFeatures& bsFeatures = (*featureCit)->GetFeatures();
                    for (bsFeatureCit = bsFeatures.begin(); bsFeatureCit != bsFeatures.end(); ++bsFeatureCit) {

                        if ((*bsFeatureCit)->IsSetLocation() && (*bsFeatureCit)->GetLocation().IsSubgraph() && (*bsFeatureCit)->GetLocation().GetSubgraph().IsResidues()) {
                            const CChem_graph_pntrs::TResidues& residues = (*bsFeatureCit)->GetLocation().GetSubgraph().GetResidues();
                            if (residues.IsInterval()) {
                                const CResidue_pntrs::TInterval& intervals = residues.GetInterval();
                                for (intervalCit = intervals.begin(); intervalCit != intervals.end(); ++intervalCit) {
                                    //  In example features, molecule 1 seemed to be the main chain
                                    if ((*intervalCit)->GetMolecule_id() == 1) {
                                        from = (TSeqPos)(*intervalCit)->GetFrom().Get();
                                        to   = (TSeqPos)(*intervalCit)->GetTo().Get();
                                        pairs.push_back(FromToPair(from, to));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return pairs.size();
}


void AppendPairToPositions(const FromToPair& pair, vector<unsigned int>& positions)
{
//    positions.clear();
    for (unsigned int j = pair.first; j <= pair.second; ++j) {
        positions.push_back(j);
    }
}

unsigned int GetAlignAnnotPositions(const CAlign_annot& feature, vector<unsigned int>& positions)
{
    vector<FromToPair> pairs;
    unsigned int i = 0, nPairs = GetAlignAnnotFromToPairs(feature, pairs);

    positions.clear();
    for (; i < nPairs; ++i) {
        AppendPairToPositions(pairs[i], positions);
    }
    return positions.size();
}

unsigned int GetAlignAnnotEvidencePositions(const CAlign_annot& feature, vector<unsigned int>& positions)
{
    vector<FromToPair> pairs;
    unsigned int i = 0, nPairs = GetAlignAnnotEvidenceFromToPairs(feature, pairs);

    positions.clear();
    for (; i < nPairs; ++i) {
        AppendPairToPositions(pairs[i], positions);
    }
    return positions.size();
}


string BSDescrToString(const CRef< CBiostruc_descr >& descr) {

    string bsDescr;
    if (descr->IsName()) {
        bsDescr = "Biostruc Name:  " + descr->GetName();
    } else if (descr->IsPdb_comment()) {
        bsDescr = "Biostruc PDB comment:  " + descr->GetPdb_comment();
    } else if (descr->IsOther_comment()) {
        bsDescr = "Biostruc comment:  " + descr->GetOther_comment();
    }
    if (bsDescr.size() > 0) {
        bsDescr += "\n";
    }
    return bsDescr;
}

string BSFeatureSetDescrToString(const CRef< CBiostruc_feature_set_descr >& descr) {

    string bsFeatureDescr;
    if (descr->IsName()) {
        bsFeatureDescr = "BSAnnot Feature Name:  " + descr->GetName();
    } else if (descr->IsPdb_comment()) {
        bsFeatureDescr = "BSAnnot Feature PDB comment:  " + descr->GetPdb_comment();
    } else if (descr->IsOther_comment()) {
        bsFeatureDescr = "BSAnnot Feature comment:  " + descr->GetOther_comment();
    }
    if (bsFeatureDescr.size() > 0) {
        bsFeatureDescr += "\n";
    }
    return bsFeatureDescr;
}



string CAlignAnnotToString(const CRef< CAlign_annot>& feature, bool includeFromTo, bool includeEvidence, bool hyphenateFromTo) {
    if (feature.NotEmpty()) {
        return CAlignAnnotToString(*feature, includeFromTo, includeEvidence, hyphenateFromTo);
    } else {
        return "";
    }
}

string PairsToString(const vector<FromToPair>& pairs, bool hyphenateRanges) {

    static const string pairSep = ", ";
    static const string closeBracket = "]";

    char buf[1024];
    unsigned int from, to;
    unsigned int nPairs = pairs.size();
    string endStr;
    string fromToStr = (nPairs > 0) ? "[" : "";

    for (unsigned int i = 0; i < nPairs; ++i) {
        from = pairs[i].first;
        to   = pairs[i].second;
        endStr = (i != nPairs - 1) ? pairSep : closeBracket;

        if (from != to) {
            if (hyphenateRanges) {
                sprintf(buf, "%d-%d%s", from, to, endStr.c_str());
            } else {
                string s;
                for (unsigned int j = from; j < to; ++j) {
                    sprintf(buf, "%d%s", j, pairSep.c_str());
                    s += string(buf);
                }
                fromToStr += s;
                sprintf(buf, "%d%s", to, endStr.c_str());
            }
        } else {
            sprintf(buf, "%d%s", from, endStr.c_str());
        }

        fromToStr += string(buf);
    }
    return fromToStr;
}

bool IsEvidenceTypeInAlignAnnot(const CAlign_annot& feature, CFeature_evidence::E_Choice type) {

    bool result = false;

    if (feature.IsSetEvidence()) {

        CAlign_annot::TEvidence::const_iterator evidenceIt, evidenceItEnd = feature.GetEvidence().end();
        for (evidenceIt = feature.GetEvidence().begin(); evidenceIt != evidenceItEnd; ++evidenceIt) {

            if (evidenceIt->Empty()) continue;

            if ((*evidenceIt)->Which() == type) {
                result = true;
                break;
            }
        }
    }
    return result;
}

bool IsCommentEvidenceInAlignAnnot(const CAlign_annot& feature) {
    return IsEvidenceTypeInAlignAnnot(feature, CFeature_evidence::e_Comment);
}

bool IsStructureEvidenceInAlignAnnot(const CAlign_annot& feature) {
    return IsEvidenceTypeInAlignAnnot(feature, CFeature_evidence::e_Bsannot);
}

bool IsReferenceEvidenceInAlignAnnot(const CAlign_annot& feature) {
    return IsEvidenceTypeInAlignAnnot(feature, CFeature_evidence::e_Reference);
}

bool IsBookRefEvidenceInAlignAnnot(const CAlign_annot& feature) {
    return IsEvidenceTypeInAlignAnnot(feature, CFeature_evidence::e_Book_ref);
}

bool IsSeqfeatEvidenceInAlignAnnot(const CAlign_annot& feature) {
    return IsEvidenceTypeInAlignAnnot(feature, CFeature_evidence::e_Seqfeat);
}

string GetAlignAnnotDescription(const CAlign_annot& feature) {
    string featureDesc = kEmptyStr;
    if (feature.IsSetDescription()) {
        featureDesc = feature.GetDescription();
    }
    return featureDesc;
}

CAlign_annot::TType GetAlignAnnotType(const CAlign_annot& feature) {
    CAlign_annot::TType result = -1;
    if (feature.IsSetType()) {
        result = feature.GetType();
    }
    return result;
}

void SetAlignAnnotDescription(const string& descr, CAlign_annot& feature)
{
    feature.SetDescription(descr);
}

bool SetAlignAnnotType(CAlign_annot::TType type, CAlign_annot& feature)
{
    static const CAlign_annot::TType minType = 0;  // as per the ASN.1 spec
    static const CAlign_annot::TType maxType = 6;  // as per the ASN.1 spec
    bool result = (type >= minType && type <= maxType);
    if (result) {
        feature.SetType(type);
    }
    return result;
}

void PurgeTypeFromAlignAnnots(CCdd& cd)
{
    if (cd.IsSetAlignannot()) {
        CAlign_annot_set::Tdata& features = cd.SetAlignannot().Set();
        NON_CONST_ITERATE ( CAlign_annot_set::Tdata, fit, features) {
            if ((*fit)->IsSetType()) (*fit)->ResetType();
        }
    }
}

string CAlignAnnotToString(const CAlign_annot& feature, bool includeFromTo, bool includeEvidence, bool hyphenateFromTo) {
    static const string descrHeader = "Description:\n";
    static const string positionHeader = "\n    Positions:\n    ";
    static const string evidenceHeader = "\n    Details:\n";
    static const string spacer = "    ";

    string featureDesc = kEmptyStr, evidenceDetails = kEmptyStr;
    CAlign_annot::TEvidence::const_iterator evidenceIt, evidenceItEnd;

    //  Global description for the align-annot
    if (feature.IsSetDescription()) {
        featureDesc = descrHeader + spacer + feature.GetDescription();
    }

    vector<FromToPair> pairs;
    if (includeFromTo && GetAlignAnnotFromToPairs(feature, pairs) > 0) {
        featureDesc += positionHeader + PairsToString(pairs, hyphenateFromTo);
    }

    //  Look at the evidence blobs for the align-annot.
    if (includeEvidence && feature.IsSetEvidence()) {
        evidenceDetails = evidenceHeader;
        evidenceItEnd = feature.GetEvidence().end();
        for (evidenceIt = feature.GetEvidence().begin(); evidenceIt != evidenceItEnd; ++evidenceIt) {

            if (evidenceIt->Empty()) continue;

            const CFeature_evidence& evidence = **evidenceIt;

            if (evidence.IsComment()) { 
                evidenceDetails += spacer + "Comment:  " + evidence.GetComment();
                evidenceDetails += "\n";

            } else if (evidence.IsBsannot()) {
                const CFeature_evidence::TBsannot& bsAnnotSet = evidence.GetBsannot();
                CFeature_evidence::TBsannot::TDescr::const_iterator bsDescrIt, bsDescrItEnd;
                if (bsAnnotSet.IsSetDescr()) {
                    bsDescrIt    = bsAnnotSet.GetDescr().begin();
                    bsDescrItEnd = bsAnnotSet.GetDescr().end();
                    for (; bsDescrIt != bsDescrItEnd; ++bsDescrIt) {
                        evidenceDetails += spacer + BSDescrToString(*bsDescrIt);
                        evidenceDetails += "\n";
                    }
                }


                const CFeature_evidence::TBsannot::TFeatures& bsFeatures = bsAnnotSet.GetFeatures();
                CFeature_evidence::TBsannot::TFeatures::const_iterator bsFeatureIt = bsFeatures.begin(), bsFeatureItEnd = bsFeatures.end();
                for (; bsFeatureIt != bsFeatureItEnd; ++bsFeatureIt) {

                    //  Biostruc_feature_set's 'descr' field ==> Biostruc_feature_set_descr
                    CBiostruc_feature_set::TDescr::const_iterator bsFeatSetDescrIt, bsFeatSetDescrItEnd;
                    if ((*bsFeatureIt)->IsSetDescr()) {
                        bsFeatSetDescrIt = (*bsFeatureIt)->GetDescr().begin();
                        bsFeatSetDescrItEnd = (*bsFeatureIt)->GetDescr().end();
                        for (; bsFeatSetDescrIt != bsFeatSetDescrItEnd; ++bsFeatSetDescrIt) {
                            evidenceDetails += spacer + BSFeatureSetDescrToString(*bsFeatSetDescrIt);
                        }
                    }

                    //  Biostruc_feature_set's 'features' field ==> Biostruc_feature
                    CBiostruc_feature_set::TFeatures::const_iterator bsFeatSetFeatIt, bsFeatSetFeatItEnd;
                    bsFeatSetFeatIt = (*bsFeatureIt)->GetFeatures().begin();
                    bsFeatSetFeatItEnd = (*bsFeatureIt)->GetFeatures().end();
                    for (; bsFeatSetFeatIt != bsFeatSetFeatItEnd; ++bsFeatSetFeatIt) {
                        if ((*bsFeatSetFeatIt)->IsSetName()) {
                            evidenceDetails += spacer + (*bsFeatSetFeatIt)->GetName() + "\n";
                        }
                    }
                }

            } else if (evidence.IsReference()) {
                if (evidence.GetReference().IsPmid()) {
                    evidenceDetails += spacer + "PMID:  " + NStr::IntToString(evidence.GetReference().GetPmid().Get());
                    evidenceDetails += "\n";
                }
            } else if (evidence.IsBook_ref()) {
                evidenceDetails += spacer + "Book reference:  " + CCddBookRefToString(evidence.GetBook_ref());
                evidenceDetails += "\n";

                //  This could contain a ton of things; deal with it as needed.  For now,
                //  just print the code for the type of the mandatory SeqFeatData object.
            } else if (evidence.IsSeqfeat()) {
                int choice = (int) evidence.GetSeqfeat().GetData().Which();
                evidenceDetails += spacer + "SeqFeat reference:  SeqFeatData type code " + NStr::IntToString(choice);
                evidenceDetails += "\n";
            } 
        }
        featureDesc += evidenceDetails;
    }
    return featureDesc;
}

string CAlignAnnotToASNString(const CRef< CAlign_annot>& feature, bool includeEvidence) {
    if (feature.NotEmpty()) {
        return CAlignAnnotToASNString(*feature, includeEvidence);
    } else {
        return "";
    }
}

string CAlignAnnotToASNString(const CAlign_annot& feature, bool includeEvidence) {

    CRef< CAlign_annot > featureCopy(new CAlign_annot);
    featureCopy->Assign(feature);
    
    // If keeping the evidence, just dump the object as-is.
    if (!includeEvidence && featureCopy->IsSetEvidence()) {
        featureCopy->ResetEvidence();
    }

    CNcbiOstrstream oss;
    oss << MSerial_AsnText << *featureCopy << endl;
    return CNcbiOstrstreamToString(oss);
}

bool GetFeatureFromCD(CCdCore& cd, unsigned int featureIndex, CRef<CAlign_annot>& feature) {
    bool result = false;
    unsigned int ctr = 0;
    
    CAlign_annot_set::Tdata::iterator featuresIt;

    //  data in Align_annot_set is mandatory
    if (cd.IsSetAlignannot()) {
        CAlign_annot_set::Tdata& features = cd.SetAlignannot().Set();
        for (featuresIt = features.begin(); featuresIt != features.end() && ctr <= featureIndex; ++featuresIt, ++ctr) {
            if (ctr == featureIndex) {
                feature = (*featuresIt);
                result = true;
            }
        }
    }
    return result;
}

bool GetFeatureFromCD(const CCdCore& cd, unsigned int featureIndex, CAlign_annot_set::Tdata::const_iterator& returnedCit) { //const CRef<CAlign_annot>& feature) {
    bool result = false;
    unsigned int ctr = 0;
    
    CAlign_annot_set::Tdata::const_iterator featuresCit;

    //  data in Align_annot_set is mandatory
    if (cd.IsSetAlignannot()) {
        const CAlign_annot_set::Tdata& features = cd.GetAlignannot().Get();
        for (featuresCit = features.begin(); featuresCit != features.end() && ctr <= featureIndex; ++featuresCit, ++ctr) {
            if (ctr == featureIndex) {
//                feature = (*featuresCit);
                returnedCit = featuresCit;
                result = true;
            }
        }
    }
    return result;
/*    unsigned int ctr = 0;
    CAlign_annot_set::Tdata::const_iterator featuresCit;

    //  data in Align_annot_set is mandatory
    if (cd.IsSetAlignannot()) {
        const CAlign_annot_set::Tdata& features = cd.GetAlignannot().Get();
        for (featuresCit = features.begin(); featuresCit != features.end() && ctr <= featureIndex; ++featuresCit, ++ctr) {
            if (ctr == featureIndex) {
                return (*featuresCit);
            }
        }
    }

    const CRef<CAlign_annot> emptyFeature(0);
    return emptyFeature;
*/
}


// ========================================
//      CCdAnnotationInfo class
// ========================================

bool CCdAnnotationInfo::m_useHyphenatedRanges = true;

CCdAnnotationInfo::CCdAnnotationInfo(const CCdCore* cd, bool onlyMasterPos) : m_cd(cd), m_cdMappedFrom(NULL) 
{
    Initialize(onlyMasterPos);
}

string ToFormattedString(const string& sourceId, const string& annotDescr, const vector<FromToPair>& pairs, bool isMapped, bool useLongForm, bool hyphenateRanges)
{
    static const string pairSep = ", ";

    unsigned int nPairs = pairs.size();
    string fromToStr;
    string mySrc = (sourceId.size() > 0) ? sourceId + " " : "No_Source ";
    string myDes = (annotDescr.size() > 0) ? annotDescr + " " : "No_Description ";

    //  0 == generic (mapped); 1 == specific (non-mapped)
    string formattedString = (isMapped) ? "0 " : "1 ";  

    //  Make string of all from-to intervals associated with the feature.
    if (nPairs > 0 && useLongForm) {
        fromToStr += PairsToString(pairs, hyphenateRanges);
    } else {
        fromToStr += "[No_positions_found]";
    }

    formattedString += mySrc;
    if (useLongForm) {
        formattedString += myDes + fromToStr;
    }

    return formattedString;
}

void PurgeEvidence(CAlign_annot_set& alignAnnotSet) {

    CAlign_annot_set::Tdata aaSet = alignAnnotSet.Set();
    CAlign_annot_set::Tdata::iterator aaIt = aaSet.begin(), aaEnd = aaSet.end();
    for (; aaIt != aaEnd; ++aaIt) {
        if ((*aaIt)->IsSetEvidence()) {
            (*aaIt)->ResetEvidence();
        }
    }
}



unsigned int CCdAnnotationInfo::ToString(const CCdCore& cd, vector<string>& annotations, bool useAccession)
{
    CCdAnnotationInfo ai(&cd, true);
    return ai.ToString(annotations, useAccession);
}

unsigned int CCdAnnotationInfo::ToString(vector<string>& annotations, bool useAccession) const {

    static const string noDescr = "No_Description";

    annotations.clear();
    if (m_cd == NULL) return 0;

    string thisFeature, thisDescr;
    const string identifier = (useAccession) ? m_cd->GetAccession() : NStr::IntToString(m_cd->GetUID());

    AnnotDescrCit descrIt, descrEnd = m_annotDescrMap.end();
    CdAnnotMap::const_iterator annotCit = m_masterAnnotMap.begin(), annotEnd = m_masterAnnotMap.end();

    for (; annotCit != annotEnd; ++annotCit) {

        thisFeature = kEmptyStr;
        descrIt = m_annotDescrMap.find(annotCit->first);
        thisDescr = (descrIt != descrEnd) ? descrIt->second : noDescr;

        const vector<FromToPair>& pairs = annotCit->second;
        thisFeature = ToFormattedString(identifier, thisDescr, pairs, (m_cdMappedFrom != NULL), true, m_useHyphenatedRanges);
        annotations.push_back(thisFeature);
    }

    return annotations.size();
}


string CCdAnnotationInfo::ToString(const CCdCore& cd, bool useAccession)
{
    CCdAnnotationInfo ai(&cd, true);
    return ai.ToString(useAccession);
}



string CCdAnnotationInfo::ToString(bool useAccession) const {
    string result = kEmptyStr;
    vector<string> annotations;

    unsigned int i, nAnnot = ToString(annotations, useAccession);
    if (nAnnot > 0) {
        for (i = 0; i < nAnnot; ++i) {
            result += annotations[i] + "\n";
        }
    }

    return result;
}

unsigned int CCdAnnotationInfo::ToASNString(const CCdCore& cd, vector<string>& annotations, bool withEvidence)
{
    CCdAnnotationInfo ai(&cd, true);
    return ai.ToASNString(annotations, withEvidence);
}

unsigned int CCdAnnotationInfo::ToASNString(vector<string>& annotations, bool withEvidence) const {

    string aaASN;
    CAlign_annot_set::Tdata::iterator aaIt, aaEnd;
    CRef< CAlign_annot_set > crefAASet( new CAlign_annot_set);
    if (m_cdMappedFrom && m_cdMappedFrom->IsSetAlignannot()) {
        crefAASet->Assign(m_cdMappedFrom->GetAlignannot());
    } else if (m_cd && m_cd->IsSetAlignannot()) {
        crefAASet->Assign(m_cd->GetAlignannot());
    }

    if (!withEvidence) {
        PurgeEvidence(*crefAASet);
    }

    GenerateMappedAnnotSet(*crefAASet);

    annotations.clear();
    aaEnd = crefAASet->Set().end();
    for (aaIt = crefAASet->Set().begin(); aaIt != aaEnd; ++aaIt) {
        aaASN = CAlignAnnotToASNString(*aaIt, withEvidence);
        annotations.push_back(aaASN);
    }
    return annotations.size();
}


string CCdAnnotationInfo::ToASNString(const CCdCore& cd, bool withEvidence)
{
    CCdAnnotationInfo ai(&cd, true);
    return ai.ToASNString(withEvidence);
}

string CCdAnnotationInfo::ToASNString(bool withEvidence) const {
    
    CRef< CAlign_annot_set > crefAASet( new CAlign_annot_set);
    if (m_cdMappedFrom && m_cdMappedFrom->IsSetAlignannot()) {
        crefAASet->Assign(m_cdMappedFrom->GetAlignannot());
    } else if (m_cd && m_cd->IsSetAlignannot()) {
        crefAASet->Assign(m_cd->GetAlignannot());
    }

    if (!withEvidence) {
        PurgeEvidence(*crefAASet);
    }

    GenerateMappedAnnotSet(*crefAASet);

    CNcbiOstrstream oss;
    oss << MSerial_AsnText << *crefAASet << endl;
    return CNcbiOstrstreamToString(oss);

}

void CCdAnnotationInfo::GenerateMappedAnnotSet(CAlign_annot_set& alignAnnotSet) const {

    if (m_cdMappedFrom == NULL) {
//        cerr << "This object represents unmapped annotations; no mapping required." << endl;
        return;
    }

    CAlign_annot_set::Tdata::iterator aaIt = alignAnnotSet.Set().begin(), aaEnd = alignAnnotSet.Set().end();
    CdAnnotMapCit cit = m_masterAnnotMap.begin(), citEnd = m_masterAnnotMap.end();

    if (alignAnnotSet.Set().size() != m_masterAnnotMap.size()) {
        cerr << "bug:  expect m_masterAnnotMap to be same size as alignAnnotSet to be mapped!" << endl;
        return;
    }

    for (; cit != citEnd && aaIt != aaEnd; ++cit, ++aaIt) {
        MapAlignAnnotUsingFromToPairs(cit->second, **aaIt);
    }
}

string CCdAnnotationInfo::ToFtpDumpString(const CCdCore& cd, bool zeroBasedCoords) 
{
    CCdAnnotationInfo ai(&cd, true);
    return ai.ToFtpDumpString(zeroBasedCoords);
}

string CCdAnnotationInfo::ToFtpDumpString(bool zeroBasedCoords) const {

    static const string zeroStr = "0";
    static const string oneStr = "1";
    static const string coordSepStr = ",";
    static const string tabDelim = "\t";
    static const string newline = "\n";

    if (m_cd == NULL || m_masterAnnotMap.size() == 0) return kEmptyStr;

    string result = kEmptyStr;
    string cdAcc = m_cd->GetAccession();
    string cdName = m_cd->GetName();
    string pssmIdStr = NStr::IntToString(m_cd->GetUID());
    string rowsStr, featureDesc;
    vector<string> stringsToJoin;
    vector<string> linesToJoin;

    unsigned int coordOffset = (zeroBasedCoords) ? 0 : 1;
    unsigned int nPairs, nPositions;
    vector<unsigned int> positions;
    const CRef<CAlign_annot> feature;
    CAlign_annot_set::Tdata::const_iterator foundCit;

    CdAnnotMap::const_iterator annotCit = m_masterAnnotMap.begin(), annotEnd = m_masterAnnotMap.end();

    for (; annotCit != annotEnd; ++annotCit) {

        positions.clear();
        rowsStr = featureDesc = kEmptyStr;
        nPairs = annotCit->second.size();

        //  Make string of all rows associated with the feature.
        for (unsigned int i = 0; i < nPairs; ++i) {
            AppendPairToPositions(annotCit->second.at(i), positions);
        }

        nPositions = positions.size();
        for (unsigned int i = 0; i < nPositions; ++i) {
            if (i > 0) rowsStr += coordSepStr;
            rowsStr += NStr::UIntToString(positions[i] + coordOffset);
        }

        //  Get descriptive information for the feature
        if (GetFeatureFromCD(*m_cd, annotCit->first, foundCit)) {

            stringsToJoin.clear();
            stringsToJoin.push_back(pssmIdStr);
            stringsToJoin.push_back(cdAcc);
            stringsToJoin.push_back(cdName);
            stringsToJoin.push_back(NStr::UIntToString(annotCit->first + 1));

            featureDesc = GetAlignAnnotDescription(**foundCit);
            stringsToJoin.push_back(featureDesc);

            if (IsStructureEvidenceInAlignAnnot(**foundCit)) {
                stringsToJoin.push_back(oneStr);
            } else {
                stringsToJoin.push_back(zeroStr);
            }
            if (IsReferenceEvidenceInAlignAnnot(**foundCit)) {
                stringsToJoin.push_back(oneStr);
            } else {
                stringsToJoin.push_back(zeroStr);
            }
            if (IsCommentEvidenceInAlignAnnot(**foundCit)) {
                stringsToJoin.push_back(oneStr);
            } else {
                stringsToJoin.push_back(zeroStr);
            }

            stringsToJoin.push_back(rowsStr);

            linesToJoin.push_back(NStr::Join(stringsToJoin, tabDelim));
        }
    }

    if (linesToJoin.size() > 0) {
        result = NStr::Join(linesToJoin, newline);
    }
    return result;
}

string CCdAnnotationInfo::ToLongString(const CCdCore& cd, bool withEvidence, bool useAccession, bool zeroBasedCoords) 
{
    CCdAnnotationInfo ai(&cd, true);
    return ai.ToLongString(withEvidence, useAccession, zeroBasedCoords);
}

string CCdAnnotationInfo::ToLongString(bool withEvidence, bool useAccession, bool zeroBasedCoords) const {

    static const string separator = "\n============================================================\n";

    if (m_cd == NULL) return kEmptyStr;

    string result;
    string cdNameStr = "CD "; 
    string identifier = (useAccession) ? m_cd->GetAccession() : NStr::IntToString(m_cd->GetUID());
    string mappedIdentifier;
    string mappedFromAccession;
    string rowsStr, featureDesc;

    unsigned int coordOffset = (zeroBasedCoords) ? 0 : 1;
    unsigned int nPairs, nPositions;
    vector<unsigned int> positions;
    const CRef<CAlign_annot> feature;
    CAlign_annot_set::Tdata::const_iterator foundCit;

    cdNameStr += m_cd->GetName();
    if (identifier.length() > 0) {
        cdNameStr += " (" + identifier + ")";
    }

    if (m_cdMappedFrom) {
        mappedIdentifier = (useAccession) ? m_cdMappedFrom->GetAccession() : NStr::IntToString(m_cdMappedFrom->GetUID());
        mappedFromAccession = m_cdMappedFrom->GetName() + " (" + mappedIdentifier + ")";
        result = "\nFeature info mapped from " + mappedFromAccession + " onto " + cdNameStr + separator;
    } else {
        result = "\nFeature info for " + cdNameStr + separator;
    }

    if (m_masterAnnotMap.size() == 0) {
        result += "    No features.\n";
    }

//    result += ToString(fics, withEvidence);

    CdAnnotMap::const_iterator annotCit = m_masterAnnotMap.begin(), annotEnd = m_masterAnnotMap.end();

    char buf[1024];
    for (; annotCit != annotEnd; ++annotCit) {

        positions.clear();
        rowsStr = featureDesc = kEmptyStr;
        nPairs = annotCit->second.size();

        //  Make string of all rows associated with the feature.
        for (unsigned int i = 0; i < nPairs; ++i) {
            AppendPairToPositions(annotCit->second.at(i), positions);
        }

        nPositions = positions.size();
        for (unsigned int i = 0; i < nPositions; ++i) {
            if (i != nPositions - 1) rowsStr += ", ";
            rowsStr += NStr::UIntToString(positions[i] + coordOffset);
        }

        //  Get descriptive information for the feature
        if (GetFeatureFromCD(*m_cd, annotCit->first, foundCit)) {
            sprintf(buf, "    Feature %d:\n        For columns: %s\n", annotCit->first + 1, rowsStr.c_str());
            featureDesc = CAlignAnnotToString(*foundCit, false, withEvidence, m_useHyphenatedRanges) + "\n";        
            result += string(buf) + featureDesc;
//            INFO_MESSAGE_CL("        Description:\n" << featureDesc);
//            INFO_MESSAGE_CL("\n");
        }
    }

    result += separator;
    return result;
}


CdAnnotMap CCdAnnotationInfo::GetEvidencePositions() const
{
    CdAnnotMap evidencePositions;
    unsigned int iFeature = 0;
    CAlign_annot_set::Tdata::const_iterator featCit, featEnd;

    if (m_cd && m_cd->IsSetAlignannot()) {

        featCit = m_cd->GetAlignannot().Get().begin();
        featEnd = m_cd->GetAlignannot().Get().end();

        for (; featCit != featEnd; ++featCit, ++iFeature) {
            vector<FromToPair>& pairs = evidencePositions[iFeature];
            GetAlignAnnotFromToPairs(**featCit, pairs);
            _TRACE("Feature " << iFeature+1 << ":  #pairs for evidence = " << pairs.size());
        }
    }

    return evidencePositions;
}

/*
CCdAnnotationInfo* CCdAnnotationInfo::MapAnnotations(CGuideAlignment_Base& guide, string& err, string& warn) const
{
    err.erase();
    CCdAnnotationInfo* mappedAI = NULL;
    unsigned int i, j;
    int mappedFrom, mappedTo;
    bool guideMaster;  // true if m_cd is the guide's master; false if m_cd is the guide's slave
    string mappingErr;
    BlockModelPair bmpCopy = guide.GetGuide();

    if (!m_cd) {
        err = "Can't map annotations:  null CD!";
    }

    //  First, check that the guide involves m_cd.
    if (guide.GetMasterCD() == m_cd) {
        bmpCopy.reverse();
        guideMaster = true;
        if (guide.GetMasterCDRow() != 0) {
            err = "Guide's master CD must use row 0 in order to map annotations correctly.";
        }
    } else if (guide.GetSlaveCD() == m_cd) {
        guideMaster = false; 
        if (guide.GetSlaveCDRow() != 0) {
            err = "Guide's slave CD must use row 0 in order to map annotations correctly.";
        }
    } else {
        guideMaster = false; 
        err = "Guide does not contain CD " + m_cd->GetAccession();
    }

    if (err.length() == 0) {
        mappedAI = new CCdAnnotationInfo();
        if (mappedAI) {

            //  mappedAI->m_cd is the CD from the guide that is *not* m_cd!
            mappedAI->m_cd = (guideMaster) ? guide.GetSlaveCD() : guide.GetMasterCD();
            mappedAI->m_cdMappedFrom = m_cd;

            mappedAI->m_masterAnnotMap = m_masterAnnotMap;

            //  Main part:  use guide to modify the from/to pairs in the mapped master annotation map.
            CdAnnotMapIt it = mappedAI->m_masterAnnotMap.begin(), itEnd = mappedAI->m_masterAnnotMap.end();
            for (j = 0; it != itEnd; ++it, ++j) {
                vector<FromToPair>& pairs = it->second;
                for (i = 0; i < pairs.size(); ++i) {

                    mappedFrom = bmpCopy.mapToMaster((int) pairs[i].first);
                    mappedTo   = bmpCopy.mapToMaster((int) pairs[i].second);

                    //  Require both ends be mappable.
                    if (mappedFrom >= 0 && mappedTo >= 0) {

                        //  The resulting range can cover a different number of residues after mapping
                        //  if the master of the child has an insert in the range of the annotation.
                        if (mappedTo - mappedFrom != (int)(pairs[i].second - pairs[i].first)) {
                            char buf[512];
                            sprintf(buf, "CD %s to CD %s:  mapped range differs for base annotation %d, interval %d, from  [%d, %d] -> [%d, %d]\n", m_cd->GetAccession().c_str(), mappedAI->m_cd->GetAccession().c_str(), j, i, pairs[i].first, pairs[i].second, mappedFrom, mappedTo);
                            warn += string(buf);
                        }

                        pairs[i].first  = (unsigned int) mappedFrom;
                        pairs[i].second = (unsigned int) mappedTo;

                    } else {
                        char buf[256];
                        sprintf(buf, "\n    could not map annotation %d, interval %d from  [%d, %d] -> [%d, %d]", j, i, pairs[i].first, pairs[i].second, mappedFrom, mappedTo);
                        mappingErr += string(buf);
                    }
                }
            }

            if (mappingErr.length() == 0) {

                //  Only create slave annotation map if did so in this instance, too.
                if (m_slaveAnnotMap.size() > 0) {
                    mappedAI->MakeSlaveAnnotMap();
                }
            
                //  Descriptions do not need modification
                mappedAI->m_annotDescrMap = m_annotDescrMap;

            } else {
                err = "Failed to map annotations from " + m_cd->GetAccession() + " to " + mappedAI->m_cd->GetAccession() + mappingErr;
                delete mappedAI;
                mappedAI = NULL;
            }
        }
    }

    return mappedAI;
}
*/


//unsigned int CCdAnnotationInfo::GetFeaturePositions(const CCdCore& cd, CdAnnotMap& coordsOnMaster) {
void CCdAnnotationInfo::Initialize(bool onlyMasterPos) 
{
    m_masterAnnotMap.clear();
    m_annotDescrMap.clear();
    m_slaveAnnotMap.clear();

    if (!m_cd || !m_cd->IsSetAlignannot()) return;

    unsigned int iFeature = 0;
    CAlign_annot_set::Tdata::const_iterator featCit = m_cd->GetAlignannot().Get().begin(), featEnd = m_cd->GetAlignannot().Get().end();

    for (; featCit != featEnd; ++featCit, ++iFeature) {
        vector<FromToPair>& pairs = m_masterAnnotMap[iFeature];
        GetAlignAnnotFromToPairs(**featCit, pairs);

        if ((*featCit)->IsSetDescription()) {
            m_annotDescrMap[iFeature] = (*featCit)->GetDescription();
        }

        _TRACE("Feature " << iFeature+1 << ":  #pairs = " << pairs.size());
    }

    if (!onlyMasterPos) MakeSlaveAnnotMap();
}

void CCdAnnotationInfo::MakeSlaveAnnotMap()
{

    list< CRef< CSeq_align > >::const_iterator sait;
    vector< CRef< CSeq_align > > seqAligns;

    if (m_masterAnnotMap.size() == 0) {
        cerr << "Not making slave annot map due to empty masterAnnotMap.\n";
        return;
    } 

    if (!m_cd->IsSetSeqannot() || m_cd->GetSeqannot().front().Empty() || !m_cd->GetSeqannot().front()->GetData().IsAlign()) {
        cerr << "Not making slave annot map due to unexpected seqannot\n(not set, empty first Seq_annot, or first seqannot not being of type 'align').\n";
        return;
    } 

    //  Cache seq-aligns to avoid continual searching through seq-align list.
    unsigned int nAligns;
    const list< CRef< CSeq_align > >& alignments = m_cd->GetSeqAligns();
    for (sait = alignments.begin(); sait != alignments.end(); ++sait) {
        seqAligns.push_back(*sait);
    }
    nAligns = seqAligns.size();

    int mappedPos;
    unsigned int i, j, nPos, pos;
    vector<unsigned int> masterPositions;
    CdAnnotMapIt it = m_masterAnnotMap.begin(), itEnd = m_masterAnnotMap.end();
    for (; it != itEnd; ++it) {
        vector<FromToPair>& pairs = it->second;
        for (i = 0; i < pairs.size(); ++i) {
            AppendPairToPositions(pairs[i], masterPositions);
        }
    }

//    cerr << "in MakeSlaveAnnotMap;  nAligns = " << nAligns << endl;
    nPos = masterPositions.size();
    for (i = 0; i < nPos; ++i) {
        pos = masterPositions[i];
        vector<unsigned int>& slaveCols = m_slaveAnnotMap[pos];
//        cerr << "    master position: " << masterPositions[i] << ";  existing size:  " << slaveCols.size() << endl;
        for (j = 0; j < nAligns; ++j) {
            mappedPos = MapPositionToChild(pos, *seqAligns[j]);
            slaveCols.push_back((unsigned int) mappedPos);
//            cerr << "        alignment " << j << "; mapped pos = " << mappedPos << endl;
        }
    }
}

string CCdAnnotationInfo::MakeRowInfoString(unsigned int row) const
{
    if (!m_cd) return "";

    string result = "Row " + NStr::UIntToString(row) + ":  ";

    string acc;
    int seqIndex, mmdbId;
    CRef<CSeq_id> seqId;

    if (m_cd->GetSeqIDFromAlignment(row, seqId)) {
        result += GetSeqIDStr(seqId);
        seqIndex = m_cd->GetSeqIndexForRowIndex(row);
        if (m_cd->GetMmdbId(seqIndex, mmdbId)) {
            result += " (MMDB ID = " + NStr::IntToString(mmdbId) + ")";
        }
        result += "\n";
    } else {
        result = "";
    }
    return result;
}

string CCdAnnotationInfo::MappedToSlaveString(unsigned int row) const
{
    string result = MakeRowInfoString(row);
    if (result.length() == 0) return result;

    vector<string> indivAnnots;
    MappedToSlaveString(row, indivAnnots, false);
    for (unsigned int i = 0; i < indivAnnots.size(); ++i) {
        result += indivAnnots[i];
    }
    return result;
}

void CCdAnnotationInfo::MappedToSlaveString(unsigned int row, vector<string>& individualAnnotStrings, bool addRowToAll) const
{
    static const string descrNotFound = "No_Description_Found";

    unsigned int id;
    string str, descr;
    string rowStr = (addRowToAll) ? "Row " + NStr::UIntToString(row) + " Annot " : "Annot ";
    vector<string> ranges;
    CdAnnotMap rowAnnots;

    individualAnnotStrings.clear();
    if (!MapRangesForRow(row, rowAnnots)) {
        return;
    }

    CdAnnotMapIt rowIt = rowAnnots.begin(), rowEnd = rowAnnots.end();
    AnnotDescrCit descrIt, descrEnd = m_annotDescrMap.end();
    
    RangeStringsForAnnots(rowAnnots, ranges);

    for (; rowIt != rowEnd; ++rowIt) {
        id = rowIt->first;
        descrIt = m_annotDescrMap.find(id);
        descr = (descrIt != descrEnd) ? descrIt->second : descrNotFound;
        str = rowStr + NStr::UIntToString(id) + ":  " + descr + "   " + ranges[id] + "\n";
        individualAnnotStrings.push_back(str);
    }
    
}

string CCdAnnotationInfo::MappedToSlaveString(bool structuresOnly, bool groupByRow) const
{
    string result;
    unsigned int j, row;
    unsigned int i = 0, nRows = (m_cd) ? m_cd->GetNumRows() : 0;
    vector<unsigned int> rows;
    vector<string> annots;
    const CPDB_seq_id* pdbId = NULL;

    //  groupByRow = true:   Maps the row number to a vector of strings; vector index is the annotation index.
    //  groupByRow = false:  Maps the annotation index to a vector of strings; vector index is the row number.
    map<unsigned int, vector<string> > indivAnnots;
    map<unsigned int, vector<string> >::iterator it, itEnd;

    for (; i < nRows; ++i) {
        if (structuresOnly) {
            if ((const_cast<CCdCore*>(m_cd))->GetPDB((int) i, pdbId)) {
                rows.push_back(i);
            }
        } else {
            rows.push_back(i);
        }
    }

    nRows = rows.size();

    for (i = 0; i < nRows; ++i) {
        row = rows[i];

        //  annots is a vector of size # of annotations; each row has the same number of annotations.
        MappedToSlaveString(row, annots, !groupByRow);
//        cout << "row " << i << " num annots " << annots.size() << endl;
        if (groupByRow) {
            indivAnnots[row] = annots;
        } else {
            for (j = 0; j < annots.size(); ++j) {
                indivAnnots[j].push_back(annots[j]);
            }
        }

    }

    it = indivAnnots.begin();
    itEnd = indivAnnots.end();
    bool firstTime = true;
    for (; it != itEnd; ++it) {
        vector<string>& annotStrs = it->second;
//        cout << "indiv it loop " << it->first << " num annot strings " << annotStrs.size() << endl;
        
        if (groupByRow) {

            result += MakeRowInfoString(it->first);
            for (i = 0; i < annotStrs.size(); ++i) {
                result += "    " + annotStrs[i];
            }
        } else {
            //  For first time through, add list of row identifiers
            if (firstTime) {
                result += "Mapped annotations for the following rows:\n";
                for (i = 0; i < annotStrs.size(); ++i) {
                    result += "    " + MakeRowInfoString(rows[i]);
                }
                result += "=============================================\n";
                firstTime = false;
            }
            result += "Annotation " + NStr::UIntToString(it->first) + ":\n";
            for (i = 0; i < annotStrs.size(); ++i) {
                result += "    " + annotStrs[i];
            }
        }
        result += "\n";
    }

    return result;
}


void CCdAnnotationInfo::RangeStringsForAnnots(const CdAnnotMap& rowAnnots, vector<string>& rangeStrings) const
{
    CdAnnotMapCit rowIt = rowAnnots.begin(), rowEnd = rowAnnots.end();

    rangeStrings.clear();
    for (; rowIt != rowEnd; ++rowIt) {
        const vector<FromToPair>& pairs = rowIt->second;
        rangeStrings.push_back(PairsToString(pairs, m_useHyphenatedRanges));
    }
}

bool CCdAnnotationInfo::MapRangesForRow(unsigned int row, CdAnnotMap& slaveRanges) const
{
    bool result = true;

    if (m_slaveAnnotMap.size() == 0) {
        cout << "    Can't map ranges; set 2nd parameter in ctor to false to initialize slave row mappings." << endl;
        return false;
    }

    //  the columns in m_slaveAnnotMap are indexed by the Seq_align number (row 1 => seq-align 0)
    unsigned int rowMinusOne = row - 1;   

    unsigned int annotIndex, from, to, nPairs, pair;
    CdAnnotPosMapCit foundFromIt, foundToIt, slaveMapEnd = m_slaveAnnotMap.end();
    CdAnnotMapCit cit = m_masterAnnotMap.begin(), citEnd = m_masterAnnotMap.end();

    slaveRanges.clear();
    for (; cit != citEnd; ++cit) {
        annotIndex = cit->first;
        const vector<FromToPair>& pairs = cit->second;
        nPairs = pairs.size();
//        cout << "        master pos = " << annotIndex << "; #pairs = " << nPairs << endl;

        vector<FromToPair>& slavePairs = slaveRanges[annotIndex];
        for (pair = 0; pair < nPairs; ++pair) {
//            cout << "        retrieve positions on row " << row << " for master pair [" << pairs[pair].first << ", " << pairs[pair].second << "]:  ";
            if (row > 0) {
                from = kMax_UInt;
                to   = kMax_UInt;
                foundFromIt = m_slaveAnnotMap.find(pairs[pair].first);
                foundToIt = m_slaveAnnotMap.find(pairs[pair].second);
                if (foundFromIt != slaveMapEnd && foundToIt != slaveMapEnd) {

                    const vector<unsigned int>& fromCols = foundFromIt->second;
                    if (rowMinusOne < fromCols.size()) from = fromCols[rowMinusOne];

                    const vector<unsigned int>& toCols = foundToIt->second;
                    if (rowMinusOne < toCols.size()) to = toCols[rowMinusOne];
//                    cout << " --> [" << from << ", " << to << "]\n";
//                } else {
//                    cout << " --> no mapping made\n";
                }
            } else {
                from = pairs[pair].first;
                to = pairs[pair].second;
//                cout << " --> [" << from << ", " << to << "]\n";
            }

            if (from == kMax_UInt || to == kMax_UInt) result = false;

            slavePairs.push_back(FromToPair(from, to));
        }
    }
    return result;
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
