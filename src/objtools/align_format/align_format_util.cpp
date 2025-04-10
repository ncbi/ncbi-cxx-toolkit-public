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
 * 12/2004
 * File Description:
 *   blast formatter utilities
 *
 */
#include <ncbi_pch.hpp>

#include <math.h> // For use of ceil

#include <objtools/align_format/align_format_util.hpp>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/metareg.hpp>
#include <html/htmlhelper.hpp>
#include <cgi/cgictx.hpp>
#include <util/tables/raw_scoremat.h>


#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objects/general/User_object.hpp>

#include <objtools/blast/services/blast_services.hpp>   // for CBlastServices
#include <objtools/blast/seqdb_reader/seqdb.hpp>   // for CSeqDB
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>   // for CSeqDBException

#include <objects/seq/seqport_util.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <util/compile_time.hpp>

#include <stdio.h>
#include <sstream>
#include <iomanip>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi);
USING_SCOPE(objects);
BEGIN_SCOPE(align_format)

const char* CAlignFormatUtil::kNoHitsFound = "No hits found";

static bool kTranslation;
static CRef<CScope> kScope;

const char k_PSymbol[ePMatrixSize + 1] = "ARNDCQEGHILKMFPSTWYVBZX";

unique_ptr<CNcbiRegistry> CAlignFormatUtil::m_Reg;
string CAlignFormatUtil::m_Protocol = "";
bool  CAlignFormatUtil::m_geturl_debug_flag = false;

MAKE_CONST_MAP(mapURLTagToAddress, ct::tagStrNocase, ct::tagStrNocase,
{
    { "BL2SEQ_WBLAST_CGI",  "https://www.ncbi.nlm.nih.gov/blast/bl2seq/wblast2.cgi" },                                                  //kBl2SeqWBlastCgi
    { "CBLAST_CGI",  "https://www.ncbi.nlm.nih.gov/Structure/cblast/cblast.cgi" },                                                      //kCBlastCgi     
    { "ENTREZ_QUERY_CGI",  "https://www.ncbi.nlm.nih.gov/entrez/query.fcgi" },                                                          //kEntrezQueryCgi
    { "ENTREZ_SITES_CGI",  "https://www.ncbi.nlm.nih.gov/sites/entrez" },                                                               //kEntrezSitesCgi     
    { "ENTREZ_SUBSEQ_TM",  "https://www.ncbi.nlm.nih.gov/<@db@>/<@gi@>?report=gbwithparts&from=<@from@>&to=<@to@>&RID=<@rid@>" },       //kEntrezSubseqTMUrl
    { "ENTREZ_TM",  "https://www.ncbi.nlm.nih.gov/<@db@>/<@acc@>?report=genbank&log$=<@log@>&blast_rank=<@blast_rank@>&RID=<@rid@>" },  //kEntrezTMUrl
    { "ENTREZ_VIEWER_CGI",  "https://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi" },                                                        //kEntrezViewerCgi    
    { "GENE_INFO",  "https://www.ncbi.nlm.nih.gov/sites/entrez?db=gene&cmd=search&term=%d&RID=%s&log$=geneexplicit%s&blast_rank=%d" },  //kGeneInfoUr        
    { "MAP_SEARCH_CGI",  "https://www.ncbi.nlm.nih.gov/mapview/map_search.cgi" },                                                       //kMapSearchCgi          
    { "TRACE_CGI",  "https://www.ncbi.nlm.nih.gov/Traces/trace.cgi" },                                                                  //kTraceCgi
    { "TREEVIEW_CGI",  "https://www.ncbi.nlm.nih.gov/blast/treeview/blast_tree_view.cgi"},                                              //kGetTreeViewCgi         
    { "WGS",  "https://www.ncbi.nlm.nih.gov/nuccore/<@wgsacc@>" },                                                                      //kWGSUrl 
});



///Get blast score information
///@param scoreList: score container to extract score info from
///@param score: place to extract the raw score to
///@param bits: place to extract the bit score to
///@param evalue: place to extract the e value to
///@param sum_n: place to extract the sum_n to
///@param num_ident: place to extract the num_ident to
///@param use_this_gi: place to extract use_this_gi to
///@return true if found score, false otherwise
///
template<class container> bool
s_GetBlastScore(const container&  scoreList,
                int& score,
                double& bits,
                double& evalue,
                int& sum_n,
                int& num_ident,
                list<TGi>& use_this_gi,
                int& comp_adj_method)
{
    const string k_GiPrefix = "gi:";
    bool hasScore = false;
    ITERATE (typename container, iter, scoreList) {
        const CObject_id& id=(*iter)->GetId();
        if (id.IsStr()) {
            if (id.GetStr()=="score"){
                score = (*iter)->GetValue().GetInt();
            } else if (id.GetStr()=="bit_score"){
                bits = (*iter)->GetValue().GetReal();
            } else if (id.GetStr()=="e_value" || id.GetStr()=="sum_e") {
                evalue = (*iter)->GetValue().GetReal();
                hasScore = true;
            } else if (id.GetStr()=="use_this_gi"){
            	Uint4 gi_v = (Uint4)((*iter)->GetValue().GetInt());
                use_this_gi.push_back(GI_FROM(Uint4, gi_v));
            } else if (id.GetStr()=="sum_n"){
                sum_n = (*iter)->GetValue().GetInt();
            } else if (id.GetStr()=="num_ident"){
                num_ident = (*iter)->GetValue().GetInt();
            } else if (id.GetStr()=="comp_adjustment_method") {
                comp_adj_method = (*iter)->GetValue().GetInt();
            }
            else if(NStr::StartsWith(id.GetStr(),k_GiPrefix)) { //will be used when switch to 64bit GIs
                string strGi = NStr::Replace(id.GetStr(),k_GiPrefix,"");
                TGi gi = NStr::StringToNumeric<TGi>(strGi);
                use_this_gi.push_back(gi);
            }
        }
    }

    return hasScore;
}


///Wrap a string to specified length.  If break happens to be in
/// a word, it will extend the line length until the end of the word
///@param str: input string
///@param line_len: length of each line desired
///@param out: stream to ouput
///
void CAlignFormatUtil::x_WrapOutputLine(string str, size_t line_len,
                                        CNcbiOstream& out, bool html)
{
    list<string> string_l;
    NStr::TWrapFlags flags = NStr::fWrap_FlatFile;
    if (html) {
        flags = NStr::fWrap_HTMLPre;
        str = CHTMLHelper::HTMLEncode(str);
    }
    NStr::Wrap(str, line_len, string_l, flags);
    list<string>::iterator iter = string_l.begin();
    while(iter != string_l.end())
    {
       out << *iter;
       out << "\n";
       iter++;
    }
}

void CAlignFormatUtil::BlastPrintError(list<SBlastError>&
                                       error_return,
                                       bool error_post, CNcbiOstream& out)
{

    string errsevmsg[] = { "UNKNOWN","INFO","WARNING","ERROR",
                            "FATAL"};

    NON_CONST_ITERATE(list<SBlastError>, iter, error_return) {

        if(iter->level > 5){
            iter->level = eDiag_Info;
        }

        if(iter->level == 4){
            iter->level = eDiag_Fatal;
        } else{
            iter->level = iter->level;
        }

        if (error_post){
            ERR_POST_EX(iter->level, 0, iter->message);
        }
        out << errsevmsg[iter->level] << ": " << iter->message << "\n";

    }

}

void  CAlignFormatUtil::PrintTildeSepLines(string str, size_t line_len,
                                           CNcbiOstream& out) {

    vector<string> split_line;
    NStr::Split(str, "~", split_line);
    ITERATE(vector<string>, iter, split_line) {
        x_WrapOutputLine(*iter,  line_len, out);
    }
}
#ifdef DO_UNUSED
/// Initialize database statistics with data from BLAST servers
/// @param dbname name of a single BLAST database [in]
/// @param info structure to fill [in|out]
/// @return true if successfully filled, false otherwise (and a warning is
/// printed out)
static bool s_FillDbInfoRemotely(const string& dbname,
                                 CAlignFormatUtil::SDbInfo& info)
{
    static CBlastServices rmt_blast_services;
    CRef<CBlast4_database> blastdb(new CBlast4_database);
    blastdb->SetName(dbname);
    blastdb->SetType() = info.is_protein
        ? eBlast4_residue_type_protein : eBlast4_residue_type_nucleotide;
    CRef<CBlast4_database_info> dbinfo =
        rmt_blast_services.GetDatabaseInfo(blastdb);

    info.name = dbname;
    if ( !dbinfo ) {
        return false;
    }
    info.definition = dbinfo->GetDescription();
    if (info.definition.empty())
        info.definition = info.name;
    CTimeFormat tf("b d, Y H:m P", CTimeFormat::fFormat_Simple);
    info.date = CTime(dbinfo->GetLast_updated()).AsString(tf);
    info.total_length = dbinfo->GetTotal_length();
    info.number_seqs = static_cast<int>(dbinfo->GetNum_sequences());
    return true;
}
#endif
/// Initialize database statistics with data obtained from local BLAST
/// databases
/// @param dbname name of a single BLAST database [in]
/// @param info structure to fill [in|out]
/// @param dbfilt_algorithm filtering algorithm ID used for this search
/// [in]
/// @return true if successfully filled, false otherwise (and a warning is
/// printed out)
static bool
s_FillDbInfoLocally(const string& dbname,
                    CAlignFormatUtil::SDbInfo& info,
                    int dbfilt_algorithm)
{
    CRef<CSeqDB> seqdb(new CSeqDB(dbname, info.is_protein
                          ? CSeqDB::eProtein : CSeqDB::eNucleotide));
    if ( !seqdb ) {
        return false;
    }
    info.name = seqdb->GetDBNameList();
    info.definition = seqdb->GetTitle();
    if (info.definition.empty())
        info.definition = info.name;
    info.date = seqdb->GetDate();
    info.total_length = seqdb->GetTotalLength();
    info.number_seqs = seqdb->GetNumSeqs();

    // Process the filtering algorithm IDs
    info.filt_algorithm_name.clear();
    info.filt_algorithm_options.clear();
    if (dbfilt_algorithm == -1) {
        return true;
    }

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    string filtering_algorithm;
    seqdb->GetMaskAlgorithmDetails(dbfilt_algorithm,
                                   filtering_algorithm,
                                   info.filt_algorithm_name,
                                   info.filt_algorithm_options);
#endif
    return true;
}

void
CAlignFormatUtil::FillScanModeBlastDbInfo(vector<CAlignFormatUtil::SDbInfo>& retval,
                           bool is_protein, int numSeqs, Int8 numLetters, string& tag)
{
	retval.clear();
	CAlignFormatUtil::SDbInfo info;
	info.is_protein = is_protein;
	if (tag == "")
		info.definition = string("User specified sequence set.");
	else
	{
		info.definition = string("User specified sequence set ") +
			string("(Input: ") + tag + string(").");
	}
	info.number_seqs = numSeqs;
	info.total_length = numLetters;
	retval.push_back(info);
}

void
CAlignFormatUtil::GetBlastDbInfo(vector<CAlignFormatUtil::SDbInfo>& retval,
                           const string& blastdb_names, bool is_protein,
                           int dbfilt_algorithm /* = -1 */,
                           bool is_remote /* = false */)
{
    retval.clear();
    if( is_remote ){
	bool found_all = false;
        static CBlastServices rmt_blast_services;
	vector<string> missing_names;
	vector< CRef<objects::CBlast4_database_info> > all_db_info =
	    rmt_blast_services.GetDatabaseInfo(blastdb_names,is_protein,&found_all,&missing_names);
	if( !missing_names.empty() ){
	    string msg("'");
	    for(size_t ndx=0 ; ndx < missing_names.size(); ndx++){
		msg += missing_names[ndx];
	    }
	    msg += string("' not found on NCBI servers.\n");
	    NCBI_THROW(CSeqDBException, eFileErr, msg);
	}
	for(size_t ndx=0 ; ndx < all_db_info.size(); ndx++){
	    CAlignFormatUtil::SDbInfo info;
	    objects::CBlast4_database_info &dbinfo = *all_db_info[ndx];
	    info.name = dbinfo.GetDatabase().GetName();
	    info.definition = dbinfo.GetDescription();
	    if (info.definition.empty())
		info.definition = info.name;
	    CTimeFormat tf("b d, Y H:m P", CTimeFormat::fFormat_Simple);
	    info.date = CTime(dbinfo.GetLast_updated()).AsString(tf);
	    info.total_length = dbinfo.GetTotal_length();
	    info.number_seqs = static_cast<int>(dbinfo.GetNum_sequences());
            if (info.total_length < 0) {
		const string kDbName = NStr::TruncateSpaces(info.name);
                if( ! s_FillDbInfoLocally(kDbName, info, dbfilt_algorithm) ){
		    string msg("'");
		    msg += kDbName;
		    msg += string("' has bad total length on NCBI servers.\n");
		    NCBI_THROW(CSeqDBException, eFileErr, msg);
		}
            }
            retval.push_back(info);
	}
	return;
    }
    else{
    vector<CTempString> dbs;
    SeqDB_SplitQuoted(blastdb_names, dbs, true);
    retval.reserve(dbs.size());

    ITERATE(vector<CTempString>, i, dbs) {
        CAlignFormatUtil::SDbInfo info;
        info.is_protein = is_protein;
        bool success = false;
	// Unsafe OK as kDbName only used in this loop.
        const string kDbName = NStr::TruncateSpaces_Unsafe(*i);
        if (kDbName.empty())
            continue;

        success = s_FillDbInfoLocally(kDbName, info, dbfilt_algorithm);

        if (success) {
            retval.push_back(info);
        } else {
            string msg("'");
            msg += kDbName;
            if (is_remote)
                msg += string("' not found on NCBI servers.\n");
            else
                msg += string("' not found.\n");
            NCBI_THROW(CSeqDBException, eFileErr, msg);
        }
    }
    }
}

void CAlignFormatUtil::PrintDbReport(const vector<SDbInfo>& dbinfo_list,
                                     size_t line_length,
                                     CNcbiOstream& out,
                                     bool top)
{
    if (top) {
        const CAlignFormatUtil::SDbInfo* dbinfo = &(dbinfo_list.front());
        out << "Database: ";

        string db_titles = dbinfo->definition;
        Int8 tot_num_seqs = static_cast<Int8>(dbinfo->number_seqs);
        Int8 tot_length = dbinfo->total_length;

        for (size_t i = 1; i < dbinfo_list.size(); i++) {
            db_titles += "; " + dbinfo_list[i].definition;
            tot_num_seqs += static_cast<Int8>(dbinfo_list[i].number_seqs);
            tot_length += dbinfo_list[i].total_length;
        }

        x_WrapOutputLine(db_titles, line_length, out);
        if ( !dbinfo->filt_algorithm_name.empty() ) {
            out << "Masked using: '" << dbinfo->filt_algorithm_name << "'";
            if ( !dbinfo->filt_algorithm_options.empty() ) {
                out << ", options: '" << dbinfo->filt_algorithm_options << "'";
            }
            out << endl;
        }
        CAlignFormatUtil::AddSpace(out, 11);
        out << NStr::Int8ToString(tot_num_seqs, NStr::fWithCommas) <<
            " sequences; " <<
            NStr::Int8ToString(tot_length, NStr::fWithCommas) <<
            " total letters\n\n";
        return;
    }

    ITERATE(vector<SDbInfo>, dbinfo, dbinfo_list) {
        if (dbinfo->subset == false) {
            out << "  Database: ";
            x_WrapOutputLine(dbinfo->definition, line_length, out);

            if ( !dbinfo->filt_algorithm_name.empty() ) {
                out << "  Masked using: '" << dbinfo->filt_algorithm_name << "'";
                if ( !dbinfo->filt_algorithm_options.empty() ) {
                    out << ", options: '" << dbinfo->filt_algorithm_options << "'";
                }
                out << endl;
            }

            out << "    Posted date:  ";
            out << dbinfo->date << "\n";

            out << "  Number of letters in database: ";
            out << NStr::Int8ToString(dbinfo->total_length,
                                      NStr::fWithCommas) << "\n";
            out << "  Number of sequences in database:  ";
            out << NStr::IntToString(dbinfo->number_seqs,
                                     NStr::fWithCommas) << "\n";

        } else {
            out << "  Subset of the database(s) listed below" << "\n";
            out << "  Number of letters searched: ";
            out << NStr::Int8ToString(dbinfo->total_length,
                                      NStr::fWithCommas) << "\n";
            out << "  Number of sequences searched:  ";
            out << NStr::IntToString(dbinfo->number_seqs,
                                     NStr::fWithCommas) << "\n";
        }
        out << "\n";
    }

}

void CAlignFormatUtil::PrintKAParameters(double lambda, double k, double h,
                                         size_t line_len,
                                         CNcbiOstream& out, bool gapped,
                                         const Blast_GumbelBlk *gbp)
{

    char buffer[256];
    if (gapped) {
        out << "Gapped" << "\n";
    }
    out << "Lambda      K        H";
    if (gbp) {
        if (gapped) {
            out << "        a         alpha    sigma";
        } else {
            out << "        a         alpha";
        }
    }
    out << "\n";
    sprintf(buffer, "%#8.3g ", lambda);
    out << buffer;
    sprintf(buffer, "%#8.3g ", k);
    out << buffer;
    sprintf(buffer, "%#8.3g ", h);
    out << buffer;
    if (gbp) {
        if (gapped) {
            sprintf(buffer, "%#8.3g ", gbp->a);
            out << buffer;
            sprintf(buffer, "%#8.3g ", gbp->Alpha);
            out << buffer;
            sprintf(buffer, "%#8.3g ", gbp->Sigma);
            out << buffer;
        } else {
            sprintf(buffer, "%#8.3g ", gbp->a_un);
            out << buffer;
            sprintf(buffer, "%#8.3g ", gbp->Alpha_un);
            out << buffer;
        }
        //x_WrapOutputLine(buffer, line_len, out);
    }
    out << "\n";
}

string
CAlignFormatUtil::GetSeqIdString(const CBioseq& cbs, bool believe_local_id)
{
    const CBioseq::TId& ids = cbs.GetId();
    return CAlignFormatUtil::GetSeqIdString(ids, believe_local_id);
}

string
CAlignFormatUtil::GetSeqIdString(const list<CRef<CSeq_id> > & ids, bool believe_local_id)
{
    string all_id_str = NcbiEmptyString;
    CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);

    if (wid && (wid->Which()!= CSeq_id::e_Local || believe_local_id)){
        TGi gi = FindGi(ids);

        bool use_long_seqids = false;
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app) {
            const CNcbiRegistry& registry = app->GetConfig();
            use_long_seqids = (registry.Get("BLAST", "LONG_SEQID") == "1");
        }
        if (!use_long_seqids) {

            all_id_str = GetBareId(*wid);
        }
        else if (strncmp(wid->AsFastaString().c_str(), "lcl|", 4) == 0) {
            if(gi == ZERO_GI){
                all_id_str =  wid->AsFastaString().substr(4);
            } else {
                all_id_str = "gi|" + NStr::NumericToString(gi) +
                    "|" + wid->AsFastaString().substr(4);
            }
        } else {
            if(gi == ZERO_GI){
                all_id_str = wid->AsFastaString();
            } else {
                all_id_str = "gi|" + NStr::NumericToString(gi) + "|" +
                    wid->AsFastaString();
            }
        }
    }

    return all_id_str;
}

string
CAlignFormatUtil::GetSeqDescrString(const CBioseq& cbs)
{
    string all_descr_str = NcbiEmptyString;

    if (cbs.IsSetDescr()) {
        const CBioseq::TDescr& descr = cbs.GetDescr();
        const CBioseq::TDescr::Tdata& data = descr.Get();
        ITERATE(CBioseq::TDescr::Tdata, iter, data) {
            if((*iter)->IsTitle()) {
                all_descr_str += (*iter)->GetTitle();
            }
        }
    }
    return all_descr_str;
}

void CAlignFormatUtil::AcknowledgeBlastQuery(const CBioseq& cbs,
                                             size_t line_len,
                                             CNcbiOstream& out,
                                             bool believe_query,
                                             bool html,
                                             bool tabular /* = false */,
                                             const string& rid /* = kEmptyStr*/)
{
    const string label("Query");
    CAlignFormatUtil::x_AcknowledgeBlastSequence(cbs, line_len, out,
                                                 believe_query, html,
                                                 label, tabular, rid);
}

void
CAlignFormatUtil::AcknowledgeBlastSubject(const CBioseq& cbs,
                                          size_t line_len,
                                          CNcbiOstream& out,
                                          bool believe_query,
                                          bool html,
                                          bool tabular /* = false */)
{
    const string label("Subject");
    CAlignFormatUtil::x_AcknowledgeBlastSequence(cbs, line_len, out,
                                                 believe_query, html,
                                                 label, tabular, kEmptyStr);
}

void
CAlignFormatUtil::x_AcknowledgeBlastSequence(const CBioseq& cbs,
                                             size_t line_len,
                                             CNcbiOstream& out,
                                             bool believe_query,
                                             bool html,
                                             const string& label,
                                             bool tabular /* = false */,
                                             const string& rid /* = kEmptyStr*/)
{

    if (html) {
        out << "<b>" << label << "=</b> ";
    } else if (tabular) {
        out << "# " << label << ": ";
    } else {
        out << label << "= ";
    }

    string all_id_str = GetSeqIdString(cbs, believe_query);
    all_id_str += " ";
    all_id_str = NStr::TruncateSpaces(all_id_str + GetSeqDescrString(cbs));

    // For tabular output, there is no limit on the line length.
    // There is also no extra line with the sequence length.
    if (tabular) {
        out << all_id_str;
    } else {
        x_WrapOutputLine(all_id_str, line_len, out, html);
        if(cbs.IsSetInst() && cbs.GetInst().CanGetLength()){
            out << "\nLength=";
            out << cbs.GetInst().GetLength() <<"\n";
        }
    }

    if (rid != kEmptyStr) {
        if (tabular) {
            out << "\n" << "# RID: " << rid;
        } else {
            out << "\n" << "RID: " << rid << "\n";
        }
    }
}

void CAlignFormatUtil::PrintPhiInfo(int num_patterns,
                                    const string& pattern,
                                    double prob,
                                    vector<int>& offsets,
                                    CNcbiOstream& out)
{
    out << num_patterns << " occurrence(s) of pattern: " << "\n"
             << pattern << " at position(s) ";

    bool first = true;
    for (vector<int>::iterator it = offsets.begin();
      it != offsets.end(); it++)
    {
           if (!first)
             out << ", ";

           out << 1 + *it ;

           first = false;
    }
    out << " of query sequence" << "\n";
    out << "pattern probability=" << prob << "\n";

}

void CAlignFormatUtil::GetAlnScores(const CSeq_align& aln,
                                    int& score,
                                    double& bits,
                                    double& evalue,
                                    int& sum_n,
                                    int& num_ident,
                                    list<TGi>& use_this_gi)
{
    int comp_adj_method = 0; // dummy variable

    CAlignFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n,
                                 num_ident, use_this_gi, comp_adj_method);
}

void CAlignFormatUtil::GetAlnScores(const CSeq_align& aln,
                                    int& score,
                                    double& bits,
                                    double& evalue,
                                    int& sum_n,
                                    int& num_ident,
                                    list<string>& use_this_seq)
{
    int comp_adj_method = 0; // dummy variable

    CAlignFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n,
                                 num_ident, use_this_seq, comp_adj_method);
}


void CAlignFormatUtil::GetAlnScores(const CSeq_align& aln,
                                    int& score,
                                    double& bits,
                                    double& evalue,
                                    int& sum_n,
                                    int& num_ident,
                                    list<TGi>& use_this_gi,
                                    int& comp_adj_method)
{
    bool hasScore = false;
    score = -1;
    bits = -1;
    evalue = -1;
    sum_n = -1;
    num_ident = -1;
    comp_adj_method = 0;

    //look for scores at seqalign level first
    hasScore = s_GetBlastScore(aln.GetScore(), score, bits, evalue,
                               sum_n, num_ident, use_this_gi, comp_adj_method);

    //look at the seg level
    if(!hasScore){
        const CSeq_align::TSegs& seg = aln.GetSegs();
        if(seg.Which() == CSeq_align::C_Segs::e_Std){
            s_GetBlastScore(seg.GetStd().front()->GetScores(),
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        } else if (seg.Which() == CSeq_align::C_Segs::e_Dendiag){
            s_GetBlastScore(seg.GetDendiag().front()->GetScores(),
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        }  else if (seg.Which() == CSeq_align::C_Segs::e_Denseg){
            s_GetBlastScore(seg.GetDenseg().GetScores(),
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        }
    }
    if(use_this_gi.size() == 0) {
        GetUseThisSequence(aln,use_this_gi);
    }
}

//converts gi list to the list of gi:XXXXXXXX strings
static list<string> s_NumGiToStringGiList(list<TGi> use_this_gi)//for backward compatability
{
    const string k_GiPrefix = "gi:";
    list<string> use_this_seq;
    ITERATE(list<TGi>, iter_gi, use_this_gi){
        string strSeq = k_GiPrefix + NStr::NumericToString(*iter_gi);
        use_this_seq.push_back(strSeq);
    }
    return use_this_seq;
}

void CAlignFormatUtil::GetAlnScores(const CSeq_align& aln,
                                    int& score,
                                    double& bits,
                                    double& evalue,
                                    int& sum_n,
                                    int& num_ident,
                                    list<string>& use_this_seq,
                                    int& comp_adj_method)
{
    bool hasScore = false;
    score = -1;
    bits = -1;
    evalue = -1;
    sum_n = -1;
    num_ident = -1;
    comp_adj_method = 0;

    list<TGi> use_this_gi;
    //look for scores at seqalign level first
    hasScore = s_GetBlastScore(aln.GetScore(), score, bits, evalue,
                               sum_n, num_ident, use_this_gi, comp_adj_method);

    //look at the seg level
    if(!hasScore){
        const CSeq_align::TSegs& seg = aln.GetSegs();
        if(seg.Which() == CSeq_align::C_Segs::e_Std){
            s_GetBlastScore(seg.GetStd().front()->GetScores(),
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        } else if (seg.Which() == CSeq_align::C_Segs::e_Dendiag){
            s_GetBlastScore(seg.GetDendiag().front()->GetScores(),
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        }  else if (seg.Which() == CSeq_align::C_Segs::e_Denseg){
            s_GetBlastScore(seg.GetDenseg().GetScores(),
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        }
    }
    if(use_this_gi.size() == 0) {
        GetUseThisSequence(aln,use_this_seq);
    }
    else {
        use_this_seq = s_NumGiToStringGiList(use_this_gi);//for backward compatability
    }
}

string CAlignFormatUtil::GetGnlID(const CDbtag& dtg)
{
   string retval = NcbiEmptyString;

   if(dtg.GetTag().IsId())
     retval = NStr::IntToString(dtg.GetTag().GetId());
   else
     retval = dtg.GetTag().GetStr();

   return retval;
}

string CAlignFormatUtil::GetLabel(CConstRef<CSeq_id> id,bool with_version)
{
    string retval = "";
    if (id->Which() == CSeq_id::e_General){
        const CDbtag& dtg = id->GetGeneral();
        retval = CAlignFormatUtil::GetGnlID(dtg);
    }
    if (retval == "")
      retval = id->GetSeqIdString(with_version);

    return retval;
}

void CAlignFormatUtil::AddSpace(CNcbiOstream& out, size_t number)

{
    for(auto i=0; i<number; i++){
        out<<" ";
    }
}

void CAlignFormatUtil::GetScoreString(double evalue,
                                      double bit_score,
                                      double total_bit_score,
                                      int raw_score,
                                      string& evalue_str,
                                      string& bit_score_str,
                                      string& total_bit_score_str,
                                      string& raw_score_str)
{
    char evalue_buf[100], bit_score_buf[100], total_bit_score_buf[100];

    /* Facilitates comparing formatted output using diff */
    static string kBitScoreFormat("%4.1lf");
#ifdef CTOOLKIT_COMPATIBLE
    static bool ctoolkit_compatible = false;
    static bool value_set = false;
    if ( !value_set ) {
        if (getenv("CTOOLKIT_COMPATIBLE")) {
            kBitScoreFormat.assign("%4.0lf");
            ctoolkit_compatible = true;
        }
        value_set = true;
    }
#endif /* CTOOLKIT_COMPATIBLE */

    if (evalue < 1.0e-180) {
        snprintf(evalue_buf, sizeof(evalue_buf), "0.0");
    } else if (evalue < 1.0e-99) {
        snprintf(evalue_buf, sizeof(evalue_buf), "%2.0le", evalue);
#ifdef CTOOLKIT_COMPATIBLE
        if (ctoolkit_compatible) {
            strncpy(evalue_buf, evalue_buf+1, sizeof(evalue_buf-1));
        }
#endif /* CTOOLKIT_COMPATIBLE */
    } else if (evalue < 0.0009) {
        snprintf(evalue_buf, sizeof(evalue_buf), "%3.0le", evalue);
    } else if (evalue < 0.1) {
        snprintf(evalue_buf, sizeof(evalue_buf), "%4.3lf", evalue);
    } else if (evalue < 1.0) {
        snprintf(evalue_buf, sizeof(evalue_buf), "%3.2lf", evalue);
    } else if (evalue < 10.0) {
        snprintf(evalue_buf, sizeof(evalue_buf), "%2.1lf", evalue);
    } else {
        snprintf(evalue_buf, sizeof(evalue_buf), "%2.0lf", evalue);
    }

    if (bit_score > 99999){
        snprintf(bit_score_buf, sizeof(bit_score_buf), "%5.3le", bit_score);
    } else if (bit_score > 99.9){
        snprintf(bit_score_buf, sizeof(bit_score_buf), "%3.0ld",
            (long)bit_score);
    } else {
        snprintf(bit_score_buf, sizeof(bit_score_buf), kBitScoreFormat.c_str(),
            bit_score);
    }
    if (total_bit_score > 99999){
        snprintf(total_bit_score_buf, sizeof(total_bit_score_buf), "%5.3le",
            total_bit_score);
    } else if (total_bit_score > 99.9){
        snprintf(total_bit_score_buf, sizeof(total_bit_score_buf), "%3.0ld",
            (long)total_bit_score);
    } else {
        snprintf(total_bit_score_buf, sizeof(total_bit_score_buf), "%2.1lf",
            total_bit_score);
    }
    evalue_str = evalue_buf;
    bit_score_str = bit_score_buf;
    total_bit_score_str = total_bit_score_buf;
    if (raw_score <= 0)
      raw_score = -1;
    NStr::IntToString(raw_score_str, raw_score);
}


void CAlignFormatUtil::PruneSeqalign(const CSeq_align_set& source_aln,
                                     CSeq_align_set& new_aln,
                                     unsigned int number)
{
    CConstRef<CSeq_id> previous_id, subid;
    bool is_first_aln = true;
    unsigned int num_align = 0;
    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){

        if ((*iter)->GetSegs().IsDisc()) {
            ++num_align;
        } else {
            subid = &((*iter)->GetSeq_id(1));
            if(is_first_aln || (!is_first_aln && !subid->Match(*previous_id))){
                ++num_align;
            }

            if(num_align > number) {
                 break;
            }

            is_first_aln = false;
            previous_id = subid;
        }
        new_aln.Set().push_back(*iter);
    }
}


unsigned int CAlignFormatUtil::GetSubjectsNumber(const CSeq_align_set& source_aln,
                                             unsigned int number)
{
    CConstRef<CSeq_id> previous_id, subid;
    bool is_first_aln = true;
    unsigned int num_align = 0;
    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){

        if ((*iter)->GetSegs().IsDisc()) {
            ++num_align;
        } else {
            subid = &((*iter)->GetSeq_id(1));
            if(is_first_aln || (!is_first_aln && !subid->Match(*previous_id))){
                ++num_align;
            }

            if(num_align >= number) {
                 break;
            }

            is_first_aln = false;
            previous_id = subid;
        }
    }
    return num_align;
}


void CAlignFormatUtil::PruneSeqalignAll(const CSeq_align_set& source_aln,
                                     CSeq_align_set& new_aln,
                                     unsigned int number)
{
    CConstRef<CSeq_id> previous_id, subid;
    bool is_first_aln = true;
    unsigned int num_align = 0;
    bool finishCurrent = false;
    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){
        if ((*iter)->GetSegs().IsDisc()) {
            ++num_align;
        } else {
            subid = &((*iter)->GetSeq_id(1));
            if(is_first_aln || (!is_first_aln && !subid->Match(*previous_id))){
                finishCurrent = (num_align + 1 == number) ? true : false;
                ++num_align;
            }
            is_first_aln = false;
            previous_id = subid;
        }
        if(num_align > number && !finishCurrent) {
            break;
        }
        new_aln.Set().push_back(*iter);
    }
}


void
CAlignFormatUtil::GetAlignLengths(CAlnVec& salv, int& align_length,
                                  int& num_gaps, int& num_gap_opens)
{
    num_gaps = num_gap_opens = align_length = 0;

    for (int row = 0; row < salv.GetNumRows(); row++) {
        CRef<CAlnMap::CAlnChunkVec> chunk_vec
            = salv.GetAlnChunks(row, salv.GetSeqAlnRange(0));
        for (int i=0; i<chunk_vec->size(); i++) {
            CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
            int chunk_length = chunk->GetAlnRange().GetLength();
            // Gaps are counted on all rows: gap can only be in one of the rows
            // for any given segment.
            if (chunk->IsGap()) {
                ++num_gap_opens;
                num_gaps += chunk_length;
            }
            // To calculate alignment length, only one row is needed.
            if (row == 0)
                align_length += chunk_length;
        }
    }
}

void
CAlignFormatUtil::ExtractSeqalignSetFromDiscSegs(CSeq_align_set& target,
                                                 const CSeq_align_set& source)
{
    if (source.IsSet() && source.CanGet()) {

        for(CSeq_align_set::Tdata::const_iterator iter = source.Get().begin();
            iter != source.Get().end(); iter++) {
            if((*iter)->IsSetSegs()){
                const CSeq_align::TSegs& seg = (*iter)->GetSegs();
                if(seg.IsDisc()){
                    const CSeq_align_set& set = seg.GetDisc();
                    for(CSeq_align_set::Tdata::const_iterator iter2 =
                            set.Get().begin(); iter2 != set.Get().end();
                        iter2 ++) {
                        target.Set().push_back(*iter2);
                    }
                } else {
                    target.Set().push_back(*iter);
                }
            }
        }
    }
}

CRef<CSeq_align>
CAlignFormatUtil::CreateDensegFromDendiag(const CSeq_align& aln)
{
    CRef<CSeq_align> sa(new CSeq_align);
    if ( !aln.GetSegs().IsDendiag()) {
        NCBI_THROW(CException, eUnknown, "Input Seq-align should be Dendiag!");
    }

    if(aln.IsSetType()){
        sa->SetType(aln.GetType());
    }
    if(aln.IsSetDim()){
        sa->SetDim(aln.GetDim());
    }
    if(aln.IsSetScore()){
        sa->SetScore() = aln.GetScore();
    }
    if(aln.IsSetBounds()){
        sa->SetBounds() = aln.GetBounds();
    }

    CDense_seg& ds = sa->SetSegs().SetDenseg();

    int counter = 0;
    ds.SetNumseg() = 0;
    ITERATE (CSeq_align::C_Segs::TDendiag, iter, aln.GetSegs().GetDendiag()){

        if(counter == 0){//assume all dendiag segments have same dim and ids
            if((*iter)->IsSetDim()){
                ds.SetDim((*iter)->GetDim());
            }
            if((*iter)->IsSetIds()){
                ds.SetIds() = (*iter)->GetIds();
            }
        }
        ds.SetNumseg() ++;
        if((*iter)->IsSetStarts()){
            ITERATE(CDense_diag::TStarts, iterStarts, (*iter)->GetStarts()){
                ds.SetStarts().push_back(*iterStarts);
            }
        }
        if((*iter)->IsSetLen()){
            ds.SetLens().push_back((*iter)->GetLen());
        }
        if((*iter)->IsSetStrands()){
            ITERATE(CDense_diag::TStrands, iterStrands, (*iter)->GetStrands()){
                ds.SetStrands().push_back(*iterStrands);
            }
        }
        if((*iter)->IsSetScores()){
            ITERATE(CDense_diag::TScores, iterScores, (*iter)->GetScores()){
                ds.SetScores().push_back(*iterScores); //this might not have
                                                       //right meaning
            }
        }
        counter ++;
    }

    return sa;
}

TTaxId CAlignFormatUtil::GetTaxidForSeqid(const CSeq_id& id, CScope& scope)
{
    TTaxId taxid = ZERO_TAX_ID;
    try{
        const CBioseq_Handle& handle = scope.GetBioseqHandle(id);
        const CRef<CBlast_def_line_set> bdlRef =
            CSeqDB::ExtractBlastDefline(handle);
        const list< CRef< CBlast_def_line > > &bdl = (bdlRef.Empty()) ? list< CRef< CBlast_def_line > >() : bdlRef->Get();
        ITERATE(list<CRef<CBlast_def_line> >, iter_bdl, bdl) {
            CConstRef<CSeq_id> bdl_id =
                GetSeq_idByType((*iter_bdl)->GetSeqid(), id.Which());
            if(bdl_id && bdl_id->Match(id) &&
               (*iter_bdl)->IsSetTaxid() && (*iter_bdl)->CanGetTaxid()){
                taxid = (*iter_bdl)->GetTaxid();
                break;
            }
        }
    } catch (CException&) {

    }
    return taxid;
}

int CAlignFormatUtil::GetFrame (int start, ENa_strand strand,
                                const CBioseq_Handle& handle)
{
    int frame = 0;
    if (strand == eNa_strand_plus) {
        frame = (start % 3) + 1;
    } else if (strand == eNa_strand_minus) {
        frame = -(((int)handle.GetBioseqLength() - start - 1)
                  % 3 + 1);

    }
    return frame;
}


void CAlignFormatUtil::
SortHitByPercentIdentityDescending(list< CRef<CSeq_align_set> >&
                                   seqalign_hit_list,
                                   bool do_translation
                                   )
{

    kTranslation = do_translation;
    seqalign_hit_list.sort(SortHitByPercentIdentityDescendingEx);
}


bool CAlignFormatUtil::
SortHspByPercentIdentityDescending(const CRef<CSeq_align>& info1,
                                   const CRef<CSeq_align>& info2)
{

    int score1, sum_n1, num_ident1;
    double bits1, evalue1;
    list<TGi> use_this_gi1;

    int score2, sum_n2, num_ident2;
    double bits2, evalue2;
    list<TGi> use_this_gi2;


    GetAlnScores(*info1, score1,  bits1, evalue1, sum_n1, num_ident1, use_this_gi1);
    GetAlnScores(*info2, score2,  bits2, evalue2, sum_n2, num_ident2, use_this_gi2);

    int length1 = GetAlignmentLength(*info1, kTranslation);
    int length2 = GetAlignmentLength(*info2, kTranslation);
    bool retval = false;


    if(length1 > 0 && length2 > 0 && num_ident1 > 0 &&num_ident2 > 0 ) {
        if (((double)num_ident1)/length1 == ((double)num_ident2)/length2) {

            retval = evalue1 < evalue2;

        } else {
            retval = ((double)num_ident1)/length1 >= ((double)num_ident2)/length2;

        }
    } else {
        retval = evalue1 < evalue2;
    }
    return retval;
}

bool CAlignFormatUtil::
SortHitByScoreDescending(const CRef<CSeq_align_set>& info1,
                         const CRef<CSeq_align_set>& info2)
{
    CRef<CSeq_align_set> i1(info1), i2(info2);

    i1->Set().sort(SortHspByScoreDescending);
    i2->Set().sort(SortHspByScoreDescending);


    int score1, sum_n1, num_ident1;
    double bits1, evalue1;
    list<TGi> use_this_gi1;

    int score2, sum_n2, num_ident2;
    double bits2, evalue2;
    list<TGi> use_this_gi2;

    GetAlnScores(*(info1->Get().front()), score1,  bits1, evalue1, sum_n1, num_ident1, use_this_gi1);
    GetAlnScores(*(info2->Get().front()), score2,  bits2, evalue2, sum_n2, num_ident2, use_this_gi2);
    return bits1 > bits2;
}

bool CAlignFormatUtil::
SortHitByMasterCoverageDescending(CRef<CSeq_align_set> const& info1,
                                  CRef<CSeq_align_set> const& info2)
{
    int cov1 = GetMasterCoverage(*info1);
    int cov2 = GetMasterCoverage(*info2);
    bool retval = false;

    if (cov1 > cov2) {
        retval = cov1 > cov2;
    } else if (cov1 == cov2) {
        int score1, sum_n1, num_ident1;
        double bits1, evalue1;
        list<TGi> use_this_gi1;

        int score2, sum_n2, num_ident2;
        double bits2, evalue2;
        list<TGi> use_this_gi2;
        GetAlnScores(*(info1->Get().front()), score1,  bits1, evalue1, sum_n1, num_ident1, use_this_gi1);
        GetAlnScores(*(info2->Get().front()), score2,  bits2, evalue2, sum_n2, num_ident2, use_this_gi2);
        retval = evalue1 < evalue2;
    }

    return retval;
}

bool CAlignFormatUtil::SortHitByMasterStartAscending(CRef<CSeq_align_set>& info1,
                                                     CRef<CSeq_align_set>& info2)
{
    int start1 = 0, start2 = 0;


    info1->Set().sort(SortHspByMasterStartAscending);
    info2->Set().sort(SortHspByMasterStartAscending);


    start1 = min(info1->Get().front()->GetSeqStart(0),
                  info1->Get().front()->GetSeqStop(0));
    start2 = min(info2->Get().front()->GetSeqStart(0),
                  info2->Get().front()->GetSeqStop(0));

    if (start1 == start2) {
        //same start then arrange by bits score
        int score1, sum_n1, num_ident1;
        double bits1, evalue1;
        list<TGi> use_this_gi1;

        int score2, sum_n2, num_ident2;
        double bits2, evalue2;
        list<TGi> use_this_gi2;


        GetAlnScores(*(info1->Get().front()), score1,  bits1, evalue1, sum_n1, num_ident1, use_this_gi1);
        GetAlnScores(*(info1->Get().front()), score2,  bits2, evalue2, sum_n2, num_ident2, use_this_gi2);
        return evalue1 < evalue2;

    } else {
        return start1 < start2;
    }

}

bool CAlignFormatUtil::
SortHspByScoreDescending(const CRef<CSeq_align>& info1,
                         const CRef<CSeq_align>& info2)
{

    int score1, sum_n1, num_ident1;
    double bits1, evalue1;
    list<TGi> use_this_gi1;

    int score2, sum_n2, num_ident2;
    double bits2, evalue2;
    list<TGi> use_this_gi2;


    GetAlnScores(*info1, score1,  bits1, evalue1, sum_n1, num_ident1, use_this_gi1);
    GetAlnScores(*info2, score2,  bits2, evalue2, sum_n2, num_ident2, use_this_gi2);
    return bits1 > bits2;

}

bool CAlignFormatUtil::
SortHspByMasterStartAscending(const CRef<CSeq_align>& info1,
                              const CRef<CSeq_align>& info2)
{
    int start1 = 0, start2 = 0;

    start1 = min(info1->GetSeqStart(0), info1->GetSeqStop(0));
    start2 = min(info2->GetSeqStart(0), info2->GetSeqStop(0)) ;

    if (start1 == start2) {
        //same start then arrange by bits score
        int score1, sum_n1, num_ident1;
        double bits1, evalue1;
        list<TGi> use_this_gi1;

        int score2, sum_n2, num_ident2;
        double bits2, evalue2;
        list<TGi> use_this_gi2;


        GetAlnScores(*info1, score1,  bits1, evalue1, sum_n1, num_ident1, use_this_gi1);
        GetAlnScores(*info2, score2,  bits2, evalue2, sum_n2, num_ident2, use_this_gi2);
        return evalue1 < evalue2;

    } else {

        return start1 < start2;
    }
}

bool CAlignFormatUtil::
SortHspBySubjectStartAscending(const CRef<CSeq_align>& info1,
                               const CRef<CSeq_align>& info2)
{
    int start1 = 0, start2 = 0;

    start1 = min(info1->GetSeqStart(1), info1->GetSeqStop(1));
    start2 = min(info2->GetSeqStart(1), info2->GetSeqStop(1)) ;

    if (start1 == start2) {
        //same start then arrange by bits score
        int score1, sum_n1, num_ident1;
        double bits1, evalue1;
        list<TGi> use_this_gi1;

        int score2, sum_n2, num_ident2;
        double bits2, evalue2;
        list<TGi> use_this_gi2;


        GetAlnScores(*info1, score1,  bits1, evalue1, sum_n1, num_ident1, use_this_gi1);
        GetAlnScores(*info2, score2,  bits2, evalue2, sum_n2, num_ident2, use_this_gi2);
        return evalue1 < evalue2;

    } else {

        return start1 < start2;
    }
}

int CAlignFormatUtil::GetAlignmentLength(const CSeq_align& aln, bool do_translation)
{

    CRef<CSeq_align> final_aln;

    // Convert Std-seg and Dense-diag alignments to Dense-seg.
    // Std-segs are produced only for translated searches; Dense-diags only for
    // ungapped, not translated searches.

    if (aln.GetSegs().IsStd()) {
        CRef<CSeq_align> denseg_aln = aln.CreateDensegFromStdseg();
        // When both query and subject are translated, i.e. tblastx, convert
        // to a special type of Dense-seg.
        if (do_translation) {
            final_aln = denseg_aln->CreateTranslatedDensegFromNADenseg();
        } else {
            final_aln = denseg_aln;

        }
    } else if (aln.GetSegs().IsDendiag()) {
        final_aln = CreateDensegFromDendiag(aln);
    }

    const CDense_seg& ds = (final_aln ? final_aln->GetSegs().GetDenseg() :
                            aln.GetSegs().GetDenseg());

    CAlnMap alnmap(ds);
    return alnmap.GetAlnStop() + 1;
}

double CAlignFormatUtil::GetPercentIdentity(const CSeq_align& aln,
                                            CScope& scope,
                                            bool do_translation) {
    double identity = 0;
    CRef<CSeq_align> final_aln;

    // Convert Std-seg and Dense-diag alignments to Dense-seg.
    // Std-segs are produced only for translated searches; Dense-diags only for
    // ungapped, not translated searches.

    if (aln.GetSegs().IsStd()) {
        CRef<CSeq_align> denseg_aln = aln.CreateDensegFromStdseg();
        // When both query and subject are translated, i.e. tblastx, convert
        // to a special type of Dense-seg.
        if (do_translation) {
            final_aln = denseg_aln->CreateTranslatedDensegFromNADenseg();
        } else {
            final_aln = denseg_aln;

        }
    } else if (aln.GetSegs().IsDendiag()) {
        final_aln = CreateDensegFromDendiag(aln);
    }

    const CDense_seg& ds = (final_aln ? final_aln->GetSegs().GetDenseg() :
                            aln.GetSegs().GetDenseg());

    CAlnVec alnvec(ds, scope);
    string query, subject;

    alnvec.SetAaCoding(CSeq_data::e_Ncbieaa);
    alnvec.GetWholeAlnSeqString(0, query);
    alnvec.GetWholeAlnSeqString(1, subject);

    int num_ident = 0;
    int length = (int)min(query.size(), subject.size());

    for (int i = 0; i < length; ++i) {
        if (query[i] == subject[i]) {
            ++num_ident;
        }
    }

    if (length > 0) {
        identity = ((double)num_ident)/length;
    }

    return identity;
}


static void s_CalcAlnPercentIdent(const CRef<CSeq_align_set>& info1,
                               const CRef<CSeq_align_set>& info2,
                               double &percentIdent1,
                               double &percentIdent2)
{

    CRef<CSeq_align_set> i1(info1), i2(info2);
    percentIdent1 = -1;
    percentIdent2 = -1;

    i1->Set().sort(CAlignFormatUtil::SortHspByPercentIdentityDescending);
    i2->Set().sort(CAlignFormatUtil::SortHspByPercentIdentityDescending);

    percentIdent1 = CAlignFormatUtil::GetSeqAlignSetCalcPercentIdent(*info1, kTranslation);
    percentIdent2 = CAlignFormatUtil::GetSeqAlignSetCalcPercentIdent(*info2, kTranslation);
    return;
}


bool CAlignFormatUtil::
SortHitByPercentIdentityDescendingEx(const CRef<CSeq_align_set>& info1,
                                     const CRef<CSeq_align_set>& info2)
{

    CRef<CSeq_align_set> i1(info1), i2(info2);

    //i1->Set().sort(SortHspByPercentIdentityDescending);
    //i2->Set().sort(SortHspByPercentIdentityDescending);


    unique_ptr<CAlignFormatUtil::SSeqAlignSetCalcParams> seqSetInfo1( CAlignFormatUtil::GetSeqAlignSetCalcParamsFromASN(*info1));
    unique_ptr<CAlignFormatUtil::SSeqAlignSetCalcParams> seqSetInfo2( CAlignFormatUtil::GetSeqAlignSetCalcParamsFromASN(*info2));
    double evalue1 = seqSetInfo1->evalue;
    double evalue2 = seqSetInfo2->evalue;
    double percentIdent1 = seqSetInfo1->percent_identity;
    double  percentIdent2 = seqSetInfo2->percent_identity;

    bool retval = false;
    if(percentIdent1 < 0 || percentIdent2 < 0) {
        s_CalcAlnPercentIdent(info1, info2,percentIdent1,percentIdent2);
    }
    if(percentIdent1 > 0 &&percentIdent2 > 0) {
        if (percentIdent1 == percentIdent2) {
            retval = evalue1 < evalue2;

        } else {
            retval = percentIdent1 >= percentIdent2;
        }
    } else {
        retval = evalue1 < evalue2;
    }
    return retval;
}

bool CAlignFormatUtil::SortHitByTotalScoreDescending(CRef<CSeq_align_set> const& info1,
                                                     CRef<CSeq_align_set> const& info2)
{
    int score1,  score2, sum_n, num_ident;
    double bits, evalue;
    list<TGi> use_this_gi;
    double total_bits1 = 0, total_bits2 = 0;

    ITERATE(CSeq_align_set::Tdata, iter, info1->Get()) {
        CAlignFormatUtil::GetAlnScores(**iter, score1, bits, evalue,
                                       sum_n, num_ident, use_this_gi);
        total_bits1 += bits;
    }

    ITERATE(CSeq_align_set::Tdata, iter, info2->Get()) {
        CAlignFormatUtil::GetAlnScores(**iter, score2, bits, evalue,
                                       sum_n, num_ident, use_this_gi);
        total_bits2 += bits;
    }


    return total_bits1 >= total_bits2;

}

#ifndef NCBI_COMPILER_WORKSHOP
/** Class to sort by linkout bit
 * @note this code doesn't compile under the Solaris' WorkShop, and because
 * this feature is only used inside NCBI (LinkoutDB), we disable this code.
 */
class CSortHitByMolecularTypeEx
{
public:
    CSortHitByMolecularTypeEx(ILinkoutDB* linkoutdb,
                              const string& mv_build_name)
        : m_LinkoutDB(linkoutdb), m_MapViewerBuildName(mv_build_name) {}

    bool operator() (const CRef<CSeq_align_set>& info1, const CRef<CSeq_align_set>& info2)
    {
        CConstRef<CSeq_id> id1, id2;
        id1 = &(info1->Get().front()->GetSeq_id(1));
        id2 = &(info2->Get().front()->GetSeq_id(1));

        int linkout1 = 0, linkout2 = 0;
        linkout1 = m_LinkoutDB
            ? m_LinkoutDB->GetLinkout(*id1, m_MapViewerBuildName)
            : 0;
        linkout2 = m_LinkoutDB
            ? m_LinkoutDB->GetLinkout(*id2, m_MapViewerBuildName)
            : 0;

        return (linkout1 & eGenomicSeq) <= (linkout2 & eGenomicSeq);
    }
private:
    ILinkoutDB* m_LinkoutDB;
    string m_MapViewerBuildName;
};
#endif /* NCBI_COMPILER_WORKSHOP */

void CAlignFormatUtil::
SortHitByMolecularType(list< CRef<CSeq_align_set> >& seqalign_hit_list,
                       CScope& scope, ILinkoutDB* linkoutdb,
                       const string& mv_build_name)
{

    kScope = &scope;
#ifndef NCBI_COMPILER_WORKSHOP
    seqalign_hit_list.sort(CSortHitByMolecularTypeEx(linkoutdb, mv_build_name));
#endif /* NCBI_COMPILER_WORKSHOP */
}

void CAlignFormatUtil::SortHit(list< CRef<CSeq_align_set> >& seqalign_hit_list,
                               bool do_translation, CScope& scope, int
                               sort_method, ILinkoutDB* linkoutdb,
                               const string& mv_build_name)
{
    kScope = &scope;
    kTranslation = do_translation;

    if (sort_method == 1) {
#ifndef NCBI_COMPILER_WORKSHOP
        seqalign_hit_list.sort(CSortHitByMolecularTypeEx(linkoutdb,
                                                         mv_build_name));
#endif /* NCBI_COMPILER_WORKSHOP */
    } else if (sort_method == 2) {
        seqalign_hit_list.sort(SortHitByTotalScoreDescending);
    } else if (sort_method == 3) {
        seqalign_hit_list.sort(SortHitByPercentIdentityDescendingEx);
    }
}

void CAlignFormatUtil::
SplitSeqalignByMolecularType(vector< CRef<CSeq_align_set> >&
                             target,
                             int sort_method,
                             const CSeq_align_set& source,
                             CScope& scope,
                             ILinkoutDB* linkoutdb,
                             const string& mv_build_name)
{
    CConstRef<CSeq_id> prevSubjectId;
    int count = 0;
    int linkoutPrev = 0;
    ITERATE(CSeq_align_set::Tdata, iter, source.Get()) {

        const CSeq_id& id = (*iter)->GetSeq_id(1);
        try {
            const CBioseq_Handle& handle = scope.GetBioseqHandle(id);
            if (handle) {
                int linkout;
                if(prevSubjectId.Empty() || !id.Match(*prevSubjectId)){
                    prevSubjectId = &id;
                    linkout = linkoutdb ? linkoutdb->GetLinkout(id, mv_build_name): 0;
                    linkoutPrev = linkout;
                    count++;
                }
                else {
                    linkout = linkoutPrev;
                }
                if (linkout & eGenomicSeq) {
                    if (sort_method == 1) {
                        target[1]->Set().push_back(*iter);
                    } else if (sort_method == 2){
                        target[0]->Set().push_back(*iter);
                    } else {
                        target[1]->Set().push_back(*iter);
                    }
                } else {
                    if (sort_method == 1) {
                        target[0]->Set().push_back(*iter);
                    } else if (sort_method == 2) {
                        target[1]->Set().push_back(*iter);
                    }  else {
                        target[0]->Set().push_back(*iter);
                    }
                }
            } else {
                target[0]->Set().push_back(*iter);
            }

        } catch (const CException&){
            target[0]->Set().push_back(*iter); //no bioseq found, leave untouched
        }
    }
}

void CAlignFormatUtil::HspListToHitList(list< CRef<CSeq_align_set> >& target,
                                        const CSeq_align_set& source)
{
    CConstRef<CSeq_id> previous_id;
    CRef<CSeq_align_set> temp;

    ITERATE(CSeq_align_set::Tdata, iter, source.Get()) {
        const CSeq_id& cur_id = (*iter)->GetSeq_id(1);
        if(previous_id.Empty()) {
            temp =  new CSeq_align_set;
            temp->Set().push_back(*iter);
            target.push_back(temp);
        } else if (cur_id.Match(*previous_id)){
            temp->Set().push_back(*iter);

        } else {
            temp =  new CSeq_align_set;
            temp->Set().push_back(*iter);
            target.push_back(temp);
        }
        previous_id = &cur_id;
    }

}

CRef<CSeq_align_set>
CAlignFormatUtil::HitListToHspList(list< CRef<CSeq_align_set> >& source)
{
    CRef<CSeq_align_set> align_set (new CSeq_align_set);
    CConstRef<CSeq_id> previous_id;
    CRef<CSeq_align_set> temp;
    // list<CRef<CSeq_align_set> >::iterator iter;

    for (list<CRef<CSeq_align_set> >::iterator iter = source.begin(); iter != source.end(); iter ++) {
        ITERATE(CSeq_align_set::Tdata, iter2, (*iter)->Get()) {
            align_set->Set().push_back(*iter2);
        }
    }
    return align_set;
}

map < string, CRef<CSeq_align_set>  >  CAlignFormatUtil::HspListToHitMap(vector <string> seqIdList,
                                       const CSeq_align_set& source)
{
    CConstRef<CSeq_id> previous_id;
    CRef<CSeq_align_set> temp;

    map < string, CRef<CSeq_align_set>  > hitsMap;

    for(size_t i = 0; i < seqIdList.size();i++) {
        CRef<CSeq_align_set> new_aln(new CSeq_align_set);
        hitsMap.insert(map<string, CRef<CSeq_align_set> >::value_type(seqIdList[i],new_aln));
    }
    size_t count = 0;
    ITERATE(CSeq_align_set::Tdata, iter, source.Get()) {
        const CSeq_id& cur_id = (*iter)->GetSeq_id(1);
        if(previous_id.Empty() || !cur_id.Match(*previous_id)) {
            if(count >= seqIdList.size()) {
                break;
            }
            string idString = NStr::TruncateSpaces(cur_id.AsFastaString());
            if(hitsMap.find(idString) != hitsMap.end()) {
                temp =  new CSeq_align_set;
                temp->Set().push_back(*iter);
                hitsMap[idString] = temp;
                count++;
            }
            else {
                temp.Reset();
            }
        }
        else if (cur_id.Match(*previous_id)){
            if(!temp.Empty()) {
                temp->Set().push_back(*iter);
            }
        }
        previous_id = &cur_id;
    }
    return hitsMap;
}

void CAlignFormatUtil::ExtractSeqAlignForSeqList(CRef<CSeq_align_set> &all_aln_set, string alignSeqList)
{
    vector <string> seqIds;
    NStr::Split(alignSeqList,",",seqIds);

    //SEQ_ALN_SET from ALIGNDB contains seq_aligns in random order
    //The followimg will create a map that contains seq-aln_set per gi from ALIGN_SEQ_LIST
    map < string, CRef<CSeq_align_set>  > hitsMap = CAlignFormatUtil::HspListToHitMap(seqIds,*all_aln_set) ;

    map < string, CRef<CSeq_align_set>  >::iterator it;
    list< CRef<CSeq_align_set> > orderedSet;
    //orderedSet wil have seq aligns in th order of gi list
    for(size_t i = 0; i < seqIds.size(); i++) {
        if(hitsMap.find(seqIds[i]) != hitsMap.end()) {
            orderedSet.push_back(hitsMap[seqIds[i]]);
        }
    }
    //This should contain seq align set in the order of gis in the list
    all_aln_set = CAlignFormatUtil::HitListToHspList(orderedSet);
}

static bool s_GetSRASeqMetadata(const CBioseq::TId& ids,string &strRun, string &strSpotId,string &strReadIndex)
{
    bool success = false;
    string link = NcbiEmptyString;
    CConstRef<CSeq_id> seqId = GetSeq_idByType(ids, CSeq_id::e_General);

    if (!seqId.Empty())
    {
        // Get the SRA tag from seqId
        if (seqId->GetGeneral().CanGetDb() &&
            seqId->GetGeneral().CanGetTag() &&
            seqId->GetGeneral().GetTag().IsStr())
        {
            // Decode the tag to collect the SRA-specific indices
            string strTag = seqId->GetGeneral().GetTag().GetStr();
            if (!strTag.empty())
            {
                vector<string> vecInfo;
                try
                {
                    NStr::Split(strTag, ".", vecInfo);
                }
                catch (...)
                {
                    return false;
                }

                if (vecInfo.size() != 3)
                {
                    return false;
                }

                strRun = vecInfo[0];
                strSpotId = vecInfo[1];
                strReadIndex = vecInfo[2];
                success = true;
            }
        }
    }
    return success;
}

string CAlignFormatUtil::BuildSRAUrl(const CBioseq::TId& ids, string user_url)
{
    string strRun, strSpotId,strReadIndex;
    string link = NcbiEmptyString;

    if(s_GetSRASeqMetadata(ids,strRun,strSpotId,strReadIndex))
    {
        // Generate the SRA link to the identified spot
        link += user_url;
        link += "?run=" + strRun;
        link += "." + strSpotId;
        link += "." + strReadIndex;
    }
    return link;
}

string s_GetBestIDForURL(CBioseq::TId& ids)
{
    string gnl;

    CConstRef<CSeq_id> id_general = GetSeq_idByType(ids, CSeq_id::e_General);
    CConstRef<CSeq_id> id_other = GetSeq_idByType(ids, CSeq_id::e_Other);
    const CRef<CSeq_id> id_accession = FindBestChoice(ids, CSeq_id::WorstRank);

    if(!id_general.Empty()  && id_general->AsFastaString().find("gnl|BL_ORD_ID") != string::npos){
        return gnl;
    }

    const CSeq_id* bestid = NULL;
    if (id_general.Empty()){
        bestid = id_other;
        if (id_other.Empty()){
            bestid = id_accession;
        }
    } else {
        bestid = id_general;
    }

    if (bestid && bestid->Which() !=  CSeq_id::e_Gi){
        gnl = NStr::URLEncode(bestid->AsFastaString());
    }
    return gnl;
}

string CAlignFormatUtil::BuildUserUrl(const CBioseq::TId& ids, TTaxId taxid,
                                      string user_url, string database,
                                      bool db_is_na, string rid, int query_number,
                                      bool for_alignment) {

    string link = NcbiEmptyString;
    CConstRef<CSeq_id> id_general = GetSeq_idByType(ids, CSeq_id::e_General);

    if(!id_general.Empty()
       && id_general->AsFastaString().find("gnl|BL_ORD_ID") != string::npos){
        /* We do need to make security protected link to BLAST gnl */
        return NcbiEmptyString;
    }
    TGi gi = FindGi(ids);
    string bestID = s_GetBestIDForURL((CBioseq::TId &)ids);


    bool nodb_path =  false;
    /* dumpgnl.cgi need to use path  */
    if (user_url.find("dumpgnl.cgi") ==string::npos){
        nodb_path = true;
    }
    int length = (int)database.size();
    string str;
    char  *chptr, *dbtmp;
    char tmpbuff[256];
    char* dbname = new char[sizeof(char)*length + 2];
    strcpy(dbname, database.c_str());
    if(nodb_path) {
        int i, j;
        dbtmp = new char[sizeof(char)*length + 2]; /* aditional space and NULL */
        memset(dbtmp, '\0', sizeof(char)*length + 2);
        for(i = 0; i < length; i++) {
            if(i > 0) {
                strcat(dbtmp, " ");  //space between db
            }
            if(isspace((unsigned char) dbname[i]) || dbname[i] == ',') {/* Rolling spaces */
                continue;
            }
            j = 0;
            while (!isspace((unsigned char) dbname[i]) && j < 256  && i < length) {
                tmpbuff[j] = dbname[i];
                j++; i++;
                if(dbname[i] == ',') { /* Comma is valid delimiter */
                    break;
                }
            }
            tmpbuff[j] = '\0';
            if((chptr = strrchr(tmpbuff, '/')) != NULL) {
                strcat(dbtmp, (char*)(chptr+1));
            } else {
                strcat(dbtmp, tmpbuff);
            }

        }
    } else {
        dbtmp = dbname;
    }

    char gnl[256];
    if (!bestID.empty()){
        strcpy(gnl, bestID.c_str());

    } else {
        gnl[0] = '\0';
    }

    str = NStr::URLEncode(dbtmp == NULL ? (char*) "nr" : dbtmp);

    if (user_url.find("?") == string::npos){
        link += user_url + "?" + "db=" + str + "&na=" + (db_is_na? "1" : "0");
    } else {
        if (user_url.find("=") != string::npos) {
            user_url += "&";
        }
        link += user_url + "db=" + str + "&na=" + (db_is_na? "1" : "0");
    }

    if (gnl[0] != '\0'){
        str = gnl;
        link += "&gnl=";
        link += str;
    }
    if (gi > ZERO_GI){
        link += "&gi=" + NStr::NumericToString(gi);
        link += "&term=" + NStr::NumericToString(gi) + NStr::URLEncode("[gi]");
    }
    if(taxid > ZERO_TAX_ID){
        link += "&taxid=" + NStr::NumericToString(taxid);
    }
    if (rid != NcbiEmptyString){
        link += "&RID=" + rid;
    }

    if (query_number > 0){
        link += "&QUERY_NUMBER=" + NStr::IntToString(query_number);
    }

    if (user_url.find("dumpgnl.cgi") ==string::npos){
        if (for_alignment)
            link += "&log$=nuclalign";
        else
            link += "&log$=nucltop";
    }

    if(nodb_path){
        delete [] dbtmp;
    }
    delete [] dbname;
    return link;
}
void CAlignFormatUtil::
BuildFormatQueryString (CCgiContext& ctx,
                        map< string, string>& parameters_to_change,
                        string& cgi_query)
{

    //add parameters to exclude
    parameters_to_change.insert(map<string, string>::
                                value_type("service", ""));
    parameters_to_change.insert(map<string, string>::
                                value_type("address", ""));
    parameters_to_change.insert(map<string, string>::
                                value_type("platform", ""));
    parameters_to_change.insert(map<string, string>::
                                    value_type("_pgr", ""));
    parameters_to_change.insert(map<string, string>::
                                value_type("client", ""));
    parameters_to_change.insert(map<string, string>::
                                value_type("composition_based_statistics", ""));

    parameters_to_change.insert(map<string, string>::
                                value_type("auto_format", ""));
    cgi_query = NcbiEmptyString;
    TCgiEntries& cgi_entry = ctx.GetRequest().GetEntries();
    bool is_first = true;

    for(TCgiEntriesI it=cgi_entry.begin(); it!=cgi_entry.end(); ++it) {
        string parameter = it->first;
        if (parameter != NcbiEmptyString) {
            if (parameters_to_change.count(NStr::ToLower(parameter)) > 0 ||
                parameters_to_change.count(NStr::ToUpper(parameter)) > 0) {
                if(parameters_to_change[NStr::ToLower(parameter)] !=
                   NcbiEmptyString &&
                   parameters_to_change[NStr::ToUpper(parameter)] !=
                   NcbiEmptyString) {
                    if (!is_first) {
                        cgi_query += "&";
                    }
                    cgi_query +=
                        it->first + "=" + parameters_to_change[it->first];
                    is_first = false;
                }
            } else {
                if (!is_first) {
                    cgi_query += "&";
                }
                cgi_query += it->first + "=" + it->second;
                is_first = false;
            }

        }
    }
}

void CAlignFormatUtil::BuildFormatQueryString(CCgiContext& ctx, string& cgi_query) {

    string format_type = ctx.GetRequestValue("FORMAT_TYPE").GetValue();
    string ridstr = ctx.GetRequestValue("RID").GetValue();
    string align_view = ctx.GetRequestValue("ALIGNMENT_VIEW").GetValue();

    cgi_query += "RID=" + ridstr;
    cgi_query += "&FORMAT_TYPE=" + format_type;
    cgi_query += "&ALIGNMENT_VIEW=" + align_view;

    cgi_query += "&QUERY_NUMBER=" + ctx.GetRequestValue("QUERY_NUMBER").GetValue();
    cgi_query += "&FORMAT_OBJECT=" + ctx.GetRequestValue("FORMAT_OBJECT").GetValue();
    cgi_query += "&RUN_PSIBLAST=" + ctx.GetRequestValue("RUN_PSIBLAST").GetValue();
    cgi_query += "&I_THRESH=" + ctx.GetRequestValue("I_THRESH").GetValue();

    cgi_query += "&DESCRIPTIONS=" + ctx.GetRequestValue("DESCRIPTIONS").GetValue();

    cgi_query += "&ALIGNMENTS=" + ctx.GetRequestValue("ALIGNMENTS").GetValue();

    cgi_query += "&NUM_OVERVIEW=" + ctx.GetRequestValue("NUM_OVERVIEW").GetValue();

    cgi_query += "&NCBI_GI=" + ctx.GetRequestValue("NCBI_GI").GetValue();

    cgi_query += "&SHOW_OVERVIEW=" + ctx.GetRequestValue("SHOW_OVERVIEW").GetValue();

    cgi_query += "&SHOW_LINKOUT=" + ctx.GetRequestValue("SHOW_LINKOUT").GetValue();

    cgi_query += "&GET_SEQUENCE=" + ctx.GetRequestValue("GET_SEQUENCE").GetValue();

    cgi_query += "&MASK_CHAR=" + ctx.GetRequestValue("MASK_CHAR").GetValue();
    cgi_query += "&MASK_COLOR=" + ctx.GetRequestValue("MASK_COLOR").GetValue();

    cgi_query += "&SHOW_CDS_FEATURE=" + ctx.GetRequestValue("SHOW_CDS_FEATURE").GetValue();

    if (ctx.GetRequestValue("FORMAT_EQ_TEXT").GetValue() != NcbiEmptyString) {
        cgi_query += "&FORMAT_EQ_TEXT=" +
            NStr::URLEncode(NStr::TruncateSpaces(ctx.
            GetRequestValue("FORMAT_EQ_TEXT").
            GetValue()));
    }

    if (ctx.GetRequestValue("FORMAT_EQ_OP").GetValue() != NcbiEmptyString) {
        cgi_query += "&FORMAT_EQ_OP=" +
            NStr::URLEncode(NStr::TruncateSpaces(ctx.
            GetRequestValue("FORMAT_EQ_OP").
            GetValue()));
    }

    if (ctx.GetRequestValue("FORMAT_EQ_MENU").GetValue() != NcbiEmptyString) {
        cgi_query += "&FORMAT_EQ_MENU=" +
            NStr::URLEncode(NStr::TruncateSpaces(ctx.
            GetRequestValue("FORMAT_EQ_MENU").
            GetValue()));
    }

    cgi_query += "&EXPECT_LOW=" + ctx.GetRequestValue("EXPECT_LOW").GetValue();
    cgi_query += "&EXPECT_HIGH=" + ctx.GetRequestValue("EXPECT_HIGH").GetValue();

    cgi_query += "&BL2SEQ_LINK=" + ctx.GetRequestValue("BL2SEQ_LINK").GetValue();

}


bool CAlignFormatUtil::IsMixedDatabase(const CSeq_align_set& alnset,
                                       CScope& scope, ILinkoutDB* linkoutdb,
                                       const string& mv_build_name)
{
    bool is_mixed = false;
    bool is_first = true;
    int prev_database = 0;

    ITERATE(CSeq_align_set::Tdata, iter, alnset.Get()) {

        const CSeq_id& id = (*iter)->GetSeq_id(1);
        int linkout = linkoutdb
            ? linkoutdb->GetLinkout(id, mv_build_name)
            : 0;
        int cur_database = (linkout & eGenomicSeq);
        if (!is_first && cur_database != prev_database) {
            is_mixed = true;
            break;
        }
        prev_database = cur_database;
        is_first = false;
    }

    return is_mixed;

}


bool CAlignFormatUtil::IsMixedDatabase(CCgiContext& ctx)
{
    bool formatAsMixedDbs = false;
    string mixedDbs = ctx.GetRequestValue("MIXED_DATABASE").GetValue();
    if(!mixedDbs.empty()) {
        mixedDbs = NStr::ToLower(mixedDbs);
        formatAsMixedDbs = (mixedDbs == "on" || mixedDbs == "true" || mixedDbs == "yes") ? true : false;
    }
    return formatAsMixedDbs;
}

static string s_MapLinkoutGenParam(string &url_link_tmpl,
                                   const string& rid,
                                   string giList,
                                   bool for_alignment,
                                   int cur_align,
                                   string &label,
                                   string &lnk_displ,
                                   string lnk_tl_info = "",
                                   string lnk_title = "")
{
    const string kLinkTitle=" title=\"View <@lnk_tl_info@> for <@label@>\" ";
    const string kLinkTarget="target=\"lnk" + rid + "\"";
    string lnkTitle = (lnk_title.empty()) ? kLinkTitle : lnk_title;
    string url_link = CAlignFormatUtil::MapTemplate(url_link_tmpl,"gi",giList);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"rid",rid);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"log",for_alignment? "align" : "top");
    url_link = CAlignFormatUtil::MapTemplate(url_link,"blast_rank",NStr::IntToString(cur_align));
    lnkTitle = NStr::StartsWith(lnk_displ,"<img") ? "" : lnkTitle;
    string lnkTarget = NStr::StartsWith(lnk_displ,"<img") ? "" : kLinkTarget;
    url_link = CAlignFormatUtil::MapTemplate(url_link,"lnkTitle",lnkTitle);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"lnkTarget",lnkTarget);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"lnk_displ",lnk_displ);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"lnk_tl_info",lnk_tl_info);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"label",label);
    url_link = CAlignFormatUtil::MapProtocol(url_link);
    return url_link;
}


static list<string> s_GetLinkoutUrl(int linkout,
                                    string giList,
                                    string labelList,
                                    TGi first_gi,
                                    CAlignFormatUtil::SLinkoutInfo &linkoutInfo,
                                    bool textLink = true)

{
    list<string> linkout_list;
    string url_link,lnk_displ,lnk_title,lnkTitleInfo;

    vector<string> accs;
    NStr::Split(labelList,",",accs);
    string firstAcc = (accs.size() > 0)? accs[0] : labelList;

    if (linkout & eUnigene) {
        url_link = CAlignFormatUtil::GetURLFromRegistry("UNIGEN");
        lnk_displ = textLink ? "UniGene" : kUnigeneImg;

        string termParam = NStr::Find(labelList,",") == NPOS ? kGeneTerm : ""; //kGeneTerm if only one seqid
        url_link = CAlignFormatUtil::MapTemplate(url_link,"termParam",termParam);

        lnkTitleInfo = "UniGene cluster";
        string uid = !linkoutInfo.is_na ? "[Protein Accession]" : "[Nucleotide Accession]";
        url_link = CAlignFormatUtil::MapTemplate(url_link,"uid",uid);
        url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,labelList,lnk_displ,lnkTitleInfo);

        if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kUnigeneDispl,"lnk",url_link);
        }
        url_link = CAlignFormatUtil::MapProtocol(url_link);
        linkout_list.push_back(url_link);
    }
    if (linkout & eStructure){
        CSeq_id seqID(firstAcc);
        string struct_link = CAlignFormatUtil::GetURLFromRegistry(
                                                             "STRUCTURE_URL");

        url_link = struct_link.empty() ? kStructureUrl : struct_link;
        string linkTitle;
        if(seqID.Which() == CSeq_id::e_Pdb) {
            lnk_displ = textLink ? "Structure" : kStructureImg;
            linkTitle = " title=\"View 3D structure <@label@>\"";
        }
        else {
            url_link = kStructureAlphaFoldUrl;
            lnk_displ = textLink ? "AlphaFold Structure" : kStructureImg;
            linkTitle = " title=\"View AlphaFold 3D structure <@label@>\"";
        }



        string molID,chainID;
        NStr::SplitInTwo(firstAcc,"_",molID,chainID);
        url_link = CAlignFormatUtil::MapTemplate(url_link,"molid",molID);
        url_link = CAlignFormatUtil::MapTemplate(url_link,"queryID",linkoutInfo.queryID);
        url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,firstAcc,lnk_displ,"",linkTitle);
        if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kStructureDispl,"lnk",url_link);
        }
        url_link = CAlignFormatUtil::MapProtocol(url_link);
        linkout_list.push_back(url_link);
    }
    if (linkout & eGeo){
        url_link = CAlignFormatUtil::GetURLFromRegistry("GEO");
        lnk_displ = textLink ? "GEO Profiles" : kGeoImg;

        lnkTitleInfo = "Expression profiles";
        //gilist contains comma separated gis
        url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,labelList,lnk_displ,lnkTitleInfo);


        if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kGeoDispl,"lnk",url_link);
        }
        url_link = CAlignFormatUtil::MapProtocol(url_link);
        linkout_list.push_back(url_link);
    }
    if(linkout & eGene){
      url_link = CAlignFormatUtil::GetURLFromRegistry("GENE");
      if(textLink) {        
        lnk_displ = "Gene";
        lnkTitleInfo = "gene information";
      }
      else {
        lnk_displ = kGeneImg;
      }
      string termParam = NStr::Find(labelList,",") == NPOS ? kGeneTerm : ""; //kGeneTerm if only one seqid
      url_link = CAlignFormatUtil::MapTemplate(url_link,"termParam",termParam);

      string uid = !linkoutInfo.is_na ? "[Protein Accession]" : "[Nucleotide Accession]";
      url_link = CAlignFormatUtil::MapTemplate(url_link,"uid",uid);

      url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,labelList,lnk_displ,lnkTitleInfo);

      if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kGeneDispl,"lnk",url_link);
      }
      url_link = CAlignFormatUtil::MapProtocol(url_link);
      linkout_list.push_back(url_link);
    }

    if((linkout & eGenomicSeq)  && first_gi != ZERO_GI){  //only for advanced view -> textlink = true
        if(textLink) {
            url_link = kMapviewBlastHitParams;
            lnk_displ = "Map Viewer";

            lnkTitleInfo = "BLAST hits on the " + linkoutInfo.taxName + " genome";

            url_link = CAlignFormatUtil::MapTemplate(url_link,"gnl",NStr::URLEncode(linkoutInfo.gnl));
            url_link = CAlignFormatUtil::MapTemplate(url_link,"db",linkoutInfo.database);
            url_link = CAlignFormatUtil::MapTemplate(url_link,"is_na",linkoutInfo.is_na? "1" : "0");
            string user_url = (linkoutInfo.user_url.empty()) ? kMapviewBlastHitUrl : linkoutInfo.user_url;
            url_link = CAlignFormatUtil::MapTemplate(url_link,"user_url",user_url);

            string taxIDStr = (linkoutInfo.taxid > ZERO_TAX_ID) ? NStr::NumericToString(linkoutInfo.taxid) : "";
            url_link = CAlignFormatUtil::MapTemplate(url_link,"taxid",taxIDStr);

            string queryNumStr = (linkoutInfo.query_number > 0) ? NStr::IntToString(linkoutInfo.query_number) : "";
            url_link = CAlignFormatUtil::MapTemplate(url_link,"query_number",queryNumStr);  //gi,term

            string giStr = (first_gi > ZERO_GI)? NStr::NumericToString(first_gi) : "";
            url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giStr,linkoutInfo.for_alignment, linkoutInfo.cur_align,labelList,lnk_displ,lnkTitleInfo);

            if(textLink) {
                url_link = CAlignFormatUtil::MapTemplate(kMapviwerDispl,"lnk",url_link);
            }
            url_link = CAlignFormatUtil::MapProtocol(url_link);
            linkout_list.push_back(url_link);
        }
    }
    else if((linkout & eMapviewer) && first_gi != ZERO_GI){
        url_link = kMapviwerUrl;
        lnk_displ = textLink ? "Map Viewer" : kMapviwerImg;

        string linkTitle = " title=\"View <@label@> aligned to the "  + linkoutInfo.taxName + " genome\"";
        url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,labelList,lnk_displ,"",linkTitle);

        if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kMapviwerDispl,"lnk",url_link);
        }
        url_link = CAlignFormatUtil::MapProtocol(url_link);
        linkout_list.push_back(url_link);
    }
    //View Bioassays involving <accession
    if(linkout & eBioAssay && linkoutInfo.is_na && first_gi != ZERO_GI){
        url_link = CAlignFormatUtil::GetURLFromRegistry("BIOASSAY_NUC");        
        lnk_displ = textLink ? "PubChem BioAssay" : kBioAssayNucImg;

        string linkTitle = " title=\"View Bioassays involving <@label@>\"";
        //gilist contains comma separated gis, change it to the following
        giList = NStr::Replace(giList,",","[RNATargetGI] OR ");
        url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,labelList,lnk_displ,"",linkTitle);

        if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kBioAssayDispl,"lnk",url_link);
        }
        url_link = CAlignFormatUtil::MapProtocol(url_link);
        linkout_list.push_back(url_link);
    }
    else if (linkout & eBioAssay && !linkoutInfo.is_na && first_gi != ZERO_GI) {
        url_link = CAlignFormatUtil::GetURLFromRegistry("BIOASSAY_PROT");
        lnk_displ = textLink ? "PubChem BioAssay" : kBioAssayProtImg;

        lnkTitleInfo ="Bioassay data";
        string linkTitle = " title=\"View Bioassays involving <@label@>\"";
        //gilist contains comma separated gis, change it to the following
        giList = NStr::Replace(giList,",","[PigGI] OR ");
        url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,labelList,lnk_displ,"",linkTitle);

        if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kBioAssayDispl,"lnk",url_link);
        }
        url_link = CAlignFormatUtil::MapProtocol(url_link);
        linkout_list.push_back(url_link);
    }
    if(linkout & eReprMicrobialGenomes){
        url_link = CAlignFormatUtil::GetURLFromRegistry("REPR_MICROBIAL_GENOMES");
        lnk_displ = textLink ? "Genome" : kReprMicrobialGenomesImg;

        lnkTitleInfo = "genomic information";
        //gilist contains comma separated gis
        string uid = !linkoutInfo.is_na ? "Protein Accession" : "Nucleotide Accession";
        url_link = CAlignFormatUtil::MapTemplate(url_link,"uid",uid);
        url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,labelList,lnk_displ,lnkTitleInfo);

        if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kReprMicrobialGenomesDispl,"lnk",url_link);
        }
        url_link = CAlignFormatUtil::MapProtocol(url_link);
        linkout_list.push_back(url_link);
    }
    if((linkout & eGenomeDataViewer) || (linkout & eTranscript)){
        string urlTag;
        lnk_displ = textLink ? "Genome Data Viewer" : kGenomeDataViewerImg;
        if(linkout & eTranscript) {
            urlTag = "GENOME_DATA_VIEWER_TRANSCR";
            lnkTitleInfo = "title=\"View the annotation of the transcript <@label@> within a genomic context in NCBI's Genome Data Viewer (GDV)- genome browser for RefSeq annotated assemblies. See other genomic features annotated at the same location as the protein annotation and browse to other regions.\"";
        }
        else {
            urlTag = linkoutInfo.is_na ? "GENOME_DATA_VIEWER_NUC" : "GENOME_DATA_VIEWER_PROT";
            lnkTitleInfo = linkoutInfo.is_na ?
                         "title=\"View BLAST hits for <@label@> within a genomic context in NCBI's Genome Data Viewer (GDV)- genome browser for RefSeq annotated assemblies. See other genomic features annotated at the same location as hits and browse to other regions.\""
                         :
                         "title=\"View the annotation of the protein <@label@> within a genomic context in NCBI's Genome Data Viewer (GDV)- genome browser for RefSeq annotated assemblies. See other genomic features annotated at the same location as the protein annotation and browse to other regions.\"";
        }
        url_link = CAlignFormatUtil::GetURLFromRegistry(urlTag);
        url_link = s_MapLinkoutGenParam(url_link,linkoutInfo.rid,giList,linkoutInfo.for_alignment, linkoutInfo.cur_align,firstAcc,lnk_displ,"",lnkTitleInfo);

        url_link = CAlignFormatUtil::MapTemplate(url_link,"queryID",linkoutInfo.queryID);

        TSeqPos seqFrom = linkoutInfo.subjRange.GetFrom();
        seqFrom = (seqFrom == 0) ? seqFrom : seqFrom - 1;

        TSeqPos seqTo = linkoutInfo.subjRange.GetTo();
        seqTo = (seqTo == 0) ? seqTo : seqTo - 1;

        url_link = CAlignFormatUtil::MapTemplate(url_link,"from",seqFrom);//-1
        url_link = CAlignFormatUtil::MapTemplate(url_link,"to",seqTo);//-1

        if(textLink) {
            url_link = CAlignFormatUtil::MapTemplate(kGenomeDataViewerDispl,"lnk",url_link);
        }
        url_link = CAlignFormatUtil::MapProtocol(url_link);
        linkout_list.push_back(url_link);
    }
    return linkout_list;
}

///Get list of linkouts for one sequence
list<string> CAlignFormatUtil::GetLinkoutUrl(int linkout, const CBioseq::TId& ids,
                                             const string& rid,
                                             const string& cdd_rid,
                                             const string& entrez_term,
                                             bool is_na,
                                             TGi first_gi,
                                             bool structure_linkout_as_group,
                                             bool for_alignment, int cur_align,
                                             string preComputedResID)

{
    list<string> linkout_list;
    TGi gi = FindGi(ids);
    CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
    string label;
    wid->GetLabel(&label, CSeq_id::eContent);
    string giString = NStr::NumericToString(gi);
    first_gi = (first_gi == ZERO_GI) ? gi : first_gi;



    SLinkoutInfo linkoutInfo;
    linkoutInfo.Init(rid,
                     cdd_rid,
                     entrez_term,
                     is_na,
                     "",  //database
                     0,  //query_number
                     "", //user_url
                     preComputedResID,
                     "", //linkoutOrder
                     structure_linkout_as_group,
                     for_alignment);

    linkoutInfo.cur_align = cur_align;
    linkoutInfo.taxid = ZERO_TAX_ID;

    linkout_list = s_GetLinkoutUrl(linkout,
                                   giString,
                                   label,
                                   first_gi,
                                   linkoutInfo,
                                   false); //textlink

    return linkout_list;
}

static int s_LinkLetterToType(string linkLetter)
{
    int linkType = 0;
    if(linkLetter == "U") {
        linkType = eUnigene;
    }
    else if(linkLetter == "S") {
           linkType = eStructure;
    }
	else if(linkLetter == "E") {
         linkType = eGeo;
    }
    else if(linkLetter == "G") {
        linkType = eGene;
    }
    else if(linkLetter == "M") {
        linkType = eMapviewer | eGenomicSeq;
    }
    else if(linkLetter == "N") {
        linkType = eGenomicSeq;
    }
    else if(linkLetter == "B") {
        linkType = eBioAssay;
    }
    else if(linkLetter == "R") {
        linkType = eReprMicrobialGenomes;
    }
    else if(linkLetter == "V") {
        linkType = eGenomeDataViewer;
    }
    else if(linkLetter == "T") {
        linkType = eTranscript;
    }

    return linkType;
}


static void s_AddLinkoutInfo(map<int, vector < CBioseq::TId > > &linkout_map,int linkout,CBioseq::TId &cur_id)
{
    if(linkout_map.count(linkout) > 0){
        linkout_map[linkout].push_back(cur_id);
    }
    else {
        vector <CBioseq::TId > idList;
        idList.push_back(cur_id);
        linkout_map.insert(map<int,  vector <CBioseq::TId > >::value_type(linkout,idList));
    }
}

int CAlignFormatUtil::GetSeqLinkoutInfo(CBioseq::TId& cur_id,
                                    ILinkoutDB **linkoutdb,
                                    const string& mv_build_name,
                                    TGi gi)
{
    int linkout = 0;

    if(*linkoutdb) {
        if(gi == INVALID_GI) {
            gi = FindGi(cur_id);
        }
        try {
            if(gi > ZERO_GI) {
                linkout = (*linkoutdb)->GetLinkout(gi, mv_build_name);
            }
            else if(GetTextSeqID(cur_id)){
                CRef<CSeq_id> seqID = FindBestChoice(cur_id, CSeq_id::WorstRank);
                linkout = (*linkoutdb)->GetLinkout(*seqID, mv_build_name);
                string str_id = seqID->GetSeqIdString(false);
                CRef<CSeq_id> seqIDNew(new CSeq_id(str_id));
                int linkoutWithoutVersion = (*linkoutdb)->GetLinkout(*seqIDNew, mv_build_name);
                if(linkoutWithoutVersion && (linkoutWithoutVersion | eStructure)) {
                    linkout = linkout | linkoutWithoutVersion;
                }
            }
        }
        catch (const CException & e) {
            ERR_POST("Problem with linkoutdb: " + e.GetMsg());
            cerr << "[BLAST FORMATTER EXCEPTION] Problem with linkoutdb: " << e.GetMsg() << endl;
            *linkoutdb = NULL;
        }
    }
    return linkout;
}

void
CAlignFormatUtil::GetBdlLinkoutInfo(CBioseq::TId& cur_id,
                                    map<int, vector <CBioseq::TId > > &linkout_map,
                                    ILinkoutDB* linkoutdb,
                                    const string& mv_build_name)
{
        if(!linkoutdb) return;

        int linkout = GetSeqLinkoutInfo(cur_id,
                                    &linkoutdb,
                                    mv_build_name);

        if(linkout & eGene){
            s_AddLinkoutInfo(linkout_map,eGene,cur_id);
        }
        if (linkout & eUnigene) {
            s_AddLinkoutInfo(linkout_map,eUnigene,cur_id);
        }
        if (linkout & eGeo){
            s_AddLinkoutInfo(linkout_map,eGeo,cur_id);
        }
        if (linkout & eStructure){
            s_AddLinkoutInfo(linkout_map,eStructure,cur_id);
        }
        //eGenomicSeq and eMapviewer cannot combine together
        if((linkout & eGenomicSeq) && (linkout & eMapviewer)){
            s_AddLinkoutInfo(linkout_map,eGenomicSeq,cur_id);
        }
        else if(linkout & eMapviewer){
            s_AddLinkoutInfo(linkout_map,eMapviewer,cur_id);
        }
        if(linkout & eBioAssay){
            s_AddLinkoutInfo(linkout_map,eBioAssay,cur_id);
        }
        if(linkout & eReprMicrobialGenomes){
            s_AddLinkoutInfo(linkout_map,eReprMicrobialGenomes,cur_id);
        }

        if(linkout & eGenomeDataViewer){
            s_AddLinkoutInfo(linkout_map,eGenomeDataViewer,cur_id);
        }
        if(linkout & eTranscript){
            s_AddLinkoutInfo(linkout_map,eTranscript,cur_id);
        }

}

void
CAlignFormatUtil::GetBdlLinkoutInfo(const list< CRef< CBlast_def_line > > &bdl,
                                    map<int, vector <CBioseq::TId > > &linkout_map,
                                    ILinkoutDB* linkoutdb,
                                    const string& mv_build_name)
{
    const int kMaxDeflineNum = 10;
    int num = 0;
    for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
        CBioseq::TId& cur_id = (CBioseq::TId &)(*iter)->GetSeqid();

        GetBdlLinkoutInfo(cur_id,
                          linkout_map,
                          linkoutdb,
                          mv_build_name);
        num++;
        if(num > kMaxDeflineNum) break;
    }
}

static string s_GetTaxName(TTaxId taxid)
{
    string taxName;
    try {
        if(taxid != ZERO_TAX_ID) {
            SSeqDBTaxInfo info;
            CSeqDB::GetTaxInfo(taxid, info);
            taxName = info.common_name;
        }
    }
    catch (CException&) {

    }
    return taxName;
}

void s_AddOtherRelatedInfoLinks(CBioseq::TId& cur_id,
                                const string& rid,
                                bool is_na,
                                bool for_alignment,
                                int cur_align,
                                list<string> &linkout_list)

{
    //Identical Proteins

     CRef<CSeq_id> wid = FindBestChoice(cur_id, CSeq_id::WorstRank);
     if (CAlignFormatUtil::GetTextSeqID(wid)) {
        string label;
        wid->GetLabel(&label, CSeq_id::eContent);
        string url_link = kIdenticalProteinsUrl;
        string lnk_displ = "Identical Proteins";
        url_link = s_MapLinkoutGenParam(url_link,rid,NStr::NumericToString(ZERO_GI),for_alignment, cur_align,label,lnk_displ);
        url_link = CAlignFormatUtil::MapTemplate(kIdenticalProteinsDispl,"lnk",url_link);
        url_link = CAlignFormatUtil::MapTemplate(url_link,"label",label);
        linkout_list.push_back(url_link);
    }
}



//reset:taxname,gnl
static list<string> s_GetFullLinkoutUrl(CBioseq::TId& cur_id,
                                        CAlignFormatUtil::SLinkoutInfo &linkoutInfo,
                                        map<int, vector < CBioseq::TId > >  &linkout_map,
                                        bool getIdentProteins)

{
    list<string> linkout_list;

    vector<string> linkLetters;
    NStr::Split(linkoutInfo.linkoutOrder,",",linkLetters); //linkoutOrder = "G,U,M,V,E,S,B,R,T"
	for(size_t i = 0; i < linkLetters.size(); i++) {
        TGi first_gi = ZERO_GI;
        vector < CBioseq::TId > idList;
        int linkout = s_LinkLetterToType(linkLetters[i]);
        linkoutInfo.taxName.clear();
        if(linkout & (eMapviewer | eGenomicSeq)) {
            linkout = (linkout_map[eGenomicSeq].size() != 0) ? eGenomicSeq : eMapviewer;
            linkoutInfo.taxName = s_GetTaxName(linkoutInfo.taxid);
        }
        if(linkout_map.find(linkout) != linkout_map.end()) {
            idList = linkout_map[linkout];
        }
        bool disableLink = (linkout == 0 || idList.size() == 0 || ( (linkout & eStructure) && (linkoutInfo.cdd_rid == "" || linkoutInfo.cdd_rid == "0")));

        string giList,labelList;
        int seqVersion = ((linkout & eGenomeDataViewer) || (linkout & eTranscript)) ? true : false;
        for (size_t i = 0; i < idList.size(); i++) {
            const CBioseq::TId& ids = idList[i];
            TGi gi = FindGi(ids);
            if (first_gi == ZERO_GI) first_gi = gi;


            CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
            string label = CAlignFormatUtil::GetLabel(wid,seqVersion);
            if(!labelList.empty()) labelList += ",";
            labelList += label;

            //use only first gi for bioAssay protein
            if(!giList.empty() && (linkout & eBioAssay) && !linkoutInfo.is_na) continue;
            if(!giList.empty()) giList += ",";
            giList += NStr::NumericToString(gi);
        }

        linkoutInfo.gnl.clear();
        if(!disableLink && linkout == eGenomicSeq) {
            linkoutInfo.gnl = s_GetBestIDForURL(cur_id);
        }

        if(!disableLink) {//
            //The following list will contain only one entry for single linkout value
            list<string> one_linkout = s_GetLinkoutUrl(linkout,
                                                          giList,
                                                          labelList,
                                                          first_gi,
                                                          linkoutInfo);
            if(one_linkout.size() > 0) {
                list<string>::iterator iter = one_linkout.begin();
                linkout_list.push_back(*iter);
            }
        }
 }
 if(getIdentProteins) {
    s_AddOtherRelatedInfoLinks(cur_id,linkoutInfo.rid,linkoutInfo.is_na,linkoutInfo.for_alignment,linkoutInfo.cur_align,linkout_list);
 }
 return linkout_list;
}

list<string> CAlignFormatUtil::GetFullLinkoutUrl(const list< CRef< CBlast_def_line > > &bdl,
                                                    CAlignFormatUtil::SLinkoutInfo &linkoutInfo)
{
    list<string> linkout_list;
    map<int, vector < CBioseq::TId > >  linkout_map;
    if(bdl.size() > 0) {
    	GetBdlLinkoutInfo(bdl,linkout_map, linkoutInfo.linkoutdb, linkoutInfo.mv_build_name);
    	list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
    	CBioseq::TId& cur_id = (CBioseq::TId &)(*iter)->GetSeqid();
        linkout_list = s_GetFullLinkoutUrl(cur_id,
                                           linkoutInfo,
                                           linkout_map,
                                           !linkoutInfo.is_na && bdl.size() > 1);
    }
    return linkout_list;
}


list<string> CAlignFormatUtil::GetFullLinkoutUrl(const list< CRef< CBlast_def_line > > &bdl,
                                                 const string& rid,
                                                 const string& cdd_rid,
                                                 const string& entrez_term,
                                                 bool is_na,
                                                 bool structure_linkout_as_group,
                                                 bool for_alignment,
                                                 int cur_align,
                                                 string& linkoutOrder,
                                                 TTaxId taxid,
                                                 string &database,
                                                 int query_number,
                                                 string &user_url,
                                                 string &preComputedResID,
                                                 ILinkoutDB* linkoutdb,
                                                 const string& mv_build_name)

{
    list<string> linkout_list;
    map<int, vector < CBioseq::TId > >  linkout_map;
    if(bdl.size() > 0) {
    	GetBdlLinkoutInfo(bdl,linkout_map, linkoutdb, mv_build_name);
    	list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
    	CBioseq::TId& cur_id = (CBioseq::TId &)(*iter)->GetSeqid();

        SLinkoutInfo linkoutInfo;
        linkoutInfo.Init(rid,
                         cdd_rid,
                         entrez_term,
                         is_na,
                         database,
                         query_number,
                         user_url,
                         preComputedResID,
                         linkoutOrder,
                         structure_linkout_as_group,
                         for_alignment);

        linkoutInfo.cur_align = cur_align;
        linkoutInfo.taxid = taxid;

        linkout_list = s_GetFullLinkoutUrl(cur_id,
                                        linkoutInfo,
                                        linkout_map,
                                        !is_na && bdl.size() > 1);
    }
    return linkout_list;
}


list<string> CAlignFormatUtil::GetFullLinkoutUrl(CBioseq::TId& cur_id,
                                                 CAlignFormatUtil::SLinkoutInfo &linkoutInfo,
                                                 bool getIdentProteins)
{
    list<string> linkout_list;
    map<int, vector < CBioseq::TId > >  linkout_map;

    GetBdlLinkoutInfo(cur_id,linkout_map, linkoutInfo.linkoutdb, linkoutInfo.mv_build_name);
    linkout_list = s_GetFullLinkoutUrl(cur_id,
                                       linkoutInfo,
                                       linkout_map,
                                       getIdentProteins);
    return linkout_list;
}

list<string> CAlignFormatUtil::GetFullLinkoutUrl(CBioseq::TId& cur_id,
                                                 const string& rid,
                                                 const string& cdd_rid,
                                                 const string& entrez_term,
                                                 bool is_na,
                                                 bool structure_linkout_as_group,
                                                 bool for_alignment,
                                                 int cur_align,
                                                 string& linkoutOrder,
                                                 TTaxId taxid,
                                                 string &database,
                                                 int query_number,
                                                 string &user_url,
                                                 string &preComputedResID,
                                                 ILinkoutDB* linkoutdb,
                                                 const string& mv_build_name,
                                                 bool getIdentProteins)

{
    list<string> linkout_list;

    map<int, vector < CBioseq::TId > >  linkout_map;
    GetBdlLinkoutInfo(cur_id,linkout_map, linkoutdb, mv_build_name);
    SLinkoutInfo linkoutInfo;
    linkoutInfo.Init(rid,
                     cdd_rid,
                     entrez_term,
                     is_na,
                     database,
                     query_number,
                     user_url,
                     preComputedResID,
                     linkoutOrder,
                     structure_linkout_as_group,
                     for_alignment);

    linkoutInfo.cur_align = cur_align;
    linkoutInfo.taxid = taxid;

    linkout_list = s_GetFullLinkoutUrl(cur_id,
                                       linkoutInfo,
                                       linkout_map,
                                       getIdentProteins);
    return linkout_list;
}


static bool FromRangeAscendingSort(CRange<TSeqPos> const& info1,
                                   CRange<TSeqPos> const& info2)
{
    return info1.GetFrom() < info2.GetFrom();
}

//0 for query, 1 for subject
//Gets query and subject range lists,oppositeStrands param
static bool s_ProcessAlignSet(const CSeq_align_set& alnset,
                              list<CRange<TSeqPos> > &query_list,
                              list<CRange<TSeqPos> > &subject_list)
{
    bool oppositeStrands = false;
    bool isFirst = false;
    ITERATE(CSeq_align_set::Tdata, iter, alnset.Get()) {
        CRange<TSeqPos> query_range = (*iter)->GetSeqRange(0);
        //for minus strand
        if(query_range.GetFrom() > query_range.GetTo()){
            query_range.Set(query_range.GetTo(), query_range.GetFrom());
        }
        query_list.push_back(query_range);

        CRange<TSeqPos> subject_range = (*iter)->GetSeqRange(1);
        //for minus strand
        if(subject_range.GetFrom() > subject_range.GetTo()){
            subject_range.Set(subject_range.GetTo(), subject_range.GetFrom());
        }
        subject_list.push_back(subject_range);

        oppositeStrands = (!isFirst) ? (*iter)->GetSeqStrand(0) != (*iter)->GetSeqStrand(1) : oppositeStrands;
        isFirst = true;
    }

    query_list.sort(FromRangeAscendingSort);
    subject_list.sort(FromRangeAscendingSort);
    return oppositeStrands;
}



//0 for query, 1 for subject
static list<CRange<TSeqPos> > s_MergeRangeList(list<CRange<TSeqPos> > &source)
{

    list<CRange<TSeqPos> > merge_list;

    bool is_first = true;
    CRange<TSeqPos> prev_range (0, 0);
    ITERATE(list<CRange<TSeqPos> >, iter, source) {

        if (is_first) {
            merge_list.push_back(*iter);
            is_first= false;
            prev_range = *iter;
        } else {
            if (prev_range.IntersectingWith(*iter)) {
                merge_list.pop_back();
                CRange<TSeqPos> temp_range = prev_range.CombinationWith(*iter);
                merge_list.push_back(temp_range);
                prev_range = temp_range;
            } else {
                merge_list.push_back(*iter);
                prev_range = *iter;
            }
        }

    }
    return merge_list;
}

int CAlignFormatUtil::GetMasterCoverage(const CSeq_align_set& alnset)
{

    list<CRange<TSeqPos> > merge_list;

    list<CRange<TSeqPos> > temp;
    ITERATE(CSeq_align_set::Tdata, iter, alnset.Get()) {
        CRange<TSeqPos> seq_range = (*iter)->GetSeqRange(0);
        //for minus strand
        if(seq_range.GetFrom() > seq_range.GetTo()){
            seq_range.Set(seq_range.GetTo(), seq_range.GetFrom());
        }
        temp.push_back(seq_range);
    }

    temp.sort(FromRangeAscendingSort);

    merge_list = s_MergeRangeList(temp);

    int master_covered_lenghth = 0;
    ITERATE(list<CRange<TSeqPos> >, iter, merge_list) {
        master_covered_lenghth += iter->GetLength();
    }
    return master_covered_lenghth;
}



CRange<TSeqPos> CAlignFormatUtil::GetSeqAlignCoverageParams(const CSeq_align_set& alnset,int *master_covered_lenghth,bool *flip)

{

    list<CRange<TSeqPos> > query_list;
    list<CRange<TSeqPos> > subject_list;

    *flip = s_ProcessAlignSet(alnset,query_list,subject_list);
    query_list = s_MergeRangeList(query_list);
    subject_list = s_MergeRangeList(subject_list);


    *master_covered_lenghth = 0;
    ITERATE(list<CRange<TSeqPos> >, iter, query_list) {
        *master_covered_lenghth += iter->GetLength();
    }

    TSeqPos from = 0,to = 0;
    ITERATE(list<CRange<TSeqPos> >, iter, subject_list) {
        from = (from == 0) ? iter->GetFrom() : min(from,iter->GetFrom());
        to = max(to,iter->GetTo());
    }
    //cerr << "from,to = " << from << "," << to << endl;
    CRange<TSeqPos> subjectRange(from + 1, to + 1);
    return subjectRange;
}

CRef<CSeq_align_set>
CAlignFormatUtil::SortSeqalignForSortableFormat(CCgiContext& ctx,
                                             CScope& scope,
                                             CSeq_align_set& aln_set,
                                             bool nuc_to_nuc_translation,
                                             int db_sort,
                                             int hit_sort,
                                             int hsp_sort,
                                             ILinkoutDB* linkoutdb,
                                             const string& mv_build_name) {


    if (db_sort == 0 && hit_sort < 1 && hsp_sort < 1)
       return (CRef<CSeq_align_set>) &aln_set;

    list< CRef<CSeq_align_set> > seqalign_hit_total_list;
    vector< CRef<CSeq_align_set> > seqalign_vec(2);
    seqalign_vec[0] = new CSeq_align_set;
    seqalign_vec[1] = new CSeq_align_set;

    if(IsMixedDatabase(ctx)) {
        SplitSeqalignByMolecularType(seqalign_vec, db_sort, aln_set, scope,
                                     linkoutdb, mv_build_name);
    }else {
        seqalign_vec[0] = const_cast<CSeq_align_set*>(&aln_set);
    }


    ITERATE(vector< CRef<CSeq_align_set> >, iter, seqalign_vec){
        list< CRef<CSeq_align_set> > one_seqalign_hit_total_list = SortOneSeqalignForSortableFormat(**iter,
                                                            nuc_to_nuc_translation,
                                                            hit_sort,
                                                            hsp_sort);

        seqalign_hit_total_list.splice(seqalign_hit_total_list.end(),one_seqalign_hit_total_list);

    }

    return HitListToHspList(seqalign_hit_total_list);
}
list< CRef<CSeq_align_set> >
CAlignFormatUtil::SortOneSeqalignForSortableFormat(const CSeq_align_set& source,
                                                bool nuc_to_nuc_translation,
                                                int hit_sort,
                                                int hsp_sort)
{
    list< CRef<CSeq_align_set> > seqalign_hit_total_list;
    list< CRef<CSeq_align_set> > seqalign_hit_list;
    HspListToHitList(seqalign_hit_list, source);

    if (hit_sort == eTotalScore) {
        seqalign_hit_list.sort(SortHitByTotalScoreDescending);
    } else if (hit_sort == eHighestScore) {
        seqalign_hit_list.sort(CAlignFormatUtil::SortHitByScoreDescending);
    } else if (hit_sort == ePercentIdentity) {
        SortHitByPercentIdentityDescending(seqalign_hit_list,
                                               nuc_to_nuc_translation);
    } else if (hit_sort == eQueryCoverage) {
        seqalign_hit_list.sort(SortHitByMasterCoverageDescending);
    }

    ITERATE(list< CRef<CSeq_align_set> >, iter2, seqalign_hit_list) {
        CRef<CSeq_align_set> temp(*iter2);
        if (hsp_sort == eQueryStart) {
            temp->Set().sort(SortHspByMasterStartAscending);
        } else if (hsp_sort == eHspPercentIdentity) {
            temp->Set().sort(SortHspByPercentIdentityDescending);
        } else if (hsp_sort == eScore) {
            temp->Set().sort(SortHspByScoreDescending);
        } else if (hsp_sort == eSubjectStart) {
            temp->Set().sort(SortHspBySubjectStartAscending);

        }
        seqalign_hit_total_list.push_back(temp);
    }
    return seqalign_hit_total_list;
}

CRef<CSeq_align_set>
CAlignFormatUtil::SortSeqalignForSortableFormat(CSeq_align_set& aln_set,
                                             bool nuc_to_nuc_translation,
                                             int hit_sort,
                                             int hsp_sort) {

    if (hit_sort <= eEvalue && hsp_sort <= eHspEvalue) {
       return (CRef<CSeq_align_set>) &aln_set;
    }

//  seqalign_vec[0] = const_cast<CSeq_align_set*>(&aln_set);
    list< CRef<CSeq_align_set> > seqalign_hit_total_list = SortOneSeqalignForSortableFormat(aln_set,
                                                            nuc_to_nuc_translation,
                                                            hit_sort,
                                                            hsp_sort);
    return HitListToHspList(seqalign_hit_total_list);
}


CRef<CSeq_align_set> CAlignFormatUtil::FilterSeqalignByEval(CSeq_align_set& source_aln,
                                     double evalueLow,
                                     double evalueHigh)
{
    int score, sum_n, num_ident;
    double bits, evalue;
    list<TGi> use_this_gi;

    CRef<CSeq_align_set> new_aln(new CSeq_align_set);

    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){
        CAlignFormatUtil::GetAlnScores(**iter, score, bits, evalue,
                                       sum_n, num_ident, use_this_gi);
        //Add the next three lines to re-calculte seq align evalue to the obe that is displayed on the screen
		//string evalue_buf, bit_score_buf, total_bit_buf, raw_score_buf;
		//CAlignFormatUtil::GetScoreString(evalue, bits, 0, 0, evalue_buf, bit_score_buf, total_bit_buf, raw_score_buf);
		//evalue = NStr::StringToDouble(evalue_buf);
        if(evalue >= evalueLow && evalue <= evalueHigh) {
            new_aln->Set().push_back(*iter);
        }
    }
    return new_aln;

}

/// Returns percent match for an alignment.
/// Normally we round up the value, unless that means that an
/// alignment with mismatches would be 100%.  In that case
/// it becomes 99%.
///@param numerator: numerator in percent identity calculation.
///@param denominator: denominator in percent identity calculation.
int CAlignFormatUtil::GetPercentMatch(int numerator, int denominator)
{
     if (numerator == denominator)
        return 100;
     else {
       int retval =(int) (0.5 + 100.0*((double)numerator)/((double)denominator));
       retval = min(99, retval);
       return retval;
     }
}

double CAlignFormatUtil::GetPercentIdentity(int numerator, int denominator)
{
     if (numerator == denominator)
        return 100;
     else {
       double retval =100*(double)numerator/(double)denominator;
       return retval;
     }
}

CRef<CSeq_align_set> CAlignFormatUtil::FilterSeqalignByPercentIdent(CSeq_align_set& source_aln,
                                                                    double percentIdentLow,
                                                                    double percentIdentHigh)
{
    int score, sum_n, num_ident;
    double bits, evalue;
    list<TGi> use_this_gi;

    CRef<CSeq_align_set> new_aln(new CSeq_align_set);

    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){
        CAlignFormatUtil::GetAlnScores(**iter, score, bits, evalue,
                                        sum_n, num_ident, use_this_gi);
        int seqAlnLength = GetAlignmentLength(**iter, kTranslation);
        if(seqAlnLength > 0 && num_ident > 0) {
            double alnPercentIdent = GetPercentIdentity(num_ident, seqAlnLength);
            if(alnPercentIdent >= percentIdentLow && alnPercentIdent <= percentIdentHigh) {
                new_aln->Set().push_back(*iter);
            }
        }
    }
    return new_aln;
}

CRef<CSeq_align_set> CAlignFormatUtil::FilterSeqalignByScoreParams(CSeq_align_set& source_aln,
                                                                    double evalueLow,
                                                                    double evalueHigh,
                                                                    double percentIdentLow,
                                                                    double percentIdentHigh)
{
    int score, sum_n, num_ident;
    double bits, evalue;
    list<TGi> use_this_gi;

    CRef<CSeq_align_set> new_aln(new CSeq_align_set);

    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){
        CAlignFormatUtil::GetAlnScores(**iter, score, bits, evalue,
                                       sum_n, num_ident, use_this_gi);
        //Add the next three lines to re-calculte seq align evalue to the one that is displayed on the screen
		//string evalue_buf, bit_score_buf, total_bit_buf, raw_score_buf;
		//CAlignFormatUtil::GetScoreString(evalue, bits, 0, 0, evalue_buf, bit_score_buf, total_bit_buf, raw_score_buf);
		//evalue = NStr::StringToDouble(evalue_buf);
		int seqAlnLength = GetAlignmentLength(**iter, kTranslation);
		if(seqAlnLength > 0 && num_ident > 0) {
			int alnPercentIdent = GetPercentMatch(num_ident, seqAlnLength);
			if( (evalue >= evalueLow && evalue <= evalueHigh) &&
				(alnPercentIdent >= percentIdentLow && alnPercentIdent <= percentIdentHigh)) {
				new_aln->Set().push_back(*iter);
			}
        }
    }
    return new_aln;
}

static double adjustPercentIdentToDisplayValue(double value)
{
    char buffer[512];
    sprintf(buffer, "%.*f", 2, value);
    double newVal = NStr::StringToDouble(buffer);
    return newVal;
}

static bool s_isAlnInFilteringRange(double evalue,
                                  double percentIdent,
                                  int queryCover,
                                  double evalueLow,
                                  double evalueHigh,
                                  double percentIdentLow,
                                  double percentIdentHigh,
                                  int queryCoverLow,
                                  int queryCoverHigh)
{


    bool isInRange = false;
    //Adjust percent identity and evalue to display values
    percentIdent = adjustPercentIdentToDisplayValue(percentIdent);
    string evalue_buf, bit_score_buf, total_bit_buf, raw_score_buf;
    double bits = 0;
	CAlignFormatUtil::GetScoreString(evalue, bits, 0, 0, evalue_buf, bit_score_buf, total_bit_buf, raw_score_buf);
	evalue = NStr::StringToDouble(evalue_buf);

    if(evalueLow >= 0 && percentIdentLow >= 0 && queryCoverLow >= 0) {
        isInRange = (evalue >= evalueLow && evalue <= evalueHigh) &&
				    (percentIdent >= percentIdentLow && percentIdent <= percentIdentHigh) &&
                    (queryCover >= queryCoverLow && queryCover <= queryCoverHigh);
    }
    else if(evalueLow >= 0 && percentIdentLow >= 0) {
        isInRange = (evalue >= evalueLow && evalue <= evalueHigh) &&
				(percentIdent >= percentIdentLow && percentIdent <= percentIdentHigh);
	}
    else if(evalueLow >= 0 && queryCoverLow >= 0) {
        isInRange = (evalue >= evalueLow && evalue <= evalueHigh) &&
				(queryCover >= queryCoverLow && queryCover <= queryCoverHigh);
	}
    else if(queryCoverLow >= 0 && 	percentIdentLow >= 0) {
        isInRange = (queryCover >= queryCoverLow && queryCover <= queryCoverHigh) &&
				(percentIdent >= percentIdentLow && percentIdent <= percentIdentHigh);
	}
    else if(evalueLow >= 0) {
        isInRange = (evalue >= evalueLow && evalue <= evalueHigh);
    }
	else if(percentIdentLow >= 0) {
        isInRange = (percentIdent >= percentIdentLow && percentIdent <= percentIdentHigh);
	}
    else if(queryCoverLow >= 0) {
        isInRange = (queryCover >= queryCoverLow && queryCover <= queryCoverHigh);
	}
    return isInRange;
}

CRef<CSeq_align_set> CAlignFormatUtil::FilterSeqalignByScoreParams(CSeq_align_set& source_aln,
                                                                    double evalueLow,
                                                                    double evalueHigh,
                                                                    double percentIdentLow,
                                                                    double percentIdentHigh,
                                                                    int queryCoverLow,
                                                                    int queryCoverHigh)
{
    list< CRef<CSeq_align_set> > seqalign_hit_total_list;
    list< CRef<CSeq_align_set> > seqalign_hit_list;

    HspListToHitList(seqalign_hit_list, source_aln);

    ITERATE(list< CRef<CSeq_align_set> >, iter, seqalign_hit_list) {
        CRef<CSeq_align_set> temp(*iter);
        CAlignFormatUtil::SSeqAlignSetCalcParams* seqSetInfo = CAlignFormatUtil::GetSeqAlignSetCalcParamsFromASN(*temp);

        if(s_isAlnInFilteringRange(seqSetInfo->evalue,
                                  seqSetInfo->percent_identity,
                                  seqSetInfo->percent_coverage,
                                  evalueLow,
                                  evalueHigh,
                                  percentIdentLow,
                                  percentIdentHigh,
                                  queryCoverLow,
                                  queryCoverHigh)) {
            seqalign_hit_total_list.push_back(temp);
        }
    }
    return HitListToHspList(seqalign_hit_total_list);
}

CRef<CSeq_align_set> CAlignFormatUtil::LimitSeqalignByHsps(CSeq_align_set& source_aln,
                                                           int maxAligns,
                                                           int maxHsps)
{
    CRef<CSeq_align_set> new_aln(new CSeq_align_set);

    CConstRef<CSeq_id> prevQueryId,prevSubjectId;
    int alignCount = 0,hspCount = 0;
    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){
        const CSeq_id& newQueryId = (*iter)->GetSeq_id(0);
        if(prevQueryId.Empty() || !newQueryId.Match(*prevQueryId)){
            if (hspCount >= maxHsps) {
                break;
            }
            alignCount = 0;
            prevQueryId = &newQueryId;
        }
        if (alignCount < maxAligns) {
            const CSeq_id& newSubjectId = (*iter)->GetSeq_id(1);
            // Increment alignments count if subject sequence is different
            if(prevSubjectId.Empty() || !newSubjectId.Match(*prevSubjectId)){
                ++alignCount;
                prevSubjectId = &newSubjectId;
            }
            // Increment HSP count if the alignments limit is not reached
            ++hspCount;
            new_aln->Set().push_back(*iter);
        }

    }
    return new_aln;
}


CRef<CSeq_align_set> CAlignFormatUtil::ExtractQuerySeqAlign(CRef<CSeq_align_set> &source_aln,
                                                            int queryNumber)
{
    if(queryNumber == 0) {
        return source_aln;
    }
    CRef<CSeq_align_set> new_aln;

    CConstRef<CSeq_id> prevQueryId;
    int currQueryNum = 0;

    ITERATE(CSeq_align_set::Tdata, iter, source_aln->Get()){
        const CSeq_id& newQueryId = (*iter)->GetSeq_id(0);
        if(prevQueryId.Empty() || !newQueryId.Match(*prevQueryId)){
            currQueryNum++;
            prevQueryId = &newQueryId;
        }
        //Record seq aligns corresponding to queryNumber
        if(currQueryNum == queryNumber) {
            if(new_aln.Empty()) {
                new_aln.Reset(new CSeq_align_set);
            }
            new_aln->Set().push_back(*iter);
        }
        else if(currQueryNum > queryNumber) {
            break;
        }
    }
    return new_aln;
}


void CAlignFormatUtil::InitConfig()
{
    string l_cfg_file_name;
    bool   l_dbg = CAlignFormatUtil::m_geturl_debug_flag;
    if( getenv("GETURL_DEBUG") ) CAlignFormatUtil::m_geturl_debug_flag = l_dbg = true;
    if( !m_Reg ) {
        bool cfgExists = true;
        string l_ncbi_env;
        string l_fmtcfg_env;
        if( NULL !=  getenv("NCBI")   ) l_ncbi_env = getenv("NCBI");
        if( NULL !=  getenv("FMTCFG") ) l_fmtcfg_env = getenv("FMTCFG");
        // config file name: value of FMTCFG or  default ( .ncbirc )
        if( l_fmtcfg_env.empty()  )
            l_cfg_file_name = ".ncbirc";
        else
            l_cfg_file_name = l_fmtcfg_env;
        // checkinf existance of configuration file
        CFile  l_fchecker( l_cfg_file_name );
        cfgExists = l_fchecker.Exists();
        if( (!cfgExists) && (!l_ncbi_env.empty()) ) {
            if( l_ncbi_env.rfind("/") != (l_ncbi_env.length() -1 ))
            l_ncbi_env.append("/");
            l_cfg_file_name = l_ncbi_env + l_cfg_file_name;
            CFile  l_fchecker2( l_cfg_file_name );
            cfgExists = l_fchecker2.Exists();
        }
        if(cfgExists) {
            CNcbiIfstream l_ConfigFile(l_cfg_file_name.c_str() );
            m_Reg.reset(new CNcbiRegistry(l_ConfigFile));
            if( l_dbg ) fprintf(stderr,"REGISTRY: %s\n",l_cfg_file_name.c_str());
        }
    }
    return;
}

//
// get given url from registry file or return corresponding kNAME
// value as default to preserve compatibility.
//
// algoritm:
// 1) config file name is ".ncbirc" unless FMTCFG specifies another name
// 2) try to read local configuration file before
//    checking location specified by the NCBI environment.
// 3) if index != -1, use it as trailing version number for a key name,
//    ABCD_V0. try to read ABCD key if version variant doesn't exist.
// 4) use INCLUDE_BASE_DIR key to specify base for all include files.
// 5) treat "_FORMAT" key as filename first and  string in second.
//    in case of existances of filename, read it starting from
//    location specified by INCLUDE_BASE_DIR key
string CAlignFormatUtil::GetURLFromRegistry( const string url_name, int index){
  string  result_url;
  string l_key, l_host_port, l_format;
  string l_secion_name = "BLASTFMTUTIL";
  string l_fmt_suffix = "_FORMAT";
  string l_host_port_suffix = "_HOST_PORT";
  string l_subst_pattern;

  if( !m_Reg ) {
    InitConfig();
  }
  if( !m_Reg ) return GetURLDefault(url_name,index); // can't read .ncbrc file
  string l_base_dir = m_Reg->Get(l_secion_name, "INCLUDE_BASE_DIR");
  if( !l_base_dir.empty() && ( l_base_dir.rfind("/") != (l_base_dir.length()-1)) ) {
    l_base_dir.append("/");
  }


  string default_host_port;
  string l_key_ndx;
  if( index >=0) {
    l_key_ndx = url_name + l_host_port_suffix + "_" + NStr::IntToString( index );
    l_subst_pattern="<@"+l_key_ndx+"@>";
    l_host_port = m_Reg->Get(l_secion_name, l_key_ndx); // try indexed
  }
  // next is initialization for non version/array type of settings
  if( l_host_port.empty()){  // not indexed or index wasn't found
    l_key = url_name + l_host_port_suffix; l_subst_pattern="<@"+l_key+"@>";
    l_host_port = m_Reg->Get(l_secion_name, l_key);
  }
  if( l_host_port.empty())   return GetURLDefault(url_name,index);

  // get format part
  l_key = url_name + l_fmt_suffix ; //"_FORMAT";
  l_key_ndx = l_key + "_" + NStr::IntToString( index );
  if( index >= 0 ){
    l_format = m_Reg->Get(l_secion_name, l_key_ndx);
  }

  if( l_format.empty() ) l_format = m_Reg->Get(l_secion_name, l_key);
  if( l_format.empty())   return GetURLDefault(url_name,index);
  // format found check wether this string or file name
  string l_format_file  = l_base_dir + l_format;
  CFile  l_fchecker( l_format_file );
  bool file_name_mode = l_fchecker.Exists();
  if( file_name_mode ) { // read whole content of the file to string buffer
    string l_inc_file_name = l_format_file;
    CNcbiIfstream l_file (l_inc_file_name.c_str(), ios::in|ios::binary|ios::ate);
    CT_POS_TYPE l_inc_size = l_file.tellg();
    //    size_t l_buf_sz = (size_t) l_inc_size;
    char *l_mem = new char [ (size_t) l_inc_size + 1];
    memset( l_mem,0, (size_t) l_inc_size + 1 ) ;
    l_file.seekg( 0, ios::beg );
    l_file.read(l_mem, l_inc_size);
    l_file.close();
    l_format.erase(); l_format.reserve( (size_t)l_inc_size + 1 );
    l_format =  l_mem;
    delete [] l_mem;
  }

  result_url = NStr::Replace(l_format,l_subst_pattern,l_host_port);

  if( result_url.empty()) return GetURLDefault(url_name,index);
  return result_url;
}
//
// return default URL value for the given key.
//
string  CAlignFormatUtil::GetURLDefault( const string url_name, int index) {

  string search_name = url_name;
  TTagUrlMap::const_iterator url_it;
  if( index >= 0 ) search_name += "_" + NStr::IntToString( index); // actual name for index value is NAME_{index}
  
  auto cit = mapURLTagToAddress.find(search_name);
  
  if( cit != mapURLTagToAddress.end()) {
      string url_link = cit->second;
      return url_link;
  }
  
  if( (url_it = sm_TagUrlMap.find( search_name ) ) != sm_TagUrlMap.end()) {
      string url_link = CAlignFormatUtil::MapProtocol(url_it->second);
      return url_link;
  }
  
  
  string error_msg = "CAlignFormatUtil::GetURLDefault:no_defualt_for"+url_name;
  if( index != -1 ) error_msg += "_index_"+ NStr::IntToString( index );
  return error_msg;
}

void
CAlignFormatUtil::GetAsciiProteinMatrix(const char* matrix_name,
                                        CNcbiMatrix<int>& retval)
{
    retval.Resize(0, 0, -1);
    if (matrix_name == NULL ||
        NStr::TruncateSpaces(string(matrix_name)).empty()) {
        return;
    }

    const SNCBIPackedScoreMatrix* packed_mtx =
        NCBISM_GetStandardMatrix(matrix_name);
    if (packed_mtx == NULL) {
        return;
    }
    retval.Resize(k_NumAsciiChar, k_NumAsciiChar, -1000);

    SNCBIFullScoreMatrix mtx;
    NCBISM_Unpack(packed_mtx, &mtx);

    for(int i = 0; i < ePMatrixSize; ++i){
        for(int j = 0; j < ePMatrixSize; ++j){
            retval((size_t)k_PSymbol[i], (size_t)k_PSymbol[j]) =
                mtx.s[(size_t)k_PSymbol[i]][(size_t)k_PSymbol[j]];
        }
    }
    for(int i = 0; i < ePMatrixSize; ++i) {
        retval((size_t)k_PSymbol[i], '*') = retval('*',(size_t)k_PSymbol[i]) = -4;
    }
    retval('*', '*') = 1;
    // this is to count Selenocysteine to Cysteine matches as positive
    retval('U', 'U') = retval('C', 'C');
    retval('U', 'C') = retval('C', 'C');
    retval('C', 'U') = retval('C', 'C');
}


string CAlignFormatUtil::MapTemplate(string inpString,string tmplParamName,Int8 templParamVal)
{
    string outString;
    string tmplParam = "<@" + tmplParamName + "@>";
    NStr::Replace(inpString,tmplParam,NStr::NumericToString(templParamVal),outString);
    return outString;
}

string CAlignFormatUtil::MapTemplate(string inpString,string tmplParamName,string templParamVal)
{
    string outString;
    string tmplParam = "<@" + tmplParamName + "@>";
    NStr::Replace(inpString,tmplParam,templParamVal,outString);
    return outString;
}

string CAlignFormatUtil::MapSpaceTemplate(string inpString,string tmplParamName,string templParamVal, unsigned int maxParamValLength, int spacesFormatFlag)
{
    templParamVal = AddSpaces(templParamVal, maxParamValLength, spacesFormatFlag);
    string outString = MapTemplate(inpString,tmplParamName,templParamVal);

    return outString;
}


string CAlignFormatUtil::AddSpaces(string paramVal, size_t maxParamValLength, int spacesFormatFlag)
{
    //if(!spacePos.empty()) {
        string spaceString;
        if(maxParamValLength >= paramVal.size()) {
            size_t numSpaces = maxParamValLength - paramVal.size() + 1;
            if(spacesFormatFlag & eSpacePosToCenter) {
                numSpaces = numSpaces/2;
            }
	    spaceString.assign(numSpaces,' ');
        }
        else {
            paramVal = paramVal.substr(0, maxParamValLength - 3) + "...";
            spaceString += " ";
        }
        if(spacesFormatFlag & eSpacePosAtLineEnd) {
            paramVal = paramVal + spaceString;
        }
        else if(spacesFormatFlag & eSpacePosToCenter) {
            paramVal = spaceString + paramVal + spaceString;
        }
        else {
            paramVal = spaceString + paramVal;
        }
        if(spacesFormatFlag & eAddEOLAtLineStart) paramVal = "\n" + paramVal;
        if(spacesFormatFlag & eAddEOLAtLineEnd) paramVal = paramVal + "\n";
    //}

    return paramVal;
}



string CAlignFormatUtil::GetProtocol()
{
    CNcbiIfstream  config_file(".ncbirc");
    CNcbiRegistry config_reg(config_file);
    string httpProt = "https:";
    if(!config_reg.Empty()) {
        if(config_reg.HasEntry("BLASTFMTUTIL","PROTOCOL")) {
            httpProt = config_reg.Get("BLASTFMTUTIL","PROTOCOL");
        }
    }
    return httpProt;
}

/*
if(no config file) protocol = "https:"
if(no "BLASTFMTUTIL","PROTOCOL" entry in config file) protocol = "https:"
if(there is entry in config) protocol = entry which could be blank = ""
*/
string CAlignFormatUtil::MapProtocol(string url_link)
{
    if(m_Protocol.empty()){
        if(!m_Reg) {
            InitConfig();
        }
        m_Protocol = (m_Reg && m_Reg->HasEntry("BLASTFMTUTIL","PROTOCOL")) ? m_Protocol = m_Reg->Get("BLASTFMTUTIL","PROTOCOL") : "https:";
    }
    url_link = CAlignFormatUtil::MapTemplate(url_link,"protocol",m_Protocol);
    return url_link;
}

static string s_MapCommonUrlParams(string urlTemplate, CAlignFormatUtil::SSeqURLInfo *seqUrlInfo)
{
    string db,logstr_moltype;
    if(seqUrlInfo->isDbNa) {
        db = "nucleotide";
        logstr_moltype = "nucl";
    } else {
        db = "protein";
        logstr_moltype ="prot";
    }
    string logstr_location = (seqUrlInfo->isAlignLink) ? "align" : "top";
    string url_link = CAlignFormatUtil::MapTemplate(urlTemplate,"db",db);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"gi", GI_TO(TIntId, seqUrlInfo->gi));
    url_link = CAlignFormatUtil::MapTemplate(url_link,"log",logstr_moltype + logstr_location);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"blast_rank",seqUrlInfo->blast_rank);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"rid",seqUrlInfo->rid);
    url_link = CAlignFormatUtil::MapTemplate(url_link,"acc",seqUrlInfo->accession);
    url_link = CAlignFormatUtil::MapProtocol(url_link);
    return url_link;
}

static string s_MapURLLink(string urlTemplate, CAlignFormatUtil::SSeqURLInfo *seqUrlInfo, const CBioseq::TId& ids)
{
    //Add specific blasttype/user_url template mapping here
    string url_link = urlTemplate;
    if (seqUrlInfo->user_url.find("sra.cgi") != string::npos) {
        string strRun, strSpotId,strReadIndex;
        if(s_GetSRASeqMetadata(ids,strRun,strSpotId,strReadIndex)) {
            url_link = CAlignFormatUtil::MapTemplate(url_link,"run",strRun);
            url_link = CAlignFormatUtil::MapTemplate(url_link,"spotid",strSpotId);
            url_link = CAlignFormatUtil::MapTemplate(url_link,"readindex",strReadIndex);
        }
    }
    //This maps generic params like log, blast_rank, rid
    url_link = s_MapCommonUrlParams(url_link, seqUrlInfo);
    return url_link;
}



bool CAlignFormatUtil::IsWGSPattern(string &wgsAccession)
{
	//const string  kWgsAccessionPattern = "^[A-Z]{4}[0-9]{8,10}(\.[0-9]+){0,1}$"; //example AUXO013124042 or AUXO013124042.1
    const unsigned int kWgsProjLength = 4;
    const unsigned int kWgsProjIDLengthMin = 8;
    const unsigned int kWgsProjIDLengthMax = 10;
    bool isWGS = true;

    if (wgsAccession.size() < 6) {
        return false;
    }

    if(NStr::Find(wgsAccession, ".") != NPOS) { //Accession has version AUXO013124042.1
	    string version;
		NStr::SplitInTwo(wgsAccession,".",wgsAccession,version);
	}

    string wgsProj = wgsAccession.substr(0,kWgsProjLength);
    for (size_t i = 0; i < wgsProj.length(); i ++){
        if(!isalpha(wgsProj[i]&0xff)) {
            isWGS = false;
            break;
        }
    }
    if(isWGS) {
        string wgsId = wgsAccession.substr(kWgsProjLength);
        if(wgsId.length() >= kWgsProjIDLengthMin && wgsId.length() <= kWgsProjIDLengthMax) {
            for (size_t i = 0; i < wgsId.length(); i ++){
                if(!isdigit(wgsId[i]&0xff)) {
                    isWGS = false;
                    break;
                }
            }
        }
        else {
            isWGS = false;
        }
    }
    return isWGS;
}


bool CAlignFormatUtil::IsWGSAccession(string &wgsAccession, string &wgsProjName)
{
	const unsigned int  kWgsProgNameLength = 6;
	bool isWGS = IsWGSPattern(wgsAccession);
	if(isWGS) {
		wgsProjName = wgsAccession.substr(0,kWgsProgNameLength);
	}
	return isWGS;
}


string CAlignFormatUtil::GetIDUrlGen(SSeqURLInfo *seqUrlInfo,const CBioseq::TId* ids)
{
    string url_link = NcbiEmptyString;
    CConstRef<CSeq_id> wid = FindBestChoice(*ids, CSeq_id::WorstRank);

    bool hasTextSeqID = GetTextSeqID(*ids);
    string title = "title=\"Show report for " + seqUrlInfo->accession + "\" ";

    string temp_class_info = kClassInfo; temp_class_info += " ";
	string wgsProj;
	string wgsAccession = seqUrlInfo->accession;
	bool isWGS = false;
        if (!(wid->Which() == CSeq_id::e_Local || wid->Which() == CSeq_id::e_General)){
            isWGS = CAlignFormatUtil::IsWGSAccession(wgsAccession, wgsProj);
        }
	if(isWGS && seqUrlInfo->useTemplates) {
		string wgsUrl = CAlignFormatUtil::GetURLFromRegistry("WGS");
		url_link = s_MapCommonUrlParams(wgsUrl, seqUrlInfo);
		url_link = CAlignFormatUtil::MapTemplate(url_link,"wgsproj",wgsProj);
		url_link = CAlignFormatUtil::MapTemplate(url_link,"wgsacc", wgsAccession);
	}
    else if (hasTextSeqID) {
        string entrezTag = (seqUrlInfo->useTemplates) ? "ENTREZ_TM" : "ENTREZ";
        string l_EntrezUrl = CAlignFormatUtil::GetURLFromRegistry(entrezTag);
        url_link = s_MapCommonUrlParams(l_EntrezUrl, seqUrlInfo);

        if(!seqUrlInfo->useTemplates) {
			url_link = CAlignFormatUtil::MapTemplate(url_link,"acc",seqUrlInfo->accession);
            temp_class_info = (!seqUrlInfo->defline.empty())? CAlignFormatUtil::MapTemplate(temp_class_info,"defline",NStr::JavaScriptEncode(seqUrlInfo->defline)):temp_class_info;
            url_link = CAlignFormatUtil::MapTemplate(url_link,"cssInf",(seqUrlInfo->addCssInfo) ? temp_class_info.c_str() : "");
            url_link = CAlignFormatUtil::MapTemplate(url_link,"target",seqUrlInfo->new_win ? "TARGET=\"EntrezView\"" : "");
        }

    } else {//seqid general, dbtag specified
        if(wid->Which() == CSeq_id::e_General){
            const CDbtag& dtg = wid->GetGeneral();
            const string& dbname = dtg.GetDb();
            if(NStr::CompareNocase(dbname, "TI") == 0){
                string actual_id = CAlignFormatUtil::GetGnlID(dtg);
                if(seqUrlInfo->useTemplates) {
                    string l_TraceUrl = CAlignFormatUtil::GetURLFromRegistry("TRACE_CGI");
                    url_link = l_TraceUrl + (string)"?cmd=retrieve&dopt=fasta&val=" + actual_id + "&RID=" + seqUrlInfo->rid;
                }
                else {
                    url_link = CAlignFormatUtil::MapTemplate(kTraceUrl,"val",actual_id);
                    temp_class_info = (!seqUrlInfo->defline.empty())? CAlignFormatUtil::MapTemplate(temp_class_info,"defline",seqUrlInfo->defline):temp_class_info;
                    url_link = CAlignFormatUtil::MapTemplate(url_link,"cssInf",(seqUrlInfo->addCssInfo) ? temp_class_info.c_str() : "");
                    url_link = CAlignFormatUtil::MapTemplate(url_link,"rid",seqUrlInfo->rid);
                }
            }
        } else if (wid->Which() == CSeq_id::e_Local){

            string url_holder = CAlignFormatUtil::GetURLFromRegistry("LOCAL_ID");

            string user_url;
            if (m_Reg) {
                user_url = (seqUrlInfo->addCssInfo) ? m_Reg->Get("LOCAL_ID","TOOL_URL_ALIGN") : m_Reg->Get("LOCAL_ID","TOOL_URL");
            }
            string id_string;
            wid->GetLabel(&id_string, CSeq_id::eContent);
            url_link = CAlignFormatUtil::MapTemplate(user_url,"seq_id", NStr::URLEncode(id_string));
            url_link = CAlignFormatUtil::MapTemplate(url_link,"db_name", NStr::URLEncode(seqUrlInfo->database));
            url_link = CAlignFormatUtil::MapTemplate(url_link,"taxid", TAX_ID_TO(int, seqUrlInfo->taxid));
            temp_class_info = (!seqUrlInfo->defline.empty())? CAlignFormatUtil::MapTemplate(temp_class_info,"defline",seqUrlInfo->defline):temp_class_info;
            url_link = CAlignFormatUtil::MapTemplate(url_link,"cssInf",(seqUrlInfo->addCssInfo) ? temp_class_info.c_str() : "");
            url_link = CAlignFormatUtil::MapTemplate(url_link,"title", id_string);
            url_link = CAlignFormatUtil::MapTemplate(url_link,"target",seqUrlInfo->new_win ? "TARGET=\"EntrezView\"" : "");
        }
    }
    url_link = CAlignFormatUtil::MapProtocol(url_link);
    seqUrlInfo->seqUrl = url_link;
	return url_link;
}

string CAlignFormatUtil::GetIDUrlGen(SSeqURLInfo *seqUrlInfo,const CSeq_id& id,objects::CScope &scope)
{
    const CBioseq_Handle& handle = scope.GetBioseqHandle(id);
    const CBioseq::TId* ids = &handle.GetBioseqCore()->GetId();

    string url_link = GetIDUrlGen(seqUrlInfo,ids);
    return url_link;
}

string CAlignFormatUtil::GetIDUrl(SSeqURLInfo *seqUrlInfo,const CBioseq::TId* ids)
{
    string url_link = NcbiEmptyString;
    CConstRef<CSeq_id> wid = FindBestChoice(*ids, CSeq_id::WorstRank);

    string title = "title=\"Show report for " + seqUrlInfo->accession + "\" ";

    if (seqUrlInfo->user_url != NcbiEmptyString &&
        !((seqUrlInfo->user_url.find("dumpgnl.cgi") != string::npos && seqUrlInfo->gi > ZERO_GI) ||
          (seqUrlInfo->user_url.find("maps.cgi") != string::npos))) {

        string url_with_parameters,toolURLParams;
        if(m_Reg && !seqUrlInfo->blastType.empty() && seqUrlInfo->blastType != "newblast") {
            toolURLParams = m_Reg->Get(seqUrlInfo->blastType, "TOOL_URL_PARAMS");
        }
        if(!toolURLParams.empty()) {
            string urlLinkTemplate = seqUrlInfo->user_url + toolURLParams;
            url_with_parameters = s_MapURLLink(urlLinkTemplate, seqUrlInfo, *ids);
        }
        else {
            if (seqUrlInfo->user_url.find("sra.cgi") != string::npos) {
                url_with_parameters = CAlignFormatUtil::BuildSRAUrl(*ids, seqUrlInfo->user_url);
            }
            else {
                url_with_parameters = CAlignFormatUtil::BuildUserUrl(*ids, seqUrlInfo->taxid, seqUrlInfo->user_url,
                                           seqUrlInfo->database,
                                           seqUrlInfo->isDbNa, seqUrlInfo->rid,
                                           seqUrlInfo->queryNumber,
                                           seqUrlInfo->isAlignLink);
            }
        }
        if (url_with_parameters != NcbiEmptyString) {
            if (!seqUrlInfo->useTemplates) {
                string deflineInfo;
                if(seqUrlInfo->addCssInfo) {
                    deflineInfo = (!seqUrlInfo->defline.empty())? CAlignFormatUtil::MapTemplate(kClassInfo,"defline",seqUrlInfo->defline):kClassInfo;
                }
                url_link += "<a " + title + deflineInfo + "href=\"";
            }
            url_link += url_with_parameters;
            if (!seqUrlInfo->useTemplates) url_link += "\">";
        }
    }
	else {
        //use entrez or dbtag specified
        url_link = GetIDUrlGen(seqUrlInfo,ids);
    }
    seqUrlInfo->seqUrl = url_link;
    return url_link;
}


string CAlignFormatUtil::GetIDUrl(SSeqURLInfo *seqUrlInfo,const CSeq_id& id,objects::CScope &scope)
{
    const CBioseq_Handle& handle = scope.GetBioseqHandle(id);
    const CBioseq::TId* ids = &handle.GetBioseqCore()->GetId();


    seqUrlInfo->blastType = NStr::TruncateSpaces(NStr::ToLower(seqUrlInfo->blastType));

    if(seqUrlInfo->taxid == INVALID_TAX_ID) { //taxid is not set
        seqUrlInfo->taxid = ZERO_TAX_ID;
        if ((seqUrlInfo->advancedView || seqUrlInfo->blastType == "mapview" || seqUrlInfo->blastType == "mapview_prev") ||
            seqUrlInfo->blastType == "gsfasta" || seqUrlInfo->blastType == "gsfasta_prev") {
            seqUrlInfo->taxid = GetTaxidForSeqid(id, scope);
        }
    }
	string url_link = GetIDUrl(seqUrlInfo,ids);
    return url_link;
}

//static const char kGenericLinkTemplate[] = "<a href=\"<@url@>\" target=\"lnk<@rid@>\" title=\"Show report for <@seqid@>\"><@gi@><@seqid@></a>";
string CAlignFormatUtil::GetFullIDLink(SSeqURLInfo *seqUrlInfo,const CBioseq::TId* ids)
{
    string seqLink;
    string linkURL = GetIDUrl(seqUrlInfo,ids);
    if(!linkURL.empty()) {
        string linkTmpl = (seqUrlInfo->addCssInfo) ? kGenericLinkMouseoverTmpl : kGenericLinkTemplate;
        seqLink = CAlignFormatUtil::MapTemplate(linkTmpl,"url",linkURL);
        seqLink = CAlignFormatUtil::MapTemplate(seqLink,"rid",seqUrlInfo->rid);
        seqLink = CAlignFormatUtil::MapTemplate(seqLink,"seqid",seqUrlInfo->accession);
        seqLink = CAlignFormatUtil::MapTemplate(seqLink,"gi", GI_TO(TIntId, seqUrlInfo->gi));
        seqLink = CAlignFormatUtil::MapTemplate(seqLink,"target","EntrezView");
        if(seqUrlInfo->addCssInfo) {
            seqLink = CAlignFormatUtil::MapTemplate(seqLink,"defline",NStr::JavaScriptEncode(seqUrlInfo->defline));
        }
    }
    return seqLink;
}

static string s_MapCustomLink(string linkUrl,string reportType,string accession, string linkText, string linktrg, string linkTitle = kCustomLinkTitle,string linkCls = "")
{
    string link = CAlignFormatUtil::MapTemplate(kCustomLinkTemplate,"custom_url",linkUrl);
    link = CAlignFormatUtil::MapProtocol(link);
    link = CAlignFormatUtil::MapTemplate(link,"custom_title",linkTitle);
    link = CAlignFormatUtil::MapTemplate(link,"custom_report_type",reportType);
    link = CAlignFormatUtil::MapTemplate(link,"seqid",accession);
    link = CAlignFormatUtil::MapTemplate(link,"custom_lnk_displ",linkText);
    link = CAlignFormatUtil::MapTemplate(link,"custom_cls",linkCls);
    link = CAlignFormatUtil::MapTemplate(link,"custom_trg",linktrg);
    return link;
}



list<string>  CAlignFormatUtil::GetGiLinksList(SSeqURLInfo *seqUrlInfo,
                                               bool hspRange)
{
    list<string> customLinksList;
    if (seqUrlInfo->hasTextSeqID) {
        //First show links to GenBank and FASTA
        string linkUrl,link,linkTiltle = kCustomLinkTitle;

        linkUrl = seqUrlInfo->seqUrl;
        if(NStr::Find(linkUrl, "report=genbank") == NPOS) { //Geo case        
            string url = CAlignFormatUtil::GetURLFromRegistry("ENTREZ_TM");
            linkUrl = s_MapCommonUrlParams(url, seqUrlInfo);
        }
        string linkText = (seqUrlInfo->isDbNa) ? "GenBank" : "GenPept";
        if(hspRange) {
            linkUrl += "&from=<@fromHSP@>&to=<@toHSP@>";
            linkTiltle = "Aligned region spanning positions <@fromHSP@> to <@toHSP@> on <@seqid@>";
        }
	    link = s_MapCustomLink(linkUrl,"genbank",seqUrlInfo->accession,linkText,"lnk" + seqUrlInfo->rid,linkTiltle);
        customLinksList.push_back(link);
    }
    return customLinksList;
}

string  CAlignFormatUtil::GetGraphiscLink(SSeqURLInfo *seqUrlInfo,
                                               bool hspRange)
{
    //seqviewer
    string dbtype = (seqUrlInfo->isDbNa) ? "nuccore" : "protein";
    string seqViewUrl = (seqUrlInfo->gi > ZERO_GI)?kSeqViewerUrl:kSeqViewerUrlNonGi;

	string linkUrl = CAlignFormatUtil::MapTemplate(seqViewUrl,"rid",seqUrlInfo->rid);

    string seqViewerParams;
    if(m_Reg && !seqUrlInfo->blastType.empty() && seqUrlInfo->blastType != "newblast") {
        seqViewerParams = m_Reg->Get(seqUrlInfo->blastType, "SEQVIEW_PARAMS");
    }
    seqViewerParams = seqViewerParams.empty() ? kSeqViewerParams : seqViewerParams;
    linkUrl = CAlignFormatUtil::MapTemplate(linkUrl,"seqViewerParams",seqViewerParams);

	linkUrl = CAlignFormatUtil::MapTemplate(linkUrl,"dbtype",dbtype);
	linkUrl = CAlignFormatUtil::MapTemplate(linkUrl,"gi", GI_TO(TIntId, seqUrlInfo->gi));
    string linkTitle = "Show alignment to <@seqid@> in <@custom_report_type@>";
    string link_loc;
    if(!hspRange) {
        int addToRange = (int) ((seqUrlInfo->seqRange.GetTo() - seqUrlInfo->seqRange.GetFrom()) * 0.05);//add 5% to each side
		linkUrl = CAlignFormatUtil::MapTemplate(linkUrl,"from",max(0,(int)seqUrlInfo->seqRange.GetFrom() - addToRange));
		linkUrl = CAlignFormatUtil::MapTemplate(linkUrl,"to",seqUrlInfo->seqRange.GetTo() + addToRange);
        link_loc = "fromSubj";
		//linkUrl = CAlignFormatUtil::MapTemplate(linkUrl,"flip",NStr::BoolToString(seqUrlInfo->flip));
    }
    else {
        link_loc = "fromHSP";
        linkTitle += " for <@fromHSP@> to <@toHSP@> range";
    }
    linkUrl = CAlignFormatUtil::MapTemplate(linkUrl,"link_loc",link_loc);

    string title = (seqUrlInfo->isDbNa) ? "Nucleotide Graphics" : "Protein Graphics";

    string link = s_MapCustomLink(linkUrl,title,seqUrlInfo->accession, "Graphics","lnk" + seqUrlInfo->rid,linkTitle,"spr");

    return link;
}

list<string>  CAlignFormatUtil::GetSeqLinksList(SSeqURLInfo *seqUrlInfo,
                                                bool hspRange)
{
    list<string> customLinksList = GetGiLinksList(seqUrlInfo,hspRange);  //ONLY FOR genBank seqUrlInfo->seqUrl has "report=genbank"
    string graphicLink = GetGraphiscLink(seqUrlInfo,hspRange);
    if(!graphicLink.empty()) {
        customLinksList.push_back(graphicLink);
    }
    return customLinksList;
}

int CAlignFormatUtil::SetCustomLinksTypes(SSeqURLInfo *seqUrlInfo, int customLinkTypesInp)
{
    int customLinkTypes = customLinkTypesInp;
    if ( seqUrlInfo->gi > ZERO_GI) {
        customLinkTypes +=eLinkTypeGenLinks;
    }
    //else if(NStr::StartsWith(seqUrlInfo->accession,"ti:")) {//seqUrlInfo->seqUrl has "trace.cgi"
    else if(NStr::Find(seqUrlInfo->seqUrl,"trace.cgi") != NPOS ){
        customLinkTypes +=eLinkTypeTraceLinks;
    }
    else if(seqUrlInfo->blastType == "sra") {//seqUrlInfo->seqUrl has sra.cgi
        customLinkTypes +=eLinkTypeSRALinks;
    }
    else if(seqUrlInfo->blastType == "snp") {//seqUrlInfo->seqUrl has snp_ref.cgi
        customLinkTypes +=eLinkTypeSNPLinks;
    }
    else if(seqUrlInfo->blastType == "gsfasta") {//seqUrlInfo->seqUrl has GSfasta.cgi
        customLinkTypes +=eLinkTypeGSFastaLinks;
    }
    return customLinkTypes;
}


//kCustomLinkTemplate:
//<a href="<@custom_url@>" class="<@custom_cls@>" title="Show <@custom_report_type@> report for <@seqid@>"><@custom_lnk_displ@></a>
list<string>  CAlignFormatUtil::GetCustomLinksList(SSeqURLInfo *seqUrlInfo,
                                          const CSeq_id& id,
                                          objects::CScope &scope,
                                          int customLinkTypes)
{
    list<string> customLinksList;
    string linkUrl,link;

    customLinkTypes = SetCustomLinksTypes(seqUrlInfo, customLinkTypes);
    //First show links to GenBank and FASTA, then to Graphics
    customLinksList = GetSeqLinksList(seqUrlInfo);
    if(customLinkTypes & eLinkTypeTraceLinks) {
        linkUrl = seqUrlInfo->seqUrl;
	    link = s_MapCustomLink(linkUrl,"Trace Archive FASTA",seqUrlInfo->accession, "FASTA","lnk" + seqUrlInfo->rid);
	    customLinksList.push_back(link);

        linkUrl = NStr::Replace(seqUrlInfo->seqUrl,"fasta","trace");
        link = s_MapCustomLink(linkUrl,"Trace Archive Trace",seqUrlInfo->accession, "Trace","lnk" + seqUrlInfo->rid);
        customLinksList.push_back(link);

        linkUrl = NStr::Replace(seqUrlInfo->seqUrl,"fasta","quality");
        link = s_MapCustomLink(linkUrl,"Trace Archive Quality",seqUrlInfo->accession, "Quality","lnk" + seqUrlInfo->rid);
        customLinksList.push_back(link);

        linkUrl = NStr::Replace(seqUrlInfo->seqUrl,"fasta","info");
        link = s_MapCustomLink(linkUrl,"Trace Archive Info",seqUrlInfo->accession, "Info","lnk" + seqUrlInfo->rid);
        customLinksList.push_back(link);
    }
    else if(customLinkTypes & eLinkTypeSRALinks) {
        linkUrl = seqUrlInfo->seqUrl;
	    link = s_MapCustomLink(linkUrl,"SRA",seqUrlInfo->accession, "SRA","lnk" + seqUrlInfo->rid);
	    customLinksList.push_back(link);
    }
    else if(customLinkTypes & eLinkTypeSNPLinks) {
        linkUrl = seqUrlInfo->seqUrl;
	    link = s_MapCustomLink(linkUrl,"SNP",seqUrlInfo->accession, "SNP","lnk" + seqUrlInfo->rid);
	    customLinksList.push_back(link);


        //SNP accession=rs35885954
        string rs = NStr::Replace(seqUrlInfo->accession,"rs","");
	    linkUrl = seqUrlInfo->resourcesUrl + rs + "?report=FLT";


        link = s_MapCustomLink(linkUrl,"Flatfile",seqUrlInfo->accession, "Flatfile","lnk" + seqUrlInfo->rid);
	    customLinksList.push_back(link);

        linkUrl = NStr::Replace(linkUrl,"FLT","fasta");
        link = s_MapCustomLink(linkUrl,"FASTA",seqUrlInfo->accession, "FASTA","lnk" + seqUrlInfo->rid);
	    customLinksList.push_back(link);

        linkUrl = NStr::Replace(linkUrl,"fasta","docsum");
        link = s_MapCustomLink(linkUrl,"Graphic summary ",seqUrlInfo->accession, "Graphic summary ","lnk" + seqUrlInfo->rid);
	    customLinksList.push_back(link);
    }
    else if(customLinkTypes & eLinkTypeGSFastaLinks) {
        linkUrl = seqUrlInfo->seqUrl;
	    link = s_MapCustomLink(linkUrl,"GSFASTA",seqUrlInfo->accession, "GSFASTA","lnk" + seqUrlInfo->rid);
	    customLinksList.push_back(link);
    }
    return customLinksList;
}


string CAlignFormatUtil::GetAlignedRegionsURL(SSeqURLInfo *seqUrlInfo,
                                          const CSeq_id& id,
                                          objects::CScope &scope)
{
    const CBioseq_Handle& handle = scope.GetBioseqHandle(id);
    const CBioseq::TId* ids = &handle.GetBioseqCore()->GetId();
    string linkUrl,link;


    linkUrl = CAlignFormatUtil::BuildUserUrl(*ids,
                                                 ZERO_TAX_ID,
                                                 kDownloadUrl,
                                                 seqUrlInfo->database,
                                                 seqUrlInfo->isDbNa,
                                                 seqUrlInfo->rid,
                                                 seqUrlInfo->queryNumber,
                                                 true);
        if(!linkUrl.empty()) {
            linkUrl += "&segs="+ seqUrlInfo->segs;
    }

    return linkUrl;
}



string  CAlignFormatUtil::GetFASTALinkURL(SSeqURLInfo *seqUrlInfo,
                                          const CSeq_id& id,
                                          objects::CScope &scope)

{
    string linkUrl;

    int customLinkTypes = SetCustomLinksTypes(seqUrlInfo, CAlignFormatUtil::eLinkTypeDefault);

    if( (customLinkTypes & eLinkTypeGenLinks) || (customLinkTypes & eLinkTypeTraceLinks)){
         linkUrl = seqUrlInfo->seqUrl;
         linkUrl = NStr::Replace(linkUrl,"genbank","fasta");
    }
    else if(customLinkTypes & eLinkTypeSNPLinks) {
        linkUrl = seqUrlInfo->seqUrl;
        vector<string> parts;
        //SNP accession=dbSNP:rs35885954
        NStr::Split(seqUrlInfo->accession,":rs",parts,NStr::fSplit_MergeDelimiters);
        string rs;
        if(parts.size() > 1) {
            rs = parts[1];
        }
	    linkUrl = seqUrlInfo->resourcesUrl + rs + "?report=fasta";
    }
    return linkUrl;
}


CAlignFormatUtil::DbType CAlignFormatUtil::GetDbType(const CSeq_align_set& actual_aln_list, CScope & scope)
{
    //determine if the database has gi by looking at the 1st hit.
    //Could be wrong but simple for now
    DbType type = eDbTypeNotSet;
    CRef<CSeq_align> first_aln = actual_aln_list.Get().front();
    const CSeq_id& subject_id = first_aln->GetSeq_id(1);

    if (subject_id.Which() != CSeq_id::e_Local){
        const CBioseq_Handle& handleTemp  = scope.GetBioseqHandle(subject_id);
        if(handleTemp){
            TGi giTemp = FindGi(handleTemp.GetBioseqCore()->GetId());
            if (giTemp > ZERO_GI || GetTextSeqID((CConstRef<CSeq_id>)&subject_id)) {
                type = eDbGi;
            } else if (subject_id.Which() == CSeq_id::e_General){
                const CDbtag& dtg = subject_id.GetGeneral();
                const string& dbName = dtg.GetDb();
                if(NStr::CompareNocase(dbName, "TI") == 0){
                    type = eDbGeneral;
                }
            }
        }
    }
    return type;
}

CAlignFormatUtil::SSeqAlignSetCalcParams*
CAlignFormatUtil::GetSeqAlignCalcParams(const CSeq_align& aln)
{
    int score = 0;
    double bits = 0;
    double evalue = 0;
    int sum_n = 0;
    int num_ident = 0;
    list<TGi> use_this_gi;

    use_this_gi.clear();
    //Gets scores directly from seq align
    GetAlnScores(aln, score, bits, evalue, sum_n,
                                       num_ident, use_this_gi);

    unique_ptr<SSeqAlignSetCalcParams> seqSetInfo(new SSeqAlignSetCalcParams);
    seqSetInfo->sum_n = sum_n == -1 ? 1:sum_n ;
    seqSetInfo->id = &(aln.GetSeq_id(1));
    seqSetInfo->use_this_gi = use_this_gi;
    seqSetInfo->bit_score = bits;
    seqSetInfo->raw_score = score;
    seqSetInfo->evalue = evalue;
    seqSetInfo->match = num_ident;
    seqSetInfo->id = &(aln.GetSeq_id(1));
    seqSetInfo->subjRange = CRange<TSeqPos>(0,0);
    seqSetInfo->flip = false;

    return seqSetInfo.release();
}



CAlignFormatUtil::SSeqAlignSetCalcParams*
CAlignFormatUtil::GetSeqAlignSetCalcParams(const CSeq_align_set& aln,int queryLength, bool do_translation)
{
    int score = 0;
    double bits = 0;
    double evalue = 0;
    int sum_n = 0;
    int num_ident = 0;
    SSeqAlignSetCalcParams* seqSetInfo = NULL;

    if(aln.Get().empty())
        return seqSetInfo;

    seqSetInfo = GetSeqAlignCalcParams(*(aln.Get().front()));

    double total_bits = 0;
    double highest_bits = 0;
    double lowest_evalue = 0;
    int highest_length = 1;
    int highest_ident = 0;
    //int highest_identity = 0;
    double totalLen = 0;

    list<TGi> use_this_gi;   // Not used here, but needed for GetAlnScores.

    seqSetInfo->subjRange = CAlignFormatUtil::GetSeqAlignCoverageParams(aln,&seqSetInfo->master_covered_length,&seqSetInfo->flip);
    seqSetInfo->percent_coverage = 100*seqSetInfo->master_covered_length/queryLength;

    ITERATE(CSeq_align_set::Tdata, iter, aln.Get()) {
        int align_length = CAlignFormatUtil::GetAlignmentLength(**iter, do_translation);
        totalLen += align_length;

        CAlignFormatUtil::GetAlnScores(**iter, score, bits, evalue, sum_n,
                                   num_ident, use_this_gi);
        use_this_gi.clear();

        total_bits += bits;

/// IMPORTANT: based on WB-1175, the trigger for setting the highest identity
///            is not the highest identity value, but the identity value of
///            the alignment with the highest score!
///
///     if (100*num_ident/align_length > highest_identity) { -- this condition is disabled

        if (bits > highest_bits) {  // this is the replacement condition (WB-1175)
            highest_length = align_length;
            highest_ident = num_ident;
///         highest_identity = 100*num_ident/align_length;
        }

        if (bits > highest_bits) {
            highest_bits = bits;
            lowest_evalue = evalue;
        }
    }
    seqSetInfo->match = highest_ident;
    seqSetInfo->align_length = highest_length;
    seqSetInfo->percent_identity = CAlignFormatUtil::GetPercentIdentity(seqSetInfo->match, seqSetInfo->align_length);

    seqSetInfo->total_bit_score = total_bits;
    seqSetInfo->bit_score = highest_bits;
    seqSetInfo->evalue = lowest_evalue;
    seqSetInfo->hspNum = static_cast<int>(aln.Size());
    seqSetInfo->totalLen = (Int8)totalLen;

    return seqSetInfo;
}

double CAlignFormatUtil::GetSeqAlignSetCalcPercentIdent(const CSeq_align_set& aln, bool do_translation)
{
    int score = 0;
    double bits = 0;
    double evalue = 0;
    int sum_n = 0;
    int num_ident = 0;

    if(aln.Get().empty())
      return -1;

    double highest_bits = 0;
    int highest_length = 1;
    int highest_ident = 0;

    list<TGi> use_this_gi;   // Not used here, but needed for GetAlnScores.

    ITERATE(CSeq_align_set::Tdata, iter, aln.Get()) {
        int align_length = CAlignFormatUtil::GetAlignmentLength(**iter, do_translation);

        CAlignFormatUtil::GetAlnScores(**iter, score, bits, evalue, sum_n,
                                   num_ident, use_this_gi);


/// IMPORTANT: based on WB-1175, the trigger for setting the highest identity
///            is not the highest identity value, but the identity value of
///            the alignment with the highest score!
///
///     if (100*num_ident/align_length > highest_identity) { -- this condition is disabled

        if (bits > highest_bits) {  // this is the replacement condition (WB-1175)
            highest_length = align_length;
            highest_ident = num_ident;
///         highest_identity = 100*num_ident/align_length;
            highest_bits = bits;
        }
    }

    double percent_identity = CAlignFormatUtil::GetPercentIdentity(highest_ident, highest_length);
    return percent_identity;
}


template<class container> bool
s_GetBlastScore(const container&  scoreList,
                double& evalue,
                double& bitScore,
                double& totalBitScore,
                int& percentCoverage,
                double& percentIdent,
                int& hspNum,
                double& totalLen,
                int &rawScore,
                int& sum_n,
                list<TGi>& use_this_gi)
{
    const string k_GiPrefix = "gi:";
    bool hasScore = false;


    ITERATE (typename container, iter, scoreList) {
        const CObject_id& id=(*iter)->GetId();
        if (id.IsStr()) {
            hasScore = true;
            if (id.GetStr()=="seq_evalue") {
                evalue = (*iter)->GetValue().GetReal();
            } else if (id.GetStr()=="seq_bit_score"){
                bitScore = (*iter)->GetValue().GetReal();
            } else if (id.GetStr()=="seq_total_bit_score"){
                totalBitScore = (*iter)->GetValue().GetReal();
            } else if (id.GetStr()=="seq_percent_coverage"){
                percentCoverage = (*iter)->GetValue().GetInt();
            } else if (id.GetStr()=="seq_percent_identity" && (*iter)->GetValue().IsInt()){
                percentIdent = (*iter)->GetValue().GetInt();
            } else if (id.GetStr()=="seq_percent_identity" && (*iter)->GetValue().IsReal()){
                percentIdent = (*iter)->GetValue().GetReal();
            } else if (id.GetStr()=="seq_hspnum"){
                hspNum = (*iter)->GetValue().GetInt();
            } else if (id.GetStr()=="seq_align_totlen"){
                totalLen = (*iter)->GetValue().GetReal();
            } else if (id.GetStr()=="score"){
                rawScore = (*iter)->GetValue().GetInt();
            } else if (id.GetStr()=="use_this_gi"){
            	Uint4 gi_v = (Uint4) ((*iter)->GetValue().GetInt());
                use_this_gi.push_back(GI_FROM(Uint4, gi_v));
            } else if (id.GetStr()=="sum_n"){
                sum_n = (*iter)->GetValue().GetInt();
            }
            else if(NStr::StartsWith(id.GetStr(),k_GiPrefix)) { //will be used when switch to 64bit GIs
                string strGi = NStr::Replace(id.GetStr(),k_GiPrefix,"");
                TGi gi = NStr::StringToNumeric<TGi>(strGi);
                use_this_gi.push_back(gi);
            }
        }
    }
    return hasScore;
}


void CAlignFormatUtil::GetUseThisSequence(const CSeq_align& aln,list<TGi>& use_this_gi)

{
    const string k_GiPrefix = "gi:";

    if(!aln.CanGetExt() || aln.GetExt().size() == 0) return;
    const CUser_object &user = *(aln.GetExt().front());

    if (user.IsSetType() && user.GetType().IsStr() && user.GetType().GetStr() == "use_this_seqid" && user.IsSetData()) {
        const CUser_object::TData& fields = user.GetData();
        for (CUser_object::TData::const_iterator fit = fields.begin();  fit != fields.end(); ++fit) {
            const CUser_field& field = **fit;

            if (field.IsSetLabel() && field.GetLabel().IsStr() && field.GetLabel().GetStr() == "SEQIDS" &&
                                                                     field.IsSetData()  &&  field.GetData().IsStrs()) {
                const CUser_field::C_Data::TStrs& strs = field.GetData().GetStrs();
                ITERATE(CUser_field::TData::TStrs, acc_iter, strs) {
                    if(NStr::StartsWith(*acc_iter,k_GiPrefix)) { //will be used when switch to 64bit GIs
                        string strGi = NStr::Replace(*acc_iter,k_GiPrefix,"");
                        TGi gi = NStr::StringToNumeric<TGi>(strGi);
                        use_this_gi.push_back(gi);
                    }
                }
            }
        }
    }
}


/*use_this_seq will contain gi:nnnnnn or seqid:ssssss string list*/
void CAlignFormatUtil::GetUseThisSequence(const CSeq_align& aln,list<string>& use_this_seq)

{
    if(!aln.CanGetExt() || aln.GetExt().size() == 0) return;
    const CUser_object &user = *(aln.GetExt().front());

    if (user.IsSetType() && user.GetType().IsStr() && user.GetType().GetStr() == "use_this_seqid" && user.IsSetData()) {
        const CUser_object::TData& fields = user.GetData();
        for (CUser_object::TData::const_iterator fit = fields.begin();  fit != fields.end(); ++fit) {
            const CUser_field& field = **fit;

            if (field.IsSetLabel() && field.GetLabel().IsStr() && field.GetLabel().GetStr() == "SEQIDS" &&
                                                                     field.IsSetData()  &&  field.GetData().IsStrs()) {
                const CUser_field::C_Data::TStrs& strs = field.GetData().GetStrs();
                ITERATE(CUser_field::TData::TStrs, acc_iter, strs) {
                    use_this_seq.push_back(*acc_iter);
                }
            }
        }
    }
}



CAlignFormatUtil::SSeqAlignSetCalcParams*
CAlignFormatUtil::GetSeqAlignSetCalcParamsFromASN(const CSeq_align_set& alnSet)
{
    bool hasScore = false;
    double evalue = -1;
    double bitScore = -1;
    double totalBitScore = -1;
    int percentCoverage = -1;
    double percentIdent = -1;
    int hspNum = 0;
    double totalLen = 0;
    int rawScore = -1;
    int sum_n = -1;
    list<TGi> use_this_gi;
    list<string> use_this_seq;

    const CSeq_align& aln = *(alnSet.Get().front());

    hasScore = s_GetBlastScore(aln.GetScore(),evalue,bitScore, totalBitScore,percentCoverage,percentIdent,hspNum,totalLen,rawScore,sum_n,use_this_gi);

    if(!hasScore){
        const CSeq_align::TSegs& seg = aln.GetSegs();
        if(seg.Which() == CSeq_align::C_Segs::e_Std){
            s_GetBlastScore(seg.GetStd().front()->GetScores(),
                            evalue,bitScore, totalBitScore,percentCoverage,percentIdent,hspNum,totalLen,rawScore,sum_n,use_this_gi);
        } else if (seg.Which() == CSeq_align::C_Segs::e_Dendiag){
            s_GetBlastScore(seg.GetDendiag().front()->GetScores(),
                            evalue,bitScore, totalBitScore,percentCoverage,percentIdent,hspNum,totalLen,rawScore,sum_n,use_this_gi);
        }  else if (seg.Which() == CSeq_align::C_Segs::e_Denseg){
            s_GetBlastScore(seg.GetDenseg().GetScores(),
                            evalue,bitScore, totalBitScore,percentCoverage,percentIdent,hspNum,totalLen,rawScore,sum_n,use_this_gi);
        }
    }

    if(use_this_gi.size() == 0) {
        GetUseThisSequence(aln,use_this_seq);
    }
    else {
        use_this_seq = s_NumGiToStringGiList(use_this_gi);//for backward compatability
    }


    unique_ptr<SSeqAlignSetCalcParams> seqSetInfo(new SSeqAlignSetCalcParams);
    seqSetInfo->evalue = evalue;
    seqSetInfo->bit_score = bitScore;
    seqSetInfo->total_bit_score = totalBitScore;
    seqSetInfo->percent_coverage = percentCoverage;
    seqSetInfo->percent_identity = percentIdent;
    seqSetInfo->hspNum = hspNum;
    seqSetInfo->totalLen = (Int8)totalLen;

    seqSetInfo->sum_n = sum_n == -1 ? 1:sum_n ;
    seqSetInfo->id = &(aln.GetSeq_id(1));
    seqSetInfo->use_this_gi = StringGiToNumGiList(use_this_seq);//for backward compatability
    seqSetInfo->use_this_seq = use_this_seq;
    seqSetInfo->raw_score = rawScore;//not used

    seqSetInfo->subjRange = CRange<TSeqPos>(0,0);
    seqSetInfo->flip = false;

    return seqSetInfo.release();
}

CRef<CSeq_id> CAlignFormatUtil::GetDisplayIds(const CBioseq_Handle& handle,
                                const CSeq_id& aln_id,
                                list<TGi>& use_this_gi,
                                TGi& gi)

{
    TTaxId taxid = ZERO_TAX_ID;
    CRef<CSeq_id> wid = CAlignFormatUtil::GetDisplayIds(handle, aln_id, use_this_gi, gi, taxid);
    return wid;
}

CRef<CSeq_id> CAlignFormatUtil::GetDisplayIds(const CBioseq_Handle& handle,
                                const CSeq_id& aln_id,
                                list<TGi>& use_this_gi,
                                TGi& gi,
                                TTaxId& taxid)

{
    const CRef<CBlast_def_line_set> bdlRef = CSeqDB::ExtractBlastDefline(handle);
    const list< CRef< CBlast_def_line > > &bdl = (bdlRef.Empty()) ? list< CRef< CBlast_def_line > >() : bdlRef->Get();

    const CBioseq::TId* ids = &handle.GetBioseqCore()->GetId();
    CRef<CSeq_id> wid;

    gi = ZERO_GI;
    taxid = ZERO_TAX_ID;
    if(bdl.empty()){
        wid = FindBestChoice(*ids, CSeq_id::WorstRank);
        gi = FindGi(*ids);
    } else {
        bool found = false;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId* cur_id = &((*iter)->GetSeqid());
            TGi cur_gi =  FindGi(*cur_id);
            wid = FindBestChoice(*cur_id, CSeq_id::WorstRank);
            if ((*iter)->IsSetTaxid() && (*iter)->CanGetTaxid()){
                taxid = (*iter)->GetTaxid();
            }
            if (!use_this_gi.empty()) {
                ITERATE(list<TGi>, iter_gi, use_this_gi){
                    if(cur_gi == *iter_gi){
                        found = true;
                        break;
                    }
                }
            } else {
                ITERATE(CBioseq::TId, iter_id, *cur_id) {
                    if ((*iter_id)->Match(aln_id)
                      || (aln_id.IsGeneral() && aln_id.GetGeneral().CanGetDb() &&
                         (*iter_id)->IsGeneral() && (*iter_id)->GetGeneral().CanGetDb() &&
                         aln_id.GetGeneral().GetDb() == (*iter_id)->GetGeneral().GetDb())) {
                        found = true;
                    }
                }
            }
            if(found){
                gi = cur_gi;
                break;
            }
        }
    }
    return wid;
}



//removes "gi:" or "seqid:" prefix from gi:nnnnnnn or seqid:nnnnn
static string s_UseThisSeqToTextSeqID(string use_this_seqid, bool &isGi)
{
    const string k_GiPrefix = "gi:";
    const string k_SeqIDPrefix = "seqid:";
    isGi = false;
    string textSeqid;
    if(NStr::StartsWith(use_this_seqid,k_GiPrefix)) {
        textSeqid = NStr::Replace(use_this_seqid,k_GiPrefix,"");
        isGi = true;
    }
    else if(NStr::StartsWith(use_this_seqid,k_SeqIDPrefix)) {
        textSeqid = NStr::Replace(use_this_seqid,k_SeqIDPrefix,"");
    }
    else  {//assume no prefix - gi
        if(NStr::StringToInt8(use_this_seqid,NStr::fConvErr_NoThrow)) {
            isGi = true;
        }
    }
    return textSeqid;
}



//assume that we have EITHER gi: OR seqid: in the list
bool CAlignFormatUtil::IsGiList(list<string> &use_this_seq)
{
    bool isGi = false;
    ITERATE(list<string>, iter_seq, use_this_seq){
        s_UseThisSeqToTextSeqID( *iter_seq, isGi);
        break;
    }
    return isGi;
}

list<TGi> CAlignFormatUtil::StringGiToNumGiList(list<string> &use_this_seq)
{
    list<TGi> use_this_gi;
    ITERATE(list<string>, iter_seq, use_this_seq){
        bool isGi = false;
        string strGI = s_UseThisSeqToTextSeqID( *iter_seq, isGi);
        if(isGi) use_this_gi.push_back(NStr::StringToNumeric<TGi>(strGI));
    }
    return use_this_gi;
}



bool CAlignFormatUtil::MatchSeqInSeqList(TGi cur_gi, CRef<CSeq_id> &seqID, list<string> &use_this_seq,bool *isGiList)
{
    bool found = false;
    bool isGi = false;

    string curSeqID = CAlignFormatUtil::GetLabel(seqID,true); //uses GetSeqIdString(true)
    ITERATE(list<string>, iter_seq, use_this_seq){
        isGi = false;
        string useThisSeq = s_UseThisSeqToTextSeqID(*iter_seq, isGi);
        if((isGi && cur_gi == NStr::StringToNumeric<TGi>((useThisSeq))) || (!isGi && curSeqID == useThisSeq)){
            found = true;
            break;
         }
    }
    if(isGiList) *isGiList = isGi;
    return found;
}


bool CAlignFormatUtil::MatchSeqInSeqList(CConstRef<CSeq_id> &alnSeqID, list<string> &use_this_seq,vector <string> &seqList)
{
    bool isGi = false;
    string curSeqID;
    if(alnSeqID->IsGi()) {
        curSeqID = NStr::NumericToString(alnSeqID->GetGi());
    }
    else {
        curSeqID = CAlignFormatUtil::GetLabel(alnSeqID,true); //uses GetSeqIdString(true)
    }
    //match with seqid in seq_align
    bool found = std::find(seqList.begin(), seqList.end(), curSeqID) != seqList.end();
    if(!found) {
        //match in use_this_seq list
        ITERATE(list<string>, iter_seq, use_this_seq){
            string useThisSeq = s_UseThisSeqToTextSeqID(*iter_seq, isGi);
            found = std::find(seqList.begin(), seqList.end(), useThisSeq) != seqList.end();
            if(found){
                break;
            }
        }
    }
    return found;
}

bool CAlignFormatUtil::MatchSeqInUseThisSeqList(list<string> &use_this_seq, string textSeqIDToMatch)
{
    bool has_match = false;

    ITERATE(list<string>, iter_seq, use_this_seq) {
        bool isGi;
        string useThisSeq = s_UseThisSeqToTextSeqID(*iter_seq, isGi);
        if(useThisSeq == textSeqIDToMatch) {
             has_match = true;
             break;
        }
    }
    return has_match;
}

bool CAlignFormatUtil::RemoveSeqsOfAccessionTypeFromSeqInUse(list<string> &use_this_seq, CSeq_id::EAccessionInfo accessionType)
{
    list<string> new_use_this_seq;
    bool hasAccType = false;
    bool isGI = false;

    ITERATE(list<string>, iter_seq, use_this_seq) {
        string useThisSeq = s_UseThisSeqToTextSeqID(*iter_seq, isGI);
        CSeq_id::EAccessionInfo useThisSeqAccType = CSeq_id::IdentifyAccession (useThisSeq);
        if(useThisSeqAccType != accessionType) {
            new_use_this_seq.push_back(useThisSeq);
        }
        else {
            hasAccType = true;
        }
    }
    use_this_seq = new_use_this_seq;
    return hasAccType;
}

CRef<CSeq_id> CAlignFormatUtil::GetDisplayIds(const CBioseq_Handle& handle,
                                const CSeq_id& aln_id,
                                list<string>& use_this_seq,
                                TGi *gi,
                                TTaxId *taxid,
                                string *textSeqID)

{
    const CRef<CBlast_def_line_set> bdlRef = CSeqDB::ExtractBlastDefline(handle);
    const list< CRef< CBlast_def_line > > &bdl = (bdlRef.Empty()) ? list< CRef< CBlast_def_line > >() : bdlRef->Get();

    const CBioseq::TId* ids = &handle.GetBioseqCore()->GetId();
    CRef<CSeq_id> wid;

    if(gi) *gi = ZERO_GI;
    if(taxid) *taxid = ZERO_TAX_ID;
    if(bdl.empty()){
        wid = FindBestChoice(*ids, CSeq_id::WorstRank);
        if(gi) *gi = FindGi(*ids);
        if(textSeqID) *textSeqID = GetLabel(wid,true);//uses GetSeqIdString(true)
    } else {
        bool found = false;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId* cur_id = &((*iter)->GetSeqid());
            TGi cur_gi =  FindGi(*cur_id);
            wid = FindBestChoice(*cur_id, CSeq_id::WorstRank);
            string curSeqID = GetLabel(wid,true);//uses GetSeqIdString(true)
            if (taxid && (*iter)->IsSetTaxid() && (*iter)->CanGetTaxid()){
                *taxid = (*iter)->GetTaxid();
            }
            if (!use_this_seq.empty()) {
                ITERATE(list<string>, iter_seq, use_this_seq){
                    bool isGi = false;
                    string useThisSeq = s_UseThisSeqToTextSeqID( *iter_seq, isGi);
                    if((isGi && cur_gi == NStr::StringToNumeric<TGi>((useThisSeq))) || (!isGi && curSeqID == useThisSeq)){
                        found = true;
                        break;
                    }
                }
            } else {
                ITERATE(CBioseq::TId, iter_id, *cur_id) {
                    if ((*iter_id)->Match(aln_id)
                      || (aln_id.IsGeneral() && aln_id.GetGeneral().CanGetDb() &&
                         (*iter_id)->IsGeneral() && (*iter_id)->GetGeneral().CanGetDb() &&
                         aln_id.GetGeneral().GetDb() == (*iter_id)->GetGeneral().GetDb())) {
                        found = true;
                    }
                }
            }
            if(found){
                if(gi) *gi = cur_gi;
                if(textSeqID) *textSeqID = curSeqID;
                break;
            }
        }
    }

    return wid;
}


TGi CAlignFormatUtil::GetDisplayIds(const list< CRef< CBlast_def_line > > &bdl,
                                              const CSeq_id& aln_id,
                                              list<TGi>& use_this_gi)


{
    TGi gi = ZERO_GI;

    if(!bdl.empty()){
        bool found = false;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId* cur_id = &((*iter)->GetSeqid());
            TGi cur_gi =  FindGi(*cur_id);
            if (!use_this_gi.empty()) {
                ITERATE(list<TGi>, iter_gi, use_this_gi){
                    if(cur_gi == *iter_gi){
                        found = true;
                        break;
                    }
                }
            } else {
                ITERATE(CBioseq::TId, iter_id, *cur_id) {
                    if ((*iter_id)->Match(aln_id)
                      || (aln_id.IsGeneral() && aln_id.GetGeneral().CanGetDb() &&
                         (*iter_id)->IsGeneral() && (*iter_id)->GetGeneral().CanGetDb() &&
                         aln_id.GetGeneral().GetDb() == (*iter_id)->GetGeneral().GetDb())) {
                        found = true;
                    }
                }
            }
            if(found){
                gi = cur_gi;
                break;
            }
        }
    }
    return gi;
}

static inline CRange<TSeqPos> & s_FixMinusStrandRange(CRange<TSeqPos> & rng)
{
	if(rng.GetFrom() > rng.GetTo()){
		rng.Set(rng.GetTo(), rng.GetFrom());
	}
	//cerr << "Query Rng: " << rng.GetFrom() << "-" << rng.GetTo() << endl;
	return rng;
}

int CAlignFormatUtil::GetUniqSeqCoverage(CSeq_align_set & alnset)
{
	if(alnset.IsEmpty())
		return 0;

	bool isDenDiag = (alnset.Get().front()->GetSegs().Which() == CSeq_align::C_Segs::e_Dendiag) ?
			          true : false;

	list<CRef<CSeq_align> >::iterator mItr=alnset.Set().begin();
	CRangeCollection<TSeqPos> subj_rng_coll((*mItr)->GetSeqRange(1));
	CRange<TSeqPos> q_rng((*mItr)->GetSeqRange(0));
	/*
	cerr << MSerial_AsnText << **mItr;
	cerr << (*mItr)->GetSeqRange(0).GetFrom() << endl;
	cerr << (*mItr)->GetSeqRange(0).GetTo() << endl;
	cerr << (*mItr)->GetSeqRange(0).GetToOpen() << endl;
	cerr << (*mItr)->GetSeqRange(1).GetFrom() << endl;
	cerr << (*mItr)->GetSeqRange(1).GetTo() << endl;
	cerr << (*mItr)->GetSeqRange(1).GetToOpen() << endl;
	*/
	CRangeCollection<TSeqPos> query_rng_coll(s_FixMinusStrandRange(q_rng));
	++mItr;
	for(;mItr != alnset.Set().end(); ++mItr) {
		const CRange<TSeqPos> align_subj_rng((*mItr)->GetSeqRange(1));
		// subject range should always be on the positive strand
		ASSERT(align_subj_rng.GetTo() > align_subj_rng.GetFrom());
		CRangeCollection<TSeqPos> coll(align_subj_rng);
		coll.Subtract(subj_rng_coll);

		if (coll.empty())
			continue;

		if(coll[0] == align_subj_rng) {
			CRange<TSeqPos> query_rng ((*mItr)->GetSeqRange(0));
			//cerr << "Subj Rng :" << align_subj_rng.GetFrom() << "-" << align_subj_rng.GetTo() << endl;
			query_rng_coll += s_FixMinusStrandRange(query_rng);
			subj_rng_coll += align_subj_rng;
		}
		else {
			ITERATE (CRangeCollection<TSeqPos>, uItr, coll) {
				CRange<TSeqPos> query_rng;
				const CRange<TSeqPos> & subj_rng = (*uItr);
				CRef<CSeq_align> densegAln
						= isDenDiag ? CAlignFormatUtil::CreateDensegFromDendiag(**mItr) : (*mItr);

				CAlnMap map(densegAln->GetSegs().GetDenseg());
				TSignedSeqPos subj_aln_start =  map.GetAlnPosFromSeqPos(1,subj_rng.GetFrom());
				TSignedSeqPos subj_aln_end =  map.GetAlnPosFromSeqPos(1,subj_rng.GetTo());
				query_rng.SetFrom(map.GetSeqPosFromAlnPos(0,subj_aln_start));
				query_rng.SetTo(map.GetSeqPosFromAlnPos(0,subj_aln_end));

				//cerr << "Subj Rng :" << subj_rng.GetFrom() << "-" << subj_rng.GetTo() << endl;
				query_rng_coll += s_FixMinusStrandRange(query_rng);
				subj_rng_coll += subj_rng;
			}
		}
	}

	return query_rng_coll.GetCoveredLength();
}

///return id type specified or null ref
///@param ids: the input ids
///@param choice: id of choice
///@return: the id with specified type
///
static CRef<CSeq_id> s_GetSeqIdByType(const list<CRef<CSeq_id> >& ids,
                                      CSeq_id::E_Choice choice)
{
    CRef<CSeq_id> cid;

    for (CBioseq::TId::const_iterator iter = ids.begin(); iter != ids.end();
         iter ++){
        if ((*iter)->Which() == choice){
            cid = *iter;
            break;
        }
    }

    return cid;
}

///return gi from id list
///@param ids: the input ids
///@return: the gi if found
///
TGi CAlignFormatUtil::GetGiForSeqIdList (const list<CRef<CSeq_id> >& ids)
{
    TGi gi = ZERO_GI;
    CRef<CSeq_id> id = s_GetSeqIdByType(ids, CSeq_id::e_Gi);
    if (!(id.Empty())){
        return id->GetGi();
    }
    return gi;
}

string CAlignFormatUtil::GetTitle(const CBioseq_Handle & bh)
{
	CSeqdesc_CI desc_t(bh, CSeqdesc::e_Title);
	string t = kEmptyStr;
	for (;desc_t; ++desc_t) {
		t += desc_t->GetTitle() + " ";
	}
	return t;
}

string CAlignFormatUtil::GetBareId(const CSeq_id& id)
{
    string retval;

    if (id.IsGi() || id.IsPrf() || id.IsPir()) {
        retval = id.AsFastaString();
    }
    else {
        retval = id.GetSeqIdString(true);
    }

    return retval;
}


bool CAlignFormatUtil::GetTextSeqID(CConstRef<CSeq_id> seqID, string *textSeqID)
{
    bool hasTextSeqID = true;

    const CTextseq_id* text_id = seqID->GetTextseq_Id();
    //returns non zero if e_Genbank,e_Embl,e_Ddbj,e_Pir,e_Swissprot,case e_Other,e_Prf,case e_Tpg,e_Tpe,case e_Tpd,case e_Gpipe, e_Named_annot_track
    if(!text_id) { //check for pdb and pat
        if(!(seqID->Which() == CSeq_id::e_Pdb) &&  !(seqID->Which() == CSeq_id::e_Patent) && !(seqID->Which() == CSeq_id::e_Gi)) {
            hasTextSeqID = false;
        }
    }

    if(hasTextSeqID && textSeqID) {
        seqID->GetLabel(textSeqID, CSeq_id::eContent);
    }
    return hasTextSeqID;
}



bool CAlignFormatUtil::GetTextSeqID(const list<CRef<CSeq_id> > & ids, string *textSeqID)
{
    bool hasTextSeqID = false;

    CConstRef<CSeq_id> seqID = FindTextseq_id(ids);
    //returns non zero if e_Genbank,e_Embl,e_Ddbj,e_Pir,e_Swissprot,case e_Other,e_Prf,case e_Tpg,e_Tpe,case e_Tpd,case e_Gpipe, e_Named_annot_track
    if(seqID.Empty()) {
        seqID = GetSeq_idByType(ids, CSeq_id::e_Pdb);
    }
    if(seqID.Empty()) {
        seqID = GetSeq_idByType(ids, CSeq_id::e_Patent);
    }
    if(!seqID.Empty()) {
        hasTextSeqID = true;
        if(textSeqID) seqID->GetLabel(textSeqID, CSeq_id::eContent);
    }
    return hasTextSeqID;
}

CRef<CSeq_align_set> CAlignFormatUtil::FilterSeqalignBySeqList(CSeq_align_set& source_aln,
                                                               vector <string> &seqList)
{
    CConstRef<CSeq_id> previous_id, subid;
    list<string> use_this_seq;
    bool match = false;

    CRef<CSeq_align_set> new_aln(new CSeq_align_set);
    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){
        subid = &((*iter)->GetSeq_id(1));
        if(previous_id.Empty() || !subid->Match(*previous_id)){
            use_this_seq.clear();
            CAlignFormatUtil::GetUseThisSequence(**iter,use_this_seq);
            match = MatchSeqInSeqList(subid, use_this_seq,seqList);
        }

        previous_id = subid;
        if(match) {
            new_aln->Set().push_back(*iter);
        }
    }
    return new_aln;
}


END_SCOPE(align_format)
END_NCBI_SCOPE
