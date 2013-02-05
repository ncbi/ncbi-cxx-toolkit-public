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

#ifndef __process_title__hpp__
#define __process_title__hpp__

//  ============================================================================
class CTitleProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CTitleProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 )
        , m_flags( 0 )
    {};

    //  ------------------------------------------------------------------------
    ~CTitleProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::ProcessInitialize( args );

        /*
        m_out = new CFastaOstream( args["o"].AsOutputFile() );
        */
        m_out = args["o"] ? &(args["o"].AsOutputFile()) : &cout;

        string options = args["options"].AsString();
        if ( options == "ignore_existing" ) {
            m_flags |= fGetTitle_Reconstruct;
        }
    };

    //  ------------------------------------------------------------------------
    void ProcessFinalize()
    //  ------------------------------------------------------------------------
    {
        /*
        delete m_out;
        */
    }

    //  ------------------------------------------------------------------------
    virtual void SeqEntryInitialize(
        CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::SeqEntryInitialize( se );
    };

    //  ------------------------------------------------------------------------
    void x_TitleSeqIdWrite(const CBioseq& bioseq)
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
            VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bit, *m_entry) {
                const CBioseq& bioseq = *bit;
                // !!! NOTE CALL TO OBJECT MANAGER !!!
                const CBioseq_Handle& hnd = m_scope->GetBioseqHandle (bioseq);
                /*
                m_out->WriteTitle( bioseq, 0, true );
                */
                string title = GetTitle (hnd, m_flags);  /* NCBI_FAKE_WARNING */
                *m_out << ">";
                x_TitleSeqIdWrite (bioseq);
                *m_out << " ";
                *m_out << title << endl;
                ++m_objectcount;
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

protected:
    /*
    CFastaOstream* m_out;
    */
    CNcbiOstream* m_out;
    TGetTitleFlags m_flags;
};

#endif
