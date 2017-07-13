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
 * Author: Tom Madden
 *
 * File Description:
 *   Class to MT KMER BLAST searches
 *
 */

#include <objmgr/object_manager.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <math.h>

#include <objtools/readers/fasta.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <algo/blast/proteinkmer/blastkmer.hpp>
#include <algo/blast/proteinkmer/blastkmerresults.hpp>
#include <algo/blast/proteinkmer/blastkmeroptions.hpp>
#include <algo/blast/proteinkmer/kblastapi.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

/////////////////////////////////////////////////////////////////////////////
/// Threading class for BlastKmer searches.
/// Each thread runs a batch of input sequences
/// through KMER lookup and then through BLAST.
////////////
class CBlastKmerThread : public CThread
{
public:
    CBlastKmerThread(CBlastInput& input, 
       CRef<CBlastDatabaseArgs> db_args,
       CRef<CBlastpKmerOptionsHandle> opts_handle,
       CRef<CFormattingArgs> fmtArgs,
       CNcbiOstream& outfile,
       CBlastAsyncFormatThread* formatThr,
       Boolean believeQuery=false)
    : m_Input(input), 
      m_DbArgs(db_args),
      m_BlastOptsHandle(opts_handle),
      m_FormattingArgs(fmtArgs),
      m_OutFile(outfile),
      m_FormattingThr(formatThr),
      m_BelieveQuery(believeQuery)
   {
	
   }

protected:
    virtual ~CBlastKmerThread(void);

    virtual void* Main(void);
private:

    CBlastInput& m_Input;

    CRef<CBlastDatabaseArgs> m_DbArgs;

    CRef<CBlastpKmerOptionsHandle> m_BlastOptsHandle;

    CRef<CFormattingArgs> m_FormattingArgs;

    CNcbiOstream& m_OutFile;

    CBlastAsyncFormatThread* m_FormattingThr;

    Boolean m_BelieveQuery;
    // FIXME: needed?
    CFastMutex m_Guard;
};

