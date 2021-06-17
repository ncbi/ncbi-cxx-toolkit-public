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
 *   Header file for CSRSearch and some related declarations.
 *
 */

#ifndef C_SR_SEARCH_HPP
#define C_SR_SEARCH_HPP

#include <objmgr/seq_vector.hpp>
#include <algo/blast/dbindex/dbindex.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( dbindex_search )

USING_SCOPE( ncbi::objects );
USING_SCOPE( ncbi::blastdbindex );

//----------------------------------------------------------------------------
class CSRSearch : public CObject
{
    public:

        enum ELevel { PE, PM, SE, SM, EM };

        typedef CDbIndex::TSeqNum TSeqNum;

        struct SSearchData
        {
            const CSeqVector * seq_1;
            const CSeqVector * seq_2;
            Uint4 num_res;
            Uint4 nr_1;
            Uint4 nr_2;
            ELevel rlevel_1;
            ELevel rlevel_2;
            bool mismatch;

            SSearchData( 
                    const CSeqVector & seq, Uint4 nr, 
                    Uint4 nr1, Uint4 nr2, ELevel rl1, ELevel rl2, 
                    bool exact = false )
                : seq_1( &seq ), seq_2( 0 ), num_res( nr ), 
                  nr_1( nr1 ), nr_2( nr2 ), 
                  rlevel_1( rl1 ), rlevel_2( rl2 ),
                  mismatch( !exact )
            {}

            SSearchData( 
                    const CSeqVector & seq1, 
                    const CSeqVector & seq2,
                    Uint4 nr, 
                    Uint4 nr1, Uint4 nr2, ELevel rl1, ELevel rl2, 
                    bool exact = false )
                : seq_1( &seq1 ), seq_2( &seq2 ), 
                  num_res( nr ),
                  nr_1( nr1 ), nr_2( nr2 ), 
                  rlevel_1( rl1 ), rlevel_2( rl2 ),
                  mismatch( !exact )
            {}
        };

        struct SResultData
        {
            bool    pair;
            Uint1   type;
            TSeqNum snum;
            TSeqPos spos_1;
            bool    fw_strand_1;
            TSeqPos mpos_1;
            Uint1   mbase_1;
            TSeqPos spos_2;
            bool    fw_strand_2;
            TSeqPos mpos_2;
            Uint1   mbase_2;

            SResultData( 
                    TSeqNum subject, 
                    TSeqPos sp,
                    bool fw_strand,
                    TSeqPos mp = 0,
                    Uint1 mbase = '-',
                    Uint1 tp = 0 )
                : pair( false ),
                  type( tp ),
                  snum( subject ),
                  spos_1( sp ),
                  fw_strand_1( fw_strand ),
                  mpos_1( mp ),
                  mbase_1( mbase )
            {}

            SResultData(
                    void *,
                    TSeqNum subject,
                    TSeqPos sp1,
                    TSeqPos sp2,
                    bool fw1,
                    bool fw2, 
                    TSeqPos mp1 = 0,
                    TSeqPos mp2 = 0,
                    Uint1 mb1 = '-',
                    Uint1 mb2 = '-' )
                : pair( true ),
                  type( 0 ),
                  snum( subject ),
                  spos_1( sp1 ),
                  fw_strand_1( fw1 ),
                  mpos_1( mp1 ),
                  mbase_1( mb1 ),
                  spos_2( sp2 ),
                  fw_strand_2( fw2 ),
                  mpos_2( mp2 ),
                  mbase_2( mb2 )
            {}
        };
        
        struct SResults
        {
            ELevel level_1;
            ELevel level_2;
            Uint4 nres_1;
            vector< SResultData > res;
        };

        typedef SResults TResults;

        static CRef< CSRSearch > MakeSRSearch( 
                CRef< CDbIndex > index, 
                TSeqPos d = 0, TSeqPos dfuzz = 0 );

        virtual void search( const SSearchData & sdata, TResults & results ) = 0;

    public: // for Solaris only

        static const Uint4 TSRRESULTS_INISIZE = 1000000UL;
        static const Uint4 TSRPAIRS_INISIZE   = 10000UL;

    protected:

        struct SSRResult
        {
            Uint4 seqnum;
            Uint4 soff;
        };

    public: // for Solaris only

        typedef vector< SSRResult >          TSRResults;
        typedef pair< SSRResult, SSRResult > TSRPairedResult;
        typedef vector< TSRPairedResult >    TSRPairedResults;

        class CResCache
        {
            private:

                vector< Uint1 > f_ind_;
                vector< Uint1 > r_ind_;
                vector< TSRResults > f_res_cache_;
                vector< TSRResults > r_res_cache_;

                Uint4 size() { return (Uint4)f_ind_.size(); }

                void resize( Uint4 new_sz )
                {
                    if( new_sz != size() ) {
                        f_ind_.resize( new_sz );
                        r_ind_.resize( new_sz );

                        Uint4 start = size();
                        Uint4 end = new_sz;

                        f_res_cache_.resize( new_sz );
                        r_res_cache_.resize( new_sz );
                    
                        for( Uint4 i = start; i < end; ++i ) {
                            f_res_cache_[i].reserve( TSRRESULTS_INISIZE );
                            r_res_cache_[i].reserve( TSRRESULTS_INISIZE );
                        }
                    }
                }

            public:

                void init( Uint4 sz )
                {
                    resize( sz );
                    fill( f_ind_.begin(), f_ind_.end(), 0 );
                    fill( r_ind_.begin(), r_ind_.end(), 0 );
                }

                bool is_set( Uint4 idx, bool fw_strand ) const 
                { return fw_strand ? (bool)f_ind_[idx] : (bool)r_ind_[idx]; }

                void set( Uint4 idx, bool fw_strand )
                {
                    if( fw_strand ) {
                        f_ind_[idx] = 1;
                        f_res_cache_[idx].clear();
                    }
                    else {
                        r_ind_[idx] = 1;
                        r_res_cache_[idx].clear();
                    }
                }

                TSRResults & at( Uint4 idx, bool fw_strand ) 
                { return fw_strand ? f_res_cache_[idx] : r_res_cache_[idx]; }
        };

    protected:

        struct SMismatchInfo
        {
            Uint4   idx;
            Uint4   num_keys;
            TSeqPos key_pos[2];
        };

    public: // for Solaris only
        
        struct SMismatchResultsEntry
        {
            TSRResults results;
            TSeqPos mismatch_position;
            Uint4 adjustment;
            Uint1 mismatch_letter;

            SMismatchResultsEntry() {}

            SMismatchResultsEntry( TSeqPos pos, Uint4 letter, Uint1 adj )
            { init( pos, letter, adj ); }

            void init( TSeqPos pos, Uint4 letter, Uint1 adj )
            {
                mismatch_position = pos;
                adjustment = adj;
                mismatch_letter = letter;
            }
        };

        typedef vector< SMismatchResultsEntry > TMismatchResults;

        class CMismatchResultsInfo
        {
                static const Uint4 BLOCK_SHIFT     = 7UL;
                static const Uint4 BLOCK_SIZE      = (1UL<<BLOCK_SHIFT);
                static const Uint4 BLOCK_MASK      = (BLOCK_SIZE - 1);
                static const Uint4 BLOCKS_INISIZE  = 1024UL;
                static const Uint4 RESULTS_INISIZE = 1024UL*10UL;

                typedef vector< TMismatchResults > TBlocks;

            public:

                CMismatchResultsInfo() : size_( 0 )
                {
                    blocks_.reserve( BLOCKS_INISIZE );
                }

                void clear() { size_ = 0; }
                Uint4 size() const { return size_; }
                bool empty() const { return size_ == 0; }

                SMismatchResultsEntry & operator[]( Uint4 i )
                {
                    if( i >= size_ ) resize( i + 1 );
                    return at( i );
                }

                void resize( Uint4 i )
                {
                    size_ = i;

                    if( size_ == 0 ) return;

                    Uint4 block = ((i-1)>>BLOCK_SHIFT);

                    while( block >= blocks_.size() ) {
                        blocks_.push_back( TMismatchResults( BLOCK_SIZE ) );
                        TMismatchResults & block = *blocks_.rbegin();

                        for( Uint4 j = 0; j < BLOCK_SIZE; ++j ) {
                            block[j].results.reserve( RESULTS_INISIZE );
                        }
                    }
                }

            private:

                SMismatchResultsEntry & at( Uint4 i )
                { return blocks_[i>>BLOCK_SHIFT][i&BLOCK_MASK]; }

                Uint4 size_;
                TBlocks blocks_;
        };

    protected:

        struct SHKData
        {
            SHKData()
            {
                exact_1[0].reserve( TSRRESULTS_INISIZE );
                exact_1[1].reserve( TSRRESULTS_INISIZE );
                exact_2[0].reserve( TSRRESULTS_INISIZE );
                exact_2[1].reserve( TSRRESULTS_INISIZE );

                pres.reserve( TSRPAIRS_INISIZE );
            }

            void reset()
            {
            }

            CResCache rc1, rc2;
            TSRResults exact_1[2];
            TSRResults exact_2[2];

            CMismatchResultsInfo mm_1[2];
            CMismatchResultsInfo mm_2[2];

            TSRPairedResults pres;
        };

#ifdef NCBI_COMPILER_MIPSPRO
    public:
#endif
        class InternalException : public CException
        {
            public:

                enum EErrCode
                {
                    eAmbig
                };

                virtual const char * GetErrCodeString() const;
    
                NCBI_EXCEPTION_DEFAULT( InternalException, CException );
        };

    protected:
        CSRSearch( CRef< CDbIndex > index, TSeqPos d, TSeqPos dfuzz ) 
            : index_( index ), dmax_( d + dfuzz ), dmin_( d - dfuzz )
        {
            ASSERT( d >= dfuzz );
            hkey_width_ = index_->getHKeyWidth();
        }

        vector< TSeqPos > GetQNmerPositions( TSeqPos sz ) const;

        Uint4 getNMer( 
                const CSeqVector & seq, TSeqPos pos, 
                bool fw, bool & ambig ) const;

        Uint4 getNMer( 
                const CSeqVector & seq, TSeqPos pos, 
                bool fw, bool & ambig,
                TSeqPos sub_pos, Uint1 sub_letter ) const;

        pair< TSeqPos, TSeqPos > Pos2Index( 
                TSeqPos pos, TSeqPos sz, SMismatchInfo & mismatch_info ) const;

        void mergeResults( 
                TSRResults & results, const TSRResults & src, Int4 step );

        void combine( const TSRResults & results_1,
                      const TSRResults & results_2,
                      TSRPairedResults & results,
                      Int4 adjust = 0 );

        pair< TSeqNum, TSeqPos > MapSOff( 
                TSeqNum lid, TSeqPos loff, TSeqPos sz, bool & overlap ) const;

        bool reportResults(
                TResults & r, Uint4 nr,
                TSeqPos qsz1, TSeqPos qsz2, 
                const TSRPairedResults & results, bool fw1, bool fw2, 
                bool mismatch1 = false, bool mismatch2 = false,
                TSeqPos mismatch_pos1 = 0, TSeqPos mismatch_pos2 = 0,
                Uint1 mismatch_letter1 = (Uint1)'-', 
                Uint1 mismatch_letter2 = (Uint1)'-',
                Uint4 adjustment1 = 0, Uint4 adjustment2 = 0 );

        bool reportResults( 
                TResults & r, Uint4 nr,
                TSeqPos qsz, const TSRResults & results, 
                bool fw, bool mismatch = false, 
                TSeqPos mismatch_pos = 0, Uint1 mismatch_letter = (Uint1)'-',
                Uint4 adjustment = 0, Uint4 pos_in_pair = 0 );

        unsigned long hkey_width_;
        SHKData hk_data_;

    private:

        CRef< CDbIndex > index_;
        TSeqPos dmax_, dmin_;
};

END_SCOPE( dbindex_search )
END_NCBI_SCOPE

#endif

