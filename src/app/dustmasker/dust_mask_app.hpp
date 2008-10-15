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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Header file for CDustMaskApplication class.
 *
 */

#ifndef C_DUST_MASK_APPLICATION_H
#define C_DUST_MASK_APPLICATION_H

#include <corelib/ncbiapp.hpp>

#include <objmgr/seq_entry_handle.hpp>

#include <algo/dustmask/symdust.hpp>

#include <objtools/seqmasks_io/mask_writer.hpp>

BEGIN_NCBI_SCOPE

class CDustMaskApplication : public CNcbiApplication
{
public:
    /// Application constructor
    CDustMaskApplication() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(1, 0, 0);
        SetFullVersion(version);
    }

    static const char * const USAGE_LINE;
    virtual void Init(void);
    virtual int Run (void);

private:
    CMaskWriter* x_GetWriter();

    typedef CSymDustMasker duster_type;
    typedef duster_type::TMaskList::const_iterator it_type;
#if 0
    typedef void (*out_handler_type)(
            CNcbiOstream *, 
            const objects::CBioseq_Handle &,
            const duster_type::TMaskList & );

    static void interval_out_handler( 
            CNcbiOstream * output_stream, 
            const objects::CBioseq_Handle & bsh, 
            const duster_type::TMaskList & res );
    static void acclist_out_handler( 
            CNcbiOstream * output_stream,
            const objects::CBioseq_Handle & bsh,
            const duster_type::TMaskList & res );
    static void fasta_out_handler( 
            CNcbiOstream * output_stream,
            const objects::CBioseq_Handle & bsh,
            const duster_type::TMaskList & res );
    static void write_normal(
            CNcbiOstream * output_stream,
            const objects::CSeqVector & data,
            TSeqPos & start, TSeqPos & stop );
    static void write_lowerc(
            CNcbiOstream * output_stream,
            const objects::CSeqVector & data,
            TSeqPos & start, TSeqPos & stop );
#endif
};

END_NCBI_SCOPE

#endif
