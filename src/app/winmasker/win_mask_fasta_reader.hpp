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
 *   Header file for CWinMaskFastaReader class.
 *
 */

#ifndef C_WIN_MASK_FASTA_READER_H
#define C_WIN_MASK_FASTA_READER_H

#include "win_mask_reader.hpp"

BEGIN_NCBI_SCOPE

/**
 **\brief Class for reading sequences from fasta files.
 **/
class CWinMaskFastaReader : public CWinMaskReader
{
public:

    /**
     **\brief Object constructor.
     **
     **\param newInputStream input stream to read data from.
     **
     **/
    CWinMaskFastaReader( CNcbiIstream & newInputStream )
        : CWinMaskReader( newInputStream ) {}

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CWinMaskFastaReader() {}

    /**
     **\brief Read next sequence from fasta stream.
     **
     **\return pointer (reference counting) to the object 
     **        containing information about the sequence, or
     **        CRef( NULL ) if end of file is reached.
     **
     **/
    virtual CRef< objects::CSeq_entry > GetNextSequence();
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/02/25 21:32:54  dicuccio
 * Rearranged winmasker files:
 * - move demo/winmasker to a separate app directory (src/app/winmasker)
 * - move win_mask_* to app directory
 *
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

#endif

