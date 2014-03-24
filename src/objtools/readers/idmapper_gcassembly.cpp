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
 * Author:  Mike DiCuccio, Aleksey Grichenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <objmgr/scope.hpp>

#include <objects/genomecoll/GC_Assembly.hpp>
#include <objects/genomecoll/GC_AssemblySet.hpp>
#include <objects/genomecoll/GC_AssemblyUnit.hpp>
#include <objects/genomecoll/GC_Replicon.hpp>
#include <objects/genomecoll/GC_Sequence.hpp>
#include <objects/genomecoll/GC_TypedSeqId.hpp>
#include <objects/genomecoll/GC_SeqIdAlias.hpp>
#include <objects/genomecoll/GC_External_Seqid.hpp>
#include <objects/genomecoll/GC_TaggedSequences.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>

#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <objtools/readers/idmapper.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

 
CIdMapperGCAssembly::CIdMapperGCAssembly(CScope&             scope,
                                         const CGC_Assembly& assm,
                                         EAliasMapping   mapping,
                                         const string&   alias_scope)
    : m_Scope(&scope)
{
    AddAliasMappings(assm, mapping, alias_scope);
}


CIdMapperGCAssembly::CIdMapperGCAssembly(CScope& scope)
    : m_Scope(&scope)
{
}


void CIdMapperGCAssembly::AddAliasMappings(const CGC_Assembly& assm,
                                           EAliasMapping       mapping,
                                           const string&       alias_scope)
{
    x_AddAliasMappings(assm, mapping, alias_scope);
}


CSeq_id_Handle CIdMapperGCAssembly::Map(const CSeq_id_Handle& from)
{
    CSeq_id_Handle id = TParent::Map(from);
    if ( !id ) {
        NCBI_THROW(CIdMapperException, eBadSeqId, MapErrorString(from));
    }
    return id;
}


void CIdMapperGCAssembly::x_AddUnversionedMapping(const CSeq_id&        src_id,
                                                  const CSeq_id_Handle& dst_id)
{
    AddMapping(CSeq_id_Handle::GetHandle(src_id), dst_id);
    // Try to create accession-only id, add mapping to the same destination.
    CSeq_id acc_id;
    acc_id.Assign(src_id);
    CTextseq_id* txt_id = 0;
    switch ( src_id.Which() ) {
    case CSeq_id::e_Genbank:
        txt_id = &acc_id.SetGenbank();
        break;
    case CSeq_id::e_Embl:
        txt_id = &acc_id.SetEmbl();
        break;
    case CSeq_id::e_Pir:
        txt_id = &acc_id.SetPir();
        break;
    case CSeq_id::e_Swissprot:
        txt_id = &acc_id.SetSwissprot();
        break;
    case CSeq_id::e_Other:
        txt_id = &acc_id.SetOther();
        break;
    case CSeq_id::e_Ddbj:
        txt_id = &acc_id.SetDdbj();
        break;
    case CSeq_id::e_Prf:
        txt_id = &acc_id.SetPrf();
        break;
    case CSeq_id::e_Tpg:
        txt_id = &acc_id.SetTpg();
        break;
    case CSeq_id::e_Tpe:
        txt_id = &acc_id.SetTpe();
        break;
    case CSeq_id::e_Tpd:
        txt_id = &acc_id.SetTpd();
        break;
    case CSeq_id::e_Gpipe:
        txt_id = &acc_id.SetGpipe();
        break;
    case CSeq_id::e_Named_annot_track:
        txt_id = &acc_id.SetNamed_annot_track();
        break;
    default:
        return; // Non-text seq-id, ignore.
    }
    _ASSERT(txt_id);
    txt_id->ResetVersion();
    txt_id->ResetName();
    txt_id->ResetRelease();
    AddMapping(CSeq_id_Handle::GetHandle(acc_id), dst_id);
}


void CIdMapperGCAssembly::x_AddAccessionMapping(const CSeq_id& id)
{
    x_AddUnversionedMapping(id, CSeq_id_Handle::GetHandle(id));
}


void CIdMapperGCAssembly::x_AddAliasMappings(const CGC_Sequence& seq,
                                             EAliasMapping map_to,
                                             const string& alias_scope)
{
    if (map_to == eAccVer) {
        x_AddAccessionMapping(seq.GetSeq_id());
    }

    if (seq.IsSetSeq_id_synonyms()) {
        CSeq_id_Handle dst_alias;
        ITERATE (CGC_Sequence::TSeq_id_synonyms, it, seq.GetSeq_id_synonyms()) {
            switch ((*it)->Which()) {
            case CGC_TypedSeqId::e_Genbank:
                if (map_to == eAccVer) {
                    x_AddAccessionMapping((*it)->GetGenbank().GetPublic());
                    if ( (*it)->GetGenbank().IsSetGpipe() ) {
                        x_AddAccessionMapping((*it)->GetGenbank().GetGpipe());
                    }
                }
                else if (map_to == eGenBank  &&  (*it)->GetGenbank().IsSetGi()) {
                    dst_alias =
                        CSeq_id_Handle::GetHandle((*it)->GetGenbank().GetGi());
                }
                else if (map_to == eGenBankAcc) {
                    dst_alias =
                        CSeq_id_Handle::GetHandle((*it)->GetGenbank().GetPublic());
                }
                break;
            case CGC_TypedSeqId::e_Refseq:
                if (map_to == eAccVer) {
                    x_AddAccessionMapping((*it)->GetRefseq().GetPublic());
                    if ( (*it)->GetRefseq().IsSetGpipe() ) {
                        x_AddAccessionMapping((*it)->GetRefseq().GetGpipe());
                    }
                }
                else if (map_to == eRefSeq  &&  (*it)->GetRefseq().IsSetGi()) {
                    dst_alias =
                        CSeq_id_Handle::GetHandle((*it)->GetRefseq().GetGi());
                }
                else if (map_to == eRefSeqAcc) {
                    dst_alias =
                        CSeq_id_Handle::GetHandle((*it)->GetRefseq().GetPublic());
                }
                break;
            case CGC_TypedSeqId::e_External:
                if (map_to == eAccVer) {
                    x_AddAccessionMapping((*it)->GetExternal().GetId());
                }
                else if (map_to == eUCSC  &&
                    (*it)->GetExternal().GetExternal() == "UCSC") {
                    dst_alias =
                        CSeq_id_Handle::GetHandle((*it)->GetExternal().GetId());
                }
                break;
            case CGC_TypedSeqId::e_Private:
                // Ignore private accessions?
                if (map_to == eOther) {
                    dst_alias =
                        CSeq_id_Handle::GetHandle((*it)->GetPrivate());
                }
                break;

            default:
                break;
            }
        }

        if (dst_alias) {
            AddMapping(CSeq_id_Handle::GetHandle(seq.GetSeq_id()), dst_alias);

            ITERATE (CGC_Sequence::TSeq_id_synonyms, it,
                     seq.GetSeq_id_synonyms()) {
                switch ((*it)->Which()) {
                case CGC_TypedSeqId::e_Genbank:
                    {{
                        const CGC_SeqIdAlias& alias = (*it)->GetGenbank();
                        if (map_to != eGenBankAcc) {
                            x_AddUnversionedMapping(alias.GetPublic(), dst_alias);
                        }
                        if (alias.IsSetGpipe()) {
                            x_AddUnversionedMapping(alias.GetGpipe(), dst_alias);
                        }
                        if (map_to != eGenBank) {
                            AddMapping(CSeq_id_Handle::GetHandle(alias.GetGi()), dst_alias);
                        }
                    }}
                    break;

                case CGC_TypedSeqId::e_Refseq:
                    {{
                        const CGC_SeqIdAlias& alias = (*it)->GetRefseq();
                        x_AddUnversionedMapping(alias.GetPublic(), dst_alias);
                        if (alias.IsSetGpipe()) {
                            x_AddUnversionedMapping(alias.GetGpipe(), dst_alias);
                        }
                        AddMapping(CSeq_id_Handle::GetHandle(alias.GetGi()), dst_alias);
                    }}
                    break;

                case CGC_TypedSeqId::e_Private:
                    {{
                        AddMapping(CSeq_id_Handle::GetHandle((*it)->GetPrivate()), dst_alias);

                        /// HACK:
                        /// here go a bunch of scary modifications to the
                        /// input data

                        if ((*it)->GetPrivate().IsLocal()) {
                            string s =
                                (*it)->GetPrivate().GetLocal().GetStr();

                            CSeq_id id;
                            id.Set("lcl|chr" + s);
                            AddMapping(CSeq_id_Handle::GetHandle(id), dst_alias);
                        }

                        /// END HACK
                    }}
                    break;

                case CGC_TypedSeqId::e_External:
                    {{
                        const CSeq_id& id = (*it)->GetExternal().GetId();
                        AddMapping(CSeq_id_Handle::GetHandle(id), dst_alias);
                    }}
                    break;

                default:
                    _ASSERT(false);
                    CNcbiOstrstream str;
                    str << MSerial_AsnText << **it;
                    NCBI_THROW(CIdMapperException, eBadSeqId,
                        "Unhandled ID type in GC-Assembly: " +
                        NStr::PrintableString(
                        CTempString(str.str(), str.pcount()),
                        NStr::fNewLine_Quote));
                }
            }
        }
        else if (map_to != eAccVer) {
            ///
            /// check for UCSC-style random chromosomes
            ///
            const CSeq_id& id = seq.GetSeq_id();
            if (id.IsLocal()  &&  id.GetLocal().IsStr()  &&
                id.GetLocal().GetStr().find("_random") != string::npos  &&
                seq.IsSetStructure()) {
                CRef<CBioseq> bioseq(new CBioseq());

                /// local ID is our only ID
                /// HACK: make this 'chr' aware!
                /// note: this should be replaced with a complete list of
                /// possible synonyms for this sequence
                CRef<CSeq_id> lcl(new CSeq_id);
                lcl->SetLocal().SetStr(id.GetLocal().GetStr());

                CRef<CSeq_id> chr_lcl;
                if ( !NStr::StartsWith(id.GetLocal().GetStr(), "chr") ) {
                    chr_lcl.Reset(new CSeq_id);
                    chr_lcl->SetLocal().SetStr("chr" + id.GetLocal().GetStr());
                    bioseq->SetId().push_back(chr_lcl);
                }
                bioseq->SetId().push_back(lcl);
                /// END HACK

                /// inst should be the delta-seq from the structure
                bioseq->SetInst().SetRepr(CSeq_inst::eRepr_delta);
                bioseq->SetInst().SetMol(CSeq_inst::eMol_na);
                bioseq->SetInst().SetExt()
                    .SetDelta(const_cast<CDelta_ext&>(seq.GetStructure()));

                CBioseq_Handle bsh = m_Scope->AddBioseq(*bioseq);

                /// build the Seq-loc-Mapper for this
                /// depending on our direction, we may need to map differently
                /// NOTE: the random delta-seqs are provided as chr*_random ->
                /// scaffold; we need to map these the other way in some cases
                switch (map_to) {
                case eUCSC:
                    {{
                        CRef<CSeq_loc_Mapper> mapper
                            (new CSeq_loc_Mapper
                            (1, bsh, CSeq_loc_Mapper::eSeqMap_Up));
                        CTypeConstIterator<CSeq_id>
                            id_iter(seq.GetStructure());

                        for ( ;  id_iter;  ++id_iter) {
                            CSeq_id_Handle idh =
                                CSeq_id_Handle::GetHandle(*id_iter);
                            m_Cache[idh].dest_mapper = mapper;
                        }
                    }}
                    break;

                case eRefSeq:
                    {{
                        CRef<CSeq_loc_Mapper> mapper
                            (new CSeq_loc_Mapper
                            (1, bsh, CSeq_loc_Mapper::eSeqMap_Down));
                        m_Cache[CSeq_id_Handle::GetHandle(*lcl)].dest_mapper = mapper;
                        if (chr_lcl) {
                            m_Cache[CSeq_id_Handle::GetHandle(*chr_lcl)].dest_mapper = mapper;
                        }
                    }}
                    break;

                default:
                    break;
                }

                ///
                /// END HACK
            }
        }
    }

    if (seq.IsSetSequences()) {
        ITERATE (CGC_Sequence::TSequences, it, seq.GetSequences()) {
            ITERATE (CGC_TaggedSequences::TSeqs, i, (*it)->GetSeqs()) {
                x_AddAliasMappings(**i, map_to, alias_scope);
            }
        }
    }
}


void CIdMapperGCAssembly::x_AddAliasMappings(const CGC_AssemblyUnit& assm,
                                             EAliasMapping map_to,
                                             const string& alias_scope)
{
    if (assm.IsSetMols()) {
        ITERATE (CGC_AssemblyUnit::TMols, iter, assm.GetMols()) {
            const CGC_Replicon& mol = **iter;

            if (mol.GetSequence().IsSingle()) {
                x_AddAliasMappings(mol.GetSequence().GetSingle(),
                                   map_to, alias_scope);
            }
            else {
                ITERATE (CGC_Replicon::TSequence::TSet, it,
                    mol.GetSequence().GetSet()) {
                    x_AddAliasMappings(**it, map_to, alias_scope);
                }
            }
        }
    }

    if (assm.IsSetOther_sequences()) {
        ITERATE (CGC_AssemblyUnit::TOther_sequences, it,
            assm.GetOther_sequences()) {
            ITERATE (CGC_TaggedSequences::TSeqs, i, (*it)->GetSeqs()) {
                x_AddAliasMappings(**i, map_to, alias_scope);
            }
        }
    }
}


void CIdMapperGCAssembly::x_AddAliasMappings(const CGC_Assembly& assm,
                                             EAliasMapping map_to,
                                             const string& alias_scope)
{
    if (assm.IsUnit()) {
        x_AddAliasMappings(assm.GetUnit(), map_to, alias_scope);
    }
    else if (assm.IsAssembly_set()) {
        x_AddAliasMappings(assm.GetAssembly_set().GetPrimary_assembly(),
            map_to, alias_scope);
        if (assm.GetAssembly_set().IsSetMore_assemblies()) {
            ITERATE (CGC_Assembly::TAssembly_set::TMore_assemblies,
                iter, assm.GetAssembly_set().GetMore_assemblies()) {
                x_AddAliasMappings(**iter, map_to, alias_scope);
            }
        }
    }
}


END_NCBI_SCOPE
