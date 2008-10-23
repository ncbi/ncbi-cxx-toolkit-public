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
 *   Header file for CMaskWriter class.
 *
 */

#ifndef CMASK_WRITER_H
#define CMASK_WRITER_H

#include <corelib/ncbistre.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <algo/winmask/seq_masker.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief A base class for winmasker output writers.
 **
 ** This class provides support for the ability of winmasker
 ** to generate output in multiple formats. A derived class
 ** should be written for each supported format.
 **
 **/
class NCBI_XOBJREAD_EXPORT CMaskWriter
{
public:

    /**
     **\brief Object constructor.
     **
     **\param arg_os the ostream object used to output masked
     **              sequences
     **
     **/
    CMaskWriter( CNcbiOstream & arg_os ) : os( arg_os ) {}

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CMaskWriter() {}

    /**
     **\brief Output masked sequence data.
     **
     ** Each implementation of this abstract method will recieve the
     ** original sequence and the list of masked intervals and is
     ** responsible to formatting it masked sequence and printing
     ** it using the ostream object passed to the writer at 
     ** construction time.
     **
     **\param bsh the bioseq handle
     **\param mask the resulting list of masked intervals
     **\param parsed_id bioseq id was parsed by CMaskReader.
     **
     **/
    virtual void Print( objects::CBioseq_Handle & bsh,
                        const CSeqMasker::TMaskList & mask,
                        bool parsed_id = false ) = 0;

protected:

    /**
     **\brief Output of the sequence id.
     **
     ** By default prints id in fasta format but implementations
     ** can redefine it as necessary and use from within Print()
     ** method.
     **
     **\param bsh the bioseq handle
     **\param parsed_id bioseq id was parsed by CMaskReader.
     **
     **/
    virtual void PrintId( objects::CBioseq_Handle& bsh, bool parsed_id );

    /**
     **\brief the standard C++ ostream object 
     **
     ** Determines the destination of the output.
     **
     **/
    CNcbiOstream & os;
};

END_NCBI_SCOPE

#endif
