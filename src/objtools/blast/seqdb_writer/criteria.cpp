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
 *   This file defines global functions which support the criteria function
 *   classes.
 *
 */

/// \author Thomas W. Rackers

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_writer/impl/criteria.hpp>

using ncbi::CCriteriaSet;


BEGIN_NCBI_SCOPE


USING_SCOPE(objects);


#define MY_TRACING 0


/// Static (file-local) function
static TCriteriaMap& GetAvailableCriteria(void)
{
    // Constructor will be invoked only once.
    static TCriteriaMap* s_Available_Criteria_ptr = new TCriteriaMap;

    // Remember if we've initialized the above map.
    static bool map_empty = true;

    // There should be an entry for each of the subclasses of ICriteria
    // defined in criteria.hpp.  The order is not important, here they're
    // listed alphabetically.
    // This is the one location where instances of the predefined subclasses
    // of ICriteria are actually created.  This effectively makes them all
    // singletons, although the Singleton Design Pattern is not enforced.
    // Custom subclasses may contain state information, in which case they
    // probably will NOT be used as singletons.
    static ICriteria* allCriteria[] = {
            new CCriteria_EST_HUMAN,
            new CCriteria_EST_MOUSE,
            new CCriteria_EST_OTHERS,
            new CCriteria_PDB,
            new CCriteria_REFSEQ,
            new CCriteria_REFSEQ_GENOMIC,
            new CCriteria_REFSEQ_RNA,
            new CCriteria_SWISSPROT
    };

    // If map is empty, we need to fill it in.  Should happen only once.
    if (map_empty  &&  s_Available_Criteria_ptr->size() == 0) {
        // Hey Mister Tally-man, tally me criteria....
        const int numCriteria = sizeof allCriteria / sizeof allCriteria[0];

        // Create a temporary vector container over which we can iterate.
        vector<ICriteria*> allCriteria_vec(
                allCriteria,
                allCriteria + numCriteria
        );

        // Add each predefined criteria function to available set.
        ITERATE(vector<ICriteria*>, critter, allCriteria_vec) {
            ICriteria* crit = *critter;
            (*s_Available_Criteria_ptr)[crit->GetLabel()] = crit;
        }

        // Map is no longer empty.  (Quicker than calling size() each time.)
        map_empty = false;
    }

    return *s_Available_Criteria_ptr;
}


/// Constructor, creates empty container.
CCriteriaSet::CCriteriaSet(void) {}


/// Destructor
/* virtual */ CCriteriaSet::~CCriteriaSet() {}


/// Factory method, retrieve pointer to existing instance of one of the
/// CCriteria_* subclasses.
///
/// \param label of desired CCriteria_* class
/// \return pointer to CCriteria_* class instance or NULL if not found
/* static */ ICriteria* CCriteriaSet::GetCriteriaInstance(
        const string& label
) {
    TCriteriaMap& critMap = GetAvailableCriteria();
    TCriteriaMap::iterator it = critMap.find(label);
    if (it == critMap.end()) {
        return NULL;
    } else {
        return it->second;
    }
}


/// Add a CCriteria_* class to the supported collection.  This method
/// supports adding custom criteria classes which do not appear in the
/// list of predefined criteria classes.
///
/// \param pointer to criteria function instance
/// \return true if criteria function added to set
bool CCriteriaSet::AddCriteria(
        ICriteria* critPtr
) {
    // If the map doesn't already include an entry with the supplied
    // criteria class's label, a new entry will be created.
    // If the map does have such an entry already, the map is not changed.
    // (This is a characteristic of map::operator[].)
    unsigned int numEntries = this->m_Crit_from_Label.size();
    this->m_Crit_from_Label[critPtr->GetLabel()] = critPtr;

    // Return true if entry was unique and therefore added,
    // false if entry duplicated existing entry.
    return (this->m_Crit_from_Label.size() > numEntries);
}


/// Add a CCriteria_* class to the supported collection.  This method
/// only supports adding criteria classes which are already defined
/// in header file criteria.hpp, because they are looked up by label.
///
/// \param label string on which set is searched for criteria function
/// \return true if criteria function added to set
bool CCriteriaSet::AddCriteria(
        const string& label
) {
    TCriteriaMap& critMap = GetAvailableCriteria();
    TCriteriaMap::iterator it = critMap.find(label);
    if (it == critMap.end()) {
        return false;
    } else {
        return AddCriteria(it->second);
    }
}


/// Fetch a CCriteria_* class based on its label.
/// Returns NULL if a matching class is not found.
///
/// \param label string on which set is searched for criteria function
/// \return pointer to criteria function
const ICriteria* CCriteriaSet::FindCriteria(
        const string& label
) {
    TCriteriaMap::iterator it = this->m_Crit_from_Label.find(label);
    if (it == this->m_Crit_from_Label.end()) {
        return NULL;
    } else {
        return it->second;
    }
}


/// Return the number of entries in the container.  Because a map
/// (not a multimap) is used, duplicate entries should not be counted.
///
/// \return count of entries
unsigned int CCriteriaSet::GetCriteriaCount(void) const {
    return this->m_Crit_from_Label.size();
}


/// Return the actual container.  This will permit the caller to
/// iterate through the entries.
///
/// \return internal container
const TCriteriaMap& CCriteriaSet::GetCriteriaMap(void) const {
    return this->m_Crit_from_Label;
}


CBlast_def_line::TMemberships CCriteriaSet_CalculateMemberships(
        const SDIRecord& direcord
) {
    static CCriteriaSet* critSet_ptr = NULL;

    if (critSet_ptr == NULL) {
        // First time through, create the default criteria set.
        // Verify that all succeed.
        critSet_ptr = new CCriteriaSet;
        _VERIFY(critSet_ptr->AddCriteria("swissprot"));
        _VERIFY(critSet_ptr->AddCriteria("pdb"));      
        _VERIFY(critSet_ptr->AddCriteria("refseq"));    
        _VERIFY(critSet_ptr->AddCriteria("refseq_rna"));    
        _VERIFY(critSet_ptr->AddCriteria("refseq_genomic"));    
    }

    // Need number of bits per mask word (i.e. an int).
    static const int MASK_WORD_SIZE = sizeof (int) * 8;    // 8 bits/byte

    // Define initially empty membership bit list (list of ints).
    CBlast_def_line::TMemberships bits_list;

    // Get the set of accepted criteria in container form
    // to allow iteration through its contents.
    const TCriteriaMap& critContainer = critSet_ptr->GetCriteriaMap();

    // Check the DI record against each criteria function in turn.
    ITERATE(TCriteriaMap, critItem, critContainer) {

        // Get the criteria function.  (The container is actually a map,
        // not a set, so each item is a std::pair<string,ICriteria*>.)
        ICriteria* crit = critItem->second;
#if MY_TRACING
        NcbiCout << "Checking for " << crit->GetLabel() << "... ";
#endif

        if (crit->is(&direcord)) {

            // Get assigned membership bit for this criteria function.
            int membership_bit = crit->GetMembershipBit();
#if MY_TRACING
            NcbiCout << "is a " << crit->GetLabel() << ", membership bit is "
                << membership_bit << NcbiEndl;
#endif

            // Verify it's not one of the two disallowed values.
            if (membership_bit != ICriteria::eUNASSIGNED
                &&  membership_bit != ICriteria::eDO_NOT_USE) {

                // Convert 1-indexed membership bit to 0-indexed offset.
                int bit_offset = membership_bit - 1;

                // Create bit-mask word and calculate its offset in list.
                int bit_mask = 0x1 << (bit_offset % MASK_WORD_SIZE);
                int list_offset = bit_offset / MASK_WORD_SIZE;

                // Is list long enough for calculated offset?
                int list_size = bits_list.size();
                if (list_size <= list_offset) {
                    // No, append extra zeros if needed.
                    while (list_size < list_offset) {
                        bits_list.push_back(0);
                        ++list_size;
                    }
                    // Now append the bit-mask word.
                    bits_list.push_back(bit_mask);
                } else {
                    // Yes, step through list, then bitwise-OR bitmask
                    // into proper location in list.
                    int cur_offset = 0;
                    NON_CONST_ITERATE(
                            CBlast_def_line::TMemberships,
                            iter,
                            bits_list
                    ) {
                        if (cur_offset == list_offset) {
                            *iter |= bit_mask;
                            break;  /* to HERE */
                        }
                        ++cur_offset;
                    }
                    /* HERE */
                }

            }

#if MY_TRACING
        } else {

            NcbiCout << "is not a " << crit->GetLabel() << NcbiEndl;

#endif
        }

    }

    // Our work here is finished.
    return bits_list;
}

int
CCriteriaSet_CalculateMemberships(const SDIRecord& direcord,
                                  objects::CBlast_def_line& defline)
{
    int retval = 0;
    try {
        CBlast_def_line::TMemberships list(CCriteriaSet_CalculateMemberships(direcord));
        defline.SetMemberships().swap(list);
    } catch (...) {
        retval = 1;
    }
    return retval;
}

END_NCBI_SCOPE
