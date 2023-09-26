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
 * Author:  Justin Foley
 *
 * File Description:
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/validator/entry_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

void CValidatorEntryInfo::SetNoPubs(bool val) { m_NoPubs = val; }
void CValidatorEntryInfo::SetNoCitSubPubs(bool val) { m_NoCitSubPubs = val; }
void CValidatorEntryInfo::SetNoBioSource(bool val) { m_NoBioSource = val; }
void CValidatorEntryInfo::SetGPS(bool val) { m_IsGPS = val; }
void CValidatorEntryInfo::SetGED(bool val) { m_IsGED = val; }
void CValidatorEntryInfo::SetPDB(bool val) { m_IsPDB = val; }
void CValidatorEntryInfo::SetPatent(bool val) { m_IsPatent = val; }
void CValidatorEntryInfo::SetRefSeq(bool val) { m_IsRefSeq = val; }
void CValidatorEntryInfo::SetEmbl(bool val) { m_IsEmbl = val; }
void CValidatorEntryInfo::SetDdbj(bool val) { m_IsDdbj = val; }
void CValidatorEntryInfo::SetTPE(bool val) { m_IsTPE = val; }
void CValidatorEntryInfo::SetGI(bool val) { m_IsGI = val; }
void CValidatorEntryInfo::SetGpipe(bool val) { m_IsGpipe = val; }
void CValidatorEntryInfo::SetLocalGeneralOnly(bool val) { m_IsLocalGeneralOnly = val; }
void CValidatorEntryInfo::SetGiOrAccnVer(bool val) { m_HasGiOrAccnVer = val; }
void CValidatorEntryInfo::SetGenomic(bool val) { m_IsGenomic = val; }
void CValidatorEntryInfo::SetSeqSubmit(bool val) { m_IsSeqSubmit = val; }
void CValidatorEntryInfo::SetSmallGenomeSet(bool val) { m_IsSmallGenomeSet = val; }
void CValidatorEntryInfo::SetGenbank(bool val) { m_IsGB = val; }
void CValidatorEntryInfo::SetFeatLocHasGI(bool val) { m_FeatLocHasGI = val; }
void CValidatorEntryInfo::SetProductLocHasGI(bool val) { m_ProductLocHasGI = val; }
void CValidatorEntryInfo::SetGeneHasLocusTag(bool val) { m_GeneHasLocusTag = val; }
void CValidatorEntryInfo::SetProteinHasGeneralID(bool val) { m_ProteinHasGeneralID = val; }
void CValidatorEntryInfo::SetINSDInSep(bool val) { m_IsINSDInSep = val; }
void CValidatorEntryInfo::SetGeneious(bool val) { m_IsGeneious = val; }

bool CValidatorEntryInfo::IsNoPubs() const { return m_NoPubs; }
bool CValidatorEntryInfo::IsNoCitSubPubs() const { return m_NoCitSubPubs; }
bool CValidatorEntryInfo::IsNoBioSource() const { return m_NoBioSource; }
bool CValidatorEntryInfo::IsGPS() const { return m_IsGPS; }
bool CValidatorEntryInfo::IsGED() const { return m_IsGED; }
bool CValidatorEntryInfo::IsPDB() const { return m_IsPDB; }
bool CValidatorEntryInfo::IsPatent() const { return m_IsPatent; }
bool CValidatorEntryInfo::IsRefSeq() const { return m_IsRefSeq; }
bool CValidatorEntryInfo::IsEmbl() const { return m_IsEmbl; }
bool CValidatorEntryInfo::IsDdbj() const { return m_IsDdbj; }
bool CValidatorEntryInfo::IsTPE() const { return m_IsTPE; }
bool CValidatorEntryInfo::IsGI() const { return m_IsGI; }
bool CValidatorEntryInfo::IsGpipe() const { return m_IsGpipe; }
bool CValidatorEntryInfo::IsLocalGeneralOnly() const { return m_IsLocalGeneralOnly; }
bool CValidatorEntryInfo::HasGiOrAccnVer() const { return m_HasGiOrAccnVer; }
bool CValidatorEntryInfo::IsGenomic() const { return m_IsGenomic; }
bool CValidatorEntryInfo::IsSeqSubmit() const { return m_IsSeqSubmit; }
bool CValidatorEntryInfo::IsSmallGenomeSet() const { return m_IsSmallGenomeSet; }
bool CValidatorEntryInfo::IsGenbank() const { return m_IsGB; }
bool CValidatorEntryInfo::DoesAnyFeatLocHaveGI() const { return m_FeatLocHasGI; }
bool CValidatorEntryInfo::DoesAnyProductLocHaveGI() const { return m_ProductLocHasGI; }
bool CValidatorEntryInfo::DoesAnyGeneHaveLocusTag() const { return m_GeneHasLocusTag; }
bool CValidatorEntryInfo::DoesAnyProteinHaveGeneralID() const { return m_ProteinHasGeneralID; }
bool CValidatorEntryInfo::IsINSDInSep() const { return m_IsINSDInSep; }
bool CValidatorEntryInfo::IsGeneious() const { return m_IsGeneious; }

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
