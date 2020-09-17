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

#ifndef  FTA_PARSER
#define  FTA_PARSER

#include <list>

#include <objects/seqset/Seq_entry.hpp>

#define FTA_OUTPUT_BIOSEQSET 0
#define FTA_OUTPUT_SEQSUBMIT 1

#define FTA_RELEASE_MODE     0
#define FTA_HTGS_MODE        1
#define FTA_HTGSCON_MODE     2

BEGIN_NCBI_SCOPE

// some forward declarations
struct file_buf;
struct indexblk_struct;
struct protein_block;
struct _fta_operon;

typedef struct file_buf* FileBufPtr;
typedef struct indexblk_struct* IndexblkPtr;
typedef struct protein_block* ProtBlkPtr;
typedef struct _fta_operon* FTAOperonPtr;

typedef std::list<CRef<objects::CSeq_entry> > TEntryList;

//typedef struct parser_vals {
struct Parser {
    Int4             indx;              /* total number of records in the
                                           flat file, exclude BadLocusName
                                           entries */
    IndexblkPtr* entrylist;         /* a pointer points to the index
                                           block */
    Int4             curindx;           /* current index of the entrylist */

    /* all the files will be produced in the directory where the program was
     * executed except the input file which located in the argument path
     */
    FILE*        ifp;               /* a file pointer for input */
    FileBufPtr       ffbuf;             /* a buffer pointer for input */

    std::string      outfile;
    std::string      release_str;
    std::string      authors_str;

    bool             output_binary;
    TEntryList       entries;

    /* next 4 + 3 variables record data from command arguments
     */
    Int4             limit;             /* limit to sequence length.
                                           As of June, 2004 sequence length
                                           limitation removed. This variable
                                           will be always 0 */
    Int2             format;            /* flat file format */
    Int2             source;            /* source of flat file */
    Int2             all;               /* any source of flat file */
    Uint1            seqtype;           /* sequence type based on source
                                           of flat file */
    Int4             num_drop;          /* number of entries with foregn
                                           acc# (dropped) */
    const char       *acprefix;         /* decide the drop value, s.t.
                                           checking the prefix character of
                                           the accession number, an option
                                           user provided from the command
                                           line argument */
    Uint1            entrez_fetch;      /* PUBSEQBioseqFetchEnable()
                                           0 - do not need this connection;
                                           1 - need it and got it;
                                           2 - need it and failed, will
                                               reconnect */
    Uint1            taxserver;         /* if != 0, call TaxArchInit() */
    ProtBlkPtr       pbp;               /* for processing nucleic acid
                                           protein sequence */
    Uint1            medserver;         /* == 1, if MedArchInit() call
                                           succeeded */
    void*          fpo;               /* for medline uid lookup */
    bool             date;              /* if TRUE, replace update date
                                           from LOCUS */
    bool             no_date;           /* if TRUE, if no update and curr
                                           date come out */
    bool             citat;             /* if TRUE, removes serial-numbers */
    bool             transl;            /* if TRUE program replaces
                                           translation */
    bool             sort;              /* if TRUE, program doesn't sort
                                           entries */
    bool             debug;             /* output everthing */
    bool             segment;           /* treat the input file as segment
                                           in embl format */
    bool             no_code;           /* no genetic code from server try
                                           to guess */
    bool             seg_acc;           /* use accession for segmented
                                           set Id */
    bool             convert;           /* convert to new asn.1 spec
                                           (ver. 4.0) */
    char**     accpref;           /* a list of allowable 2-letter
                                           prefixes in new format of accession
                                           numbers 2 letters + 6 digits */
    bool             accver;            /* ACCESSION.VERSION */
    bool             histacc;           /* Populate Seq-inst.hist.replaces
                                           with secondaries */
    bool             ign_toks;          /* Ignore multiple tokens in DDBJ's
                                           VERSION line. Default = FALSE */
    bool             ign_prot_src;      /* If set to TRUE, then does not
                                           reject record if protein accession
                                           prefix does not fit sequence
                                           owner */
    bool             ign_bad_qs;        /* If TRUE, then does not reject the
                                           record with bad quality score */
    Int4             mode;              /* Known so far: RELEASE and HTGS.
                                           For now only difference between
                                           severity of error messages. */
    bool             diff_lt;           /* If TRUE, then will allow to have
                                           same genes with different
                                           locus_tags. Default is FALSE. */
    Int4             errstat;           /* Just a temporary storage */
    bool             allow_uwsec;       /* Allows unusual secondary WGS
                                           accessions with prefixes not
                                           matching the primary one */
    FTAOperonPtr     operon;
    bool             xml_comp;          /* INSDSeq/GenBank/EMBL compatible */
    bool             sp_dt_seq_ver;     /* For SwissProt "Reviewed" records
                                           only: puts the sequence version
                                           number from "sequence version" DT
                                           line into Seq-id.version slot */
    bool             simple_genes;      /* If set to TRUE, then will always
                                           merge join locations to the single
                                           ones while generating genes */
    Int4             cleanup;           /* pick the required cleanup function:
                                           0 - legacy parser version of SSEC;
                                           1 - SSEC;
                                           2 - none.
                                           Default is 0. */
    bool             allow_crossdb_featloc;
    bool             genenull;
    const char*      qsfile;            /* Do not free, just a pointer */
    FILE*        qsfd;
    bool             qamode;
    char*          buf;               /* Temporary storage for
                                           locations checks */
    Int4             output_format;     /* 0 = Bioseq-set, 1 = Seq-submit */

    // buffer based parsing
    bool             ffdb;              /* Use FlatFile database */
    bool             farseq;
    void*          user_data;
    char*(*ff_get_entry)(const char* accession);
    char*(*ff_get_entry_v)(const char* accession, Int2 vernum);
    char*(*ff_get_qscore)(const char* accession, Int2 v);
    char*(*ff_get_qscore_pp)(const char* accession, Int2 v, Parser *pp);
    char*(*ff_get_entry_pp)(const char* accession, Parser *pp);
    char*(*ff_get_entry_v_pp)(const char* accession, Int2 vernum, Parser *pp);


    Parser() :
        indx(0),
        entrylist(NULL),
        curindx(0),
        ifp(NULL),
        ffbuf(NULL),
        output_binary(false),
        limit(0),
        format(0),
        source(0),
        all(0),
        seqtype(0),
        num_drop(0),
        acprefix(NULL),
        entrez_fetch(0),
        taxserver(0),
        pbp(NULL),
        medserver(0),
        fpo(NULL),
        date(false),
        no_date(false),
        citat(false),
        transl(false),
        sort(false),
        debug(false),
        segment(false),
        no_code(false),
        seg_acc(false),
        convert(false),
        accpref(NULL),
        accver(false),
        histacc(false),
        ign_toks(false),
        ign_prot_src(false),
        ign_bad_qs(false),
        mode(0),
        diff_lt(false),
        errstat(0),
        allow_uwsec(false),
        operon(NULL),
        xml_comp(false),
        sp_dt_seq_ver(true),
        simple_genes(false),
        cleanup(0),
        allow_crossdb_featloc(false),
        genenull(false),
        qsfile(NULL),
        qsfd(NULL),
        qamode(false),
        buf(NULL),
        output_format(0),
        ffdb(false),
        farseq(false),
        user_data(NULL),
        ff_get_entry(NULL),
        ff_get_entry_v(NULL),
        ff_get_qscore(NULL),
        ff_get_qscore_pp(NULL),
        ff_get_entry_pp(NULL),
        ff_get_entry_v_pp(NULL)
    {}

    virtual ~Parser();
};

using ParserPtr = Parser*;

/**************************************************************************/
void fta_init_pp(Parser& pp);

END_NCBI_SCOPE

#endif
