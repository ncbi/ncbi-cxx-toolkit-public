/* The CGI_C library, by Thomas Boutell, version 1.0. CGI_C is intended
	to be a high-quality API to simplify CGI programming tasks. */

/* Make sure this is only included once. */

#ifndef CGI_C
#define CGI_C 1

extern "C" {
/* Bring in standard I/O since some of the functions refer to
	types defined by it, such as FILE *. */

#include <stdio.h>

/* The various CGI environment variables. Instead of using getenv(),
	the programmer should refer to these, which are always
	valid null-terminated strings (they may be empty, but they 
	will never be null). If these variables are used instead
	of calling getenv(), then it will be possible to save
	and restore CGI environments, which is highly convenient
	for debugging. */

extern char *cgiServerSoftware;
extern char *cgiServerName;
extern char *cgiGatewayInterface;
extern char *cgiServerProtocol;
extern char *cgiServerPort;
extern char *cgiRequestMethod;
extern char *cgiPathInfo;
extern char *cgiPathTranslated;
extern char *cgiScriptName;
extern char *cgiQueryString;
extern char *cgiRemoteHost;
extern char *cgiRemoteAddr;
extern char *cgiAuthType;
extern char *cgiRemoteUser;
extern char *cgiRemoteIdent;
extern char *cgiContentType;
extern char *cgiAccept;
extern char *cgiUserAgent;

/* The number of bytes of data received.
	Note that if the submission is a form submission
	the library will read and parse all the information
	directly from cgiIn; the programmer need not do so. */

extern int cgiContentLength;

/* Pointer to CGI output. The cgiHeader functions should be used
	first to output the mime headers; the output HTML
	page, GIF image or other web document should then be written
	to cgiOut by the programmer. */

extern FILE *cgiOut;

/* Pointer to CGI input. In 99% of cases, the programmer will NOT
	need this. However, in some applications, things other than 
	forms are posted to the server, in which case this file may
	be read from in order to retrieve the contents. */

extern FILE *cgiIn;

/* Possible return codes from the cgiForm family of functions (see below). */

typedef enum {
	cgiFormSuccess,
	cgiFormTruncated,
	cgiFormBadType,
	cgiFormEmpty,
	cgiFormNotFound,
	cgiFormConstrained,
	cgiFormNoSuchChoice,
	cgiFormMemory
} cgiFormResultType;

/* These functions are used to retrieve form data. See
	cgic.html for documentation. */


extern cgiFormResultType cgiFormString(
	char *name, char *result, int max);


extern cgiFormResultType cgiFormStringNoNewlines(
	char *name, char *result, int max);


extern cgiFormResultType cgiFormStringSpaceNeeded(
	char *name, int *length);


extern cgiFormResultType cgiFormStringMultiple(
	char *name, char ***ptrToStringArray);


extern void cgiStringArrayFree(char **stringArray);
	

extern cgiFormResultType cgiFormInteger(
	char *name, int *result, int defaultV);


extern cgiFormResultType cgiFormIntegerBounded(
	char *name, int *result, int min, int max, int defaultV);

extern cgiFormResultType cgiFormDouble(
	char *name, double *result, double defaultV);


extern cgiFormResultType cgiFormDoubleBounded(
	char *name, double *result, double min, double max, double defaultV);


extern cgiFormResultType cgiFormSelectSingle(
	char *name, char **choicesText, int choicesTotal, 
	int *result, int defaultV);	


extern cgiFormResultType cgiFormSelectMultiple(
	char *name, char **choicesText, int choicesTotal, 
	int *result, int *invalid);


extern cgiFormResultType cgiFormCheckboxSingle(
	char *name);


extern cgiFormResultType cgiFormCheckboxMultiple(
	char *name, char **valuesText, int valuesTotal, 
	int *result, int *invalid);


extern cgiFormResultType cgiFormRadio(
	char *name, char **valuesText, int valuesTotal, 
	int *result, int defaultV);	


extern void cgiHeaderLocation(char *redirectUrl);
extern void cgiHeaderStatus(int status, char *statusMessage);
extern void cgiHeaderContentType(char *mimeType);

#ifndef NO_SYSTEM

extern int cgiSaferSystem(char *command);

#endif /* NO_SYSTEM */

typedef enum {
	cgiEnvironmentIO,
	cgiEnvironmentMemory,
	cgiEnvironmentSuccess
} cgiEnvironmentResultType;

extern cgiEnvironmentResultType cgiWriteEnvironment(char *filename);
extern cgiEnvironmentResultType cgiReadEnvironment(char *filename);


extern int cgiMain(int argc, char *argv[]);

}

#endif /* CGI_C */

