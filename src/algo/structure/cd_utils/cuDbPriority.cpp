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
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Utility class to define and manage the ranking of the typical database sources found
 *       in NCBI Bioseqs and Seq_ids.  Use functions in cuSequence to query with a particular sequence.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algorithm>
#include <corelib/ncbistr.hpp>
#include <algo/structure/cd_utils/cuDbPriority.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

CSafeStatic<CCdDbPriority::TSourcePriorityMap> CCdDbPriority::m_sourcePriorityMap;
CSafeStatic< CCdDbPriority::TSourceNameMap>    CCdDbPriority::m_sourceNameMap;
CSafeStatic< CCdDbPriority::TNameSourceMap>    CCdDbPriority::m_nameSourceMap;

void CCdDbPriority::Initialize() 
{

    //  Priority assignments
    if (m_sourcePriorityMap->size() == 0) {

        auto& m_ = m_sourcePriorityMap.Get();

        //  Top Tier
        m_[eDPPdb] = eTopTier;

        //  Tier 1
        m_[eDPSwissprot] = eTier1;

        //  Tier 2
        m_[eDPGenbank] = eTier2;
        m_[eDPEmbl] = eTier2;
        m_[eDPDdbj] = eTier2;
        m_[eDPPrf] = eTier2;
        m_[eDPPir] = eTier2;
        m_[eDPRefSeqCurated] = eTier2;
        m_[eDPRefSeqNP] = eTier2;
        m_[eDPRefSeqAP] = eTier2;

        //  Tier 3
        m_[eDPRefSeqAny] = eTier3;   // undifferentiated Refseq considered to be curated
        m_[eDPRefSeqAutomated] = eTier3;
        m_[eDPRefSeqXP] = eTier3;
        m_[eDPRefSeqYP] = eTier3;
        m_[eDPRefSeqWP] = eTier3;
        m_[eDPLocal] = eTier3;
        m_[eDPGibbsq] = eTier3;
        m_[eDPGibbmt] = eTier3;
        m_[eDPGiim] = eTier3;
        m_[eDPPatent] = eTier3;
        m_[eDPTPGenbank] = eTier3;
        m_[eDPTPEmbl] = eTier3;
        m_[eDPTPDdbj] = eTier3;
        m_[eDPGpipe] = eTier3;
        m_[eDPGeneral] = eTier3;
        m_[eDPUnsupported] = eTier3;   //  not unknown, but may not know what to do w/ it

        //  Tier 4 
        m_[eDPRefSeqZP] = eTier4;   //  ZP sequences obsoleted as of May 2013

        //  Bottom Tier
        m_[eDPGi] = eBottomTier;   //  this is here since this doesn't specify a database source
        m_[eDPUnknown] = eBottomTier;
    }


    if (m_nameSourceMap->size() == 0) {

        auto& m_ = m_nameSourceMap.Get();

        m_["gi"] = eDPGi;
        m_["pdb"] = eDPPdb;
        m_["swissprot"] = eDPSwissprot;
        m_["genbank"] = eDPGenbank;
        m_["embl"] = eDPEmbl;
        m_["ddbj"] = eDPDdbj;
        m_["prf"] = eDPPrf;
        m_["pir"] = eDPPir;
        m_["refseq"] = eDPRefSeqAny;
        m_["curated refseq"] = eDPRefSeqCurated;
        m_["refseq: curated"] = eDPRefSeqNP;
        m_["refseq: alternate"] = eDPRefSeqAP;
        m_["automated refseq"] = eDPRefSeqAutomated;
        m_["refseq: automated"] = eDPRefSeqXP;
        m_["refseq: automated/no transcript"] = eDPRefSeqYP;
        m_["refseq: automated/shotgun"] = eDPRefSeqWP;
        m_["refseq: automated/shotgun [obsolete]"] = eDPRefSeqZP;
        m_["local"] = eDPLocal;
        m_["gibbsq"] = eDPGibbsq;
        m_["gibbmt"] = eDPGibbmt;
        m_["giim"] = eDPGiim;
        m_["patent"] = eDPPatent;
        m_["thirdpartyGenbank"] = eDPTPGenbank;
        m_["thirdpartyEmbl"] = eDPTPEmbl;
        m_["thirdpartyDdbj"] = eDPTPDdbj;
        m_["gpipe"] = eDPGpipe;

        //  Not sure what these will be a priori; will have to supplement this....
        m_["general"] = eDPGeneral;

        //  May want to move some of the above here vs. giving them their own 'source'.
        m_["unsupported"]         = eDPUnsupported;

        //  Flags unexpected conditions...
        m_["unknown"] = eDPUnknown;
    }

    //  Use the above values to fill in the multi-map.
    if (m_sourceNameMap->size() == 0) {
        for (TNameSourceMap::iterator it = m_nameSourceMap->begin(); it != m_nameSourceMap->end(); ++it) {
            m_sourceNameMap->insert(TSourceNameVT(it->second, it->first));
        }
    }
}

CCdDbPriority::EDbSource CCdDbPriority::SeqIdTypeToSourceCode(unsigned int seqIdType, string accession)
{
                        /*  From Seq_id_base (6/12/06): 
                        enum E_Choice {
                                e_not_set = 0,  ///< No variant selected
                        1        e_Local,        ///< Variant Local is selected.
                        2        e_Gibbsq,       ///< Variant Gibbsq is selected.
                        3        e_Gibbmt,       ///< Variant Gibbmt is selected.
                        4        e_Giim,         ///< Variant Giim is selected.
                        5        e_Genbank,      ///< Variant Genbank is selected.
                        6        e_Embl,         ///< Variant Embl is selected.
                        7        e_Pir,          ///< Variant Pir is selected.
                        8        e_Swissprot,    ///< Variant Swissprot is selected.
                        9        e_Patent,       ///< Variant Patent is selected.
                        10       e_Other,        ///< Variant Other is selected.   ***  considered a generic refseq
                        11        e_General,      ///< Variant General is selected.
                        12       e_Gi,           ///< Variant Gi is selected.
                        13        e_Ddbj,         ///< Variant Ddbj is selected.
                        14        e_Prf,          ///< Variant Prf is selected.
                        15        e_Pdb,          ///< Variant Pdb is selected.
                        16        e_Tpg,          ///< Variant Tpg is selected.
                        17        e_Tpe,          ///< Variant Tpe is selected.
                        18        e_Tpd,          ///< Variant Tpd is selected.
                        19        e_Gpipe         ///< Variant Gpipe is selected.
                            };
                            */
    EDbSource p;
    switch (seqIdType) {
        case 1:  //e_Local
            p = eDPLocal;
            break;
        case 2:  //e_Gibbsq
            p = eDPGibbsq;
            break;
        case 3:  // e_Gibbmt
            p = eDPGibbmt;
            break;
        case 4:  // e_Giim
            p = eDPGiim;
            break;
        case 5:  // e_Genbank
            p = eDPGenbank;
            break;
        case 6:  // e_Embl
            p = eDPEmbl;
            break;
        case 7:  // e_Pir
            p = eDPPir;
            break;
        case 8:  // e_Swissprot
            p = eDPSwissprot;
            break;
        case 9:  // e_Patent
            p = eDPPatent;
            break;
        case 10:  // e_Other
            p = eDPRefSeqAny;
            //  Look for a more exact match, if desired.
            if (accession.length() > 0) {
                string refseqPrefix = accession.substr(0,2);
                if (NStr::CompareNocase(refseqPrefix, "XP") == 0) {
                    p = eDPRefSeqXP;
                } else if (NStr::CompareNocase(refseqPrefix, "YP") == 0) {
                    p = eDPRefSeqYP;
                } else if (NStr::CompareNocase(refseqPrefix, "WP") == 0) {
                    p = eDPRefSeqWP;
                } else if (NStr::CompareNocase(refseqPrefix, "ZP") == 0) {
                    p = eDPRefSeqZP;
                } else {
                    p = eDPRefSeqCurated;
                    if (NStr::CompareNocase(refseqPrefix, "NP") == 0) {
                        p = eDPRefSeqNP;
                    } else if (NStr::CompareNocase(refseqPrefix, "AP") == 0) {
                        p = eDPRefSeqAP;
                    }
                }
            }
            break;
        case 11:  // e_General
            p = eDPGeneral;
            break;
        case 12:  // e_Gi
            p = eDPGi;
            break;
        case 13:  // e_Ddbj
            p = eDPDdbj;
            break;
        case 14:  // e_Prf
            p = eDPPrf;
            break;
        case 15:  // e_Pdb
            p = eDPPdb;
            break;
        case 16:  // e_Tpg
            p = eDPTPGenbank;
            break;
        case 17:  // e_Tpe
            p = eDPTPEmbl;
            break;
        case 18:  // e_Tpd
            p = eDPTPDdbj;
            break;
        case 19:  // e_Gpipe
            p = eDPGpipe;
            break;
        default:
            p = eDPUnknown;
            break;
    }

    return p;
}

string CCdDbPriority::SeqIdTypeToSource(unsigned int seqIdType, string accession)
{
    EDbSource sourceCode = SeqIdTypeToSourceCode(seqIdType, accession);
    return GetSourceName(sourceCode);
}

CCdDbPriority::TDbPriority CCdDbPriority::SeqIdTypeToPriority(unsigned int seqIdType, string accession)
{
    EDbSource s = SeqIdTypeToSourceCode(seqIdType, accession);
    TSourcePriorityIt pit = m_sourcePriorityMap->find(s);    
    return (pit != m_sourcePriorityMap->end() ? pit->second : eBottomTier);
}

string CCdDbPriority::GetSourceName(EDbSource priority)
{
    Initialize();

    string name = "unknown";
    TSourceNameMap::iterator it = m_sourceNameMap->find(priority);

    if (it != m_sourceNameMap->end()) {
        name = it->second;
    } else {
        it = m_sourceNameMap->find(eDPUnknown);
        if (it != m_sourceNameMap->end()) name = it->second;
    }
    return name;  
}

unsigned int CCdDbPriority::GetSourceNames(EDbSource priority, vector<string>& names)
{
    names.clear();
    Initialize();

    TSourceNameMap::iterator it;
    typedef pair<TSourceNameMap::iterator, TSourceNameMap::iterator> TPair;
    TPair p = m_sourceNameMap->equal_range(priority);

    for (it = p.first; it != p.second; ++it) {
        names.push_back(it->second);
    }
    return static_cast<unsigned int>(names.size());
}

bool CCdDbPriority::IsKnownDbSource(string dbSource)
{
    Initialize();
    return m_nameSourceMap->count(dbSource) > 0;
}


CCdDbPriority::TDbPriority CCdDbPriority::GetPriority(string dbSource)
{
    Initialize();

    EDbSource sourceCode = GetSourceCode(dbSource);
    TSourcePriorityIt pit = m_sourcePriorityMap->find(sourceCode);
    return (pit != m_sourcePriorityMap->end()) ? pit->second : eBottomTier;
}

CCdDbPriority::EDbSource CCdDbPriority::GetSourceCode(string dbSource)
{
    TNameSourceMap::iterator it = m_nameSourceMap->find(dbSource);
    return (it != m_nameSourceMap->end()) ? it->second : eDPUnknown;
}

int CCdDbPriority::CompareSources(const string& source1, const string& source2) 
{
    //  When an unknown source string is encountered, eBottomTier is returned by GetPriority.
    TDbPriority prio1 = GetPriority(source1);
    TDbPriority prio2 = GetPriority(source2);

    //  Better priority is a smaller number!!!
    return (prio1 == prio2) ? 0 : ((prio1 < prio2) ? 1 : -1);
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
