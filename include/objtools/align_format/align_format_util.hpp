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
 */

#ifndef OBJTOOLS_ALIGN_FORMAT___ALIGN_FORMAT_UTIL_HPP
#define OBJTOOLS_ALIGN_FORMAT___ALIGN_FORMAT_UTIL_HPP

#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/align_format/format_flags.hpp>
#include <util/math/matrix.hpp>
#include <objtools/blast/gene_info_reader/gene_info_reader.hpp>
#include <algo/blast/core/blast_stat.h>
#include <objtools/align_format/ilinkoutdb.hpp>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strdup _strdup
#define snprintf _snprintf
#endif

/**setting up scope*/
BEGIN_NCBI_SCOPE

class CCgiContext;

BEGIN_SCOPE(align_format)


///blast related url

///class info
static const char kClassInfo[] = "class=\"info\"";

///entrez
// .ncbirc alias: ENTREZ
static const char kEntrezUrl[] = "<a title=\"Show report for <@acc@>\" <@cssInf@>href=\"//www.ncbi.nlm.nih.gov/<@db@>/<@gi@>?report=genbank&log$=<@log@>&blast_rank=<@blast_rank@>&RID=<@rid@>\" <@target@>>";

//.ncbirc alias: ENTREZ_TM
static const char kEntrezTMUrl[] = "//www.ncbi.nlm.nih.gov/<@db@>/<@gi@>?report=genbank&log$=<@log@>&blast_rank=<@blast_rank@>&RID=<@rid@>";

//.ncbirc alias: WGS
static const char kWGSUrl[] = "//www.ncbi.nlm.nih.gov/Traces/wgs/?val=<@wgsproj@>&page=1&display=contigs&search=<@wgsacc@>";


///trace db
//.ncbirc alias: TRACE
static const char kTraceUrl[] = "<a title=\"Show report for <@val@>\" <@cssInf@>href=\"//www.ncbi.nlm.nih.gov/Traces/trace.cgi?cmd=retrieve&dopt=fasta&val=<@val@>&RID=<@rid@>\">";

///genome button
//.ncbirc alias: GENOME_BTN
static const char kGenomeButton[] = "<table border=0 width=600 cellpadding=8>\
<tr valign=\"top\"><td><a href=\
\"//www.ncbi.nlm.nih.gov/mapview/map_search.cgi?taxid=%d&RID=%s&CLIENT=\
%s&QUERY_NUMBER=%d\"><img border=0 src=\"html/GenomeView.gif\"></a></td>\
<td>Show positions of the BLAST hits in the %s genome \
using the Entrez Genomes MapViewer</td></tr></table><p>";

///unigene
// .ncbirc alias: UNIGEN
static const char kUnigeneUrl[] = "<a href=\"//www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=<@db@>&cmd=Display&dopt=<@dopt@>_unigene&from_uid=<@gi@>&RID=<@rid@>&log$=unigene<@log@>&blast_rank=<@blast_rank@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";

//substitues <@lnk_displ@>
static const char kUnigeneImg[] = "<img border=0 height=16 width=16 src=\"images/U.gif\" alt=\"UniGene info linked to <@label@>\">";
//For text link <@lnk@> is substituted by formatted url
static const string kUnigeneDispl =  "<div><@lnk@>-<span class=\"rlLink\">clustered expressed sequence tags</span></div>";

///structure
// .ncbirc alias: STRUCTURE_URL
static const char kStructureUrl[] = "<a href=\"//www.ncbi.nlm.nih.gov/Structure/cblast/cblast.cgi?blast_RID=<@rid@>&blast_rep_gi=<@blast_rep_gi@>&hit=<@gi@>&<@cdd_params@>\
&blast_view=<@blast_view@>&hsp=0&taxname=<@taxname@>&client=blast&log$=structure<@log@>&blast_rank=<@blast_rank@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";
//substitues <@lnk_displ@>
static const char kStructureImg[] = "<img border=0 height=16 width=16 src=\"//www.ncbi.nlm.nih.gov/Structure/cblast/str_link.gif\" alt=\"Structure related to <@label@>\">";
//For text link <@lnk@> is substituted by formatted url
static const string kStructureDispl =  "<div><@lnk@>-<span class=\"rlLink\">3D structure displays</span></div>";

///structure overview
static const char kStructure_Overview[] = "<a href=\"//www.ncbi.nlm.nih.\
gov/Structure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&%s\
&blast_view=%s&hsp=0&taxname=%s&client=blast\">Related Structures</a>";


///Geo
// .ncbirc alias: GEO
static const char kGeoUrl[] =  "<a href=\"//www.ncbi.nlm.nih.gov/geoprofiles/?LinkName=nucleotide_geoprofiles&from_uid=<@gi@>&RID=<@rid@>&log$=geo<@log@>&blast_rank=<@blast_rank@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";


//substitues <@lnk_displ@>
static const char kGeoImg[] = "<img border=0 height=16 width=16 src=\"images/E.gif\" alt=\"GEO profiles info linked to <@label@>\">";
//For text link <@lnk@> is substituted by formatted url
static const string kGeoDispl =  "<div><@lnk@>-<span class=\"rlLink\">microarray expression data</span></div>";

///Gene
// .ncbirc alias: GENE
static const char kGeneUrl[] = "<a href=\"//www.ncbi.nlm.nih.gov/gene?term=<@gi@>[<@uid@>]&RID=<@rid@>&log$=gene<@log@>&blast_rank=<@blast_rank@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";

//substitues <@lnk_displ@>
static const char kGeneImg[] = "<img border=0 height=16 width=16 src=\"images/G.gif\" alt=\"Gene info linked to <@label@>\">";
//For text link <@lnk@> is substituted by formatted url
static const string kGeneDispl =  "<div><@lnk@>-<span class=\"rlLink\">associated gene details</span></div>";

///Bioassay for proteins
// .ncbirc alias: BIOASSAY_PROT
static const char kBioAssayProtURL[] = "<a href=\"//www.ncbi.nlm.nih.gov/entrez?db=pcassay&term=<@gi@>[PigGI]&RID=<@rid@>&log$=pcassay<@log@>&blast_rank=<@blast_rank@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";
//substitues <@lnk_displ@>
static const char kBioAssayProtImg[] = "<img border=0 height=16 width=16 src=\"images/Bioassay.gif\" alt=\"PubChem BioAssay Info linked to <@label@>\">";

///Bioassay for nucleotides
// .ncbirc alias: BIOASSAY_NUC
static const char kBioAssayNucURL[] = "<a href=\"//www.ncbi.nlm.nih.gov/entrez?db=pcassay&term=<@gi@>[RNATargetGI]&RID=<@rid@>&log$=pcassay<@log@>&blast_rank=<@blast_rank@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";
static const char kBioAssayNucImg[] = "<img border=0 height=16 width=16 src=\"images/Bioassay.gif\" alt=\"PubChem BioAssay Info linked to <@label@>\">";

//For text link <@lnk@> is substituted by formatted url for both BioAssay Nuc and Prot
static const string kBioAssayDispl =  "<div><@lnk@>-<span class=\"rlLink\">bioactivity screening</span></div>";

///mapviewer linkout
// .ncbirc alias: MAPVIEWER
static const char kMapviwerUrl[] = "<a href=\"//www.ncbi.nlm.nih.gov/mapview/map_search.cgi?direct=on&gbgi=<@gi@>&THE_BLAST_RID=<@rid@>&log$=map<@log@>&blast_rank=<@blast_rank@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";
//substitues <@lnk_displ@>
static const char kMapviwerImg[] = "<img border=0 height=16 width=16 src=\"images/M.gif\" alt=\"Genome view with mapviewer linked to <@label@>\">";
//For text link <@lnk@> is substituted by formatted url
static const string kMapviwerDispl =  "<div><@lnk@>-<span class=\"rlLink\">aligned genomic context</span></div>";

///mapviewer linkout
//for used for NT/NW/NC
static const string kMapviewBlastHitUrl = "//www.ncbi.nlm.nih.gov/mapview/maps.cgi?maps=blast_set";
static const string kMapviewBlastHitParams = "<a href=\"<@user_url@>&db=<@db@>&na=<@is_na@>&gnl=<@gnl@>&gi=<@gi@>&term=<@gi@>[gi]&taxid=<@taxid@>&RID=<@rid@>&QUERY_NUMBER=<@query_number@>&log$=nucl<@log@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";


///Repr microbial Genome linkout
// .ncbirc alias: REPR_MICROBIAL_GENOMES
static const char kReprMicrobialGenomesUrl[] = "<a href=\"//www.ncbi.nlm.nih.gov/genome?term=<@gi@>[gi]&RID=<@rid@>&log$=map<@log@>&blast_rank=<@blast_rank@>\"<@lnkTitle@><@lnkTarget@>><@lnk_displ@></a>";
//substitues <@lnk_displ@>
static const char kReprMicrobialGenomesImg[] = "<img border=0 height=16 width=16 src=\"images/L.gif\" alt=\"View genome information for <@label@>\">";
//For text link <@lnk@> is substituted by formatted url
static const string kReprMicrobialGenomesDispl =  "<div><@lnk@>-<span class=\"rlLink\">Genomic Sequence</span></div>";

static const char kIdenticalProteinsUrl[] = "<a href=\"//www.ncbi.nlm.nih.gov/protein/<@gi@>?report=ipg\" title=\"View proteins identical to <@label@>\" <@lnkTarget@>><@lnk_displ@></a>";
static const string kIdenticalProteinsDispl =  "<div><@lnk@>-<span class=\"rlLink\">Identical proteins to <@label@></span></div>";


///dumpgnl
static const char kDownloadUrl[] = "/blast/dumpgnl.cgi";
static const char kDownloadLink[] = "<a href=\"<@download_url@>&segs=<@segs@>\"><@lnk_displ@></a>";
//substitues <@lnk_displ@>
static const char kDownloadImg[] = "<img border=0 height=16 width=16 src=\"images/D.gif\" alt=\"Download subject sequence <@label@> spanning the HSP\">";

static const char kSeqViewerUrl[] = "//www.ncbi.nlm.nih.gov/<@dbtype@>/<@gi@>?report=graph&rid=<@rid@>[<@gi@>]&<@seqViewerParams@>&v=<@from@>:<@to@>&appname=ncbiblast&link_loc=<@link_loc@>";
static const string kSeqViewerParams = "tracks=[key:sequence_track,name:Sequence,display_name:Sequence,id:STD1,category:Sequence,annots:Sequence,ShowLabel:true][key:gene_model_track,CDSProductFeats:false][key:alignment_track,name:other alignments,annots:NG Alignments|Refseq Alignments|Gnomon Alignments|Unnamed,shown:false]";

static const char kSeqViewerUrlNonGi[] = "//www.ncbi.nlm.nih.gov/projects/sviewer/?RID=<@rid@>&id=<@firstSeqID@>&<@seqViewerParams@>&v=<@from@>:<@to@>&appname=ncbiblast&link_loc=<@link_loc@>";

//to test ranges use:
//static const char kSeqViewerUrl[] = "//www.ncbi.nlm.nih.gov/<@dbtype@>/<@gi@>?report=graph&rid=<@rid@>&tracks=[key:gene_model_track],[key:alignment_track]&v=<@from@>:<@to@>,<@fromTest@>:<@toTest@>&flip=<@flip@>";

static const char kCustomLinkTemplate[] = "<a href=\"<@custom_url@>\" class=\"<@custom_cls@>\" target=\"<@custom_trg@>\" title=\"<@custom_title@>\"><@custom_lnk_displ@></a>"; 
static const char kCustomLinkTitle[]="Show <@custom_report_type@> report for <@seqid@>";

static const char kGenericLinkTemplate[] = "<a title=\"Show report for <@seqid@>\" href=\"<@url@>\" ><@seqid@></a>"; 
static const char kGenericLinkMouseoverTmpl[] = "<span class=\"jig-ncbipopper\" data-jigconfig=\"destText:'<@defline@>'\"><a title=\"Show report for <@seqid@>\" onclick=\"window.open(this.href,'<@target@>')\" href=\"<@url@>\" ><@seqid@></a></span>";


///Sub-sequence
// .ncbirc alias: ENTREZ_SUBSEQ

static const char kEntrezSubseqUrl[] = "<a href=\"//www.ncbi.nlm.nih.gov/<@db@>/<@gi@>?report=gbwithparts&from=<@from@>&to=<@to@>&RID=<@rid@>\">";

// .ncbirc alias: ENTREZ_SUBSEQ_TM
static const char kEntrezSubseqTMUrl[] = "//www.ncbi.nlm.nih.gov/<@db@>/<@gi@>?report=gbwithparts&from=<@from@>&to=<@to@>&RID=<@rid@>";

///Default linkout order 
//.ncbirc alias: LINKOUT_ORDER
static const char kLinkoutOrderStr[] = "G,U,E,S,B,R,M"; 

///Bl2seq ************fix this and test titles Irena
// .ncbirc alias: BL2SEQ
//static const char kBl2seqUrl[] = "<a href=\"blast.ncbi.nlm.nih.gov/Blast.cgi?QUERY=<@query@>&SUBJECTS=<@subject@>&EXPECT=10&SHOW_OVERVIEW=on&OLD_BLAST=false&NEW_VIEW=on\">Get TBLASTX alignments</a>";
static const char kBl2seqUrl[] = "<a href=\"blast.ncbi.nlm.nih.gov/Blast.cgi?QUERY=<@query@>&SUBJECTS=<@subject@>&PROGRAM=tblastx&EXPECT=10&CMD=request&SHOW_OVERVIEW=on&OLD_BLAST=false&NEW_VIEW=on\">Get TBLASTX alignments</a>";



// .ncbirc alias: GETSEQ_SUB_FRM_0
// .ncbirc alias: GETSEQ_SUB_FRM_1
static const char k_GetSeqSubmitForm_0[] = "<FORM  method=\"post\" \
action=\"//www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?SUBMIT=y\" \
name=\"%s%d\"><input type=button value=\"Get selected sequences\" \
onClick=\"finalSubmit(%d, 'getSeqAlignment%d', 'getSeqGi', '%s%d', %d)\"><input \
type=\"hidden\" name=\"db\" value=\"\"><input type=\"hidden\" name=\"term\" \
value=\"\"><input type=\"hidden\" name=\"doptcmdl\" value=\"docsum\"><input \
type=\"hidden\" name=\"cmd\" value=\"search\"></form>";
static const char k_GetSeqSubmitForm_1[] = "<FORM  method=\"POST\" \
action=\"//www.ncbi.nlm.nih.gov/Traces/trace.cgi\" \
name=\"%s%d\"><input type=button value=\"Get selected sequences\" \
onClick=\"finalSubmit(%d, 'getSeqAlignment%d', 'getSeqGi', '%s%d', %d)\"><input \
type=\"hidden\" name=\"val\" value=\"\"><input \
type=\"hidden\" name=\"cmd\" value=\"retrieve\"></form>";

// .ncbirc alias: GETSEQ_SEL_FRM
// 'getSeqAlignment%d', 'getSeqGi')\"></form>";
static const char k_GetSeqSelectForm[] = "<FORM><input \
type=\"button\" value=\"Select all\" onClick=\"handleCheckAll('select', \
'getSeqAlignment%d', 'getSeqGi')\"></form></td><td><FORM><input \
type=\"button\" value=\"Deselect all\" onClick=\"handleCheckAll('deselect', \
'getSeqAlignment%d', 'getSeqGi')\"></form>";

// .ncbirc alias: TREEVIEW_FRM
static const char k_GetTreeViewForm[] =  "<FORM  method=\"post\" \
action=\"//www.ncbi.nlm.nih.gov/blast/treeview/blast_tree_view.cgi?request=page&rid=%s&queryID=%s&distmode=on\" \
name=\"tree%s%d\" target=\"trv%s\"> \
<input type=button value=\"Distance tree of results\" onClick=\"extractCheckedSeq('getSeqAlignment%d', 'getSeqGi', 'tree%s%d')\"> \
<input type=\"hidden\" name=\"sequenceSet\" value=\"\"><input type=\"hidden\" name=\"screenWidth\" value=\"\"></form>";

// .ncbirc alias: GENE_INFO
static const char kGeneInfoUrl[] =
"//www.ncbi.nlm.nih.gov/sites/entrez?db=gene&cmd=search&term=%d&RID=%s&log$=geneexplicit%s&blast_rank=%d";

// .ncbirc alias: TREEVIEW_CGI
static const char kGetTreeViewCgi[] = "//www.ncbi.nlm.nih.gov/blast/treeview/blast_tree_view.cgi";
// .ncbirc alias: ENTREZ_QUERY_CGI
static const char kEntrezQueryCgi[] = "//www.ncbi.nlm.nih.gov/entrez/query.fcgi";
// .ncbirc alias: TRACE_CGI
static const char kTraceCgi[] = "//www.ncbi.nlm.nih.gov/Traces/trace.cgi";
// .ncbirc alias:  MAP_SEARCH_CGI
static const char kMapSearchCgi[] = "//www.ncbi.nlm.nih.gov/mapview/map_search.cgi";
// .ncbirc alias: CBLAST_CGI
static const char kCBlastCgi[] = "//www.ncbi.nlm.nih.gov/Structure/cblast/cblast.cgi";
// .ncbirc alias: ENTREZ_VIEWER_CGI
static const char kEntrezViewerCgi[] = "//www.ncbi.nlm.nih.gov/entrez/viewer.fcgi";
// .ncbirc alias: BL2SEQ_WBLAST_CGI
static const char kBl2SeqWBlastCgi[] = "//www.ncbi.nlm.nih.gov/blast/bl2seq/wblast2.cgi";
// .ncbirc alias: ENTREZ_SITES_CGI
static const char kEntrezSitesCgi[] = "//www.ncbi.nlm.nih.gov/sites/entrez";


/// create map source of all static URL's using previously defined pairs
/// this map should be in alphabetical order!!!
typedef SStaticPair<const char*, const char*> TTagUrl;
static const TTagUrl s_TagUrls [] = {
  { "BIOASSAY_NUC",  kBioAssayNucURL },
  { "BIOASSAY_PROT",  kBioAssayProtURL },
  { "BL2SEQ",  kBl2seqUrl },
  { "BL2SEQ_WBLAST_CGI",  kBl2SeqWBlastCgi },
  { "CBLAST_CGI",  kCBlastCgi },
  { "ENTREZ",  kEntrezUrl  },
  { "ENTREZ_QUERY_CGI",  kEntrezQueryCgi },
  { "ENTREZ_SITES_CGI",  kEntrezSitesCgi },
  { "ENTREZ_SUBSEQ",  kEntrezSubseqUrl },
  { "ENTREZ_SUBSEQ_TM",  kEntrezSubseqTMUrl },  
  { "ENTREZ_TM",  kEntrezTMUrl },  
  { "ENTREZ_VIEWER_CGI",  kEntrezViewerCgi },
  { "GENE",  kGeneUrl },
  { "GENE_INFO",  kGeneInfoUrl },
  { "GENOME_BTN",  kGenomeButton },
  { "GEO",    kGeoUrl },
  { "GETSEQ_SEL_FRM",  k_GetSeqSelectForm },
  { "GETSEQ_SUB_FRM_0",  k_GetSeqSubmitForm_0 },
  { "GETSEQ_SUB_FRM_1",  k_GetSeqSubmitForm_1 },
  { "MAPVIEWER",  kMapviwerUrl },
  { "MAP_SEARCH_CGI",  kMapSearchCgi },
  { "REPR_MICROBIAL_GENOMES",  kReprMicrobialGenomesUrl},  
  { "STRUCTURE_OVW",  kStructure_Overview },
  { "STRUCTURE_URL",  kStructureUrl },
  { "TRACE",  kTraceUrl },
  { "TRACE_CGI",  kTraceCgi },
  { "TREEVIEW_CGI",  kGetTreeViewCgi },
  { "TREEVIEW_FRM",  k_GetTreeViewForm },
  { "UNIGEN",  kUnigeneUrl },    
  { "WGS",  kWGSUrl },    
};

#ifndef NCBI_SWIG
typedef CStaticArrayMap<string, string> TTagUrlMap;
DEFINE_STATIC_ARRAY_MAP(TTagUrlMap, sm_TagUrlMap, s_TagUrls);
#endif


#ifndef DIM
/// Calculates the dimensions of a static array
#define DIM(static_array) (sizeof(static_array)/sizeof(*static_array))
#endif

///protein matrix define
enum {
    ePMatrixSize = 23       // number of amino acid for matrix
};

/// Number of ASCII characters for populating matrix columns
const int k_NumAsciiChar = 128;

/// Residues
NCBI_ALIGN_FORMAT_EXPORT
extern const char k_PSymbol[];

/** This class contains misc functions for displaying BLAST results. */

class NCBI_ALIGN_FORMAT_EXPORT CAlignFormatUtil 
{
public:
   
    /// The string containing the message that no hits were found
    static const char kNoHitsFound[];

    ///Error info structure
    struct SBlastError {
        EDiagSev level;   
        string message;  
    };

    ///Blast database info
    struct SDbInfo {
        bool   is_protein;
        string name;
        string definition;
        string date;
        Int8   total_length;
        int    number_seqs;
        bool   subset;    
        /// Filtering algorithm ID used in BLAST search
        string filt_algorithm_name;
        /// Filtering algorithm options used in BLAST search
        string filt_algorithm_options;

        /// Default constructor
        SDbInfo() {
            is_protein = true;
            name = definition = date = "Unknown";
            total_length = 0;
            number_seqs = 0;
            subset = false;
        }
    };

    ///Structure that holds information needed for creation seqID URL in descriptions
    /// and alignments
    struct SSeqURLInfo { 
        string user_url;        ///< user url TOOL_URL from .ncbirc
        string blastType;       ///< blast type refer to blobj->adm->trace->created_by
        bool isDbNa;            ///< bool indicating if the database is nucleotide or not
        string database;        ///< name of the database
        string rid;             ///< blast RID
        int queryNumber;        ///< the query number
        TGi gi;                 ///< gi to use
        string accession;       ///< accession
        int linkout;            ///< linkout flag
        int blast_rank;         ///< index of the current alignment
        bool isAlignLink;       ///< bool indicating if link is in alignment section
        bool new_win;           ///< bool indicating if click of the url will open a new window
        CRange<TSeqPos> seqRange;///< sequence range
        bool flip;              ///< flip sequence in case of opposite strands
        int taxid;              ///< taxid
        bool addCssInfo;        ///< bool indicating that css info should be added
        string segs;            ///< string containing align segments in the the following format seg1Start-seg1End,seg2Start-seg2End
        string resourcesUrl;    ///< URL(s) to other resources from .ncbirc
        bool useTemplates;      ///< bool indicating that templates should be used when contsructing links
        bool advancedView;      ///< bool indicating that advanced view design option should be used when contsructing links
        string seqUrl;          ///< sequence URL created
        string defline;         ///< sequence defline
        
        
        /// Constructor        
        SSeqURLInfo(string usurl,string bt, bool isnuc,string db, string rid,int qn, 
                    TGi gi,  string acc, int lnk, int blrk,bool alnLink, bool nw, CRange<TSeqPos> range = CRange<TSeqPos>(0,0),bool flp = false, int txid = -1,bool addCssInf = false,string seqSegs = "",string resUrl = "",bool useTmpl = false, bool advView = false) 
                    : user_url(usurl),blastType(bt), isDbNa(isnuc), database(db),rid(rid), 
                    queryNumber(qn), gi(gi), accession(acc), linkout(lnk),blast_rank(blrk),isAlignLink(alnLink),
                    new_win(nw),seqRange(range),flip(flp),taxid (txid),addCssInfo(addCssInf),segs(seqSegs),
                    resourcesUrl(resUrl),useTemplates(useTmpl),advancedView(advView){}

    };
    
    ///Structure that holds information for all hits of one subject in Seq Align Set    
    struct SSeqAlignSetCalcParams {        
        //values used in descriptions display
        double evalue;                  //lowest evalue in Seq Align Set , displayed on the results page as 'Evalue', 
        double bit_score;               //Highest bit_score in Seq Align Set, displayed on the results page as 'Max Score'
        double total_bit_score;         //total bit_score for Seq Align Set, displayed on the results page as 'Total Score'
        int percent_coverage;           //percent coverage for Seq Align Set, displayed on the results page as 'Query coverage'
                                        //calulated as 100*master_covered_length/queryLength
        int percent_identity;           //highest percent identity in Seq Align Set, displayed on the results page as 'Max ident'
                                        //calulated as 100*match/align_length

        int hspNum;                     //hsp number, number of hits
        Int8 totalLen;                  //total alignment length

        int raw_score;                  //raw score, read from the 'score' in first align in Seq Aln Set, not used        
        list<TGi> use_this_gi;          //Limit formatting by these GI's, read from the first align in Seq Aln Set        
        int sum_n;                      //sum_n in score block , read from the first align in Seq Aln Set        

        int master_covered_length;      //total query length covered by alignment - calculated, used calculate percent_coverage

        int match;                      //number of matches in the alignment with highest percent identity,used to calulate percent_identity
        int align_length;               //length of the alignment with highest percent identity,used to calulate percent_identity
        
        CConstRef<objects::CSeq_id> id; //subject seq id               
        CRange<TSeqPos> subjRange;      //total subject sequence range- calculated
        bool flip;					    //indicates opposite strands in the first seq align	   
    };  
      
    enum DbSortOrder {
        eNonGenomicFirst = 1,
        eGenomicFirst
    };

    enum HitOrder {
        eEvalue = 0,
        eHighestScore,
        eTotalScore,
        ePercentIdentity,
        eQueryCoverage
    };

    enum HspOrder {
        eHspEvalue = 0,
        eScore,
        eQueryStart,
        eHspPercentIdentity,
        eSubjectStart
    };

    enum CustomLinkType {
        eLinkTypeDefault = 0,
        eLinkTypeMapViewer = (1 << 0),
        eLinkTypeSeqViewer = (1 << 1),
        eDownLoadSeq = (1 << 2),
        eLinkTypeGenLinks = (1 << 3),
        eLinkTypeTraceLinks = (1 << 4),
        eLinkTypeSRALinks = (1 << 5),
        eLinkTypeSNPLinks = (1 << 6),
        eLinkTypeGSFastaLinks = (1 << 7)
    };

    ///db type
    enum DbType {
        eDbGi = 0,
        eDbGeneral,
        eDbTypeNotSet
    };

    ///Output blast errors
    ///@param error_return: list of errors to report
    ///@param error_post: post to stderr or not
    ///@param out: stream to ouput
    ///
    static void BlastPrintError(list<SBlastError>& error_return, 
                                bool error_post, CNcbiOstream& out);

    ///Print out misc information separated by "~"
    ///@param str:  input information
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///
    static void PrintTildeSepLines(string str, size_t line_len, 
                                   CNcbiOstream& out);

    /// Retrieve BLAST database information for presentation in BLAST report
    /// @param dbname space-separated list of BLAST database names [in]
    /// @param is_protein are these databases protein? [in]
    /// @param dbfilt_algorithm BLAST database filtering algorithm ID (if
    /// applicable), use -1 if not applicable [in]
    /// @param is_remote is this a remote BLAST search? [in]
    static void GetBlastDbInfo(vector<SDbInfo>& retval,
                               const string& blastdb_names, bool is_protein,
                               int dbfilt_algorithm,
                               bool is_remote = false);

    ///Print out blast database information
    ///@param dbinfo_list: database info list
    ///@param line_length: length of each line desired
    ///@param out: stream to ouput
    ///@param top Is this top or bottom part of the BLAST report?
    static void PrintDbReport(const vector<SDbInfo>& dbinfo_list, 
                              size_t line_length, 
                              CNcbiOstream& out, 
                              bool top=false);
    
    ///Print out kappa, lamda blast parameters
    ///@param lambda
    ///@param k
    ///@param h
    ///@param line_len length of each line desired
    ///@param out stream to ouput
    ///@param gapped gapped alignment?
    ///@param gbp Gumbel parameters
    static void PrintKAParameters(double lambda, double k, double h,
                                  size_t line_len, CNcbiOstream& out, 
                                  bool gapped, const Blast_GumbelBlk *gbp=NULL);

    /// Returns a full '|'-delimited Seq-id string for a Bioseq.
    /// @param cbs Bioseq object [in]
    /// @param believe_local_id Should local ids be parsed? [in]
    static string 
    GetSeqIdString(const objects::CBioseq& cbs, bool believe_local_id=true);
    
    /// Returns a full '|'-delimited Seq-id string for a  a list of seq-ids.
    /// @param ids lsit of seq-ids [in]
    /// @param believe_local_id Should local ids be parsed? [in]
    static string
    GetSeqIdString(const list<CRef<objects::CSeq_id> > & ids, bool believe_local_id);

    /// Returns a full description for a Bioseq, concatenating all available 
    /// titles.
    /// @param cbs Bioseq object [in]
    static string GetSeqDescrString(const objects::CBioseq& cbs);

    ///Print out blast query info
    /// @param cbs bioseq of interest
    /// @param line_len length of each line desired
    /// @param out stream to ouput
    /// @param believe_query use user id or not
    /// @param html in html format or not [in]
    /// @param tabular Is this done for tabular formatting? [in]
    /// @param rid the RID to acknowledge (if not empty) [in]
    ///
    static void AcknowledgeBlastQuery(const objects::CBioseq& cbs, size_t line_len,
                                      CNcbiOstream& out, bool believe_query,
                                      bool html, bool tabular=false,
                                      const string& rid = kEmptyStr);

    /// Print out blast subject info
    /// @param cbs bioseq of interest
    /// @param line_len length of each line desired
    /// @param out stream to ouput
    /// @param believe_query use user id or not
    /// @param html in html format or not [in]
    /// @param tabular Is this done for tabular formatting? [in]
    ///
    static void AcknowledgeBlastSubject(const objects::CBioseq& cbs, size_t line_len,
                                        CNcbiOstream& out, bool believe_query,
                                        bool html, bool tabular=false);

    /// Retrieve a scoring matrix for the provided matrix name
    /// @return the requested matrix (indexed using ASCII characters) or an empty
    /// matrix if matrix_name is invalid or can't be found.
    static void GetAsciiProteinMatrix(const char* matrix_name,
                                 CNcbiMatrix<int>& retval);
private:
    static void x_AcknowledgeBlastSequence(const objects::CBioseq& cbs, 
                                           size_t line_len,
                                           CNcbiOstream& out,
                                           bool believe_query,
                                           bool html, 
                                           const string& label,
                                           bool tabular /* = false */,
                                           const string& rid /* = kEmptyStr*/);
public:

    /// Prints out PHI-BLAST info for header (or footer)
    /// @param num_patterns number of times pattern appears in query [in]
    /// @param pattern the pattern used [in]
    /// @param prob probability of pattern [in]
    /// @param offsets vector of pattern offsets in query [in]
    /// @param out stream to ouput [in]
    static void PrintPhiInfo(int num_patterns, const string& pattern,
                                    double prob,
                                    vector<int>& offsets,
                                    CNcbiOstream& out);

    ///Extract score info from blast alingment
    ///@param aln: alignment to extract score info from
    ///@param score: place to extract the raw score to
    ///@param bits: place to extract the bit score to
    ///@param evalue: place to extract the e value to
    ///@param sum_n: place to extract the sum_n to
    ///@param num_ident: place to extract the num_ident to
    ///@param use_this_gi: place to extract use_this_gi to
    ///
    static void GetAlnScores(const objects::CSeq_align& aln,
                             int& score, 
                             double& bits, 
                             double& evalue,
                             int& sum_n,
                             int& num_ident,
                             list<TGi>& use_this_gi);
    
    ///Extract score info from blast alingment
    /// Second version that fetches compositional adjustment integer
    ///@param aln: alignment to extract score info from
    ///@param score: place to extract the raw score to
    ///@param bits: place to extract the bit score to
    ///@param evalue: place to extract the e value to
    ///@param sum_n: place to extract the sum_n to
    ///@param num_ident: place to extract the num_ident to
    ///@param use_this_gi: place to extract use_this_gi to
    ///@param comp_adj_method: composition based statistics method [out]
    ///
    static void GetAlnScores(const objects::CSeq_align& aln,
                             int& score, 
                             double& bits, 
                             double& evalue,
                             int& sum_n,
                             int& num_ident,
                             list<TGi>& use_this_gi,
                             int& comp_adj_method);

    
    ///Add the specified white space
    ///@param out: ostream to add white space
    ///@param number: the number of white spaces desired
    ///
    static void AddSpace(CNcbiOstream& out, int number);

    ///Return ID for GNL label
    ///@param dtg: dbtag to build label from
    static string GetGnlID(const objects::CDbtag& dtg);

    ///Return a label for an ID
    /// Tries to recreate behavior of GetLabel before a change that 
    /// prepends "ti|" to trace IDs
    ///@param id CSeqId: to build label from
    static string GetLabel(CConstRef<objects::CSeq_id> id);
    
    ///format evalue and bit_score 
    ///@param evalue: e value
    ///@param bit_score: bit score
    ///@param total_bit_score: total bit score(??)
    ///@param raw_score: raw score (e.g., BLOSUM score)
    ///@param evalue_str: variable to store the formatted evalue
    ///@param bit_score_str: variable to store the formatted bit score
    ///@param raw_score_str: variable to store the formatted raw score
    ///
    static void GetScoreString(double evalue, 
                               double bit_score, 
                               double total_bit_score, 
                               int raw_score,
                               string& evalue_str, 
                               string& bit_score_str,
                               string& total_bit_score_str,
                               string& raw_score_str);
    
    ///Fill new alignset containing the specified number of alignments with
    ///unique slave seqids.  Note no new seqaligns were created. It just 
    ///references the original seqalign
    ///@param source_aln: the original alnset
    ///@param new_aln: the new alnset
    ///@param num: the specified number
    ///
    static void PruneSeqalign(const objects::CSeq_align_set& source_aln, 
                              objects::CSeq_align_set& new_aln,
                              unsigned int num = kDfltArgNumAlignments);

    ///Fill new alignset containing the specified number of alignments 
    ///plus the rest of alignments for the last subget seq
    ///unique slave seqids.  Note no new seqaligns were created. It just 
    ///references the original seqalign
    ///
    ///@param source_aln: the original alnset
    ///@param new_aln: the new alnset
    ///@param num: the specified number
    ///
    static void PruneSeqalignAll(const objects::CSeq_align_set& source_aln, 
                                     objects::CSeq_align_set& new_aln,
                                     unsigned int number);

    /// Count alignment length, number of gap openings and total number of gaps
    /// in a single alignment.
    /// @param salv Object representing one alignment (HSP) [in]
    /// @param align_length Total length of this alignment [out]
    /// @param num_gaps Total number of insertions and deletions in this 
    ///                 alignment [out]
    /// @param num_gap_opens Number of gap segments in the alignment [out]
    static void GetAlignLengths(objects::CAlnVec& salv, int& align_length, 
                                int& num_gaps, int& num_gap_opens);

    /// If a Seq-align-set contains Seq-aligns with discontinuous type segments, 
    /// extract the underlying Seq-aligns and put them all in a flat 
    /// Seq-align-set.
    /// @param source Original Seq-align-set
    /// @param target Resulting Seq-align-set
    static void ExtractSeqalignSetFromDiscSegs(objects::CSeq_align_set& target,
                                               const objects::CSeq_align_set& source);

    ///Create denseseg representation for densediag seqalign
    ///@param aln: the input densediag seqalign
    ///@return: the new denseseg seqalign
    static CRef<objects::CSeq_align> CreateDensegFromDendiag(const objects::CSeq_align& aln);

    ///return the tax id for a seqid
    ///@param id: seq id
    ///@param scope: scope to fetch this sequence
    ///
    static int GetTaxidForSeqid(const objects::CSeq_id& id, objects::CScope& scope);
    
    ///return the frame for a given strand
    ///Note that start is zero bases.  It returns frame +/-(1-3).
    ///0 indicates error
    ///@param start: sequence start position
    ///@param strand: strand
    ///@param id: the seqid
    ///@return: the frame
    ///
    static int GetFrame (int start, objects::ENa_strand strand, const objects::CBioseq_Handle& handle); 

    ///return the comparison result: 1st >= 2nd => true, false otherwise
    ///@param info1
    ///@param info2
    ///@return: the result
    ///
    static bool SortHitByTotalScoreDescending(CRef<objects::CSeq_align_set> const& info1,
                                    CRef<objects::CSeq_align_set> const& info2);

    static bool 
    SortHitByMasterCoverageDescending(CRef<objects::CSeq_align_set> const& info1,
                                     CRef<objects::CSeq_align_set> const& info2);
    

    ///group hsp's with the same id togeter
    ///@param target: the result list
    ///@param source: the source list
    ///
    static void HspListToHitList(list< CRef<objects::CSeq_align_set> >& target,
                                 const objects::CSeq_align_set& source); 

    ///extract all nested hsp's into a list
    ///@param source: the source list
    ///@return the list of hsp's
    ///
    static CRef<objects::CSeq_align_set> HitListToHspList(list< CRef<objects::CSeq_align_set> >& source);

    ///extract seq_align_set coreesponding to seqid list
    ///@param all_aln_set: CSeq_align_set source/target list
    ///@param alignSeqList: string of seqIds separated by comma
    ///
    static void ExtractSeqAlignForSeqList(CRef<objects::CSeq_align_set> &all_aln_set, string alignSeqList);

    ///return the custom url (such as mapview)
    ///@param ids: the id list
    ///@param taxid
    ///@param user_url: the custom url
    ///@param database
    ///@param db_is_na:  is db nucleotide?
    ///@param rid: blast rid
    ///@param query_number: the blast query number.
    ///@param for_alignment: is the URL generated for an alignment or a top defline?
    ///
    static string BuildUserUrl(const objects::CBioseq::TId& ids, int taxid, string user_url,
                               string database, bool db_is_na, string rid,
                               int query_number, bool for_alignment);

    ///return the SRA (Short Read Archive) URL
    ///@param ids: the id list
    ///@param user_url: the URL of SRA cgi
    ///@return newly constructed SRA URL pointing to the identified spot
    ///
    static string BuildSRAUrl(const objects::CBioseq::TId& ids, string user_url);
    
 
    ///calculate the percent identity for a seqalign
    ///@param aln" the seqalign
    ///@param scope: scope to fetch sequences
    ///@do_translation: is this a translated nuc to nuc alignment?
    ///@return: the identity 
    ///
    static double GetPercentIdentity(const objects::CSeq_align& aln, objects::CScope& scope,
                                     bool do_translation);

    ///get the alignment length
    ///@param aln" the seqalign
    ///@do_translation: is this a translated nuc to nuc alignment?
    ///@return: the alignment length
    ///
    static int GetAlignmentLength(const objects::CSeq_align& aln, bool do_translation);

    ///sort a list of seqalign set by alignment identity
    ///@param seqalign_hit_list: list to be sorted.
    ///@param do_translation: is this a translated nuc to nuc alignment?
    ///
    static void SortHitByPercentIdentityDescending(list< CRef<objects::CSeq_align_set> >&
                                                   seqalign_hit_list,
                                                   bool do_translation);

    ///sorting function for sorting a list of seqalign set by descending identity
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHitByPercentIdentityDescendingEx
        (const CRef<objects::CSeq_align_set>& info1,
         const CRef<objects::CSeq_align_set>& info2);
    
    ///sorting function for sorting a list of seqalign by descending identity
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHspByPercentIdentityDescending 
    (const CRef<objects::CSeq_align>& info1,
     const CRef<objects::CSeq_align>& info2);
    
    ///sorting function for sorting a list of seqalign by ascending mater 
    ///start position
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHspByMasterStartAscending(const CRef<objects::CSeq_align>& info1,
                                              const CRef<objects::CSeq_align>& info2);

    static bool SortHspBySubjectStartAscending(const CRef<objects::CSeq_align>& info1,
                                               const CRef<objects::CSeq_align>& info2);

    static bool SortHitByScoreDescending
    (const CRef<objects::CSeq_align_set>& info1,
     const CRef<objects::CSeq_align_set>& info2);
    

    static bool SortHspByScoreDescending(const CRef<objects::CSeq_align>& info1,
                                         const CRef<objects::CSeq_align>& info2);

    ///sorting function for sorting a list of seqalign set by ascending mater 
    ///start position
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHitByMasterStartAscending(CRef<objects::CSeq_align_set>& info1,
                                              CRef<objects::CSeq_align_set>& info2);

    ///sort a list of seqalign set by molecular type
    ///@param seqalign_hit_list: list to be sorted.
    ///@param scope: scope to fetch sequence
    ///
    static void 
    SortHitByMolecularType(list< CRef<objects::CSeq_align_set> >& seqalign_hit_list,
                           objects::CScope& scope, ILinkoutDB* linkoutdb,
                           const string& mv_build_name);
    
    ///actual sorting function for SortHitByMolecularType
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    //static bool SortHitByMolecularTypeEx (const CRef<objects::CSeq_align_set>& info1,
    //                                      const CRef<objects::CSeq_align_set>& info2);

    static void 
    SortHit(list< CRef<objects::CSeq_align_set> >& seqalign_hit_list,
            bool do_translation, objects::CScope& scope, int sort_method,
            ILinkoutDB* linkoutdb, const string& mv_build_name);
    
    static void SplitSeqalignByMolecularType(vector< CRef<objects::CSeq_align_set> >& 
                                             target,
                                             int sort_method,
                                             const objects::CSeq_align_set& source,
                                             objects::CScope& scope,
                                             ILinkoutDB* linkoutdb,
                                             const string& mv_build_name);
    static CRef<objects::CSeq_align_set> 
    SortSeqalignForSortableFormat(CCgiContext& ctx,
                               objects::CScope& scope,
                               objects::CSeq_align_set& aln_set,
                               bool nuc_to_nuc_translation,
                               int db_order,
                               int hit_order,
                               int hsp_order,
                               ILinkoutDB* linkoutdb,
                               const string& mv_build_name);

	/// function for calculating  percent match for an alignment.	
	///@param numerator
	/// int numerator in percent identity calculation.
	///@param denominator
	/// int denominator in percent identity calculation.
	static int GetPercentMatch(int numerator, int denominator);

    ///function for Filtering seqalign by expect value
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param evalueLow 
    /// double min expect value
    ///@param evalueHigh 
    /// double max expect value
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
    static CRef<objects::CSeq_align_set> FilterSeqalignByEval(objects::CSeq_align_set& source_aln,                                      
                                     double evalueLow,
                                     double evalueHigh);

	///function for Filtering seqalign by percent identity
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param percentIdentLow
    /// double min percent identity
    ///@param percentIdentHigh 
    /// double max percent identity
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
    static CRef<objects::CSeq_align_set> FilterSeqalignByPercentIdent(objects::CSeq_align_set& source_aln,
                                                                      double percentIdentLow,
                                                                      double percentIdentHigh);

    ///function for Filtering seqalign by expect value and percent identity
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param evalueLow 
    /// double min expect value
    ///@param evalueHigh 
    /// double max expect value
    ///@param percentIdentLow
    /// double min percent identity
    ///@param percentIdentHigh 
    /// double max percent identity
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
	static CRef<objects::CSeq_align_set> FilterSeqalignByScoreParams(objects::CSeq_align_set& source_aln,
	                                                                 double evalueLow,
	                                                                 double evalueHigh,
	                                                                 double percentIdentLow,
	                                                                 double percentIdentHigh);
    ///function for Limitting seqalign by hsps number
    ///(by default results are not cut off within the query)
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param maxAligns 
    /// double max number of alignments (per query)
    ///@param maxHsps 
    /// double max number of Hsps (for all qeuries)    
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
    static CRef<objects::CSeq_align_set> LimitSeqalignByHsps(objects::CSeq_align_set& source_aln,
                                                    int maxAligns,
                                                    int maxHsps); 

    ///function for extracting seqalign for the query
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param queryNumber 
    /// int query number ,starts from 1, 0 means return all queries    
    ///@return
    /// CRef<CSeq_align_set> - seq align set for queryNumber, if invalid queryNumber return empty  CSeq_align_set
    static CRef<objects::CSeq_align_set> ExtractQuerySeqAlign(CRef<objects::CSeq_align_set>& source_aln,
                                                     int queryNumber);
    
    static void BuildFormatQueryString (CCgiContext& ctx, 
                                       string& cgi_query);

    static void BuildFormatQueryString (CCgiContext& ctx, 
                                        map< string, string>& parameters_to_change,
                                        string& cgi_query);

    static bool IsMixedDatabase(const objects::CSeq_align_set& alnset, 
                                objects::CScope& scope, ILinkoutDB* linkoutdb,
                                const string& mv_build_name); 
    static bool IsMixedDatabase(CCgiContext& ctx);

    
    ///Get the list of urls for linkouts
    ///@param linkout: the membership value
    ///@param ids: CBioseq::TId object    
    ///@param rid: RID
    ///@param cdd_rid: CDD RID
    ///@param entrez_term: entrez query term
    ///@param is_na: is this sequence nucleotide or not
    ///@param first_gi: first gi in the list (used to contsruct structure url)
    ///@param structure_linkout_as_group: bool used to contsruct structure url
    ///@param for_alignment: bool indicating if link is located in alignment section
    ///@param int cur_align: int current alignment/description number
    ///@param bool textLink: bool indicating that if true link will be presented as text, otherwise as image
    ///@return list of string containing all linkout urls for one seq 
    static list<string> GetLinkoutUrl(int linkout, 
                                      const objects::CBioseq::TId& ids, 
                                      const string& rid, 
                                      const string& cdd_rid, 
                                      const string& entrez_term,
                                      bool is_na, 
                                      TGi first_gi,
                                      bool structure_linkout_as_group,
                                      bool for_alignment, 
                                      int cur_align,
                                      string preComputedResID);
                                      
    
    ///Create map that holds all linkouts for the list of blast deflines and corresponding seqIDs
    ///@param bdl: list of CRef<CBlast_def_line>
    ///@param linkout_map: map that holds linkouts and corresponding CBioseq::TId for the whole list  of blast deflines  
    ///
    static void GetBdlLinkoutInfo(const list< CRef< objects::CBlast_def_line > > &bdl,
                                  map<int, vector < objects::CBioseq::TId > > &linkout_map,
                                  ILinkoutDB* linkoutdb,
                                  const string& mv_build_name);
    ///Get linkout membership for for the list of blast deflines
    ///@param bdl: list of CRef<CBlast_def_line>    
    ///@param rid: blast rid
    ///@param cdd_rid: blast cdd_rid
    ///@param entrez_term: entrez_term for building url
    ///@param is_na: bool indication if query is nucleotide
    ///@param first_gi: first gi in the list (used to contsruct structure url)
    ///@param structure_linkout_as_group: bool used to contsruct structure url
    ///@param for_alignment: bool indicating tif link is locted in alignment section
    ///@param int cur_align: int current alignment/description number
    ///@param linkoutOrder: string of letters separated by comma specifing linkout order like "G,U,M,E,S,B"
    ///@param taxid: int taxid
    ///@param database: database name
    ///@param query_number: query_number
    ///@param user_url: url defined as TOOL_URL for blast_type in .ncbirc
    ///@return list of string containing all linkout urls for all of the seqs in the list of blast deflines
    ///
    static list<string> GetFullLinkoutUrl(const list< CRef< objects::CBlast_def_line > > &bdl,                                             
                                                 const string& rid,
                                                 const string& cdd_rid, 
                                                 const string& entrez_term,
                                                 bool is_na,                                                            
                                                 bool structure_linkout_as_group,
                                                 bool for_alignment, 
                                                 int cur_align,
                                                 string& linkoutOrder,
                                                 int taxid,
                                                 string &database,
                                                 int query_number,                                                 
                                                 string &user_url,
                                                 string &preComputedResID,
                                                 ILinkoutDB* linkoutdb,
                                                 const string& mv_build_name);
                                   
    static int GetMasterCoverage(const objects::CSeq_align_set& alnset);
	static CRange<TSeqPos> GetSeqAlignCoverageParams(const objects::CSeq_align_set& alnset,int *masterCoverage,bool *flip);
												

    ///retrieve URL from .ncbirc file combining host/port and format strings values.
    ///consult blastfmtutil.hpp
    ///@param url_name:  url name to retrieve
    ///@param index:   name index ( default: -1: )
    ///@return: URL format string from .ncbirc file or default as kNAME
    ///
    static string GetURLFromRegistry( const string url_name, int index = -1);

    ////get default value if there is problem with .ncbirc file or
    ////settings are not complete. return corresponding static value
    ///@param url_name: constant name to return .
    ///@param index:   name index ( default: -1: )
    ///@return: URL format string defined in blastfmtutil.hpp 
    static string GetURLDefault( const string url_name, int index = -1);

    ///release memory allocated for the registry object by GetURLFromRegistry
    ///
    static void ReleaseURLRegistry(void);

    ///Replace template tags by real data
    ///@param inpString: string containing template data
    ///@param tmplParamName:string with template tag name
    ///@param templParamVal: int value that replaces template
    ///@return:string containing template data replaced by real data
    ///
    ///<@tmplParamName@> is replaced by templParamVal
    static string MapTemplate(string inpString,string tmplParamName,int templParamVal);

    ///Replace template tags by real data
    ///@param inpString: string containing template data
    ///@param tmplParamName:string with template tag name
    ///@param templParamVal: string value that replaces template
    ///@return:string containing template data replaced by real data
    ///
    ///<@tmplParamName@> is replaced by templParamVal
    static string MapTemplate(string inpString,string tmplParamName,string templParamVal);
    
    ///Create URL for seqid
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: seqid CSeq_id
    ///@param scopeRef:scope to fetch sequence    
    static string GetIDUrl(SSeqURLInfo *seqUrlInfo,
                           const objects::CSeq_id& id,
                           objects::CScope &scope);
                           

    ///Create URL for seqid 
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param ids: CBioseq::TId object        
    static string GetIDUrl(SSeqURLInfo *seqUrlInfo,
                            const objects::CBioseq::TId* ids);                            

    static string GetFullIDLink(SSeqURLInfo *seqUrlInfo,
                            const objects::CBioseq::TId* ids);                            

    ///Create URL for seqid that goes to entrez or trace
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: seqid CSeq_id
    ///@param scopeRef:scope to fetch sequence    
    static string GetIDUrlGen(SSeqURLInfo *seqUrlInfo,
                              const objects::CSeq_id& id,
                              objects::CScope &scope);
                              

    ///Create URL for seqid that goes to entrez or trace
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param ids: CBioseq::TId object        
    static string GetIDUrlGen(SSeqURLInfo *seqUrlInfo,const objects::CBioseq::TId* ids);

    ///Create info indicating what kind of links to display
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction    
    ///@param customLinkTypesInp: original types of links to be included in the list    
    ///@return: int containing customLinkTypes with the bits set to indicate what kind of links to display for the sequence
    ///
    ///examples:(Mapviewer,Download,GenBank,FASTA,Seqviewer, Trace, SRA, SNP, GSFASTA)
    static int SetCustomLinksTypes(SSeqURLInfo *seqUrlInfo, int customLinkTypesInp);

    ///Create the list of string links for seqid that go 
    /// - to GenBank,FASTA and Seqviewer for gi > 0 
    /// - customized links determined by seqUrlInfo->blastType for gi = 0
    /// - customized links determined by customLinkTypes    
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: CSeq_id object    
    ///@param scope: scope to fetch this sequence
    ///@param customLinkTypes: types of links to be included in the list(mapviewer,seqviewer or download etc)    
    ///@param customLinksList: list of strings containing links
    static list <string>  GetCustomLinksList(SSeqURLInfo *seqUrlInfo,
                                   const objects::CSeq_id& id,
                                   objects::CScope &scope,                                             
                                   int customLinkTypes = eLinkTypeDefault);

    static list<string>  GetGiLinksList(SSeqURLInfo *seqUrlInfo,bool hspRange = false);
    static string  GetGraphiscLink(SSeqURLInfo *seqUrlInfo,bool hspRange = false);
    static list<string>  GetSeqLinksList(SSeqURLInfo *seqUrlInfo,bool hspRange = false);
    
    


    ///Create URL showing aligned regions info
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: CSeq_id object    
    ///@param scope: scope to fetch this sequence
    ///@return: string containing URL
    ///
    static string  GetFASTALinkURL(SSeqURLInfo *seqUrlInfo,
                                   const objects::CSeq_id& id,
                                   objects::CScope &scope);

    ///Create URL to FASTA info
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: CSeq_id object    
    ///@param scope: scope to fetch this sequence
    ///@return: string containing URL
    ///
    static string GetAlignedRegionsURL(SSeqURLInfo *seqUrlInfo,
                                const objects::CSeq_id& id,
                                objects::CScope &scope);

    ///Set the database as gi type
    ///@param actual_aln_list: the alignment
    ///@param scope: scope to fetch sequences
    ///
    static CAlignFormatUtil::DbType GetDbType(const objects::CSeq_align_set& actual_aln_list, 
                                              objects::CScope & scope);
                                          
    static CAlignFormatUtil::SSeqAlignSetCalcParams* GetSeqAlignCalcParams(const objects::CSeq_align& aln);

    static CAlignFormatUtil::SSeqAlignSetCalcParams* GetSeqAlignSetCalcParams(const objects::CSeq_align_set& aln,int queryLength,bool do_translation);

    static CAlignFormatUtil::SSeqAlignSetCalcParams* GetSeqAlignSetCalcParamsFromASN(const objects::CSeq_align_set& alnSet);

    static map < string, CRef<objects::CSeq_align_set>  >  HspListToHitMap(vector <string> seqIdList, const objects::CSeq_align_set& source);

    ///Scan the the list of blast deflines and find seqID to be use in display    
    ///@param handle: CBioseq_Handle [in]
    ///@param aln_id: CSeq_id object for alignment seq [in]
    ///@param use_this_gi: list<int> list of gis to use [in]
    ///@param gi: gi to be used for display if exists or 0    
    ///@return: CSeq_id object to be used for display
    static CRef<objects::CSeq_id> GetDisplayIds(const objects::CBioseq_Handle& handle,
                                                const objects::CSeq_id& aln_id,
                                                list<TGi>& use_this_gi,
                                                TGi& gi);

    ///Check if accession is WGS
    ///@param accession: string accession [in]
    ///@param wgsProj: string  wgsProj [out]      
    ///@return: bool indicating if accession is WGS
    static bool IsWGSAccession(string &accession, string &wgsProj);

    ///Check if accession is WGS
    ///@param accession: string accession [in]    
    ///@return: bool indicating if accession is WGS
    static bool IsWGSPattern(string &wgsAccession);

    ///Get Gene symobol for gi
    ///@param  giForGeneLookup: gi
    ///@return: string gene symbol
    static string  GetGeneInfo(TGi giForGeneLookup);
    static CNcbiRegistry *m_Reg;
    static bool   m_geturl_debug_flag;
    static auto_ptr<CGeneInfoFileReader> m_GeneInfoReader;

    /// Calculate the uniq subject query coverage range (blastn only)
    static int GetUniqSeqCoverage(objects::CSeq_align_set & alnset);

protected:

    ///Wrap a string to specified length.  If break happens to be in
    /// a word, it will extend the line length until the end of the word
    ///@param str: input string
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///@param html Is this HTML output? [in]
    static void x_WrapOutputLine(string str, size_t line_len, 
                                 CNcbiOstream& out,
                                 bool html = false);
};

END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___ALIGN_FORMAT_UTIL_HPP */
