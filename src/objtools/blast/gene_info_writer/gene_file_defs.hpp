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
 * Author:  Vahram Avagyan
 *
 */

/// @file gene_file_defs.hpp
/// Defines constants for reading and processing the Gene files.
///
/// The format of Gene files, as well as the actual files, can be found
/// here: ftp://ftp.ncbi.nlm.nih.gov/gene/DATA. The README text document
/// defines the format of gene_info, gene2accession and gene2pubmed
/// files, among others. This header file translates those definitions
/// into integer constants used in parsing the Gene files. This header
/// file should be updated every time there's a major change of format
/// of the Gene files as documented at the above URL.

#ifndef OBJTOOLS_BLAST_GENE_INFO_WRITER___GENE_FILE_DEFS__HPP
#define OBJTOOLS_BLAST_GENE_INFO_WRITER___GENE_FILE_DEFS__HPP

/// Minimum valid length of a Gene->Accession line.
#define GENE_2_ACCN_LINE_MIN                30
/// Number of items on a valid Gene->Accession line.
#define GENE_2_ACCN_NUM_ITEMS               13
/// Index of the taxonomy ID item on a Gene->Accession line.
#define GENE_2_ACCN_TAX_ID_INDEX            0
/// Index of the Gene ID item on a Gene->Accession line.
#define GENE_2_ACCN_GENE_ID_INDEX           1
/// Index of the RNA Gi item on a Gene->Accession line.
#define GENE_2_ACCN_RNA_GI_INDEX            4
/// Index of the Protein Gi item on a Gene->Accession line.
#define GENE_2_ACCN_PROT_GI_INDEX           6
/// Index of the Genomic Gi item on a Gene->Accession line.
#define GENE_2_ACCN_GENOMIC_GI_INDEX        8
/// Upper bound on the Gi numeric value.
#define GENE_2_ACCN_MAX_GI_VALUE            200000000

/// Minimum valid length of a Gene Info line.
#define GENE_INFO_LINE_MIN                  50
/// Number of items on a valid Gene Info line.
#define GENE_INFO_NUM_ITEMS                 15
/// Index of the taxonomy ID item on a Gene Info line.
#define GENE_INFO_TAX_ID_INDEX              0
/// Index of the Gene ID item on a Gene Info line.
#define GENE_INFO_GENE_ID_INDEX             1
/// Index of the Gene Symbol item on a Gene Info line.
#define GENE_INFO_SYMBOL_INDEX              2
/// Index of the Gene Description item on a Gene Info line.
#define GENE_INFO_DESCRIPTION_INDEX         8

/// Minimum valid length of a Gene->PubMed line.
#define GENE_2_PM_LINE_MIN                  5
/// Number of items on a valid Gene->PubMed line.
#define GENE_2_PM_NUM_ITEMS                 3
/// Index of the Gene ID item on a Gene->PubMed line.
#define GENE_2_PM_GENE_ID_INDEX             1
/// Index of the PubMed ID item on a Gene->PubMed line.
#define GENE_2_PM_PMID_INDEX                2

#endif

