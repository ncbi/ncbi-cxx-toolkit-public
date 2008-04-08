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
 * Author:  Christiam Camacho
 *
 * File Description:
 *   Header file for CMaskWriterBlastDbMaskInfo class.
 *
 */

#ifndef CMASK_WRITER_BLASTDB_MASKINFO_H
#define CMASK_WRITER_BLASTDB_MASKINFO_H

#include <objtools/seqmasks_io/mask_writer.hpp>
#include <objects/blastdb/Blast_db_mask_info.hpp>
#include <objects/blastdb/Blast_mask_list.hpp>
#include <objects/blastdb/Blast_filter_program.hpp>
#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE

/**
 ** Output filter to print masked sequence locations as Blast-db-mask-info
 ** objects.
 **/
class NCBI_XOBJREAD_EXPORT CMaskWriterBlastDbMaskInfo : public CMaskWriter
{
public:

    /**
     **\brief Object constructor.
     **
     **\param arg_os output stream used to initialize the
     **              base class instance
     **\param format format for the Seq-locs (as defined in 
     **              mask_cmdline_args.hpp). \sa kOutputFormats
     **\param algo_id Identifier for this algorithm
     **\param filt_program Filtering program being used
     **\param algo_options algorithm options being used
     **
     **/
    CMaskWriterBlastDbMaskInfo( CNcbiOstream & arg_os, 
                                const string & format,
                                int algo_id,
                                objects::EBlast_filter_program filt_program,
                                const string & algo_options
                                );

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CMaskWriterBlastDbMaskInfo();

    /**
     **\brief Send the masking data to the output stream.
     **
     **\param bsh the bioseq handle
     **\param mask the resulting list of masked intervals
     **\param match_id use CSeq_id objects instead of strings
     **
     **/
    virtual void Print( objects::CBioseq_Handle& bsh,
                        const CSeqMasker::TMaskList & mask,
                        bool match_id = false );

    void Print( int gi, const CSeqMasker::TMaskList & mask );
    void Print( const objects::CSeq_id& id, 
                const CSeqMasker::TMaskList & mask );

protected:
    /// The data type objects of this class will print
    CRef<objects::CBlast_db_mask_info> m_BlastDbMaskInfo;
    /// convenience typedef
    typedef vector< CRef<objects::CBlast_mask_list> > TBlastMaskLists;
    /// vector of list of masks
    TBlastMaskLists m_ListOfMasks;
    /// Output format for data types above
    ESerialDataFormat m_OutputFormat;

private:
    /// Consolidate the list of masks so that each element contains the masks
    /// for multiple OIDs, this should make reading the masks more efficient
    /// and reduce the storage requirements
    void x_ConsolidateListOfMasks();
};

/** Builds an algorithm options string for the filtering applications
 * (segmasker, dustmasker) by examining the command line arguments specified
 * @param args Command line arguments [in]
 * @return string with algorithm parameters or empty string is algorithm cannot
 * be determined
 */
NCBI_XOBJREAD_EXPORT 
string BuildAlgorithmParametersString(const CArgs& args);

END_NCBI_SCOPE

#endif
