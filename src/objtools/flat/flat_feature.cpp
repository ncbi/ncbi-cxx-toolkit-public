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
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_quals.hpp>
#include <objtools/flat/flat_gbseq_formatter.hpp>

#include <serial/iterator.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqfeat/seqfeat__.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CRef<CFlatFeature> IFlattishFeature::Format(void) const
{
    // extremely rough cut for now -- qualifiers still in progress!
    if (m_FF) {
        return m_FF;
    }

    m_FF.Reset(new CFlatFeature(GetKey(),
                                *new CFlatLoc(*m_Loc, *m_Context), *m_Feat));
    x_AddQuals();
    x_FormatQuals();
    return m_FF;
}


void CFlattishFeature::x_AddQuals(void) const
{
    CScope&             scope = m_Context->GetHandle().GetScope();
    const CSeqFeatData& data  = m_Feat->GetData();
    m_Type = data.GetSubtype();
    // add various generic qualifiers...
    if (m_Feat->IsSetComment()) {
        x_AddQual(eFQ_seqfeat_note, new CFlatStringQV(m_Feat->GetComment()));
    }
    if (m_Feat->IsSetProduct()) {
        if (m_IsProduct) {
            x_AddQual(eFQ_coded_by, new CFlatSeqLocQV(m_Feat->GetLocation()));
        } else {
            CBioseq_Handle prod = scope.GetBioseqHandle(m_Feat->GetProduct());
            if ( !m_Context->IsProt() ) {
                EFeatureQualifier slot
                    = ((prod.GetBioseqCore()->GetInst().GetMol()
                        == CSeq_inst::eMol_aa)
                       ? eFQ_translation : eFQ_transcription);
                x_AddQual(slot, new CFlatSeqDataQV(m_Feat->GetProduct()));
            }
            try {
                const CSeq_id& id = sequence::GetId(m_Feat->GetProduct(),
                                                    &scope);
                if (id.IsGi()) {
                    // cheat slightly
                    x_AddQual(eFQ_db_xref,
                              new CFlatStringQV
                              ("GI:" + NStr::IntToString(id.GetGi())));
                }
                if (data.IsCdregion()) {
                    x_AddQual(eFQ_protein_id, new CFlatSeqIdQV(id));
                }
            } catch (sequence::CNotUnique&) {
            }
        }
    }
    if ( !data.IsGene() ) {
        CConstRef<CSeq_feat> gene_feat
            = sequence::GetBestOverlappingFeat(m_Feat->GetLocation(),
                                               CSeqFeatData::e_Gene,
                                               sequence::eOverlap_Simple,
                                               scope);
        if (gene_feat) {
            const CGene_ref& gene = gene_feat->GetData().GetGene();
            string label;
            gene.GetLabel(&label);
            if ( !label.empty() ) {
                // XXX - should expand certain SGML entities
                x_AddQual(eFQ_gene, new CFlatStringQV(label));
            }
            if (gene.IsSetDb()  &&  !data.IsCdregion()  &&  !data.IsRna() ) {
                x_AddQual(eFQ_gene_xref, new CFlatXrefQV(gene.GetDb()));
            }
            if (gene.IsSetLocus_tag()  &&  gene.GetLocus_tag() != label) {
                x_AddQual(eFQ_locus_tag,
                          new CFlatStringQV(gene.GetLocus_tag()));
            }
        }
    }
    if (m_Feat->IsSetQual()) {
        x_ImportQuals(m_Feat->GetQual());
    }
    if (m_Feat->IsSetTitle()) {
        x_AddQual(eFQ_label, new CFlatLabelQV(m_Feat->GetTitle()));
    }
    if (m_Feat->IsSetCit()) {
        x_AddQual(eFQ_citation, new CFlatPubSetQV(m_Feat->GetCit()));
    }
    if (m_Feat->IsSetExp_ev()) {
        x_AddQual(eFQ_evidence, new CFlatExpEvQV(m_Feat->GetExp_ev()));
    }
    if (m_Feat->IsSetDbxref()) {
        x_AddQual(eFQ_db_xref, new CFlatXrefQV(m_Feat->GetDbxref()));
    }
    if (m_Feat->IsSetPseudo()) {
        x_AddQual(eFQ_pseudo, new CFlatBoolQV(m_Feat->GetPseudo()));
    }
    if (m_Feat->IsSetExcept_text()) {
        x_AddQual(eFQ_exception, new CFlatStringQV(m_Feat->GetExcept_text()));
    }
    switch (data.Which()) {
    case CSeqFeatData::e_Gene:      x_AddQuals(data.GetGene());      break;
    case CSeqFeatData::e_Cdregion:  x_AddQuals(data.GetCdregion());  break;
    case CSeqFeatData::e_Prot:      x_AddQuals(data.GetProt());      break;
        // ...
    default: break;
    }
}


void CFlattishFeature::x_AddQuals(const CGene_ref& gene) const
{
    bool got_name = false;
    if (gene.IsSetLocus()  &&  !gene.GetLocus().empty() ) {
        x_AddQual(eFQ_gene, new CFlatStringQV(gene.GetLocus()));
        got_name = true;
    }
    if (gene.IsSetDesc()  &&   !gene.GetDesc().empty() ) {
        x_AddQual(got_name ? eFQ_gene_desc : eFQ_gene,
                  new CFlatStringQV(gene.GetDesc()));
        got_name = true;
    }
    if (gene.IsSetSyn()  &&  !gene.GetSyn().empty() ) {
        ITERATE (CGene_ref::TSyn, it, gene.GetSyn()) {
            if ( !it->empty() ) {
                x_AddQual(got_name ? eFQ_gene_syn : eFQ_gene,
                          new CFlatStringQV(*it));
                got_name = true;
            }
        }
    }

    if (gene.IsSetAllele()  &&  !gene.GetAllele().empty() ) {
        x_AddQual(eFQ_gene_allele, new CFlatStringQV(gene.GetAllele()));
    }
    if (gene.IsSetMaploc()  &&  !gene.GetMaploc().empty() ) {
        x_AddQual(eFQ_gene_map, new CFlatStringQV(gene.GetMaploc()));
    }
    if (gene.IsSetDb()) {
        x_AddQual(eFQ_gene_xref, new CFlatXrefQV(gene.GetDb()));
    }
    if (gene.IsSetLocus_tag()) {
        x_AddQual(eFQ_locus_tag, new CFlatStringQV(gene.GetLocus_tag()));
    }
}


void CFlattishFeature::x_AddQuals(const CCdregion& cds) const
{
    if (m_IsProduct) {
        return; // We don't need directions when we have the sequence!
    } else if ( !m_Feat->IsSetProduct() ) {
        // warn?
        return;
    }
    CScope& scope = m_Context->GetHandle().GetScope();
    CConstRef<CSeq_feat> prod
        = sequence::GetBestOverlappingFeat(m_Feat->GetProduct(),
                                           CSeqFeatData::e_Prot,
                                           sequence::eOverlap_Contains,
                                           scope);
    if (prod) {
        string label;
        prod->GetData().GetProt().GetLabel(&label);
        x_AddQual(eFQ_cds_product, new CFlatStringQV(label));
    }
    if (cds.IsSetFrame()) {
        x_AddQual(eFQ_codon_start, new CFlatIntQV(cds.GetFrame()));
    }
    if (cds.IsSetCode()) {
        int id = cds.GetCode().GetId();
        if (id == 255) { // none found, so substitute default
            id = 1;
        }
        if (id != 1  ||
            dynamic_cast<CFlatGBSeqFormatter*>(&m_Context->GetFormatter())) {
            x_AddQual(eFQ_transl_table, new CFlatIntQV(id));
        }
        const string& ncbieaa = cds.GetCode().GetNcbieaa();
        if ( !ncbieaa.empty() ) {
            const string& std_ncbieaa = CGen_code_table::GetNcbieaa(id);
            if ( !std_ncbieaa.empty() ) {
                for (unsigned int i = 0;  i < ncbieaa.size();  ++i) {
                    if (ncbieaa[i] != std_ncbieaa[i]) {
                        x_AddQual(eFQ_codon, new CFlatCodonQV(i, ncbieaa[i]));
                    }
                }
            }
        }
    }
    if (cds.IsSetCode_break()) {
        x_AddQual(eFQ_transl_except,
                  new CFlatCodeBreakQV(cds.GetCode_break()));
    }
}


void CFlattishFeature::x_AddQuals(const CProt_ref& prot) const
{
    bool got_name = false;
    if (prot.IsSetName()  &&  !prot.GetName().empty() ) {
        ITERATE (CProt_ref::TName, it, prot.GetName()) {
            if ( !it->empty() ) {
                x_AddQual(got_name ? eFQ_prot_names : eFQ_product,
                          new CFlatStringQV(*it));
                got_name = true;
            }
        }
    }
#if 0
    if (prot.IsSetDesc()  &&   !prot.GetDesc().empty() ) {
        x_AddQual(got_name ? eFQ_prot_desc : eFQ_prot,
                  new CFlatStringQV(prot.GetDesc()));
        got_name = true;
    }
    if (prot.IsSetSyn()  &&  !prot.GetSyn().empty() ) {
        typedef CProt_ref::TSyn::const_iterator TProtSyn_CI;
        TProtSyn_CI first = prot.GetSyn().begin(), last = prot.GetSyn().end();
        if ( !got_name  &&  first != last) {
            x_AddQual(eFQ_prot, new CFlatStringQV(*first));
            got_name = true;
            ++first;
        }
        while (first != last) {
            x_AddQual(eFQ_prot_syn, new CFlatStringQV(*first));
            ++first;
        }
    }

    if (prot.IsSetAllele()  &&  !prot.GetAllele().empty() ) {
        x_AddQual(eFQ_prot_allele, new CFlatStringQV(prot.GetAllele()));
    }
    if (prot.IsSetMaploc()  &&  !prot.GetMaploc().empty() ) {
        x_AddQual(eFQ_prot_map, new CFlatStringQV(prot.GetMaploc()));
    }
    if (prot.IsSetDb()) {
        x_AddQual(eFQ_prot_xref, new CFlatXrefQV(prot.GetDb()));
    }
    if (prot.IsSetLocus_tag()) {
        x_AddQual(eFQ_locus_tag, new CFlatStringQV(prot.GetLocus_tag()));
    }
#endif
}


struct SLegalImport {
    const char*       m_Name;
    EFeatureQualifier m_Value;

    operator string(void) const { return m_Name; }
};


void CFlattishFeature::x_ImportQuals(const CSeq_feat::TQual& quals) const
{
    static const SLegalImport kLegalImports[] = {
        // Must be in case-insensitive alphabetical order!
#define DO_IMPORT(x) { #x, eFQ_##x }
        DO_IMPORT(allele),
        DO_IMPORT(bound_moiety),
        DO_IMPORT(clone),
        DO_IMPORT(codon),
        DO_IMPORT(cons_splice),
        DO_IMPORT(direction),
        DO_IMPORT(EC_number),
        DO_IMPORT(frequency),
        DO_IMPORT(function),
        DO_IMPORT(insertion_seq),
        DO_IMPORT(label),
        DO_IMPORT(map),
        DO_IMPORT(mod_base),
        DO_IMPORT(number),
        DO_IMPORT(organism),
        DO_IMPORT(PCR_conditions),
        DO_IMPORT(phenotype),
        { "product", eFQ_product_quals },
        DO_IMPORT(replace),
        DO_IMPORT(rpt_family),
        DO_IMPORT(rpt_type),
        DO_IMPORT(rpt_unit),
        DO_IMPORT(standard_name),
        DO_IMPORT(transposon),
        DO_IMPORT(usedin)
#undef DO_IMPORT
    };
    static const SLegalImport* kLegalImportsEnd
        = kLegalImports + sizeof(kLegalImports)/sizeof(SLegalImport);

    ITERATE (CSeq_feat::TQual, it, quals) {
        const string&       name = (*it)->GetQual();
        const SLegalImport* li   = lower_bound(kLegalImports, kLegalImportsEnd,
                                               name, PNocase());
        EFeatureQualifier   slot = eFQ_illegal_qual;
        if (li != kLegalImportsEnd && !NStr::CompareNocase(li->m_Name,name)) {
            slot = li->m_Value;
        }
        switch (slot) {
        case eFQ_codon:
        case eFQ_cons_splice:
        case eFQ_direction:
        case eFQ_mod_base:
        case eFQ_number:
        case eFQ_rpt_type:
        case eFQ_rpt_unit:
        case eFQ_usedin:
            // XXX -- each of these should really get its own class
            // (to verify correct syntax)
            x_AddQual(slot, new CFlatStringQV((*it)->GetVal(),
                                              CFlatQual::eUnquoted));
            break;
        case eFQ_label:
            x_AddQual(slot, new CFlatLabelQV((*it)->GetVal()));
            break;
        case eFQ_illegal_qual:
            x_AddQual(slot, new CFlatIllegalQV(**it));
            break;
        default:
            // XXX - should split off EC_number and replace
            // (to verify correct syntax)
            x_AddQual(slot, new CFlatStringQV((*it)->GetVal()));
            break;
        }
    }
}


void CFlattishFeature::x_FormatQuals(void) const
{
    m_FF->SetQuals().reserve(m_Quals.size());

#define DO_QUAL(x) x_FormatQual(eFQ_##x, #x)
    DO_QUAL(partial);
    DO_QUAL(gene);

    DO_QUAL(locus_tag);

    DO_QUAL(product);

    x_FormatQual(eFQ_prot_EC_number, "EC_number");
    x_FormatQual(eFQ_prot_activity,  "function");

    DO_QUAL(standard_name);
    DO_QUAL(coded_by);
    DO_QUAL(derived_from);

    x_FormatQual(eFQ_prot_name, "name");
    DO_QUAL(region_name);
    DO_QUAL(bond_type);
    DO_QUAL(site_type);
    DO_QUAL(sec_str_type);
    DO_QUAL(heterogen);

#define DO_NOTE(x) x_FormatNoteQual(eFQ_##x, #x)
    DO_NOTE(gene_desc);
    DO_NOTE(gene_syn);
    DO_NOTE(trna_codons);
    DO_NOTE(prot_desc);
    DO_NOTE(prot_note);
    DO_NOTE(prot_comment);
    DO_NOTE(prot_method);
    DO_NOTE(figure);
    DO_NOTE(maploc);
    DO_NOTE(prot_conflict);
    DO_NOTE(prot_missing);
    DO_NOTE(seqfeat_note);
    DO_NOTE(exception_note);
    DO_NOTE(region);
    // DO_NOTE(selenocysteine);
    DO_NOTE(prot_names);
    DO_NOTE(bond);
    DO_NOTE(site);
    DO_NOTE(rrna_its);
    x_FormatNoteQual(eFQ_xtra_prod_quals, "xtra_products");
    x_FormatNoteQual(eFQ_modelev,         "model_evidence");

    DO_NOTE(go_component);
    DO_NOTE(go_function);
    DO_NOTE(go_process);
#undef DO_NOTE
        
    DO_QUAL(citation);

    DO_QUAL(number);

    DO_QUAL(pseudo);

    DO_QUAL(codon_start);

    DO_QUAL(anticodon);
    DO_QUAL(bound_moiety);
    DO_QUAL(clone);
    DO_QUAL(cons_splice);
    DO_QUAL(direction);
    DO_QUAL(function);
    DO_QUAL(evidence);
    DO_QUAL(exception);
    DO_QUAL(frequency);
    DO_QUAL(EC_number);
    x_FormatQual(eFQ_gene_map,    "map");
    x_FormatQual(eFQ_gene_allele, "allele");
    DO_QUAL(allele);
    DO_QUAL(map);
    DO_QUAL(mod_base);
    DO_QUAL(PCR_conditions);
    DO_QUAL(phenotype);
    DO_QUAL(rpt_family);
    DO_QUAL(rpt_type);
    DO_QUAL(rpt_unit);
    DO_QUAL(insertion_seq);
    DO_QUAL(transposon);
    DO_QUAL(usedin);

    // extra imports, actually...
    x_FormatQual(eFQ_illegal_qual, "illegal");

    DO_QUAL(replace);

    DO_QUAL(transl_except);
    DO_QUAL(transl_table);
    DO_QUAL(codon);
    DO_QUAL(organism);
    DO_QUAL(label);
    x_FormatQual(eFQ_cds_product, "product");
    DO_QUAL(protein_id);
    DO_QUAL(transcript_id);
    DO_QUAL(db_xref);
    x_FormatQual(eFQ_gene_xref, "db_xref");
    DO_QUAL(translation);
    DO_QUAL(transcription);
    DO_QUAL(peptide);
#undef DO_QUAL
}


void CFlattishSourceFeature::x_AddQuals(void) const
{
    const CSeqFeatData& data = m_Feat->GetData();
    _ASSERT(data.IsOrg()  ||  data.IsBiosrc());
    // add various generic qualifiers...
    x_AddQual(eSQ_mol_type,
              new CFlatMolTypeQV(m_Context->GetBiomol(), m_Context->GetMol()));
    if (m_Feat->IsSetComment()) {
        x_AddQual(eSQ_seqfeat_note, new CFlatStringQV(m_Feat->GetComment()));
    }
    if (m_Feat->IsSetTitle()) {
        x_AddQual(eSQ_label, new CFlatLabelQV(m_Feat->GetTitle()));
    }
    if (m_Feat->IsSetCit()) {
        x_AddQual(eSQ_citation, new CFlatPubSetQV(m_Feat->GetCit()));
    }
    if (m_Feat->IsSetDbxref()) {
        x_AddQual(eSQ_org_xref, new CFlatXrefQV(m_Feat->GetDbxref()));
    }
    switch (data.Which()) {
    case CSeqFeatData::e_Org:     x_AddQuals(data.GetOrg());     break;
    case CSeqFeatData::e_Biosrc:  x_AddQuals(data.GetBiosrc());  break;
    default: break; // can't happen, but some compilers warn anyway
    }
}


static ESourceQualifier s_OrgModToSlot(const COrgMod& om)
{
    switch (om.GetSubtype()) {
#define DO_ORGMOD(x) case COrgMod::eSubtype_##x:  return eSQ_##x;
        DO_ORGMOD(strain);
        DO_ORGMOD(substrain);
        DO_ORGMOD(type);
        DO_ORGMOD(subtype);
        DO_ORGMOD(variety);
        DO_ORGMOD(serotype);
        DO_ORGMOD(serogroup);
        DO_ORGMOD(serovar);
        DO_ORGMOD(cultivar);
        DO_ORGMOD(pathovar);
        DO_ORGMOD(chemovar);
        DO_ORGMOD(biovar);
        DO_ORGMOD(biotype);
        DO_ORGMOD(group);
        DO_ORGMOD(subgroup);
        DO_ORGMOD(isolate);
        DO_ORGMOD(common);
        DO_ORGMOD(acronym);
        DO_ORGMOD(dosage);
    case COrgMod::eSubtype_nat_host:  return eSQ_spec_or_nat_host;
        DO_ORGMOD(sub_species);
        DO_ORGMOD(specimen_voucher);
        DO_ORGMOD(authority);
        DO_ORGMOD(forma);
        DO_ORGMOD(forma_specialis);
        DO_ORGMOD(ecotype);
        DO_ORGMOD(synonym);
        DO_ORGMOD(anamorph);
        DO_ORGMOD(teleomorph);
        DO_ORGMOD(breed);
        DO_ORGMOD(gb_acronym);
        DO_ORGMOD(gb_anamorph);
        DO_ORGMOD(gb_synonym);
        DO_ORGMOD(old_lineage);
        DO_ORGMOD(old_name);
#undef DO_ORGMOD
    case COrgMod::eSubtype_other:  return eSQ_orgmod_note;
    default:                       return eSQ_none;
    }
}


void CFlattishSourceFeature::x_AddQuals(const COrg_ref& org) const
{
    {{
        string name;
        if (org.IsSetTaxname()) {
            name = org.GetTaxname();
        }
        if (name.empty()  &&  org.IsSetOrgname()) {
            org.GetOrgname().GetFlatName(name);
        }
        if (org.IsSetCommon()) {
            if (name.empty()) {
                name = org.GetCommon();
            }
            x_AddQual(eSQ_common_name, new CFlatStringQV(org.GetCommon()));
        }
        if ( !name.empty() ) {
            x_AddQual(eSQ_organism, new CFlatStringQV(name));
        }
    }}
    if (org.IsSetDb()) {
        x_AddQual(eSQ_db_xref, new CFlatXrefQV(org.GetDb()));
    }
    for (CTypeConstIterator<COrgMod> it(org);  it;  ++it) {
        ESourceQualifier slot = s_OrgModToSlot(*it);
        if (slot != eSQ_none) {
            x_AddQual(slot, new CFlatOrgModQV(*it));
        }
    }
}


static ESourceQualifier s_SubSourceToSlot(const CSubSource& ss)
{
    switch (ss.GetSubtype()) {
#define DO_SS(x) case CSubSource::eSubtype_##x:  return eSQ_##x;
        DO_SS(chromosome);
        DO_SS(map);
        DO_SS(clone);
        DO_SS(subclone);
        DO_SS(haplotype);
        DO_SS(genotype);
        DO_SS(sex);
        DO_SS(cell_line);
        DO_SS(cell_type);
        DO_SS(tissue_type);
        DO_SS(clone_lib);
        DO_SS(dev_stage);
        DO_SS(frequency);
        DO_SS(germline);
        DO_SS(rearranged);
        DO_SS(lab_host);
        DO_SS(pop_variant);
        DO_SS(tissue_lib);
        DO_SS(plasmid_name);
        DO_SS(transposon_name);
        DO_SS(insertion_seq_name);
        DO_SS(plastid_name);
        DO_SS(country);
        DO_SS(segment);
        DO_SS(endogenous_virus_name);
        DO_SS(transgenic);
        DO_SS(environmental_sample);
        DO_SS(isolation_source);
#undef DO_SS
    case CSubSource::eSubtype_other:  return eSQ_subsource_note;
    default:                          return eSQ_none;
    }
}


void CFlattishSourceFeature::x_AddQuals(const CBioSource& src) const
{
    x_AddQual(eSQ_organelle, new CFlatOrganelleQV(src.GetGenome()));
    x_AddQuals(src.GetOrg());
    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        ESourceQualifier slot = s_SubSourceToSlot(**it);
        if (slot != eSQ_none) {
            x_AddQual(slot, new CFlatSubSourceQV(**it));
        }
    }
    x_AddQual(eSQ_focus, new CFlatBoolQV(src.IsSetIs_focus()));
}


void CFlattishSourceFeature::x_FormatQuals(void) const
{
    m_FF->SetQuals().reserve(m_Quals.size());

#define DO_QUAL(x) x_FormatQual(eSQ_##x, #x)
    DO_QUAL(organism);

    DO_QUAL(organelle);

    DO_QUAL(mol_type);

    DO_QUAL(strain);
    x_FormatQual(eSQ_substrain, "sub_strain");
    DO_QUAL(variety);
    DO_QUAL(serotype);
    DO_QUAL(serovar);
    DO_QUAL(cultivar);
    DO_QUAL(isolate);
    DO_QUAL(isolation_source);
    x_FormatQual(eSQ_spec_or_nat_host, "specific_host");
    DO_QUAL(sub_species);
    DO_QUAL(specimen_voucher);

    DO_QUAL(db_xref);
    x_FormatQual(eSQ_org_xref, "db_xref");

    DO_QUAL(chromosome);

    DO_QUAL(segment);

    DO_QUAL(map);
    DO_QUAL(clone);
    x_FormatQual(eSQ_subclone, "sub_clone");
    DO_QUAL(haplotype);
    DO_QUAL(sex);
    DO_QUAL(cell_line);
    DO_QUAL(cell_type);
    DO_QUAL(tissue_type);
    DO_QUAL(clone_lib);
    DO_QUAL(dev_stage);
    DO_QUAL(frequency);

    DO_QUAL(germline);
    DO_QUAL(rearranged);
    DO_QUAL(transgenic);
    DO_QUAL(environmental_sample);

    DO_QUAL(lab_host);
    DO_QUAL(pop_variant);
    DO_QUAL(tissue_lib);

    x_FormatQual(eSQ_plasmid_name,       "plasmid");
    x_FormatQual(eSQ_transposon_name,    "transposon");
    x_FormatQual(eSQ_insertion_seq_name, "insertion_seq");

    DO_QUAL(country);

    DO_QUAL(focus);

#define DO_NOTE(x) x_FormatNoteQual(eSQ_##x, #x)
    if (m_WasDesc) {
        DO_NOTE(seqfeat_note);
        DO_NOTE(orgmod_note);
        DO_NOTE(subsource_note);
    } else {
        DO_NOTE(unstructured);
    }

    DO_NOTE(type);
    DO_NOTE(subtype);
    DO_NOTE(serogroup);
    DO_NOTE(pathovar);
    DO_NOTE(chemovar);
    DO_NOTE(biovar);
    DO_NOTE(biotype);
    DO_NOTE(group);
    DO_NOTE(subgroup);
    DO_NOTE(common);
    DO_NOTE(acronym);
    DO_NOTE(dosage);

    DO_NOTE(authority);
    DO_NOTE(forma);
    DO_NOTE(forma_specialis);
    DO_NOTE(ecotype);
    DO_NOTE(synonym);
    DO_NOTE(anamorph);
    DO_NOTE(teleomorph);
    DO_NOTE(breed);

    DO_NOTE(genotype);
    x_FormatNoteQual(eSQ_plastid_name, "plastid");

    x_FormatNoteQual(eSQ_endogenous_virus_name, "endogenous_virus");

    if ( !m_WasDesc ) {
        DO_NOTE(seqfeat_note);
        DO_NOTE(orgmod_note);
        DO_NOTE(subsource_note);
    }

    x_FormatNoteQual(eSQ_common_name, "common");

    x_FormatNoteQual(eSQ_zero_orgmod, "?");
    x_FormatNoteQual(eSQ_one_orgmod,  "?");
    x_FormatNoteQual(eSQ_zero_subsrc, "?");
#undef DO_NOTE

    DO_QUAL(sequenced_mol);
    DO_QUAL(label);
    DO_QUAL(usedin);
    DO_QUAL(citation);
#undef DO_QUAL
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.11  2004/05/21 21:42:53  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.10  2003/10/17 20:58:41  ucko
* Don't assume coding-region features have their "product" fields set.
*
* Revision 1.9  2003/10/16 20:21:53  ucko
* Fix a copy-and-paste error in CFlattishFeature::x_AddQuals
*
* Revision 1.8  2003/10/08 21:11:12  ucko
* Add a couple of accessors to IFlattishFeature for the GFF/GTF formatter.
*
* Revision 1.7  2003/07/22 18:04:13  dicuccio
* Fixed access of unset optional variables
*
* Revision 1.6  2003/06/02 16:06:42  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.5  2003/04/24 16:15:58  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.4  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.3  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.2  2003/03/10 22:01:36  ucko
* Change SLegalImport::m_Name from string to const char* (needed by MSVC).
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
