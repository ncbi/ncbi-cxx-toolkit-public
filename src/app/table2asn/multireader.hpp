#ifndef TABLE2ASN_MULTIREADER_HPP
#define TABLE2ASN_MULTIREADER_HPP

#include <util/format_guess.hpp>
#include <corelib/ncbistre.hpp>
#include <objtools/readers/idmapper.hpp>
#include <objects/general/Date.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
//class CSeq_annot;
class CSeq_entry;
class CSeq_submit;
};

// command line parameters are mapped into the context 
// those with only only symbol still needs to be implemented
struct CTable2AsnContext
{
	string rResultsDirectory;
	CNcbiOstream* m_output;
	string a;
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
	bool   q;
	bool   u;
	bool   I;
	string G;
	bool   R;
	bool   S;
	string Q;
	bool   U;
	bool   L;
	bool   T;
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

	// 
	//bool   m_make_submit;
	CRef<objects::CSeq_submit> m_submit_template;
	CRef<objects::CSeq_entry>  m_other_template;
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
	   gGenomicProductSet(false)
	{
	}
};


//  ============================================================================
class CMultiReader
//  ============================================================================
{
public:
	CMultiReader();
//    CMultiReaderApp(): m_pErrors( 0 ) {};

   void Process(const CTable2AsnContext& args, const CNcbiApplication& app);

   //CArgDescriptions* InitAppArgs(CNcbiApplication& app);
   int RunOld(const CTable2AsnContext& args, const string& ifname, CNcbiOstream& output);

   CRef<CSerialObject> ReadFile(const CTable2AsnContext& args, const string& ifname);
   CRef<CSerialObject> LoadFile(const CTable2AsnContext& args, const string& ifname);
   void Cleanup(const CTable2AsnContext& args, CRef<CSerialObject>);
   void WriteObject(CSerialObject&, CNcbiOstream& );
   void ApplyAdditionalProperties(const CTable2AsnContext& args, CSerialObject* obj);
   void ApplyAdditionalProperties(const CTable2AsnContext& args, objects::CSeq_entry& entry);
   void LoadTemplate(const CTable2AsnContext& args, const string& ifname, CRef<objects::CSeq_entry> & out_ent_templ, CRef<objects::CSeq_submit> & out_submit_templ);
   void LoadDescriptors(const CTable2AsnContext& args, const string& ifname, CRef<objects::CSeq_descr> & out_desc);
   objects::CUser_object* AddStructuredComment(objects::CUser_object* obj, const string& name, const string& value, CSerialObject& cont);
   objects::CUser_object* AddStructuredComment(objects::CUser_object* obj, const string& name, const string& value, objects::CSeq_descr& cont);
   static
   CRef<objects::CSeq_descr> GetSeqDescr(CSerialObject* obj);
   
   static
   void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeq_descr & source);
   static
   void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeqdesc & source);
   static
   void ApplyDescriptors(CSerialObject & obj, const objects::CSeq_descr & source);

protected:
       
private:
//    virtual void Init(void);
//    virtual int  Run(void);

    
    void xProcessDefault(const CTable2AsnContext&, CNcbiIstream&);
    void xProcessWiggle(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessWiggleRaw(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessBed(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessBedRaw(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGtf(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessVcf(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessNewick(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGff3(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGff2(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGvf(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessAlignment(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessAgp(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);

    void xSetFormat(const CTable2AsnContext&, CNcbiIstream&);
    void xSetFlags(const CTable2AsnContext&, CNcbiIstream&);
    void xSetMapper(const CTable2AsnContext&);
    void xSetErrorContainer(const CTable2AsnContext&);
            
    void xWriteObject(CSerialObject&, CNcbiOstream& );
    void xDumpErrors(CNcbiOstream& );

    CFormatGuess::EFormat m_uFormat;
    bool m_bCheckOnly;
    bool m_bDumpStats;
    int  m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;

    auto_ptr<objects::CIdMapper> m_pMapper;
    CRef<objects::CMessageListenerBase> m_pErrors;
	CRef<CSerialObject> m_pObject;
};

END_NCBI_SCOPE

#endif
