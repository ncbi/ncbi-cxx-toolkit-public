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
 *  and reliability of the software and data, the NLM 
 *  Although all reasonable efforts have been taken to ensure the accuracyand the U.S.
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

#ifndef PEPXML_HPP
#define PEPXML_HPP

#include <algo/ms/formats/pepxml/pepXML__.hpp>
#include <objects/omssa/omssa__.hpp>
#include "msmerge.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)


class NCBI_XOMSSA_EXPORT CPepXML : public CMsms_pipeline_analysis {

    typedef CMsms_pipeline_analysis Tparent;

public:

    // constructor
    CPepXML(void) {}
    // destructor
    ~CPepXML(void) {}

    /**
     * convert OMSSA to PepXML
     * 
     * @param inOMSSA the ASN.1 structure to search to be copied
     */
    void ConvertFromOMSSA(CMSSearch& inOMSSA, CRef <CMSModSpecSet> Modset, string basename, string newname);

private:
    // Prohibit copy constructor and assignment operator
    CPepXML(const CPepXML& value);
    CPepXML& operator=(const CPepXML& value);

    string ConvertDouble(double n);
    char ConvertAA(char in);
    CRef<CModification_info> ConvertModifications(CRef<CMSHits> msHits, CRef<CMSModSpecSet> Modset, set<int>& vModSet,
                                                  CMSSearch& inOMSSA);
    void ConvertModSetting(CRef<CSearch_summary> sSum, CRef<CMSModSpecSet> Modset, int modnum, bool fixed);
    void ConvertScanID(CRef<CSpectrum_query> sQuery, string SpecID, int query, int charge);
    string GetProteinName(CRef<CMSPepHit> pHit);
    void ConvertMSHitSet(CRef<CMSHitSet> pHitSet, CMsms_run_summary::TSpectrum_query& sQueries, CRef<CMSModSpecSet> Modset, set<int>& variableMods, CMSSearch& inOMSSA);

    typedef pair<char, double> TAminoAcidMassPair;
    typedef map<char, double> TAminoAcidMassMap;
    
    TAminoAcidMassMap m_aaMassMap;
    set<char> m_staticModSet;

    float m_scale;
    int m_index;
};


END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // MSMERGE_HPP
