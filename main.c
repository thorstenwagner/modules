/*****
 ** ** Module Header ******************************************************* **
 ** 									     **
 **   Modules Revision 3.0						     **
 **   Providing a flexible user environment				     **
 ** 									     **
 **   File:		main.c						     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Authors:	John Furlan, jlf@behere.com				     **
 **		Jens Hamisch, jens@Strawberry.COM			     **
 **		R.K. Owen, <rk@owen.sj.ca.us> or <rkowen@nersc.gov>	     **
 ** 									     **
 **   Description:	The main routine of Tcl Modules including all of     **
 **			the global data.				     **
 ** 									     **
 **   Exports:		main		Main program			     **
 **			module_usage	Module usage information	     **
 **			Tcl_AppInit	Tcl Application initialization	     **
 ** 									     **
 **   Notes:								     **
 ** 									     **
 ** ************************************************************************ **
 ****/

/** ** Copyright *********************************************************** **
 ** 									     **
 ** Copyright 1991-1994 by John L. Furlan.                      	     **
 ** see LICENSE.GPL, which must be provided, for details		     **
 ** 									     ** 
 ** ************************************************************************ **/

static char Id[] = "@(#)$Id$";
static void *UseId[] = { &UseId, Id };

/** ************************************************************************ **/
/** 				      HEADERS				     **/
/** ************************************************************************ **/

#include "modules_def.h"
#include "getopt.h"

/** ************************************************************************ **/
/** 				  LOCAL DATATYPES			     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/** 				     CONSTANTS				     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/** 				    COMMON STRINGS			     **/
/** ************************************************************************ **/

/* TRANSLATORS: The next five are the common strings */
char	em_reading[]	= N_("reading");
char	em_writing[]	= N_("writing");
char	em_appending[]	= N_("appending");
char	em_read_write[]	= N_("read/write");
char	em_unknown[]	= N_("unknown");
/* TRANSLATORS: id string for default versions */
char	em_default[]	= N_("default"); 

/** ************************************************************************ **/
/** 				    GLOBAL DATA				     **/
/** ************************************************************************ **/

uvec	 *ModulePathVec;		/** MODULEPATH uvec object	     **/
char	**ModulePath,			/** vector of the above		     **/
	 *g_current_module = NULL,	/** The module which is handled by   **/
					/** the current command		     **/
	 *g_specified_module = NULL,	/** The module that was specified    **/
					/** on the command line		     **/
	 *g_module_path = NULL,		/** The path to the module	     **/
	 *shell_name,			/** Name of the shell		     **/
					/** (first parameter to modulcmd)    **/
	 *shell_derelict,		/** Shell family (sh, csh, etc)	     **/
	 *shell_init,			/** Shell init script name	     **/
	 *shell_cmd_separator,		/** Shell command separator char     **/
	  _colon[] = ":",		/** directory separator		     **/
	 *psep;				/** path separator (default = "/")   **/
int	  g_flags = 0,			/** Control what to do at the moment **/
					/** The posible values are defined in**/
					/** module_def.h		     **/
	  g_retval = 0,			/** exit return value		     **/
	  g_output = 0,			/** Has output been generated	     **/
	  append_flag = 0;		/** only used by the 'use' command   **/
Tcl_Obj	 *cwd;				/** Tcl version of cwd		     **/
MHash	 *skipdirs;			/** directories to skip		     **/

/**
 **  Name of the rc files
 **  PREFIX points to the location where modules is going to be installed.
 **  It comes from the Makefile
 **/

char	*etcpath = SYSCONFDIR,
	*rc_file = RCFILE,
	*modulerc_file = MODULERCFILE,
	*version_file = VERSIONFILE;

/**
 **  pointers for regular expression evaluations
 **/
char
    *addRE      = "^(add|load)", 		/** 'module add <file>'	     **/
    *rmRE       = "^(rm|del|era|remov|unload)",	/** 'module unload <file>'   **/
    *swRE       = "^sw",			/** 'module switch <file>'   **/
    *dispRE     = "^(disp|show)", 		/** 'module display <file>'  **/
    *listRE     = "^li", 			/** 'module list <file>'     **/
    *availRE    = "^av",			/** 'module avail <file>'    **/
    *helpRE     = "^(help|\\?)",		/** 'module help <file>'     **/
    *initRE     = "^init",			/** 'module init'	     **/
    *useRE      = "^use",			/** 'module use <file>'	     **/
    *unuseRE    = "^unuse",			/** 'module unuse <file>'    **/
    *updateRE   = "^update",			/** 'module update'	     **/
    *purgeRE    = "^purge",			/** 'module purge'	     **/
    *clearRE    = "^clear",			/** 'module clear'	     **/
    *whatisRE   = "^wh",			/** 'module whatis'	     **/
    *aproposRE  = "^(apr|key)",			/** 'module apropos'	     **/
    *refreshRE  = "^refr";			/** 'module refresh'	     **/
  
/**
 **  Hash-Tables for all changes to the environment.
 **/

MHash		*setenvHashTable,
		*unsetenvHashTable,
		*aliasSetHashTable,
		*aliasUnsetHashTable,
		*markVariableHashTable,
		*markAliasHashTable;

MHash		*GlobalHashTables[5];

/**
 **  A buffer for reading a single line
 **/

char	*line = NULL;

/**
 **  Flags influenced by the command line switches
 **/

int	sw_force = 0,
	sw_detach = 0,
	sw_format = 0,
	sw_verbose = 0,
	sw_userlvl = UL_ADVANCED,
	sw_icase = 0;

/** ************************************************************************ **/
/** 				    LOCAL DATA				     **/
/** ************************************************************************ **/

static	char	module_name[] = __FILE__;

/** ************************************************************************ **/
/**				    PROTOTYPES				     **/
/** ************************************************************************ **/

static int	Check_Switches( int *argc, char *argv[]);
static void	version (FILE *output);

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		main						     **
 ** 									     **
 **   Description:	Main program					     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	int	argc		Number of parameters	     **
 **			char	*argv[]		Command line arguments	     **
 **			char	*environ[]	Pointer to the process' en-  **
 **						vironment.		     **
 **   Result:		int	1		Usage information printed    **
 **				TCL_OK		Successful completion	     **
 **				other		Return value of the module   **
 **			 			subcomand		     **
 ** 									     **
 **   Attached Globals:	*Ptr	 	by Initialize_Module		     **
 **			*HashTable	by Initialize_Module	 	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int main(
	int argc,
	char *argv[],
	char *environ[]
) {

	Tcl_Interp     *interp;
	int             return_val = 0;
	char           *rc_name;
	char           *rc_path;
	Tcl_Obj       **objv;		/** Tcl Object vector **/
	int             objc;		/** Tcl Object vector count **/

#ifdef HAVE_SETLOCALE
	/* set local via LC_ALL */
	setlocale(LC_ALL, "");
#endif

#if ENABLE_NLS
	/* the text message domain. */
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif
    /**
     ** check if first argument is --version or -V then output the
     ** version to stdout.  This is a special circumstance handled
     ** by the regular options.
     **/
	if (argc > 1 && *argv[1] == '-') {
		if (!strcmp("-V", argv[1]) || !strcmp("--version", argv[1])) {
			version(stderr);
			return 0;
		}
	}
    /**
     **  Initialization. 
     **/
	if (!(ModulePathVec = ModulePathList())) {
		ModulePath = NULL;
		/* goto unwind0; */
	} else {
		ModulePath = uvec_vector(ModulePathVec);
	}

    /**
     **  Check the command line syntax. There will be no return from the
     **  initialization function in case of invalid command line arguments.
     **/
	if (TCL_OK != Initialize_Module(&interp, argc, argv, environ))
		goto unwind1;

	if (TCL_OK != Setup_Environment(interp))
		goto unwind1;

    /**
     **  Check for command line switches
     **/
	return_val = Check_Switches(&argc, argv);
	/* deal with help and version special case */
	if (return_val != TCL_OK) {
	  if(return_val == ~TCL_OK)
	    g_retval = return_val;
	  goto unwind1;
	}
    /**
     **  Figure out, which global RC file to use. This depends on the environ-
     **  ment variable 'MODULERCFILE', which can be set to one of the following:
     **
     **		<filename>	-->	SYSCONFDIR/<filename>
     **		<dir>/		-->	<dir>/RC_FILE
     **		<dir>/<file>	-->	<dir>/<file>
     **  Use xgetenv to expand 1 level of env.vars.
     **/
	if ((rc_name = xgetenv("MODULERCFILE"))) {
		/* found something in MODULERCFILE */
		if (!(rc_path = stringer(NULL, 0, rc_name, NULL))) {
			if (OK != ErrorLogger(ERR_STRING, LOC, NULL))
				goto unwind2;
			else
				null_free((void *)&rc_name);
		} else {
			null_free((void *)&rc_name);
			if (!(rc_name = strrchr(rc_path, *psep))) {
				rc_name = rc_path;
				rc_path = etcpath;
			} else
				*rc_name++ = '\0';
			if (!*rc_name) {
				rc_name = rc_file;
			}
		}
	} else {
		rc_path = stringer(NULL,0, etcpath, NULL);
		null_free((void *)&rc_name);
		rc_name = rc_file;
	}
    /**
     **  Source the global and the user defined RC file
     **/
	g_current_module = (char *)NULL;

	if (TCL_ERROR == SourceRC(interp, rc_path, rc_name, Mod_Load) ||
	    TCL_ERROR == SourceRC(interp,getenv("HOME"),modulerc_file,Mod_Load))
		exit(1);

	if (rc_path)
		null_free((void *)&rc_path);
    /**
     **  Invocation of the module command as specified in the command line
     **/
	g_flags = 0;
	Tcl_ArgvToObjv(&objc, &objv, argc - 1, argv + 1);
	return_val = cmdModule((ClientData) 0, interp, objc, objv);
    /**
     **  If we were doing some operation that has already flushed its output,
     **  then we don't need to re-flush the output here.
     **
     **  Also, if we've had an error here, then the whole modulecmd failed
     **  and not just the values for a single modulefile.  So, we'll pass in
     **  a NULL here to indicate that any error message should say that
     **  absolutely NO changes were made to the environment.
     **/
	if (TCL_OK == return_val) {
		Output_Modulefile_Changes(interp);
#ifdef HAS_X11LIBS
		xresourceFinish(1);
#endif
	} else {
		Unwind_Modulefile_Changes(interp, NULL);
#ifdef HAS_X11LIBS
		xresourceFinish(0);
#endif
	}
    /**
     **  Finally clean up. Delete the required hash tables and conditionally
     **  allocated areas.
     **/
	Global_Hash_Tables(GHashDelete, NULL);

	if (line)
		null_free((void *)&line);
	if (error_line)
		null_free((void *)&error_line);
    /**
     **  This return value may be evaluated by the calling shell
     **/
	OutputExit();
	return (return_val ? return_val : g_retval);

/* unwind3:
    null_free((void *) &rc_path); */
unwind2:
	null_free((void *)&rc_name);
unwind1:
	FreeList(&ModulePathVec);
unwind0:
	/* and error occurred of some type */
	if (g_retval ==  ~TCL_OK)  /* deal with help and version special case */
	  g_retval = 0;
	else
	  g_retval = (g_retval ? g_retval : 1);
	OutputExit();
	return (g_retval);

} /** End of 'main' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		module_usage					     **
 ** 									     **
 **   Description:	Lists out the helpful usage info that we've all come **
 ** 			to expect from unix commands.			     **
 ** 									     **
 **   First Edition:	2002/10/13					     **
 ** 									     **
 **   Parameters:	FILE	*output		Where the output goes	     **
 ** 									     **
 **   Result:		void			No return values	     **
 ** 									     **
 **   Attached Globals:							     **
 ** 			version_string		Current module version	     **
 ** 			date_string		Current module date	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void module_usage(FILE *output)
{
    /**
     **  General help wanted.
     **/
	fprintf(output,
		_("\n  Modules Release %s %s (Copyright GNU GPL v2 1991):\n\n"),
                version_string,date_string);
	
	fprintf(output,
/* TRANSLATORS: keep the options and formatting the same */
_(
"  Usage: module [ switches ] [ subcommand ] [subcommand-args ]\n\n"
"Switches:\n"
"	-H|--help		this usage info\n"
"	-V|--version		modules version & configuration options\n"
"	-f|--force		force active dependency resolution\n"
"	-t|--terse		terse    format avail and list format\n"
"	-l|--long		long     format avail and list format\n"
"	-h|--human		readable format avail and list format\n"
"	-v|--verbose		enable  verbose messages\n"
"	-s|--silent		disable verbose messages\n"
"	-i|--icase		case insensitive\n"
"	-u|--userlvl <lvl>	set user level to (nov[ice],exp[ert],adv[anced])\n"
"  Available SubCommands and Args:\n"
"	+ add|load		modulefile [modulefile ...]\n"
"	+ rm|unload		modulefile [modulefile ...]\n"
"	+ switch|swap		[modulefile1] modulefile2\n"
"	+ display|show		modulefile [modulefile ...]\n"
"	+ avail			[modulefile [modulefile ...]]\n"
"	+ use [-a|--append]	dir [dir ...]\n"
"	+ unuse			dir [dir ...]\n"));
#ifdef BEGINENV
	fprintf(output,_(
"	+ update\n"));
#endif
	fprintf(output,_(
"	+ refresh\n"
"	+ purge\n"
"	+ list\n"
"	+ clear\n"
"	+ help			[modulefile [modulefile ...]]\n"
"	+ whatis		[modulefile [modulefile ...]]\n"
"	+ apropos|keyword	string\n"
"	+ initadd		modulefile [modulefile ...]\n"
"	+ initprepend		modulefile [modulefile ...]\n"
"	+ initrm		modulefile [modulefile ...]\n"
"	+ initswitch		modulefile1 modulefile2\n"
"	+ initlist\n"
"	+ initclear\n\n"));

} /** End of 'module_usage' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		Tcl_AppInit					     **
 ** 									     **
 **   Description:	This is needed if you use shared TCL libraries.	     **
 **			It won't be called, but the linker complains if it   **
 **		   	doesn't exist.					     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	Tcl_Interp	*interp		Tcl interpreter to   **
 **							be initialized	     **
 **   Result:		int		TCL_OK		Initialization succ. **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int Tcl_AppInit(Tcl_Interp *interp)
{
    return( TCL_OK);

} /** End of 'Tcl_AppInit' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		Check_Switches					     **
 ** 									     **
 **   Description:	Check for command line switches and set internal     **
 **			control variable according to them. Command line     **
 **			switches are defined to appear between the shell and **
 **			the module command. They begin on a dash and may     **
 **			appear in long and short format. 		     **
 **									     **
 **			The following switches are defined:		     **
 **									     **
 **			    --force, -f		Force prerequired actions    **
 **			    --terse, -t		Terse, parseable messages    **
 **			    --human, -h		Human readable form          **
 **			    --long, -l		Long messages		     **
 **			    --verbose, -v	Verbose mode on		     **
 **			    --silent, -s	Verbose mode off	     **
 **			    --userlvl, -u	Change the user level	     **
 **			    --icase, -i		Ignore case of modulefile    **
 **						names			     **
 **			    --help, -H		Helpful usage info	     **
 **			    --version, -V	Report version only	     **
 ** 									     **
 **   First Edition:	1995/12/20					     **
 ** 									     **
 **   Parameters:	int	*argc		Number of parameters	     **
 **			char	*argv[]		Command line arguments	     **
 **									     **
 **   Result:		int	TCL_OK		Successful completion	     **
 **				TCL_ERROR	Unknown switch found	     **
 **			*argc, *argv		All switches are removed     **
 **						from the argv vector	     **
 ** 									     **
 **   Attached Globals:	sw_force		--force, -f		     **
 **			sw_format		-t, -l, -h, -p               **
 **			sw_verbose		--verbose, -v, --silent, -s  **
 **			sw_userlvl		--userlvl, -u		     **
 **			sw_icase		--icase, -i		     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static int	Check_Switches( int *argc, char *argv[])
{

    int c;

    /**
     **  These are the options we do provide
     **/

    const struct option longopts[] = {
	{ "force", no_argument, NULL, 'f' },
	{ "terse", no_argument, NULL, 't' },
	{ "long", no_argument, NULL, 'l' },
	{ "human", no_argument, NULL, 'h' },
	{ "parseable", no_argument, NULL, 'p' },
	{ "parse", no_argument, NULL, 'p' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "silent", no_argument, NULL, 's' },
	{ "icase", no_argument, NULL, 'i' },
	{ "userlvl", required_argument, NULL, 'u'},
	{ "append", no_argument, NULL, 'a' },
	{ "help", no_argument, NULL, 'H' },
	{ "version", no_argument, NULL, 'V' },
	{ NULL, no_argument, NULL, 0 }
    };

    /**
     **  Scan the command line for options defined in the longopt table.
     **  Skip the very first argument, which is the shell to be used
     **/
    if( *argc > 1) {

	while( EOF != (c = getopt_long( *argc-1, &argv[1], "hpftlvsciu:aHV",
	    longopts, NULL))) {

	    switch( c) {

		/**
		 **  Force
		 **/

		case 'f':			/* force */
		    sw_force = 1;
		    break;

		/**
		 **  Format of the messages
		 **/

		case 't':			/* terse */
		    sw_format |= (SW_SET | SW_TERSE);
		    sw_format &= ~ SW_LONG;
		    break;

		case 'l':			/* long */
		    sw_format |= (SW_SET | SW_LONG);
		    sw_format &= ~ SW_TERSE;
		    break;

		case 'h':			/* human */
		    sw_format |= (SW_SET | SW_HUMAN);
		    sw_format &= ~ SW_PARSE;
		    break;

		case 'p':			/* parseable */
		    sw_format |= (SW_SET | SW_PARSE);
		    sw_format &= ~ SW_HUMAN;
		    break;

		/**
		 **  Verbosity
		 **/

		case 'v':			/* verbose */
		    sw_verbose = 1;
		    break;

		case 's':			/* silent */
		    sw_detach = 1;
		    break;

		/**
		 **  Locating
		 **/

		case 'i':			/* icase */
		    sw_icase = 1;
		    break;

		/**
		 **  The user level comes as a string argument to the -u option
		 **/

		case 'u':			/* userlvl */
		    cmdModuleUser_sub( optarg);
		    break;

		/**
		 **  a special purpose flag for 'use' only
		 **/
		case 'a':			/* --append */
		    append_flag = 1;
		    break;

		case 'H':			/* helpful info */
		    module_usage(stderr);
		    return ~TCL_OK;

		case 'V':			/* version */
		    version(stderr);
		    return ~TCL_OK;

		/**
		 **  All remaining options will influence their flags as defined
		 **  in the optlong table above.
		 **/

		case 0:
		    break;

		/**
		 **  Error messages for unknown options will be printed by
		 **  getopt ...
		 **/

		case '?':
		    break;

		/**
		 **  Well, this seems to be an internal error
		 **/

		default:
		    if( OK != ErrorLogger( ERR_GETOPT, LOC, NULL))
			return( TCL_ERROR);	/** --- EXIT (FAILURE) ----> **/
		    break;

	    }  /** switch() **/
	}  /** while() **/
    } /** if( argc) **/

    /**
     **  Special things to be dine for the 'silent' option: Pipe stderr
     **  output to /dev/null
     **/

    if( sw_detach) {
	sw_verbose = 0;
	if (!ttyname(2)) {
	    int temp_fd = open("/dev/null", O_RDWR);

	    close(2);
	    dup2(temp_fd, 2);
	}
    }

    /**
     **  Finally remove all options from the command line stream
     **/

    c = optind - 1;
    if( optind < *argc && c > 0) {
	while( optind < *argc) {
	    argv[ optind-c] = argv[ optind];
	    optind++;
	}
	*argc -= c;
    }

    /**
     **  Exit on success
     **/

    return( TCL_OK);

} /** End of 'Check_Switches' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		dup2						     **
 ** 									     **
 **   Description:	Duplicate file descriptor			     **
 ** 									     **
 **   First Edition:	1996/02/08					     **
 ** 									     **
 **   Parameters:	int	old		Old file descriptor	     **
 **			int	new		New file descriptor	     **
 **									     **
 **   Result:		int	-1		any error		     **
 **				other		new file descriptor	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

#ifndef HAVE_DUP2
int dup2( int old, int new)
{
    int fd;

    close(new);
    fd = dup(old);
    if (fd != new) {
	close( fd);
	errno = EMFILE;
	fd = -1;
    }

    return( fd);
}
#endif

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		version						     **
 ** 									     **
 **   Description:	Outputs the Modules version and features	     **
 ** 									     **
 **   First Edition:	2002/06/13					     **
 ** 									     **
 **   Parameters:	FILE *	output		All input is from defined    **
 **						macros			     **
 **   Result:		void			no return value		     **
 **						All output is to output	     **
 ** 									     **
 **   Attached Globals:							     **
 ** 			version_string		Current module version	     **
 ** 			date_string		Current module date	     **
 **									     **
 ** ************************************************************************ **
 ++++*/

#define str(a) #a
#define isdefined(a,b)	{if (strcmp(str(a),b)) x=str(a); else x="undef"; \
			fprintf(output,format,b,x);}

static void version (FILE *output) {
	char	*x,
		*format = "%s=%s\n";

	fprintf(output, format, _("VERSION"), version_string);
	fprintf(output, format, _("DATE"), date_string);
	fprintf(output, "\n");

	isdefined(AUTOLOADPATH,str(AUTOLOADPATH));
	isdefined(BEGINENV,str(BEGINENV));
	isdefined(DEFAULTPATH,str(DEFAULTPATH));
	isdefined(DEF_COLLATE_BY_NUMBER,str(DEF_COLLATE_BY_NUMBER));
	isdefined(DOT_EXT,str(DOT_EXT));
	isdefined(ENABLE_NLS,str(ENABLE_NLS));
	isdefined(EVAL_ALIAS,str(EVAL_ALIAS));
	isdefined(HAS_BOURNE_ALIAS,str(HAS_BOURNE_ALIAS));
	isdefined(HAS_BOURNE_FUNCS,str(HAS_BOURNE_FUNCS));
	isdefined(HAS_TCLXLIBS,str(HAS_TCLXLIBS));
	isdefined(HAS_X11LIBS,str(HAS_X11LIBS));
	isdefined(INITPATH,str(INITPATH));
	isdefined(LMSPLIT_SIZE,str(LMSPLIT_SIZE));
	isdefined(PREFIX,str(PREFIX));
	isdefined(SYSCONFDIR,str(SYSCONFDIR));
	isdefined(TCL_PATCH_LEVEL,str(TCL_PATCH_LEVEL));
	isdefined(TCL_VERSION,str(TCL_VERSION));
	isdefined(TMP_DIR,str(TMP_DIR));
	isdefined(USE_FREE,str(USE_FREE));
	isdefined(VERSION_MAGIC,str(VERSION_MAGIC));
	isdefined(VERSIONPATH,str(VERSIONPATH));
	isdefined(WANTS_VERSIONING,str(WANTS_VERSIONING));

  	fprintf(output, "\n");
}

#undef str
#undef isdefined