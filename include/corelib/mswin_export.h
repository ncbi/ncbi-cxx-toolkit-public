#ifndef CORELIB___MSWIN_EXPORT__H
#define CORELIB___MSWIN_EXPORT__H

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
 * Author:  Mike DiCuccio
 *
 */

/// @file mswin_export.h
///   Defines to provide correct exporting from DLLs in Windows.
///   These are necessary to compile DLLs with Visual C++ - exports must be
///   explicitly labeled as such.


/** @addtogroup WinDLL
 *
 * @{
 */


#if defined(NCBI_OS_MSWIN)  &&  defined(NCBI_DLL_BUILD)

#ifndef _MSC_VER
#  error "This toolkit is not buildable with a compiler other than MSVC."
#endif


/*
 * Dumping ground for Windows-specific stuff
 */
#pragma warning (disable : 4786 4251 4275)


/*
 * -------------------------------------------------
 * DLL clusters
 */


/*
 * Definitions for NCBI_CORE.DLL
 */
#ifdef NCBI_CORE_EXPORTS
#  define NCBI_XNCBI_EXPORTS
#  define NCBI_XSERIAL_EXPORTS
#  define NCBI_XUTIL_EXPORTS
#  define NCBI_XREGEXP_EXPORTS
#endif


/*
 * Definitions for NCBI_PUB.DLL
 */
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_BIBLIO_EXPORTS
#  define NCBI_MEDLINE_EXPORTS
#  define NCBI_MEDLARS_EXPORTS
#  define NCBI_MLA_EXPORTS
#  define NCBI_PUBMED_EXPORTS
#endif

/*
 * Definitions for NCBI_SEQ.DLL
 */
#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_BLAST_EXPORTS
#  define NCBI_BLASTDB_EXPORTS
#  define NCBI_SCOREMAT_EXPORTS
#  define NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQLOC_EXPORTS
#  define NCBI_SEQRES_EXPORTS
#  define NCBI_SEQSET_EXPORTS
#  define NCBI_SEQTEST_EXPORTS
#  define NCBI_SUBMIT_EXPORTS
#  define NCBI_TAXON1_EXPORTS
#endif


/*
 * Definitions for NCBI_SEQEXT.DLL
 */
#ifdef NCBI_SEQEXT_EXPORTS
#  define NCBI_ID1_EXPORTS
#  define NCBI_ID2_EXPORTS
#  define NCBI_ID2_SPLIT_EXPORTS
#  define NCBI_FLAT_EXPORTS
#  define NCBI_XALNMGR_EXPORTS
#  define NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJREAD_EXPORTS
#  define NCBI_XOBJWRITE_EXPORTS
#  define NCBI_XOBJUTIL_EXPORTS
#  define NCBI_XOBJMANIP_EXPORTS
#  define NCBI_FORMAT_EXPORTS
#  define NCBI_XOBJEDIT_EXPORTS
#endif


/*
 * Definitions for NCBI_MISC.DLL
 */
#ifdef NCBI_MISC_EXPORTS
#  define NCBI_ACCESS_EXPORTS
#  define NCBI_DOCSUM_EXPORTS
#  define NCBI_ENTREZ2_EXPORTS
#  define NCBI_FEATDEF_EXPORTS
#  define NCBI_GBSEQ_EXPORTS
#  define NCBI_INSDSEQ_EXPORTS
#  define NCBI_MIM_EXPORTS
#  define NCBI_OBJPRT_EXPORTS
#  define NCBI_TINYSEQ_EXPORTS
#  define NCBI_ENTREZGENE_EXPORTS
#  define NCBI_BIOTREE_EXPORTS
#  define NCBI_REMAP_EXPORTS
#endif


/*
 * Definitions for NCBI_MMDB.DLL
 */
#ifdef NCBI_MMDB_EXPORTS
#  define NCBI_CDD_EXPORTS
#  define NCBI_CN3D_EXPORTS
#  define NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB3_EXPORTS
#  define NCBI_NCBIMIME_EXPORTS
#endif

/*
 * Definitions for NCBI_ALGO.DLL
 */
#ifdef NCBI_XALGO_EXPORTS
#  define NCBI_XALGOALIGN_EXPORTS
#  define NCBI_XALGOSEQ_EXPORTS
#  define NCBI_XALGOGNOMON_EXPORTS
#  define NCBI_XALGOPHYTREE_EXPORTS
#  define NCBI_XALGOSEQQA_EXPORTS
#  define NCBI_XALGOWINMASK_EXPORTS
#endif

/*
 * Definitions for NCBI_WEB.DLL
 */
#ifdef NCBI_WEB_EXPORTS
#  define NCBI_XHTML_EXPORTS
#  define NCBI_XCGI_EXPORTS
#  define NCBI_XCGI_REDIRECT_EXPORTS
#endif

/*
 * Definitions for NCBI_ALGO_MS.DLL
 */
#ifdef NCBI_ALGOMS_EXPORTS
#  define NCBI_OMSSA_EXPORTS
#  define NCBI_XOMSSA_EXPORTS
#endif

/* ------------------------------------------------- */
/*
 * Individual Library Definitions
 * Please keep alphabetized!
 */

/*
 * Export specifier for library access
 */
#ifdef NCBI_ACCESS_EXPORTS
#  define NCBI_ACCESS_EXPORT __declspec(dllexport)
#else
#  define NCBI_ACCESS_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library bdb
 */
#ifdef NCBI_BDB_EXPORTS
#  define NCBI_BDB_EXPORT __declspec(dllexport)
#else
#  define NCBI_BDB_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library bdb
 */
#ifdef NCBI_BDB_CACHE_EXPORTS
#  define NCBI_BDB_CACHE_EXPORT __declspec(dllexport)
#else
#  define NCBI_BDB_CACHE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library biblo
 */
#ifdef NCBI_BIBLIO_EXPORTS
#  define NCBI_BIBLIO_EXPORT __declspec(dllexport)
#else
#  define NCBI_BIBLIO_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library biotree
 */
#ifdef NCBI_BIOTREE_EXPORTS
#  define NCBI_BIOTREE_EXPORT __declspec(dllexport)
#else
#  define NCBI_BIOTREE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library blastdb
 */
#ifdef NCBI_BLASTDB_EXPORTS
#  define NCBI_BLASTDB_EXPORT __declspec(dllexport)
#else
#  define NCBI_BLASTDB_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library blast
 */
#ifdef NCBI_BLAST_EXPORTS
#  define NCBI_BLAST_EXPORT __declspec(dllexport)
#else
#  define NCBI_BLAST_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library cdd
 */
#ifdef NCBI_CDD_EXPORTS
#  define NCBI_CDD_EXPORT __declspec(dllexport)
#else
#  define NCBI_CDD_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library cn3d
 */
#ifdef NCBI_CN3D_EXPORTS
#  define NCBI_CN3D_EXPORT __declspec(dllexport)
#else
#  define NCBI_CN3D_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver
 */
#ifdef NCBI_DBAPIDRIVER_EXPORTS
#  define NCBI_DBAPIDRIVER_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_ctlib
 */
#ifdef NCBI_DBAPIDRIVER_CTLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_dblib
 */
#ifdef NCBI_DBAPIDRIVER_DBLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_msdblib
 */
#ifdef NCBI_DBAPIDRIVER_MSDBLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_mysql
 */
#ifdef NCBI_DBAPIDRIVER_MYSQL_EXPORTS
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_odbc
 */
#ifdef NCBI_DBAPIDRIVER_ODBC_EXPORTS
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_ftds
 */
#ifdef NCBI_DBAPIDRIVER_FTDS_EXPORTS
#  define NCBI_DBAPIDRIVER_FTDS_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_FTDS_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi
 */
#ifdef NCBI_DBAPI_EXPORTS
#  define NCBI_DBAPI_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPI_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi
 */
#ifdef NCBI_DBAPI_CACHE_EXPORTS
#  define NCBI_DBAPI_CACHE_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPI_CACHE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_util_blobstore
 */
#ifdef NCBI_DBAPIUTIL_BLOBSTORE_EXPORTS
#  define NCBI_DBAPIUTIL_BLOBSTORE_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIUTIL_BLOBSTORE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library docsum
 */
#ifdef NCBI_DOCSUM_EXPORTS
#  define NCBI_DOCSUM_EXPORT __declspec(dllexport)
#else
#  define NCBI_DOCSUM_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library entrez2
 */
#ifdef NCBI_ENTREZ2_EXPORTS
#  define NCBI_ENTREZ2_EXPORT __declspec(dllexport)
#else
#  define NCBI_ENTREZ2_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library entrezgene
 */
#ifdef NCBI_ENTREZGENE_EXPORTS
#  define NCBI_ENTREZGENE_EXPORT __declspec(dllexport)
#else
#  define NCBI_ENTREZGENE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library featdef
 */
#ifdef NCBI_FEATDEF_EXPORTS
#  define NCBI_FEATDEF_EXPORT __declspec(dllexport)
#else
#  define NCBI_FEATDEF_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library flat
 */
#ifdef NCBI_FLAT_EXPORTS
#  define NCBI_FLAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_FLAT_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library format
 */
#ifdef NCBI_FORMAT_EXPORTS
#  define NCBI_FORMAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_FORMAT_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library gbseq
 */
#ifdef NCBI_GBSEQ_EXPORTS
#  define NCBI_GBSEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_GBSEQ_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library general
 */
#ifdef NCBI_GENERAL_EXPORTS
#  define NCBI_GENERAL_EXPORT __declspec(dllexport)
#else
#  define NCBI_GENERAL_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library id1
 */
#ifdef NCBI_ID1_EXPORTS
#  define NCBI_ID1_EXPORT __declspec(dllexport)
#else
#  define NCBI_ID1_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library id2
 */
#ifdef NCBI_ID2_EXPORTS
#  define NCBI_ID2_EXPORT __declspec(dllexport)
#else
#  define NCBI_ID2_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library id2_split
 */
#ifdef NCBI_ID2_SPLIT_EXPORTS
#  define NCBI_ID2_SPLIT_EXPORT           __declspec(dllexport)
#else
#  define NCBI_ID2_SPLIT_EXPORT           __declspec(dllimport)
#endif

/*
 * Export specifier for library insdseq
 */
#ifdef NCBI_INSDSEQ_EXPORTS
#  define NCBI_INSDSEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_INSDSEQ_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library lds
 */
#ifdef NCBI_LDS_EXPORTS
#  define NCBI_LDS_EXPORT __declspec(dllexport)
#else
#  define NCBI_LDS_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library medlars
 */
#ifdef NCBI_MEDLARS_EXPORTS
#  define NCBI_MEDLARS_EXPORT __declspec(dllexport)
#else
#  define NCBI_MEDLARS_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library medline
 */
#ifdef NCBI_MEDLINE_EXPORTS
#  define NCBI_MEDLINE_EXPORT __declspec(dllexport)
#else
#  define NCBI_MEDLINE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library mim
 */
#ifdef NCBI_MIM_EXPORTS
#  define NCBI_MIM_EXPORT __declspec(dllexport)
#else
#  define NCBI_MIM_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library mla
 */
#ifdef NCBI_MLA_EXPORTS
#  define NCBI_MLA_EXPORT __declspec(dllexport)
#else
#  define NCBI_MLA_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library mmdb1
 */
#ifdef NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB1_EXPORT __declspec(dllexport)
#else
#  define NCBI_MMDB1_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library mmdb2
 */
#ifdef NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB2_EXPORT __declspec(dllexport)
#else
#  define NCBI_MMDB2_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library mmdb3
 */
#ifdef NCBI_MMDB3_EXPORTS
#  define NCBI_MMDB3_EXPORT __declspec(dllexport)
#else
#  define NCBI_MMDB3_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library ncbi_mime
 */
#ifdef NCBI_NCBIMIME_EXPORTS
#  define NCBI_NCBIMIME_EXPORT __declspec(dllexport)
#else
#  define NCBI_NCBIMIME_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library objprt
 */
#ifdef NCBI_OBJPRT_EXPORTS
#  define NCBI_OBJPRT_EXPORT __declspec(dllexport)
#else
#  define NCBI_OBJPRT_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library omssa
 */
#ifdef NCBI_OMSSA_EXPORTS
#  define NCBI_OMSSA_EXPORT __declspec(dllexport)
#else
#  define NCBI_OMSSA_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library pubmed
 */
#ifdef NCBI_PUBMED_EXPORTS
#  define NCBI_PUBMED_EXPORT __declspec(dllexport)
#else
#  define NCBI_PUBMED_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library pub
 */
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_PUB_EXPORT __declspec(dllexport)
#else
#  define NCBI_PUB_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library remap
 */
#ifdef NCBI_REMAP_EXPORTS
#  define NCBI_REMAP_EXPORT __declspec(dllexport)
#else
#  define NCBI_REMAP_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library scoremat
 */
#ifdef NCBI_SCOREMAT_EXPORTS
#  define NCBI_SCOREMAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_SCOREMAT_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seqalign
 */
#ifdef NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQALIGN_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQALIGN_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seqblock
 */
#ifdef NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQBLOCK_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQBLOCK_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seqcode
 */
#ifdef NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQCODE_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQCODE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seqfeat
 */
#ifdef NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQFEAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQFEAT_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seqloc
 */
#ifdef NCBI_SEQLOC_EXPORTS
#  define NCBI_SEQLOC_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQLOC_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seqres
 */
#ifdef NCBI_SEQRES_EXPORTS
#  define NCBI_SEQRES_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQRES_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seqset
 */
#ifdef NCBI_SEQSET_EXPORTS
#  define NCBI_SEQSET_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQSET_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seqtest
 */
#ifdef NCBI_SEQTEST_EXPORTS
#  define NCBI_SEQTEST_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQTEST_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library seq
 */
#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_SEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQ_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library submit
 */
#ifdef NCBI_SUBMIT_EXPORTS
#  define NCBI_SUBMIT_EXPORT __declspec(dllexport)
#else
#  define NCBI_SUBMIT_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library taxon1
 */
#ifdef NCBI_TAXON1_EXPORTS
#  define NCBI_TAXON1_EXPORT __declspec(dllexport)
#else
#  define NCBI_TAXON1_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library tinyseq
 */
#ifdef NCBI_TINYSEQ_EXPORTS
#  define NCBI_TINYSEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_TINYSEQ_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library validator
 */
#ifdef NCBI_VALIDATOR_EXPORTS
#  define NCBI_VALIDATOR_EXPORT __declspec(dllexport)
#else
#  define NCBI_VALIDATOR_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xalgoalign
 */
#ifdef NCBI_XALGOALIGN_EXPORTS
#  define NCBI_XALGOALIGN_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOALIGN_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xalgognomon
 */
#ifdef NCBI_XALGOGNOMON_EXPORTS
#  define NCBI_XALGOGNOMON_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOGNOMON_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xalgophytree
 */
#ifdef NCBI_XALGOPHYTREE_EXPORTS
#  define NCBI_XALGOPHYTREE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOPHYTREE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xalgoseq
 */
#ifdef NCBI_XALGOSEQ_EXPORTS
#  define NCBI_XALGOSEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOSEQ_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xalgoseqqa
 */
#ifdef NCBI_XALGOSEQQA_EXPORTS
#  define NCBI_XALGOSEQQA_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOSEQQA_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xalgoseqqa
 */
#ifdef NCBI_XALGOWINMASK_EXPORTS
#  define NCBI_XALGOWINMASK_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOWINMASK_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xalnmgr
 */
#ifdef NCBI_XALNMGR_EXPORTS
#  define NCBI_XALNMGR_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALNMGR_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xcgi
 */
#ifdef NCBI_XCGI_EXPORTS
#  define NCBI_XCGI_EXPORT __declspec(dllexport)
#else
#  define NCBI_XCGI_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xcgi_redirect
 */
#ifdef NCBI_XCGI_REDIRECT_EXPORTS
#  define NCBI_XCGI_REDIRECT_EXPORT __declspec(dllexport)
#else
#  define NCBI_XCGI_REDIRECT_EXPORT __declspec(dllimport)
#endif

#if 0
/*
 * Export specifier for library xgbplugin
 */
#ifdef NCBI_XGBPLUGIN_EXPORTS
#  define NCBI_XGBPLUGIN_EXPORT __declspec(dllexport)
#else
#  define NCBI_XGBPLUGIN_EXPORT __declspec(dllimport)
#endif
#endif
/*
 * Export specifier for library xhtml
 */
#ifdef NCBI_XHTML_EXPORTS
#  define NCBI_XHTML_EXPORT __declspec(dllexport)
#else
#  define NCBI_XHTML_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library ximage
 */
#ifdef NCBI_XIMAGE_EXPORTS
#  define NCBI_XIMAGE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XIMAGE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library ncbi_xloader_blastdb
 */
#ifdef NCBI_XLOADER_BLASTDB_EXPORTS
#  define NCBI_XLOADER_BLASTDB_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_BLASTDB_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library ncbi_xloader_cdd
 */
#ifdef NCBI_XLOADER_CDD_EXPORTS
#  define NCBI_XLOADER_CDD_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_CDD_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library ncbi_xloader_genbank
 */
#ifdef NCBI_XLOADER_GENBANK_EXPORTS
#  define NCBI_XLOADER_GENBANK_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_GENBANK_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library ncbi_xloader_lds
 */
#ifdef NCBI_XLOADER_LDS_EXPORTS
#  define NCBI_XLOADER_LDS_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_LDS_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library ncbi_xloader_table
 */
#ifdef NCBI_XLOADER_TABLE_EXPORTS
#  define NCBI_XLOADER_TABLE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_TABLE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library ncbi_xloader_trace
 */
#ifdef NCBI_XLOADER_TRACE_EXPORTS
#  define NCBI_XLOADER_TRACE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_TRACE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xncbi
 */
#ifdef NCBI_XNCBI_EXPORTS
#  define NCBI_XNCBI_EXPORT __declspec(dllexport)
#else
#  define NCBI_XNCBI_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xobjedit
 */
#ifdef NCBI_XOBJEDIT_EXPORTS
#  define NCBI_XOBJEDIT_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJEDIT_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xobjmanip
 */
#ifdef NCBI_XOBJMANIP_EXPORTS
#  define NCBI_XOBJMANIP_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJMANIP_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xobjmgr
 */
#ifdef NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJMGR_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJMGR_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xobjread
 */
#ifdef NCBI_XOBJREAD_EXPORTS
#  define NCBI_XOBJREAD_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJREAD_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xobjsimple
 */
#ifdef NCBI_XOBJSIMPLE_EXPORTS
#  define NCBI_XOBJSIMPLE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJSIMPLE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xobjutil
 */
#ifdef NCBI_XOBJUTIL_EXPORTS
#  define NCBI_XOBJUTIL_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJUTIL_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xobjwrite
 */
#ifdef NCBI_XOBJWRITE_EXPORTS
#  define NCBI_XOBJWRITE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJWRITE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xomssa
 */
#ifdef NCBI_XOMSSA_EXPORTS
#  define NCBI_XOMSSA_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOMSSA_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xreader
 */
#ifdef NCBI_XREADER_EXPORTS
#  define NCBI_XREADER_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xreader_id1
 */
#ifdef NCBI_XREADER_ID1_EXPORTS
#  define NCBI_XREADER_ID1_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_ID1_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xreader_id2
 */
#ifdef NCBI_XREADER_ID2_EXPORTS
#  define NCBI_XREADER_ID2_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_ID2_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xreader_pubseqos
 */
#ifdef NCBI_XREADER_PUBSEQOS_EXPORTS
#  define NCBI_XREADER_PUBSEQOS_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_PUBSEQOS_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xregexp
 */
#ifdef NCBI_XREGEXP_EXPORTS
#  define NCBI_XREGEXP_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREGEXP_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xserial
 */
#ifdef NCBI_XSERIAL_EXPORTS
#  define NCBI_XSERIAL_EXPORT __declspec(dllexport)
#else
#  define NCBI_XSERIAL_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xsqlite
 */
#ifdef NCBI_XSQLITE_EXPORTS
#  define NCBI_XSQLITE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XSQLITE_EXPORT __declspec(dllimport)
#endif

/*
 * Export specifier for library xutil
 */
#ifdef NCBI_XUTIL_EXPORTS
#  define NCBI_XUTIL_EXPORT __declspec(dllexport)
#else
#  define NCBI_XUTIL_EXPORT __declspec(dllimport)
#endif



#else  /*  !defined(NCBI_OS_MSWIN)  ||  !defined(NCBI_DLL_BUILD)  */

/*
 * NULL operations for other cases
 */

#  define NCBI_ACCESS_EXPORT
#  define NCBI_BDB_CACHE_EXPORT
#  define NCBI_BDB_EXPORT
#  define NCBI_BIBLIO_EXPORT
#  define NCBI_BIOTREE_EXPORT
#  define NCBI_BLASTDB_EXPORT
#  define NCBI_BLAST_EXPORT
#  define NCBI_CDD_EXPORT
#  define NCBI_CN3D_EXPORT
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT
#  define NCBI_DBAPIDRIVER_EXPORT
#  define NCBI_DBAPIDRIVER_FTDS_EXPORT
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT
#  define NCBI_DBAPIUTIL_BLOBSTORE_EXPORT
#  define NCBI_DBAPI_CACHE_EXPORT
#  define NCBI_DBAPI_EXPORT
#  define NCBI_DOCSUM_EXPORT
#  define NCBI_ENTREZ2_EXPORT
#  define NCBI_ENTREZGENE_EXPORT
#  define NCBI_FEATDEF_EXPORT
#  define NCBI_FLAT_EXPORT
#  define NCBI_FORMAT_EXPORT
#  define NCBI_GBSEQ_EXPORT
#  define NCBI_GENERAL_EXPORT
#  define NCBI_ID1_EXPORT
#  define NCBI_ID2_EXPORT
#  define NCBI_ID2_SPLIT_EXPORT
#  define NCBI_INSDSEQ_EXPORT
#  define NCBI_LDS_EXPORT
#  define NCBI_MEDLARS_EXPORT
#  define NCBI_MEDLINE_EXPORT
#  define NCBI_MIM_EXPORT
#  define NCBI_MLA_EXPORT
#  define NCBI_MMDB1_EXPORT
#  define NCBI_MMDB2_EXPORT
#  define NCBI_MMDB3_EXPORT
#  define NCBI_NCBIMIME_EXPORT
#  define NCBI_OBJPRT_EXPORT
#  define NCBI_OMSSA_EXPORT
#  define NCBI_PUBMED_EXPORT
#  define NCBI_PUB_EXPORT
#  define NCBI_REMAP_EXPORT
#  define NCBI_SCOREMAT_EXPORT
#  define NCBI_SEQALIGN_EXPORT
#  define NCBI_SEQBLOCK_EXPORT
#  define NCBI_SEQCODE_EXPORT
#  define NCBI_SEQFEAT_EXPORT
#  define NCBI_SEQLOC_EXPORT
#  define NCBI_SEQRES_EXPORT
#  define NCBI_SEQSET_EXPORT
#  define NCBI_SEQTEST_EXPORT
#  define NCBI_SEQUENCE_EXPORT
#  define NCBI_SEQ_EXPORT
#  define NCBI_SUBMIT_EXPORT
#  define NCBI_TAXON1_EXPORT
#  define NCBI_TINYSEQ_EXPORT
#  define NCBI_VALIDATOR_EXPORT
#  define NCBI_XALGOALIGN_EXPORT
#  define NCBI_XALGOGNOMON_EXPORT
#  define NCBI_XALGOPHYTREE_EXPORT
#  define NCBI_XALGOSEQQA_EXPORT
#  define NCBI_XALGOSEQ_EXPORT
#  define NCBI_XALGOWINMASK_EXPORT
#  define NCBI_XALNMGR_EXPORT
#  define NCBI_XALNUTIL_EXPORT
#  define NCBI_XBLASTFORMAT_EXPORT
#  define NCBI_XCGI_EXPORT
#  define NCBI_XCGI_REDIRECT_EXPORT
#  define NCBI_XHTML_EXPORT
#  define NCBI_XIMAGE_EXPORT
#  define NCBI_XLOADER_BLASTDB_EXPORT
#  define NCBI_XLOADER_CDD_EXPORT
#  define NCBI_XLOADER_GENBANK_EXPORT
#  define NCBI_XLOADER_LDS_EXPORT
#  define NCBI_XLOADER_TABLE_EXPORT
#  define NCBI_XLOADER_TRACE_EXPORT
#  define NCBI_XNCBI_EXPORT
#  define NCBI_XOBJEDIT_EXPORT
#  define NCBI_XOBJMANIP_EXPORT
#  define NCBI_XOBJMGR_EXPORT
#  define NCBI_XOBJREAD_EXPORT
#  define NCBI_XOBJSIMPLE_EXPORT
#  define NCBI_XOBJUTIL_EXPORT
#  define NCBI_XOBJWRITE_EXPORT
#  define NCBI_XOMSSA_EXPORT
#  define NCBI_XREADER_EXPORT
#  define NCBI_XREADER_ID1_EXPORT
#  define NCBI_XREADER_ID2_EXPORT
#  define NCBI_XREADER_PUBSEQOS_EXPORT
#  define NCBI_XREGEXP_EXPORT
#  define NCBI_XSERIAL_EXPORT
#  define NCBI_XSQLITE_EXPORT
#  define NCBI_XUTIL_EXPORT

#endif


/* STATIC LIBRARIES SECTION */
/* This section is for static-only libraries */

#define NCBI_TEST_MT_EXPORT
#define NCBI_XALNUTIL_EXPORT
#define NCBI_XALNTOOL_EXPORT
#define NCBI_XBLASTFORMAT_EXPORT


#endif  /*  CORELIB___MSWIN_EXPORT__H  */


/* @} */




/*
 * ==========================================================================
 * $Log$
 * Revision 1.93  2005/02/12 19:14:34  dicuccio
 * Added export specifiers for xalgowinmask.
 *
 * Revision 1.92  2005/01/25 17:37:29  jianye
 * Added NCBI_XBLASTFORMAT_EXPORT
 *
 * Revision 1.91  2005/01/18 17:59:02  dicuccio
 * Drop NCBI_XBLAST_EXPORT from main export list - maintained separately in
 * blast_export.h, duplicated here
 *
 * Revision 1.90  2004/12/22 21:02:53  grichenk
 * BDB and DBAPI caches split into separate libs.
 * Added entry point registration, fixed driver names.
 *
 * Revision 1.89  2004/12/13 20:34:58  jcherry
 * Moved xobjsimple into its own dll
 *
 * Revision 1.88  2004/12/13 18:00:08  jcherry
 * Added NCBI_XOBJSIMPLE_EXPORT(S) (under NCBI_SEQEXT_EXPORTS)
 *
 * Revision 1.87  2004/12/10 15:26:41  ssikorsk
 * FreeTDS is ported on windows
 *
 * Revision 1.86  2004/10/20 16:53:20  ivanov
 * Renamed NCBI_DBAPIDRIVER_BLOBSTORE_EXPORT->NCBI_DBAPIUTIL_BLOBSTORE_EXPORT
 *
 * Revision 1.85  2004/10/20 16:10:52  ivanov
 * + NCBI_DBAPIDRIVER_BLOBSTORE_EXPORT export specifier
 *
 * Revision 1.84  2004/10/06 19:50:39  jcherry
 * Added NCBI_XALGOSEQQA_EXPORT
 *
 * Revision 1.83  2004/09/07 18:09:21  vakatov
 * DOXIGEN'ization
 *
 * Revision 1.82  2004/08/19 16:24:17  jcherry
 * Added NCBI_SEQTEST_EXPORT (to NCBI_SEQ)
 *
 * Revision 1.81  2004/08/12 17:33:27  dicuccio
 * Expunged last bits of GUI export specifiers
 *
 * Revision 1.80  2004/08/04 15:42:25  dicuccio
 * Moved most GUI export specifiers into gui_export; specifiers for GUICORE,
 * GUICONFIG, and XGBPLUGIN, as well as the GUICORE DLL cluster, remain here.
 *
 * Revision 1.79  2004/08/04 14:37:07  vasilche
 * Added exports for ID2 libraries. ID1 & ID2 moved to ncbi_seqext.dll.
 *
 * Revision 1.78  2004/08/03 13:31:40  dicuccio
 * Ordered export specifiers alphabetically.  Removed duplicate copies of export
 * specifier #ifdef blocks.
 *
 * Revision 1.77  2004/07/22 20:23:53  jcherry
 * Added NCBI_REMAP_EXPORT specifier
 *
 * Revision 1.76  2004/07/06 13:19:56  jcherry
 * Added NCBI_XOBJWRITE_EXPORT specifier (in NCBI_SEQEXT_EXPORTS)
 *
 * Revision 1.75  2004/06/16 15:20:00  shomrat
 * Added export for xobjedit library
 *
 * Revision 1.74  2004/05/24 13:56:42  kuznets
 * Added NCBI_BIOTREE_EXPORT
 *
 * Revision 1.73  2004/05/14 16:15:05  dicuccio
 * Added export specifier for gui/widgets/workspace
 *
 * Revision 1.72  2004/05/03 19:41:02  gorelenk
 * Added NCBI_XALNTOOL_EXPORT to static-only libraries section.
 *
 * Revision 1.71  2004/05/03 13:25:32  dicuccio
 * Corrected export specifier
 *
 * Revision 1.70  2004/05/03 12:36:36  dicuccio
 * added export specifier for library gui_objutils
 *
 * Revision 1.69  2004/04/13 20:10:40  ucko
 * New (shared) ASN.1 spec: insdseq
 *
 * Revision 1.68  2004/03/25 14:19:42  dicuccio
 * Added export specifier for TRACE data loader library
 *
 * Revision 1.67  2004/03/18 15:34:17  gorelenk
 * Export define for library xalnutil ( NCBI_XALNUTIL_EXPORT ) moved
 * to static libraries section .
 *
 * Revision 1.66  2004/03/16 19:44:01  gorelenk
 * Added definition of NCBI_ENTREZGENE_EXPORTS to
 * definition of NCBI_MISC_EXPORTS .
 *
 * Revision 1.65  2004/03/15 17:16:47  gorelenk
 * Removed #undef NCBI_DLL_BUILD when defined _LIB .
 *
 * Revision 1.64  2004/03/12 13:56:46  dicuccio
 * Renamed NCBI_REGEXP_EXPORT -> NCBI_XREGEXP_EXPORT; restored to non-static.
 * Moved GUI modules to top
 *
 * Revision 1.63  2004/03/11 16:52:25  gorelenk
 * Added STATIC LIBRARIES SECTION  - place for static-only libraries export
 * prefix defines.
 * Added defines for NCBI_REGEXP_EXPORT and NCBI_TEST_MT_EXPORT.
 *
 * Revision 1.62  2004/03/11 12:49:41  dicuccio
 * Rearranged GUI export specifiers (regrouped at end).  Added export specifier
 * for GUIMATH library.  Corrected NCBI_TEST_MT_EXPORT.
 *
 * Revision 1.61  2004/03/10 20:21:33  gorelenk
 * Added defines for NCBI_TEST_MT_EXPORT (static lib test_mt.lib) .
 *
 * Revision 1.60  2004/02/13 16:58:30  tereshko
 * Added NCBI_GUIWIDGETS_PHYLO_TREE_EXPORT for Phylogenetic Tree widget / viewer
 *
 * Revision 1.59  2004/02/10 19:55:21  ucko
 * Also add NCBI_XALGOPHYTREE_EXPORT to the empty define list....
 *
 * Revision 1.58  2004/02/10 16:59:01  dicuccio
 * Added export specifier for libxalgophytree
 *
 * Revision 1.57  2004/02/09 19:23:58  ivanov
 * + NCBI_XCGI_REDIRECT_EXPORTS, added it to NCBI_WEB.DLL
 *
 * Revision 1.56  2004/01/14 16:28:55  tereshko
 * Added NCBI_GUIWIDGETSMISC_EXPORTS for Taxplot viewer
 *
 * Revision 1.55  2004/01/13 16:38:34  vasilche
 * Added NCBI_XREADER_EXPORT
 *
 * Revision 1.54  2003/12/22 19:09:13  dicuccio
 * Added taxonomy to ncbi_seq
 *
 * Revision 1.53  2003/12/17 21:02:57  shomrat
 * added export specifier for xformat library
 *
 * Revision 1.52  2003/12/17 19:41:24  kuznets
 * Restored accidentally deleted NCBI_XLOADER_TABLE_EXPORT. Sorry.
 *
 * Revision 1.51  2003/12/17 13:48:39  kuznets
 * Added export symbols for genbank data loader and associated readers.
 *
 * Revision 1.50  2003/12/15 16:56:58  ivanov
 * + NCBI_DBAPI_EXPORT
 *
 * Revision 1.49  2003/12/09 15:59:03  dicuccio
 * Dropped NCBI_GUIWIDGETS_FLEK_EXPORT - repalced with NCBI_GUIWIDGETS_FLU_EXPORT
 *
 * Revision 1.48  2003/11/17 21:38:10  yazhuk
 * Added export specifiers for Hit Matrix library
 *
 * Revision 1.47  2003/11/05 18:40:07  dicuccio
 * Added CGI/HTML library export specifiers
 *
 * Revision 1.46  2003/10/30 15:45:44  ivanov
 * Add NCBI_GUIWIDGETS_GL_EXPORT to the null define list
 *
 * Revision 1.45  2003/10/29 23:05:30  yazhuk
 * Added NCBI_GUIWIDGETS_GL_EXPORT macros
 *
 * Revision 1.44  2003/10/24 21:28:41  lewisg
 * add omssa, xomssa, omssacl to win32 build, including dll
 *
 * Revision 1.43  2003/10/24 15:23:26  dicuccio
 * Fixed gnomon export specifier
 *
 * Revision 1.42  2003/10/24 15:17:15  dicuccio
 * Added export specifier for gnomon
 *
 * Revision 1.41  2003/10/20 18:38:02  dicuccio
 * Added export specifier for xloader_cdd
 *
 * Revision 1.40  2003/10/10 19:34:06  dicuccio
 * Added export specifier for gui_config
 *
 * Revision 1.39  2003/09/29 13:50:17  dicuccio
 * Added export specifiers for XSQLITE, XLOADER_TABLE, and XOBJMANIP
 *
 * Revision 1.38  2003/09/16 14:33:01  dicuccio
 * Cleaned up and clarified export specifiers for the gui/widgets/ libraries -
 * added a specifier for each static project, added top-level groupings for DLL
 * projects
 *
 * Revision 1.37  2003/09/12 13:01:06  dicuccio
 * Added dummy export specifiers for non-Win32 platforms
 *
 * Revision 1.36  2003/09/12 12:56:52  dicuccio
 * Added new export specifiers for gui_widgets_seq, gui_widgets_aln
 *
 * Revision 1.35  2003/09/09 18:53:44  grichenk
 * +ID2
 *
 * Revision 1.34  2003/09/04 14:58:51  dicuccio
 * Added export specifier for GUIGRAPH library
 *
 * Revision 1.33  2003/08/27 16:41:43  ivanov
 * * Added export specifier NCBI_XIMAGE_EXPORT
 *
 * Revision 1.32  2003/08/06 16:09:21  jianye
 * Add specifiers for new libraries: blastdb, xalnutil, xloader_blastdb.
 *
 * Revision 1.31  2003/08/04 15:44:12  dicuccio
 * Added export specifier for libxblast.  Modified layout of algorithm export
 * specifiers
 *
 * Revision 1.30  2003/07/30 16:35:17  kuznets
 * Fixed typo with NCBI_XLOADER_LDS_EXPORT
 *
 * Revision 1.29  2003/07/16 20:15:00  kuznets
 * + NCBI_XLOADER_LDS_EXPORTS (export/import macro for lds dataloader)
 *
 * Revision 1.28  2003/06/27 19:00:08  dicuccio
 * Moved BLAST, scoremat, and ID1 from ncbi_seq.dll to ncbi_seqext.dll
 *
 * Revision 1.27  2003/06/04 17:05:35  ucko
 * +NCBI_XOBJREAD_EXPORT(S), under NCBI_SEQEXT_EXPORTS.
 *
 * Revision 1.26  2003/06/03 19:22:20  dicuccio
 * Fixed specification of MMDB lib exports
 *
 * Revision 1.25  2003/06/03 18:48:54  kuznets
 * + export defines for bdb and lds libraries.
 *
 * Revision 1.24  2003/06/02 16:01:29  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.23  2003/05/23 16:19:48  ivanov
 * Fixed typo
 *
 * Revision 1.22  2003/05/23 15:22:40  ivanov
 * Added export specifier NCBI_MLA_EXPORT
 *
 * Revision 1.21  2003/04/25 21:05:02  ucko
 * +SCOREMAT (under SEQEXT)
 *
 * Revision 1.20  2003/04/15 16:30:38  dicuccio
 * Moved BLAST object files into ncbi_seqext.dll
 *
 * Revision 1.19  2003/04/14 19:37:42  ivanov
 * Added master export group NCBI_SEQEXT_EXPORTS. Moved a part code from NCBI_SEQ.DLL
 * to NCBI_SEQEXT.DLLmswin_export.h
 *
 * Revision 1.18  2003/04/10 13:31:40  dicuccio
 * Added BLAST objects to NCBI_SEQ
 *
 * Revision 1.17  2003/04/09 16:12:05  ivanov
 * Fix for the previous commit
 *
 * Revision 1.16  2003/04/08 20:07:05  ivanov
 * Added export specifiers NCBI_BLAST_EXPORT and NCBI_ENTREZGENE_EXPORT
 *
 * Revision 1.15  2003/03/28 20:29:42  kans
 * define NCBI_FLAT_EXPORT at end
 *
 * Revision 1.14  2003/03/28 17:44:54  dicuccio
 * Added export specifier for flatfile generator library.  Made this specifier
 * part of the NCBI_SEQ_EXPORTS master group
 *
 * Revision 1.13  2003/02/25 19:34:21  kuznets
 * Added NCBI_DBAPIDRIVER_MYSQL_EXPORTS
 *
 * Revision 1.12  2003/02/21 16:42:16  dicuccio
 * Added export specifiers for XALGO, XGBPLUGIN
 *
 * Revision 1.11  2003/02/12 22:02:29  coremake
 * Added export specifiers NCBI_DBAPIDRIVER_*_EXPORT for ctlib, dblib, msdblib
 * and odbc DBAI driver libraries
 *
 * Revision 1.10  2003/02/06 18:49:58  dicuccio
 * Added NCBI_TAXON1_EXPORT specifier
 *
 * Revision 1.9  2003/01/17 19:44:28  lavr
 * Reduce dependencies
 *
 * Revision 1.8  2003/01/16 19:53:13  dicuccio
 * Add NCBI_GUIDIALOGS_EXPORT to the null define list...
 *
 * Revision 1.7  2003/01/16 18:23:15  dicuccio
 * Added export specifiers for library GUI_DIALOGS.DLL
 *
 * Revision 1.6  2003/01/07 22:17:25  lavr
 * Move '#include <connect/connect_export.h>' up
 *
 * Revision 1.5  2003/01/07 19:58:25  shomrat
 * Added NCBI_VALIDATOR_EXPORT
 *
 * Revision 1.4  2002/12/31 16:15:46  dicuccio
 * Added missing NCBI_SUBMIT_EXPORT to empty define list
 *
 * Revision 1.3  2002/12/31 15:08:23  dicuccio
 * Moved featdef and gbseq into ncbi_misc.dll
 *
 * Revision 1.2  2002/12/26 12:51:41  dicuccio
 * Fixed some minor niggling errors with export specifiers in the wrong places.
 *
 * Revision 1.1  2002/12/18 22:52:19  dicuccio
 * Initial revision.
 *
 * ==========================================================================
 */
