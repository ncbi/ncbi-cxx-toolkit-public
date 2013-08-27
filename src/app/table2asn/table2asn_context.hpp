#ifndef __TABLE2ASN_CONTEXT_HPP_INCLUDED__
#define __TABLE2ASN_CONTEXT_HPP_INCLUDED__

//#include <objects/general/Date.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CSeq_descr;
class CDate;
class IMessageListener;
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
	bool   m_HandleAsSet;
	bool   m_GenomicProductSet;
	bool   J;
	string F;
	string A;
	string C;
	string m_OrganismName;
	string m_source_qualifiers;
	string m_Comment;
	string m_single_table5_file;
	string m_find_open_read_frame;
	string V;
	bool   m_SetIDFromFile;
	bool   m_NucProtSet;
	bool   I;
	string G;
	bool   R;
	bool   S;
	string Q;
	bool   U;
	bool   L;
	bool   m_RemoteTaxonomyLookup;
	bool   m_RemotePubLookup;
	bool   W;
	bool   K;
	CRef<objects::CDate> m_HoldUntilPublish;
	string ZOutFile;
	string c;
	string zOufFile;
	string X;
	int    m_ProjectVersionNumber;
	string m_single_structure_cmt;
	bool   m_flipped_struc_cmt;
	string M;
	string l;
	string m;
	int    m_taxid;
	string m_strain;
	string m_url;

	//
	//bool   m_make_submit;
	CRef<objects::CSeq_descr>  m_descriptors;

	//string logfile;
	//string conffile;

/////////////
	string m_format;

	CTable2AsnContext();
	~CTable2AsnContext();

    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data) const;
	void SetOrganismData(objects::CSeq_descr& SD, int genome_code, const string& taxname, int taxid, const string& strain) const;

    void FindOpenReadingFrame(objects::CSeq_entry& entry);

	CRef<objects::CSeq_submit> m_submit_template;
	CRef<objects::CSeq_entry>  m_entry_template;
    objects::IMessageListener* m_logger;
private:
};


END_NCBI_SCOPE


#endif
