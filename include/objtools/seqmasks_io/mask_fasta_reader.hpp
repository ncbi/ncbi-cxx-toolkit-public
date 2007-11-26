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
 *   Header file for CMaskFastaReader class.
 *
 */

#ifndef CMASK_FASTA_READER_H
#define CMASK_FASTA_READER_H

#include <objtools/seqmasks_io/mask_reader.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Class for reading sequences from fasta files.
 **/
class NCBI_XOBJREAD_EXPORT CMaskFastaReader : public CMaskReader
{
public:

    /**
     **\brief Object constructor.
     **
     **\param newInputStream input stream to read data from.
     **
     **/
    CMaskFastaReader( CNcbiIstream & newInputStream, bool is_nucl = true )
        : CMaskReader( newInputStream ), is_nucleotide_(is_nucl) {}

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CMaskFastaReader() {}

    /**
     **\brief Read next sequence from fasta stream.
     **
     **\return pointer (reference counting) to the object 
     **        containing information about the sequence, or
     **        CRef( NULL ) if end of file is reached.
     **
     **/
    virtual CRef< objects::CSeq_entry > GetNextSequence();

private:
    bool is_nucleotide_; /**< This object is reading nucleotide sequences */
};

END_NCBI_SCOPE

#endif
