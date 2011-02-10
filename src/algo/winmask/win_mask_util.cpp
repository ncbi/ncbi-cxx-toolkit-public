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
 *   WindowMasker helper functions (implementation).
 *
 */

#include <ncbi_pch.hpp>

#include <vector>

#include <objmgr/util/sequence.hpp>
#include <algo/winmask/win_mask_util.hpp>

#include <objtools/seqmasks_io/mask_fasta_reader.hpp>
#include <objtools/seqmasks_io/mask_bdb_reader.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//----------------------------------------------------------------------------
void CWinMaskUtil::CIdSet_SeqId::insert( const string & id_str )
{
    try {
        CRef<CSeq_id> id(new CSeq_id(id_str));
        idset.insert(CSeq_id_Handle::GetHandle(*id));
    } catch (CSeqIdException& e) {
        LOG_POST(Error
            << "CWinMaskConfig::FillIdList(): can't understand id: "
            << id_str << ": " << e.what() << ": ignoring");
    }
}

//----------------------------------------------------------------------------
bool CWinMaskUtil::CIdSet_SeqId::find( 
        const objects::CBioseq_Handle & bsh ) const
{
    const CBioseq_Handle::TId & syns = bsh.GetId();

    ITERATE( CBioseq_Handle::TId, iter, syns )
    {
        if( idset.find( *iter ) != idset.end() ) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
const vector< Uint4 > 
CWinMaskUtil::CIdSet_TextMatch::split( const string & id_str )
{
    vector< Uint4 > result;
    string tmp = id_str;

    if( !tmp.empty() && tmp[tmp.length() - 1] == '|' )
        tmp = tmp.substr( 0, tmp.length() - 1 );

    if( !tmp.empty() ) {
        string::size_type pos = ( tmp[0] == '>' ) ? 1 : 0;
        string::size_type len = tmp.length();

        while( pos != string::npos && pos < len ) {
            result.push_back( pos );

            if( (pos = tmp.find_first_of( "|", pos )) != string::npos ) {
                ++pos;
            }
        }
    }

    result.push_back( tmp.length() + 1 );
    return result;
}

//----------------------------------------------------------------------------
void CWinMaskUtil::CIdSet_TextMatch::insert( const string & id_str )
{
    Uint4 nwords = split( id_str ).size() - 1;

    if( nwords == 0 ) {
        LOG_POST( Error
            << "CWinMaskConfig::CIdSet_TextMatch::insert(): bad id: "
            << id_str << ": ignoring");
    }

    if( nword_sets_.size() < nwords ) {
        nword_sets_.resize( nwords );
    }

    if( id_str[id_str.length() - 1] != '|' ) {
        nword_sets_[nwords - 1].insert( id_str );
    }else {
        nword_sets_[nwords - 1].insert( 
                id_str.substr( 0, id_str.length() - 1 ) );
    }
}

//----------------------------------------------------------------------------
bool CWinMaskUtil::CIdSet_TextMatch::find( 
        const objects::CBioseq_Handle & bsh ) const
{
    CConstRef< CBioseq > seq = bsh.GetCompleteBioseq();
    string id_str = sequence::GetTitle( bsh );
    
    if( !id_str.empty() ) {
        string::size_type pos = id_str.find_first_of( " \t" );
        id_str = id_str.substr( 0, pos );
    }

    if( find( id_str ) ) return true;
    else if( id_str.substr( 0, 4 ) == "lcl|" ) {
        id_str = id_str.substr( 4, string::npos );
        return find( id_str );
    }
    else return false;
}

//----------------------------------------------------------------------------
inline bool CWinMaskUtil::CIdSet_TextMatch::find( 
        const string & id_str, Uint4 nwords ) const
{
    return nword_sets_[nwords].find( id_str ) != nword_sets_[nwords].end();
}

//----------------------------------------------------------------------------
bool CWinMaskUtil::CIdSet_TextMatch::find( const string & id_str ) const
{
    vector< Uint4 > word_starts = split( id_str );

    for( Uint4 i = 0; 
            i < nword_sets_.size() && i < word_starts.size() - 1; ++i ) {
        if( !nword_sets_[i].empty() ) {
            for( Uint4 j = 0; j < word_starts.size() - i - 1; ++j ) {
                string pattern = id_str.substr(
                        word_starts[j],
                        word_starts[j + i + 1] - word_starts[j] - 1 );

                if( find( pattern, i ) ) {
                    return true;
                }
            }
        }
    }

    return false;
}

CWinMaskUtil::CInputBioseq_CI::CInputBioseq_CI(const string & input_file,
                                 const string & input_format)
  : m_InputFile(new CNcbiIfstream(input_file.c_str()))
{
    if( input_format == "fasta" ) {
        m_Reader.reset( new CMaskFastaReader( *m_InputFile, true, false ) );
    }
    else if( input_format == "blastdb" ) {
        m_Reader.reset( new CMaskBDBReader( input_file ) );
    } else if( input_format != "seqids" ) {
        NCBI_THROW(CException, eUnknown,
                    "Invalid CInputBioseq_CI input format: " + input_format);
    }
    operator++();
}

CWinMaskUtil::CInputBioseq_CI& CWinMaskUtil::CInputBioseq_CI::operator++ (void)
{
    m_Scope.Reset(new CScope(*CObjectManager::GetInstance()));
    m_Scope->AddDefaults();
    m_CurrentBioseq.Reset();

    if(m_Reader.get()){
        CRef<CSeq_entry> next_entry = m_Reader->GetNextSequence();
        if( next_entry.NotEmpty() ){
            NCBI_ASSERT(next_entry->IsSeq(), "Reader returned bad entry");
            m_CurrentBioseq = m_Scope->AddTopLevelSeqEntry(*next_entry).GetSeq();
        }
    } else {
        // No reader; this means input is a list of gis, one per line
        string id;
        while (NcbiGetlineEOL(*m_InputFile, id)) {
            if(id.empty() || id[0] == '#')
                continue;
            m_CurrentBioseq = m_Scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
            break;
        }
    }

    return *this;
}

//------------------------------------------------------------------------------
bool CWinMaskUtil::consider( const objects::CBioseq_Handle & bsh,
                             const CIdSet * ids, const CIdSet * exclude_ids )
{
    if( (ids == 0 || ids->empty()) && 
        (exclude_ids == 0 || exclude_ids->empty()) ) {
        return true;
    }

    bool result = true;

    if( ids != 0 && !ids->empty() )
    {
        result = false;

        if( ids->find( bsh ) ) {
            result = true;
        }
    }

    if( exclude_ids != 0 && !exclude_ids->empty() )
    {
        if( exclude_ids->find( bsh ) ) {
            result = false;
        }
    }

    return result;
}

END_NCBI_SCOPE
