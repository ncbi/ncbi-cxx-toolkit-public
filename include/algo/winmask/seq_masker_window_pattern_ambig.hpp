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
 *   Header file for CSeqMaskerWindowPatternAmbig class.
 *
 */

#ifndef C_SEQ_MASKER_WINDOW_PATTERN_AMBIG_H
#define C_SEQ_MASKER_WINDOW_PATTERN_AMBIG_H

#include <algo/winmask/seq_masker_window_pattern.hpp>

BEGIN_NCBI_SCOPE


/**
 **\brief Window iterator for discontiguous units used for the merging pass.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerWindowPatternAmbig : public CSeqMaskerWindowPattern
{
public:

    /**
     **\brief Object constructor.
     **     
     **\param arg_data the base sequence
     **\param arg_unit_size the value of the unit size to use
     **\param arg_window_size the value of the window size to use
     **\param window_step the number of bases by which the window
     **                   advances when operator++() is applied
     **\param arg_pattern pattern to construct discontiguous units
     **\param arg_ambig_unit the value to use for units containing 
     **                      ambiguity characters.
     **\param window_start offset of the first window
     **\param arg_unit_step distance between consequtive units within
     **                     a window
     **
     **/
    CSeqMaskerWindowPatternAmbig( const string & arg_data, 
                                  Uint1 arg_unit_size, 
                                  Uint1 arg_window_size,
                                  Uint4 window_step, Uint4 arg_pattern, 
                                  TUnit arg_ambig_unit,
                                  Uint4 window_start = 0,
                                  Uint1 arg_unit_step = 1 );

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CSeqMaskerWindowPatternAmbig() {}

protected:

    /**
     **\brief Advance the window by a specified number of characters.
     **
     ** This function always advances by the correct number of characters
     ** (as opposed to CSeqMaskerWindowPattern::Advance() that can jump 
     ** over the ambiguities).
     **
     **\param step advance by that many bases.
     **/
    virtual void Advance( Uint4 step );

    /**
     **\brief Value to use for units containing ambiguity characters.
     **
     **/
    TUnit ambig_unit;

private:

    /** 
     **\brief Computes the units starting at specified position.
     **
     **\param winstart new start position of the window.
     **/
    void FillWindow( Uint4 winstart );

    /**
     **\brief Ambiguity status of the window.
     **
     **The value is true if the window currently has units with 
     **ambiguity characters, false otherwise.
     **/
    bool ambig;
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.2  2005/02/12 19:58:03  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

#endif

