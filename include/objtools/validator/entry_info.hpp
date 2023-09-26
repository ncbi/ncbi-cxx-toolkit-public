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
 *`
 * Author:  Justin Foley
 *
 * File Description:
 *   .......
 *
 */

#ifndef _VALIDATOR_ENTRY_INFO_HPP_
#define _VALIDATOR_ENTRY_INFO_HPP_

#include <corelib/ncbistd.hpp>
#include <optional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

class NCBI_VALIDATOR_EXPORT CValidatorEntryInfo {

public:
    void SetNoPubs(bool val=true);
    void SetNoCitSubPubs(bool val=true);
    void SetNoBioSource(bool val=true);
    void SetGPS(bool val=true);
    void SetGED(bool val=true);
    void SetPDB(bool val=true);
    void SetPatent(bool val=true);
    void SetRefSeq(bool val=true);
    void SetEmbl(bool val=true);
    void SetDdbj(bool val=true);
    void SetTPE(bool val=true);
    void SetNC(bool val=true);
    void SetNG(bool val=true);
    void SetNM(bool val=true);
    void SetNP(bool val=true);
    void SetNR(bool val=true);
    void SetNS(bool val=true);
    void SetNT(bool val=true);
    void SetNW(bool val=true);
    void SetNZ(bool val=true);
    void SetWP(bool val=true);
    void SetXR(bool val=true);
    void SetGI(bool val=true);
    void SetGpipe(bool val=true);
    void SetLocalGeneralOnly(bool val=true);
    void SetGiOrAccnVer(bool val=true);
    void SetGenomic(bool val=true);
    void SetSeqSubmit(bool val=true);
    void SetSmallGenomeSet(bool val=true);
    void SetGenbank(bool val=true);
    void SetFeatLocHasGI(bool val=true);
    void SetProductLocHasGI(bool val=true);
    void SetGeneHasLocusTag(bool val=true);
    void SetProteinHasGeneralID(bool val=true);
    void SetINSDInSep(bool val=true);
    void SetGeneious(bool val=true);

    bool IsNoPubs() const;
    bool IsNoCitSubPubs() const;
    bool IsNoBioSource() const;
    bool IsGPS() const;
    bool IsGED() const;
    bool IsPDB() const;
    bool IsPatent() const;
    bool IsRefSeq() const;
    bool IsEmbl() const;
    bool IsDdbj() const;
    bool IsTPE() const;
    bool IsGI() const;
    bool IsGpipe() const;
    bool IsHtg() const;
    bool IsLocalGeneralOnly() const;
    bool HasGiOrAccnVer() const;
    bool IsGenomic() const;
    bool IsSeqSubmit() const;
    bool IsSmallGenomeSet() const;
    bool IsGenbank() const;
    bool DoesAnyFeatLocHaveGI() const;
    bool DoesAnyProductLocHaveGI() const;
    bool DoesAnyGeneHaveLocusTag() const;
    bool DoesAnyProteinHaveGeneralID() const;
    bool IsINSDInSep() const;
    bool IsGeneious() const;

private:
    bool m_NoPubs=false;                  // Suppress no pub error if true
    bool m_NoCitSubPubs=false;            // Suppress no cit-sub pub error if true
    bool m_NoBioSource=false;             // Suppress no organism error if true
    bool m_IsGPS=false;
    bool m_IsGED=false;
    bool m_IsPDB=false;
    bool m_IsPatent=false;
    bool m_IsRefSeq=false;
    bool m_IsEmbl=false;
    bool m_IsDdbj=false;
    bool m_IsTPE=false;
    bool m_IsGI=false;
    bool m_IsGB=false;
    bool m_IsGpipe=false;
    bool m_IsLocalGeneralOnly=true;
    bool m_HasGiOrAccnVer=false;
    bool m_IsGenomic=false;
    bool m_IsSeqSubmit=false;
    bool m_IsSmallGenomeSet=false;
    bool m_FeatLocHasGI=false;
    bool m_ProductLocHasGI=false;
    bool m_GeneHasLocusTag=false;
    bool m_ProteinHasGeneralID=false;
    bool m_IsINSDInSep=false;
    bool m_FarFetchFailure=false;
    bool m_IsGeneious=false;
};

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
