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
 *   Header file for CMaskWriterTabular class.
 *
 */

#ifndef CMASK_WRITER_ACCLIST_H
#define CMASK_WRITER_ACCLIST_H

#include <objtools/seqmasks_io/mask_writer.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Output filter to print masked sequences as sets of
 **       intervals one per line.
 **
 ** Masking data for each new sequence in the file starts with
 ** a fasta style id and is followed by a range indicating the
 ** masked sequence starting at position 'start' and ending
 ** at position 'end', each field is separated by a tab character
 ** (i.e.: [id]\t[start]\t[end])
 **
 **/
class NCBI_XOBJREAD_EXPORT CMaskWriterTabular : public CMaskWriter
{
public:

    /**
     **\brief Object constructor.
     **
     **\param arg_os output stream used to initialize the
     **              base class instance
     **
     **/
    CMaskWriterTabular( CNcbiOstream & arg_os ) 
        : CMaskWriter( arg_os ) {}

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CMaskWriterTabular() {}

    /**
     **\brief Send the masking data to the output stream.
     **
     **\param bsh the bioseq handle
     **\param mask the resulting list of masked intervals
     **\param parsed_id bioseq id was parsed by CMaskReader.
     **
     **/
    virtual void Print( objects::CBioseq_Handle& bsh,
                        const TMaskList & mask,
                        bool parsed_id = false );
};

END_NCBI_SCOPE

#endif
