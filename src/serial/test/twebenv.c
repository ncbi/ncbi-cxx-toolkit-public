#include <ncbiconf.h>
#if HAVE_NCBI_C
#include <asn.h>

#define NLM_GENERATED_CODE_PROTO

#include "twebenv.h"

static Boolean loaded = FALSE;

#include "twebenvasn.h"

#ifndef NLM_EXTERN_LOADS
#define NLM_EXTERN_LOADS {}
#endif

NLM_EXTERN Boolean LIBCALL
twebenvAsnLoad(void)
{

   if ( ! loaded) {
      NLM_EXTERN_LOADS

      if ( ! AsnLoad ())
      return FALSE;
      loaded = TRUE;
   }

   return TRUE;
}



/**************************************************
*    Generated object loaders for Module NCBI-Env
*    Generated using ASNCODE Revision: 6.0 at Oct 4, 1999  4:26 PM
*
**************************************************/


/**************************************************
*
*    WebEnvNew()
*
**************************************************/
NLM_EXTERN 
WebEnvPtr LIBCALL
WebEnvNew(void)
{
   WebEnvPtr ptr = MemNew((size_t) sizeof(WebEnv));

   return ptr;

}


/**************************************************
*
*    WebEnvFree()
*
**************************************************/
NLM_EXTERN 
WebEnvPtr LIBCALL
WebEnvFree(WebEnvPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   AsnGenericUserSeqOfFree(ptr -> arguments, (AsnOptFreeFunc) ArgumentFree);
   AsnGenericUserSeqOfFree(ptr -> db_Env, (AsnOptFreeFunc) DbEnvFree);
   AsnGenericUserSeqOfFree(ptr -> queries, (AsnOptFreeFunc) QueryHistoryFree);
   return MemFree(ptr);
}


/**************************************************
*
*    WebEnvAsnRead()
*
**************************************************/
NLM_EXTERN 
WebEnvPtr LIBCALL
WebEnvAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   WebEnvPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* WebEnv ::= (self contained) */
      atp = AsnReadId(aip, amp, WEB_ENV);
   } else {
      atp = AsnLinkType(orig, WEB_ENV);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = WebEnvNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == WEB_ENV_arguments) {
      ptr -> arguments = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) ArgumentAsnRead, (AsnOptFreeFunc) ArgumentFree);
      if (isError && ptr -> arguments == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == WEB_ENV_db_Env) {
      ptr -> db_Env = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) DbEnvAsnRead, (AsnOptFreeFunc) DbEnvFree);
      if (isError && ptr -> db_Env == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == WEB_ENV_queries) {
      ptr -> queries = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) QueryHistoryAsnRead, (AsnOptFreeFunc) QueryHistoryFree);
      if (isError && ptr -> queries == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = WebEnvFree(ptr);
   goto ret;
}



/**************************************************
*
*    WebEnvAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
WebEnvAsnWrite(WebEnvPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, WEB_ENV);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   AsnGenericUserSeqOfAsnWrite(ptr -> arguments, (AsnWriteFunc) ArgumentAsnWrite, aip, WEB_ENV_arguments, WEB_ENV_arguments_E);
   AsnGenericUserSeqOfAsnWrite(ptr -> db_Env, (AsnWriteFunc) DbEnvAsnWrite, aip, WEB_ENV_db_Env, WEB_ENV_db_Env_E);
   AsnGenericUserSeqOfAsnWrite(ptr -> queries, (AsnWriteFunc) QueryHistoryAsnWrite, aip, WEB_ENV_queries, WEB_ENV_queries_E);
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    ArgumentNew()
*
**************************************************/
NLM_EXTERN 
ArgumentPtr LIBCALL
ArgumentNew(void)
{
   ArgumentPtr ptr = MemNew((size_t) sizeof(Argument));

   return ptr;

}


/**************************************************
*
*    ArgumentFree()
*
**************************************************/
NLM_EXTERN 
ArgumentPtr LIBCALL
ArgumentFree(ArgumentPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   MemFree(ptr -> name);
   MemFree(ptr -> value);
   return MemFree(ptr);
}


/**************************************************
*
*    ArgumentAsnRead()
*
**************************************************/
NLM_EXTERN 
ArgumentPtr LIBCALL
ArgumentAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   ArgumentPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* Argument ::= (self contained) */
      atp = AsnReadId(aip, amp, ARGUMENT);
   } else {
      atp = AsnLinkType(orig, ARGUMENT);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = ArgumentNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == ARGUMENT_name) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> name = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == ARGUMENT_value) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> value = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = ArgumentFree(ptr);
   goto ret;
}



/**************************************************
*
*    ArgumentAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
ArgumentAsnWrite(ArgumentPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, ARGUMENT);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> name != NULL) {
      av.ptrvalue = ptr -> name;
      retval = AsnWrite(aip, ARGUMENT_name,  &av);
   }
   if (ptr -> value != NULL) {
      av.ptrvalue = ptr -> value;
      retval = AsnWrite(aip, ARGUMENT_value,  &av);
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    DbEnvNew()
*
**************************************************/
NLM_EXTERN 
DbEnvPtr LIBCALL
DbEnvNew(void)
{
   DbEnvPtr ptr = MemNew((size_t) sizeof(DbEnv));

   return ptr;

}


/**************************************************
*
*    DbEnvFree()
*
**************************************************/
NLM_EXTERN 
DbEnvPtr LIBCALL
DbEnvFree(DbEnvPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   MemFree(ptr -> name);
   AsnGenericUserSeqOfFree(ptr -> arguments, (AsnOptFreeFunc) ArgumentFree);
   AsnGenericUserSeqOfFree(ptr -> filters, (AsnOptFreeFunc) FilterValueFree);
   AsnGenericUserSeqOfFree(ptr -> clipboard, (AsnOptFreeFunc) DbClipboardFree);
   return MemFree(ptr);
}


/**************************************************
*
*    DbEnvAsnRead()
*
**************************************************/
NLM_EXTERN 
DbEnvPtr LIBCALL
DbEnvAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   DbEnvPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* DbEnv ::= (self contained) */
      atp = AsnReadId(aip, amp, DB_ENV);
   } else {
      atp = AsnLinkType(orig, DB_ENV);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = DbEnvNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == DB_ENV_name) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> name = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == DB_ENV_arguments) {
      ptr -> arguments = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) ArgumentAsnRead, (AsnOptFreeFunc) ArgumentFree);
      if (isError && ptr -> arguments == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == DB_ENV_filters) {
      ptr -> filters = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) FilterValueAsnRead, (AsnOptFreeFunc) FilterValueFree);
      if (isError && ptr -> filters == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == DB_ENV_clipboard) {
      ptr -> clipboard = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) DbClipboardAsnRead, (AsnOptFreeFunc) DbClipboardFree);
      if (isError && ptr -> clipboard == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = DbEnvFree(ptr);
   goto ret;
}



/**************************************************
*
*    DbEnvAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
DbEnvAsnWrite(DbEnvPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, DB_ENV);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> name != NULL) {
      av.ptrvalue = ptr -> name;
      retval = AsnWrite(aip, DB_ENV_name,  &av);
   }
   AsnGenericUserSeqOfAsnWrite(ptr -> arguments, (AsnWriteFunc) ArgumentAsnWrite, aip, DB_ENV_arguments, DB_ENV_arguments_E);
   AsnGenericUserSeqOfAsnWrite(ptr -> filters, (AsnWriteFunc) FilterValueAsnWrite, aip, DB_ENV_filters, DB_ENV_filters_E);
   AsnGenericUserSeqOfAsnWrite(ptr -> clipboard, (AsnWriteFunc) DbClipboardAsnWrite, aip, DB_ENV_clipboard, DB_ENV_clipboard_E);
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    QueryHistoryNew()
*
**************************************************/
NLM_EXTERN 
QueryHistoryPtr LIBCALL
QueryHistoryNew(void)
{
   QueryHistoryPtr ptr = MemNew((size_t) sizeof(QueryHistory));

   return ptr;

}


/**************************************************
*
*    QueryHistoryFree()
*
**************************************************/
NLM_EXTERN 
QueryHistoryPtr LIBCALL
QueryHistoryFree(QueryHistoryPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   MemFree(ptr -> name);
   TimeFree(ptr -> time);
   QueryCommandFree(ptr -> command);
   return MemFree(ptr);
}


/**************************************************
*
*    QueryHistoryAsnRead()
*
**************************************************/
NLM_EXTERN 
QueryHistoryPtr LIBCALL
QueryHistoryAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   QueryHistoryPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* QueryHistory ::= (self contained) */
      atp = AsnReadId(aip, amp, QUERY_HISTORY);
   } else {
      atp = AsnLinkType(orig, QUERY_HISTORY);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = QueryHistoryNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == QUERY_HISTORY_name) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> name = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_HISTORY_seqNumber) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> seqNumber = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_HISTORY_time) {
      ptr -> time = TimeAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_HISTORY_command) {
      ptr -> command = QueryCommandAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = QueryHistoryFree(ptr);
   goto ret;
}



/**************************************************
*
*    QueryHistoryAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
QueryHistoryAsnWrite(QueryHistoryPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, QUERY_HISTORY);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> name != NULL) {
      av.ptrvalue = ptr -> name;
      retval = AsnWrite(aip, QUERY_HISTORY_name,  &av);
   }
   av.intvalue = ptr -> seqNumber;
   retval = AsnWrite(aip, QUERY_HISTORY_seqNumber,  &av);
   if (ptr -> time != NULL) {
      if ( ! TimeAsnWrite(ptr -> time, aip, QUERY_HISTORY_time)) {
         goto erret;
      }
   }
   if (ptr -> command != NULL) {
      if ( ! QueryCommandAsnWrite(ptr -> command, aip, QUERY_HISTORY_command)) {
         goto erret;
      }
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    WebSettingsNew()
*
**************************************************/
NLM_EXTERN 
WebSettingsPtr LIBCALL
WebSettingsNew(void)
{
   WebSettingsPtr ptr = MemNew((size_t) sizeof(WebSettings));

   return ptr;

}


/**************************************************
*
*    WebSettingsFree()
*
**************************************************/
NLM_EXTERN 
WebSettingsPtr LIBCALL
WebSettingsFree(WebSettingsPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   AsnGenericUserSeqOfFree(ptr -> arguments, (AsnOptFreeFunc) ArgumentFree);
   AsnGenericUserSeqOfFree(ptr -> db_Env, (AsnOptFreeFunc) DbEnvFree);
   return MemFree(ptr);
}


/**************************************************
*
*    WebSettingsAsnRead()
*
**************************************************/
NLM_EXTERN 
WebSettingsPtr LIBCALL
WebSettingsAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   WebSettingsPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* WebSettings ::= (self contained) */
      atp = AsnReadId(aip, amp, WEB_SETTINGS);
   } else {
      atp = AsnLinkType(orig, WEB_SETTINGS);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = WebSettingsNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == WEB_SETTINGS_arguments) {
      ptr -> arguments = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) ArgumentAsnRead, (AsnOptFreeFunc) ArgumentFree);
      if (isError && ptr -> arguments == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == WEB_SETTINGS_db_Env) {
      ptr -> db_Env = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) DbEnvAsnRead, (AsnOptFreeFunc) DbEnvFree);
      if (isError && ptr -> db_Env == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = WebSettingsFree(ptr);
   goto ret;
}



/**************************************************
*
*    WebSettingsAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
WebSettingsAsnWrite(WebSettingsPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, WEB_SETTINGS);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   AsnGenericUserSeqOfAsnWrite(ptr -> arguments, (AsnWriteFunc) ArgumentAsnWrite, aip, WEB_SETTINGS_arguments, WEB_SETTINGS_arguments_E);
   AsnGenericUserSeqOfAsnWrite(ptr -> db_Env, (AsnWriteFunc) DbEnvAsnWrite, aip, WEB_SETTINGS_db_Env, WEB_SETTINGS_db_Env_E);
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    WebSavedNew()
*
**************************************************/
NLM_EXTERN 
WebSavedPtr LIBCALL
WebSavedNew(void)
{
   WebSavedPtr ptr = MemNew((size_t) sizeof(WebSaved));

   return ptr;

}


/**************************************************
*
*    WebSavedFree()
*
**************************************************/
NLM_EXTERN 
WebSavedPtr LIBCALL
WebSavedFree(WebSavedPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   AsnGenericUserSeqOfFree(ptr -> queries, (AsnOptFreeFunc) NamedQueryFree);
   AsnGenericUserSeqOfFree(ptr -> item_Sets, (AsnOptFreeFunc) NamedItemSetFree);
   return MemFree(ptr);
}


/**************************************************
*
*    WebSavedAsnRead()
*
**************************************************/
NLM_EXTERN 
WebSavedPtr LIBCALL
WebSavedAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   WebSavedPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* WebSaved ::= (self contained) */
      atp = AsnReadId(aip, amp, WEB_SAVED);
   } else {
      atp = AsnLinkType(orig, WEB_SAVED);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = WebSavedNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == WEB_SAVED_queries) {
      ptr -> queries = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) NamedQueryAsnRead, (AsnOptFreeFunc) NamedQueryFree);
      if (isError && ptr -> queries == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == WEB_SAVED_item_Sets) {
      ptr -> item_Sets = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) NamedItemSetAsnRead, (AsnOptFreeFunc) NamedItemSetFree);
      if (isError && ptr -> item_Sets == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = WebSavedFree(ptr);
   goto ret;
}



/**************************************************
*
*    WebSavedAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
WebSavedAsnWrite(WebSavedPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, WEB_SAVED);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   AsnGenericUserSeqOfAsnWrite(ptr -> queries, (AsnWriteFunc) NamedQueryAsnWrite, aip, WEB_SAVED_queries, WEB_SAVED_queries_E);
   AsnGenericUserSeqOfAsnWrite(ptr -> item_Sets, (AsnWriteFunc) NamedItemSetAsnWrite, aip, WEB_SAVED_item_Sets, WEB_SAVED_item_Sets_E);
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    NamedQueryNew()
*
**************************************************/
NLM_EXTERN 
NamedQueryPtr LIBCALL
NamedQueryNew(void)
{
   NamedQueryPtr ptr = MemNew((size_t) sizeof(NamedQuery));

   return ptr;

}


/**************************************************
*
*    NamedQueryFree()
*
**************************************************/
NLM_EXTERN 
NamedQueryPtr LIBCALL
NamedQueryFree(NamedQueryPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   NameFree(ptr -> name);
   TimeFree(ptr -> time);
   QueryCommandFree(ptr -> command);
   return MemFree(ptr);
}


/**************************************************
*
*    NamedQueryAsnRead()
*
**************************************************/
NLM_EXTERN 
NamedQueryPtr LIBCALL
NamedQueryAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   NamedQueryPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* NamedQuery ::= (self contained) */
      atp = AsnReadId(aip, amp, NAMED_QUERY);
   } else {
      atp = AsnLinkType(orig, NAMED_QUERY);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = NamedQueryNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == NAMED_QUERY_name) {
      ptr -> name = NameAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == NAMED_QUERY_time) {
      ptr -> time = TimeAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == NAMED_QUERY_command) {
      ptr -> command = QueryCommandAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = NamedQueryFree(ptr);
   goto ret;
}



/**************************************************
*
*    NamedQueryAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
NamedQueryAsnWrite(NamedQueryPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, NAMED_QUERY);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> name != NULL) {
      if ( ! NameAsnWrite(ptr -> name, aip, NAMED_QUERY_name)) {
         goto erret;
      }
   }
   if (ptr -> time != NULL) {
      if ( ! TimeAsnWrite(ptr -> time, aip, NAMED_QUERY_time)) {
         goto erret;
      }
   }
   if (ptr -> command != NULL) {
      if ( ! QueryCommandAsnWrite(ptr -> command, aip, NAMED_QUERY_command)) {
         goto erret;
      }
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    NamedItemSetNew()
*
**************************************************/
NLM_EXTERN 
NamedItemSetPtr LIBCALL
NamedItemSetNew(void)
{
   NamedItemSetPtr ptr = MemNew((size_t) sizeof(NamedItemSet));

   return ptr;

}


/**************************************************
*
*    NamedItemSetFree()
*
**************************************************/
NLM_EXTERN 
NamedItemSetPtr LIBCALL
NamedItemSetFree(NamedItemSetPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   NameFree(ptr -> name);
   MemFree(ptr -> db);
   ItemSetFree(ptr -> item_Set);
   return MemFree(ptr);
}


/**************************************************
*
*    NamedItemSetAsnRead()
*
**************************************************/
NLM_EXTERN 
NamedItemSetPtr LIBCALL
NamedItemSetAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   NamedItemSetPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* NamedItemSet ::= (self contained) */
      atp = AsnReadId(aip, amp, NAMED_ITEM_SET);
   } else {
      atp = AsnLinkType(orig, NAMED_ITEM_SET);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = NamedItemSetNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == NAMED_ITEM_SET_name) {
      ptr -> name = NameAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == NAMED_ITEM_SET_db) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> db = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == NAMED_ITEM_SET_item_Set) {
      ptr -> item_Set = ItemSetAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = NamedItemSetFree(ptr);
   goto ret;
}



/**************************************************
*
*    NamedItemSetAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
NamedItemSetAsnWrite(NamedItemSetPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, NAMED_ITEM_SET);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> name != NULL) {
      if ( ! NameAsnWrite(ptr -> name, aip, NAMED_ITEM_SET_name)) {
         goto erret;
      }
   }
   if (ptr -> db != NULL) {
      av.ptrvalue = ptr -> db;
      retval = AsnWrite(aip, NAMED_ITEM_SET_db,  &av);
   }
   if (ptr -> item_Set != NULL) {
      if ( ! ItemSetAsnWrite(ptr -> item_Set, aip, NAMED_ITEM_SET_item_Set)) {
         goto erret;
      }
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    FilterValueNew()
*
**************************************************/
NLM_EXTERN 
FilterValuePtr LIBCALL
FilterValueNew(void)
{
   FilterValuePtr ptr = MemNew((size_t) sizeof(FilterValue));

   return ptr;

}


/**************************************************
*
*    FilterValueFree()
*
**************************************************/
NLM_EXTERN 
FilterValuePtr LIBCALL
FilterValueFree(FilterValuePtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   MemFree(ptr -> name);
   MemFree(ptr -> value);
   return MemFree(ptr);
}


/**************************************************
*
*    FilterValueAsnRead()
*
**************************************************/
NLM_EXTERN 
FilterValuePtr LIBCALL
FilterValueAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   FilterValuePtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* FilterValue ::= (self contained) */
      atp = AsnReadId(aip, amp, FILTER_VALUE);
   } else {
      atp = AsnLinkType(orig, FILTER_VALUE);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = FilterValueNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == FILTER_VALUE_name) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> name = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == FILTER_VALUE_value) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> value = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = FilterValueFree(ptr);
   goto ret;
}



/**************************************************
*
*    FilterValueAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
FilterValueAsnWrite(FilterValuePtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, FILTER_VALUE);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> name != NULL) {
      av.ptrvalue = ptr -> name;
      retval = AsnWrite(aip, FILTER_VALUE_name,  &av);
   }
   if (ptr -> value != NULL) {
      av.ptrvalue = ptr -> value;
      retval = AsnWrite(aip, FILTER_VALUE_value,  &av);
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    DbClipboardNew()
*
**************************************************/
NLM_EXTERN 
DbClipboardPtr LIBCALL
DbClipboardNew(void)
{
   DbClipboardPtr ptr = MemNew((size_t) sizeof(DbClipboard));

   return ptr;

}


/**************************************************
*
*    DbClipboardFree()
*
**************************************************/
NLM_EXTERN 
DbClipboardPtr LIBCALL
DbClipboardFree(DbClipboardPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   MemFree(ptr -> name);
   ItemSetFree(ptr -> items);
   return MemFree(ptr);
}


/**************************************************
*
*    DbClipboardAsnRead()
*
**************************************************/
NLM_EXTERN 
DbClipboardPtr LIBCALL
DbClipboardAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   DbClipboardPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* DbClipboard ::= (self contained) */
      atp = AsnReadId(aip, amp, DB_CLIPBOARD);
   } else {
      atp = AsnLinkType(orig, DB_CLIPBOARD);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = DbClipboardNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == DB_CLIPBOARD_name) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> name = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == DB_CLIPBOARD_count) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> count = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == DB_CLIPBOARD_items) {
      ptr -> items = ItemSetAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = DbClipboardFree(ptr);
   goto ret;
}



/**************************************************
*
*    DbClipboardAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
DbClipboardAsnWrite(DbClipboardPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, DB_CLIPBOARD);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> name != NULL) {
      av.ptrvalue = ptr -> name;
      retval = AsnWrite(aip, DB_CLIPBOARD_name,  &av);
   }
   av.intvalue = ptr -> count;
   retval = AsnWrite(aip, DB_CLIPBOARD_count,  &av);
   if (ptr -> items != NULL) {
      if ( ! ItemSetAsnWrite(ptr -> items, aip, DB_CLIPBOARD_items)) {
         goto erret;
      }
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    TimeFree()
*
**************************************************/
NLM_EXTERN 
TimePtr LIBCALL
TimeFree(ValNodePtr anp)
{
   Pointer pnt;

   if (anp == NULL) {
      return NULL;
   }

   pnt = anp->data.ptrvalue;
   switch (anp->choice)
   {
   default:
      break;
   case Time_full:
      FullTimeFree(anp -> data.ptrvalue);
      break;
   }
   return MemFree(anp);
}


/**************************************************
*
*    TimeAsnRead()
*
**************************************************/
NLM_EXTERN 
TimePtr LIBCALL
TimeAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   ValNodePtr anp;
   Uint1 choice;
   Boolean isError = FALSE;
   Boolean nullIsError = FALSE;
   AsnReadFunc func;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* Time ::= (self contained) */
      atp = AsnReadId(aip, amp, TIME);
   } else {
      atp = AsnLinkType(orig, TIME);    /* link in local tree */
   }
   if (atp == NULL) {
      return NULL;
   }

   anp = ValNodeNew(NULL);
   if (anp == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the CHOICE or OpenStruct value (nothing) */
      goto erret;
   }

   func = NULL;

   atp = AsnReadId(aip, amp, atp);  /* find the choice */
   if (atp == NULL) {
      goto erret;
   }
   if (atp == TIME_unix) {
      choice = Time_unix;
      if (AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      anp->data.intvalue = av.intvalue;
   }
   else if (atp == TIME_full) {
      choice = Time_full;
      func = (AsnReadFunc) FullTimeAsnRead;
   }
   anp->choice = choice;
   if (func != NULL)
   {
      anp->data.ptrvalue = (* func)(aip, atp);
      if (aip -> io_failure) goto erret;

      if (nullIsError && anp->data.ptrvalue == NULL) {
         goto erret;
      }
   }

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return anp;

erret:
   anp = MemFree(anp);
   aip -> io_failure = TRUE;
   goto ret;
}


/**************************************************
*
*    TimeAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
TimeAsnWrite(TimePtr anp, AsnIoPtr aip, AsnTypePtr orig)

{
   DataVal av;
   AsnTypePtr atp, writetype = NULL;
   Pointer pnt;
   AsnWriteFunc func = NULL;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad())
      return FALSE;
   }

   if (aip == NULL)
   return FALSE;

   atp = AsnLinkType(orig, TIME);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (anp == NULL) { AsnNullValueMsg(aip, atp); goto erret; }

   av.ptrvalue = (Pointer)anp;
   if (! AsnWriteChoice(aip, atp, (Int2)anp->choice, &av)) {
      goto erret;
   }

   pnt = anp->data.ptrvalue;
   switch (anp->choice)
   {
   case Time_unix:
      av.intvalue = anp->data.intvalue;
      retval = AsnWrite(aip, TIME_unix, &av);
      break;
   case Time_full:
      writetype = TIME_full;
      func = (AsnWriteFunc) FullTimeAsnWrite;
      break;
   }
   if (writetype != NULL) {
      retval = (* func)(pnt, aip, writetype);   /* write it out */
   }
   if (!retval) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}


/**************************************************
*
*    QueryCommandFree()
*
**************************************************/
NLM_EXTERN 
QueryCommandPtr LIBCALL
QueryCommandFree(ValNodePtr anp)
{
   Pointer pnt;

   if (anp == NULL) {
      return NULL;
   }

   pnt = anp->data.ptrvalue;
   switch (anp->choice)
   {
   default:
      break;
   case QueryCommand_search:
      QuerySearchFree(anp -> data.ptrvalue);
      break;
   case QueryCommand_select:
      QuerySelectFree(anp -> data.ptrvalue);
      break;
   case QueryCommand_related:
      QueryRelatedFree(anp -> data.ptrvalue);
      break;
   }
   return MemFree(anp);
}


/**************************************************
*
*    QueryCommandAsnRead()
*
**************************************************/
NLM_EXTERN 
QueryCommandPtr LIBCALL
QueryCommandAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   ValNodePtr anp;
   Uint1 choice;
   Boolean isError = FALSE;
   Boolean nullIsError = FALSE;
   AsnReadFunc func;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* QueryCommand ::= (self contained) */
      atp = AsnReadId(aip, amp, QUERY_COMMAND);
   } else {
      atp = AsnLinkType(orig, QUERY_COMMAND);    /* link in local tree */
   }
   if (atp == NULL) {
      return NULL;
   }

   anp = ValNodeNew(NULL);
   if (anp == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the CHOICE or OpenStruct value (nothing) */
      goto erret;
   }

   func = NULL;

   atp = AsnReadId(aip, amp, atp);  /* find the choice */
   if (atp == NULL) {
      goto erret;
   }
   if (atp == QUERY_COMMAND_search) {
      choice = QueryCommand_search;
      func = (AsnReadFunc) QuerySearchAsnRead;
   }
   else if (atp == QUERY_COMMAND_select) {
      choice = QueryCommand_select;
      func = (AsnReadFunc) QuerySelectAsnRead;
   }
   else if (atp == QUERY_COMMAND_related) {
      choice = QueryCommand_related;
      func = (AsnReadFunc) QueryRelatedAsnRead;
   }
   anp->choice = choice;
   if (func != NULL)
   {
      anp->data.ptrvalue = (* func)(aip, atp);
      if (aip -> io_failure) goto erret;

      if (nullIsError && anp->data.ptrvalue == NULL) {
         goto erret;
      }
   }

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return anp;

erret:
   anp = MemFree(anp);
   aip -> io_failure = TRUE;
   goto ret;
}


/**************************************************
*
*    QueryCommandAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
QueryCommandAsnWrite(QueryCommandPtr anp, AsnIoPtr aip, AsnTypePtr orig)

{
   DataVal av;
   AsnTypePtr atp, writetype = NULL;
   Pointer pnt;
   AsnWriteFunc func = NULL;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad())
      return FALSE;
   }

   if (aip == NULL)
   return FALSE;

   atp = AsnLinkType(orig, QUERY_COMMAND);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (anp == NULL) { AsnNullValueMsg(aip, atp); goto erret; }

   av.ptrvalue = (Pointer)anp;
   if (! AsnWriteChoice(aip, atp, (Int2)anp->choice, &av)) {
      goto erret;
   }

   pnt = anp->data.ptrvalue;
   switch (anp->choice)
   {
   case QueryCommand_search:
      writetype = QUERY_COMMAND_search;
      func = (AsnWriteFunc) QuerySearchAsnWrite;
      break;
   case QueryCommand_select:
      writetype = QUERY_COMMAND_select;
      func = (AsnWriteFunc) QuerySelectAsnWrite;
      break;
   case QueryCommand_related:
      writetype = QUERY_COMMAND_related;
      func = (AsnWriteFunc) QueryRelatedAsnWrite;
      break;
   }
   if (writetype != NULL) {
      retval = (* func)(pnt, aip, writetype);   /* write it out */
   }
   if (!retval) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}


/**************************************************
*
*    QuerySearchNew()
*
**************************************************/
NLM_EXTERN 
QuerySearchPtr LIBCALL
QuerySearchNew(void)
{
   QuerySearchPtr ptr = MemNew((size_t) sizeof(QuerySearch));

   return ptr;

}


/**************************************************
*
*    QuerySearchFree()
*
**************************************************/
NLM_EXTERN 
QuerySearchPtr LIBCALL
QuerySearchFree(QuerySearchPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   MemFree(ptr -> db);
   MemFree(ptr -> term);
   MemFree(ptr -> field);
   AsnGenericUserSeqOfFree(ptr -> filters, (AsnOptFreeFunc) FilterValueFree);
   return MemFree(ptr);
}


/**************************************************
*
*    QuerySearchAsnRead()
*
**************************************************/
NLM_EXTERN 
QuerySearchPtr LIBCALL
QuerySearchAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   QuerySearchPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* QuerySearch ::= (self contained) */
      atp = AsnReadId(aip, amp, QUERY_SEARCH);
   } else {
      atp = AsnLinkType(orig, QUERY_SEARCH);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = QuerySearchNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == QUERY_SEARCH_db) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> db = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_SEARCH_term) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> term = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_SEARCH_field) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> field = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_SEARCH_filters) {
      ptr -> filters = AsnGenericUserSeqOfAsnRead(aip, amp, atp, &isError, (AsnReadFunc) FilterValueAsnRead, (AsnOptFreeFunc) FilterValueFree);
      if (isError && ptr -> filters == NULL) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_SEARCH_count) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> count = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_SEARCH_flags) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> flags = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = QuerySearchFree(ptr);
   goto ret;
}



/**************************************************
*
*    QuerySearchAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
QuerySearchAsnWrite(QuerySearchPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, QUERY_SEARCH);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> db != NULL) {
      av.ptrvalue = ptr -> db;
      retval = AsnWrite(aip, QUERY_SEARCH_db,  &av);
   }
   if (ptr -> term != NULL) {
      av.ptrvalue = ptr -> term;
      retval = AsnWrite(aip, QUERY_SEARCH_term,  &av);
   }
   if (ptr -> field != NULL) {
      av.ptrvalue = ptr -> field;
      retval = AsnWrite(aip, QUERY_SEARCH_field,  &av);
   }
   AsnGenericUserSeqOfAsnWrite(ptr -> filters, (AsnWriteFunc) FilterValueAsnWrite, aip, QUERY_SEARCH_filters, QUERY_SEARCH_filters_E);
   av.intvalue = ptr -> count;
   retval = AsnWrite(aip, QUERY_SEARCH_count,  &av);
   av.intvalue = ptr -> flags;
   retval = AsnWrite(aip, QUERY_SEARCH_flags,  &av);
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    QuerySelectNew()
*
**************************************************/
NLM_EXTERN 
QuerySelectPtr LIBCALL
QuerySelectNew(void)
{
   QuerySelectPtr ptr = MemNew((size_t) sizeof(QuerySelect));

   return ptr;

}


/**************************************************
*
*    QuerySelectFree()
*
**************************************************/
NLM_EXTERN 
QuerySelectPtr LIBCALL
QuerySelectFree(QuerySelectPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   MemFree(ptr -> db);
   ItemSetFree(ptr -> items);
   return MemFree(ptr);
}


/**************************************************
*
*    QuerySelectAsnRead()
*
**************************************************/
NLM_EXTERN 
QuerySelectPtr LIBCALL
QuerySelectAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   QuerySelectPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* QuerySelect ::= (self contained) */
      atp = AsnReadId(aip, amp, QUERY_SELECT);
   } else {
      atp = AsnLinkType(orig, QUERY_SELECT);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = QuerySelectNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == QUERY_SELECT_db) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> db = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_SELECT_items) {
      ptr -> items = ItemSetAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = QuerySelectFree(ptr);
   goto ret;
}



/**************************************************
*
*    QuerySelectAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
QuerySelectAsnWrite(QuerySelectPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, QUERY_SELECT);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> db != NULL) {
      av.ptrvalue = ptr -> db;
      retval = AsnWrite(aip, QUERY_SELECT_db,  &av);
   }
   if (ptr -> items != NULL) {
      if ( ! ItemSetAsnWrite(ptr -> items, aip, QUERY_SELECT_items)) {
         goto erret;
      }
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    QueryRelatedNew()
*
**************************************************/
NLM_EXTERN 
QueryRelatedPtr LIBCALL
QueryRelatedNew(void)
{
   QueryRelatedPtr ptr = MemNew((size_t) sizeof(QueryRelated));

   return ptr;

}


/**************************************************
*
*    QueryRelatedFree()
*
**************************************************/
NLM_EXTERN 
QueryRelatedPtr LIBCALL
QueryRelatedFree(QueryRelatedPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   QueryCommandFree(ptr -> base);
   MemFree(ptr -> relation);
   MemFree(ptr -> db);
   Items_itemsFree(ptr -> Items_items);
   return MemFree(ptr);
}


/**************************************************
*
*    Items_itemsFree()
*
**************************************************/
static 
Items_itemsPtr LIBCALL
Items_itemsFree(ValNodePtr anp)
{
   Pointer pnt;

   if (anp == NULL) {
      return NULL;
   }

   pnt = anp->data.ptrvalue;
   switch (anp->choice)
   {
   default:
      break;
   case Items_items_items:
      ItemSetFree(anp -> data.ptrvalue);
      break;
   }
   return MemFree(anp);
}


/**************************************************
*
*    QueryRelatedAsnRead()
*
**************************************************/
NLM_EXTERN 
QueryRelatedPtr LIBCALL
QueryRelatedAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   QueryRelatedPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* QueryRelated ::= (self contained) */
      atp = AsnReadId(aip, amp, QUERY_RELATED);
   } else {
      atp = AsnLinkType(orig, QUERY_RELATED);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = QueryRelatedNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == QUERY_RELATED_base) {
      ptr -> base = QueryCommandAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_RELATED_relation) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> relation = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_RELATED_db) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> db = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == QUERY_RELATED_items) {
      ptr -> Items_items = Items_itemsAsnRead(aip, atp);
      if (aip -> io_failure) {
         goto erret;
      }
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = QueryRelatedFree(ptr);
   goto ret;
}



/**************************************************
*
*    Items_itemsAsnRead()
*
**************************************************/
static 
Items_itemsPtr LIBCALL
Items_itemsAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   ValNodePtr anp;
   Uint1 choice;
   Boolean isError = FALSE;
   Boolean nullIsError = FALSE;
   AsnReadFunc func;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* Items_items ::= (self contained) */
      atp = AsnReadId(aip, amp, QUERY_RELATED_items);
   } else {
      atp = AsnLinkType(orig, QUERY_RELATED_items);    /* link in local tree */
   }
   if (atp == NULL) {
      return NULL;
   }

   anp = ValNodeNew(NULL);
   if (anp == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the CHOICE or OpenStruct value (nothing) */
      goto erret;
   }

   func = NULL;

   atp = AsnReadId(aip, amp, atp);  /* find the choice */
   if (atp == NULL) {
      goto erret;
   }
   if (atp == QUERY_RELATED_items_items) {
      choice = Items_items_items;
      func = (AsnReadFunc) ItemSetAsnRead;
   }
   else if (atp == QUERY_RELATED_items_itemCount) {
      choice = Items_items_itemCount;
      if (AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      anp->data.intvalue = av.intvalue;
   }
   anp->choice = choice;
   if (func != NULL)
   {
      anp->data.ptrvalue = (* func)(aip, atp);
      if (aip -> io_failure) goto erret;

      if (nullIsError && anp->data.ptrvalue == NULL) {
         goto erret;
      }
   }

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return anp;

erret:
   anp = MemFree(anp);
   aip -> io_failure = TRUE;
   goto ret;
}


/**************************************************
*
*    QueryRelatedAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
QueryRelatedAsnWrite(QueryRelatedPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, QUERY_RELATED);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> base != NULL) {
      if ( ! QueryCommandAsnWrite(ptr -> base, aip, QUERY_RELATED_base)) {
         goto erret;
      }
   }
   if (ptr -> relation != NULL) {
      av.ptrvalue = ptr -> relation;
      retval = AsnWrite(aip, QUERY_RELATED_relation,  &av);
   }
   if (ptr -> db != NULL) {
      av.ptrvalue = ptr -> db;
      retval = AsnWrite(aip, QUERY_RELATED_db,  &av);
   }
   if (ptr -> Items_items != NULL) {
      if ( ! Items_itemsAsnWrite(ptr -> Items_items, aip, QUERY_RELATED_items)) {
         goto erret;
      }
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    Items_itemsAsnWrite()
*
**************************************************/
static Boolean LIBCALL 
Items_itemsAsnWrite(Items_itemsPtr anp, AsnIoPtr aip, AsnTypePtr orig)

{
   DataVal av;
   AsnTypePtr atp, writetype = NULL;
   Pointer pnt;
   AsnWriteFunc func = NULL;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad())
      return FALSE;
   }

   if (aip == NULL)
   return FALSE;

   atp = AsnLinkType(orig, QUERY_RELATED_items);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (anp == NULL) { AsnNullValueMsg(aip, atp); goto erret; }

   av.ptrvalue = (Pointer)anp;
   if (! AsnWriteChoice(aip, atp, (Int2)anp->choice, &av)) {
      goto erret;
   }

   pnt = anp->data.ptrvalue;
   switch (anp->choice)
   {
   case Items_items_items:
      writetype = QUERY_RELATED_items_items;
      func = (AsnWriteFunc) ItemSetAsnWrite;
      break;
   case Items_items_itemCount:
      av.intvalue = anp->data.intvalue;
      retval = AsnWrite(aip, QUERY_RELATED_items_itemCount, &av);
      break;
   }
   if (writetype != NULL) {
      retval = (* func)(pnt, aip, writetype);   /* write it out */
   }
   if (!retval) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}


/**************************************************
*
*    ItemSetNew()
*
**************************************************/
NLM_EXTERN 
ItemSetPtr LIBCALL
ItemSetNew(void)
{
   ItemSetPtr ptr = MemNew((size_t) sizeof(ItemSet));

   return ptr;

}


/**************************************************
*
*    ItemSetFree()
*
**************************************************/
NLM_EXTERN 
ItemSetPtr LIBCALL
ItemSetFree(ItemSetPtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   BSFree(ptr -> items);
   return MemFree(ptr);
}


/**************************************************
*
*    ItemSetAsnRead()
*
**************************************************/
NLM_EXTERN 
ItemSetPtr LIBCALL
ItemSetAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   ItemSetPtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* ItemSet ::= (self contained) */
      atp = AsnReadId(aip, amp, ITEM_SET);
   } else {
      atp = AsnLinkType(orig, ITEM_SET);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = ItemSetNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == ITEM_SET_items) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> items = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == ITEM_SET_count) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> count = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = ItemSetFree(ptr);
   goto ret;
}



/**************************************************
*
*    ItemSetAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
ItemSetAsnWrite(ItemSetPtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, ITEM_SET);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> items != NULL) {
      av.ptrvalue = ptr -> items;
      retval = AsnWrite(aip, ITEM_SET_items,  &av);
   }
   av.intvalue = ptr -> count;
   retval = AsnWrite(aip, ITEM_SET_count,  &av);
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    FullTimeNew()
*
**************************************************/
NLM_EXTERN 
FullTimePtr LIBCALL
FullTimeNew(void)
{
   FullTimePtr ptr = MemNew((size_t) sizeof(FullTime));

   return ptr;

}


/**************************************************
*
*    FullTimeFree()
*
**************************************************/
NLM_EXTERN 
FullTimePtr LIBCALL
FullTimeFree(FullTimePtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   return MemFree(ptr);
}


/**************************************************
*
*    FullTimeAsnRead()
*
**************************************************/
NLM_EXTERN 
FullTimePtr LIBCALL
FullTimeAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   FullTimePtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* FullTime ::= (self contained) */
      atp = AsnReadId(aip, amp, FULL_TIME);
   } else {
      atp = AsnLinkType(orig, FULL_TIME);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = FullTimeNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == FULL_TIME_year) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> year = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == FULL_TIME_month) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> month = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == FULL_TIME_day) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> day = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == FULL_TIME_hour) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> hour = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == FULL_TIME_minute) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> minute = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == FULL_TIME_second) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> second = av.intvalue;
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = FullTimeFree(ptr);
   goto ret;
}



/**************************************************
*
*    FullTimeAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
FullTimeAsnWrite(FullTimePtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, FULL_TIME);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   av.intvalue = ptr -> year;
   retval = AsnWrite(aip, FULL_TIME_year,  &av);
   av.intvalue = ptr -> month;
   retval = AsnWrite(aip, FULL_TIME_month,  &av);
   av.intvalue = ptr -> day;
   retval = AsnWrite(aip, FULL_TIME_day,  &av);
   av.intvalue = ptr -> hour;
   retval = AsnWrite(aip, FULL_TIME_hour,  &av);
   av.intvalue = ptr -> minute;
   retval = AsnWrite(aip, FULL_TIME_minute,  &av);
   av.intvalue = ptr -> second;
   retval = AsnWrite(aip, FULL_TIME_second,  &av);
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}



/**************************************************
*
*    NameNew()
*
**************************************************/
NLM_EXTERN 
NamePtr LIBCALL
NameNew(void)
{
   NamePtr ptr = MemNew((size_t) sizeof(Name));

   return ptr;

}


/**************************************************
*
*    NameFree()
*
**************************************************/
NLM_EXTERN 
NamePtr LIBCALL
NameFree(NamePtr ptr)
{

   if(ptr == NULL) {
      return NULL;
   }
   MemFree(ptr -> name);
   MemFree(ptr -> description);
   return MemFree(ptr);
}


/**************************************************
*
*    NameAsnRead()
*
**************************************************/
NLM_EXTERN 
NamePtr LIBCALL
NameAsnRead(AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean isError = FALSE;
   AsnReadFunc func;
   NamePtr ptr;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return NULL;
      }
   }

   if (aip == NULL) {
      return NULL;
   }

   if (orig == NULL) {         /* Name ::= (self contained) */
      atp = AsnReadId(aip, amp, NAME);
   } else {
      atp = AsnLinkType(orig, NAME);
   }
   /* link in local tree */
   if (atp == NULL) {
      return NULL;
   }

   ptr = NameNew();
   if (ptr == NULL) {
      goto erret;
   }
   if (AsnReadVal(aip, atp, &av) <= 0) { /* read the start struct */
      goto erret;
   }

   atp = AsnReadId(aip,amp, atp);
   func = NULL;

   if (atp == NAME_name) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> name = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }
   if (atp == NAME_description) {
      if ( AsnReadVal(aip, atp, &av) <= 0) {
         goto erret;
      }
      ptr -> description = av.ptrvalue;
      atp = AsnReadId(aip,amp, atp);
   }

   if (AsnReadVal(aip, atp, &av) <= 0) {
      goto erret;
   }
   /* end struct */

ret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return ptr;

erret:
   aip -> io_failure = TRUE;
   ptr = NameFree(ptr);
   goto ret;
}



/**************************************************
*
*    NameAsnWrite()
*
**************************************************/
NLM_EXTERN Boolean LIBCALL 
NameAsnWrite(NamePtr ptr, AsnIoPtr aip, AsnTypePtr orig)
{
   DataVal av;
   AsnTypePtr atp;
   Boolean retval = FALSE;

   if (! loaded)
   {
      if (! twebenvAsnLoad()) {
         return FALSE;
      }
   }

   if (aip == NULL) {
      return FALSE;
   }

   atp = AsnLinkType(orig, NAME);   /* link local tree */
   if (atp == NULL) {
      return FALSE;
   }

   if (ptr == NULL) { AsnNullValueMsg(aip, atp); goto erret; }
   if (! AsnOpenStruct(aip, atp, (Pointer) ptr)) {
      goto erret;
   }

   if (ptr -> name != NULL) {
      av.ptrvalue = ptr -> name;
      retval = AsnWrite(aip, NAME_name,  &av);
   }
   if (ptr -> description != NULL) {
      av.ptrvalue = ptr -> description;
      retval = AsnWrite(aip, NAME_description,  &av);
   }
   if (! AsnCloseStruct(aip, atp, (Pointer)ptr)) {
      goto erret;
   }
   retval = TRUE;

erret:
   AsnUnlinkType(orig);       /* unlink local tree */
   return retval;
}
#endif

