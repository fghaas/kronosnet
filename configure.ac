#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([kronosnetd], [0.1], [fabbione@kronosnet.org])
AM_INIT_AUTOMAKE([1.11.1 dist-bzip2 dist-xz color-tests -Wno-portability])
LT_PREREQ([2.2.6])
LT_INIT

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST
AC_PROG_LIBTOOL

AC_LANG([C])

if test "$prefix" = "NONE"; then
	prefix="/usr"
	if test "$localstatedir" = "\${prefix}/var"; then
		localstatedir="/var"
	fi
	if test "$sysconfdir" = "\${prefix}/etc"; then
		sysconfdir="/etc"
	fi
fi

# Checks for programs.
if ! ${MAKE-make} --version /cannot/make/this >/dev/null 2>&1; then
	AC_MSG_ERROR([you don't seem to have GNU make; it is required])
fi
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CXX
AC_PROG_RANLIB
AC_CHECK_PROGS([PUBLICAN], [publican], [:])
AC_CHECK_PROGS([PKGCONFIG], [pkg-config])

## local helper functions
# this function checks if CC support options passed as
# args. Global CFLAGS are ignored during this test.
cc_supports_flag() {
	local CFLAGS="$@"
	AC_MSG_CHECKING([whether $CC supports "$@"])
	AC_COMPILE_IFELSE([int main(){return 0;}] ,
			  [RC=0; AC_MSG_RESULT([yes])],
			  [RC=1; AC_MSG_RESULT([no])])
	return $RC
}

# helper macro to check libs without adding them to LIBS
check_lib_no_libs() {
	lib_no_libs_arg1=$1
	shift
	lib_no_libs_arg2=$1
	shift
	lib_no_libs_args=$@
	AC_CHECK_LIB([$lib_no_libs_arg1],
		     [$lib_no_libs_arg2],,,
		     [$lib_no_libs_args])
        LIBS=$ac_check_lib_save_LIBS
}

# Checks for C features
AC_C_INLINE

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([rt], [clock_gettime])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([syslog.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([limits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_PID_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([dup2])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([inet_ntoa])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([atexit])
AC_CHECK_FUNCS([ftruncate])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([clock_gettime])

# PAM check
AC_CHECK_HEADERS([security/pam_appl.h],
		 [AC_CHECK_LIB([pam], [pam_start])],
		 [AC_MSG_ERROR([Unable to find LinuxPAM devel files])])

AC_CHECK_HEADERS([security/pam_misc.h],
		 [AC_CHECK_LIB([pam_misc], [misc_conv])],
		 [AC_MSG_ERROR([Unable to find LinuxPAM MISC devel files])])

# local options
AC_ARG_ENABLE([debug],
	[  --enable-debug          enable debug build. ],
	[ default="no" ])

AC_ARG_ENABLE([publicandocs],
	[  --enable-publicandocs   enable docs build. ],
	[ default="no" ])

AC_ARG_WITH([syslogfacility],
	[  --with-syslogfacility=FACILITY
			  default syslog facility. ],
	[ SYSLOGFACILITY="$withval" ],
	[ SYSLOGFACILITY="LOG_LOCAL4" ])

AC_ARG_WITH([sysloglevel],
	[  --with-sysloglevel=LEVEL
			  default syslog level. ],
	[ SYSLOGLEVEL="$withval" ],
	[ SYSLOGLEVEL="LOG_INFO" ])

AC_ARG_WITH([defaultadmgroup],
	[  --with-defaultadmgroup=GROUP
			  define PAM group. Users part of this group will be
			  allowed to configure kronosnet. Others will only
			  receive read-only rights. ],
	[ DEFAULTADMGROUP="$withval" ],
	[ DEFAULTADMGROUP="kronosnetadm" ])

## random vars
LOGDIR=${localstatedir}/log/
RUNDIR=${localstatedir}/run/
DEFAULT_CONFIG_DIR=${sysconfdir}/kronosnet

## do subst

AM_CONDITIONAL(BUILD_DOCS, test "x${enable_publicandocs}" = xyes)

AC_SUBST([DEFAULT_CONFIG_DIR])
AC_DEFINE_UNQUOTED([DEFAULT_CONFIG_DIR], "$(eval echo ${DEFAULT_CONFIG_DIR})",
		   [Default config directory])

AC_DEFINE_UNQUOTED([DEFAULT_CONFIG_FILE],
		   "$(eval echo ${DEFAULT_CONFIG_DIR}/kronosnetd.conf)",
		   [Default config file])

AC_DEFINE_UNQUOTED([LOGDIR], "$(eval echo ${LOGDIR})",
		   [Default logging directory])

AC_DEFINE_UNQUOTED([RUNDIR], "$(eval echo ${RUNDIR})",
		   [Default run directory])

AC_DEFINE_UNQUOTED([SYSLOGFACILITY], "$(eval echo ${SYSLOGFACILITY})",
		   [Default syslog facility])

AC_DEFINE_UNQUOTED([SYSLOGLEVEL], "$(eval echo ${SYSLOGLEVEL})",
		   [Default syslog level])

AC_DEFINE_UNQUOTED([DEFAULTADMGROUP], "$(eval echo ${DEFAULTADMGROUP})",
		   [Default admin group])

## *FLAGS handling
ENV_CFLAGS="$CFLAGS"
ENV_CPPFLAGS="$CPPFLAGS"
ENV_LDFLAGS="$LDFLAGS"

# debug build stuff
if test "x${enable_debug}" = xyes; then
	AC_DEFINE_UNQUOTED([DEBUG], [1], [Compiling Debugging code])
	OPT_CFLAGS="-O0"
else
	OPT_CFLAGS="-O3"
fi

# gdb flags
if test "x${GCC}" = xyes; then
	GDB_FLAGS="-ggdb3"
else
	GDB_FLAGS="-g"
fi

# extra warnings
EXTRA_WARNINGS=""

WARNLIST="
	all
	shadow
	missing-prototypes
	missing-declarations
	strict-prototypes
	declaration-after-statement
	pointer-arith
	write-strings
	cast-align
	bad-function-cast
	missing-format-attribute
	format=2
	format-security
	format-nonliteral
	no-long-long
	unsigned-char
	gnu89-inline
	no-strict-aliasing
	error
	address
	cpp
	enum-compare
	overflow
	parentheses
	sequence-point
	switch
	uninitialized
	unused-but-set-variable
	unused-function
	unused-result
	unused-value
	unused-variable
	"

for j in $WARNLIST; do
	if cc_supports_flag -W$j; then
		EXTRA_WARNINGS="$EXTRA_WARNINGS -W$j";
	fi
done

CFLAGS="$ENV_CFLAGS $OPT_CFLAGS $GDB_FLAGS \
	$EXTRA_WARNINGS $WERROR_CFLAGS"
CPPFLAGS="$ENV_CPPFLAGS"
LDFLAGS="$ENV_LDFLAGS"

AC_CONFIG_FILES([
		Makefile
		libtap/Makefile
		libtap/libtap.pc
		libknet/Makefile
		libknet/libknet.pc
		libvty/Makefile
		libvty/libvty.pc
		docs/Makefile
		tests/Makefile
		])

AC_OUTPUT
