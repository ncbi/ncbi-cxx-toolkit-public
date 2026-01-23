#!/bin/sh
set -e # exit immediately if any commands fail

if diff --help 2>/dev/null | grep -e '--strip-trailing-cr' >/dev/null; then
    flags=--strip-trailing-cr
else
    flags=
fi

#Clearing existing environment variables
for x in `printenv | sed -ne 's/^\(NCBI_CONFIG_[^=]*\)=.*/\1/p'`; do
    echo "Clearing $x setting."
    unset $x
done

# Set environment variables with registry values (Windows version has capitalized value names)
export NCBI_CONFIG_PATH=`dirname $0`/test_sub_reg_data
unamestr=`uname -s`
case "$unamestr" in
  MINGW* | CYGWIN* )
    echo "Operating system is Windows"
    case "$PWD" in
      *GCC* )
        echo "compiler is GCC"
        ;;
      * )
        echo "compiler is MSVC"
        ;;
    esac
    export NCBI_CONFIG_OVERRIDES=$NCBI_CONFIG_PATH/indirect_env.win.ini
    export NCBI_CONFIG_E__TEST=env
    export NCBI_CONFIG__TEST__E=env
    export NCBI_CONFIG__TEST__EX=
    export NCBI_CONFIG__ENVIRONMENT_DOT_INDIRECTENV__E_IE=env
    export NCBI_CONFIG__ENVIRONMENT_DOT_INDIRECTENV__EX_IE=
    export NCBI_CONFIG__OVERRIDESBASE_DOT_ENVIRONMENT__OB_E=env
    export NCBI_CONFIG__OVERRIDESBASE_DOT_ENVIRONMENT__OBX_E=env
    export NCBI_CONFIG__A_SLASH_B__C_DOT_D_HYPHEN_E=f.g
    $CHECK_EXEC test_sub_reg -defaults "$NCBI_CONFIG_PATH/defaults.ini" \
                             -overrides "$NCBI_CONFIG_PATH/overrides.win.ini" \
                             -out test_sub_reg_1strun.out.ini
    ;;
  *)
    echo "Operating system is not Windows"
    export NCBI_CONFIG_OVERRIDES=$NCBI_CONFIG_PATH/indirect_env.ini
    export NCBI_CONFIG_e__TEST=env
    export NCBI_CONFIG__TEST__e=env
    export NCBI_CONFIG__TEST__ex=
    export NCBI_CONFIG__environment_DOT_indirectenv__e_ie=env
    export NCBI_CONFIG__environment_DOT_indirectenv__ex_ie=
    export NCBI_CONFIG__overridesbase_DOT_environment__ob_e=env
    export NCBI_CONFIG__overridesbase_DOT_environment__obx_e=env
    export NCBI_CONFIG__a_SLASH_b__c_DOT_d_HYPHEN_e=f.g
    $CHECK_EXEC test_sub_reg -defaults "$NCBI_CONFIG_PATH/defaults.ini" \
                             -overrides "$NCBI_CONFIG_PATH/overrides.ini" \
                             -out test_sub_reg_1strun.out.ini
  ;;
esac

echo "Comparing first run and expected output"
case "$unamestr" in
  MINGW* | CYGWIN* )
    case "$PWD" in
      *GCC* )
        diff $flags $NCBI_CONFIG_PATH/expected_win_gcc.ini test_sub_reg_1strun.out.ini
        ;;
      *)
        diff $flags $NCBI_CONFIG_PATH/expected_win_msvc.ini test_sub_reg_1strun.out.ini
        ;;
    esac
    ;;
  *)
    diff $flags $NCBI_CONFIG_PATH/expected.ini test_sub_reg_1strun.out.ini
    ;;
esac

# Second run
unset NCBI_CONFIG_PATH NCBI_CONFIG_OVERRIDES
export NCBI_DONT_USE_LOCAL_CONFIG=1

wd=`pwd`
case "$unamestr" in
  MINGW* | CYGWIN* )
    unset NCBI_CONFIG_E__TEST NCBI_CONFIG__TEST__E NCBI_CONFIG__TEST__EX
    unset NCBI_CONFIG__ENVIRONMENT_DOT_INDIRECTENV__EX_IE NCBI_CONFIG__OVERRIDESBASE_DOT_ENVIRONMENT__OB_E NCBI_CONFIG__OVERRIDESBASE_DOT_ENVIRONMENT__OBX_E 
    unset NCBI_CONFIG__ENVIRONMENT_DOT_INDIRECTENV__E_IE NCBI_CONFIG__A_DOT_B__C_DOT_D
    wd="$(cygpath -w $wd)"
    ;;
  * )
    unset NCBI_CONFIG_e__test NCBI_CONFIG__test__e NCBI_CONFIG__test__ex 
    unset NCBI_CONFIG__environment_DOT_indirectenv__ex_ie NCBI_CONFIG__overridesbase_DOT_environment__ob_e NCBI_CONFIG__overridesbase_DOT_environment__obx_e 
    unset NCBI_CONFIG__environment_DOT_indirectenv__e_ie NCBI_CONFIG__a_DOT_b__c_DOT_d
    ;;
esac

cp test_sub_reg_1strun.out.ini test_sub_reg.ini

$CHECK_EXEC test_sub_reg -conffile ${wd}/test_sub_reg.ini -out test_sub_reg_2ndrun.out.ini
sort test_sub_reg_1strun.out.ini -o test_sub_reg_1strun.sorted.out.ini
sort test_sub_reg_2ndrun.out.ini -o test_sub_reg_2ndrun.sorted.out.ini
echo "Comparing second run and first run"
diff $flags test_sub_reg_1strun.sorted.out.ini test_sub_reg_2ndrun.sorted.out.ini

rm test_sub_reg.ini

echo Test passed -- no differences encountered.
exit 0
