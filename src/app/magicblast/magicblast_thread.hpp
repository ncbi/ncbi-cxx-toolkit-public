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
 * Author:  Greg Boratyn
 *
 * File description: 
 *   Class for MT MagicBlast searches
 *
 */


#ifndef APP_MAGICBLAST___MAGICBLAST_THREAD__HPP
#define APP_MAGICBLAST___MAGICBLAST_THREAD__HPP

#include <corelib/ncbithr.hpp>
#include <algo/blast/blastinput/blast_args.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/api/magicblast_options.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

class CMagicBlastThread : public CThread
{
public:
    CMagicBlastThread(CBlastInputOMF& input,
                      CRef<CMagicBlastOptionsHandle> options,
                      CRef<CMapperQueryOptionsArgs> query_opts,
                      CRef<CBlastDatabaseArgs> db_args,
                      CRef<CMapperFormattingArgs> fmt_args,
                      CNcbiOstream& out,
                      CNcbiOstream* unaligned_stream);

protected:
    virtual ~CMagicBlastThread() {}
    virtual void* Main(void);
    
private:
    CBlastInputOMF& m_Input;
    CRef<CMagicBlastOptionsHandle> m_Options;
    CRef<CMapperQueryOptionsArgs> m_QueryOptions;
    CRef<CBlastDatabaseArgs> m_DatabaseArgs;
    CRef<CMapperFormattingArgs> m_FormattingArgs;
    CNcbiOstream& m_OutStream;
    CNcbiOstream* m_OutUnalignedStream;
};


END_SCOPE(blast)
END_NCBI_SCOPE


#endif  /* APP_MAGICBLAST___MAGICBLAST_THREAD__HPP */
