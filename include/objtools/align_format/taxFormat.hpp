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

 const unsigned int kMinLineLength = 100; //used for text output





class NCBI_ALIGN_FORMAT_EXPORT CTaxFormat {

  public:
        
        
    /// Constructors
    ///@param seqalign: seqalign used to display taxonomy info     
    ///@param scope: scope to fetch your sequence 
    ///@param connectToTaxServe: default true indicates to connect to Tax server
    ///@param lineLength: lineLength for text formatting
    ///@param displayOption: displayOption HTML or text formatting
    CTaxFormat(const objects::CSeq_align_set & seqalign,
                     objects::CScope & scope,
                     unsigned int displayOption = eHtml,
                     bool connectToTaxServer = true,
                     unsigned int lineLength = 100);

    ///@param accessionTaxidList: list of pair<string,TTaxId> accession and corresponding taxid to display taxonomy info     
    ///@param scope: scope to fetch your sequence 
    ///@param connectToTaxServe: default true indicates to connect to Tax server
    ///@param lineLength: lineLength for text formatting
    ///@param displayOption: displayOption HTML or text formatting
    CTaxFormat(list< pair<string,TTaxId> > &accessionTaxidList,
                     objects::CScope & scope,                     
                     unsigned int displayOption = eHtml,
                     bool connectToTaxServer = true,
                     unsigned int lineLength = 100);
                     
                     
        /// Destructor
    ~CTaxFormat();   

    enum eDisplayOption {
        eHtml,
        eText
    };

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
        TTaxId taxid;                    ///< taxid   
        TGi   gi;                        ///< gi
        CRef<objects::CSeq_id> seqID;    ///< seqID used in defline
        string label;                    ///< sequence label        
        string title;                    ///< sequnce title
        string bit_score;                ///< score 
        string evalue;                   ///< evalue
        TGi displGi;                     ///<gi for seq that is displayed in alignment section
        string dispSeqID;
    };
    
                                             
    struct STaxInfo {
        TTaxId taxid;                    ///< taxid   
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
        vector <TTaxId>    lineage;      ///< vector of taxids containg lineage for taxid  
        unsigned int       numHits;      ///< Number of sequences in alignmnet corresponding to taxid and it's children
        unsigned int       numOrgs;      ///< Number of organism in alignmnet corresponding to taxid and it's children
    };

    typedef map < TTaxId, struct STaxInfo > TSeqTaxInfoMap; 

    struct SBlastResTaxInfo {
        vector <TTaxId> orderedTaxids;   ///< taxids ordered by highest score or common tree top to bottom tarveres??
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

    void SetDisplayOption(unsigned int displayOption){m_DisplayOption = displayOption;}

    
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
    vector <TTaxId> &GetAlignTaxIDs(void){return m_BlastResTaxInfo->orderedTaxids;}

    ///Gets info for sequences in alignment corresponding to taxid
    ///@param taxid: taxid
    ///@return: STaxInfo 
    ///    
    struct STaxInfo &GetAlignTaxInfo(TTaxId taxid){ return m_BlastResTaxInfo->seqTaxInfoMap[taxid];}//Check if taxid exists in aligh first    

    ///Gets taxids for sequences in taxonomy tree
    ///@return: vector of taxids
    ///    
    vector <TTaxId> &GetTaxTreeTaxIDs(void){return m_TaxTreeinfo->orderedTaxids;}

    ///Gets info for sequences in taxonomy tree corresponding to taxid
    ///@param taxid: taxid
    ///@return: STaxInfo 
    ///    
    struct STaxInfo &GetTaxTreeInfo(TTaxId taxid){ return m_TaxTreeinfo->seqTaxInfoMap[taxid];}

    ///Checks if there are sequences in alignment with specified taxid
    ///@param taxid: taxid
    ///@return: bool true  if there are sequences in alignment with specified taxid
    ///    
    bool isTaxidInAlign(TTaxId taxid);
    

protected:
 
    /// reference to seqalign set
    CConstRef < objects::CSeq_align_set > m_SeqalignSetRef;     
    list< pair<string,TTaxId> > m_AccessionTaxidList;
    objects::CScope & m_Scope;        
    CCgiContext* m_Ctx;
    
    
    SBlastResTaxInfo *m_BlastResTaxInfo;    ///< SBlastResTaxInfo structure containing information for taxids in alignment, orderedTaxids are ordered by highest score
    SBlastResTaxInfo *m_TaxTreeinfo;        ///< SBlastResTaxInfo structure containing information for all taxids in common tree, intermediate nodes with no hits or only 1 child are removed
                                            ///< orderedTaxids are ordered by highest score    
    list <STaxInfo> m_AlnLineageTaxInfo;    ///< STaxInfo structure list contaning info for lineage report ordered by linage display order. All taxids are "mapped" to the best hit linage
    vector <TTaxId> m_BestHitLineage;       ///< vector of <int> containing taxids for the best hit linage

      
    string m_Rid;    
    STaxFormatTemplates *m_TaxFormatTemplates; ///<
    CNcbiIfstream *m_ConfigFile;
    CNcbiRegistry *m_Reg;
    string  m_TaxBrowserURL;                ///< Taxonomy Browser URL
    CTaxon1 *m_TaxClient;                   ///< Taxonomy server client
    string m_TaxIdToSeqsMap;                /// String containig map of taxids to corresponding accessions

    bool    m_Debug;

    unsigned int m_DisplayOption;
    bool  m_ConnectToTaxServer;
    bool  m_TaxTreeLoaded;    
    CRef< ITreeIterator > m_TreeIterator;


    //Text formatting
    unsigned int m_MaxAccLength;
    unsigned int m_MaxDescrLength;
    unsigned int m_MaxScoreLength;
    unsigned int m_MaxEvalLength;

    unsigned int m_LineLength;
    

    string   m_Protocol; ///< protocol, default https otherwise get from .ncbirc
    
    CTaxFormat::SSeqInfo *x_FillTaxDispParams(const CRef< objects::CBlast_def_line > &bdl,
                                  const objects::CBioseq_Handle& bsp_handle,
                                  double bits, 
                                  double evalue,    
	                              list<string>& use_this_seqid);

    CTaxFormat::SSeqInfo *x_FillTaxDispParams(const objects::CBioseq_Handle& bsp_handle,
                             double bits, 
                             double evalue);
                                  

    void x_InitBlastDBTaxInfo(SSeqInfo *seqInfo);
    void x_InitBlastDBTaxInfo(pair<string,TTaxId> &accTaxidPair);
    void x_InitTaxInfoMap(void);
    void x_InitTaxInfoMapFromAccList(void);
    void x_InitTaxFormat(void);

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
    void x_InitTextFormatInfo(CTaxFormat::SSeqInfo *seqInfo);
    void x_InitTaxIdToSeqsMap(void);

    void x_PrintTaxInfo(vector <TTaxId> taxids, string title);       
    void x_PrintLineage(void);
};


END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___TAXFORMAT_HPP */
