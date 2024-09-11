#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_REPORT_ENTRY_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_REPORT_ENTRY_HPP_
/*****************************************************************************
 *  $Id$
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
 *  IPG report entry
 *
 *****************************************************************************/

#include <corelib/ncbistd.hpp>

#include <list>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <corelib/ncbitime.hpp>

#include <objects/seqfeat/BioSource.hpp>
#include <objtools/pubseq_gateway/impl/ipg/ipg_types.hpp>
#include <sra/readers/sra/wgsread.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(ipg)

struct CIpgStorageReportEntry
{
 public:
    using TWeights = TIpgWeights;
    CIpgStorageReportEntry() = default;
    CIpgStorageReportEntry(CIpgStorageReportEntry const &) = default;
    CIpgStorageReportEntry(CIpgStorageReportEntry &&) = default;

    CIpgStorageReportEntry& operator=(CIpgStorageReportEntry const &) = default;
    CIpgStorageReportEntry& operator=(CIpgStorageReportEntry&&) = default;

    CIpgStorageReportEntry& SetIpg(TIpg value) {
        m_Ipg = value;
        return *this;
    }

    CIpgStorageReportEntry& SetAccession(string const& value) {
        m_Accession = value;
        return *this;
    }

    CIpgStorageReportEntry& SetAccession(string&& value) {
        m_Accession = std::move(value);
        return *this;
    }

    CIpgStorageReportEntry& SetCds(TIpgCds value) {
        m_Cds = value;
        return *this;
    }

    CIpgStorageReportEntry& SetLength(Int4 value) {
        m_Length = value;
        return *this;
    }

    CIpgStorageReportEntry& SetNucAccession(string const& value) {
        m_NucAccession = value;
        return *this;
    }

    CIpgStorageReportEntry& SetNucAccession(string&& value) {
        m_NucAccession = std::move(value);
        return *this;
    }

    CIpgStorageReportEntry& SetProductName(string const& value) {
        m_ProductName = value;
        return *this;
    }

    CIpgStorageReportEntry& SetProductName(string&& value) {
        m_ProductName = std::move(value);
        return *this;
    }

    CIpgStorageReportEntry& SetStrain(string const& value) {
        m_Strain = value;
        return *this;
    }

    CIpgStorageReportEntry& SetStrain(string&& value) {
        m_Strain = std::move(value);
        return *this;
    }

    CIpgStorageReportEntry& SetSrcDb(Int4 value) {
        m_SrcDb = value;
        return *this;
    }

    CIpgStorageReportEntry& SetTaxid(TTaxId value) {
        m_Taxid = value;
        return *this;
    }

    CIpgStorageReportEntry& SetRefseq(list<string> const& value) {
        m_Refseq = value;
        return *this;
    }

    CIpgStorageReportEntry& SetRefseq(list<string> && value) {
        swap(m_Refseq, value);
        return *this;
    }

    CIpgStorageReportEntry& SetDiv(string const& value) {
        m_Div = value;
        return *this;
    }

    CIpgStorageReportEntry& SetDiv(string&& value) {
        m_Div = std::move(value);
        return *this;
    }

    CIpgStorageReportEntry& SetAssembly(string const& value) {
        m_Assembly = value;
        return *this;
    }

    CIpgStorageReportEntry& SetAssembly(string&& value) {
        m_Assembly = std::move(value);
        return *this;
    }

    CIpgStorageReportEntry& SetGbState(TGbState value) {
        m_GbState = value;
        return *this;
    }

    CIpgStorageReportEntry& SetUpdated(CTime value) {
        m_Updated = value;
        return *this;
    }

    CIpgStorageReportEntry& SetCreated(CTime value) {
        m_Created = value;
        return *this;
    }

    CIpgStorageReportEntry& SetWriteTime(CTime value) {
        m_WriteTime = value;
        return *this;
    }

    CIpgStorageReportEntry& SetProtGi(TGi& value) {
        m_ProtGi = value;
        return *this;
    }

    CIpgStorageReportEntry& SetWeights(TIpgWeights&& value) {
        m_Weights = std::move(value);
        return *this;
    }

    CIpgStorageReportEntry& SetWeights(TIpgWeights const& value) {
        m_Weights = value;
        return *this;
    }

    CIpgStorageReportEntry& SetBioProject(string const& value) {
        m_BioProject = value;
        return *this;
    }

    CIpgStorageReportEntry& SetBioProject(string&& value) {
        m_BioProject = std::move(value);
        return *this;
    }

    CIpgStorageReportEntry& SetGenome(objects::CBioSource::EGenome value) {
        m_Genome = value;
        return *this;
    }

    CIpgStorageReportEntry& SetPartial(bool value) {
        x_SetFlag(value, EIpgProteinFlags::ePartial);
        return *this;
    }

    CIpgStorageReportEntry& SetRemote(bool value=true) {
        x_SetFlag(value, EIpgProteinFlags::eRemote);
        return *this;
    }

    CIpgStorageReportEntry& SetUnverified(bool value=true) {
        x_SetFlag(value, EIpgProteinFlags::eUnverified);
        return *this;
    }

    CIpgStorageReportEntry& SetMatPeptide(bool value=true) {
        x_SetFlag(value, EIpgProteinFlags::eMatPeptide);
        return *this;
    }

    CIpgStorageReportEntry& SetFlags(Int4 value) {
        m_Flags = value;
        return *this;
    }

    CIpgStorageReportEntry& SetSatkey(Int4 value) {
        m_Satkey = value;
        return *this;
    }

    CIpgStorageReportEntry& SetPatent(bool value) {
        m_IsPatent = value;
        return *this;
    }

    CIpgStorageReportEntry& AddPubMedId(int id) {
        m_PubMedIds.insert(id);
        return *this;
    }

    CIpgStorageReportEntry& SetPubMedIds(TPubMedIds&& ids) {
        m_PubMedIds = std::move(ids);
        return *this;
    }

    CIpgStorageReportEntry& SetPubMedIds(TPubMedIds const & ids) {
        m_PubMedIds = ids;
        return *this;
    }

    CIpgStorageReportEntry& SetDefLine(string const & defline) {
        m_DefLine = defline;
        return *this;
    }

    CIpgStorageReportEntry& SetDefLine(string&& value) {
        m_DefLine = std::move(value);
        return *this;
    }

//-----------------------------------------

    inline bool IsValid() const {
        return m_Length > 0;
    }

    TIpg GetIpg() const {
        return m_Ipg;
    }

    string const& GetAccession() const {
        return m_Accession;
    }

    TIpgCds const& GetCds() const {
        return m_Cds;
    }

    Int4 GetLength() const {
        return m_Length;
    }

    string const& GetNucAccession() const {
        return m_NucAccession;
    }

    string const& GetProductName() const {
        return m_ProductName;
    }

    Int4 GetSrcDb() const {
        return m_SrcDb;
    }

    TTaxId GetTaxid() const {
        return m_Taxid;
    }

    list<string> const& GetRefseq() const {
        return m_Refseq;
    }

    string const& GetDiv() const {
        return m_Div;
    }

    string const& GetAssembly() const {
        return m_Assembly;
    }

    string const& GetStrain() const {
        return m_Strain;
    }

    TIpgWeights const& GetWeights() const {
        return m_Weights;
    }

    TGbState GetGbState() const {
        return m_GbState;
    }

    CTime GetUpdated() const {
        return m_Updated;
    }

    CTime GetCreated() const {
        return m_Created;
    }

    CTime GetWriteTime() const {
        return m_WriteTime;
    }

    TGi GetProtGi() const {
        return m_ProtGi;
    }

    string const& GetBioProject() const {
        return m_BioProject;
    }

    objects::CBioSource::EGenome GetGenome() const {
        return m_Genome;
    }

    bool GetPartial() const {
        return x_GetFlag(EIpgProteinFlags::ePartial);
    }

    bool GetRemote() const {
        return x_GetFlag(EIpgProteinFlags::eRemote);
    }

    bool GetUnverified() const {
        return x_GetFlag(EIpgProteinFlags::eUnverified);
    }

    bool GetMatPeptide() const {
        return x_GetFlag(EIpgProteinFlags::eMatPeptide);
    }

    Int4 GetFlags() const {
        return m_Flags;
    }

    Int4 GetSatkey() const {
        return m_Satkey;
    }

    bool IsPatent() const {
        return m_IsPatent;
    }

    bool HasPubMed() const {
        return !m_PubMedIds.empty();
    }

    const TPubMedIds& GetPubMedLinks() const {
        return m_PubMedIds;
    }

    string const& GetDefLine() const {
        return m_DefLine.empty() ? m_ProductName : m_DefLine;
    }

 private:
    void x_SetFlag(bool set_flag, EIpgProteinFlags flag_value) {
        if (set_flag) {
            m_Flags |= static_cast<Int4>(flag_value);
        } else {
            m_Flags &= ~(static_cast<Int4>(flag_value));
        }
    }

    bool x_GetFlag(EIpgProteinFlags flag_value) const {
        return m_Flags & static_cast<Int4>(flag_value);
    }

    TIpg m_Ipg{TIpg()};
    string m_Accession;
    string m_NucAccession;

    string m_ProductName;
    string m_Div;
    string m_Assembly;
    string m_Strain;
    string m_BioProject;
    // BD-811,BD-814 : defline cannot in general be derived from product name,
    // so it's saved in a separate column.
    string m_DefLine;
    list<string> m_Refseq;
    TWeights m_Weights;

    // ID-4390 : needed to support restricting BLAST searches by IPGs which are
    // referenced in PubMed articles.
    TPubMedIds m_PubMedIds;

    // Used for delayed accession and length batch retrieval for
    // FAR-pointer proteins.
    TGi m_ProtGi{ZERO_GI};

    CTime m_Updated;
    CTime m_Created;
    CTime m_WriteTime;

    TTaxId m_Taxid{ZERO_TAX_ID};
    Int4 m_Length{0};
    Int4 m_Flags{0};  // Added as decided in ID-3941
    Int4 m_Satkey{0};  // Needed for failure reporting
    Int4 m_SrcDb{0};

    TGbState m_GbState{NCBI_gb_state_eWGSGenBankLive};
    objects::CBioSource::EGenome m_Genome{objects::CBioSource::eGenome_unknown};

    TIpgCds m_Cds;

    // ID-4118 : for nuc-prots, patent Seq-id is present on nucleotides only,
    // so need to pass this info to the protein entry, however his is NOT
    // intended to be saved in Cassandra.
    bool m_IsPatent{false};
};

using CPubseqGatewayIpgReportConsumeCallback = function<bool(vector<CIpgStorageReportEntry> &&, bool is_last)>;

END_SCOPE(ipg)
END_NCBI_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_REPORT_ENTRY_HPP_
