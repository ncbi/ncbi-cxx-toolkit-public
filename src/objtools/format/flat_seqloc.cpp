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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   new (early 2003) flat-file generator -- location representation
*
* ===========================================================================
*/



#include <ncbi_pch.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/misc/sequence_util_macros.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objtools/format/items/flat_seqloc.hpp>
#include <objtools/format/context.hpp>
#include <algorithm>
#include <objmgr/util/objutil.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);

CFlatSeqLoc::CGuardedToAccessionMap CFlatSeqLoc::m_ToAccessionMap;

static bool s_IsVirtualId(const CSeq_id_Handle& id, const CBioseq_Handle& seq)
{
    if (!id  ||  !seq) {
        return true;
    }
    CBioseq_Handle::TId ids = seq.GetId();
    if (find(ids.begin(), ids.end(), id) == ids.end()) {
        CBioseq_Handle bsh = seq.GetScope().GetBioseqHandle(id, CScope::eGetBioseq_Loaded);
        return bsh ? bsh.GetInst_Repr() == CSeq_inst::eRepr_virtual : false;
    }
    return false;
}


static bool s_IsVirtualSeqInt
(const CSeq_interval& seqint,
 const CBioseq_Handle& seq)
{
    return seqint.IsSetId() ?
        s_IsVirtualId(CSeq_id_Handle::GetHandle(seqint.GetId()), seq) :
        false;
}


static bool s_IsVirtualLocation(const CSeq_loc& loc, const CBioseq_Handle& seq)
{
    const CSeq_id* id = loc.GetId();
    return id ?
        s_IsVirtualId(CSeq_id_Handle::GetHandle(*id), seq) :
        false;
}


static bool s_NeedsFlattening (const CSeq_loc &loc)

{
    if (! loc.IsMix()) return false;

    ITERATE (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
        if ((*it)->IsPacked_int()) {
            return true;
        }
    }

    return false;
}


static CConstRef<CSeq_loc> s_FlattenLoc (const CSeq_loc &loc)

{
    bool any_nulls_seen = false;
    CRef<CSeq_loc_mix> new_mix(new CSeq_loc_mix);
    CSeq_loc_mix::Tdata & new_mix_pieces = new_mix->Set();

    ITERATE (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
        const CSeq_loc& curr = **it;
        if (curr.IsNull()) {
            any_nulls_seen = true;
        } else if (curr.IsPacked_int()) {
            ITERATE (CPacked_seqint::Tdata, pit, curr.GetPacked_int().Get()) {
                if( any_nulls_seen && ! new_mix_pieces.empty() ) {
                    CRef<CSeq_loc> null_piece( new CSeq_loc );
                    null_piece->SetNull();
                    new_mix_pieces.push_back( null_piece );
                }
                CRef<CSeq_loc> old_piece(new CSeq_loc);
                const CSeq_interval& seqint = **pit;
                old_piece->SetInt().Assign(seqint);
                new_mix_pieces.push_back( old_piece );
            }
        } else {
            if( any_nulls_seen && ! new_mix_pieces.empty() ) {
                CRef<CSeq_loc> null_piece( new CSeq_loc );
                null_piece->SetNull();
                new_mix_pieces.push_back( null_piece );
            }
            CRef<CSeq_loc> old_piece(new CSeq_loc);
            old_piece->Assign(curr);
            new_mix_pieces.push_back( old_piece );
        }
    }

    CRef<CSeq_loc> new_loc(new CSeq_loc);
    new_loc->SetMix(*new_mix);

    return new_loc;
}


CFlatSeqLoc::CFlatSeqLoc
(const CSeq_loc& loc,
 CBioseqContext& ctx,
 TType type,
 bool show_all_accns,
 bool add_join,
 bool suppress_accession)
{
    // load the map that caches conversion to accession, because
    // it's *much* faster when done in bulk vs. one at a time.
    // Try accession DG000029 for an example that's really sped up
    // by this code, or gbcon180 in general.
    {
        set<CSeq_id_Handle> handles_set;
        CSeq_loc_CI loc_ci( loc );
        for( ; loc_ci; ++loc_ci ) {
            CSeq_id_Handle handle = loc_ci.GetSeq_id_Handle();

            // skip ones whose value we've already cached
            CSeq_id_Handle cached_handle = m_ToAccessionMap.Get(handle);
            if( cached_handle ) {
                continue;
            }

            // if it's already an accession, then it maps to itself
            if( x_IsAccessionVersion( handle ) ) {
                m_ToAccessionMap.Insert( handle, handle );
                continue;
            }

            // don't translate ones that are synonyms of this bioseq
            if (! show_all_accns) {
                if( ctx.GetHandle().IsSynonym( loc_ci.GetSeq_id() ) ) {
                    continue;
                }
            }

            handles_set.insert( handle );
        }
        if( ! handles_set.empty() )
        {
            vector<CSeq_id_Handle> handles_vec;
            copy( handles_set.begin(), handles_set.end(),
                back_inserter(handles_vec) );

            CScope::TSeq_id_Handles results;
            // GetAccVers will divide our request into smaller requests,
            // so don't worry about overwhelming ID2
            ctx.GetScope().GetAccVers( &results, handles_vec );
            _ASSERT( handles_vec.size() == results.size() );
            for( unsigned int id_idx = 0; id_idx < handles_vec.size(); ++id_idx ) {
                CSeq_id_Handle acc_handle = results[id_idx];
                if( acc_handle ) {
                    m_ToAccessionMap.Insert(
                        handles_vec[id_idx], acc_handle );
                }
            }
        }

    }

    ostringstream oss;
    if (s_NeedsFlattening (loc)) {
        CConstRef<CSeq_loc> flat = s_FlattenLoc (loc);
        x_Add(*flat, oss, ctx, type, true, show_all_accns, add_join, suppress_accession);
    } else {
        x_Add(loc, oss, ctx, type, true, show_all_accns, add_join, suppress_accession);
    }
    oss.str().swap( m_String );
}


bool CFlatSeqLoc::x_Add(
    const CSeq_loc& loc,
    ostream& oss,
    CBioseqContext& ctx,
    TType type,
    bool show_comp,
    bool show_all_accns,
    bool add_join,
    bool suppress_accession)
{
    CScope& scope = ctx.GetScope();
    const CBioseq_Handle& seq = ctx.GetHandle();

    // some later logic needs to know we're inside an "order"
    bool is_flat_order = false;

    const char* prefix = "join(";

    // deal with complement of entire location
    if ( type == eType_location ) {
        if ( show_comp  &&  GetStrand(loc, &scope) == eNa_strand_minus ) {
            CRef<CSeq_loc> rev_loc(SeqLocRevCmpl(loc, &scope));
            oss << "complement(";
            x_Add(*rev_loc, oss, ctx, type, false, show_all_accns, add_join, suppress_accession);
            oss << ')';
            return true;
        }

        if ( loc.IsMix() ) {
            ITERATE (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
                if ( (*it)->IsNull()  ||  s_IsVirtualLocation(**it, seq) ) {
                    prefix = "order(";
                    is_flat_order = true;
                    break;
                }
            }
        } else if ( loc.IsPacked_int() ) {
            ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
                if ( s_IsVirtualSeqInt(**it, seq) ) {
                    prefix = "order(";
                    is_flat_order = true;
                    break;
                }
            }
        }
    }

    // handle each location component
    switch ( loc.Which() ) {
    case CSeq_loc::e_Null:
    {{
        const CFlatGapLoc* gap = dynamic_cast<const CFlatGapLoc*>(&loc);
        if (! gap) {
            oss << "gap()";
            break;
        }
        size_t uLength = gap->GetLength();
        const CInt_fuzz* fuzz = gap->GetFuzz();
        oss << "gap(";
        if (fuzz  &&  fuzz->IsLim()  &&
            fuzz->GetLim() == CInt_fuzz::eLim_unk) {
            oss << "unk";
        }
        oss << uLength << ")";

        //oss << (uLength==100 ? "gap(" : "gap(") << uLength << ")";
        break;
    }}
    case CSeq_loc::e_Empty:
    {{
        oss << "gap()";
        break;
    }}
    case CSeq_loc::e_Whole:
    {{
        if (add_join)
            oss << prefix;
        x_AddID(loc.GetWhole(), oss, ctx, type, show_all_accns, suppress_accession);
        TSeqPos len = sequence::GetLength(loc, &scope);
        oss << "1";
        if (len > 1) {
            oss << ".." << len;
        }
        if (add_join)
            oss << ')';
        break;
    }}
    case CSeq_loc::e_Int:
    {{
        if (add_join)
            oss << prefix;
        x_Add(loc.GetInt(), oss, ctx, type, show_comp, show_all_accns, suppress_accession);
        if (add_join)
            oss << ')';
        break;
    }}
    case CSeq_loc::e_Packed_int:
    {{
        // Note: At some point, we should add the "join inside order" logic here (like for the e_Mix case), but since I didn't immediately find a
        // test case in the repository, I'm taking the conservative approach and leaving it alone for now.
        oss << prefix;
        const char* delim = "";
        ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            oss << delim;
            if (!x_Add(**it, oss, ctx, type, show_comp, show_all_accns, suppress_accession)) {
                delim = "";
            } else {
                delim = ",";
            }
        }
        oss << ')';
        break;
    }}
    case CSeq_loc::e_Pnt:
    {{
        if (add_join)
            oss << prefix;
        x_Add(loc.GetPnt(), oss, ctx, type, show_comp, show_all_accns, suppress_accession);
        if (add_join)
            oss << ')';
        break;
    }}
    case CSeq_loc::e_Packed_pnt:
    {{
        const CPacked_seqpnt& ppnt  = loc.GetPacked_pnt();
        ENa_strand strand = ppnt.IsSetStrand() ? ppnt.GetStrand() : eNa_strand_unknown;
        x_AddID(ppnt.GetId(), oss, ctx, type, show_all_accns, suppress_accession);
        if (strand == eNa_strand_minus  &&  show_comp) {
            oss << "complement(";
        }
        oss << prefix;
        const char* delim = "";
        ITERATE (CPacked_seqpnt::TPoints, it, ppnt.GetPoints()) {
            oss << delim;
            const CInt_fuzz* fuzz = ppnt.CanGetFuzz() ? &ppnt.GetFuzz() : nullptr;
            if (!x_Add(*it, fuzz, oss, ( ctx.Config().DoHTML() ? eHTML_Yes : eHTML_None ), eForce_None, eSource_Other, show_all_accns, suppress_accession )) {
                delim = "";
            } else {
                delim = ",";
            }
        }
        if (strand == eNa_strand_minus  &&  show_comp) {
            oss << ")";
        }
        break;
    }}
    case CSeq_loc::e_Mix:
    {{
         /// odd corner case:
         /// a mix with one interval should not have a prefix, except when
         /// prefix is requested unconditionally.
        const bool print_virtual = ( type != eType_location );
         CSeq_loc_CI it(loc, CSeq_loc_CI::eEmpty_Allow);
         ++it;
         bool has_one = !it;
         it.Rewind();

         const char* delim = "";
         if ( !has_one || add_join ) {
             oss << prefix;
         }
         bool join_inside_order = false; // true when we're inside a join() inside an order()
         bool next_is_virtual = ( !it || it.GetEmbeddingSeq_loc().IsNull() || s_IsVirtualLocation( it.GetEmbeddingSeq_loc(), seq ) );
         for (  ; it; ++it ) {
             oss << delim;

             const CSeq_loc& this_loc = it.GetEmbeddingSeq_loc();

             // save some work by using what was done on the last loop iteration
             // (this is set before the loop on the first iteration)
             const bool this_is_virtual = next_is_virtual;

             // get iterator to next one
             CSeq_loc_CI next = it;
             ++next;

             // begin join in order, if necessary
             next_is_virtual = ( ! next || next.GetEmbeddingSeq_loc().IsNull() || s_IsVirtualLocation( next.GetEmbeddingSeq_loc(), seq ) );
             if( is_flat_order ) {
                 if( ( this_loc.IsInt() || this_loc.IsPnt() ) &&
                     ! join_inside_order && ! this_is_virtual && ! next_is_virtual ) {
                     oss << "join(";
                     join_inside_order = true;
                 }
             }

             // skip gaps, etc.
             if( this_is_virtual && ! print_virtual ) {
                 delim = "";
             } else {
                 // add the actual location
                 if (!x_Add(this_loc, oss, ctx, type, show_comp, show_all_accns, false, suppress_accession)) {
                     delim = "";
                 } else {
                     delim = ",";
                 }
             }

             // end join in order, if necessary
             if( is_flat_order ) {
                 if( join_inside_order && next_is_virtual ) {
                     oss << ')';
                     join_inside_order = false;
                 }
             }
         }
         if( join_inside_order ) {
             oss << ')';
         }
         if ( !has_one || add_join ) {
             oss << ')';
         }
        break;
    }}
    case CSeq_loc::e_Equiv:
    {{
        const char* delim = "";
        oss << "one-of(";
        ITERATE (CSeq_loc_equiv::Tdata, it, loc.GetEquiv().Get()) {
            oss << delim;
            if (!x_Add(**it, oss, ctx, type, show_comp, show_all_accns, suppress_accession)) {
                delim = "";
            } else {
                delim = ",";
            }
        }
        oss << ')';
        break;
    }}
    case CSeq_loc::e_Bond:
    {{
        const CSeq_bond& bond = loc.GetBond();
        if ( !bond.CanGetA() ) {
            return false;
        }
        if (add_join)
            oss << prefix;
        oss << "bond(";
        x_Add(bond.GetA(), oss, ctx, type, show_comp, show_all_accns, suppress_accession);
        if ( bond.CanGetB() ) {
            oss << ",";
            x_Add(bond.GetB(), oss, ctx, type, show_comp, show_all_accns, suppress_accession);
        }
        oss << ")";
        if (add_join)
            oss << ")";
        break;
    }}
    case CSeq_loc::e_Feat:
    default:
        return false;
        /*NCBI_THROW(CException, eUnknown,
                   "CFlatSeqLoc::CFlatSeqLoc: unsupported (sub)location type "
                   + NStr::IntToString(loc.Which()));*/
    } // end of switch statement

    return true;
}


bool CFlatSeqLoc::x_FuzzToDisplayed(const CSeq_interval& si) const
{
    if (!si.IsSetFuzz_to()) {
        return false;
    }

    const CInt_fuzz& to_fuzz = si.GetFuzz_to();
/* Range fuzziness is not displayed
    if (to_fuzz.IsRange()) {
        return true;
    }
*/
    if (to_fuzz.IsLim()) {
        switch( to_fuzz.GetLim() ) {
        case CInt_fuzz::eLim_tr:
        case CInt_fuzz::eLim_gt:
        case CInt_fuzz::eLim_tl:
        case CInt_fuzz::eLim_lt:
            return true;
        default:
            break;
        }
    }
    return false;
}



bool CFlatSeqLoc::x_Add(
    const CSeq_interval& si,
    ostream& oss,
    CBioseqContext& ctx,
    TType type,
    bool show_comp,
    bool show_all_accns,
    bool suppress_accession)
{
    bool do_html = ctx.Config().DoHTML();

    TSeqPos from = si.GetFrom(), to = si.GetTo();
    ENa_strand strand = si.CanGetStrand() ? si.GetStrand() : eNa_strand_unknown;
    bool comp = show_comp  &&  (strand == eNa_strand_minus);

    if (type == eType_location  &&
        s_IsVirtualId(CSeq_id_Handle::GetHandle(si.GetId()), ctx.GetHandle()) ) {
        return false;
    }
    if (comp) {
        oss << "complement(";
    }
    x_AddID(si.GetId(), oss, ctx, type, show_all_accns, suppress_accession);

    // get the fuzz we need, but certain kinds of fuzz do not belong in an interval
    const CSeq_interval::TFuzz_from *from_fuzz = (si.IsSetFuzz_from() ? &si.GetFuzz_from() : nullptr);

    x_Add(from, from_fuzz, oss, ( do_html ? eHTML_Yes : eHTML_None ));

    if ( (type == eType_assembly) ||
         ((from != to || x_FuzzToDisplayed(si)) ) )
    {
        oss << "..";

        const CSeq_interval::TFuzz_from *to_fuzz = (si.IsSetFuzz_to() ? &si.GetFuzz_to() : nullptr);

        x_Add(to, to_fuzz, oss, ( do_html ? eHTML_Yes : eHTML_None ));
    }
    if (comp) {
        oss << ')';
    }

    return true;
}


bool CFlatSeqLoc::x_Add(
    const CSeq_point& pnt,
    ostream& oss,
    CBioseqContext& ctx,
    TType type,
    bool show_comp,
    bool show_all_accns,
    bool suppress_accession)
{
    if ( !pnt.CanGetPoint() ) {
        return false;
    }

    const bool do_html = ctx.Config().DoHTML();
    const bool is_comp =
        pnt.IsSetStrand()  &&  IsReverse(pnt.GetStrand())  &&  show_comp;

    TSeqPos pos = pnt.GetPoint();
    x_AddID(pnt.GetId(), oss, ctx, type, show_all_accns, suppress_accession);
    if( is_comp ) {
        oss << "complement(";
    }
    x_Add(pos, pnt.IsSetFuzz() ? &pnt.GetFuzz() : nullptr,
        oss, ( do_html ? eHTML_Yes : eHTML_None ),
        ( (eType_assembly == type) ? eForce_ToRange : eForce_None ),
        eSource_Point );
    if( is_comp ) {
        oss << ')';
    }

    return true;
}


bool CFlatSeqLoc::x_Add(
    TSeqPos pnt,
    const CInt_fuzz* fuzz,
    ostream& oss,
    EHTML html,
    EForce force,
    ESource source,
    bool show_all_accns,
    bool suppress_accession)
{
    // need to convert to 1-based coordinates
    pnt += 1;

    const bool bFromSeqPoint = (source == eSource_Point);

    if (fuzz) {
        switch ( fuzz->Which() ) {
        case CInt_fuzz::e_P_m:
            {
                oss << '(' << pnt - fuzz->GetP_m() << '.';
                if( bFromSeqPoint ) {
                    oss << pnt << ")..(" << pnt << '.';
                }
                oss << pnt + fuzz->GetP_m() << ')';
                break;
            }
        case CInt_fuzz::e_Range:
            {
                oss << ( bFromSeqPoint ? "" : "(" )
                    << (fuzz->GetRange().GetMin() + 1)
                    << ( bFromSeqPoint ? '^' : '.' )
                    << (fuzz->GetRange().GetMax() + 1)
                    << ( bFromSeqPoint ? "" : ")");
                break;
            }
        case CInt_fuzz::e_Pct: // actually per thousand...
            {
                // calculate in floating point to avoid overflow (or underflow)
                const double delta = 0.001 * pnt * fuzz->GetPct();
                const long start = static_cast<long>(pnt - delta);
                const long end   = static_cast<long>(pnt + delta);
                if( bFromSeqPoint ) {
                    oss << start << '^' << end;
                } else {
                    oss << '(' << start << '.' << end << ')';
                }
                break;
            }
        case CInt_fuzz::e_Lim:
            {
                switch ( fuzz->GetLim() ) {
                case CInt_fuzz::eLim_tr:
                    if( bFromSeqPoint ) {
                        oss << pnt << '^' << (pnt + 1);
                        break;
                    }
                    // sometimes FALL-THROUGH
                case CInt_fuzz::eLim_gt:
                    oss << (html == eHTML_Yes ? "&gt;" : ">") << pnt;
                    break;
                case CInt_fuzz::eLim_tl:
                    if( bFromSeqPoint ) {
                        oss << (pnt - 1) << '^' << pnt;
                        break;
                    }
                    // sometimes FALL-THROUGH
                case CInt_fuzz::eLim_lt:
                    oss << (html == eHTML_Yes ? "&lt;" : "<") << pnt;
                    break;
                default:
                    oss << pnt;
                    if( force == eForce_ToRange ) {
                        oss << ".." << pnt;
                    }
                    break;
                }
                break;
            }
        default:
            {
                oss << pnt;
                if( force == eForce_ToRange ) {
                    oss << ".." << pnt;
                }
                break;
            }
        } // end of switch statement
    } else {
        oss << pnt;
        if( force == eForce_ToRange ) {
            oss << ".." << pnt;
        }
    }

    return true;
}


void CFlatSeqLoc::x_AddID(
    const CSeq_id& id,
    ostream& oss,
    CBioseqContext& ctx,
    TType type,
    bool show_all_accns,
    bool suppress_accession)
{
    const bool do_html = ( ctx.Config().DoHTML() && type == eType_assembly);

    if (! show_all_accns) {
        if (ctx.GetHandle().IsSynonym(id)) {
            if ( type == eType_assembly ) {
                oss << ctx.GetAccession() << ':';
            }
            return;
        }
    }

    if (suppress_accession) {
        return;
    }

    CConstRef<CSeq_id> idp;
    try {
        CSeq_id_Handle handle =
            m_ToAccessionMap.Get( CSeq_id_Handle::GetHandle(id) );
        if( handle ) {
            idp = handle.GetSeqId();
        }
    } catch (CException&) {
        idp.Reset();
    }
    if (!idp) {
        idp.Reset(&id);
    }
    switch ( idp->Which() ) {
    default:
        oss << idp->GetSeqIdString(true) << ':';
        break;
    case CSeq_id::e_Gi:
        if( do_html ) {
            const string gi_str = idp->GetSeqIdString(true);
            oss << "<a href=\"" << strLinkBaseEntrezViewer << gi_str << "\">gi|" << gi_str << "</a>:";
        } else {
            oss << "gi|" << idp->GetSeqIdString(true) << ':';
        }
        break;
    }
}

bool CFlatSeqLoc::x_IsAccessionVersion( CSeq_id_Handle id )
{
    CConstRef<CSeq_id> seq_id = id.GetSeqIdOrNull();
    if( ! seq_id ) {
        return false;
    }

    return (seq_id->GetTextseq_Id() != nullptr);
}

void
CFlatSeqLoc::CGuardedToAccessionMap::Insert(
    CSeq_id_Handle from, CSeq_id_Handle to )
{
    CFastMutexGuard guard(m_MutexForTheMap);
    m_TheMap.insert( CFlatSeqLoc::TToAccessionMap::value_type(from, to) );
}

CSeq_id_Handle
CFlatSeqLoc::CGuardedToAccessionMap::Get( CSeq_id_Handle query )
{
    CFastMutexGuard guard(m_MutexForTheMap);
    CFlatSeqLoc::TToAccessionMap::const_iterator map_iter =
        m_TheMap.find(query);
    if( map_iter == m_TheMap.end() ) {
        return CSeq_id_Handle();
    } else {
        return map_iter->second;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
