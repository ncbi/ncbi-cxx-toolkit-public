#! /bin/bash

module="$1"

if test "$module" = ""; then
    echo "Usage: $0 [module]"
    exit 1
fi

mkdir "$module" "../../include/objects/$module"
(
    echo "MODULE = $module"
    echo ""
    echo "include ../Makefile.sources.mk"
) > "$module/Makefile"
(
	echo "MODULE_PROJ = $module"
	echo "srcdir = @srcdir@"
	echo "include @builddir@/Makefile.meta"
) > "$module/Makefile.in"
(
	echo "MODULE = $module"
	grep "_$module" Makefile.modules.dep | sed "s/_$module//"
	echo "IROOT = objects"
) > "$module/Makefile.$module.module"
