#ifndef ALGO_BLAST_API___DISC_NUCL_OPTIONS__HPP
#define ALGO_BLAST_API___DISC_NUCL_OPTIONS__HPP

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
 * Authors:  Christiam Camacho
 *
 */

/// @file disc_nucl_options.hpp
/// Declares the CDiscNucleotideOptionsHandle class.

#include <algo/blast/api/blast_nucl_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the nucleotide-nucleotide options to the discontiguous 
/// BLAST algorithm.
///
/// Adapter class for nucleotide-nucleotide discontiguous BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CDiscNucleotideOptionsHandle : 
                                            public CBlastNucleotideOptionsHandle
{
public:

    /// Creates object with default options set
    CDiscNucleotideOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CDiscNucleotideOptionsHandle() {}

    /******************* Lookup table options ***********************/
    unsigned char GetTemplateLength() const { 
        return m_Opts->GetMBTemplateLength();
    }
    void SetTemplateLength(unsigned char length) {
        m_Opts->SetMBTemplateLength(length);
    }

    unsigned char GetTemplateType() const { 
        return m_Opts->GetMBTemplateType();
    }
    void SetTemplateType(unsigned char type) {
        m_Opts->SetMBTemplateType(type);
    }

    void SetWordSize(short ws) { 
        if (ws == 11 || ws == 12) {
            m_Opts->SetWordSize(ws); 
        } else {
            NCBI_THROW(CBlastException, eBadParameter, 
                       "Word size must be 11 or 12 only");
        }
    }

    // Unavailable for discontiguous megablast, throws a CBlastException
    void SetTraditionalBlastnDefaults();

protected:

    void SetMBLookupTableDefaults();
    void SetMBInitialWordOptionsDefaults();
    void SetMBGappedExtensionDefaults();
    void SetMBScoringOptionsDefaults();

private:
    CDiscNucleotideOptionsHandle(const CDiscNucleotideOptionsHandle& rhs);
    CDiscNucleotideOptionsHandle& operator=(const CDiscNucleotideOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/05/04 13:09:20  camacho
 * Made copy-ctor & assignment operator private
 *
 * Revision 1.7  2004/03/22 20:15:58  dondosha
 * Added custom setting of MB scoring options for discontiguous case
 *
 * Revision 1.6  2004/03/19 14:53:24  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.5  2004/03/17 21:48:30  dondosha
 * Added custom SetMBInitialWordOptionsDefaults and SetMBGappedExtensionDefaults methods
 *
 * Revision 1.4  2004/01/20 15:19:25  camacho
 * Provide missing default parameters to default ctor
 *
 * Revision 1.3  2004/01/16 20:44:08  bealer
 * - Add locality flag for options handle classes.
 *
 * Revision 1.2  2003/12/09 12:40:22  camacho
 * Added windows export specifiers
 *
 * Revision 1.1  2003/11/26 18:22:17  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___DISC_NUCL_OPTIONS__HPP */
