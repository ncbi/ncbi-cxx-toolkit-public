/* fta_parser.h
 *
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
 * File Name:  fta_parser.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen, Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------
 */

#ifndef  __FLATFILE_PARSE_INFO__
#define  __FLATFILE_PARSE_INFO__

#include <list>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE

// some forward declarations
struct FileBuf {
    const char* start=nullptr;
    const char* current=nullptr;
};
struct indexblk_struct;
struct protein_block;
struct _fta_operon;


typedef struct indexblk_struct* IndexblkPtr;
typedef struct protein_block* ProtBlkPtr;
typedef struct _fta_operon* FTAOperonPtr;

using TEntryList = list<CRef<objects::CSeq_entry>>;

struct Parser {

    enum class EOutput {
        BioseqSet,
        Seqsubmit
    };

    enum class EMode {
        Release,
        HTGS,
        HTGSCON,
        Relaxed
    };

    enum class ESource {
        unknown,
        NCBI,
        EMBL,
        GenBank,
        DDBJ,
        LANL,
        PIR,
        SPROT,
        PRF,
        Refseq,
        Flybase,
        USPTO,
        All
    };

    enum class EFormat {
        unknown,
        EMBL,
        GenBank,
        PIR,
        SPROT,
        DDBJ,
        PRF,
        XML,
        ALL 
    };


    Int4 indx=0;                          /* total number of records in the
                                           flat file, exclude BadLocusName entries */
    IndexblkPtr* entrylist=nullptr;     /* a pointer points to the index block */
    Int4 curindx=0;                     /* current index of the entrylist */

    /* all the files will be produced in the directory where the program was
     * executed except the input file which located in the argument path
     */
    FILE*            ifp=nullptr;               /* a file pointer for input */
    FileBuf       ffbuf;             

    string      release_str;
    string      authors_str;

    TEntryList       entries;

    /* next 4 + 3 variables record data from command arguments
     */
    Int4 limit=0;                     /* limit to sequence length.
                                         As of June, 2004 sequence length
                                         limitation removed. This variable
                                         will be always 0 */
    EFormat format=EFormat::unknown;  /* flat file format */
    ESource source=ESource::unknown;  /* source of flat file */
    bool    all=false;                /* any source of flat file */
    Uint1   seqtype=0;                /* sequence type based on source
                                         of flat file */
    Int4 num_drop=0;                  /* number of entries with foregn
                                         acc# (dropped) */
    const char *acprefix=nullptr;     /* decide the drop value, s.t.
                                         checking the prefix character of
                                         the accession number, an option
                                         user provided from the command
                                         line argument */
    Uint1 entrez_fetch=0;             /* PUBSEQBioseqFetchEnable()
                                         0 - do not need this connection;
                                         1 - need it and got it;
                                         2 - need it and failed, will
                                         reconnect */
    Uint1 taxserver=0;                /* if != 0, call TaxArchInit() */
    ProtBlkPtr pbp=nullptr;           /* for processing nucleic acid
                                         protein sequence */
    Uint1 medserver=0;                /* == 1, if MedArchInit() call
                                         succeeded */

    struct SFindPubOptions {
        bool    always_look=true;       /* if TRUE, look up even if muid in
                                           Pub-equiv */
        bool    replace_cit=true;       /* if TRUE, replace Cit-art w/ replace
                                           from MEDLINE */
        int lookups_attempted;           /* citartmatch tries */
        int lookups_succeeded;           /* citartmatch worked */
        int  fetches_attempted;          /* FetchPubs tried */
        int  fetches_succeeded;          /* FetchPubs that worked */
        bool merge_ids = true;           /* If TRUE then merges Cit-art.ids from
                                            input Cit-sub and one gotten from
                                            med server. */
    };


    SFindPubOptions  fpo;         /* for medline uid lookup */
    bool date=false;              /* if TRUE, replace update date
                                     from LOCUS */
    bool no_date=false;           /* if TRUE, if no update and curr
                                     date come out */
    bool citat=false;             /* if TRUE, removes serial-numbers */
    bool transl=false;            /* if TRUE program replaces translation */
    bool sort=false;              /* if TRUE, program doesn't sort entries */
    bool debug=false;             /* output everthing */
    bool segment=false;           /* treat the input file as segment in embl format */
    bool no_code=false;           /* no genetic code from server try to guess */
    bool seg_acc=false;           /* use accession for segmented set Id */
    bool convert=false;           /* convert to new asn.1 spec (ver. 4.0) */
    char** accpref=nullptr;       /* a list of allowable 2-letter
                                     prefixes in new format of accession
                                    numbers 2 letters + 6 digits */
    bool accver=false;            /* ACCESSION.VERSION */
    bool histacc=false;           /* Populate Seq-inst.hist.replaces with secondaries */
    bool ign_toks=false;          /* Ignore multiple tokens in DDBJ's VERSION line. Default = FALSE */
    bool ign_prot_src=false;      /* If set to TRUE, then does not reject record if protein accession
                                     prefix does not fit sequence owner */
    bool ign_bad_qs=false;        /* If TRUE, then does not reject the record with bad quality score */
    EMode mode=EMode::Release;    /* Known so far: RELEASE and HTGS. For now only difference between
                                     severity of error messages. */
    bool diff_lt=false;           /* If TRUE, then will allow to have same genes with different
                                     locus_tags. Default is FALSE. */
    Int4 errstat=0;               /* Just a temporary storage */
    bool allow_uwsec=false;       /* Allows unusual secondary WGS accessions with prefixes not
                                     matching the primary one */
    FTAOperonPtr operon=nullptr;
    bool xml_comp=false;          /* INSDSeq/GenBank/EMBL compatible */
    bool sp_dt_seq_ver=true;      /* For SwissProt "Reviewed" records
                                     only: puts the sequence version
                                     number from "sequence version" DT
                                     line into Seq-id.version slot */
    bool simple_genes=false;      /* If set to TRUE, then will always
                                     merge join locations to the single
                                     ones while generating genes */
    Int4 cleanup=0;               /* pick the required cleanup function:
                                     0 - legacy parser version of SSEC;
                                     1 - SSEC;
                                     2 - none.
                                     Default is 0. */
    bool allow_crossdb_featloc=false;
    bool genenull=false;
    const char* qsfile=nullptr;   /* Do not free, just a pointer */


    FILE* qsfd=nullptr;
    bool  qamode=false;
    char* buf=nullptr;         /* Temporary storage for locations checks */
    EOutput output_format=EOutput::BioseqSet; /* Bioseq-set or Seq-submit */

    // buffer based parsing
    bool ffdb=false;              /* Use FlatFile database */
    bool farseq=false;
    void* user_data=nullptr;
    char*(*ff_get_entry)(const char* accession)=nullptr;
    char*(*ff_get_entry_v)(const char* accession, Int2 vernum)=nullptr;
    char*(*ff_get_qscore)(const char* accession, Int2 v)=nullptr;
    char*(*ff_get_qscore_pp)(const char* accession, Int2 v, Parser *pp)=nullptr;
    char*(*ff_get_entry_pp)(const char* accession, Parser *pp)=nullptr;
    char*(*ff_get_entry_v_pp)(const char* accession, Int2 vernum, Parser *pp)=nullptr;

    virtual ~Parser();
};

using ParserPtr = Parser*;

/**************************************************************************/
void fta_init_pp(Parser& pp);

END_NCBI_SCOPE

#endif
