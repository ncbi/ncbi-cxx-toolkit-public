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

#ifndef __processor_releasefile_hpp__
#define __processor_releasefile_hpp__

//  ============================================================================
class CReleaseFileProcessor
//  ============================================================================
    : public CBioseqProcessor
    , public CReadObjectHook
{
public:
    //  ------------------------------------------------------------------------
    CReleaseFileProcessor()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    virtual void Initialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CBioseqProcessor::Initialize( args );  
        m_is.reset( 
            CObjectIStream::Open(eSerial_AsnBinary, args["i"].AsInputFile() ) );
    };

    //  ------------------------------------------------------------------------
    virtual void Run(
        CBioseqProcess* process )
    //  ------------------------------------------------------------------------
    {   
        CBioseqProcessor::Run( process );

        CObjectTypeInfo bsi = CType< CBioseq >();
        bsi.SetLocalReadHook( *m_is, this );
        while( true ) {
            try {
                CBioseq_set bss;
                m_is->Read( &bss, bss.GetThisTypeInfo() );
            }
            catch( ... ) {
                break;
            }
        }
        if (m_report_final) {
            FinalReport();
        }
    };

    //  ------------------------------------------------------------------------
    virtual void ReadObject(
        CObjectIStream& is,
        const CObjectInfo& info )
    //  ------------------------------------------------------------------------
    {
        try {
            CRef<CBioseq> bs( new CBioseq );
            info.GetTypeInfo()->DefaultReadData( is, bs );
            m_stopwatch.Restart();
            if ( m_process ) {
                m_process->Process( *bs );
            }
            ++m_objectcount;
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing bioseq: " << e.what());
        }

        if ( m_stopwatch.IsRunning() ) {
            double elapsed = m_stopwatch.Elapsed();
            m_stopwatch.Stop();
            m_total_time += elapsed;
            m_diff_time += elapsed;
        }

        if ( m_report_interval && ! (GetObjectCount() % m_report_interval) ) {
            ProgressReport();
        }
    };

protected:       
    auto_ptr<CObjectIStream> m_is;
};

#endif
