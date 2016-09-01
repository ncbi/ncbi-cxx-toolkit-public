#ifndef OBJTOOLS_BLAST_SEQDB_WRITER_IMPL___CRITERIA__HPP
#define OBJTOOLS_BLAST_SEQDB_WRITER_IMPL___CRITERIA__HPP

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
 * Author: Thomas W. Rackers
 *
 * File Description:
 *   This file defines a hierarchy of criteria function classes.
 *
 */

/// \author Thomas W. Rackers

// NOTE: Put correct file name and description here:
/// @file criteria.hpp
/// This is the header file for defining and working with criteria functions.
///
/// Defines classes:
///     ICriteria
///     CCriteria_EST_HUMAN
///     CCriteria_EST_MOUSE
///     CCriteria_EST_OTHERS
///     CCriteria_SWISSPROT
///     CCriteria_PDB
///     CCriteria_REFSEQ
///     CCriteria_REFSEQ_RNA
///     CCriteria_REFSEQ_GENOMIC
///     CCriteriaSet
///
/// Implemented for: Linux, MS-Windows


#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/blastdb/Blast_def_line.hpp>

BEGIN_NCBI_SCOPE


/// Structure which corresponds to format of records in DI files.
struct SDIRecord {
    int     oid;            /// Ordinal ID
    TGi     gi;             /// GenInfo ID
    int     taxid;          /// Taxonomy ID
    int     owner;          /// Owner
    string  div;            /// 3-letter division
    int     len;            /// Length of sequence
    int     hash;           /// Hash value for sequence data
    int     sat_key;        /// ???
    string  acc;            /// Accession
    int     mol;            /// Molecule type, as in Seq-inst::mol

    SDIRecord() {
        oid = taxid = owner = len = hash = sat_key = mol = 0;
        gi = ZERO_GI;
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// ICriteria
///
/// "Interface" class for criteria-function classes.
///
/// Each criteria-function class must at least implement
/// the virtual methods 'is(...)', 'GetLabel()', and
/// 'GetMembershipBit()'.
/// Since all methods in this base class are pure virtual
/// and there are no data members (except for the destructor),
/// class ICriteria qualifies as an "interface", similar to
/// the concept of interfaces in Java and C#.
/// "Interfaces" may be safely used in multiple-inheritance cases,
/// as long as they follow all non-interfaces in the class declaration.
class NCBI_XOBJWRITE_EXPORT ICriteria
{
public:
    /// Fixed assignment of membership bits.
    /// Note that bit positions are indexed from 1, that is,
    /// the least significant bit of a bitmask is bit 1, not bit 0.
    /// This enumeration can be extended upwards as needed.
    enum EMembershipBit {
        eUNASSIGNED         = -1,   // sentinel value
        eDO_NOT_USE         =  0,   // under threat of torture
        eSWISSPROT          =  1,
        ePDB                =  2,
        eREFSEQ             =  3
    };

    /// Virtual destructor.
    virtual ~ICriteria() {}

    /// Return true if criteria are met.
    virtual bool is(const SDIRecord* ptr) const = 0;

    /// Return assigned label.
    virtual const char* GetLabel(void) const = 0;

    /// Return assigned membership bit > 0.
    virtual const int GetMembershipBit(void) const = 0;
};


// Each of the criteria subclasses defined below must be included in the
// static array 'all_criteria[]' in constructor CCriteriaSet::CCriteriaSet()
// (below).

/// CCriteria_EST_HUMAN
///
/// Subclass of ICriteria for EST_HUMAN sequences.
///
/// The body of method 'is' is ported from the C Toolkit source
/// file 'readdb.c'.
class NCBI_XOBJWRITE_EXPORT CCriteria_EST_HUMAN : public ICriteria
{
public:
    /// @inheritDoc
    virtual bool is(const SDIRecord* ptr) const {
        return (ptr->taxid == 9606);
    }

    /// @inheritDoc
    virtual const char* GetLabel(void) const {
        return "est_human";
    }

    /// @inheritDoc
    virtual const int GetMembershipBit(void) const {
        return ICriteria::eUNASSIGNED;
    }
};


/// CCriteria_EST_MOUSE
///
/// Subclass of ICriteria for EST_MOUSE sequences.
///
/// The body of method 'is' is ported from the C Toolkit source
/// file 'readdb.c'.
class NCBI_XOBJWRITE_EXPORT CCriteria_EST_MOUSE : public ICriteria
{
public:
    /// @inheritDoc
    virtual bool is(const SDIRecord* ptr) const {
        const int taxid = ptr->taxid;
        return (
            taxid == 10090
            ||  taxid == 10091
            ||  taxid == 10092
            ||  taxid == 35531
            ||  taxid == 80274
            ||  taxid == 57486
        );
    }

    /// @inheritDoc
    virtual const char* GetLabel(void) const {
        return "est_mouse";
    }

    /// @inheritDoc
    virtual const int GetMembershipBit(void) const {
        return ICriteria::eUNASSIGNED;
    }
};


/// CCriteria_EST_OTHERS
///
/// Subclass of ICriteria for EST_OTHERS sequences.
///
/// The body of method 'is' is ported from the C Toolkit source
/// file 'readdb.c'.
class NCBI_XOBJWRITE_EXPORT CCriteria_EST_OTHERS : public ICriteria
{
private:
    CCriteria_EST_HUMAN crit_EST_HUMAN;
    CCriteria_EST_MOUSE crit_EST_MOUSE;

public:
    /// @inheritDoc
    virtual bool is(const SDIRecord* ptr) const {
        return ! (
                this->crit_EST_HUMAN.is(ptr)
                ||  this->crit_EST_MOUSE.is(ptr)
        );
    }

    /// @inheritDoc
    virtual const char* GetLabel(void) const {
        return "est_others";
    }

    /// @inheritDoc
    virtual const int GetMembershipBit(void) const {
        return ICriteria::eUNASSIGNED;
    }
};


/// CCriteria_SWISSPROT
///
/// Subclass of ICriteria for SWISSPROT sequences.
///
/// The body of method 'is' is ported from the C Toolkit source
/// file 'readdb.c'.
class NCBI_XOBJWRITE_EXPORT CCriteria_SWISSPROT : public ICriteria
{
public:
    /// @inheritDoc
    virtual bool is(const SDIRecord* ptr) const {
        return (ptr->owner == 6);
    }

    /// @inheritDoc
    virtual const char* GetLabel(void) const {
        return "swissprot";
    }

    /// @inheritDoc
    virtual const int GetMembershipBit(void) const {
        return ICriteria::eSWISSPROT;
    }
};


/// CCriteria_PDB
///
/// Subclass of ICriteria for PDB sequences.
///
/// The body of method 'is' is ported from the C Toolkit source
/// file 'readdb.c'.
class NCBI_XOBJWRITE_EXPORT CCriteria_PDB : public ICriteria
{
public:
    /// @inheritDoc
    virtual bool is(const SDIRecord* ptr) const {
        return (ptr->owner == 10);
    }

    /// @inheritDoc
    virtual const char* GetLabel(void) const {
        return "pdb";
    }

    /// @inheritDoc
    virtual const int GetMembershipBit(void) const {
        return ICriteria::ePDB;
    }
};


/// CCriteria_REFSEQ
///
/// Subclass of ICriteria for REFSEQ sequences.
///
/// The body of method 'is' is ported from the C Toolkit source
/// file 'readdb.c'.
class NCBI_XOBJWRITE_EXPORT CCriteria_REFSEQ : public ICriteria
{
public:
    /// @inheritDoc
    ///
    /// Criteria for determining whether a sequence is REFSEQ:
    /// 1. First 2 characters of the accession are letters.
    /// 2. 3rd character is an '_' (underscore).
    /// 3. Must be at least kMinAccessionLength characters long (currently 9).
    /// Updated per suggestion from Misha Kimelman (via email).
    virtual bool is(const SDIRecord* ptr) const {
        const size_t kMinAccessionLength = 9;

        const string& accession = ptr->acc;
        return (
            (accession.length() >= kMinAccessionLength)
            &&  isalpha(accession[0])
            &&  isalpha(accession[1])
            &&  (accession[2] == '_')
        );
    }

    /// @inheritDoc
    virtual const char* GetLabel(void) const {
        return "refseq";
    }

    /// @inheritDoc
    virtual const int GetMembershipBit(void) const {
        return ICriteria::eREFSEQ;
    }
};


/// CCriteria_REFSEQ_RNA
///
/// Subclass of ICriteria for REFSEQ_RNA sequences.
///
/// The body of method 'is' is ported from the C Toolkit source
/// file 'readdb.c'.
class NCBI_XOBJWRITE_EXPORT CCriteria_REFSEQ_RNA : public ICriteria
{
private:
    CCriteria_REFSEQ crit_REFSEQ;

public:
    /// @inheritDoc
    ///
    /// Criteria for determining whether a sequence belongs in the refseq_rna
    /// database.
    /// This is a subset of the sequences identified by is_REFSEQ
    /// with the additional constraint that the molecule type must be RNA.
    virtual bool is(const SDIRecord* ptr) const {
        const int Seq_mol_rna = 2; // imported from C Toolkit's 'objseq.h'
        return (this->crit_REFSEQ.is(ptr)  &&  (ptr->mol == Seq_mol_rna));
    }

    /// @inheritDoc
    virtual const char* GetLabel(void) const {
        return "refseq_rna";
    }

    /// @inheritDoc
    virtual const int GetMembershipBit(void) const {
        return ICriteria::eREFSEQ;
    }
};


/// CCriteria_REFSEQ_GENOMIC
///
/// Subclass of ICriteria for REFSEQ_GENOMIC sequences.
///
/// The body of method 'is' is ported from the C Toolkit source
/// file 'readdb.c'.
class NCBI_XOBJWRITE_EXPORT CCriteria_REFSEQ_GENOMIC : public ICriteria
{
private:
    CCriteria_REFSEQ     crit_REFSEQ;
    CCriteria_REFSEQ_RNA crit_REFSEQ_RNA;

public:
    /// @inheritDoc
    virtual bool is(const SDIRecord* ptr) const {
        return (
            this->crit_REFSEQ.is(ptr)
            &&  ! this->crit_REFSEQ_RNA.is(ptr)
        );
    }

    /// @inheritDoc
    virtual const char* GetLabel(void) const {
        return "refseq_genomic";
    }

    /// @inheritDoc
    virtual const int GetMembershipBit(void) const {
        return ICriteria::eREFSEQ;
    }
};


typedef map<string, ICriteria*, ncbi::PNocase> TCriteriaMap;


/// CCriteriaSet
///
/// Container class for pointers to CCriteria objects.
///
/// This class provides a means to create a collection of CCriteria classes
/// which will be supported by a user application.  All of the above
/// subclasses of CCriteria are eligible for inclusion in a CCriteriaSet,
/// but some applications may only want to support a subset of the CCriteria.
class NCBI_XOBJWRITE_EXPORT CCriteriaSet
{
private:
    /// Actual container is a map object rather than a set, with the label
    /// used as the key.
    TCriteriaMap m_Crit_from_Label;

public:
    /// Constructor, creates empty container.
    CCriteriaSet(void);

    /// Destructor
    virtual ~CCriteriaSet();

    /// Factory method, retrieve pointer to existing instance of one of the
    /// CCriteria_* subclasses.
    ///
    /// \param label of desired CCriteria_* class
    /// \return pointer to CCriteria_* class instance or NULL if not found
    static ICriteria* GetCriteriaInstance(
            const string& label
    );

    /// Add a CCriteria_* class to the supported collection.  This method
    /// supports adding custom criteria classes which do not appear in the
    /// list of predefined criteria classes.
    ///
    /// \param pointer to criteria function instance
    /// \return true if criteria function added to set
    bool AddCriteria(
            ICriteria* critPtr
    );


    /// Add a CCriteria_* class to the supported collection.  This method
    /// only supports adding criteria classes which are already defined
    /// in this header file.
    ///
    /// \param label string on which set is searched for criteria function
    /// \return true if criteria function added to set
    bool AddCriteria(
            const string& label
    );


    /// Fetch a CCriteria_* class based on its label.
    /// Returns NULL if a matching class is not found.
    ///
    /// \param label string on which set is searched for criteria function
    /// \return pointer to criteria function
    const ICriteria* FindCriteria(
            const string& label
    );


    /// Return the number of entries in the container.  Because a map
    /// (not a multimap) is used, duplicate entries should not be counted.
    ///
    /// \return count of entries
    unsigned int GetCriteriaCount(void) const;


    /// Return the actual container.  This will permit the caller to
    /// iterate through the entries.
    ///
    /// \return internal container
    const TCriteriaMap& GetCriteriaMap(void) const;

};


/// CCriteriaSet_CalculateMemberships
///
/// This function uses a predefined subset of the available criteria functions
/// to create a membership bitmap.
///
/// \param direcord struct containing data derived from a single DI record
/// \return TMemberships object (typedef'ed to list<int>)
NCBI_XOBJWRITE_EXPORT
objects::CBlast_def_line::TMemberships CCriteriaSet_CalculateMemberships(
        const SDIRecord& direcord       /// DI record data
);

/// Overloaded version of CCriteriaSet_CalculateMemberships.
/// @return 0 on success and non-zero on failure
NCBI_XOBJWRITE_EXPORT
int
CCriteriaSet_CalculateMemberships(
        const SDIRecord& direcord,      /// DI record data
        objects::CBlast_def_line& defline   /// The
);


END_NCBI_SCOPE
#endif  //OBJTOOLS_BLAST_SEQDB_WRITER_IMPL___CRITERIA__HPP


/**
 * @page _impl_criteria_howto Creating Custom Criteria Subclasses
 *
 *  The ICriteria "interface" class is provided with a predefined collection
 *  of criteria subclasses with class names of the form "CCriteria_<subset>"
 *  where <subset> matches the commonly used label of the subset, but all
 *  uppercase.  There is one such subclass for each instance of a criteria
 *  function in the C Toolkit's readdb.c source file.  Below is a table of
 *  the old criteria function names and their workalikes in the C++ Toolkit.
 *
 *  @code
 *  C Toolkit function      C++ Toolkit class           Class label
 *  --------------------    --------------------        ------------
 *  is_EST_HUMAN            CCriteria_EST_HUMAN         "est_human"
 *  is_EST_MOUSE            CCriteria_EST_MOUSE         "est_mouse"
 *  is_EST_OTHERS           CCriteria_EST_OTHERS        "est_others"
 *  is_PDB                  CCriteria_PDB               "pdb"
 *  is_REFSEQ               CCriteria_REFSEQ            "refseq"
 *  is_REFSEQ_GENOMIC       CCriteria_REFSEQ_GENOMIC    "refseq_genomic"
 *  is_REFSEQ_RNA           CCriteria_REFSEQ_RNA        "refseq_rna"
 *  is_SWISSPROT            CCriteria_SWISSPROT         "swissprot"
 *  @endcode
 *
 *  None of the CCriteria_<subset> classes above possess any state information
 *  or methods beyond the three which must be implemented as required by the
 *  abstract base class (ABC) ICriteria (more information below).  However,
 *  the design of the criteria class suite allows the definition and inclusion
 *  of custom subsets which can implement state variables and specialized
 *  methods.  The following is an example of defining and using such a custom
 *  class.
 *
 *  - Defining the custom criteria class
 *  @code
 *  // Demonstrate the definition of a custom criteria-function class.
 *  // Unlike the predefined criteria classes, this one has:
 *  //  - a non-trivial constructor,
 *  //  - a data member, and
 *  //  - a non-virtual method.
 *  // This criteria class allows a taxid to be specified when the constructor
 *  // is called.  Henceforth membership in this custom subset will be
 *  // determined by whether the supplied DI record's taxid matches it.
 *  class CCriteria_CUSTOM : public ICriteria
 *  {
 *  private:
 *      // Data member, set by constructor.
 *      const int m_taxid;
 *
 *  public:
 *      // Constructor, non-default.
 *      CCriteria_CUSTOM(const int val) : m_taxid(val) {
 *      }
 *
 *      // Accessor method specific to this criteria class.
 *      const int GetTaxid(void) {
 *          return this->m_taxid;
 *      }
 *
 *      // The next three methods MUST be implemented in this class;
 *      // they are pure virtual methods in parent class ICriteria.
 *
 *      // Implementation of 'is' method (required).
 *      virtual bool is(const SDIRecord* ptr) const {
 *          return (ptr->taxid == this->m_taxid);
 *      }
 *
 *      // Implementation of 'GetLabel' method (required).
 *      virtual const char* GetLabel(void) const {
 *          return "custom";                    // all lowercase recommended
 *      }
 *
 *      // Implementation of 'GetMembershipBit' method (required).
 *      virtual const int GetMembershipBit(void) const {
 *          return ICriteria::eUNASSIGNED;      // usually not assigned
 *      }
 *  };
 *  @endcode
 *
 *  - Using the custom criteria class in application code
 *  To set up an application's accepted set of criteria functions, you use
 *  class CCriteriaSet's AddCriteria methods.  To add one of the predefined
 *  criteria class, you would probably use "AddCriteria(const string& label)",
 *  supplying the class's label as shown in the table above (case is ignored).
 *  To add an instance of CCriteria_CUSTOM, you would use the OTHER form of
 *  AddCriteria as shown below.
 *
 *  @code
 *  // Create empty criteria set.
 *  CCriteriaSet myCriteriaSet;
 *
 *  // Add a predefined criteria class.
 *  myCriteriaSet.AddCriteria("SwissProt");         // case-insensitive
 *
 *  // Define and add a custom criteria class.
 *  ICriteria* myCustomCriteria = new CCriteria_CUSTOM(1776);
 *  NcbiCout << "CCriteria_CUSTOM's taxid is "
 *      << myCustomCriteria->GetTaxid() << NcbiEndl;
 *  // prints "CCriteria_CUSTOM's taxid is 1776"
 *  myCriteriaSet.AddCriteria(myCustomCriteria);    // the OTHER form
 *
 *  // At this point, both of the following lines would return non-NULL
 *  // pointers to ICriteria objects.
 *  const ICriteria* swissprot_criteria =
 *      myCriteriaSet.FindCriteria("SWISSPROT");    // still case-insensitive
 *  const ICriteria* custom_criteria =
 *      myCriteriaSet.FindCriteria("Custom");       // here too
 *  @endcode
 */
