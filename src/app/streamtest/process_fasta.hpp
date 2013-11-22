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

#ifndef __process_fasta__hpp__
#define __process_fasta__hpp__

//  ============================================================================
class CFastaProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CFastaProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 )
    {};

    //  ------------------------------------------------------------------------
    ~CFastaProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::ProcessInitialize( args );

        m_out = new CFastaOstream( args["o"].AsOutputFile() );
        m_gap_mode = args["gap-mode"].AsString();
        m_show_mods = args["show-mods"];
    };

    //  ------------------------------------------------------------------------
    void ProcessFinalize()
    //  ------------------------------------------------------------------------
    {
        delete m_out;
    }

    //  ------------------------------------------------------------------------
    virtual void SeqEntryInitialize(
        CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::SeqEntryInitialize( se );
    };

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        try {
            m_out->SetAllFlags( CFastaOstream::fInstantiateGaps |
                                CFastaOstream::fAssembleParts |
                                CFastaOstream::fNoDupCheck |
                                CFastaOstream::fKeepGTSigns |
                                CFastaOstream::fNoExpensiveOps );
            if ( m_show_mods ) {
                m_out->SetFlag( CFastaOstream::fShowModifiers );
            }

            CFastaOstream::EGapMode gap_mode = CFastaOstream::eGM_letters;
            if ( m_gap_mode == "one-dash" ) {
                gap_mode = CFastaOstream::eGM_one_dash;
            } else if ( m_gap_mode == "dashes" ) {
                gap_mode = CFastaOstream::eGM_dashes;
            } else if ( m_gap_mode == "letters" ) {
                gap_mode = CFastaOstream::eGM_letters;
            } else if ( m_gap_mode == "count" ) {
                gap_mode = CFastaOstream::eGM_count;
            }
            m_out->SetGapMode( gap_mode );

            // m_out->Write( m_topseh );
            VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bit, *m_entry) {
                const CBioseq& bioseq = *bit;
                // m_out->Write( bioseq, 0, true );
                const CBioseq_Handle& bsh = (*m_scope).GetBioseqHandle (bioseq);
                m_out->Write( bsh );
                ++m_objectcount;
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

protected:
    CFastaOstream* m_out;
    string m_gap_mode;
    bool m_show_mods;
};

#endif
