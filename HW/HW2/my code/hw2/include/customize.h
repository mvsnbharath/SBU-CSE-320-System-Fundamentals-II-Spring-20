/*
   This is the customizations file.  It changes our ideas of
   how to read directories.
*/

#define NAMELEN	512		/* max size of a full pathname */

#ifdef BSD
#	include		<sys/dir.h>
#	define	OPEN	DIR
#	define	READ	struct direct
#	define	NAME(x)	((x).d_name)
#endif

#ifdef SCO_XENIX
#	include <sys/ndir.h>
#	define	OPEN	DIR
#	define	READ	struct direct
#	define	NAME(x)	((x).d_name)
#endif

#ifdef SYS_V
 /* Customize this.  This is part of Doug Gwyn's package for */
 /* reading directories.  If you've put this file somewhere */
 /* else, edit the next line. */

#	include		<sys/dirent.h>

#	define	OPEN	struct direct
#	define	READ	struct dirent
#	define	NAME(x)	((x).d_name)
#endif

#ifdef SYS_III
#	define	OPEN	FILE
#	define	READ	struct direct
#	define	NAME(x)	((x).d_name)
#	define	INO(x)	((x).d_ino)

#	include		"direct.c"

#endif

#ifdef LINUX
#   include     <dirent.h>
#   define  OPEN    DIR
#   define  READ    struct dirent
#   define  NAME(x) ((x).d_name)
#   define  INO(x)  ((x).d_ino)
#endif

#if !(defined(BSD) || !defined(SYS_V) || !defined(SYS_III) || !defined(SCO_XENIX) || !defined(LINUX) )
"This is an Error"
#endif


//vtree.c
int is_directory(char *path);
int chk_4_dir(char  *path);
void get_data(char *path,int cont);

//hash.c
extern int h_enter(dev_t dev, ino_t ino);
extern void h_stats();