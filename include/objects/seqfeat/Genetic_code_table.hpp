/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqfeat.asn'.
 */

#ifndef OBJECTS_SEQFEAT_GENETIC_CODE_TABLE_HPP
#define OBJECTS_SEQFEAT_GENETIC_CODE_TABLE_HPP


// generated includes
#include <objects/seqfeat/Genetic_code_table_.hpp>

#include <memory> // for auto_ptr<>

// generated classes

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CGen_code_table_imp;


class NCBI_SEQFEAT_EXPORT CGenetic_code_table : public CGenetic_code_table_Base
{
    typedef CGenetic_code_table_Base Tparent;
public:
    // constructor
    CGenetic_code_table(void);
    // destructor
    ~CGenetic_code_table(void);

private:
    // Prohibit copy constructor and assignment operator
    CGenetic_code_table(const CGenetic_code_table& value);
    CGenetic_code_table& operator=(const CGenetic_code_table& value);
};



/////////////////// CGenetic_code_table inline methods

// constructor
inline
CGenetic_code_table::CGenetic_code_table(void)
{
}


/////////////////// end of CGenetic_code_table inline methods


// genetic code translation table class

// the function CCdregion_translate::TranslateCdregion in
// <objects/util/sequence.hpp> obtains the sequence under
// a coding region feature location and uses CTrans_table
// to do the translation

class NCBI_SEQFEAT_EXPORT CTrans_table : public CObject
{
public:
    // constructor
    CTrans_table(const CGenetic_code&);
    // destructor
    ~CTrans_table(void);

    // translation finite state machine methods
    static int SetCodonState  (unsigned char ch1,
                               unsigned char ch2,
                               unsigned char ch3);
    static int NextCodonState (int state,
                               unsigned char ch);
    static int RevCompState   (int state);

    // lookup of amino acid translated for given codon (state)
    char GetCodonResidue (int state) const;
    char GetStartResidue (int state) const;
    char GetStopResidue  (int state) const;

    bool IsOrfStart   (int state) const;
    bool IsAmbigStart (int state) const;
    bool IsAnyStart   (int state) const;
    bool IsOrfStop    (int state) const;
    bool IsATGStart   (int state) const;
    bool IsAltStart   (int state) const;

private:
    // translation tables common to all genetic codes (single copy)
    static int  sm_NextState  [4097];
    static int  sm_RvCmpState [4097];
    static int  sm_BaseToIdx  [256];

    // initialize single copy translation tables
    static void x_InitFsaTable (void);

    // translation tables specific to each genetic code instance
    mutable char  m_AminoAcid [4097];
    mutable char  m_OrfStart  [4097];
    mutable char  m_OrfStop   [4097];

    // initialize genetic code specific translation tables
    void x_InitFsaTransl (const string *ncbieaa,
                          const string *sncbieaa) const;

    friend class CGen_code_table_imp;
};



// public interface for (single instance) genetic code and translation tables

class NCBI_SEQFEAT_EXPORT CGen_code_table
{
public:
    // return initialized translation table given genetic code
    static const CTrans_table& GetTransTable (int id);
    static const CTrans_table& GetTransTable (const CGenetic_code& gc);

    // return table of loaded genetic codes for iteration
    static const CGenetic_code_table& GetCodeTable (void);

    // Get the ncbieaa string for a specific genetic code.
    static const string& GetNcbieaa(int id);
    static const string& GetNcbieaa(const CGenetic_code& gc);

    // Get the sncbieaa string for a specific genetic code
    static const string& GetSncbieaa(int id);
    static const string& GetSncbieaa(const CGenetic_code& gc);

    // Convert the numeric representation of a codon to a base one.
    static string IndexToCodon(int index);

    // Convert the base representation of a codon to a numeric one.
    static int CodonToIndex(char base1, char base2, char base3);
    static int CodonToIndex(const string& codon);

    // Attempt to load a new translation table; if it fails, throw
    // an exception and continue to use the old one.
    static void LoadTransTable(CObjectIStream& ois);
    static void LoadTransTable(const string& path, ESerialDataFormat format);

private:
    // this class uses a singleton internally to manage the specifics
    // of the genetic code implementation
    // these are the variables / functions that control the singleton
    static AutoPtr<CGen_code_table_imp> sm_Implementation;

    static void                 x_InitImplementation(void);
    static CGen_code_table_imp& x_GetImplementation (void);
};


inline
CGen_code_table_imp& CGen_code_table::x_GetImplementation(void)
{
    if ( !sm_Implementation.get() ) {
        x_InitImplementation();
    }
    return *sm_Implementation;
}



/////////////////// CTrans_table inline methods

inline
int CTrans_table::SetCodonState (unsigned char ch1,
                                 unsigned char ch2,
                                 unsigned char ch3)
{
    return (256 * sm_BaseToIdx [(int) ch1] +
            16 * sm_BaseToIdx [(int) ch2] +
            sm_BaseToIdx [(int) ch3] + 1);
}

inline
int CTrans_table::NextCodonState (int state,
                                  unsigned char ch)
{
    if (state < 0 || state > 4096) return 0;
    return (sm_NextState [state] + sm_BaseToIdx [(int) ch]);
}

inline
int CTrans_table::RevCompState (int state)
{
    if (state < 0 || state > 4096) return 0;
    return (sm_RvCmpState [state]);
}

inline
char CTrans_table::GetCodonResidue (int state) const
{
    if (state < 0 || state > 4096) return 0;
    return (m_AminoAcid [state]);
}

inline
char CTrans_table::GetStartResidue (int state) const
{
    if (state < 0 || state > 4096) return 0;
    return (m_OrfStart [state]);
}

inline
char CTrans_table::GetStopResidue (int state) const
{
    if (state < 0 || state > 4096) return 0;
    return (m_OrfStop [state]);
}

inline
bool CTrans_table::IsOrfStart (int state) const
{
    return (GetStartResidue (state) == 'M');
}

inline
bool CTrans_table::IsAmbigStart (int state) const
{
    return (GetStartResidue (state) == 'X');
}

inline
bool CTrans_table::IsAnyStart (int state) const
{
    return (GetStartResidue (state) != '-');
}

inline
bool CTrans_table::IsOrfStop (int state) const
{
    return (GetStopResidue (state) == '*');
}

inline
bool CTrans_table::IsATGStart (int state) const
{
    static const int k_ATG_state = 389; // ATG initiation codon state
    return (IsOrfStart (state) && state == k_ATG_state);
}

inline
bool CTrans_table::IsAltStart (int state) const
{
    static const int k_ATG_state = 389; // ATG initiation codon state
    return (IsOrfStart (state) && state != k_ATG_state);
}

/////////////////// end of CTrans_table inline methods



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_SEQFEAT_GENETIC_CODE_TABLE_HPP
/* Original file checksum: lines: 93, chars: 2557, CRC32: e1e5ca57 */
