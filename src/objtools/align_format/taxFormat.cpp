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
/*
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>
*/

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
        m_TaxBrowserURL = m_Reg->Get("TAXONOMY","TOOL_URL");
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
    /*setlocal nobomb */    
    if (m_ConfigFile) {
        delete m_ConfigFile;
    } 
    if (m_Reg) {
        delete m_Reg;
    }
    if(m_BlastResTaxInfo) {
        delete m_BlastResTaxInfo;
    }
    //Add memory handling
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

    virtual ~CUpwardTreeFiller() {} //add desctructor m_TaxClient->Fini()
    CUpwardTreeFiller(CTaxFormat::TSeqTaxInfoMap &seqAlignTaxInfoMap)        
        : m_SeqAlignTaxInfoMap(seqAlignTaxInfoMap),
          m_Curr(NULL)          
    {
        m_TreeTaxInfo = new CTaxFormat::SBlastResTaxInfo;    
    }    
    CTaxFormat::SBlastResTaxInfo* GetTreeTaxInfo(void){ return m_TreeTaxInfo;}

    ITreeIterator::EAction LevelBegin(const ITaxon1Node* tax_node)
    {
        x_InitTaxInfo(tax_node); // sets m_Curr
        //cerr << "******Begin taxid branch=" << m_Curr->taxid << " " << m_Curr->scientificName << endl;        
        m_Curr->numChildren = 0;
        m_Curr->numHits = 0;
        m_Curr->numOrgs = 0;

        if(m_Nodes.size() > 0) { //there is parent
            CTaxFormat::STaxInfo* par = m_Nodes.top();
            par->numChildren++; 
        }    
        m_Nodes.push(m_Curr); //Push current taxid info into stack
        m_Curr = NULL;
                
        return ITreeIterator::eOk;
    }

    ITreeIterator::EAction Execute(const ITaxon1Node* tax_node)
    {
        int taxid = tax_node->GetTaxId();
               
        //if(m_Curr) { //there was Level End - we are at brahch processing
        int currTaxid = (m_Curr) ? m_Curr->taxid : 0;
        bool useTaxid = false;                       
        if(taxid == currTaxid) {//branch processing            
            m_Curr->numHits += m_Curr->seqInfoList.size();                                                            
            useTaxid = (m_Curr->numChildren > 1 || m_Curr->seqInfoList.size() > 0);
            if(!useTaxid) {                
                //cerr << "******Removed taxid branch=" << m_Curr->taxid << " " << m_Curr->scientificName << endl;
            }            
            if(m_Curr->seqInfoList.size() > 0) {
                m_Curr->numOrgs++;
                if(!m_Curr->taxidList.empty()) m_Curr->taxidList += ",";
                m_Curr->taxidList += NStr::NumericToString(m_Curr->taxid);
            }
        }
        else {// - terminal node
            x_InitTaxInfo(tax_node); // sets m_Curr                        
            //cerr << "******taxid child=" << m_Curr->taxid <<  " " << m_Curr->scientificName << endl;            
            m_Curr->numHits = m_Curr->seqInfoList.size();             
            m_Curr->numOrgs = 1;
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
        //Check CTaxTreeBrowser code is Alive should be used        
        return ITreeIterator::eOk;
    }

    ITreeIterator::EAction LevelEnd(const ITaxon1Node* tax_node)
    {
        m_Curr = m_Nodes.top();                
        //cerr << "******End taxid branch=" << m_Curr->taxid << " " << m_Curr->scientificName << endl;        
        m_Nodes.pop();                
        return ITreeIterator::eOk;
    }

    void x_InitTaxInfo(const ITaxon1Node* tax_node);
    void x_InitTreeTaxInfo(void);

       
    CTaxFormat::TSeqTaxInfoMap       m_SeqAlignTaxInfoMap; ///< SBlastResTaxInfo structure containing information for taxids in alignment, orderedTaxids are ordered by highest score
    CTaxFormat::SBlastResTaxInfo     *m_TreeTaxInfo;       /// SBlastResTaxInfo Map containing information for all taxids in common tree, intermidiate nodes with no hits or only 1 child are removed
    CTaxFormat::STaxInfo*            m_Curr;        
    stack<CTaxFormat::STaxInfo*>     m_Nodes;        
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

    virtual ~CDownwardTreeFiller() { }
    CDownwardTreeFiller(CTaxFormat::TSeqTaxInfoMap *treeTaxInfoMap)        
        : m_TreeTaxInfoMap(treeTaxInfoMap)          
    {
        m_Depth = 0;        
    }    
    

    ITreeIterator::EAction LevelBegin(const ITaxon1Node* tax_node)
    {
        int taxid = tax_node->GetTaxId();
        string scientificName = tax_node->GetName();        
        if((*m_TreeTaxInfoMap).count(taxid) != 0 ) {//sequence is in alignment            
            m_Depth++;
            m_Lineage.push_back(taxid); //Push current taxid info into stack
        }        
        //cerr << "******Begin taxid branch=" << taxid << " " << scientificName << " depth" <<  m_Depth << endl;                
        
                
        return ITreeIterator::eOk;
    }

    ITreeIterator::EAction Execute(const ITaxon1Node* tax_node)
    {
        int taxid = tax_node->GetTaxId();
        string scientificName = tax_node->GetName();        
        if((*m_TreeTaxInfoMap).count(taxid) != 0 ) {//sequence is in alignment            
            (*m_TreeTaxInfoMap)[taxid].depth = m_Depth;
            for(size_t i = 0; i < m_Lineage.size(); i++) {
                (*m_TreeTaxInfoMap)[taxid].lineage.assign(m_Lineage.begin(),m_Lineage.end());
            }
        }        
        //cerr << "******Execute taxid branch=" << taxid << " " << scientificName << " depth" <<  m_Depth << endl;        
        return ITreeIterator::eOk;
    }

    ITreeIterator::EAction LevelEnd(const ITaxon1Node* tax_node)
    {
        int taxid = tax_node->GetTaxId();
        string scientificName = tax_node->GetName();        
        if((*m_TreeTaxInfoMap).count(taxid) != 0 ) {//sequence is in alignment            
            m_Depth--;
            m_Lineage.pop_back();
        }        
        //cerr << "******End taxid branch=" << taxid << " " << scientificName << " depth" <<  m_Depth << endl;                
        return ITreeIterator::eOk;
    }
               
    CTaxFormat::TSeqTaxInfoMap       *m_TreeTaxInfoMap;    
    int                              m_Depth;
    vector <int>                     m_Lineage;
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
            STaxInfo taxInfo = *iter;
            taxInfo.lineage.push_back(iter->taxid);
            for(size_t i = 0; i< taxInfo.lineage.size();i++) {
                int taxid = taxInfo.lineage[i];                
                if(header || isTaxidInAlign(taxid)) {
                    string shift;    
                    for(size_t j = 0; j < depth; j++) {
                        shift += ".";
                    }
                    STaxInfo taxInfo = GetTaxTreeInfo(taxid);                 
                    string tableRowTemplate = (header && !isTaxidInAlign(taxid)) ?  m_TaxFormatTemplates->lineageReportOrganismHeader : m_TaxFormatTemplates->lineageReportTableRow;

                    lngReportTableRow = x_MapTaxInfoTemplate(tableRowTemplate,taxInfo);                    
                    int numHits = taxInfo.seqInfoList.size();
                    numHits = (numHits > 0) ? numHits : taxInfo.numHits;
                    lngReportTableRow = CAlignFormatUtil::MapTemplate(lngReportTableRow,"numhits",numHits);                    
                    lngReportTableRow = CAlignFormatUtil::MapTemplate(lngReportTableRow,"depth",shift);                     
                    if(isTaxidInAlign(taxid)) {                                                     
                        lngReportTableRow = x_MapSeqTemplate(lngReportTableRow,taxInfo);                                            
                        header = false;
                    }                      
                    else {                                                
                        lngReportTableRow = CAlignFormatUtil::MapTemplate(lngReportTableRow,"taxidList",taxInfo.taxidList);  
                        lngReportTableRow = CAlignFormatUtil::MapTemplate(lngReportTableRow,"descr",taxInfo.scientificName + " sequences");  
                        lngReportTableRow = CAlignFormatUtil::MapTemplate(lngReportTableRow,"descr_abbr",taxInfo.scientificName + " sequences");              
                    }
                
                    lngReportRows += lngReportTableRow;
                }
                depth++;
            }                        
            
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
        string shift;    
        for(size_t j = 0; j < taxInfo.depth; j++) {
            shift += ".";
        }

        if(isTaxidInAlign(taxid)) {                                                     
            taxReportTableRow = x_MapSeqTemplate(m_TaxFormatTemplates->taxonomyReportTableRow,taxInfo);                                                        
        }                       
        else {
            taxReportTableRow = CAlignFormatUtil::MapTemplate(m_TaxFormatTemplates->taxonomyReportOrganismHeader,"taxidList",taxInfo.taxidList);  
            taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"descr",taxInfo.scientificName + " sequences");  
            taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"descr_abbr",taxInfo.scientificName + " sequences");              
        }
        taxReportTableRow = x_MapTaxInfoTemplate(taxReportTableRow,taxInfo);               
        taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"numhits",taxInfo.numHits);                    
        taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"numOrgs",taxInfo.numOrgs); 
        //taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"lineage",taxInfo.lineage); 
        taxReportTableRow = CAlignFormatUtil::MapTemplate(taxReportTableRow,"depth",shift);         

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
    //Add error processing use 
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
            m_TaxClient->GetPopsetJoin(alignTaxids,taxidsToRoot);
            //taxidsToRoot contain all taxids excluding alignTaxids
        
            bool tax_load_ok = false;
            //Adding alignTaxids
            for(size_t i = 0; i < alignTaxids.size(); i++) {
                int tax_id = alignTaxids[i];                                    
                tax_load_ok |= m_TaxClient->LoadNode(tax_id);                                        
            }
       
            ITERATE(vector<int>, it, taxidsToRoot) {
                int tax_id = *it;                        
                tax_load_ok |= m_TaxClient->LoadNode(tax_id);                                       
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


void CTaxFormat::x_PrintTaxInfo(int taxid)
{
    STaxInfo taxInfo = GetTaxTreeInfo(taxid); //m_TaxTreeinfo->seqTaxInfoMap[taxid];                         
    cerr << "*****taxid=" << taxid << " " << taxInfo.scientificName << " " << taxInfo.blastName << " " << "depth:" <<  taxInfo.depth << " numHits:" << taxInfo.numHits << " numOrgs:" << taxInfo.numOrgs << endl;            
}



void CTaxFormat::x_InitOrgTaxMetaData(void)
{
    if(!m_TreeIterator.Empty()) {        
        CUpwardTreeFiller upwFiller(m_BlastResTaxInfo->seqTaxInfoMap);

        //Create m_TaxTreeinfo with all taxids participating in common tree
        m_TreeIterator->TraverseUpward(upwFiller);            
        m_TaxTreeinfo = upwFiller.GetTreeTaxInfo();
        std::reverse(m_TaxTreeinfo->orderedTaxids.begin(),m_TaxTreeinfo->orderedTaxids.end());        
        for(size_t i = 0; i < m_TaxTreeinfo->orderedTaxids.size(); i++) {
            int taxid = m_TaxTreeinfo->orderedTaxids[i];
            x_PrintTaxInfo(taxid);            
        }         

        //Add depth and linage infor to m_TaxTreeinfo
        CDownwardTreeFiller dwnwFiller(&m_TaxTreeinfo->seqTaxInfoMap);
        m_TreeIterator->TraverseDownward(dwnwFiller);
        vector <int> taxTreeTaxids = GetTaxTreeTaxIDs();        
        for(size_t i = 0; i < taxTreeTaxids.size(); i++) {
            int taxid = taxTreeTaxids[i];
            x_PrintTaxInfo(taxid);            
        } 
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

void CTaxFormat::x_InitLineageMetaData(void)
{
        int betsHitTaxid = m_BlastResTaxInfo->orderedTaxids[0];
        m_BestHitLineage = GetTaxTreeInfo(betsHitTaxid).lineage; 
        vector <int> alignTaxids = GetAlignTaxIDs();
        
        list <STaxInfo> alnTaxInfo;
        for(size_t i = 0; i < alignTaxids.size(); i++) {                    
            STaxInfo &taxInfo = GetTaxTreeInfo(alignTaxids[i]);
            vector <int> alignHitLineage = s_InitAlignHitLineage(m_BestHitLineage,taxInfo);
            taxInfo.lineage = alignHitLineage;
            x_InitBlastNameTaxInfo(taxInfo);                                    
            m_AlnLineageTaxInfo.push_back(taxInfo);            
        }
        m_AlnLineageTaxInfo.sort(s_SortByLinageToBestHit);                
        ITERATE(list <STaxInfo>, iter, m_AlnLineageTaxInfo) {             
            int taxid = iter->taxid;    
            string name = iter->scientificName;            
            cerr << "taxid:" << taxid << " " << name;
            for(size_t i = 0; i< iter->lineage.size();i++) {                
                int lnTaxid = iter->lineage[i];
                STaxInfo &taxInfo = GetTaxTreeInfo(lnTaxid);
                x_InitBlastNameTaxInfo(taxInfo);                
                cerr << " " << lnTaxid << " " << GetTaxTreeInfo(lnTaxid).scientificName + "," ;
            }
            cerr << endl;
        }
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
        CTaxFormat::STaxInfo *seqsForTaxID = new CTaxFormat::STaxInfo();
        seqsForTaxID->taxid = m_Curr->taxid;
        seqsForTaxID->commonName = m_Curr->commonName;
        seqsForTaxID->scientificName = m_Curr->scientificName;                
        seqsForTaxID->blastName = m_Curr->blastName;
        seqsForTaxID->seqInfoList = m_Curr->seqInfoList;
        seqsForTaxID->taxidList  = m_Curr->taxidList;        
        seqsForTaxID->numHits = m_Curr->numHits;
        seqsForTaxID->numOrgs = m_Curr->numOrgs;
        m_TreeTaxInfo->seqTaxInfoMap.insert(CTaxFormat::TSeqTaxInfoMap::value_type(taxid,*seqsForTaxID));  
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

        STaxInfo *seqsForTaxID = new STaxInfo();
        seqsForTaxID->taxid = taxid;
        seqsForTaxID->commonName = taxInfo.common_name;
        seqsForTaxID->scientificName = taxInfo.scientific_name;
        seqsForTaxID->blastName = taxInfo.blast_name;                    
        seqsForTaxID->giList = NStr::NumericToString(seqInfo->gi);        
        seqsForTaxID->accList = seqInfo->label;
        x_InitBlastNameTaxInfo(*seqsForTaxID);
        seqsForTaxID->seqInfoList.push_back(seqInfo);
		m_BlastResTaxInfo->seqTaxInfoMap.insert(CTaxFormat::TSeqTaxInfoMap::value_type(taxid,*seqsForTaxID));  
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
    string descr = (taxInfo.seqInfoList.size() > 1) ? "Top hit: " : "";

    string reportTableRow = CAlignFormatUtil::MapTemplate(seqTemplate,"acc",m_BlastResTaxInfo->seqTaxInfoMap[taxid].accList);        
    reportTableRow = CAlignFormatUtil::MapTemplate(reportTableRow,"descr",descr + seqInfo->title);    
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

string CTaxFormat::x_MapTaxInfoTemplate(string tableRowTemplate,STaxInfo &taxInfo)
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
    return reportTableRow;
}



END_SCOPE(align_format)
END_NCBI_SCOPE
