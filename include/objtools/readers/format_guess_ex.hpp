#ifndef FORMATGUESS_EX__HPP
#define FORMATGUESS_EX__HPP

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
 * Author: Nathan Bouk
 *
 * File Description: wrapper and extention to CFormatGuess, using
 *     actual file readers CFormatGuess fails
 *
 */

#include <corelib/ncbistd.hpp>
#include <bitset>

#include <util/format_guess.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////
///
/// Wraps CFormatGuess, and if CFormatGuess's result is Unknown,
///  it tries every file reader until one works.
/// 

class NCBI_XOBJREAD_EXPORT CFormatGuessEx
{
    //  Construction, destruction
public:
    CFormatGuessEx();

    CFormatGuessEx(
        const string& /* file name */ );

    CFormatGuessEx(
        CNcbiIstream& );

    ~CFormatGuessEx();

    //  Interface:
public:

    CFormatGuess::EFormat GuessFormat();
    bool TestFormat(CFormatGuess::EFormat );

    /// Get format hints
    CFormatGuess::CFormatHints& GetFormatHints(void) 
		{ return m_Guesser->GetFormatHints(); }

    // helpers:
protected:
   
private:
protected:
    auto_ptr<CFormatGuess> m_Guesser;
	std::stringstream m_LocalBuffer;
	bool x_FillLocalBuffer(CNcbiIstream& In);
	
	bool x_TryFormat(CFormatGuess::EFormat Format);
	
 	//	bool x_TryBinaryAsn();
		bool x_TryRmo();
		bool x_TryAgp();
	//	bool x_TryXml();
		bool x_TryWiggle();
		bool x_TryBed();
		bool x_TryBed15();
		bool x_TryFasta();
	//	bool x_TryTextAsn();
		bool x_TryGtf();
		bool x_TryGff3();
		bool x_TryGff2();
	//	bool x_TryHgvs();

};


END_NCBI_SCOPE

#endif
