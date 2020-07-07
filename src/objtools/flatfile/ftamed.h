#ifndef FTAMED_H
#define FTAMED_H

typedef struct findpubstr {
    bool    always_look;                /* if TRUE, look up even if muid in
                                           Pub-equiv */
    bool    replace_cit;                /* if TRUE, replace Cit-art w/ replace
                                           from MEDLINE */
    Int4    lookups_attempted;          /* citartmatch tries */
    Int4    lookups_succeeded;          /* citartmatch worked */
    Int4    fetches_attempted;          /* FetchPubs tried */
    Int4    fetches_succeeded;          /* FetchPubs that worked */
    bool    merge_ids;                  /* If TRUE then merges Cit-art.ids from
                                           input Cit-sub and one gotten from
                                           med server. */
} FindPubOption, PNTR FindPubOptionPtr;

ncbi::CRef<ncbi::objects::CCit_art> FetchPubPmId PROTO((Int4 pmid));

void FixPub PROTO((TPubList& pub_list, FindPubOptionPtr fpop));
void FixPubEquiv PROTO((TPubList& pub_list, FindPubOptionPtr fpop));
bool MedArchInit PROTO((void));
void MedArchFini PROTO((void));

#endif // FTAMED_H
