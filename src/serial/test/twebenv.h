#ifndef _twebenv_ 
#define _twebenv_ 

#undef NLM_EXTERN
#ifdef NLM_IMPORT
#define NLM_EXTERN NLM_IMPORT
#else
#define NLM_EXTERN extern
#endif


#ifdef __cplusplus
extern "C" { /* } */
#endif


/**************************************************
*
*    Generated objects for Module NCBI-Env
*    Generated using ASNCODE Revision: 6.0 at Oct 4, 1999  4:26 PM
*
**************************************************/

NLM_EXTERN Boolean LIBCALL
twebenvAsnLoad PROTO((void));


/**************************************************
*
*    WebEnv
*
**************************************************/
typedef struct struct_Web_Env {
   struct struct_Argument PNTR   arguments;
   struct struct_Db_Env PNTR   db_Env;
   struct struct_Query_History PNTR   queries;
} WebEnv, PNTR WebEnvPtr;


NLM_EXTERN WebEnvPtr LIBCALL WebEnvFree PROTO ((WebEnvPtr ));
NLM_EXTERN WebEnvPtr LIBCALL WebEnvNew PROTO (( void ));
NLM_EXTERN WebEnvPtr LIBCALL WebEnvAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL WebEnvAsnWrite PROTO (( WebEnvPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    Argument
*
**************************************************/
typedef struct struct_Argument {
   struct struct_Argument PNTR next;
   CharPtr   name;
   CharPtr   value;
} Argument, PNTR ArgumentPtr;


NLM_EXTERN ArgumentPtr LIBCALL ArgumentFree PROTO ((ArgumentPtr ));
NLM_EXTERN ArgumentPtr LIBCALL ArgumentNew PROTO (( void ));
NLM_EXTERN ArgumentPtr LIBCALL ArgumentAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL ArgumentAsnWrite PROTO (( ArgumentPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    DbEnv
*
**************************************************/
typedef struct struct_Db_Env {
   struct struct_Db_Env PNTR next;
   CharPtr   name;
   struct struct_Argument PNTR   arguments;
   struct struct_Filter_Value PNTR   filters;
   struct struct_Db_Clipboard PNTR   clipboard;
} DbEnv, PNTR DbEnvPtr;


NLM_EXTERN DbEnvPtr LIBCALL DbEnvFree PROTO ((DbEnvPtr ));
NLM_EXTERN DbEnvPtr LIBCALL DbEnvNew PROTO (( void ));
NLM_EXTERN DbEnvPtr LIBCALL DbEnvAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL DbEnvAsnWrite PROTO (( DbEnvPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    QueryHistory
*
**************************************************/
typedef struct struct_Query_History {
   struct struct_Query_History PNTR next;
   CharPtr   name;
   Int4   seqNumber;
   ValNodePtr   time;
   ValNodePtr   command;
} QueryHistory, PNTR QueryHistoryPtr;


NLM_EXTERN QueryHistoryPtr LIBCALL QueryHistoryFree PROTO ((QueryHistoryPtr ));
NLM_EXTERN QueryHistoryPtr LIBCALL QueryHistoryNew PROTO (( void ));
NLM_EXTERN QueryHistoryPtr LIBCALL QueryHistoryAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL QueryHistoryAsnWrite PROTO (( QueryHistoryPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    WebSettings
*
**************************************************/
typedef struct struct_Web_Settings {
   struct struct_Argument PNTR   arguments;
   struct struct_Db_Env PNTR   db_Env;
} WebSettings, PNTR WebSettingsPtr;


NLM_EXTERN WebSettingsPtr LIBCALL WebSettingsFree PROTO ((WebSettingsPtr ));
NLM_EXTERN WebSettingsPtr LIBCALL WebSettingsNew PROTO (( void ));
NLM_EXTERN WebSettingsPtr LIBCALL WebSettingsAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL WebSettingsAsnWrite PROTO (( WebSettingsPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    WebSaved
*
**************************************************/
typedef struct struct_Web_Saved {
   struct struct_Named_Query PNTR   queries;
   struct struct_Named_Item_Set PNTR   item_Sets;
} WebSaved, PNTR WebSavedPtr;


NLM_EXTERN WebSavedPtr LIBCALL WebSavedFree PROTO ((WebSavedPtr ));
NLM_EXTERN WebSavedPtr LIBCALL WebSavedNew PROTO (( void ));
NLM_EXTERN WebSavedPtr LIBCALL WebSavedAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL WebSavedAsnWrite PROTO (( WebSavedPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    NamedQuery
*
**************************************************/
typedef struct struct_Named_Query {
   struct struct_Named_Query PNTR next;
   struct struct_Name PNTR   name;
   ValNodePtr   time;
   ValNodePtr   command;
} NamedQuery, PNTR NamedQueryPtr;


NLM_EXTERN NamedQueryPtr LIBCALL NamedQueryFree PROTO ((NamedQueryPtr ));
NLM_EXTERN NamedQueryPtr LIBCALL NamedQueryNew PROTO (( void ));
NLM_EXTERN NamedQueryPtr LIBCALL NamedQueryAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL NamedQueryAsnWrite PROTO (( NamedQueryPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    NamedItemSet
*
**************************************************/
typedef struct struct_Named_Item_Set {
   struct struct_Named_Item_Set PNTR next;
   struct struct_Name PNTR   name;
   CharPtr   db;
   struct struct_Item_Set PNTR   item_Set;
} NamedItemSet, PNTR NamedItemSetPtr;


NLM_EXTERN NamedItemSetPtr LIBCALL NamedItemSetFree PROTO ((NamedItemSetPtr ));
NLM_EXTERN NamedItemSetPtr LIBCALL NamedItemSetNew PROTO (( void ));
NLM_EXTERN NamedItemSetPtr LIBCALL NamedItemSetAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL NamedItemSetAsnWrite PROTO (( NamedItemSetPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    FilterValue
*
**************************************************/
typedef struct struct_Filter_Value {
   struct struct_Filter_Value PNTR next;
   CharPtr   name;
   CharPtr   value;
} FilterValue, PNTR FilterValuePtr;


NLM_EXTERN FilterValuePtr LIBCALL FilterValueFree PROTO ((FilterValuePtr ));
NLM_EXTERN FilterValuePtr LIBCALL FilterValueNew PROTO (( void ));
NLM_EXTERN FilterValuePtr LIBCALL FilterValueAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL FilterValueAsnWrite PROTO (( FilterValuePtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    DbClipboard
*
**************************************************/
typedef struct struct_Db_Clipboard {
   struct struct_Db_Clipboard PNTR next;
   CharPtr   name;
   Int4   count;
   struct struct_Item_Set PNTR   items;
} DbClipboard, PNTR DbClipboardPtr;


NLM_EXTERN DbClipboardPtr LIBCALL DbClipboardFree PROTO ((DbClipboardPtr ));
NLM_EXTERN DbClipboardPtr LIBCALL DbClipboardNew PROTO (( void ));
NLM_EXTERN DbClipboardPtr LIBCALL DbClipboardAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL DbClipboardAsnWrite PROTO (( DbClipboardPtr , AsnIoPtr, AsnTypePtr));

typedef ValNodePtr TimePtr;
typedef ValNode Time;
#define Time_unix 1
#define Time_full 2


NLM_EXTERN TimePtr LIBCALL TimeFree PROTO ((TimePtr ));
NLM_EXTERN TimePtr LIBCALL TimeAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL TimeAsnWrite PROTO (( TimePtr , AsnIoPtr, AsnTypePtr));

typedef ValNodePtr QueryCommandPtr;
typedef ValNode QueryCommand;
#define QueryCommand_search 1
#define QueryCommand_select 2
#define QueryCommand_related 3


NLM_EXTERN QueryCommandPtr LIBCALL QueryCommandFree PROTO ((QueryCommandPtr ));
NLM_EXTERN QueryCommandPtr LIBCALL QueryCommandAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL QueryCommandAsnWrite PROTO (( QueryCommandPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    QuerySearch
*
**************************************************/
typedef struct struct_Query_Search {
   CharPtr   db;
   CharPtr   term;
   CharPtr   field;
   struct struct_Filter_Value PNTR   filters;
   Int4   count;
   Int4   flags;
} QuerySearch, PNTR QuerySearchPtr;


NLM_EXTERN QuerySearchPtr LIBCALL QuerySearchFree PROTO ((QuerySearchPtr ));
NLM_EXTERN QuerySearchPtr LIBCALL QuerySearchNew PROTO (( void ));
NLM_EXTERN QuerySearchPtr LIBCALL QuerySearchAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL QuerySearchAsnWrite PROTO (( QuerySearchPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    QuerySelect
*
**************************************************/
typedef struct struct_Query_Select {
   CharPtr   db;
   struct struct_Item_Set PNTR   items;
} QuerySelect, PNTR QuerySelectPtr;


NLM_EXTERN QuerySelectPtr LIBCALL QuerySelectFree PROTO ((QuerySelectPtr ));
NLM_EXTERN QuerySelectPtr LIBCALL QuerySelectNew PROTO (( void ));
NLM_EXTERN QuerySelectPtr LIBCALL QuerySelectAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL QuerySelectAsnWrite PROTO (( QuerySelectPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    QueryRelated
*
**************************************************/
typedef struct struct_Query_Related {
   ValNodePtr   base;
   CharPtr   relation;
   CharPtr   db;
   ValNodePtr   Items_items;
} QueryRelated, PNTR QueryRelatedPtr;


NLM_EXTERN QueryRelatedPtr LIBCALL QueryRelatedFree PROTO ((QueryRelatedPtr ));
NLM_EXTERN QueryRelatedPtr LIBCALL QueryRelatedNew PROTO (( void ));
NLM_EXTERN QueryRelatedPtr LIBCALL QueryRelatedAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL QueryRelatedAsnWrite PROTO (( QueryRelatedPtr , AsnIoPtr, AsnTypePtr));


#ifdef NLM_GENERATED_CODE_PROTO

typedef ValNodePtr Items_itemsPtr;
typedef ValNode Items_items;

#endif /* NLM_GENERATED_CODE_PROTO */

#define Items_items_items 1
#define Items_items_itemCount 2

#ifdef NLM_GENERATED_CODE_PROTO

static Items_itemsPtr LIBCALL Items_itemsFree PROTO ((Items_itemsPtr ));
static Items_itemsPtr LIBCALL Items_itemsAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
static Boolean LIBCALL Items_itemsAsnWrite PROTO (( Items_itemsPtr , AsnIoPtr, AsnTypePtr));

#endif /* NLM_GENERATED_CODE_PROTO */



/**************************************************
*
*    ItemSet
*
**************************************************/
typedef struct struct_Item_Set {
   Pointer   items;
   Int4   count;
} ItemSet, PNTR ItemSetPtr;


NLM_EXTERN ItemSetPtr LIBCALL ItemSetFree PROTO ((ItemSetPtr ));
NLM_EXTERN ItemSetPtr LIBCALL ItemSetNew PROTO (( void ));
NLM_EXTERN ItemSetPtr LIBCALL ItemSetAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL ItemSetAsnWrite PROTO (( ItemSetPtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    FullTime
*
**************************************************/
typedef struct struct_Full_Time {
   Int4   year;
   Int4   month;
   Int4   day;
   Int4   hour;
   Int4   minute;
   Int4   second;
} FullTime, PNTR FullTimePtr;


NLM_EXTERN FullTimePtr LIBCALL FullTimeFree PROTO ((FullTimePtr ));
NLM_EXTERN FullTimePtr LIBCALL FullTimeNew PROTO (( void ));
NLM_EXTERN FullTimePtr LIBCALL FullTimeAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL FullTimeAsnWrite PROTO (( FullTimePtr , AsnIoPtr, AsnTypePtr));



/**************************************************
*
*    Name
*
**************************************************/
typedef struct struct_Name {
   CharPtr   name;
   CharPtr   description;
} Name, PNTR NamePtr;


NLM_EXTERN NamePtr LIBCALL NameFree PROTO ((NamePtr ));
NLM_EXTERN NamePtr LIBCALL NameNew PROTO (( void ));
NLM_EXTERN NamePtr LIBCALL NameAsnRead PROTO (( AsnIoPtr, AsnTypePtr));
NLM_EXTERN Boolean LIBCALL NameAsnWrite PROTO (( NamePtr , AsnIoPtr, AsnTypePtr));

#ifdef __cplusplus
/* { */ }
#endif

#endif /* _twebenv_ */

#undef NLM_EXTERN
#ifdef NLM_EXPORT
#define NLM_EXTERN NLM_EXPORT
#else
#define NLM_EXTERN
#endif

