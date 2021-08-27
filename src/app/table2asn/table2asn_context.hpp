#ifndef __TABLE2ASN_CONTEXT_HPP_INCLUDED__
#define __TABLE2ASN_CONTEXT_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>
#include <objtools/edit/gaps_edit.hpp>
#include <misc/discrepancy/discrepancy.hpp>

#include "suspect_feat.hpp"

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CBioseq_set;
class CSeq_descr;
class ILineErrorListener;
class CUser_object;
class CBioSource;
class CScope;
class CObjectManager;
class CSeq_entry_EditHandle;
class CSeq_feat;
class CSourceModParser;
class CSeq_id;
class CFeat_CI;

namespace edit
{
    class CRemoteUpdater;
}

}

class CMemorySrcFileMap;

#include <objects/seq/Seqdesc.hpp>

template<typename _Stream, size_t _bufsize=8192*2>
class CBufferedStream
{
public:
    CBufferedStream()
    {
    }
    operator bool() const
    {
        return m_buffer.get() != nullptr;
    }
    operator _Stream&()
    {
        return get();
    }
    _Stream& get()
    {
        if (!m_buffer)
        {
            m_buffer.reset(new char[bufsize]);
            m_stream.rdbuf()->pubsetbuf(m_buffer.get(), bufsize);
        }
        return m_stream;
    }
    ~CBufferedStream()
    {
    }
private:
    static constexpr size_t bufsize = _bufsize;
    unique_ptr<char> m_buffer;
    _Stream m_stream;
};

using CBufferedOutput = CBufferedStream<CNcbiOfstream>;
using CBufferedInput  = CBufferedStream<CNcbiIfstream>;

// command line parameters are mapped into the context
// those with only only symbol still needs to be implemented
class CTable2AsnContext
{
public:
    string m_current_file;
    string m_ResultsDirectory;
    CNcbiOstream* m_output{ nullptr };
    string m_output_filename;
    string m_base_name;
    CRef<objects::CSeq_id> m_accession;
    string m_OrganismName;
    string m_single_source_qual_file;
    string m_Comment;
    string m_single_annot_file;
    string m_ft_url;
    string m_ft_url_mod;
    CTime m_HoldUntilPublish;
    string F;
    string A;
    string m_genome_center_id;
    string m_validate;
    bool   m_delay_genprodset { false };
    string G;
    bool   R{ false };
    bool   S{ false };
    string Q;
    bool   L{ false };
    bool   W{ false };
    bool   m_t{ false };
    bool   m_save_bioseq_set{ false };
    string c;
    string zOufFile;
    string X;
    string m_master_genome_flag;
    string m;
    string m_cleanup;
    string m_single_structure_cmt;
    string m_ProjectVersionNumber;
    string m_disc_lineage;
    bool   m_RemoteTaxonomyLookup{ false };
    bool   m_RemotePubLookup{ false };
    bool   m_HandleAsSet{ false };
    objects::CBioseq_set::TClass m_ClassValue{ objects::CBioseq_set::eClass_genbank };
    bool   m_SetIDFromFile{ false };
    //int    m_taxid;
    TSeqPos m_gapNmin{ 0 };
    TSeqPos m_gap_Unknown_length{ 0 };
    TSeqPos m_minimal_sequence_length{ 0 };
    objects::CGapsEditor::TEvidenceSet m_DefaultEvidence;
    objects::CGapsEditor::TCountToEvidenceMap m_GapsizeToEvidence;

    int    m_gap_type{ -1 };
    bool   m_fcs_trim{ false };
    bool   m_split_log_files{ false };
    bool   m_postprocess_pubs{ false };
    string m_asn1_suffix;
    string m_locus_tag_prefix;
    bool   m_locus_tags_needed;
    bool   m_use_hypothetic_protein{ false };
    bool   m_eukaryote{ false };
    bool   m_di_fasta{ false };
    bool   m_d_fasta{ false };
    bool   m_allow_accession{ false };
    bool   m_verbose{ false };
    bool   m_augustus_fix{ false };
    bool   m_make_flatfile{ false };
    bool   m_run_discrepancy{ false };
    bool   m_split_discrepancy{ false };
    bool   m_accumulate_mods{ false };
    bool   m_binary_asn1_output { false };
    bool   m_use_huge_files { false };


    NDiscrepancy::EGroup m_discrepancy_group{ NDiscrepancy::eOncaller };

    unique_ptr<objects::edit::CRemoteUpdater> m_remote_updater;

    objects::CFixSuspectProductName m_suspect_rules;

    //string conffile;

/////////////
    CTable2AsnContext();
    ~CTable2AsnContext();

    static
    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data);
    void SetOrganismData(objects::CSeq_descr& SD, int genome_code, const string& taxname, int taxid, const string& strain) const;

    CNcbiOstream& GetOstream(CTempString suffix, CTempString basename=kEmptyStr);
    void ClearOstream(const CTempString& suffix);

    string GenerateOutputFilename(const CTempString& ext, CTempString basename=kEmptyStr) const;
    void ReleaseOutputs();

    static
    objects::CUser_object& SetUserObject(objects::CSeq_descr& descr, const CTempString& type);
    static
    objects::CSeq_descr& SetBioseqOrParentDescr(objects::CBioseq& bioseq);
    void ApplyCreateUpdateDatesSingle(objects::CSeq_entry& entry) const;
    bool ApplyCreateUpdateDates(objects::CSeq_entry& entry) const;
    void ApplyUpdateDate(objects::CSeq_entry& entry) const;
    void ApplyAccession(objects::CSeq_entry& entry);
    void ApplyFileTracks(objects::CSeq_entry& entry) const;
    void ApplyComments(objects::CSeq_entry& entry);
    CRef<CSerialObject> CreateSubmitFromTemplate(
        CRef<objects::CSeq_entry>& object,
        CRef<objects::CSeq_submit>& submit) const;
    CRef<CSerialObject>
        CreateSeqEntryFromTemplate(CRef<objects::CSeq_entry> object) const;
    void UpdateSubmitObject(CRef<objects::CSeq_submit>& submit) const;

    void MergeWithTemplate(objects::CSeq_entry& entry) const;
    void SetSeqId(objects::CSeq_entry& entry) const;
    void CopyFeatureIdsToComments(objects::CSeq_entry& entry) const;
    void RemoveUnnecessaryXRef(objects::CSeq_entry& entry) const;
    void SmartFeatureAnnotation(objects::CSeq_entry& entry) const;

    void CorrectCollectionDates(objects::CSeq_entry& entry);

    void MakeGenomeCenterId(objects::CSeq_entry& entry);
    void RenameProteinIdsQuals(objects::CSeq_feat& feature);
    void RemoveProteinIdsQuals(objects::CSeq_feat& feature);
    static bool IsDBLink(const objects::CSeqdesc& desc);


    CRef<objects::CSeq_submit> m_submit_template;
    CRef<objects::CSeq_entry>  m_entry_template;
    objects::ILineErrorListener* m_logger{ nullptr };

    CRef<objects::CScope>      m_scope;
    CRef<objects::CObjectManager> m_ObjMgr;
    string mCommandLineMods;

    static bool GetOrgName(string& name, const objects::CSeq_entry& entry);
    static CRef<objects::COrg_ref> GetOrgRef(objects::CSeq_descr& descr);
    static void UpdateTaxonFromTable(objects::CBioseq& bioseq);

    void MergeSeqDescr(objects::CSeq_entry& dest, const objects::CSeq_descr& src, bool only_set) const;

private:
    static void x_ApplyAccession(CTable2AsnContext& context, objects::CBioseq& bioseq);
    map<string, pair<string, unique_ptr<CNcbiOstream>>> m_outputs;
};

void g_LoadLinkageEvidence(const string& linkageEvidenceFilename,
        objects::CGapsEditor::TCountToEvidenceMap& gapsizeToEvidence,
        objects::ILineErrorListener* pMessageListener);

END_NCBI_SCOPE


#endif
