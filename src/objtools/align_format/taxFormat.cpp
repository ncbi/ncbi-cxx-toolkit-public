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
 *  and reliability of the software and data, the NLM and thesubset U.S.
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
 *
 * File Description:
 *   Sequence alignment display
 *
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/taxon1/Taxon2_data.hpp>

#include <objtools/align_format/taxFormat.hpp>


#include <objtools/blast/seqdb_reader/seqdb.hpp>    // for CSeqDB::ExtractBlastDefline
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align_set.hpp>


#include <stdio.h>
#include <cgi/cgictx.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);
BEGIN_SCOPE(align_format)



CTaxFormat::CTaxFormat(const CSeq_align_set& seqalign, 
                       CScope& scope,
                       bool connectToTaxServer)
                      
    : m_SeqalignSetRef(&seqalign),
      m_Scope(scope),
      m_ConnectToTaxServer(connectToTaxServer)
{
    m_TaxClient = NULL;    
    m_TaxTreeLoaded = false;    
    m_Rid = "0";         
    m_Ctx = NULL;    
    m_TaxTreeinfo = NULL;
    m_BlastResTaxInfo = NULL;    
    m_Debug = false;
    
    if(m_ConnectToTaxServer) {        
        x_InitTaxClient();        
    }
    x_InitTaxInfoMap();    //Sets m_BlastResTaxInfo
    if(m_ConnectToTaxServer) {        
        x_LoadTaxTree();
    }
    
    //set config file
    m_ConfigFile = new CNcbiIfstream(".ncbirc");
    m_Reg = new CNcbiRegistry(*m_ConfigFile);             
    if(m_Reg) {
        m_TaxBrowserURL = m_Reg->Get("TAX_BROWSER","BLASTFMTUTIL");
    }
    m_TaxBrowserURL = (m_TaxBrowserURL.empty()) ? kTaxBrowserURL : m_TaxBrowserURL;

    
        
    m_TaxFormatTemplates = new STaxFormatTemplates;        

    m_TaxFormatTemplates->blastNameLink = kBlastNameLink;
    m_TaxFormatTemplates->orgReportTable = kOrgReportTable;
    m_TaxFormatTemplates->orgReportOrganismHeader = kOrgReportOrganismHeader; ///< Template for displaying header
    m_TaxFormatTemplates->orgReportTableHeader  = kOrgReportTableHeader; ///< Template for displaying header
    m_TaxFormatTemplates->orgReportTableRow = kOrgReportTableRow;

    m_TaxFormatTemplates->taxIdToSeqsMap = kTaxIdToSeqsMap;

    m_TaxFormatTemplates->lineageReportTable = kLineageReportTable;
    m_TaxFormatTemplates->lineageReportOrganismHeader = kLineageReportOrganismHeader; ///< Template for displaying header
    m_TaxFormatTemplates->lineageReportTableHeader  = kLineageReportTableHeader; ///< Template for displaying header
    m_TaxFormatTemplates->lineageReportTableRow = kLineageReportTableRow;

    m_TaxFormatTemplates->taxonomyReportTable           = kTaxonomyReportTable;
    m_TaxFormatTemplates->taxonomyReportOrganismHeader  = kTaxonomyReportOrganismHeader;    
    m_TaxFormatTemplates->taxonomyReportTableHeader     = kTaxonomyReportTableHeader;
    m_TaxFormatTemplates->taxonomyReportTableRow        = kTaxonomyReportTableRow;
}

CTaxFormat::~CTaxFormat()
{   
    
    if (m_ConfigFile) {
        delete m_ConfigFile;
    } 
    if (m_Reg) {
        delete m_Reg;
    }
    if(m_BlastResTaxInfo) {        
        for(TSeqTaxInfoMap::iterator it=m_BlastResTaxInfo->seqTaxInfoMap.begin(); it!=m_BlastResTaxInfo->seqTaxInfoMap.end(); ++it) {
            for(size_t i = 0; i < it->second.seqInfoList.size(); i++) {
                SSeqInfo* seqInfo = it->second.seqInfoList[i];
                if(seqInfo) delete seqInfo;        
            }        
            delete m_BlastResTaxInfo;
        }
    }
    if(m_TaxTreeinfo) {
        delete m_TaxTreeinfo;
    }
    if(m_TaxFormatTemplates) {
        delete m_TaxFormatTemplates;
    }
    if(m_TaxClient) {
        m_TaxClient->Fini();
        delete m_TaxClient;
    } 
}


// Here's a tree A drawing that will be used to explain trversing modes  
//              /|   
//             B C  
//            /|   
//           D E
// This function arranges 'upward' traverse mode when lower nodes are  
// processed first. The sequence of calls to I4Each functions for  
// iterator at the node A whould be:  
//   LevelBegin( A )  
//     LevelBegin( B )  
//       Execute( D ), Execute( E )  
//     LevelEnd( B ), Execute( B )  
//     Execute( C )  
//   LevelEnd( A ), Execute( A )
class CUpwardTreeFiller : public ITreeIterator::I4Each
{
public:

    virtual ~CUpwardTreeFiller() {} 
    CUpwardTreeFiller(CTaxFormat::TSeqTaxInfoMap &seqAlignTaxInfoMap)        
        : m_SeqAlignTaxInfoMap(seqAlignTaxInfoMap),
          m_Curr(NULL)          
    {
        m_TreeTaxInfo = new CTaxFormat::SBlastResTaxInfo;    
        m_Debug = false;
    }    

    CTaxFormat::SBlastResTaxInfo* GetTreeTaxInfo(void){ return m_TreeTaxInfo;}
    void SetDebugMode(bool debug) {m_Debug = debug;}

    ITreeIterator::EAction LevelBegin(const ITaxon1Node* tax_node)
    {
        x_InitTaxInfo(tax_node); // sets m_Curr        
        x_PrintTaxInfo("Begin branch");
        m_Curr->numChildren = 0;
        m_Curr->numHits = 0;
        m_Curr->numOrgs = 0;

        if(m_Nodes.size() > 0) { //there is parent
            CTaxFormat::STaxInfo* par = m_Nodes.top();
            par->numChildren++; 
        }    
        m_Nodes.push(m_Curr); //Push current taxid node info into stack
        m_Curr = NULL;
                
        return ITreeIterator::eOk;
    }

    ITreeIterator::EAction Execute(const ITaxon1Node* tax_node)
    {
        int taxid = tax_node->GetTaxId();
               
        //if m_Curr != NULL there was Level End - we are at branch processing
        int currTaxid = (m_Curr) ? m_Curr->taxid : 0;
        bool useTaxid = false;                       
        if(taxid == currTaxid) {//branch processing            
            m_Curr->numHits += m_Curr->seqInfoList.size();                                                            
            useTaxid = (m_Curr->numChildren > 1 || m_Curr->seqInfoList.size() > 0);
            if(!useTaxid) {                
                x_PrintTaxInfo("Removed branch");                
            }            
            if(m_Curr->seqInfoList.size() > 0) {
                m_Curr->numOrgs++;
                if(!m_Curr->taxidList.empty()) m_Curr->taxidList += ",";
                m_Curr->taxidList += NStr::NumericToString(m_Curr->taxid);
            }
        }
        else { // - terminal node
            x_InitTaxInfo(tax_node); // sets m_Curr                                    
            x_PrintTaxInfo("Terminal node");
            m_Curr->numHits = m_Curr->seqInfoList.size();             
            m_Curr->numOrgs = 1;
            m_Curr->numChildren = 0;
            m_Curr->taxidList = NStr::NumericToString(m_Curr->taxid);
            useTaxid = true;         
        }          
        if(m_Nodes.size() > 0) { //there is a parent            
            CTaxFormat::STaxInfo* par = m_Nodes.top();
            par->numHits += m_Curr->numHits;            
            par->numOrgs += m_Curr->numOrgs;                        

            if(!par->taxidList.empty()) par->taxidList += ",";
            par->taxidList += m_Curr->taxidList;            
            if(m_Curr->seqInfoList.size() > 0) par->numChildren++;            
        }          
        if(useTaxid) {
            x_InitTreeTaxInfo();//Adds taxid to the map
        }
        if(taxid != currTaxid) {            
            m_Curr = NULL;
        }        
        return ITreeIterator::eOk;
    }

    ITreeIterator::EAction LevelEnd(const ITaxon1Node* tax_node)
    {
        m_Curr = m_Nodes.top();                        
        x_PrintTaxInfo("End branch");
        m_Nodes.pop();                
        return ITreeIterator::eOk;
    }

    void x_InitTaxInfo(const ITaxon1Node* tax_node);
    void x_InitTreeTaxInfo(void);
    void x_PrintTaxInfo(string header) {
        if(m_Debug) {
            cerr << header << " for taxid: " << m_Curr->taxid << " " << m_Curr->scientificName << endl;        
        }
    }
       
    CTaxFormat::TSeqTaxInfoMap       m_SeqAlignTaxInfoMap; ///< Map containing information for taxids and corresponding sequnces in alignment
    CTaxFormat::SBlastResTaxInfo     *m_TreeTaxInfo;       ///< SBlastResTaxInfo Map containing information for all taxids in common tree, intermidiate nodes with no hits or only 1 child are removed
    CTaxFormat::STaxInfo*            m_Curr;        
    stack<CTaxFormat::STaxInfo*>     m_Nodes;        
    bool                             m_Debug;
};



//   Execute( A ), LevelBegin( A )
//     Execute( B ), LevelBegin( B )  
//       Execute( D ), Execute( E )  
//     LevelEnd( B )  
//     Execute( C )
//   LevelEnd( A )
class CDownwardTreeFiller : public ITreeIterator::I4Each
{
public:
    void SetDebugMode(bool debug) {m_Debug = debug;}
    virtual ~CDownwardTreeFiller() { }
    CDownwardTreeFiller(CTaxFormat::TSeqTaxInfoMap *treeTaxInfoMap)        
        : m_TreeTaxInfoMap(treeTaxInfoMap)          
    {
        m_Depth = 0;        
        m_Debug = false;
    }    
    

    ITreeIterator::EAction LevelBegin(const ITaxon1Node* tax_node)
    {
        int taxid = tax_node->GetTaxId();        
        if((*m_TreeTaxInfoMap).count(taxid) != 0 ) {//sequence is in alignment            
            m_Depth++;
            m_Lineage.push_back(taxid); //Push current taxid info into stack
        }        
        x_PrintTaxInfo("Begin branch",tax_node);        
                
        return ITreeIterator::eOk;
    }

    ITreeIterator::EAction Execute(const ITaxon1Node* tax_node)
    {
        int taxid = tax_node->GetTaxId();        
        if((*m_TreeTaxInfoMap).count(taxid) != 0 ) {//sequence is in alignment            
            (*m_TreeTaxInfoMap)[taxid].depth = m_Depth;
            for(size_t i = 0; i < m_Lineage.size(); i++) {
                (*m_TreeTaxInfoMap)[taxid].lineage.assign(m_Lineage.begin(),m_Lineage.end());
            }
        }     
        x_PrintTaxInfo("Execute branch",tax_node);                
        return ITreeIterator::eOk;
    }

    ITreeIterator::EAction LevelEnd(const ITaxon1Node* tax_node)
    {
        int taxid = tax_node->GetTaxId();        
        if((*m_TreeTaxInfoMap).count(taxid) != 0 ) {//sequence is in alignment            
            m_Depth--;
            m_Lineage.pop_back();
        }     
        x_PrintTaxInfo("End branch",tax_node);                
        return ITreeIterator::eOk;
    }
               
    void x_PrintTaxInfo(string header, const ITaxon1Node* tax_node) {        
        if(m_Debug) {
            string lineage;
            for(size_t j = 0; j < m_Lineage.size(); j++) {
                if(!lineage.empty()) lineage += ",";
                lineage += NStr::NumericToString(m_Lineage[j]);
            }            
            cerr << header << " for taxid: " << tax_node->GetTaxId() << " " << tax_node->GetName() << " depth: " <<  m_Depth << " lineage: " << lineage << endl;
        }
    }

    CTaxFormat::TSeqTaxInfoMap       *m_TreeTaxInfoMap;    
    int                              m_Depth;
    vector <int>                     m_Lineage;
    bool                             m_Debug;
};




void CTaxFormat::DisplayOrgReport(CNcbiOstream& out) 
{
    string orgReportData;  
    string taxIdToSeqsMap;

    for(size_t i = 0; i < m_BlastResTaxInfo->orderedTaxids.size(); i++) {
        int taxid = m_BlastResTaxInfo->orderedTaxids[i];
        STaxInfo seqsForTaxID = m_BlastResTaxInfo->seqTaxInfoMap[taxid];
        
        string orgHeader = (m_ConnectToTaxServer) ? m_TaxFormatTemplates->orgReportOrganismHeader : kOrgReportOrganismHeaderNoTaxConnect;
        orgHeader = x_MapTaxInfoTemplate(orgHeader,seqsForTaxID);
        string prevTaxid,nextTaxid, hidePrevTaxid, hideNextTaxid, hideTop;
        const string kDisabled = "disabled=\"disabled\"";
        if(i == 0) {        
            hidePrevTaxid = kDisabled;
            hideTop = kDisabled;            
        }
        if (i == m_BlastResTaxInfo->orderedTaxids.size() - 1) {        
            hideNextTaxid = kDisabled;            
        }        
        if(i > 0) {            
            prevTaxid = NStr::NumericToString(m_BlastResTaxInfo->orderedTaxids[i - 1]);
        }
        if(i < m_BlastResTaxInfo->orderedTaxids.size() - 1) {
            nextTaxid = NStr::NumericToString(m_BlastResTaxInfo->orderedTaxids[i + 1]);            
        }
        

        orgHeader = CAlignFormatUtil::MapTemplate(orgHeader,"next_taxid",nextTaxid);
        orgHeader = CAlignFormatUtil::MapTemplate(orgHeader,"disable_nexttaxid",hideNextTaxid);
        orgHeader = CAlignFormatUtil::MapTemplate(orgHeader,"prev_taxid",prevTaxid);
        orgHeader = CAlignFormatUtil::MapTemplate(orgHeader,"disable_prevtaxid",hidePrevTaxid);
        orgHeader = CAlignFormatUtil::MapTemplate(orgHeader,"disable_top",hideTop);
        
        string orgReportSeqInfo;        
        for(size_t j = 0; j < seqsForTaxID.seqInfoList.size(); j++) {
            SSeqInfo *seqInfo = seqsForTaxID.seqInfoList[j];            
            string orgReportTableRow = x_MapSeqTemplate(m_TaxFormatTemplates->orgReportTableRow,seqInfo);            
            orgReportSeqInfo += orgReportTableRow;
        }

        string oneOrgInfo = orgHeader + orgReportSeqInfo;
        orgReportData += oneOrgInfo; 

        string taxIdToSeqsRow = CAlignFormatUtil::MapTemplate(m_TaxFormatTemplates->taxIdToSeqsMap,"giList",seqsForTaxID.giList);
        taxIdToSeqsRow = CAlignFormatUtil::MapTemplate(taxIdToSeqsRow,"taxid",taxid);
        taxIdToSeqsMap += taxIdToSeqsRow;
    }
    orgReportData = CAlignFormatUtil::MapTemplate(m_TaxFormatTemplates->orgReportTable,"table_rows",orgReportData);
    orgReportData = CAlignFormatUtil::MapTemplate(orgReportData,"taxidToSeqsMap",taxIdToSeqsMap);    
    out << orgReportData;
}

 

void CTaxFormat::DisplayLineageReport(CNcbiOstream& out) 
{
    x_InitLineageReport();    
    
    bool header = true;    
    string lngReportRows = m_TaxFormatTemplates->lineageReportTableHeader;//!!!Do the same for organism report
    ITERATE(list <STaxInfo>, iter, m_AlnLineageTaxInfo) {             
            string lngReportTableRow;
            unsigned int depth = 0;
            STaxInfo alnSeqTaxInfo = *iter;
//            taxInfo.lineage.push_back(iter->taxid);            
            for(size_t i = 0; i< alnSeqTaxInfo.lineage.size();i++) {
                int taxid = alnSeqTaxInfo.lineage[i];                
                if(header) {                                        
                    STaxInfo taxInfo = GetTaxTreeInfo(taxid);                   

                    lngReportTableRow = x_MapTaxInfoTemplate(m_TaxFormatTemplates->lineageReportOrganismHeader,taxInfo,depth);
                    

                    lngReportTableRow = CAlignFormatUtil::MapTemplate(lngReportTableRow,"taxidList",taxInfo.taxidList);  
                    lngReportTableRow = CAlignFormatUtil::MapTemplate(lngReportTableRow,"descr",taxInfo.scientificName + " sequences");  
                    lngReportTableRow = CAlignFormatUtil::MapTemplate(lngReportTableRow,"descr_abbr",taxInfo.scientificName + " sequences");                              
                    lngReportRows += lngReportTableRow;
                }
                depth++;
            }    
            
            lngReportTableRow = x_MapTaxInfoTemplate(m_TaxFormatTemplates->lineageReportTableRow,alnSeqTaxInfo,depth);
            
            lngReportTableRow = x_MapSeqTemplate(lngReportTableRow,alnSeqTaxInfo);                                                                                       
            lngReportRows += lngReportTableRow;
            header = false;            
     }
     string lngReportData = CAlignFormatUtil::MapTemplate(m_TaxFormatTemplates->lineageReportTable,"table_rows",lngReportRows);
     out << lngReportData;
}



void CTaxFormat::DisplayTaxonomyReport(CNcbiOstream& out) 
{
    x_InitTaxReport();    
    vector <int> taxTreeTaxids = GetTaxTreeTaxIDs();        
    string taxReportRows;
    taxReportRows = m_TaxFormatTemplates->taxonomyReportTableHeader;//!!!Do the same for organism report

    for(size_t i = 0; i < taxTreeTaxids.size(); i++) {
        int taxid = taxTreeTaxids[i];
        STaxInfo taxInfo = GetTaxTreeInfo(taxid);     
        string taxReportTableRow;             
        if(isTaxidInAlign(taxid)) {                                                     
            taxReportTableRow = x_MapSeqTemplate(m_TaxFormatTemplates->taxonomyReportTableRow,taxInfo);                                                        
        }                       
        else {
            taxReportTableRow = CAlignFormatUtil::MapTemplate(m_TaxFormatTemplates->taxonomyReportOrganismHeader,"taxidList",taxInfo.taxidList);  
            taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"descr",taxInfo.scientificName + " sequences");  
            taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"descr_abbr",taxInfo.scientificName + " sequences");              
        }
        taxReportTableRow = x_MapTaxInfoTemplate(taxReportTableRow,taxInfo,taxInfo.depth);               
        taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"numhits",taxInfo.numHits);                    
        taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"numOrgs",taxInfo.numOrgs);        
        string lineage,parentTaxid;
        for(size_t i = 0; i < taxInfo.lineage.size(); i++) {
            if(!lineage.empty()) lineage += " ";
            lineage += "partx_" + NStr::NumericToString(taxInfo.lineage[i]);
            if(i == taxInfo.lineage.size() - 1) {
                parentTaxid = NStr::NumericToString(taxInfo.lineage[i]);
            }
        }        
        taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"lineage",lineage);
        taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"parTaxid",parentTaxid);
        if(taxInfo.numChildren > 0) {
            taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"showChildren","showing");
        }
        else {
            taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"showChildren","hidden");
        }

        taxReportRows += taxReportTableRow;            
     }
     string taxReportData = CAlignFormatUtil::MapTemplate(m_TaxFormatTemplates->taxonomyReportTable,"table_rows",taxReportRows);
     out << taxReportData;
}


void CTaxFormat::x_InitTaxClient(void)
{
    if(!m_TaxClient) {
        m_TaxClient = new CTaxon1();
        m_TaxClient->Init();  

        if(!m_TaxClient->IsAlive()) {
            const string& err = m_TaxClient->GetLastError();
            NCBI_THROW(CException, eUnknown,"Cannot connect to tax server. " + err);
        }
    }        
}


void CTaxFormat::x_InitTaxReport(void)
{
    if(!m_TaxTreeLoaded) {
        x_LoadTaxTree();    
    }
    if(!m_TaxTreeinfo) {
        x_InitOrgTaxMetaData();    
    }
}

void CTaxFormat::x_InitLineageReport(void)
{
    if(!m_TaxTreeLoaded) {
        x_LoadTaxTree(); 
    }
    if(!m_TaxTreeinfo) {
        x_InitOrgTaxMetaData();
    }
    x_InitLineageMetaData();
}


void CTaxFormat::x_LoadTaxTree(void)
{
    x_InitTaxClient();                
    if(!m_TaxTreeLoaded) {
        vector <int> taxidsToRoot;    
        vector <int> alignTaxids = GetAlignTaxIDs();

        bool tax_load_ok = false;
        if(m_TaxClient->IsAlive()) {          
            m_TaxClient->GetPopsetJoin(alignTaxids,taxidsToRoot);
            //taxidsToRoot contain all taxids excluding alignTaxids        

            //Adding alignTaxids
            for(size_t i = 0; i < alignTaxids.size(); i++) {
                int tax_id = alignTaxids[i];                                    

                if(!m_TaxClient->IsAlive()) break;
                const ITaxon1Node* tax_node = NULL;
                tax_load_ok |= m_TaxClient->LoadNode(tax_id,&tax_node);                                                        
                if(!tax_load_ok) break;                

                if(tax_node && tax_node->GetTaxId() != tax_id) {
                    int newTaxid = tax_node->GetTaxId();
                    if(m_Debug) {
                        cerr << "*******TAXID MISMATCH: changing " << tax_id << " to " << tax_node->GetTaxId() << "-" << endl;
                    }
                    STaxInfo &taxInfo = GetAlignTaxInfo(tax_id);
                    taxInfo.taxid = newTaxid;
                    for(size_t j = 0; i < taxInfo.seqInfoList.size(); j++) {
                        SSeqInfo* seqInfo = taxInfo.seqInfoList[j];                        
                        seqInfo->taxid = newTaxid;
                    }        
                    m_BlastResTaxInfo->seqTaxInfoMap.insert(CTaxFormat::TSeqTaxInfoMap::value_type(newTaxid,taxInfo));  
                    m_BlastResTaxInfo->orderedTaxids[i] = newTaxid;                    
                    m_BlastResTaxInfo->seqTaxInfoMap.erase(tax_id);
                }
            }
        }
        if(m_TaxClient->IsAlive() && tax_load_ok) {          
            ITERATE(vector<int>, it, taxidsToRoot) {
                int tax_id = *it;                 

                if(!m_TaxClient->IsAlive()) break;
                tax_load_ok |= m_TaxClient->LoadNode(tax_id);

                if(!tax_load_ok) break;
            }            
        }
        if (!tax_load_ok) {
            NCBI_THROW(CException, eUnknown,"Taxonomic load was not successfull.");
        }
        m_TaxTreeLoaded = true;
            
        if(m_TaxClient->IsAlive()) {
            m_TreeIterator = m_TaxClient->GetTreeIterator();
        }
        else {
            const string& err = m_TaxClient->GetLastError();
            NCBI_THROW(CException, eUnknown,"Cannot connect to tax server. " + err);
        }
    }
}


void CTaxFormat::x_PrintTaxInfo(vector <int> taxids, string title)
{
    if(m_Debug) {
        cerr << "******" << title << "**********" << endl;
        for(size_t i = 0; i < taxids.size(); i++) {
            int taxid = taxids[i];                            
            STaxInfo taxInfo = GetTaxTreeInfo(taxid);                         
            string lineage;
            for(size_t j = 0; j < taxInfo.lineage.size(); j++) {
                if(!lineage.empty()) lineage += ",";
                lineage += NStr::NumericToString(taxInfo.lineage[j]);
            }
            cerr << "taxid=" << taxid << " " << taxInfo.scientificName << " " << taxInfo.blastName << " " << "depth: " <<  taxInfo.depth << " numHits: " << taxInfo.numHits << " numOrgs: " << taxInfo.numOrgs << " numChildren: " << taxInfo.numChildren << " lineage: " << lineage << endl;                        
        }
    }
}



void CTaxFormat::x_InitOrgTaxMetaData(void)
{
    if(!m_TreeIterator.Empty()) {        
        CUpwardTreeFiller upwFiller(m_BlastResTaxInfo->seqTaxInfoMap);
        upwFiller.SetDebugMode(m_Debug);
        //Create m_TaxTreeinfo with all taxids participating in common tree
        m_TreeIterator->TraverseUpward(upwFiller);            
        m_TaxTreeinfo = upwFiller.GetTreeTaxInfo();
        std::reverse(m_TaxTreeinfo->orderedTaxids.begin(),m_TaxTreeinfo->orderedTaxids.end());                            
        
        //Add depth and linage infor to m_TaxTreeinfo
        CDownwardTreeFiller dwnwFiller(&m_TaxTreeinfo->seqTaxInfoMap);
        dwnwFiller.SetDebugMode(m_Debug);
        m_TreeIterator->TraverseDownward(dwnwFiller);
        vector <int> taxTreeTaxids = GetTaxTreeTaxIDs();

        x_PrintTaxInfo(taxTreeTaxids,"Taxonomy tree");                     
    }
}

void CTaxFormat::x_InitBlastNameTaxInfo(STaxInfo &taxInfo)
{
    
    if(m_TaxClient && m_TaxClient->IsAlive()) {
        m_TaxClient->GetBlastName(taxInfo.taxid,taxInfo.blastName);
        taxInfo.blNameTaxid = m_TaxClient->SearchTaxIdByName(taxInfo.blastName,CTaxon1::eSearch_Exact);        
    }
}

static string s_TaxidLinageToString(CTaxFormat::STaxInfo const& info)
{
    vector <int> lineage = info.lineage;            
    string strTaxidLineage;
    for(size_t j = 0; j < lineage.size(); j++) {
        if(!strTaxidLineage.empty()) strTaxidLineage += " ";
        strTaxidLineage += NStr::NumericToString(lineage[j]);
    }            
    return strTaxidLineage;
}


static bool s_SortByLinageToBestHit(CTaxFormat::STaxInfo const& info1,
                                    CTaxFormat::STaxInfo const& info2)
{
    string lineage1 = s_TaxidLinageToString(info1);
    string lineage2 = s_TaxidLinageToString(info2);
    return lineage1 > lineage2;
}


static vector <int> s_InitAlignHitLineage(vector <int> bestHitLinage, struct CTaxFormat::STaxInfo &taxTreeInfo)
{
    vector <int> alignHitLineage;
    for(size_t i = 0; i < bestHitLinage.size(); i++) {
        if(taxTreeInfo.lineage.size() - 1 >= i) {
          int taxid = bestHitLinage[i];
          if(taxTreeInfo.lineage[i] == taxid) {
              alignHitLineage.push_back(taxid);
          }          
        }
    }        
    return alignHitLineage;
}



void CTaxFormat::x_PrintLineage(void)
{
    if(m_Debug) {
        cerr << "*********Lineage*********" << endl;
        ITERATE(list <STaxInfo>, iter, m_AlnLineageTaxInfo) {             
            int taxid = iter->taxid;    
            string name = iter->scientificName;            
            cerr << "taxid" << taxid << " " << name << ": "; 
            for(size_t i = 0; i< iter->lineage.size();i++) {                
                int lnTaxid = iter->lineage[i];            
                cerr << " " << lnTaxid << " " << GetTaxTreeInfo(lnTaxid).scientificName + "," ;
            }
            cerr << endl;
        }
    }
}

void CTaxFormat::x_InitLineageMetaData(void)
{
        int betsHitTaxid = m_BlastResTaxInfo->orderedTaxids[0];
        m_BestHitLineage = GetTaxTreeInfo(betsHitTaxid).lineage; 
        vector <int> alignTaxids = GetAlignTaxIDs();
        
        list <STaxInfo> alnTaxInfo;
        for(size_t i = 0; i < alignTaxids.size(); i++) {                    
            STaxInfo taxInfo = GetTaxTreeInfo(alignTaxids[i]);
            vector <int> alignHitLineage = s_InitAlignHitLineage(m_BestHitLineage,taxInfo);
            taxInfo.lineage = alignHitLineage;
            x_InitBlastNameTaxInfo(taxInfo);                                    
            m_AlnLineageTaxInfo.push_back(taxInfo);            
        }        
        m_AlnLineageTaxInfo.sort(s_SortByLinageToBestHit);                
        ITERATE(list <STaxInfo>, iter, m_AlnLineageTaxInfo) {            
            for(size_t i = 0; i< iter->lineage.size();i++) {                
                int lnTaxid = iter->lineage[i];  
                STaxInfo &taxInfo = GetTaxTreeInfo(lnTaxid);
                x_InitBlastNameTaxInfo(taxInfo);                                                    
            }            
        }
        x_PrintLineage();                
}

void CUpwardTreeFiller::x_InitTaxInfo(const ITaxon1Node* tax_node)
{
    
    CTaxFormat::STaxInfo *info = new CTaxFormat::STaxInfo;        

    
    int taxid = tax_node->GetTaxId();    
    if(m_SeqAlignTaxInfoMap.count(taxid) != 0 ) {//sequence is in alignment
        info->seqInfoList = m_SeqAlignTaxInfoMap[taxid].seqInfoList;        
    }   

    info->taxid = taxid;
    info->scientificName = tax_node->GetName();
    info->blastName = tax_node->GetBlastName();            
    m_Curr = info;
}

void CUpwardTreeFiller::x_InitTreeTaxInfo(void)
{
    int taxid = m_Curr->taxid;
    if(m_TreeTaxInfo->seqTaxInfoMap.count(taxid) == 0) {              
        CTaxFormat::STaxInfo seqsForTaxID;
        seqsForTaxID.taxid = m_Curr->taxid;
        seqsForTaxID.commonName = m_Curr->commonName;
        seqsForTaxID.scientificName = m_Curr->scientificName;                
        seqsForTaxID.blastName = m_Curr->blastName;
        seqsForTaxID.seqInfoList = m_Curr->seqInfoList;
        seqsForTaxID.taxidList  = m_Curr->taxidList;        
        seqsForTaxID.numHits = m_Curr->numHits;
        seqsForTaxID.numOrgs = m_Curr->numOrgs;
        seqsForTaxID.numChildren = m_Curr->numChildren;
        m_TreeTaxInfo->seqTaxInfoMap.insert(CTaxFormat::TSeqTaxInfoMap::value_type(taxid,seqsForTaxID));  
        m_TreeTaxInfo->orderedTaxids.push_back(taxid);
	}    
}


bool CTaxFormat::isTaxidInAlign(int taxid)
{
    bool ret = false;
    if(m_BlastResTaxInfo->seqTaxInfoMap.count(taxid) != 0 && m_BlastResTaxInfo->seqTaxInfoMap[taxid].seqInfoList.size() > 0) {
        ret = true;
    }
    return ret;    
}


CTaxFormat::SSeqInfo *CTaxFormat::x_FillTaxDispParams(const CBioseq_Handle& bsp_handle,
                                    double bits, 
                                    double evalue) 
{
    SSeqInfo *seqInfo = new SSeqInfo;
	seqInfo->gi = FindGi(bsp_handle.GetBioseqCore()->GetId());
	seqInfo->seqID = FindBestChoice(bsp_handle.GetBioseqCore()->GetId(),CSeq_id::WorstRank);
	seqInfo->label =  CAlignFormatUtil::GetLabel(seqInfo->seqID);//Just accession without db part like ref| or pdbd|		
    seqInfo->bit_score = bits;
    seqInfo->evalue = evalue;        
    seqInfo->displGi = seqInfo->gi;
    seqInfo->taxid = 0;	
	seqInfo->title = CDeflineGenerator().GenerateDefline(bsp_handle);			    
    return seqInfo;
}





CTaxFormat::SSeqInfo *CTaxFormat::x_FillTaxDispParams(const CRef< CBlast_def_line > &bdl,
                                     const CBioseq_Handle& bsp_handle,
                                     double bits, 
                                     double evalue,                                    
		                             list<TGi>& use_this_gi)
                                                                        
{
    SSeqInfo *seqInfo = NULL;

	const list<CRef<CSeq_id> > ids = bdl->GetSeqid();
	TGi gi =  CAlignFormatUtil::GetGiForSeqIdList(ids);
    TGi gi_in_use_this_gi = ZERO_GI;
    
    ITERATE(list<TGi>, iter_gi, use_this_gi){
        if(gi == *iter_gi){
            gi_in_use_this_gi = *iter_gi;
            break;
        }
    }
	if(use_this_gi.empty() || gi_in_use_this_gi > ZERO_GI) {
        
		seqInfo = new SSeqInfo();
		seqInfo->gi =  gi;
		seqInfo->seqID = FindBestChoice(ids, CSeq_id::WorstRank);		
		seqInfo->label =  CAlignFormatUtil::GetLabel(seqInfo->seqID);//Just accession without db part like ref| or pdbd|		
        seqInfo->bit_score = bits;
        seqInfo->evalue = evalue;         
	    int taxid = 0;
		
		if(bdl->IsSetTaxid() &&  bdl->CanGetTaxid()){
		    taxid = bdl->GetTaxid();
		}
        seqInfo->taxid = taxid;
        		
		
		if(bdl->IsSetTitle()){
			seqInfo->title = bdl->GetTitle();
		}
        if(seqInfo->title.empty()) {
            seqInfo->title = CDeflineGenerator().GenerateDefline(bsp_handle);            
        }        
	}    	
    return seqInfo;
}


void CTaxFormat::x_InitTaxInfoMap(void)
{
    int score, sum_n, num_ident;
    double bits, evalue;
    list<TGi> use_this_gi;
    
    m_BlastResTaxInfo = new SBlastResTaxInfo;
    CConstRef<CSeq_id> previousId,subid;    
    ITERATE(CSeq_align_set::Tdata, iter, m_SeqalignSetRef->Get()){ 

        subid = &((*iter)->GetSeq_id(1));

        if(!previousId.Empty() && subid->Match(*previousId)) continue;        

        CAlignFormatUtil::GetAlnScores(**iter, score, bits, evalue,
                                       sum_n, num_ident, use_this_gi);        
        try {
            const CBioseq_Handle& handle = m_Scope.GetBioseqHandle(*subid);            
            if( !handle ) continue;

            const CRef<CBlast_def_line_set> bdlRef =  CSeqDB::ExtractBlastDefline(handle);    
            const list< CRef< CBlast_def_line > > &bdl = (bdlRef.Empty()) ? list< CRef< CBlast_def_line > >() : bdlRef->Get();
            		    
            if(!bdl.empty()){ //no blast defline struct, should be no such case now
                                
                TGi dispGI = ZERO_GI;
                for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin(); iter != bdl.end(); iter++){                                                        
                    SSeqInfo *seqInfo = x_FillTaxDispParams(*iter,handle,bits,evalue,use_this_gi);
                    if(seqInfo) {
                        if(dispGI == ZERO_GI){
                            dispGI = CAlignFormatUtil::GetDisplayIds(bdl,*seqInfo->seqID,use_this_gi);                            
                        }                                        
                        seqInfo->displGi = dispGI;
                        x_InitBlastDBTaxInfo(seqInfo);
                    }
                    
                }
            }                                    
            else 
            {      
                SSeqInfo *seqInfo = x_FillTaxDispParams(handle,bits,evalue);                
                if(seqInfo) {
                    x_InitBlastDBTaxInfo(seqInfo);
                }
            }              
            previousId = subid;
        } 
        catch (const CException&){
            continue;
        }                
    }        
}

void CTaxFormat::x_InitBlastDBTaxInfo(CTaxFormat::SSeqInfo *seqInfo)    
{
    int taxid = seqInfo->taxid;
    if(m_BlastResTaxInfo->seqTaxInfoMap.count(taxid) == 0){		                            
        SSeqDBTaxInfo taxInfo;
        CSeqDB::GetTaxInfo(taxid, taxInfo);                

        STaxInfo seqsForTaxID;
        seqsForTaxID.taxid = taxid;
        seqsForTaxID.commonName = taxInfo.common_name;
        seqsForTaxID.scientificName = taxInfo.scientific_name;
        seqsForTaxID.blastName = taxInfo.blast_name;                    
        seqsForTaxID.giList = NStr::NumericToString(seqInfo->gi);        
        seqsForTaxID.accList = seqInfo->label;
        x_InitBlastNameTaxInfo(seqsForTaxID);
        seqsForTaxID.seqInfoList.push_back(seqInfo);
		m_BlastResTaxInfo->seqTaxInfoMap.insert(CTaxFormat::TSeqTaxInfoMap::value_type(taxid,seqsForTaxID));  
        m_BlastResTaxInfo->orderedTaxids.push_back(taxid);
	}
    else {
        m_BlastResTaxInfo->seqTaxInfoMap[taxid].giList += ",";
        m_BlastResTaxInfo->seqTaxInfoMap[taxid].giList += NStr::NumericToString(seqInfo->gi);

        m_BlastResTaxInfo->seqTaxInfoMap[taxid].accList += ", ";
        m_BlastResTaxInfo->seqTaxInfoMap[taxid].accList += seqInfo->label;

        m_BlastResTaxInfo->seqTaxInfoMap[taxid].seqInfoList.push_back(seqInfo);
    }    
}


string CTaxFormat::x_MapSeqTemplate(string seqTemplate, STaxInfo &taxInfo)
{
    SSeqInfo *seqInfo = taxInfo.seqInfoList[0];
    int taxid = taxInfo.taxid;
    
    string reportTableRow = CAlignFormatUtil::MapTemplate(seqTemplate,"acc",m_BlastResTaxInfo->seqTaxInfoMap[taxid].accList);        
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"descr",seqInfo->title);    
    reportTableRow = x_MapSeqTemplate(reportTableRow,seqInfo);    
    
    return reportTableRow;
}


string CTaxFormat::x_MapSeqTemplate(string seqTemplate, SSeqInfo *seqInfo)
{
    string evalue_buf, bit_score_buf, total_bit_score_buf, raw_score_buf;
    CAlignFormatUtil::GetScoreString(seqInfo->evalue, seqInfo->bit_score, 0, 0,
                                             evalue_buf, bit_score_buf, total_bit_score_buf,raw_score_buf);

    string reportTableRow = CAlignFormatUtil::MapTemplate(seqTemplate,"acc",seqInfo->label);    
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"gi",NStr::NumericToString(seqInfo->gi));
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"disp_gi",NStr::NumericToString(seqInfo->displGi));    
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"descr",seqInfo->title);
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"descr_abbr",seqInfo->title.substr(0,60));//for standalone output    
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"score",bit_score_buf);
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"evalue",evalue_buf);           
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"rid",m_Rid);
    return reportTableRow;
}

string CTaxFormat::x_MapTaxInfoTemplate(string tableRowTemplate,STaxInfo &taxInfo,unsigned int depth)
{
    string reportTableRow = CAlignFormatUtil::MapTemplate(tableRowTemplate,"blast_name_link",m_TaxFormatTemplates->blastNameLink);
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"scientific_name",taxInfo.scientificName);
    string commonName = (taxInfo.commonName != taxInfo.scientificName) ? "(" + taxInfo.commonName + ")" : "";    
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"common_name",commonName);        
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"blast_name",taxInfo.blastName);                    
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"bl_taxid",taxInfo.blNameTaxid);
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"taxid",taxInfo.taxid);
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"taxBrowserURL",m_TaxBrowserURL);    
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"rid",m_Rid);
    int numHits = taxInfo.seqInfoList.size();
    numHits = (numHits > 0) ? numHits : taxInfo.numHits;
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"numhits",numHits);                        

    string shift;    
    for(size_t i = 0; i < depth; i++) {
        shift += ".";
    }
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"depth",shift);                                             
    
    return reportTableRow;
}



END_SCOPE(align_format)
END_NCBI_SCOPE
