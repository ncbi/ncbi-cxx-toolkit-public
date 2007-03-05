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
    /// Returns TemplateLength
    unsigned char GetTemplateLength() const { 
        return m_Opts->GetMBTemplateLength();
    }
    /// Sets TemplateLength
    /// @param length TemplateLength [in]
    void SetTemplateLength(unsigned char length) 
    {
        m_Opts->SetMBTemplateLength(length);
    }

    /// Returns TemplateType
    unsigned char GetTemplateType() const { 
        return m_Opts->GetMBTemplateType();
    }
    /// Sets TemplateType
    /// @param type TemplateType [in]
    void SetTemplateType(unsigned char type) {
        m_Opts->SetMBTemplateType(type);
    }

    /// Sets WordSize
    /// @param ws WordSize [in]
    void SetWordSize(int ws) { 
        if (ws == 11 || ws == 12) {
            m_Opts->SetWordSize(ws); 
        } else {
            NCBI_THROW(CBlastException, eInvalidOptions, 
                       "Word size must be 11 or 12 only");
        }
    }

    /// NOTE: Unavailable for discontiguous megablast
    /// @throws CBlastException if this is called on an object configured for
    /// discontiguous megablast
    void SetTraditionalBlastnDefaults();

protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("blastn", "megablast");
    }
    
    /// Sets MBLookupTableDefaults
    void SetMBLookupTableDefaults();
    /// Sets MBInitialWordOptionsDefaults
    void SetMBInitialWordOptionsDefaults();
    /// Sets MBGappedExtensionDefaults
    void SetMBGappedExtensionDefaults();
    /// Sets MBScoringOptionsDefaults
    void SetMBScoringOptionsDefaults();

private:
    /// Disallow copy constructor
    CDiscNucleotideOptionsHandle(const CDiscNucleotideOptionsHandle& rhs);
    /// Disallow assignment operator
    CDiscNucleotideOptionsHandle& operator=(const CDiscNucleotideOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


#endif  /* ALGO_BLAST_API___DISC_NUCL_OPTIONS__HPP */
