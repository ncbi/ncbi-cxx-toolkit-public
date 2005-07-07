/*
 * strtok_r(3)
 * 20020927 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 */

static char software_version[] = "$Id$";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#include <ncbi_tds_config.h> 

#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "replacements.h"

char *
strtok_r(char *str, const char *sep, char **lasts)
{
	char *p;

	if (str == NULL) {
		str = *lasts;
	}
	if (str == NULL) {
		return NULL;
	}
	str += strspn(str, sep);
	if ((p = strpbrk(str, sep)) != NULL) {
		*lasts = p + 1;
		*p = '\0';
	} else {
		*lasts = NULL;
	}
	return str;
}
