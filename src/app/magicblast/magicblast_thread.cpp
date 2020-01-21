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
 * Authors:  Greg Boratyn
 *    Class for MT MagicBlast searches
 *
 */


#include <ncbi_pch.hpp>
#include <algo/blast/api/magicblast.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include "magicblast_util.hpp"
#include "magicblast_thread.hpp"
#include <sstream>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

CFastMutex input_mutex;
CFastMutex output_mutex;

CMagicBlastThread::CMagicBlastThread(CBlastInputOMF& input,
                                     CRef<CMagicBlastOptionsHandle> options,
                                     CRef<CMapperQueryOptionsArgs> query_opts,
                                     CRef<CBlastDatabaseArgs> db_args,
                                     CRef<CMapperFormattingArgs> fmt_args,
                                     CNcbiOstream& out,
                                     CNcbiOstream* unaligned_stream)
 : m_Input(input),
   m_Options(options),
   m_QueryOptions(query_opts),
   m_DatabaseArgs(db_args),
   m_FormattingArgs(fmt_args),
   m_OutStream(out),
   m_OutUnalignedStream(unaligned_stream)
{}

void* CMagicBlastThread::Main(void)
{
    const bool kTrimReadIdForSAM =
            m_QueryOptions->IsPaired() && m_FormattingArgs->TrimReadIds();

    const bool kPrintUnaligned = m_FormattingArgs->PrintUnaligned();
    const bool kNoDiscordant = m_FormattingArgs->NoDiscordant();
    const bool kPrintMdTag = m_FormattingArgs->PrintMdTag();

    // Is either strand-specificity flag set? (mutually exclusive)
    const bool only_specific = m_FormattingArgs->SelectOnlyStrandSpecific();
    const bool fr = m_FormattingArgs->SelectFwdRev();
    const bool rf = m_FormattingArgs->SelectRevFwd();

    // One or both MUST be false. (enforced by command-line processing)
    _ASSERT(fr == false  ||  rf == false);
    // "-fr" and "-rf" flags can only be used without
    // "-only_strand_specific" for SAM output.  Return an error if this
    // condition is not met.
    if (m_FormattingArgs->GetFormattedOutputChoice() != CFormattingArgs::eSAM) {
        if (!only_specific  &&  (fr || rf)) {
            NCBI_THROW(CArgException, eNoValue,
                       "-fr or -rf can only be used with SAM format."
                       " Use -oufmt sam option.");
        }
    }
    // "-only_strand_specific" without "-fr" or "-rf" (or in the future,
    // "-f" or "-r") is not meaningful.
    // FIXME: should this be a warning?
    if (only_specific  &&  !(fr || rf)) {
        NCBI_THROW(CArgException, eNoValue,
                   "-only_strand_specific without either -fr or -rf "
                   "is not valid.");
    }

    E_StrandSpecificity kStrandSpecific = eNonSpecific;
    if (fr) {
        kStrandSpecific = eFwdRev;
    } else if (rf) {
        kStrandSpecific = eRevFwd;
    }

    bool isDone = false;
    while (!isDone) {
        CRef<CBioseq_set> query_batch(new CBioseq_set);
        const string kDbName = m_DatabaseArgs->GetDatabaseName();

        {
            CFastMutexGuard guard(input_mutex);
            isDone = m_Input.End();
            if (isDone) {
                break;
            }

            m_Input.GetNextSeqBatch(*query_batch);
        }

        if (query_batch->IsSetSeq_set() &&
            !query_batch->GetSeq_set().empty()) {

            CRef<IQueryFactory> queries(
                                 new CObjMgrFree_QueryFactory(query_batch));

            CRef<CMagicBlastResultSet> results;
            CRef<CSearchDatabase> search_db;
            CRef<CLocalDbAdapter> thread_db_adapter;

            if (!kDbName.empty()) {

                search_db.Reset(new CSearchDatabase(kDbName,
                                       CSearchDatabase::eBlastDbIsNucleotide));

                CRef<CSeqDBGiList> gilist =
                    m_DatabaseArgs->GetSearchDatabase()->GetGiList();

                CRef<CSeqDBGiList> neg_gilist =
                    m_DatabaseArgs->GetSearchDatabase()->GetNegativeGiList();


                if (gilist.NotEmpty()) {
                    search_db->SetGiList(gilist.GetNonNullPointer());
                }
                else if (neg_gilist.NotEmpty()) {
                    search_db->SetNegativeGiList(
                                          neg_gilist.GetNonNullPointer());
                }

                // this must be the last operation on searh_db, because
                // CSearchDatabase::GetSeqDb initializes CSeqDB with
                // whatever information it currently has
                search_db->GetSeqDb()->SetNumberOfThreads(1, true);

                thread_db_adapter.Reset(new CLocalDbAdapter(*search_db));
            }
            else {
                CRef<CScope> scope;
                scope.Reset(new CScope(*CObjectManager::GetInstance()));
                CRef<IQueryFactory> subjects;
                subjects = m_DatabaseArgs->GetSubjects(scope);
                thread_db_adapter.Reset(
                              new CLocalDbAdapter(subjects, m_Options, true));
            }

            // do mapping
            CMagicBlast magicblast(queries, thread_db_adapter, m_Options);
            results = magicblast.RunEx();

            // use a single stream when reporting to one file, or two streams
            // when reporting unaligned reads separately
            ostringstream ostr;
            ostringstream the_unaligned_ostr;
            ostringstream& unaligned_ostr = 
                m_OutUnalignedStream ? the_unaligned_ostr : ostr;

            // format ouput
            if (m_FormattingArgs->GetFormattedOutputChoice() ==
                CFormattingArgs::eTabular) {

                CRef<ILocalQueryData> query_data =
                    queries->MakeLocalQueryData(&m_Options->GetOptions());

                PrintTabular(ostr,
                             unaligned_ostr,
                             m_FormattingArgs->GetUnalignedOutputFormat(),
                             *results,
                             *query_batch,
                             m_Options->GetPaired(),
                             /*thread_batch_number*/ 1,
                             kTrimReadIdForSAM,
                             kPrintUnaligned,
                             kNoDiscordant);
            }
            else if (m_FormattingArgs->GetFormattedOutputChoice() ==
                     CFormattingArgs::eAsnText) {

                PrintASN1(ostr, *query_batch,
                          *results->GetFlatResults(kNoDiscordant));
            }
            else {
                
                CRef<ILocalQueryData> query_data =
                    queries->MakeLocalQueryData(&m_Options->GetOptions());

                PrintSAM(ostr,
                         unaligned_ostr,
                         m_FormattingArgs->GetUnalignedOutputFormat(),
                         *results,
                         *query_batch,
                         query_data->GetQueryInfo(),
                         m_Options->GetSpliceAlignments(),
                         /*thread_batch_number*/ 1,
                         kTrimReadIdForSAM,
                         kPrintUnaligned,
                         kNoDiscordant,
                         kStrandSpecific,
                         only_specific,
                         kPrintMdTag);
            }


            // write formatted ouput to stream
            {
                CFastMutexGuard guard(output_mutex);
                m_OutStream << ostr.str();
                // flush string
                ostr.str("");

                // report unaligned reads to a separate stream if requested 
                if (m_OutUnalignedStream) {
                    *m_OutUnalignedStream << unaligned_ostr.str();
                    unaligned_ostr.str("");
                }
            }

            query_batch.Reset();
        }
    }
 
    return nullptr;
}
