# BLAST+ Usage Data Collection & Privacy

Help NCBI prioritize software features and improvements by opting in to usage data collection from BLAST+ command-line searches. Make sure that your favorite BLAST features are supported. If you choose to opt in, BLAST will collect a small amount of data about your searches to guide future development. Your participation is vital to making BLAST better for everyone.

Participation is optional. Starting with BLAST+ version 2.18.0, usage data collection is *off by default*.

This page explains why we ask, what is collected, and how to enable and disable usage data collection whenever you like. This document applies to BLAST+ searches that do not use the `-remote` command line option. Remote searches are covered separately below.

The short version: your query and database sequence data as well as search results are not collected.

## Remote searches follow a different privacy policy

When BLAST+ applications are invoked with the `-remote` command line option, the search is submitted to, executed at NCBI, and search results are transmitted back to your computer. Your query sequences and search parameters must be transmitted to NCBI so that the search can be performed. For remote searches, usage data collection is governed by the Privacy Policy in [NCBI Website and Data Usage Policies and Disclaimers](https://www.ncbi.nlm.nih.gov/home/about/policies/).

## Why is data being collected?

This information shows us whether BLAST+ is being used by the community, and therefore is worth being maintained and developed by NCBI. It also allows us to focus our development efforts on the most used aspects of BLAST+ and helps us keep BLAST+ reliable, performant, and around for the long term. If possible, please enable usage data collection, so we can continue to support and develop BLAST+ to best suit your needs.

## What is collected

When usage data collection is enabled, BLAST+ sends a short, fixed set of technical details after a search.

Here is the complete list, with an example value for each:


| Parameter | Example | What it means |
| --- | --- | --- |
| program | blastp | Which BLAST program you ran |
| task | blastp | The specific task / algorithm variant |
| version | 2.18.0 | Your BLAST+ version |
| db_name | swissprot | Name of the database searched |
| db_date | Aug 26, 2025 | When that database was created |
| db_length | 179658219 | Size of the database in letters (bases or residues) |
| db_num_seqs | 474714 | Number of sequences in the database |
| num_queries | 1 | How many query sequences you searched |
| queries_length | 656 | Total length of your queries in letters |
| hitlist_size | 500 | Max matches requested (same as max_target_seqs) |
| evalue_threshold | 10 | Expect value cutoff |
| comp_based_stats | 2 | Composition-based statistics setting |
| output_fmt | 11 | Output format chosen |
| num_threads | 2 | Number of threads used |
| run_time | 3.076507 | How long the search took in seconds |
| exit_status | 0 | BLAST program exit status. The value 0 indicates success. |
| os | UNIX | Operating system family |
| IP | 123.45.67.90 | The apparent IP address of the host system |
| ncbi_app | standalone-blast | Fixed value: standalone-blast |

## How do I opt in?

### blast\_usage\_report utility

To turn usage data collection on, run this command in a terminal:

```bash
blast_usage_report -on
```

The utility records your choice in an NCBI configuration file so it persists across sessions. You can check whether usage data collection is enabled, at anytime, by running this command in a terminal:

```bash
blast_usage_report -status
```

Run `blast_usage_report -help` for the full list of options. Using the utility is recommended over hand-editing configuration files, since it finds the right file for you (and tells you if it cannot write to it).

### Environment variable

To opt in for a session, run either command in a terminal. You can also add one to a shell profile, batch job, or container. The accepted values are `1`, `ON`, `TRUE`, or `YES` (case-insensitive):

```bash
export BLAST_USAGE_REPORT=1
```

## How do I opt out?

There is often nothing to do: *off is the default*. If you have never opted in, nothing is sent, and non-interactive runs are treated as opt-out automatically.

### blast\_usage\_report utility

To turn usage data collection off, run this command in a terminal:

```bash
blast_usage_report -off
```

### Environment variable

You can also change your choice for a session by running this command in a terminal. This is handy on a shared machine or in a container. `BLAST_USAGE_REPORT` is a two-way switch:

```bash
# 0, false, no, or off: all turn usage data collection off
export BLAST_USAGE_REPORT=false
```

`DO_NOT_TRACK` is a dedicated *off* switch: it can only *disable* usage data collection, following the [Console Do Not Track standard](https://donottrack.sh/). Any value other than `0`, `FALSE`, `NO`, or `OFF` (case-insensitive) disables usage data collection and overrides an opt-in from any other source:

```bash
export DO_NOT_TRACK=1
```

An invalid boolean value (for example `DO_NOT_TRACK=foo`) is rejected with a warning and ignored.

## How BLAST+ decides between multiple switches?

BLAST+ checks your preference in this order and stops at the first one it finds:

1. *`DO_NOT_TRACK`* : if set to anything other than `0`/`FALSE`/`NO`/`OFF`, disables usage data collection. This one only ever turns usage data collection *off*.
1. *`BLAST_USAGE_REPORT`* : two-way switch; a value of `1,` `ON`, `TRUE`, `YES` (case-insensitive) turns usage data collection *on*, a value of `0`, `FALSE`, `NO`, or `OFF` (case-insensitive) turns it *off*.
1. *Configuration file* : your saved preference. It lives in a standard NCBI configuration file (for a single user, `${HOME}/.ncbirc` under a `[BLAST]` section); machine-wide defaults can be set in the system configuration (`/etc/ncbirc.ini`). Settings from these files are layered rather than replaced, so a per-user choice and site-wide defaults can coexist. Let the `blast_usage_report` utility manage this for you rather than editing by hand.

Because the environment variables come first, they override a stored preference to enable or disable usage data collection.

## How changes to usage data collection will be announced

If the data BLAST+ collects changes, you will be able to find it documented in these places:

- *BLAST+ release notes*: [https://www.ncbi.nlm.nih.gov/books/NBK131777/](https://www.ncbi.nlm.nih.gov/books/NBK131777/)
- *BLAST_PRIVACY.md* in the source tree: [https://github.com/ncbi/ncbi-cxx-toolkit-public/tree/main/src/algo/blast/PRIVACY.md](https://github.com/ncbi/ncbi-cxx-toolkit-public/tree/main/src/algo/blast/PRIVACY.md)
- *BLAST+ User Manual*: [https://www.ncbi.nlm.nih.gov/books/NBK569851/](https://www.ncbi.nlm.nih.gov/books/NBK569851/)

Updated information will also reference the applicable NLM privacy policies.

## More information

*NLM Web Policies*: [https://www.nlm.nih.gov/web\_policies.html](https://www.nlm.nih.gov/web_policies.html)
