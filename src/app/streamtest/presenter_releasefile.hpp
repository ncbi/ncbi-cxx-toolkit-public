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
* Author:
*
* File Description:
*
* ===========================================================================
*/

#ifndef __presenter_releasefile_hpp__
#define __presenter_releasefile_hpp__

//  ============================================================================
class CReleaseFilePresenter
//  ============================================================================
    : public CSeqEntryPresenter
    , public CGBReleaseFile::ISeqEntryHandler
{
public:
    //  ------------------------------------------------------------------------
    CReleaseFilePresenter()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    virtual void Initialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryPresenter::Initialize( args );  
        m_is.reset( 
            CObjectIStream::Open(
                (args["binary"] ? eSerial_AsnBinary : eSerial_AsnText), 
                args["i"].AsInputFile() ) );
    };

    //  ------------------------------------------------------------------------
    virtual void Run(
        CSeqEntryProcess* process )
    //  ------------------------------------------------------------------------
    {   
        CSeqEntryPresenter::Run( process );

        CGBReleaseFile in(*m_is.release());
        in.RegisterHandler( this );
        in.Read();  // HandleSeqEntry will be called from this function

        if (m_report_final) {
            FinalReport();
        }
    };

    //  ------------------------------------------------------------------------
    bool HandleSeqEntry(CRef<CSeq_entry>& se) {
    //  ------------------------------------------------------------------------
        if ( m_process ) {
            m_process->SeqEntryInitialize( se );

            m_stopwatch.Restart();

            m_process->SeqEntryProcess( se );

            if ( m_stopwatch.IsRunning() ) {
                double elapsed = m_stopwatch.Elapsed();
                m_stopwatch.Stop();
                m_total_time += elapsed;
                m_diff_time += elapsed;
            }

            if ( m_report_interval && 
                ! (m_process->GetObjectCount() % m_report_interval) ) 
            {
                ProgressReport();
            }
            m_process->SeqEntryFinalize( se );
        }
        return true;
    };

protected:       
    auto_ptr<CObjectIStream> m_is;
};

#endif
