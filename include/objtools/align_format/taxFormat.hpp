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
 * Author:  Irena Zaretskaya
 */

/** @file taxFormat.hpp
 *  Sequence alignment taxonomy display tool
 *
 */

#ifndef OBJTOOLS_ALIGN_FORMAT___TAXFORMAT_HPP
#define OBJTOOLS_ALIGN_FORMAT___TAXFORMAT_HPP

#include <corelib/ncbireg.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objtools/align_format/align_format_util.hpp>





BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
class CCgiContext;
BEGIN_SCOPE(align_format)


/**
 * Example:
 * @code
 * CRef<objects::CSeq_align_set> aln_set = ...
 * CRef<objects::CScope> scope = ... 
 * .......
 * CTaxFormat txformat(*aln_set, *scope,<connectToTaxServer>);   
 * ds.SetAlignOption(display_option);
 * txformat.DisplayOrgReport(stdout);
 * to display Lineage and/or taxonomy report connectToTaxServer = true (defualt)
 * txformat.DisplayLineageReport(stdout);
 * txformat.DisplayTaxonomyReport(stdout); 
 * @endcode
 */
 
 const string kTaxBrowserURL           = "//www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi";
 const string kBlastNameLink           = "<a href=\"<@taxBrowserURL@>?id=<@bl_taxid@>\" target=\"lnktx<@rid@>\" title=\"Show taxonomy info for <@blast_name@> (taxid <@bl_taxid@>)\"><@blast_name@></a>";

 const string kOrgReportTable          = "<table><caption><h1>Organism Report</h1></caption><tr><th>Accession</th><th>Descr</th><th>Score</th><th>E-value</th></tr><@table_rows@></table><@taxidToSeqsMap@>";
 const string kOrgReportOrganismHeader = "<tr><th colspan=\"4\"><a href=\"<@taxBrowserURL@>?id=<@taxid@>\" name=\"<@taxid@>\" title=\"Show taxonomy info for <@scientific_name@> (taxid <@taxid@>)\" target=\"lnktx<@rid@>\"><@scientific_name@>[<@blast_name_link@>] taxid <@taxid@></th></tr>";
 const string kOrgReportOrganismHeaderNoTaxConnect = "<tr><th colspan=\"4\"><a href=\"<@taxBrowserURL@>?id=<@taxid@>\" name=\"<@taxid@>\" title=\"Show taxonomy info for <@scientific_name@> (taxid <@taxid@>)\" target=\"lnktx<@rid@>\"><@scientific_name@>[<@blast_name@>]</th></tr>";
 const string kOrgReportTableHeader    = "<tr><th>Accession</th><th>Description</th><th>Score</th><th>E-value</th></tr>";
 const string kOrgReportTableRow       = "<tr><td><a title=\"Show report for <@acc@>\" target=\"lnktx<@rid@>\" href=\"//www.ncbi.nlm.nih.gov/protein/<@gi@>?report=fwwwtax&amp;log$=taxrep&amp;RID=<@rid@>\"><@acc@></a></td><td><@descr_abbr@></td><td><@score@></td><td><@evalue@></td></tr>";

 const string kTaxIdToSeqsMap          = "<input type=\"hidden\" id=\"txForSeq_<@taxid@>\" value=\"<@giList@>\" />";

 const string kLineageReportTable          = "<table><caption><h1>Linage Report</h1><caption><@table_rows@></table>";
 const string kLineageReportTableHeader    = "<tr><th>Organism</th><th>Blast Name</th><th>Score</th><th>Number of Hits</th><th>Description</th></tr>";
 const string kLineageReportOrganismHeader = "<tr><td><@depth@><a href=\"//<@taxBrowserURL@>?id=<@taxid@>\" title=\"Show taxonomy info for <@scientific_name@> (taxid <@taxid@>)\" target=\"lnktx<@rid@>\"><@scientific_name@></a><td><@blast_name_link@></td><td colspan =\"3\"></td></tr>";
 const string kLineageReportTableRow       = "<tr><td><@depth@><a href=\"//<@taxBrowserURL@>?id=<@taxid@>\" title=\"Show taxonomy info for <@scientific_name@> (taxid <@taxid@>)\" target=\"lnktx<@rid@>\"><@scientific_name@></a></td><td><@blast_name_link@></td><td><@score@></td><td><a href=\"#<@taxid@>\" title=\"Show organism report for <@scientific_name@>\"><@numhits@></a></td><td><a title=\"Show report for <@acc@> <@descr_abbr@>\" target=\"lnktx<@rid@>\" href=\"//www.ncbi.nlm.nih.gov/protein/<@gi@>?report=genbank&amp;log$=taxrep&amp;RID=<@rid@>\"><@descr_abbr@></a></td></tr>";


 const string kTaxonomyReportTable          = "<table><caption><h1>Taxonomy Report</h1><caption><@table_rows@></table>";
 const string kTaxonomyReportTableHeader    = "<tr><th>Taxonomy</th><th>Number of hits</th><th>Number of organisms</th><th>Description</th></tr>"; 
 const string kTaxonomyReportOrganismHeader = "<tr><td><@depth@><a href=\"//<@taxBrowserURL@>?id=<@taxid@>\" title=\"Show taxonomy info for <@scientific_name@> (taxid <@taxid@>)\" target=\"lnktx<@rid@>\"><@scientific_name@></a></td><td><@numhits@></td><td><@numOrgs@></td><td><@descr_abbr@></td></tr>"; 
 const string kTaxonomyReportTableRow       = "<tr><td><@depth@><a href=\"//<@taxBrowserURL@>?id=<@taxid@>\" title=\"Show taxonomy info for <@scientific_name@> (taxid <@taxid@>)\" target=\"lnktx<@rid@>\"><@scientific_name@></a></td><td><@numhits@></td><td><@numOrgs@></td><td><@descr_abbr@></td></tr>";

 

class NCBI_ALIGN_FORMAT_EXPORT CTaxFormat {

  public:
        
        
    /// Constructors
    ///@param seqalign: seqalign used to display taxonomy info     
    ///@param scope: scope to fetch your sequence 
    ///@param connectToTaxServe: default true indicates to connect to Tax server
    CTaxFormat(const objects::CSeq_align_set & seqalign,
                     objects::CScope & scope,
                     bool connectToTaxServer = true);
                     
        /// Destructor
    ~CTaxFormat();    

    ///Displays Organism Report
    ///@param out: stream for display
    ///    
    void DisplayOrgReport(CNcbiOstream & out);    

    ///Displays Linage Report
    ///@param out: stream for display
    ///    
    void DisplayLineageReport(CNcbiOstream& out);

    ///Displays Taxonomy Report
    ///@param out: stream for display
    ///    
    void DisplayTaxonomyReport(CNcbiOstream& out);
    
    struct SSeqInfo {
        int taxid;                       ///< taxid   
        TGi   gi;                        ///< gi
        CRef<objects::CSeq_id> seqID;    ///< seqID used in defline
        string label;                    ///< sequence label        
        string title;                    ///< sequnce title
        double bit_score;                ///< score 
        double evalue;                   ///< evalue
        TGi displGi;                     ///<gi for seq that is displayed in alignment section
    };

    struct STaxInfo {
        int taxid;                       ///< taxid   
        string commonName;               ///< commonName
        string scientificName;           ///< scientificName  
        string blastName;                ///< blastName
        int    blNameTaxid;              ///< blastName taxid
        vector <SSeqInfo*> seqInfoList;  ///< vector of SSeqInfo corresponding to taxid
        string             giList;       ///< string of gis separated by comma corresponding to taxid
        string             accList;      ///< string of accessions separated by comma corresponding to taxid
        string             taxidList;    ///< string of "children" taxids containing sequences in alignment separated by comma corresponding to taxid

        unsigned int       numChildren;  ///< Number of childre for taxid 
        unsigned int       depth;        ///< Depth   
        vector <int>       lineage;      ///< vector of taxids containg lineage for taxid  
        unsigned int       numHits;      ///< Number of sequences in alignmnet corresponding to taxid and it's children
        unsigned int       numOrgs;      ///< Number of organism in alignmnet corresponding to taxid and it's children
    };

    typedef map < int, struct STaxInfo > TSeqTaxInfoMap; 

    struct SBlastResTaxInfo {
        vector <int> orderedTaxids;   ///< taxids ordered by highest score or common tree top to bottom tarveres??
        TSeqTaxInfoMap seqTaxInfoMap; ///< Map containing info for orderedTaxids
    };


    struct STaxFormatTemplates {
        string blastNameLink;           ///< Template for displaying blast name link

        string orgReportTable;          ///< Template for displaying organism report table
        string orgReportOrganismHeader; ///< Template for organism report organism header
        string orgReportTableHeader;    ///< Template for displaying organism report table header
        string orgReportTableRow;       ///< Tempalte for displaying organism report table row

        string taxIdToSeqsMap;          ///< Tempalte for mapping taxids to seqlist

        string lineageReportTable;          ///< Template for displaying lineage report table
        string lineageReportOrganismHeader; ///< Template for displaying lineage report organism header
        string lineageReportTableHeader;    ///< Template for displaying lineage report table header
        string lineageReportTableRow;       ///< Template for displaying lineage report table row

        string taxonomyReportTable;         ///< Template for displaying taxonomy report table
        string taxonomyReportOrganismHeader;///< Template for displaying taxonomy report organism header
        string taxonomyReportTableHeader;   ///< Template for displaying taxonomy report table header
        string taxonomyReportTableRow;      ///< Template for displaying taxonomy report table row
    };

    ///set connection to taxonomy server
    ///@param connectToTaxServer: bool
    ///
    void SetConnectToTaxServer(bool connectToTaxServer) {m_ConnectToTaxServer = connectToTaxServer;}

    ///set blast request id
    ///@param rid: blast RID
    ///
    void SetRid(string rid) {
        m_Rid = rid;
    }

    void SetCgiContext (CCgiContext& ctx) {
        m_Ctx = &ctx;
    }

    ///Gets pointer to STaxFormatTemplates
    ///@return: taxFormatTemplates
    ///    
    STaxFormatTemplates *GetTaxFormatTemplates(void) {return m_TaxFormatTemplates;}    

    ///Gets taxids for sequences in alignment
    ///@return: vector of taxids
    ///    
    vector <int> &GetAlignTaxIDs(void){return m_BlastResTaxInfo->orderedTaxids;}

    ///Gets info for sequences in alignment corresponding to taxid
    ///@param taxid: taxid
    ///@return: STaxInfo 
    ///    
    struct STaxInfo &GetAlignTaxInfo(int taxid){ return m_BlastResTaxInfo->seqTaxInfoMap[taxid];}//Check if taxid exists in aligh first    

    ///Gets taxids for sequences in taxonomy tree
    ///@return: vector of taxids
    ///    
    vector <int> &GetTaxTreeTaxIDs(void){return m_TaxTreeinfo->orderedTaxids;}

    ///Gets info for sequences in taxonomy tree corresponding to taxid
    ///@param taxid: taxid
    ///@return: STaxInfo 
    ///    
    struct STaxInfo &GetTaxTreeInfo(int taxid){ return m_TaxTreeinfo->seqTaxInfoMap[taxid];}

    ///Checks if there are sequences in alignment with specified taxid
    ///@param taxid: taxid
    ///@return: bool true  if there are sequences in alignment with specified taxid
    ///    
    bool isTaxidInAlign(int taxid);
    

protected:
 
    /// reference to seqalign set
    CConstRef < objects::CSeq_align_set > m_SeqalignSetRef;     
    objects::CScope & m_Scope;        
    CCgiContext* m_Ctx;

    
    SBlastResTaxInfo *m_BlastResTaxInfo;    ///< SBlastResTaxInfo structure containing information for taxids in alignment, orderedTaxids are ordered by highest score
    SBlastResTaxInfo *m_TaxTreeinfo;        ///< SBlastResTaxInfo structure containing information for all taxids in common tree, intermediate nodes with no hits or only 1 child are removed
                                            ///< orderedTaxids are ordered by highest score    
    list <STaxInfo> m_AlnLineageTaxInfo;    ///< STaxInfo structure list contaning info for lineage report ordered by linage display order. All taxids are "mapped" to the best hit linage
    vector <int> m_BestHitLineage;          ///< vector of <int> containing taxids for the best hit linage

      
    string m_Rid;    
    STaxFormatTemplates *m_TaxFormatTemplates; ///<
    CNcbiIfstream *m_ConfigFile;
    CNcbiRegistry *m_Reg;
    string  m_TaxBrowserURL;                ///< Taxonomy Browser URL
    CTaxon1 *m_TaxClient;                   ///< Taxonomy server client

    bool  m_ConnectToTaxServer;
    bool  m_TaxTreeLoaded;    
    CRef< ITreeIterator > m_TreeIterator;
    
    CTaxFormat::SSeqInfo *x_FillTaxDispParams(const CRef< objects::CBlast_def_line > &bdl,
                                  const objects::CBioseq_Handle& bsp_handle,
                                  double bits, 
                                  double evalue,    
	                              list<TGi>& use_this_gi);

    CTaxFormat::SSeqInfo *x_FillTaxDispParams(const objects::CBioseq_Handle& bsp_handle,
                             double bits, 
                             double evalue);
                                  

    void x_InitBlastDBTaxInfo(SSeqInfo *seqInfo);
    void x_InitTaxInfoMap(void);

    void x_InitTaxClient(void); /// Initializes CTaxon1 *m_TaxClient

    void x_LoadTaxTree(void); // Loads tree containing taxids from seqalign in taxonomy client m_TaxClient       
    

    void x_InitLineageReport(void);
    void x_InitTaxReport(void);

    void x_InitOrgTaxMetaData(void); // Inits tree containing information for all taxids in common tree (m_TaxTreeinfo)
    void x_InitLineageMetaData(void); // Inits list contaning info for lineage report ordered by linage display order (m_AlnLineageTaxInfo)

    void x_InitBlastNameTaxInfo(STaxInfo &taxInfo);

    string x_MapSeqTemplate(string seqTemplate, SSeqInfo *seqInfo);
    string x_MapSeqTemplate(string seqTemplate, STaxInfo &taxInfo);
    string x_MapTaxInfoTemplate(string tableRowTemplate,STaxInfo &taxInfo,unsigned int depth = 0);

    void x_PrintTaxInfo(vector <int> taxids, string title);       
    void x_PrintLineage(void);
};


END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___TAXFORMAT_HPP */
