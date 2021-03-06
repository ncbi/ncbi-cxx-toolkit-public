/*
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

#ifndef __process_cleanup__hpp__
#define __process_cleanup__hpp__

//  ============================================================================
class CCleanupProcess
//  ============================================================================
    : public CSeqEntryProcess
{
public:
    //  ------------------------------------------------------------------------
    CCleanupProcess()
    //  ------------------------------------------------------------------------
        : CSeqEntryProcess()
        , m_out( 0 )
    {};

    //  ------------------------------------------------------------------------
    ~CCleanupProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryProcess::ProcessInitialize( args );

        m_out = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    };

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        try {
            CCleanup cleanup;
            cleanup.BasicCleanup( *m_entry, CCleanup::eClean_SyncGenCodes );

            *m_out << MSerial_AsnText << *m_entry << endl;
        }
        catch (CException& e) {
            ERR_POST(Error << "error processing seqentry: " << e.what());
        }
    };

protected:
    CNcbiOstream* m_out;
};

#endif
