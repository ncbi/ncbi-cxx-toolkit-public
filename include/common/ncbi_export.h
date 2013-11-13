#ifndef COMMON___NCBI_EXPORT__H
#define COMMON___NCBI_EXPORT__H

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
 * Authors:  Mike DiCuccio, Aaron Ucko
 *
 *
 */

/**
 * @file ncbi_export.h
 *
 * Defines to provide correct exporting from DLLs in some configurations.
 *
 * Defines to provide correct exporting from DLLs in some configurations.
 * These are necessary to compile DLLs with Visual C++ -- exports must be
 * explicitly labeled as such.  Builds on other platforms that take
 * advantage of GCC 4.x's -fvisibility options can also run into problems
 * with cross-module exception passing when not explicitly marking some
 * definitions as globally visible.
 *
 */

#include <ncbiconf.h>

/** @addtogroup WinDLL
 *
 * @{
 */


#if defined(NCBI_OS_MSWIN)  &&  defined(NCBI_DLL_BUILD)

#  ifndef _MSC_VER
#    error "This toolkit is not buildable with a compiler other than MSVC."
#  endif


/* Dumping ground for Windows-specific stuff
 */
#  pragma warning (disable : 4786 4251 4275 4800)
#  pragma warning (3 : 4062 4191 4263 4265 4287 4239 4296)

#  define NCBI_DLL_EXPORT __declspec(dllexport)
#  define NCBI_DLL_IMPORT __declspec(dllimport)

#elif defined(HAVE_ATTRIBUTE_VISIBILITY_DEFAULT)
/* Compensate for possible use of -fvisibility=hidden; the Unix build
 * system doesn't normally pass any visibility flags at present
 * (August 2009), but Xcode is another matter.
 */
#  define NCBI_DLL_EXPORT __attribute__((visibility("default")))
#  define NCBI_DLL_IMPORT __attribute__((visibility("default")))
#else
#  define NCBI_DLL_EXPORT
#  define NCBI_DLL_IMPORT
#endif

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
#  define NCBI_TAXON3_EXPORTS
#  define NCBI_VARIATION_EXPORTS
#endif


/* Definitions for NCBI_SEQEXT.DLL
 */
#ifdef NCBI_SEQEXT_EXPORTS
#  define NCBI_SNPUTIL_EXPORTS
#  define NCBI_ID1_EXPORTS
#  define NCBI_ID2_EXPORTS
#  define NCBI_ID2_SPLIT_EXPORTS
#  define NCBI_FLAT_EXPORTS
#  define NCBI_XALNMGR_EXPORTS
#  define NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJREAD_EXPORTS
#  define NCBI_XOBJWRITE_EXPORTS
#  define NCBI_XOBJRWUTIL_EXPORTS
#  define NCBI_XOBJUTIL_EXPORTS
#  define NCBI_XOBJMANIP_EXPORTS
#  define NCBI_FORMAT_EXPORTS
#  define NCBI_XOBJEDIT_EXPORTS
#  define NCBI_CLEANUP_EXPORTS
#  define NCBI_UNIT_TEST_UTIL_EXPORTS
#  define NCBI_UUDUTIL_EXPORTS
#  define NCBI_VALERR_EXPORTS
#  define NCBI_BLASTDB_FORMAT_EXPORTS
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
#  define NCBI_OBJCOORDS_EXPORTS
#  define NCBI_OBJPRT_EXPORTS
#  define NCBI_TINYSEQ_EXPORTS
#  define NCBI_ENTREZGENE_EXPORTS
#  define NCBI_BIOTREE_EXPORTS
#  define NCBI_REMAP_EXPORTS
#  define NCBI_PROJ_EXPORTS
#  define NCBI_PCASSAY_EXPORTS
#  define NCBI_PCSUBSTANCE_EXPORTS
#  define NCBI_GENESBYLOC_EXPORTS
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
#  define NCBI_XALGOSEGMASK_EXPORTS
#  define NCBI_XALGOCONTIG_ASSEMBLY_EXPORTS
#  define NCBI_XALGOID_MAPPER_EXPORTS
#  define NCBI_XBLASTFORMAT_EXPORTS
#  define NCBI_XPRIMER_EXPORTS
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
#  define NCBI_UNIMOD_EXPORTS
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
#  define NCBI_ACCESS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_ACCESS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library bdb
 */
#ifdef NCBI_BDB_EXPORTS
#  define NCBI_BDB_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BDB_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library bdb
 */
#ifdef NCBI_BDB_CACHE_EXPORTS
#  define NCBI_BDB_CACHE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BDB_CACHE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library netcache (ICache)
 */
#ifdef NCBI_NET_CACHE_EXPORTS
#  define NCBI_NET_CACHE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_NET_CACHE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library netcache (IBlobStorage)
 */
#ifdef NCBI_BLOBSTORAGE_NETCACHE_EXPORTS
#  define NCBI_BLOBSTORAGE_NETCACHE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BLOBSTORAGE_NETCACHE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library filestorage (IBlobStorage)
 */
#ifdef NCBI_BLOBSTORAGE_FILE_EXPORTS
#  define NCBI_BLOBSTORAGE_FILE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BLOBSTORAGE_FILE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library biblo
 */
#ifdef NCBI_BIBLIO_EXPORTS
#  define NCBI_BIBLIO_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BIBLIO_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library biotree
 */
#ifdef NCBI_BIOTREE_EXPORTS
#  define NCBI_BIOTREE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BIOTREE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library blastdb
 */
#ifdef NCBI_BLASTDB_EXPORTS
#  define NCBI_BLASTDB_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BLASTDB_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library blastinput
 */
#ifdef NCBI_XALGO_BLASTINPUT_EXPORTS
#  define NCBI_BLASTINPUT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BLASTINPUT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library blastxml
 */
#ifdef NCBI_BLASTXML_EXPORTS
#  define NCBI_BLASTXML_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BLASTXML_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library blast
 */
#ifdef NCBI_BLAST_EXPORTS
#  define NCBI_BLAST_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BLAST_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library cdd
 */
#ifdef NCBI_BMAREFINE_EXPORTS
#  define NCBI_BMAREFINE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BMAREFINE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library cdd
 */
#ifdef NCBI_CDD_EXPORTS
#  define NCBI_CDD_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_CDD_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library cdd
 */
#ifdef NCBI_CDUTILS_EXPORTS
#  define NCBI_CDUTILS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_CDUTILS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library cn3d
 */
#ifdef NCBI_CN3D_EXPORTS
#  define NCBI_CN3D_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_CN3D_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi_driver
 */
#ifdef NCBI_DBAPIDRIVER_EXPORTS
#  define NCBI_DBAPIDRIVER_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPIDRIVER_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi_driver_ctlib
 */
#ifdef NCBI_DBAPIDRIVER_CTLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi_driver_dblib
 */
#ifdef NCBI_DBAPIDRIVER_DBLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi_driver_mysql
 */
#ifdef NCBI_DBAPIDRIVER_MYSQL_EXPORTS
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi_driver_odbc
 */
#ifdef NCBI_DBAPIDRIVER_ODBC_EXPORTS
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi_driver_ftds
 */
#ifdef NCBI_DBAPIDRIVER_FTDS_EXPORTS
#  define NCBI_DBAPIDRIVER_FTDS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPIDRIVER_FTDS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi
 */
#ifdef NCBI_DBAPI_EXPORTS
#  define NCBI_DBAPI_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPI_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi
 */
#ifdef NCBI_DBAPI_CACHE_EXPORTS
#  define NCBI_DBAPI_CACHE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPI_CACHE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library dbapi_util_blobstore
 */
#ifdef NCBI_DBAPIUTIL_BLOBSTORE_EXPORTS
#  define NCBI_DBAPIUTIL_BLOBSTORE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DBAPIUTIL_BLOBSTORE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library docsum
 */
#ifdef NCBI_DOCSUM_EXPORTS
#  define NCBI_DOCSUM_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DOCSUM_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library entrez2
 */
#ifdef NCBI_ENTREZ2_EXPORTS
#  define NCBI_ENTREZ2_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_ENTREZ2_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library entrezgene
 */
#ifdef NCBI_ENTREZGENE_EXPORTS
#  define NCBI_ENTREZGENE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_ENTREZGENE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library featdef
 */
#ifdef NCBI_FEATDEF_EXPORTS
#  define NCBI_FEATDEF_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_FEATDEF_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library flat
 */
#ifdef NCBI_FLAT_EXPORTS
#  define NCBI_FLAT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_FLAT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library format
 */
#ifdef NCBI_FORMAT_EXPORTS
#  define NCBI_FORMAT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_FORMAT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library gbseq
 */
#ifdef NCBI_GBSEQ_EXPORTS
#  define NCBI_GBSEQ_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_GBSEQ_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library general
 */
#ifdef NCBI_GENERAL_EXPORTS
#  define NCBI_GENERAL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_GENERAL_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library genesbyloc
 */
#ifdef NCBI_GENESBYLOC_EXPORTS
#  define NCBI_GENESBYLOC_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_GENESBYLOC_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library sgenome_collection
 */
#ifdef NCBI_GENOME_COLLECTION_EXPORTS
#  define NCBI_GENOME_COLLECTION_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_GENOME_COLLECTION_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library snputil
 */
#ifdef NCBI_SNPUTIL_EXPORTS
#  define NCBI_SNPUTIL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SNPUTIL_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library id1
 */
#ifdef NCBI_ID1_EXPORTS
#  define NCBI_ID1_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_ID1_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library id2
 */
#ifdef NCBI_ID2_EXPORTS
#  define NCBI_ID2_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_ID2_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library id2_split
 */
#ifdef NCBI_ID2_SPLIT_EXPORTS
#  define NCBI_ID2_SPLIT_EXPORT           NCBI_DLL_EXPORT
#else
#  define NCBI_ID2_SPLIT_EXPORT           NCBI_DLL_IMPORT
#endif

/* Export specifier for library insdseq
 */
#ifdef NCBI_INSDSEQ_EXPORTS
#  define NCBI_INSDSEQ_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_INSDSEQ_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library lds
 */
#ifdef NCBI_LDS_EXPORTS
#  define NCBI_LDS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_LDS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library lds v.2
 */
#ifdef NCBI_LDS2_EXPORTS
#  define NCBI_LDS2_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_LDS2_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library medlars
 */
#ifdef NCBI_MEDLARS_EXPORTS
#  define NCBI_MEDLARS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_MEDLARS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library medline
 */
#ifdef NCBI_MEDLINE_EXPORTS
#  define NCBI_MEDLINE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_MEDLINE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library mim
 */
#ifdef NCBI_MIM_EXPORTS
#  define NCBI_MIM_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_MIM_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library mla
 */
#ifdef NCBI_MLA_EXPORTS
#  define NCBI_MLA_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_MLA_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library mmdb1
 */
#ifdef NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB1_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_MMDB1_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library mmdb2
 */
#ifdef NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB2_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_MMDB2_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library mmdb3
 */
#ifdef NCBI_MMDB3_EXPORTS
#  define NCBI_MMDB3_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_MMDB3_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_mime
 */
#ifdef NCBI_NCBIMIME_EXPORTS
#  define NCBI_NCBIMIME_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_NCBIMIME_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library objcoords
 */
#ifdef NCBI_OBJCOORDS_EXPORTS
#  define NCBI_OBJCOORDS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_OBJCOORDS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library objprt
 */
#ifdef NCBI_OBJPRT_EXPORTS
#  define NCBI_OBJPRT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_OBJPRT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library omssa
 */
#ifdef NCBI_OMSSA_EXPORTS
#  define NCBI_OMSSA_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_OMSSA_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library pcassay
 */
#ifdef NCBI_PCASSAY_EXPORTS
#  define NCBI_PCASSAY_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_PCASSAY_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library pcsubstance
 */
#ifdef NCBI_PCSUBSTANCE_EXPORTS
#  define NCBI_PCSUBSTANCE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_PCSUBSTANCE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library pepxml
 */
#ifdef NCBI_PEPXML_EXPORTS
#  define NCBI_PEPXML_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_PEPXML_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library proj
 */
#ifdef NCBI_PROJ_EXPORTS
#  define NCBI_PROJ_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_PROJ_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library pubmed
 */
#ifdef NCBI_PUBMED_EXPORTS
#  define NCBI_PUBMED_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_PUBMED_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library pub
 */
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_PUB_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_PUB_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library remap
 */
#ifdef NCBI_REMAP_EXPORTS
#  define NCBI_REMAP_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_REMAP_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library scoremat
 */
#ifdef NCBI_SCOREMAT_EXPORTS
#  define NCBI_SCOREMAT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SCOREMAT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seqalign
 */
#ifdef NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQALIGN_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQALIGN_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seqblock
 */
#ifdef NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQBLOCK_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQBLOCK_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seqcode
 */
#ifdef NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQCODE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQCODE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seqfeat
 */
#ifdef NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQFEAT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQFEAT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seqloc
 */
#ifdef NCBI_SEQLOC_EXPORTS
#  define NCBI_SEQLOC_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQLOC_EXPORT NCBI_DLL_IMPORT
#endif

/*
 * Export specifier for library seqres
 */
#ifdef NCBI_SEQRES_EXPORTS
#  define NCBI_SEQRES_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQRES_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seqset
 */
#ifdef NCBI_SEQSET_EXPORTS
#  define NCBI_SEQSET_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQSET_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seqedit
 */
#ifdef NCBI_SEQEDIT_EXPORTS
#  define NCBI_SEQEDIT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQEDIT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seqtest
 */
#ifdef NCBI_SEQTEST_EXPORTS
#  define NCBI_SEQTEST_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQTEST_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library seq
 */
#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_SEQ_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SEQ_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library struct_dp
 */
#ifdef NCBI_STRUCTDP_EXPORTS
#  define NCBI_STRUCTDP_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_STRUCTDP_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library struct_util
 */
#ifdef NCBI_STRUCTUTIL_EXPORTS
#  define NCBI_STRUCTUTIL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_STRUCTUTIL_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library submit
 */
#ifdef NCBI_SUBMIT_EXPORTS
#  define NCBI_SUBMIT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SUBMIT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library taxon1
 */
#ifdef NCBI_TAXON1_EXPORTS
#  define NCBI_TAXON1_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_TAXON1_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library taxon3
 */
#ifdef NCBI_TAXON3_EXPORTS
#  define NCBI_TAXON3_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_TAXON3_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library variation
 */
#ifdef NCBI_VARIATION_EXPORTS
#  define NCBI_VARIATION_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_VARIATION_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library threader
 */
#ifdef NCBI_THREADER_EXPORTS
#  define NCBI_THREADER_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_THREADER_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library tinyseq
 */
#ifdef NCBI_TINYSEQ_EXPORTS
#  define NCBI_TINYSEQ_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_TINYSEQ_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library unimod
 */
#ifdef NCBI_UNIMOD_EXPORTS
#  define NCBI_UNIMOD_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_UNIMOD_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library validator
 */
#ifdef NCBI_VALIDATOR_EXPORTS
#  define NCBI_VALIDATOR_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_VALIDATOR_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library valerr
 */
#ifdef NCBI_VALERR_EXPORTS
#  define NCBI_VALERR_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_VALERR_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library discrepancy_report
 */
#ifdef NCBI_DISCREPANCY_REPORT_EXPORTS
#  define NCBI_DISCREPANCY_REPORT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_DISCREPANCY_REPORT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library cleanup
 */
#ifdef NCBI_CLEANUP_EXPORTS
#  define NCBI_CLEANUP_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_CLEANUP_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library unit_test_util
 */
#ifdef NCBI_UNIT_TEST_UTIL_EXPORTS
#  define NCBI_UNIT_TEST_UTIL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_UNIT_TEST_UTIL_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library uudutil
 */
#ifdef NCBI_UUDUTIL_EXPORTS
#  define NCBI_UUDUTIL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_UUDUTIL_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgoalign
 */
#ifdef NCBI_COBALT_EXPORTS
#  define NCBI_COBALT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_COBALT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgoalign
 */
#ifdef NCBI_XALGOALIGN_EXPORTS
#  define NCBI_XALGOALIGN_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOALIGN_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgocontig_assembly
 */
#ifdef NCBI_XALGOCONTIG_ASSEMBLY_EXPORTS
#  define NCBI_XALGOCONTIG_ASSEMBLY_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOCONTIG_ASSEMBLY_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgognomon
 */
#ifdef NCBI_XALGOGNOMON_EXPORTS
#  define NCBI_XALGOGNOMON_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOGNOMON_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgophytree
 */
#ifdef NCBI_XALGOPHYTREE_EXPORTS
#  define NCBI_XALGOPHYTREE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOPHYTREE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgoseq
 */
#ifdef NCBI_XALGOSEQ_EXPORTS
#  define NCBI_XALGOSEQ_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOSEQ_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgoseqqa
 */
#ifdef NCBI_XALGOSEQQA_EXPORTS
#  define NCBI_XALGOSEQQA_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOSEQQA_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgowinmask
 */
#ifdef NCBI_XALGOWINMASK_EXPORTS
#  define NCBI_XALGOWINMASK_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOWINMASK_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgodustmask
 */
#ifdef NCBI_XALGODUSTMASK_EXPORTS
#  define NCBI_XALGODUSTMASK_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGODUSTMASK_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalgosegmask
 */
#ifdef NCBI_XALGOSEGMASK_EXPORTS
#  define NCBI_XALGOSEGMASK_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOSEGMASK_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xid_mapper
 */
#ifdef NCBI_XALGOID_MAPPER_EXPORTS
#  define NCBI_XALGOID_MAPPER_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALGOID_MAPPER_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xalnmgr
 */
#ifdef NCBI_XALNMGR_EXPORTS
#  define NCBI_XALNMGR_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XALNMGR_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xblastformat
 */
#ifdef NCBI_XBLASTFORMAT_EXPORTS
#  define NCBI_XBLASTFORMAT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XBLASTFORMAT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library align_format
 */
#ifdef NCBI_ALIGN_FORMAT_EXPORTS
#  define NCBI_ALIGN_FORMAT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_ALIGN_FORMAT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library blastdb_format
 */
#ifdef NCBI_BLASTDB_FORMAT_EXPORTS
#  define NCBI_BLASTDB_FORMAT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BLASTDB_FORMAT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xcgi
 */
#if defined(NCBI_XCGI_EXPORTS) || defined(NCBI_XFCGI_EXPORTS)
#  define NCBI_XCGI_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XCGI_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xcgi_redirect
 */
#ifdef NCBI_XCGI_REDIRECT_EXPORTS
#  define NCBI_XCGI_REDIRECT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XCGI_REDIRECT_EXPORT NCBI_DLL_IMPORT
#endif

#if 0
/* Export specifier for library xgbplugin
 */
#ifdef NCBI_XGBPLUGIN_EXPORTS
#  define NCBI_XGBPLUGIN_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XGBPLUGIN_EXPORT NCBI_DLL_IMPORT
#endif
#endif

/* Export specifier for library xgridcgi
 */
#ifdef NCBI_XGRIDCGI_EXPORTS
#  define NCBI_XGRIDCGI_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XGRIDCGI_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xhtml
 */
#ifdef NCBI_XHTML_EXPORTS
#  define NCBI_XHTML_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XHTML_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ximage
 */
#ifdef NCBI_XIMAGE_EXPORTS
#  define NCBI_XIMAGE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XIMAGE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_blastdb
 */
#ifdef NCBI_XLOADER_BLASTDB_EXPORTS
#  define NCBI_XLOADER_BLASTDB_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_BLASTDB_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_remoteblast
 */
#ifdef NCBI_XLOADER_REMOTEBLAST_EXPORTS
#  define NCBI_XLOADER_REMOTEBLAST_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_REMOTEBLAST_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_cdd
 */
#ifdef NCBI_XLOADER_CDD_EXPORTS
#  define NCBI_XLOADER_CDD_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_CDD_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_genbank
 */
#ifdef NCBI_XLOADER_GENBANK_EXPORTS
#  define NCBI_XLOADER_GENBANK_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_GENBANK_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_lds
 */
#ifdef NCBI_XLOADER_LDS_EXPORTS
#  define NCBI_XLOADER_LDS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_LDS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_lds2
 */
#ifdef NCBI_XLOADER_LDS2_EXPORTS
#  define NCBI_XLOADER_LDS2_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_LDS2_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_table
 */
#ifdef NCBI_XLOADER_TABLE_EXPORTS
#  define NCBI_XLOADER_TABLE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_TABLE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_trace
 */
#ifdef NCBI_XLOADER_TRACE_EXPORTS
#  define NCBI_XLOADER_TRACE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_TRACE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_patcher
 */
#ifdef  NCBI_XLOADER_PATCHER_EXPORTS
#  define  NCBI_XLOADER_PATCHER_EXPORT NCBI_DLL_EXPORT
#else
#  define  NCBI_XLOADER_PATCHER_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library sraread
 */
#ifdef NCBI_SRAREAD_EXPORTS
#  define NCBI_SRAREAD_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_SRAREAD_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library bamread
 */
#ifdef NCBI_BAMREAD_EXPORTS
#  define NCBI_BAMREAD_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_BAMREAD_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_sra
 */
#ifdef NCBI_XLOADER_SRA_EXPORTS
#  define NCBI_XLOADER_SRA_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_SRA_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_csra
 */
#ifdef NCBI_XLOADER_CSRA_EXPORTS
#  define NCBI_XLOADER_CSRA_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_CSRA_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_wgs
 */
#ifdef NCBI_XLOADER_WGS_EXPORTS
#  define NCBI_XLOADER_WGS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_WGS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_vdbgraph
 */
#ifdef NCBI_XLOADER_VDBGRAPH_EXPORTS
#  define NCBI_XLOADER_VDBGRAPH_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_VDBGRAPH_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library ncbi_xloader_bam
 */
#ifdef NCBI_XLOADER_BAM_EXPORTS
#  define NCBI_XLOADER_BAM_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XLOADER_BAM_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xncbi
 */
#ifdef NCBI_XNCBI_EXPORTS
#  define NCBI_XNCBI_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XNCBI_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xobjedit
 */
#ifdef NCBI_XOBJEDIT_EXPORTS
#  define NCBI_XOBJEDIT_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XOBJEDIT_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xobjmanip
 */
#ifdef NCBI_XOBJMANIP_EXPORTS
#  define NCBI_XOBJMANIP_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XOBJMANIP_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xobjmgr
 */
#ifdef NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJMGR_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XOBJMGR_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xobjread
 */
#ifdef NCBI_XOBJREAD_EXPORTS
#  define NCBI_XOBJREAD_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XOBJREAD_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xobjsimple
 */
#ifdef NCBI_XOBJSIMPLE_EXPORTS
#  define NCBI_XOBJSIMPLE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XOBJSIMPLE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xobjutil
 */
#ifdef NCBI_XOBJUTIL_EXPORTS
#  define NCBI_XOBJUTIL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XOBJUTIL_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xobjwrite
 */
#ifdef NCBI_XOBJWRITE_EXPORTS
#  define NCBI_XOBJWRITE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XOBJWRITE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xomssa
 */
#ifdef NCBI_XOMSSA_EXPORTS
#  define NCBI_XOMSSA_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XOMSSA_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xprimer
 */
#ifdef NCBI_XPRIMER_EXPORTS
#  define NCBI_XPRIMER_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XPRIMER_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xreader
 */
#ifdef NCBI_XREADER_EXPORTS
#  define NCBI_XREADER_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XREADER_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xreader_id1
 */
#ifdef NCBI_XREADER_ID1_EXPORTS
#  define NCBI_XREADER_ID1_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XREADER_ID1_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xreader_id2
 */
#ifdef NCBI_XREADER_ID2_EXPORTS
#  define NCBI_XREADER_ID2_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XREADER_ID2_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xreader_cache
 */
#ifdef NCBI_XREADER_CACHE_EXPORTS
#  define NCBI_XREADER_CACHE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XREADER_CACHE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xreader_pubseqos
 */
#ifdef NCBI_XREADER_PUBSEQOS_EXPORTS
#  define NCBI_XREADER_PUBSEQOS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XREADER_PUBSEQOS_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xreader_pubseqos2
 */
#ifdef NCBI_XREADER_PUBSEQOS2_EXPORTS
#  define NCBI_XREADER_PUBSEQOS2_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XREADER_PUBSEQOS2_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xreader_gicache
 */
#ifdef NCBI_XREADER_GICACHE_EXPORTS
#  define NCBI_XREADER_GICACHE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XREADER_GICACHE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xregexp
 */
#ifdef NCBI_XREGEXP_EXPORTS
#  define NCBI_XREGEXP_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XREGEXP_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xserial
 */
#ifdef NCBI_XSERIAL_EXPORTS
#  define NCBI_XSERIAL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XSERIAL_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xsqlite
 */
#ifdef NCBI_XSQLITE_EXPORTS
#  define NCBI_XSQLITE_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XSQLITE_EXPORT NCBI_DLL_IMPORT
#endif

/* Export specifier for library xutil
 */
#ifdef NCBI_XUTIL_EXPORTS
#  define NCBI_XUTIL_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_XUTIL_EXPORT NCBI_DLL_IMPORT
#endif


/* Export specifier for library eutils
 */
#ifdef NCBI_EUTILS_EXPORTS
#  define NCBI_EUTILS_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_EUTILS_EXPORT NCBI_DLL_IMPORT
#endif



/* STATIC LIBRARIES SECTION */
/* This section is for static-only libraries */

#define NCBI_TEST_MT_EXPORT
#define NCBI_XALNUTIL_EXPORT
#define NCBI_XALNTOOL_EXPORT


#endif  /*  COMMON___NCBI_EXPORT__H  */


/* @} */
