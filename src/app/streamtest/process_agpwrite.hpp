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

#ifndef __process_agpwrite__hpp__
#define __process_agpwrite__hpp__

//  ============================================================================
class CAgpwriteProcess
//  ============================================================================
    : public CScopedProcess
{
public:

    //  ------------------------------------------------------------------------
    class CTestCompIdMapper : public CAgpWriteComponentIdMapper
    //  ------------------------------------------------------------------------
    {
    public:
        void do_map( string &in_out_component_id ) {
            in_out_component_id = "PREFIX" + in_out_component_id + "SUFFIX";
        }
    };

    //  ------------------------------------------------------------------------
    CAgpwriteProcess( const string & option )
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 )
    {
        if( option.empty() ) {
            // do nothing
        } else if( option == "map" ) {
            comp_id_mapper.reset( new CTestCompIdMapper );
        } else {
            throw runtime_error("unknown process option: " + option );
        }
    }

    //  ------------------------------------------------------------------------
    ~CAgpwriteProcess()
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
                if (bioseq.IsNa()) {
                    const CBioseq_Handle& bs = (*m_scope).GetBioseqHandle (bioseq);
                    AgpWrite( *m_out, bs, "chr1", vector<char>(), comp_id_mapper.get() );
                }
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

protected:
    CNcbiOstream* m_out;
    auto_ptr<CTestCompIdMapper> comp_id_mapper;
};

#endif
