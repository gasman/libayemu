# Process this file with autoconf to produce a configure script.
AC_INIT(ayconv.c)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(ayconv, 0.9.1)

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB(ayemu, ayemu_init,,
AC_MSG_ERROR(cannot find ayemu library. Check your ayemu library installation))
AC_CHECK_LIB(sndfile, sf_open,,
AC_MSG_ERROR(cannot find sndfile library. Check your sndfile library installation))

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(stddef.h)

AC_CHECK_HEADERS(ayemu.h,,
AC_MSG_ERROR(cannot find ayemu.h. Check your ayemu library installation))

AC_CHECK_HEADERS(sndfile.h,,
AC_MSG_ERROR(cannot find sndfile.h. Check your sndfile library installation))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
#AC_FUNC_REALLOC
#AC_CHECK_FUNCS([memset])

#AC_CHECK_HEADERS([fcntl.h]) 
#AC_CHECK_HEADERS([sys/ioctl.h])

AC_CONFIG_FILES(Makefile)
AC_OUTPUT
