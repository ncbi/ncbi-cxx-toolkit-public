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

#ifndef __presenter_hpp__
#define __presenter_hpp__

//  ============================================================================
class CSeqEntryPresenter
//  ============================================================================
{
public:
    //  ------------------------------------------------------------------------
    CSeqEntryPresenter()
    //  ------------------------------------------------------------------------
        : m_process( 0 )
        , m_run_time( 0 )
        , m_process_time( 0 )
        , m_diff_time( 0 )
        , m_report_final( true )
        , m_report_interval( 1000 )
        , m_repeatitions( 1 )
    {};

    //  ------------------------------------------------------------------------
    virtual ~CSeqEntryPresenter()
    //  ------------------------------------------------------------------------
    {};

    //  ------------------------------------------------------------------------
    virtual void Initialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        m_report_final = args["rf"];
        m_report_interval = args["ri"].AsInteger();
        m_repeatitions = args["count"].AsInteger();

        m_meter.Restart();
    };

    //  ------------------------------------------------------------------------
    virtual void Run(
        CSeqEntryProcess* process )
    //  ------------------------------------------------------------------------
    {
        m_process = process;
    };

    //  ------------------------------------------------------------------------
    virtual void Finalize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        m_run_time = m_meter.Elapsed();
        m_meter.Stop();

        if (m_report_final) {
            FinalReport();
        }
    };

    //  ------------------------------------------------------------------------
    double GetDiffTime() const
    //  ------------------------------------------------------------------------
    {
        return m_diff_time;
    };

    //  ------------------------------------------------------------------------
    double GetProcessTime() const
    //  ------------------------------------------------------------------------
    {
        return m_process_time;
    };

    //  ------------------------------------------------------------------------
    double GetRunTime() const
    //  ------------------------------------------------------------------------
    {
        return m_run_time;
    };

    //  ------------------------------------------------------------------------
    virtual void Process(
        CRef< CSeq_entry >& se )
    //  ------------------------------------------------------------------------
    {
        try {
            if ( m_process ) {
                m_process->SeqEntryInitialize( se );

                m_stopwatch.Restart();

                for ( int i=0; i < m_repeatitions; ++i ) {
                    m_process->SeqEntryProcess();
                }
                
                if ( m_stopwatch.IsRunning() ) {
                    double elapsed = m_stopwatch.Elapsed();
                    m_stopwatch.Stop();
                    m_process_time += elapsed;
                    m_diff_time += elapsed;
                }

                m_process->SeqEntryFinalize();
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
        if ( m_report_interval && 
            ! (m_process->GetObjectCount() % m_report_interval) ) 
        {
            ProgressReport();
        }
    };

protected:
    //  ------------------------------------------------------------------------
    void ProgressReport()
    //  ------------------------------------------------------------------------
    {
        cerr << "-----------------------------------------------------" << endl;
        cerr << "Current object count   : " << m_process->GetObjectCount() 
             << endl;
        cerr << "Time since last report : " << GetDiffTime() 
             << " secs" << endl;
        m_diff_time = 0;
    };

    //  ------------------------------------------------------------------------
    void FinalReport()
    //  ------------------------------------------------------------------------
    {
        cerr << "=====================================================" << endl;
        cerr << "Final object count     : " << m_process->GetObjectCount() 
             << endl;
        cerr << "Internal process time  : " << GetProcessTime() 
             << " secs" << endl;
        cerr << "Total running time     : " << GetRunTime() 
             << " secs" << endl;
        cerr << "=====================================================" << endl;
    };

protected:
    CSeqEntryProcess* m_process;
    CStopWatch m_stopwatch;
    CStopWatch m_meter;
    double m_run_time;
    double m_process_time;
    double m_diff_time;
    bool m_report_final;
    int m_report_interval;
    int m_repeatitions;
};

#endif /* __presenter_hpp__ */

