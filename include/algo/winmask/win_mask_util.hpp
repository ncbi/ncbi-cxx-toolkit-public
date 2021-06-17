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
 *   WindowMasker helper functions (prototypes).
 *
 */

#ifndef C_WIN_MASK_UTIL_HPP
#define C_WIN_MASK_UTIL_HPP

#include <set>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/seqmasks_io/mask_reader.hpp>

// #include "win_mask_config.hpp"

BEGIN_NCBI_SCOPE

class NCBI_XALGOWINMASK_EXPORT CWinMaskUtil
{
    public:

    /**\brief Base class for sets of seq_id representations used with
              -ids and -exclude-ids options.
    */
    class NCBI_XALGOWINMASK_EXPORT CIdSet
    {
        public:

            /**\brief Object destructor.
            */
            virtual ~CIdSet() {}

            /**\brief Add a string to the id set.
               \param id_str id to add
            */
            virtual void insert( const string & id_str ) = 0;

            /**\brief Check if the id set is empty.
               \return true if the id set is empty, false otherwise
            */
            virtual bool empty() const = 0;

            /**\brief Check if the id of the given bioseq is in the id set.
               \param bsh bioseq handle which id is to be checked
               \return true if the id of the bsh is found in the id set;
                       false otherwise
            */
            virtual bool find( const objects::CBioseq_Handle & bsh ) const = 0;
    };

    /**\brief Implementation of CIdSet that compares CSeq_id handles.
    */
    class NCBI_XALGOWINMASK_EXPORT CIdSet_SeqId : public CIdSet
    {
        public:

            /**\brief Object destuctor.
            */
            virtual ~CIdSet_SeqId() {}

            /**\brief See documentation for CIdSet::insert().
            */
            virtual void insert( const string & id_str );

            /**\brief See documentation for CIdSet::empty().
            */
            virtual bool empty() const { return idset.empty(); }

            /**\brief See documentation for CIdSet::find().
            */
            virtual bool find( const objects::CBioseq_Handle & ) const;

        private:

            /**\internal
               \brief Container to store id handles.
            */
            set< objects::CSeq_id_Handle > idset;
    };

    /**\brief Implementation of CIdSet that does substring matching.
    */
    class NCBI_XALGOWINMASK_EXPORT CIdSet_TextMatch : public CIdSet
    {
        public:

            /**\brief Object destructor.
            */
            virtual ~CIdSet_TextMatch() {}

            /**\brief See documentation for CIdSet::insert().
            */
            virtual void insert( const string & id_str );

            /**\brief See documentation for CIdSet::empty().
            */
            virtual bool empty() const { return nword_sets_.empty(); }

            /**\brief See documentation for CIdSet::find().
            */
            virtual bool find( const objects::CBioseq_Handle & ) const;

        private:

            /**\internal\brief Set of ids consisting of the same number of words.
            */
            typedef set< string > TNwordSet;

            /**\internal\brief Split a string into words and return an array
                               of word start offsets.

                The last element is always one past the end of the last word.

                \param id_str string to split into words
                \return vector of word start offsets
            */
            static const vector< Uint4 > split( const string & id_str );

            /**\internal\brief Match an id by string.
               \param id_str string to match against.
               \return true if some id in the id set is a whole word substring 
                       of id_str, false otherwise
            */
            bool find( const string & id_str ) const;

            /**\internal\brief Match an n-word id by strings.
               \param id_str n-word id substring
               \param nwords number of words in id_str - 1
               \return true if id_str is found in id set, false otherwise
            */
            bool find( const string & id_str, Uint4 nwords ) const;

            /**\internal\brief Set of ids grouped by the number of words.
            */
            vector< TNwordSet > nword_sets_;
    };

    /** Function iterating over bioseqs in input. Handles input as a list of seq-ids
     *  to be queried from the object manager, in Fasta format or in BlastDB format
    */
    class NCBI_XALGOWINMASK_EXPORT CInputBioseq_CI
    {
    public:
        CInputBioseq_CI(const string & input_file, const string & input_format);

        /// Move to the next object in iterated sequence
        CInputBioseq_CI& operator++ (void);

        /// Check if iterator points to an object
        DECLARE_OPERATOR_BOOL(m_CurrentBioseq);

        const objects::CBioseq_Handle& operator* (void) const { return m_CurrentBioseq; }
        const objects::CBioseq_Handle* operator-> (void) const { return &m_CurrentBioseq; }

    private:
        unique_ptr< CNcbiIstream > m_InputFile; // input file
        unique_ptr< CMaskReader > m_Reader;      // reader used for fasta and bdb formats
        CRef<objects::CScope> m_Scope;
        objects::CBioseq_Handle      m_CurrentBioseq; // current found Bioseq

        // disallow copying of object
        CInputBioseq_CI(const CInputBioseq_CI&);
        CInputBioseq_CI& operator= (const CInputBioseq_CI&);
    };

        /**
	  \brief Check if the given bioseq should be considered for 
	  processing.

            ids and exclude_ids should not be simultaneousely non empty.

            \param bsh bioseq handle in question
            \param ids set of seq ids to consider
            \param exclude_ids set of seq ids excluded from consideration
            \return true if ids is not empty and bsh is among ids, or else
                         if exclude_ids is not empty and bsh is not among
                            exclude_ids;
                    false otherwise
         */
        static bool consider( 
            const objects::CBioseq_Handle & bsh,
            const CIdSet * ids,
            const CIdSet * exclude_ids );

};

END_NCBI_SCOPE

#endif
