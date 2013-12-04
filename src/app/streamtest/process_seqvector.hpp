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
        m_gap_mode = args["gap-mode"].AsString();

        m_debug = args["debug"];

        m_timer = CStopWatch();
        m_timer.Start();
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

    enum EGapType {
        eGT_one_dash, ///< A single dash, followed by a line break.
        eGT_dashes,   ///< Multiple inline dashes.
        eGT_letters,  ///< Multiple inline Ns or Xs as appropriate (default).
        eGT_count     ///< >?N or >?unk100, as appropriate.
    };

    //  ------------------------------------------------------------------------
    void x_FsaSeqIdWrite(const CBioseq& bioseq)
    //  ------------------------------------------------------------------------
    {
        string gi_string;
        string accn_string;

        FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, bioseq) {
            const CSeq_id& sid = **sid_itr;
            TSEQID_CHOICE chs = sid.Which();
            switch (chs) {
                case NCBI_SEQID(Gi):
                {
                    const string str = sid.AsFastaString();
                    gi_string = str;
                    break;
                }
                default:
                    break;
            }
        }

        FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, bioseq) {
            const CSeq_id& sid = **sid_itr;
            TSEQID_CHOICE chs = sid.Which();
            switch (chs) {
                case NCBI_SEQID(Other):
                case NCBI_SEQID(Genbank):
                case NCBI_SEQID(Embl):
                case NCBI_SEQID(Ddbj):
                case NCBI_SEQID(Tpg):
                case NCBI_SEQID(Tpe):
                case NCBI_SEQID(Tpd):
                {
                    const string str = sid.AsFastaString();
                    accn_string = str;
                    break;
                }
                 default:
                   break;
            }
        }

        if (gi_string.empty() || accn_string.empty()) {
            CSeq_id::WriteAsFasta (*m_out, bioseq);
        } else {
            *m_out << gi_string << "|" << accn_string;
        }
    }

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        try {
            CDeflineGenerator gen (m_topseh);

            VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bit, *m_entry) {
                const CBioseq& bioseq = *bit;
                // !!! NOTE CALL TO OBJECT MANAGER !!!
                const CBioseq_Handle& bsh = m_scope->GetBioseqHandle (bioseq);

                const string& title = gen.GenerateDefline (bsh, 0);

                *m_out << ">";
                x_FsaSeqIdWrite (bioseq);
                *m_out << " ";
                *m_out << title << endl;

                CSeqVector sv = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

                EGapType gap_type = eGT_letters;
                if ( m_gap_mode == "one-dash" ) {
                    gap_type = eGT_one_dash;
                } else if ( m_gap_mode == "dashes" ) {
                    gap_type = eGT_dashes;
                } else if ( m_gap_mode == "letters" ) {
                    gap_type = eGT_letters;
                } else if ( m_gap_mode == "count" ) {
                    gap_type = eGT_count;
                }

                int pos = 0;
                for ( CSeqVector_CI sv_iter(sv); (sv_iter); ++sv_iter ) {
                    if ( gap_type != eGT_letters && sv_iter.GetGapSizeForward() > 0 ) {
                        int gap_len = sv_iter.SkipGap();
                        if ( gap_type == eGT_one_dash ) {
                            *m_out << "-\n";
                            pos = 0;
                        } else if ( gap_type == eGT_dashes ) {
                            while ( gap_len > 0 ) {
                                *m_out << "-";
                                pos++;
                                if ( pos >= 60 ) {
                                    pos = 0;
                                    *m_out << endl;
                                }
                                gap_len--;
                            }
                        }
                        // *m_out << endl << "Gap length " << NStr::IntToString (gap_len) << endl;
                        // pos = 0;
                    } else {
                        CSeqVector::TResidue res = *sv_iter;
                        *m_out << res;
                        pos++;
                        if ( pos >= 60 ) {
                            pos = 0;
                            *m_out << endl;
                        }
                    }
                }


                size_t total, resident, shared;
                if ( m_debug && GetMemoryUsage ( &total, &resident, &shared) ) {
                    *m_out << "Tot " << total << ", Res " << resident;
                    *m_out << ", Shr " << shared << ", Sec " << m_timer.Restart() << " - ";
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
    string m_gap_mode;
    bool m_debug;
    CStopWatch m_timer;
};

#endif
