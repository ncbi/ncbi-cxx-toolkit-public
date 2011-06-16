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
* Author:  Michael Kornbluh
*
* File Description:
*   This returns, in order, the bioseq_handles that will be gathered for
*   formatting.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/format/gather_iter.hpp>

#include <objtools/format/flat_file_config.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CGather_Iter::CGather_Iter( 
    const CSeq_entry_Handle& top_seq_entry, 
    const CFlatFileConfig& config )
    : m_Config(config)
{
    x_AddSeqEntryToStack( top_seq_entry );
}

CGather_Iter::operator bool(void) const
{
    return NULL != m_BioseqIter.get() || ! m_SeqEntryIterStack.empty();
}


CGather_Iter &CGather_Iter::operator++(void)
{
    _ASSERT( NULL != m_BioseqIter.get() );

    ++(*m_BioseqIter);
    for ( ; (*m_BioseqIter); ++(*m_BioseqIter) ) {
        if( ! x_IsBioseqHandleOkay(**m_BioseqIter) ) {
            continue;
        }

        // The next one is good, so keep it
        return *this;
    }
    // m_BioseqIter is exhausted, so we need to climb upwards
    m_BioseqIter.reset();

    // The m_BioseqIter is exhausted, so we need to find the next one.
    while( ! m_SeqEntryIterStack.empty() ) {
        CSeq_entry_CI& lowest_seq_entry_iter = m_SeqEntryIterStack.back();
        ++lowest_seq_entry_iter;
        if( *lowest_seq_entry_iter ) {
            // lowest one still valid, try to add what's under the next Seq-entry
            if( x_AddSeqEntryToStack(*lowest_seq_entry_iter) ) {
                return *this;
            }
        } else {
            // This one's exhausted.  Pop off that one, and try iterating the next one up.
            m_SeqEntryIterStack.pop_back();
        }
    }

    return *this;
}

const CBioseq_Handle &CGather_Iter::operator*(void) const
{
    _ASSERT(*m_BioseqIter);
    return **m_BioseqIter;
}

bool CGather_Iter::x_AddSeqEntryToStack( 
    const CSeq_entry_Handle& entry )
{
    _ASSERT( m_SeqEntryIterStack.empty() || 
        *m_SeqEntryIterStack.back() == entry );

    if ( entry.IsSet()  &&  entry.GetSet().IsSetClass() ) {
        CBioseq_set::TClass clss = entry.GetSet().GetClass();
        if ( clss == CBioseq_set::eClass_genbank  ||
            clss == CBioseq_set::eClass_mut_set  ||
            clss == CBioseq_set::eClass_pop_set  ||
            clss == CBioseq_set::eClass_phy_set  ||
            clss == CBioseq_set::eClass_eco_set  ||
            clss == CBioseq_set::eClass_wgs_set  ||
            clss == CBioseq_set::eClass_gen_prod_set ) 
        {
            CSeq_entry_CI it(entry);
            if( it ) {
                for ( ; it; ++it ) {
                    m_SeqEntryIterStack.push_back(it);
                    if( x_AddSeqEntryToStack(*it) ) {
                        return true;
                    }
                    m_SeqEntryIterStack.pop_back();
                }
            }
            return false;
        }
    }

    CSeq_inst::TMol mol_type;
    if (m_Config.IsViewAll()) {
        mol_type = CSeq_inst::eMol_not_set;
    } else if (m_Config.IsViewNuc()) {
        mol_type = CSeq_inst::eMol_na;
    } else if (m_Config.IsViewProt()) {
        mol_type = CSeq_inst::eMol_aa;
    } else {
        return false;
    }

    auto_ptr<CBioseq_CI> seq_iter( new CBioseq_CI(entry, mol_type, CBioseq_CI::eLevel_Mains) );
    for( ; (*seq_iter); ++(*seq_iter) ) {
        if( ! x_IsBioseqHandleOkay(**seq_iter) ) {
            continue;
        }

        // found a good one
        m_BioseqIter = seq_iter;
        return true;
    }
    return false;
}

bool CGather_Iter::x_IsBioseqHandleOkay( const CBioseq_Handle &bioseq )
{
    CSeq_id_Handle id = sequence::GetId(bioseq, sequence::eGetId_Best);
    if (  m_Config.SuppressLocalId() && id.GetSeqId()->IsLocal() ) {
        return false;
    }

    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE
