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
    void ConvertFromOMSSA(CMSSearch& inOMSSA, CRef <CMSModSpecSet> Modset, string basename);

private:
    // Prohibit copy constructor and assignment operator
    CPepXML(const CPepXML& value);
    CPepXML& operator=(const CPepXML& value);

    char ConvertAA(char in);
    CRef<CModification_info> ConvertModifications(CRef<CMSHits> msHits, CRef<CMSModSpecSet> Modset);
    void ConvertModSetting(CRef<CSearch_summary> sSum, CRef<CMSModSpecSet> Modset, int modnum, bool fixed);
    string ParseScan(string SpecID, int field, string def);
    string GetProteinName(CRef<CMSPepHit> pHit);

    typedef pair<char, double> TAminoAcidMassPair;
    typedef map<char, double> TAminoAcidMassMap;
    
    TAminoAcidMassMap aaMassMap;

};


END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // MSMERGE_HPP
