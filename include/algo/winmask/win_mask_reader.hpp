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
 *   Header file for CWinMaskReader class.
 *
 */

#ifndef C_WIN_MASK_READER_H
#define C_WIN_MASK_READER_H

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistre.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Virtual base class for all input readers.
 **
 ** Each derived class should implement GetNextSequence()
 ** interface to supply new sequences to the user.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CWinMaskReader
{
public:

    /**
     **\brief Object constructor.
     **
     **\param newInputStream iostream object from which the
     **                      data will be read. The format
     **                      of the data is determined by
     **                      the implementation.
     **
     **/
    CWinMaskReader( CNcbiIstream & newInputStream )
        : input_stream( newInputStream ) {}

    /**
     **\brief Object destructor.
     **/
    virtual ~CWinMaskReader() {}

    /**
     **\brief Read the next sequence from the source stream.
     **
     **\return Pointer (reference counting) to the next biological
     **        sequence entry read from the data source. Returns
     **        CRef( CSeq_entry >( NULL ) if no more data is
     **        available.
     **
     **/
    virtual CRef< objects::CSeq_entry > GetNextSequence() = 0;

protected:

    /**\internal
     **\brief istream object to read data from.
     **
     **/
    CNcbiIstream & input_stream;
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

#endif

