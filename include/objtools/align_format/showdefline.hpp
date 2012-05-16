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
 * Author:  Jian Ye
 */

/** @file showdefline.hpp
 * Declares class to display one-line descriptions at the top of the BLAST
 * report.
 */

#ifndef OBJTOOLS_ALIGN_FORMAT___SHOWDEFLINE_HPP
#define OBJTOOLS_ALIGN_FORMAT___SHOWDEFLINE_HPP

#include <corelib/ncbireg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objtools/align_format/align_format_util.hpp>
#include <cgi/cgictx.hpp>

//forward declarations
struct CShowBlastDeflineTest;  //For internal test only

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(align_format)

/**
 * This class displays the defline for BLAST result.
 *
 * Example:
 * @code
 * int defline_option = 0;
 * defline_option += CShowBlastDefline::eShowGi;
 * defline_option += CShowBlastDefline::eHtml;
 * .......
 * CShowBlastDefline sbd(aln_set, scope);   
 * sbd.SetRid(rid);
 *
 * sbd.SetOption(defline_option);
 * sbd.DisplayBlastDefline(out);
 * @endcode
 */
class NCBI_ALIGN_FORMAT_EXPORT CShowBlastDefline 
{

public:
    /// Constructors
    ///@param seqalign: input seqalign
    ///@param scope: scope to fetch sequences
    ///@param line_length: length of defline desired
    ///@param deflines_to_show: number of seqalign hits to show
    ///
    CShowBlastDefline(const objects::CSeq_align_set& seqalign,                       
                      objects::CScope& scope,
                      size_t line_length = 65,
                      size_t deflines_to_show = kDfltArgNumDescriptions,
                      bool translated_nuc_alignment = false,
                      CRange<TSeqPos>* master_range = NULL);
                      
    
    ~CShowBlastDefline();
    
    ///Display options
    enum DisplayOption {
        eHtml = (1 << 0),               // Html output. Default text.
        eLinkout = (1 << 1),            // Linkout gifs. 
        eShowGi = (1 << 2),             //show gi 
        eCheckbox = (1 << 3),           //checkbox for psiblast
        eShowSumN = (1 << 4),           //Sum_n in blast score structure
        eCheckboxChecked = (1 << 5),    //pre-check the checkbox
        eNoShowHeader = (1 << 6),       //defline annotation at the top
        eNewTargetWindow = (1 << 7),    //open url link in a new window
        eShowNewSeqGif = (1 << 8),      //show new sequence gif image
        eShowPercentIdent = (1 << 9)    //show percent identity column
    };

    ///Data Representing each defline
    struct SDeflineInfo {
        CConstRef<objects::CSeq_id> id;         //best accession type id
        int gi;                        //gi 
        string defline;                //defline
        list<string> linkout_list;     //linkout urls
        int linkout;                   //linkout membership
        string id_url;                 //seqid url		
        string score_url;              //score url (quick jump to alignment)
        bool is_new;                   //is this sequence new (for psiblast)?
        bool was_checked;              //was this sequence checked before?
    };

    //Data representing templates for defline display 
    struct SDeflineTemplates {
        string defLineTmpl;           ///< whole defilne template
        string subHeaderTmpl;         ///< subheader templates for Transcript/Genomic case
        string subHeaderSort;         ///< subheader sort template for Transcript/Genomic case        
        string scoreInfoTmpl;         ///< score info template
        string seqInfoTmpl;           ///< sequence infor template
        string psiFirstNewAnchorTmpl; ///< first new seq anchor template (psi blast)
        string psiGoodGiHiddenTmpl;   ///< good gi hidden field tewmplate (psi blast)
        bool   advancedView;
    };

    ///options per DisplayOption
    ///@param option: input option using bit numbers defined in DisplayOption
    ///
    void SetOption(int option){
        m_Option = option;
    }

    /// Set the linkout implementation
    void SetLinkoutDB(ILinkoutDB* l,
                      const string& mv_build_name) 
    {
        m_LinkoutDB = l; 
        m_MapViewerBuildName = mv_build_name;
    }

    /// PSI-BLAST sequence status (applicable in HTML only)
    enum PsiblastSeqStatus {
        eUnknown = (0 << 0),            ///< Uninitialized
        eGoodSeq = (1 << 0),            ///< above the threshold evalue
        eCheckedSeq = (1 << 1),         ///< user checked
        eRepeatSeq = (1 << 2)           ///< previously already found
    };
    
    /// PSI-BLAST defline groupings
    enum PsiblastStatus {
        eFirstPass = 0,    ///< First pass. Default
        eRepeatPass,       ///< Sequences were found in previous pass
        eNewPass           ///< Sequences are newly found in current pass
    };

    /// Map of sequence IDs to PSI-BLAST sequence status
    typedef map<string, PsiblastSeqStatus> TIdString2SeqStatus;

    ///Set psiblast specific options.
    ///@param seq_status: the hash table containing status for each sequence.
    ///The key contains id only (no type tag)
    ///@param status: status for this round of search
    void SetupPsiblast(TIdString2SeqStatus* seq_status = NULL, 
                       PsiblastStatus status = eFirstPass){
        m_PsiblastStatus = status;
        m_SeqStatus = seq_status;
    }
    
    ///Set path to html images.
    ///@param path: i.e. "mypath/". internal defaul current dir if this
    ///function is not called
    ///
    void SetImagePath(string path) { 
        m_ImagePath = path;
    }
  
    ///Set this for linking to mapviewer
    ///@param number: the query number
    void SetQueryNumber(int number) {
        m_QueryNumber = number;
    }

    ///Display top num seqalign
    ///@param num: number desired
    void SetNumAlignToShow(int num) {
        m_NumToShow = num;
    }

    ///Skip certain seqaligns (only used in Igblast)
    ///@param from: skip from (inclusive)
    ///@param to: skip to (exclusive)
    void SetSkipRange(int from, int to) {
        m_SkipFrom = from;
        m_SkipTo = to;
    }

    ///Set this for constructing structure linkout
    ///@param term: entrez query term
    ///
    void SetEntrezTerm(string term) {
        m_EntrezTerm = term;
    }
    
    ///Set blast request id
    ///@param rid: blast request id
    ///
    void SetRid(string rid) {
        m_Rid = rid;
    }
    
    ///Set this for constructing structure linkout
    ///@param cdd_rid:  cdd request id
    ///
    void SetCddRid(string cdd_rid) {
        m_CddRid = cdd_rid;
    }

    ///Set this for constructing seqid url
    ///@param is_na: nucleotide or protein database?
    ///
    void SetDbType(bool is_na) {
        m_IsDbNa = is_na;
    }
    
    ///Set this for constructing seqid url
    ///@param database: name of the database
    ///
    void SetDbName(string database){
        m_Database = database;
    }
    
    ///Set this for proper construction of seqid url
    ///@param type: refer to blobj->adm->trace->created_by
    ///
    void SetBlastType(string type) {
        m_BlastType = type;
    }

    ///Sets the start index of defline for PSI blast description table
    /// @param int startIndex [in]
    void SetStartIndex(int startIndex) {
        m_StartIndex = startIndex;
    }

    void SetCgiContext (CCgiContext& ctx) {
        m_Ctx = &ctx;
    }

    ///Set this if defline tempaltes are used 
    ///Param deflineTemplates: struct containg defline templates info
    ///
    void SetDeflineTemplates (SDeflineTemplates *deflineTemplates) {
        m_DeflineTemplates = deflineTemplates;
    }
    
    ///Sets CDD precomputed results ID
    /// @param string containing seq id used in contsructing URL to CDART
    void SetPreComputedResID(string preComputedResID) {
        m_PreComputedResID = preComputedResID;
    }
    
    /// Creates a '|' delimited string, corresponding to a list of Seq-ids
    /// @param id List of Seq-ids [in]
    /// @param show_gi Should gi ids be shown or skipped?
    /// @return Corresponding string.
    static string GetSeqIdListString(const list<CRef<objects::CSeq_id> >& id,
                                     bool show_gi);

    /// Converts a Bioseq handle's sequence id type into a list of objects::CSeq_id 
    /// references, substituting an artificial gnl|BL_ORD_ID identifier by a 
    /// corresponding first token of the title. 
    /// @param bh Bioseq handle [in]
    /// @param ids list of Seq-ids from this Bioseq handle. [out]
    static void GetSeqIdList(const objects::CBioseq_Handle& bh,
                             list<CRef<objects::CSeq_id> >& ids);
    
    /// Check the vector of objects::CSeq_id  references (original_seqids),
    /// substituting an artificial gnl|BL_ORD_ID identifier by a 
    /// corresponding first token of the title (bh). 
    /// @param bh Bioseq handle [in]
    /// @param original_seqids vector< CConstRef<objects::CSeq_id> > [in]
    /// @param ids list of possibly modified Seq-ids  [out]
    static void GetSeqIdList(const objects::CBioseq_Handle& bh,
                                vector< CConstRef<objects::CSeq_id> > &original_seqids,
                                list<CRef<objects::CSeq_id> >& ids);

    /// Returns sequence id and a BLAST defline as strings, given a Bioseq 
    /// handle and a list of gis. 
    /// to use. Does not need an object.
    /// @param handle Bioseq handle to retrieve data from [in]
    /// @param use_this_gi List of gis to use in the defline. All will be used 
    ///                    if the list is empty. [in]
    /// @param seqid Seq-id string, combining all Seq-ids from the first defline.
    /// @param defline Defline string, combining all deflines in case of 
    ///                redundant sequences.
    /// @param show_gi Should gi Seq-ids be included? [in]
    /// @param this_gi_first: if not -1, sort the return value if necessary so
    /// that the  first element in the list is the requested gi
    ///
    static void
    GetBioseqHandleDeflineAndId(const objects::CBioseq_Handle& handle,
                                list<int>& use_this_gi, string& seqid, 
                                string& defline, bool show_gi=true,
                                int this_gi_first = -1);
    
    ///Display defline
    ///@param out: stream to output
    ///
    void DisplayBlastDefline(CNcbiOstream & out);
    void DisplayBlastDeflineTable(CNcbiOstream & out);
 
    ///Initialize defline params
    void Init(void);

    ///Display defline
    ///@param out: stream to output
    ///
    void Display(CNcbiOstream & out);    

    ///Indicates if 'Related structures' link should show
    ///@return: true if show
    bool HasStructureLinkout(void){return m_StructureLinkout;}
        
    ///Get defline info for the set of seqIds
    ///@param seqIds: vector of CConstRef<objects::CSeq_id>
    ///@return: vector of SDeflineInfo
    vector <CShowBlastDefline::SDeflineInfo*> 
            GetDeflineInfo(vector< CConstRef<objects::CSeq_id> > &seqIds);

protected:
    /// Internal data with score information for each defline.
    struct SScoreInfo {
        list<int> use_this_gi;         // Limit formatting by these GI's.
        string bit_string;             //bit score
        string raw_score_string;       //raw score
        string evalue_string;          //e value
        int sum_n;                     //sum_n in score block
        string total_bit_string;       //total bit score for this hit
        int match;                     //number of matches for the top hsp with the hit
        int master_covered_length;     //total length covered by alignment
        int align_length;              //length of alignment
        int percent_coverage;          //query coverage 
        int percent_identity;          //percent identity   
        CConstRef<objects::CSeq_id> id;
        int blast_rank;                // "Rank" of defline.
        int hspNum;                    //hsp number
        CRange<TSeqPos> subjRange;     //subject sequence range
        bool flip;					   //indicates opposite strands in the first seq align	
    };

    ///Seqalign 
    CConstRef<objects::CSeq_align_set> m_AlnSetRef;   

    ///Database name
    string m_Database;

    ///Scope to fetch sequence
    CRef<objects::CScope> m_ScopeRef;

    ///Line length
    size_t  m_LineLen;

    ///Number of seqalign hits to show
    size_t m_NumToShow;

    ///Display options
    int m_Option;

    ///List containing score info for all seqalign
    vector<SScoreInfo*> m_ScoreList;      

    ///Blast type
    string m_BlastType;

    ///Linkout order for display
    string m_LinkoutOrder;
    
    ///Gif images path
    string  m_ImagePath;

    ///Internal configure file, i.e. .ncbirc
    auto_ptr<CNcbiIfstream> m_ConfigFile;
    auto_ptr<CNcbiRegistry> m_Reg;

    ///query number
    int m_QueryNumber;

    ///entrez term
    string m_EntrezTerm;
    
    ///blast request id
    string m_Rid;

    ///cdd blast request id
    string m_CddRid;

    bool m_IsDbNa;
    PsiblastStatus m_PsiblastStatus;

    ///hash table to track psiblast status for each sequence
    TIdString2SeqStatus* m_SeqStatus;

    ///used to calculate the alignment length
    bool m_TranslatedNucAlignment;

    ///seq aligns to be skipped
    int m_SkipFrom;
    int m_SkipTo;

    ///Score header size
    size_t m_MaxScoreLen;
    ///Eval header size
    size_t m_MaxEvalueLen;
    size_t m_MaxSumNLen;
    ///Total score header size
    size_t m_MaxTotalScoreLen;
    //Percent identity header size
    size_t m_MaxPercentIdentityLen;    
    //Coverage header size
    size_t m_MaxQueryCoverLen;  
    //Query Lenghth
    int m_QueryLength;
    ///Indicates if 'Related structures' link should show
    bool m_StructureLinkout;

    ///blast sub-sequnce query
    CRange<TSeqPos>* m_MasterRange;

    ///blast defline templates
    SDeflineTemplates *m_DeflineTemplates;

    ///CDD precomputed results ID
    string m_PreComputedResID;
    
    CCgiContext* m_Ctx;

    /// Reference to LinkoutDB implementation. Not owned by this class
    ILinkoutDB* m_LinkoutDB;
    /// mapviewer build name associated with the sequences in the BLAST
    /// database out of which the results are being formatted by this class.
    string m_MapViewerBuildName;

    ///The start index of defline for PSI blast description table
    int    m_StartIndex;

    ///Internal function to return score info
    ///@param aln seq-align we are working with [in]
    ///@param blast_rank ordinal nubmer of defline [in]
    ///@return score info
    SScoreInfo* x_GetScoreInfo(const objects::CSeq_align& aln, int blast_rank);

    ///Internal function to return score info
    ///@param aln seq-align-set we are working with [in]
    ///@param blast_rank ordinal nubmer of defline [in]
    ///@return score info
    SScoreInfo* x_GetScoreInfoForTable(const objects::CSeq_align_set& aln, int blast_rank);

    ///Internal function to return defline info
    ///@param id: seq-id we are working with [in]
    ///@param use_this_gi: list of GI's to limit formatting by [in]
    ///@return defline info
    SDeflineInfo* x_GetDeflineInfo(CConstRef<objects::CSeq_id> id, list<int>& use_this_gi, int blast_rank);
   
    ///Internal function to return defline info
    ///@param aln: seqalign we are working on
    ///@return defline info
    SDeflineInfo* x_GetHitDeflineInfo(const objects::CSeq_align_set& aln);

    /// Checks the first X deflines (currently X is 200) for a structure link.
    /// If one is not found there it is assumed one is not in the results.
    /// If one is found we exit right away.
    /// We do this so we can fetch most of the Bioseqs for large results as we format.
    /// @return true if structure link present, false if not.
    bool x_CheckForStructureLink();

    ///Internal function to fill defline info
    ///@param handle: sequence handle for current seqalign
    ///@param aln_id: seqid from current seqalign
    ///@param use_this_gi: gi from use_this_gi in seqalign
    ///@param sdl: this is where is info is filled to
    void x_FillDeflineAndId(const objects::CBioseq_Handle& handle,
                            const objects::CSeq_id& aln_id,
                            list<int>& use_this_gi,
                            SDeflineInfo* sdl,
                            int blast_rank);
    
    ///Initialize defline params for regular output
    ///
    void x_InitDefline(void);

    ///Initialize defline params for table output
    ///
    void x_InitDeflineTable(void);

    ///Display defline for regular output
    ///
    void x_DisplayDefline(CNcbiOstream & out);

    ///Display defline for table output
    ///
    void x_DisplayDeflineTable(CNcbiOstream & out);
    ///Display defline table contents for table output
    ///
    void x_DisplayDeflineTableBody(CNcbiOstream & out);

    ///Format headers for Transcrit/Genomic seq set
    ///
    string x_FormatSeqSetHeaders(int isGenomic, bool formatHeaderSort);
    ///Format defline
    ///
    string x_FormatDeflineTableLine(SDeflineInfo* sdl,SScoreInfo* iter,bool &first_new);
    ///Format PSI blat related data
    string x_FormatPsi(SDeflineInfo* sdl, bool &first_new);
    ///Display defline for table output using templates
    ///
    void x_DisplayDeflineTableTemplate(CNcbiOstream & out);

    //For internal test
    friend struct ::CShowBlastDeflineTest;    
};

END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___SHOWDEFLINE_HPP */
