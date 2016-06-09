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
 * Authors:  Ning Ma
 *
 */

/** @file igblastn_app.cpp
 * IGBLASTN command line application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
	"$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/igblastn_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include "../blast/blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CIgBlastnApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CIgBlastnApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CIgBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// This application's command line args
    CRef<CIgBlastnAppArgs> m_CmdLineArgs; 
  


    struct SCloneNuc {
        string na;
        string chain_type;
        string v_gene;
        string d_gene;
        string j_gene;
    };

    struct SAaInfo {
        string seqid;
        int count;
        string all_seqid;
        double min_identity;
        double max_identity;
    };

    struct SAaStatus {
        string aa;
        string productive;
    };

    struct sort_order {
        bool operator()(const SCloneNuc s1, const SCloneNuc s2) const
        {
            
            if (s1.na != s2.na)
                return s1.na < s2.na ? true : false;
            
            if (s1.chain_type != s2.chain_type)
                return s1.chain_type < s2.chain_type ? true : false;

            if (s1.v_gene != s2.v_gene)
                return s1.v_gene < s2.v_gene ? true : false;
            
            if (s1.d_gene != s2.d_gene)
                return s1.d_gene < s2.d_gene ? true : false;

            if (s1.j_gene != s2.j_gene)
                return s1.j_gene < s2.j_gene ? true : false;
            
            
            return false;
        }
    };

    struct sort_order_aa_status {
        bool operator()(const SAaStatus s1, const SAaStatus s2) const
        {
            if (s1.aa != s2.aa)
                return s1.aa < s2.aa ? true : false;
            
            if (s1.productive != s2.productive)
                return s1.productive < s2.productive ? true : false;
                                                                        
            return false;
        }
    };

    typedef map<SAaStatus, SAaInfo*, sort_order_aa_status> AaMap;
    typedef map<SCloneNuc, AaMap*, sort_order> CloneInfo;
    CloneInfo m_Clone;
    
    static bool x_SortByCount(const pair<const SCloneNuc*, AaMap*> *c1, const pair<const SCloneNuc*, AaMap*> *c2) {
        
        int c1_total = 0;
        int c2_total = 0;
        ITERATE(AaMap, iter, *(c1->second)){
            c1_total += iter->second->count;
        }
        ITERATE(AaMap, iter, *(c2->second)){
            c2_total += iter->second->count;
        }
 
        
        return c1_total > c2_total;
    }


};



void CIgBlastnApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CIgBlastnAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

int CIgBlastnApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
        CRef<CBlastOptionsHandle> opts_hndl;
        if(RecoverSearchStrategy(args, m_CmdLineArgs)) {
           	opts_hndl.Reset(&*m_CmdLineArgs->SetOptionsForSavedStrategy(args));
        }
        else {
           	opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));
        }
        const CBlastOptions& opt = opts_hndl->GetOptions();

        /*** Get the query sequence(s) ***/
        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();
        SDataLoaderConfig dlconfig(query_opts->QueryIsProtein());
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        CBlastInputSourceConfig iconfig(dlconfig, query_opts->GetStrand(),
                                     query_opts->UseLowercaseMasks(),
                                     query_opts->GetParseDeflines(),
                                     query_opts->GetRange());
        iconfig.SetQueryLocalIdMode();
        CBlastFastaInputSource fasta(m_CmdLineArgs->GetInputStream(), iconfig);
        CBlastInput input(&fasta, m_CmdLineArgs->GetQueryBatchSize());

        /*** Initialize igblast database/subject and options ***/
        CRef<CIgBlastArgs> ig_args(m_CmdLineArgs->GetIgBlastArgs());
        CRef<CIgBlastOptions> ig_opts(ig_args->GetIgBlastOptions());

        /*** Initialize the database/subject ***/
        bool db_is_remote = true;
        CRef<CScope> scope;
        CRef<CLocalDbAdapter> blastdb;
        CRef<CLocalDbAdapter> blastdb_full;
        
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
        if (db_args->GetDatabaseName() == kEmptyStr && 
            db_args->GetSubjects().Empty()) {
            blastdb.Reset(&(*(ig_opts->m_Db[0])));
            scope.Reset(new CScope(*CObjectManager::GetInstance()));
            db_is_remote = false;
            CSearchDatabase sdb(ig_opts->m_Db[0]->GetDatabaseName() + " " +
                    ig_opts->m_Db[1]->GetDatabaseName() + " " +
                    ig_opts->m_Db[2]->GetDatabaseName(), 
                    CSearchDatabase::eBlastDbIsNucleotide);
            blastdb_full.Reset(new CLocalDbAdapter(sdb));
        } else {
            InitializeSubject(db_args, opts_hndl, m_CmdLineArgs->ExecuteRemotely(),
                              blastdb, scope);
            if (m_CmdLineArgs->ExecuteRemotely()) {
                blastdb_full.Reset(&(*blastdb));
            } else {
                CSearchDatabase sdb(ig_opts->m_Db[0]->GetDatabaseName() + " " +
                    ig_opts->m_Db[1]->GetDatabaseName() + " " +
                    ig_opts->m_Db[2]->GetDatabaseName() + " " +
                    blastdb->GetDatabaseName(), 
                    CSearchDatabase::eBlastDbIsNucleotide);
                blastdb_full.Reset(new CLocalDbAdapter(sdb));
            }
        }
        _ASSERT(blastdb && scope);

        // TODO: whose priority is higher?
        ig_args->AddIgSequenceScope(scope);

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        Int4 num_alignments = (db_args->GetDatabaseName() == kEmptyStr) ? 
                               0 : fmt_args->GetNumAlignments();
        CBlastFormat formatter(opt, *blastdb_full,
                               fmt_args->GetFormattedOutputChoice(),
                               query_opts->GetParseDeflines(),
                               m_CmdLineArgs->GetOutputStream(),
                               fmt_args->GetNumDescriptions(),
                               num_alignments,
                               *scope,
                               opt.GetMatrixName(),
                               fmt_args->ShowGis(),
                               fmt_args->DisplayHtmlOutput(),
                               opt.GetQueryGeneticCode(),
                               opt.GetDbGeneticCode(),
                               opt.GetSumStatisticsMode(),
                               false,
                               blastdb->GetFilteringAlgorithm(),
                               fmt_args->GetCustomOutputFormatSpec(),
                               m_CmdLineArgs->GetTask() == "megablast",
                               opt.GetMBIndexLoaded(),
                               &*ig_opts);
                               
        
        //formatter.PrintProlog();
        if(fmt_args->GetFormattedOutputChoice() == 
           CFormattingArgs::eFlatQueryAnchoredIdentities || 
           fmt_args->GetFormattedOutputChoice() == 
           CFormattingArgs::eFlatQueryAnchoredNoIdentities){
            if(blastdb_full->GetDatabaseName() != NcbiEmptyString){
                vector<CBlastFormatUtil::SDbInfo> db_info;
                CBlastFormatUtil::GetBlastDbInfo(db_info, blastdb_full->GetDatabaseName(), 
                                                 ig_opts->m_IsProtein, -1, false);
                CBlastFormatUtil::PrintDbReport(db_info, 68, m_CmdLineArgs->GetOutputStream(), true);
            }
        }

        int total_input = 0;
        
        /*** Process the input ***/
        for (; !input.End(); formatter.ResetScopeHistory()) {

            CRef<CBlastQueryVector> query(input.GetNextSeqBatch(*scope));

            //SaveSearchStrategy(args, m_CmdLineArgs, queries, opts_hndl);
            CRef<CSearchResultSet> results;

            if (m_CmdLineArgs->ExecuteRemotely() && db_is_remote) {
                CIgBlast rmt_blast(query, 
                                   db_args->GetSearchDatabase(), 
                                   db_args->GetSubjects(),
                                   opts_hndl, ig_opts,
                                   NcbiEmptyString,
                                   scope);
                //TODO:          m_CmdLineArgs->ProduceDebugRemoteOutput(),
                //TODO:          m_CmdLineArgs->GetClientId());
                results = rmt_blast.Run();
            } else {
                CIgBlast lcl_blast(query, blastdb, opts_hndl, ig_opts, scope);
                lcl_blast.SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
                results = lcl_blast.Run();
            }

            /* TODO should we support archive format?
            if (fmt_args->ArchiveFormatRequested(args)) {
                CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(*query));
                formatter.WriteArchive(*qf, *opts_hndl, *results);
            } else {
            */
            
            BlastFormatter_PreFetchSequenceData(*results, scope);
            ITERATE(CSearchResultSet, result, *results) {
                CBlastFormat::SClone clone_info;
                SCloneNuc clone_nuc;
                AaMap* aa_info = new AaMap;
                CIgBlastResults &ig_result = *const_cast<CIgBlastResults *>
                        (dynamic_cast<const CIgBlastResults *>(&(**result)));
                formatter.PrintOneResultSet(ig_result, query, clone_info, !(ig_opts->m_IsProtein));
                total_input ++;
                if (!(ig_opts->m_IsProtein) && clone_info.na != NcbiEmptyString && 
                    clone_info.aa != NcbiEmptyString){
                    clone_nuc.na = clone_info.na;
                    clone_nuc.chain_type = clone_info.chain_type;
                    clone_nuc.v_gene = clone_info.v_gene;
                    clone_nuc.d_gene = clone_info.d_gene;
                    clone_nuc.j_gene = clone_info.j_gene;
               
                    SAaInfo* info = new SAaInfo;
                    info->count = 1;
                    info->seqid = clone_info.seqid;
                    info->all_seqid = clone_info.seqid;
                    info->min_identity = clone_info.identity;
                    info->max_identity = clone_info.identity;
                    
                    SAaStatus aa_status;
                    aa_status.aa = clone_info.aa;
                    aa_status.productive = clone_info.productive;
                    CloneInfo::iterator iter = m_Clone.find(clone_nuc); 
                    if (iter != m_Clone.end()) {
                        AaMap::iterator iter2 = iter->second->find(aa_status);
                        if (iter2 != (*iter->second).end()) {
                            if (info->min_identity < iter2->second->min_identity) {
                                iter2->second->min_identity = info->min_identity;
                            }
                            if (info->max_identity > iter2->second->max_identity) {
                                iter2->second->max_identity = info->max_identity;
                            }
                            
                            iter2->second->count  ++;
                            iter2->second->all_seqid = iter2->second->all_seqid + "," + info->seqid; 
                        } else {
                            (*iter->second).insert(AaMap::value_type(aa_status, info));
                        }
                        
                    } else {
                        (*aa_info)[aa_status] = info;
                        m_Clone.insert(CloneInfo::value_type(clone_nuc, aa_info));
                    }
                }
            }
        }
         
        //sort by clone abundance
        typedef vector<pair<const SCloneNuc*,  AaMap*> * > MapVec;
        MapVec map_vec; 
        ITERATE(CloneInfo, iter, m_Clone) {
            pair<const SCloneNuc*, AaMap*> *data = new pair<const SCloneNuc*, AaMap* > (&(iter->first), iter->second); 
            map_vec.push_back(data); 
        }
        stable_sort(map_vec.begin(), map_vec.end(), x_SortByCount);
        
        int total_elements = 0;
        ITERATE(MapVec, iter, map_vec) {
            ITERATE(AaMap, iter2, *((*iter)->second)){
                total_elements += iter2->second->count;
            }
        }

        CNcbiOstream* outfile = NULL;
            
        if (args.Exist("clonotype_out") && args["clonotype_out"] && args["clonotype_out"].AsString() != NcbiEmptyString) {
            outfile = &(args["clonotype_out"].AsOutputFile());
        } else {
            outfile = &m_CmdLineArgs->GetOutputStream();
        }
        *outfile << "\nTotal queries = " << total_input << ", queries with identifiable CDR3 = " <<  total_elements << endl << endl;
        
        
        if (!(ig_opts->m_IsProtein) && total_elements > 1) {
            *outfile << "\n" << "#Clonotype summary.  A particular clonotype includes any V(D)J rearrangements that have the same germline V(D)J gene segments, the same productive/non-productive status and the same CDR3 nucleotide as well as amino sequence (Those having the same CDR3 nucleotide but different amino acid sequence or productive/non-productive status due to frameshift in V or J gene are assigned to a different clonotype.  However, their clonotype identifers share the same prefix, for example, 6a, 6b).  Fields (tab-delimited) are clonotype identifier, representative query sequence name, count, frequency (%), CDR3 nucleotide sequence, CDR3 amino acid sequence, productive status, chain type, V gene, D gene, J gene\n" << endl;
            
            int count = 1; 
            string suffix = "abcdefghijklmnop";  //4x4 possibility = 16.  empty string included.
            
            ITERATE(MapVec, iter, map_vec) {
                if (count > args["num_clonotype"].AsInteger()) {
                    break;
                }

                int aa_count = 0;
                ITERATE(AaMap, iter2, *((*iter)->second)){
                    
                    double frequency = ((double) iter2->second->count)/total_elements*100;

                    string clone_name = NStr::IntToString(count);   
                    if ((*iter)->second->size() > 1) {
                        clone_name += suffix[aa_count];
                    }
                
                    *outfile << std::setprecision(3) << clone_name << "\t"
                        
                        <<iter2->second->seqid<<"\t"
                        <<iter2->second->count<<"\t"
                        <<frequency<<"\t"
                        <<(*iter)->first->na<<"\t"
                        <<iter2->first.aa<<"\t"
                        <<iter2->first.productive<<"\t"
                        <<(*iter)->first->chain_type<<"\t"
                        <<(*iter)->first->v_gene<<"\t"
                        <<(*iter)->first->d_gene<<"\t"
                        <<(*iter)->first->j_gene<<"\t"
                        <<endl;
                    aa_count ++;
                }
                count ++;
            }

            count = 1;
            *outfile << "\n#All query sequences grouped by clonotypes.  Fields (tab-delimited) are clonotype identifier, count, min similarity to top germline V gene (%), max similarity to top germline V gene (%), query sequence name (multiple names are separated by a comma if applicable)"<< endl << endl;
            ITERATE(MapVec, iter, map_vec) {
                if (count > args["num_clonotype"].AsInteger()) {
                    break;
                }
                int aa_count = 0;
                ITERATE(AaMap, iter2, *((*iter)->second)){
                    string clone_name = NStr::IntToString(count);   
                    if ((*iter)->second->size() > 1) {
                        clone_name += suffix[aa_count];
                    }
                    
                    *outfile << std::setprecision(3) 
                        << clone_name << "\t"
                        <<iter2->second->count<<"\t"
                        <<iter2->second->min_identity*100<<"\t"
                        <<iter2->second->max_identity*100<<"\t"
                        <<iter2->second->all_seqid<<"\t" << endl;
                         
                    aa_count ++;
                }
                count ++;
            }
        }
        
        ITERATE(CloneInfo, iter, m_Clone) {
            ITERATE(AaMap, iter2, *(iter->second)){
                delete iter2->second;
            }
            delete iter->second;
        }

        ITERATE(MapVec, iter, map_vec) {
            delete *iter;
        }
      
        formatter.PrintEpilog(opt);

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

    } CATCH_ALL(status)
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CIgBlastnApp().AppMain(argc, argv, 0, eDS_Default, "");
}
#endif /* SKIP_DOXYGEN_PROCESSING */
