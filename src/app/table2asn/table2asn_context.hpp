#ifndef __TABLE2ASN_CONTEXT_HPP_INCLUDED__
#define __TABLE2ASN_CONTEXT_HPP_INCLUDED__

#include <objects/general/Date.hpp>
#include <objects/seqfeat/BioSource.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
//class CSeq_annot;
class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CSeq_descr;
};

// command line parameters are mapped into the context 
// those with only only symbol still needs to be implemented
struct CTable2AsnContext
{
	string m_current_file;
	string rResultsDirectory;
	CNcbiOstream* m_output;
	string m_accession;
	bool   sHandleAsSet;
	bool   gGenomicProductSet;
	bool   J;
	string F;
	string A;
	string C;
	string nOrganismName;
	string j;
	string yComment;
	string fInFile;
	string k;
	string V;
	bool   qSetIDFromFile;
	bool   u;
	bool   I;
	string G;
	bool   R;
	bool   S;
	string Q;
	bool   U;
	bool   L;
	bool   TRemoteTaxonomyLookup;
	bool   P;
	bool   W;
	bool   K;
	objects::CDate  HoldUntilPublish;
	string ZOutFile;
	string c;
	string zOufFile;
	string X;
	int    NProjectVersionNumber;
	string wInFile;
	string M;
	string l;
	string m;
	string m_taxname;
	int    m_taxid;
	string m_strain;
	string m_url;

	// 
	//bool   m_make_submit;
	CRef<objects::CSeq_submit> m_submit_template;
	CRef<objects::CSeq_entry>  m_entry_template;
	CRef<objects::CSeq_descr>  m_descriptors;

	//string logfile;
	//string conffile;
	//bool   dryrun;

/////////////
	string m_format;
	//CNcbiIstream* m_input;
	//CNcbiOstream* m_output;
	/*
  table2asn.exe [-h] [-help] [-xmlhelp] [-p Directory] [-r Directory]
    [-i InFile] [-o OutFile] [-x String] [-E] [-t InFile] [-a String] [-s]
    [-g] [-J] [-F String] [-A String] [-C String] [-n String] [-j String]
    [-y String] [-Y InFile] [-D InFile] [-f InFile] [-k String] [-V String]
    [-q] [-u] [-I] [-G String] [-R] [-S] [-Q String] [-U] [-L] [-T] [-P] [-W]
    [-K] [-H String] [-Z OutFile] [-c String] [-z OutFile] [-X String]
    [-N Integer] [-w InFile] [-M String] [-l String] [-m String]
    [-logfile File_Name] [-conffile File_Name] [-version] [-version-full]
    [-dryrun]	*/
	CTable2AsnContext():
	   //m_input(0), 
	   m_output(0),
	   //m_make_set(false),
	   sHandleAsSet(false),
	   gGenomicProductSet(false),
	   m_taxid(0)
	{
	}

	CRef<CSerialObject> UseTemplate(objects::CBioseq*& bioseq) const;

    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data) const;
	void SetOrganismData(objects::CSeq_descr& SD, objects::CBioSource::EGenome m_genome, const string& m_taxname, int m_taxid, const string& m_strain) const;
	void RemoteRequestTaxid();
};


END_NCBI_SCOPE


#endif
