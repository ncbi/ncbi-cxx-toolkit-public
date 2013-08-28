#ifndef __TABLE2ASN_CONTEXT_HPP_INCLUDED__
#define __TABLE2ASN_CONTEXT_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CSeq_descr;
class CDate;
class IMessageListener;
class CUser_object;
class CBioSource;
};

// command line parameters are mapped into the context
// those with only only symbol still needs to be implemented
class CTable2AsnContext
{
public:
    string m_current_file;
    string m_ResultsDirectory;
    CNcbiOstream* m_output;
    string m_accession;
    string m_OrganismName;
    string m_source_qualifiers;
    string m_Comment;
    string m_single_table5_file;
    string m_find_open_read_frame;
    string m_strain;
    string m_url;
    CRef<objects::CDate> m_HoldUntilPublish;
    string F;
    string A;
    string C;
    string V;
    bool   J;
    bool   I;
    string G;
    bool   R;
    bool   S;
    string Q;
    bool   U;
    bool   L;
    bool   W;
    bool   K;
    string c;
    string ZOutFile;
    string zOufFile;
    string X;
    string M;
    string l;
    string m;
    string m_single_structure_cmt;
    int    m_ProjectVersionNumber;
    bool   m_flipped_struc_cmt;
    bool   m_RemoteTaxonomyLookup;
    bool   m_RemotePubLookup;
    bool   m_HandleAsSet;
    bool   m_GenomicProductSet;
    bool   m_SetIDFromFile;
    bool   m_NucProtSet;
    int    m_taxid;

    CRef<objects::CSeq_descr>  m_descriptors;

    //string conffile;

/////////////
    CTable2AsnContext();
    ~CTable2AsnContext();

    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data) const;
    void SetOrganismData(objects::CSeq_descr& SD, int genome_code, const string& taxname, int taxid, const string& strain) const;
    void ApplySourceQualifiers(objects::CSeq_entry& bioseq, const string& src_qualifiers);

    void FindOpenReadingFrame(objects::CSeq_entry& entry);
    static
    objects::CUser_object& SetUserObject(objects::CSeq_descr& descr, const string& type);
    static
    objects::CBioSource& SetBioSource(objects::CSeq_descr& SD);

    CRef<objects::CSeq_submit> m_submit_template;
    CRef<objects::CSeq_entry>  m_entry_template;
    objects::IMessageListener* m_logger;
private:
};


END_NCBI_SCOPE


#endif
