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
 *
 */

/**
 * @file mswin_export.h
 *
 * Defines to provide correct exporting from DLLs in Windows.
 *
 * Defines to provide correct exporting from DLLs in Windows.
 * These are necessary to compile DLLs with Visual C++ - exports must be
 * explicitly labeled as such.
 *
 */

/** @addtogroup WinDLL
 *
 * @{
 */


#if defined(NCBI_OS_MSWIN)  &&  defined(NCBI_DLL_BUILD)

#ifndef _MSC_VER
#  error "This toolkit is not buildable with a compiler other than MSVC."
#endif


/* Dumping ground for Windows-specific stuff
 */
#pragma warning (disable : 4786 4251 4275 4800)
#pragma warning (3 : 4062 4191 4263 4265 4287 4296)


/* -------------------------------------------------
 * DLL clusters
 */


/* Definitions for NCBI_CORE.DLL
 */
#ifdef NCBI_CORE_EXPORTS
#  define NCBI_XNCBI_EXPORTS
#  define NCBI_XSERIAL_EXPORTS
#  define NCBI_XUTIL_EXPORTS
#  define NCBI_XREGEXP_EXPORTS
#endif


/* Definitions for NCBI_PUB.DLL
 */
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_BIBLIO_EXPORTS
#  define NCBI_MEDLINE_EXPORTS
#  define NCBI_MEDLARS_EXPORTS
#  define NCBI_MLA_EXPORTS
#  define NCBI_PUBMED_EXPORTS
#endif


/* Definitions for NCBI_SEQ.DLL
 */
#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_BLASTDB_EXPORTS
#  define NCBI_BLASTXML_EXPORTS
#  define NCBI_BLAST_EXPORTS
#  define NCBI_GENOME_COLLECTION_EXPORTS
#  define NCBI_SCOREMAT_EXPORTS
#  define NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQEDIT_EXPORTS
#  define NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQLOC_EXPORTS
#  define NCBI_SEQRES_EXPORTS
#  define NCBI_SEQSET_EXPORTS
#  define NCBI_SEQTEST_EXPORTS
#  define NCBI_SUBMIT_EXPORTS
#  define NCBI_TAXON1_EXPORTS
#endif


/* Definitions for NCBI_SEQEXT.DLL
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
#  define NCBI_CLEANUP_EXPORTS
#  define NCBI_VALERR_EXPORTS
#endif


/* Definitions for NCBI_MISC.DLL
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
#  define NCBI_PROJ_EXPORTS
#  define NCBI_PCASSAY_EXPORTS
#  define NCBI_PCSUBSTANCE_EXPORTS
#endif


/* Definitions for NCBI_MMDB.DLL
 */
#ifdef NCBI_MMDB_EXPORTS
#  define NCBI_CDD_EXPORTS
#  define NCBI_CN3D_EXPORTS
#  define NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB3_EXPORTS
#  define NCBI_NCBIMIME_EXPORTS
#endif


/* Definitions for NCBI_ALGO.DLL
 */
#ifdef NCBI_XALGO_EXPORTS
#  define NCBI_SEQ_EXPORTS
#  define NCBI_COBALT_EXPORTS
#  define NCBI_XALGOALIGN_EXPORTS
#  define NCBI_XALGOSEQ_EXPORTS
#  define NCBI_XALGOGNOMON_EXPORTS
#  define NCBI_XALGOPHYTREE_EXPORTS
#  define NCBI_XALGOSEQQA_EXPORTS
#  define NCBI_XALGOWINMASK_EXPORTS
#  define NCBI_XALGODUSTMASK_EXPORTS
#  define NCBI_XALGOCONTIG_ASSEMBLY_EXPORTS
#endif


/* Definitions for NCBI_WEB.DLL
 */
#ifdef NCBI_WEB_EXPORTS
#  define NCBI_XHTML_EXPORTS
#  define NCBI_XCGI_EXPORTS
#  define NCBI_XCGI_REDIRECT_EXPORTS
#endif


/* Definitions for NCBI_ALGO_MS.DLL
 */
#ifdef NCBI_ALGOMS_EXPORTS
#  define NCBI_OMSSA_EXPORTS
#  define NCBI_XOMSSA_EXPORTS
#  define NCBI_PEPXML_EXPORTS
#endif

/* Definitions for NCBI_ALGO_STRUCTURE.DLL
 */
#ifdef NCBI_ALGOSTRUCTURE_EXPORTS
#  define NCBI_BMAREFINE_EXPORTS
#  define NCBI_CDUTILS_EXPORTS
#  define NCBI_STRUCTDP_EXPORTS
#  define NCBI_STRUCTUTIL_EXPORTS
#  define NCBI_THREADER_EXPORTS
#endif


/* ------------------------------------------------- */

/* Individual Library Definitions
 * Please keep alphabetized!
 */

/* Export specifier for library access
 */
#ifdef NCBI_ACCESS_EXPORTS
#  define NCBI_ACCESS_EXPORT __declspec(dllexport)
#else
#  define NCBI_ACCESS_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library bdb
 */
#ifdef NCBI_BDB_EXPORTS
#  define NCBI_BDB_EXPORT __declspec(dllexport)
#else
#  define NCBI_BDB_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library bdb
 */
#ifdef NCBI_BDB_CACHE_EXPORTS
#  define NCBI_BDB_CACHE_EXPORT __declspec(dllexport)
#else
#  define NCBI_BDB_CACHE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library netcache (ICache)
 */
#ifdef NCBI_NET_CACHE_EXPORTS
#  define NCBI_NET_CACHE_EXPORT __declspec(dllexport)
#else
#  define NCBI_NET_CACHE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library netcache (IBlobStorage)
 */
#ifdef NCBI_BLOBSTORAGE_NETCACHE_EXPORTS
#  define NCBI_BLOBSTORAGE_NETCACHE_EXPORT __declspec(dllexport)
#else
#  define NCBI_BLOBSTORAGE_NETCACHE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library filestorage (IBlobStorage)
 */
#ifdef NCBI_BLOBSTORAGE_FILE_EXPORTS
#  define NCBI_BLOBSTORAGE_FILE_EXPORT __declspec(dllexport)
#else
#  define NCBI_BLOBSTORAGE_FILE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library biblo
 */
#ifdef NCBI_BIBLIO_EXPORTS
#  define NCBI_BIBLIO_EXPORT __declspec(dllexport)
#else
#  define NCBI_BIBLIO_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library biotree
 */
#ifdef NCBI_BIOTREE_EXPORTS
#  define NCBI_BIOTREE_EXPORT __declspec(dllexport)
#else
#  define NCBI_BIOTREE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library blastdb
 */
#ifdef NCBI_BLASTDB_EXPORTS
#  define NCBI_BLASTDB_EXPORT __declspec(dllexport)
#else
#  define NCBI_BLASTDB_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library blastxml
 */
#ifdef NCBI_BLASTXML_EXPORTS
#  define NCBI_BLASTXML_EXPORT __declspec(dllexport)
#else
#  define NCBI_BLASTXML_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library blast
 */
#ifdef NCBI_BLAST_EXPORTS
#  define NCBI_BLAST_EXPORT __declspec(dllexport)
#else
#  define NCBI_BLAST_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library cdd
 */
#ifdef NCBI_BMAREFINE_EXPORTS
#  define NCBI_BMAREFINE_EXPORT __declspec(dllexport)
#else
#  define NCBI_BMAREFINE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library cdd
 */
#ifdef NCBI_CDD_EXPORTS
#  define NCBI_CDD_EXPORT __declspec(dllexport)
#else
#  define NCBI_CDD_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library cdd
 */
#ifdef NCBI_CDUTILS_EXPORTS
#  define NCBI_CDUTILS_EXPORT __declspec(dllexport)
#else
#  define NCBI_CDUTILS_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library cn3d
 */
#ifdef NCBI_CN3D_EXPORTS
#  define NCBI_CN3D_EXPORT __declspec(dllexport)
#else
#  define NCBI_CN3D_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_driver
 */
#ifdef NCBI_DBAPIDRIVER_EXPORTS
#  define NCBI_DBAPIDRIVER_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_driver_ctlib
 */
#ifdef NCBI_DBAPIDRIVER_CTLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_driver_dblib
 */
#ifdef NCBI_DBAPIDRIVER_DBLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_driver_msdblib
 */
#ifdef NCBI_DBAPIDRIVER_MSDBLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_driver_mysql
 */
#ifdef NCBI_DBAPIDRIVER_MYSQL_EXPORTS
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_driver_odbc
 */
#ifdef NCBI_DBAPIDRIVER_ODBC_EXPORTS
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_driver_ftds
 */
#ifdef NCBI_DBAPIDRIVER_FTDS_EXPORTS
#  define NCBI_DBAPIDRIVER_FTDS_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_FTDS_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_driver_sqlite3
 */
#ifdef NCBI_DBAPIDRIVER_SQLITE3_EXPORTS
#  define NCBI_DBAPIDRIVER_SQLITE3_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_SQLITE3_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi
 */
#ifdef NCBI_DBAPI_EXPORTS
#  define NCBI_DBAPI_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPI_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi
 */
#ifdef NCBI_DBAPI_CACHE_EXPORTS
#  define NCBI_DBAPI_CACHE_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPI_CACHE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library dbapi_util_blobstore
 */
#ifdef NCBI_DBAPIUTIL_BLOBSTORE_EXPORTS
#  define NCBI_DBAPIUTIL_BLOBSTORE_EXPORT __declspec(dllexport)
#else
#  define NCBI_DBAPIUTIL_BLOBSTORE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library docsum
 */
#ifdef NCBI_DOCSUM_EXPORTS
#  define NCBI_DOCSUM_EXPORT __declspec(dllexport)
#else
#  define NCBI_DOCSUM_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library entrez2
 */
#ifdef NCBI_ENTREZ2_EXPORTS
#  define NCBI_ENTREZ2_EXPORT __declspec(dllexport)
#else
#  define NCBI_ENTREZ2_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library entrezgene
 */
#ifdef NCBI_ENTREZGENE_EXPORTS
#  define NCBI_ENTREZGENE_EXPORT __declspec(dllexport)
#else
#  define NCBI_ENTREZGENE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library featdef
 */
#ifdef NCBI_FEATDEF_EXPORTS
#  define NCBI_FEATDEF_EXPORT __declspec(dllexport)
#else
#  define NCBI_FEATDEF_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library flat
 */
#ifdef NCBI_FLAT_EXPORTS
#  define NCBI_FLAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_FLAT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library format
 */
#ifdef NCBI_FORMAT_EXPORTS
#  define NCBI_FORMAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_FORMAT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library gbseq
 */
#ifdef NCBI_GBSEQ_EXPORTS
#  define NCBI_GBSEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_GBSEQ_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library general
 */
#ifdef NCBI_GENERAL_EXPORTS
#  define NCBI_GENERAL_EXPORT __declspec(dllexport)
#else
#  define NCBI_GENERAL_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library sgenome_collection
 */
#ifdef NCBI_GENOME_COLLECTION_EXPORTS
#  define NCBI_GENOME_COLLECTION_EXPORT __declspec(dllexport)
#else
#  define NCBI_GENOME_COLLECTION_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library id1
 */
#ifdef NCBI_ID1_EXPORTS
#  define NCBI_ID1_EXPORT __declspec(dllexport)
#else
#  define NCBI_ID1_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library id2
 */
#ifdef NCBI_ID2_EXPORTS
#  define NCBI_ID2_EXPORT __declspec(dllexport)
#else
#  define NCBI_ID2_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library id2_split
 */
#ifdef NCBI_ID2_SPLIT_EXPORTS
#  define NCBI_ID2_SPLIT_EXPORT           __declspec(dllexport)
#else
#  define NCBI_ID2_SPLIT_EXPORT           __declspec(dllimport)
#endif

/* Export specifier for library insdseq
 */
#ifdef NCBI_INSDSEQ_EXPORTS
#  define NCBI_INSDSEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_INSDSEQ_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library lds
 */
#ifdef NCBI_LDS_EXPORTS
#  define NCBI_LDS_EXPORT __declspec(dllexport)
#else
#  define NCBI_LDS_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library medlars
 */
#ifdef NCBI_MEDLARS_EXPORTS
#  define NCBI_MEDLARS_EXPORT __declspec(dllexport)
#else
#  define NCBI_MEDLARS_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library medline
 */
#ifdef NCBI_MEDLINE_EXPORTS
#  define NCBI_MEDLINE_EXPORT __declspec(dllexport)
#else
#  define NCBI_MEDLINE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library mim
 */
#ifdef NCBI_MIM_EXPORTS
#  define NCBI_MIM_EXPORT __declspec(dllexport)
#else
#  define NCBI_MIM_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library mla
 */
#ifdef NCBI_MLA_EXPORTS
#  define NCBI_MLA_EXPORT __declspec(dllexport)
#else
#  define NCBI_MLA_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library mmdb1
 */
#ifdef NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB1_EXPORT __declspec(dllexport)
#else
#  define NCBI_MMDB1_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library mmdb2
 */
#ifdef NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB2_EXPORT __declspec(dllexport)
#else
#  define NCBI_MMDB2_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library mmdb3
 */
#ifdef NCBI_MMDB3_EXPORTS
#  define NCBI_MMDB3_EXPORT __declspec(dllexport)
#else
#  define NCBI_MMDB3_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_mime
 */
#ifdef NCBI_NCBIMIME_EXPORTS
#  define NCBI_NCBIMIME_EXPORT __declspec(dllexport)
#else
#  define NCBI_NCBIMIME_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library objprt
 */
#ifdef NCBI_OBJPRT_EXPORTS
#  define NCBI_OBJPRT_EXPORT __declspec(dllexport)
#else
#  define NCBI_OBJPRT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library omssa
 */
#ifdef NCBI_OMSSA_EXPORTS
#  define NCBI_OMSSA_EXPORT __declspec(dllexport)
#else
#  define NCBI_OMSSA_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library pcassay
 */
#ifdef NCBI_PCASSAY_EXPORTS
#  define NCBI_PCASSAY_EXPORT __declspec(dllexport)
#else
#  define NCBI_PCASSAY_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library pcsubstance
 */
#ifdef NCBI_PCSUBSTANCE_EXPORTS
#  define NCBI_PCSUBSTANCE_EXPORT __declspec(dllexport)
#else
#  define NCBI_PCSUBSTANCE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library pepxml
 */
#ifdef NCBI_PEPXML_EXPORTS
#  define NCBI_PEPXML_EXPORT __declspec(dllexport)
#else
#  define NCBI_PEPXML_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library proj
 */
#ifdef NCBI_PROJ_EXPORTS
#  define NCBI_PROJ_EXPORT __declspec(dllexport)
#else
#  define NCBI_PROJ_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library pubmed
 */
#ifdef NCBI_PUBMED_EXPORTS
#  define NCBI_PUBMED_EXPORT __declspec(dllexport)
#else
#  define NCBI_PUBMED_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library pub
 */
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_PUB_EXPORT __declspec(dllexport)
#else
#  define NCBI_PUB_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library remap
 */
#ifdef NCBI_REMAP_EXPORTS
#  define NCBI_REMAP_EXPORT __declspec(dllexport)
#else
#  define NCBI_REMAP_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library scoremat
 */
#ifdef NCBI_SCOREMAT_EXPORTS
#  define NCBI_SCOREMAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_SCOREMAT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library seqalign
 */
#ifdef NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQALIGN_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQALIGN_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library seqblock
 */
#ifdef NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQBLOCK_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQBLOCK_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library seqcode
 */
#ifdef NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQCODE_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQCODE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library seqfeat
 */
#ifdef NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQFEAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQFEAT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library seqloc
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

/* Export specifier for library seqset
 */
#ifdef NCBI_SEQSET_EXPORTS
#  define NCBI_SEQSET_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQSET_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library seqedit
 */
#ifdef NCBI_SEQEDIT_EXPORTS
#  define NCBI_SEQEDIT_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQEDIT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library seqtest
 */
#ifdef NCBI_SEQTEST_EXPORTS
#  define NCBI_SEQTEST_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQTEST_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library seq
 */
#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_SEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_SEQ_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library struct_dp
 */
#ifdef NCBI_STRUCTDP_EXPORTS
#  define NCBI_STRUCTDP_EXPORT __declspec(dllexport)
#else
#  define NCBI_STRUCTDP_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library struct_util
 */
#ifdef NCBI_STRUCTUTIL_EXPORTS
#  define NCBI_STRUCTUTIL_EXPORT __declspec(dllexport)
#else
#  define NCBI_STRUCTUTIL_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library submit
 */
#ifdef NCBI_SUBMIT_EXPORTS
#  define NCBI_SUBMIT_EXPORT __declspec(dllexport)
#else
#  define NCBI_SUBMIT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library taxon1
 */
#ifdef NCBI_TAXON1_EXPORTS
#  define NCBI_TAXON1_EXPORT __declspec(dllexport)
#else
#  define NCBI_TAXON1_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library threader
 */
#ifdef NCBI_THREADER_EXPORTS
#  define NCBI_THREADER_EXPORT __declspec(dllexport)
#else
#  define NCBI_THREADER_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library tinyseq
 */
#ifdef NCBI_TINYSEQ_EXPORTS
#  define NCBI_TINYSEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_TINYSEQ_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library validator
 */
#ifdef NCBI_VALIDATOR_EXPORTS
#  define NCBI_VALIDATOR_EXPORT __declspec(dllexport)
#else
#  define NCBI_VALIDATOR_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library valerr
 */
#ifdef NCBI_VALERR_EXPORTS
#  define NCBI_VALERR_EXPORT __declspec(dllexport)
#else
#  define NCBI_VALERR_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library cleanup
 */
#ifdef NCBI_CLEANUP_EXPORTS
#  define NCBI_CLEANUP_EXPORT __declspec(dllexport)
#else
#  define NCBI_CLEANUP_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgoalign
 */
#ifdef NCBI_COBALT_EXPORTS
#  define NCBI_COBALT_EXPORT __declspec(dllexport)
#else
#  define NCBI_COBALT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgoalign
 */
#ifdef NCBI_XALGOALIGN_EXPORTS
#  define NCBI_XALGOALIGN_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOALIGN_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgocontig_assembly
 */
#ifdef NCBI_XALGOCONTIG_ASSEMBLY_EXPORTS
#  define NCBI_XALGOCONTIG_ASSEMBLY_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOCONTIG_ASSEMBLY_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgognomon
 */
#ifdef NCBI_XALGOGNOMON_EXPORTS
#  define NCBI_XALGOGNOMON_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOGNOMON_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgophytree
 */
#ifdef NCBI_XALGOPHYTREE_EXPORTS
#  define NCBI_XALGOPHYTREE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOPHYTREE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgoseq
 */
#ifdef NCBI_XALGOSEQ_EXPORTS
#  define NCBI_XALGOSEQ_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOSEQ_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgoseqqa
 */
#ifdef NCBI_XALGOSEQQA_EXPORTS
#  define NCBI_XALGOSEQQA_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOSEQQA_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgowinmask
 */
#ifdef NCBI_XALGOWINMASK_EXPORTS
#  define NCBI_XALGOWINMASK_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGOWINMASK_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalgodustmask
 */
#ifdef NCBI_XALGODUSTMASK_EXPORTS
#  define NCBI_XALGODUSTMASK_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALGODUSTMASK_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xalnmgr
 */
#ifdef NCBI_XALNMGR_EXPORTS
#  define NCBI_XALNMGR_EXPORT __declspec(dllexport)
#else
#  define NCBI_XALNMGR_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xblastformat
 */
#ifdef NCBI_XBLASTFORMAT_EXPORTS
#  define NCBI_XBLASTFORMAT_EXPORT __declspec(dllexport)
#else
#  define NCBI_XBLASTFORMAT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xcgi
 */
#ifdef NCBI_XCGI_EXPORTS
#  define NCBI_XCGI_EXPORT __declspec(dllexport)
#else
#  define NCBI_XCGI_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xcgi_redirect
 */
#ifdef NCBI_XCGI_REDIRECT_EXPORTS
#  define NCBI_XCGI_REDIRECT_EXPORT __declspec(dllexport)
#else
#  define NCBI_XCGI_REDIRECT_EXPORT __declspec(dllimport)
#endif

#if 0
/* Export specifier for library xgbplugin
 */
#ifdef NCBI_XGBPLUGIN_EXPORTS
#  define NCBI_XGBPLUGIN_EXPORT __declspec(dllexport)
#else
#  define NCBI_XGBPLUGIN_EXPORT __declspec(dllimport)
#endif
#endif

/* Export specifier for library xgridcgi
 */
#ifdef NCBI_XGRIDCGI_EXPORTS
#  define NCBI_XGRIDCGI_EXPORT __declspec(dllexport)
#else
#  define NCBI_XGRIDCGI_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xhtml
 */
#ifdef NCBI_XHTML_EXPORTS
#  define NCBI_XHTML_EXPORT __declspec(dllexport)
#else
#  define NCBI_XHTML_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ximage
 */
#ifdef NCBI_XIMAGE_EXPORTS
#  define NCBI_XIMAGE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XIMAGE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_xloader_blastdb
 */
#ifdef NCBI_XLOADER_BLASTDB_EXPORTS
#  define NCBI_XLOADER_BLASTDB_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_BLASTDB_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_xloader_remoteblast
 */
#ifdef NCBI_XLOADER_REMOTEBLAST_EXPORTS
#  define NCBI_XLOADER_REMOTEBLAST_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_REMOTEBLAST_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_xloader_cdd
 */
#ifdef NCBI_XLOADER_CDD_EXPORTS
#  define NCBI_XLOADER_CDD_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_CDD_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_xloader_genbank
 */
#ifdef NCBI_XLOADER_GENBANK_EXPORTS
#  define NCBI_XLOADER_GENBANK_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_GENBANK_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_xloader_lds
 */
#ifdef NCBI_XLOADER_LDS_EXPORTS
#  define NCBI_XLOADER_LDS_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_LDS_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_xloader_table
 */
#ifdef NCBI_XLOADER_TABLE_EXPORTS
#  define NCBI_XLOADER_TABLE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_TABLE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_xloader_trace
 */
#ifdef NCBI_XLOADER_TRACE_EXPORTS
#  define NCBI_XLOADER_TRACE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XLOADER_TRACE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library ncbi_xloader_patcher
 */
#ifdef  NCBI_XLOADER_PATCHER_EXPORTS
#  define  NCBI_XLOADER_PATCHER_EXPORT __declspec(dllexport)
#else
#  define  NCBI_XLOADER_PATCHER_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xncbi
 */
#ifdef NCBI_XNCBI_EXPORTS
#  define NCBI_XNCBI_EXPORT __declspec(dllexport)
#else
#  define NCBI_XNCBI_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xobjedit
 */
#ifdef NCBI_XOBJEDIT_EXPORTS
#  define NCBI_XOBJEDIT_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJEDIT_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xobjmanip
 */
#ifdef NCBI_XOBJMANIP_EXPORTS
#  define NCBI_XOBJMANIP_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJMANIP_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xobjmgr
 */
#ifdef NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJMGR_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJMGR_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xobjread
 */
#ifdef NCBI_XOBJREAD_EXPORTS
#  define NCBI_XOBJREAD_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJREAD_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xobjsimple
 */
#ifdef NCBI_XOBJSIMPLE_EXPORTS
#  define NCBI_XOBJSIMPLE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJSIMPLE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xobjutil
 */
#ifdef NCBI_XOBJUTIL_EXPORTS
#  define NCBI_XOBJUTIL_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJUTIL_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xobjwrite
 */
#ifdef NCBI_XOBJWRITE_EXPORTS
#  define NCBI_XOBJWRITE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOBJWRITE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xomssa
 */
#ifdef NCBI_XOMSSA_EXPORTS
#  define NCBI_XOMSSA_EXPORT __declspec(dllexport)
#else
#  define NCBI_XOMSSA_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xreader
 */
#ifdef NCBI_XREADER_EXPORTS
#  define NCBI_XREADER_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xreader_id1
 */
#ifdef NCBI_XREADER_ID1_EXPORTS
#  define NCBI_XREADER_ID1_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_ID1_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xreader_id2
 */
#ifdef NCBI_XREADER_ID2_EXPORTS
#  define NCBI_XREADER_ID2_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_ID2_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xreader_id2
 */
#ifdef NCBI_XREADER_CACHE_EXPORTS
#  define NCBI_XREADER_CACHE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_CACHE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xreader_pubseqos
 */
#ifdef NCBI_XREADER_PUBSEQOS_EXPORTS
#  define NCBI_XREADER_PUBSEQOS_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_PUBSEQOS_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xreader_pubseqos
 */
#ifdef NCBI_XREADER_PUBSEQOS2_EXPORTS
#  define NCBI_XREADER_PUBSEQOS2_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREADER_PUBSEQOS2_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xregexp
 */
#ifdef NCBI_XREGEXP_EXPORTS
#  define NCBI_XREGEXP_EXPORT __declspec(dllexport)
#else
#  define NCBI_XREGEXP_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xserial
 */
#ifdef NCBI_XSERIAL_EXPORTS
#  define NCBI_XSERIAL_EXPORT __declspec(dllexport)
#else
#  define NCBI_XSERIAL_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xsqlite
 */
#ifdef NCBI_XSQLITE_EXPORTS
#  define NCBI_XSQLITE_EXPORT __declspec(dllexport)
#else
#  define NCBI_XSQLITE_EXPORT __declspec(dllimport)
#endif

/* Export specifier for library xutil
 */
#ifdef NCBI_XUTIL_EXPORTS
#  define NCBI_XUTIL_EXPORT __declspec(dllexport)
#else
#  define NCBI_XUTIL_EXPORT __declspec(dllimport)
#endif


/* Export specifier for library eutils
 */
#ifdef NCBI_EUTILS_EXPORTS
#  define NCBI_EUTILS_EXPORT __declspec(dllexport)
#else
#  define NCBI_EUTILS_EXPORT __declspec(dllimport)
#endif


#else  /* !defined(NCBI_OS_MSWIN)  ||  !defined(NCBI_DLL_BUILD) */


/* NULL operations for other cases
 */
#  define NCBI_ACCESS_EXPORT
#  define NCBI_BDB_CACHE_EXPORT
#  define NCBI_BDB_EXPORT
#  define NCBI_BIBLIO_EXPORT
#  define NCBI_BIOTREE_EXPORT
#  define NCBI_BLASTDB_EXPORT
#  define NCBI_BLASTXML_EXPORT
#  define NCBI_BLAST_EXPORT
#  define NCBI_BLOBSTORAGE_NETCACHE_EXPORT
#  define NCBI_BLOBSTORAGE_FILE_EXPORT
#  define NCBI_BMAREFINE_EXPORT
#  define NCBI_CDD_EXPORT
#  define NCBI_CDUTILS_EXPORT
#  define NCBI_CLEANUP_EXPORT
#  define NCBI_CN3D_EXPORT
#  define NCBI_COBALT_EXPORT
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT
#  define NCBI_DBAPIDRIVER_EXPORT
#  define NCBI_DBAPIDRIVER_FTDS_EXPORT
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT
#  define NCBI_DBAPIDRIVER_SQLITE3_EXPORT
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
#  define NCBI_GENOME_COLLECTION_EXPORT
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
#  define NCBI_NET_CACHE_EXPORT
#  define NCBI_OBJPRT_EXPORT
#  define NCBI_OMSSA_EXPORT
#  define NCBI_PCASSAY_EXPORT
#  define NCBI_PCSUBSTANCE_EXPORT
#  define NCBI_PEPXML_EXPORT
#  define NCBI_PROJ_EXPORT
#  define NCBI_PUBMED_EXPORT
#  define NCBI_PUB_EXPORT
#  define NCBI_REMAP_EXPORT
#  define NCBI_SCOREMAT_EXPORT
#  define NCBI_SEQALIGN_EXPORT
#  define NCBI_SEQBLOCK_EXPORT
#  define NCBI_SEQCODE_EXPORT
#  define NCBI_SEQEDIT_EXPORT
#  define NCBI_SEQFEAT_EXPORT
#  define NCBI_SEQLOC_EXPORT
#  define NCBI_SEQRES_EXPORT
#  define NCBI_SEQSET_EXPORT
#  define NCBI_SEQTEST_EXPORT
#  define NCBI_SEQUENCE_EXPORT
#  define NCBI_SEQ_EXPORT
#  define NCBI_STRUCTDP_EXPORT
#  define NCBI_STRUCTUTIL_EXPORT
#  define NCBI_SUBMIT_EXPORT
#  define NCBI_TAXON1_EXPORT
#  define NCBI_THREADER_EXPORT
#  define NCBI_TINYSEQ_EXPORT
#  define NCBI_VALERR_EXPORT
#  define NCBI_VALIDATOR_EXPORT
#  define NCBI_XALGOALIGN_EXPORT
#  define NCBI_XALGOCONTIG_ASSEMBLY_EXPORT
#  define NCBI_XALGODUSTMASK_EXPORT
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
#  define NCBI_XCONNEXT_EXPORT
#  define NCBI_XGRIDCGI_EXPORT
#  define NCBI_XHTML_EXPORT
#  define NCBI_XIMAGE_EXPORT
#  define NCBI_XLOADER_BLASTDB_EXPORT
#  define NCBI_XLOADER_REMOTEBLAST_EXPORT
#  define NCBI_XLOADER_CDD_EXPORT
#  define NCBI_XLOADER_GENBANK_EXPORT
#  define NCBI_XLOADER_LDS_EXPORT
#  define NCBI_XLOADER_PATCHER_EXPORT
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
#  define NCBI_XREADER_CACHE_EXPORT
#  define NCBI_XREADER_EXPORT
#  define NCBI_XREADER_ID1_EXPORT
#  define NCBI_XREADER_ID2_EXPORT
#  define NCBI_XREADER_PUBSEQOS_EXPORT
#  define NCBI_XREADER_PUBSEQOS2_EXPORT
#  define NCBI_XREGEXP_EXPORT
#  define NCBI_XSERIAL_EXPORT
#  define NCBI_XSQLITE_EXPORT
#  define NCBI_XUTIL_EXPORT
#  define NCBI_EUTILS_EXPORT
#endif


/* STATIC LIBRARIES SECTION */
/* This section is for static-only libraries */

#define NCBI_TEST_MT_EXPORT
#define NCBI_XALNUTIL_EXPORT
#define NCBI_XALNTOOL_EXPORT


#endif  /*  CORELIB___MSWIN_EXPORT__H  */


/* @} */
