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

#ifndef __process_seqvector__hpp__
#define __process_seqvector__hpp__

//  ============================================================================
class CSeqVectorProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CSeqVectorProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 )
    {};

    //  ------------------------------------------------------------------------
    ~CSeqVectorProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::ProcessInitialize( args );

        m_out = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    };

    //  ------------------------------------------------------------------------
    void ProcessFinalize()
    //  ------------------------------------------------------------------------
    {
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
            VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bit, *m_entry) {
                const CBioseq& bioseq = *bit;
                // !!! NOTE CALL TO OBJECT MANAGER !!!
                const CBioseq_Handle& bsh = m_scope->GetBioseqHandle (bioseq);

                CSeqVector sv = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

                int pos = 0;
                for ( CSeqVector_CI sv_iter(sv); (sv_iter); ++sv_iter ) {
                    CSeqVector::TResidue res = *sv_iter;
                    *m_out << res;
                    pos++;
                    if ( pos >= 60 ) {
                        pos = 0;
                        *m_out << endl;
                    }
                }

                *m_out << endl;
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

protected:
    CNcbiOstream* m_out;
};

#endif
