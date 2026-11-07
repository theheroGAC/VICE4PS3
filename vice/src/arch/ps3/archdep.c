

#include "vice.h"

#include "archdep.h"
#include "ioutil.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "util.h"
#include "keyboard.h"
#include "joy.h"
#include "app_defs.h"
#include "debug_ps3.h"
#include "ps3_paths.h"

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <ps3_thread.h> 
#include <ps3_io.h> 


#ifdef HAVE_VFORK_H
#include <vfork.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

static char *argv0 = NULL;
static char *boot_path = NULL;
static char *home_dir = NULL;
static char *program_name = NULL;
static char *vice_resource_dir = NULL;



const char *archdep_pref_path = NULL;

static FILE *log_file = NULL;


#define TOTAL_PATHS 16


static char *sysfile_path = NULL;

#define SCE_ERROR_ERRNO_EEXIST 0x80010011

int archdep_init(int *argc, char **argv)
{
    argv0 = lib_stralloc(APP_NAME);

    log_file = fopen("/dev_hdd0/vice/vice_core_log.txt", "w");
    if (log_file == NULL) {
        log_error(LOG_ERR, "failed to open log file '/dev_hdd0/vice/vice_core_log.txt' for writing");
    } else {
        setvbuf(log_file, NULL, _IONBF, 0);
    }

    return 0;
}

const char *archdep_boot_path(void)
{
    if (boot_path == NULL) {
		boot_path = lib_stralloc(VICE_DIR);
    }

    return boot_path;
} 

const char *archdep_home_path(void)
{
	if (home_dir == NULL) {
		home_dir = lib_stralloc(VICE_DIR);
	}

    return home_dir;
}

char *archdep_default_autostart_disk_image_file_name(void)
{
    return util_concat(archdep_home_path(),
						ARCHDEP_AUTOSTART_DISKIMAGE_PREFIX,
						machine_get_name(),
						ARCHDEP_AUTOSTART_DISKIMAGE_SUFFIX,
						NULL);
}


char *archdep_default_sysfile_pathlist(const char *emu_id)
{	const char *base_path = ps3_base_dir();
	const char *resource_path = archdep_vice_resource_path();
	
    char *lib_root = NULL;
    char *lib_machine_roms = NULL;
    char *lib_drive_roms = NULL;
    char *lib_printer_roms = NULL;
    char *boot_root = NULL;
    char *boot_machine_roms = NULL;
    char *boot_drive_roms = NULL;
    char *boot_printer_roms = NULL;
    char *home_root = NULL;
    char *home_machine_roms = NULL;
    char *home_drive_roms = NULL;
    char *home_printer_roms = NULL;

    const char *paths[TOTAL_PATHS + 1];
    int i;


    if (sysfile_path != NULL) {
        
        return lib_stralloc(sysfile_path);
    }

    
    for (i = 0; i <= TOTAL_PATHS; i++) {
        paths[i] = NULL;
    }	if (base_path == NULL || base_path[0] == '\0') {
        base_path = "/dev_hdd0/game/VICEP3001/USRDIR";
    }
    home_machine_roms = archdep_join_paths(base_path, resource_path, emu_id, NULL);
    home_drive_roms = archdep_join_paths(base_path, resource_path, "DRIVES", NULL);
    home_printer_roms = archdep_join_paths(base_path, resource_path, "PRINTER", NULL);

    
    i = 0;

    
    if (lib_root != NULL) {
        paths[i++] = lib_root;
    }
    if (lib_machine_roms != NULL) {
        paths[i++] = lib_machine_roms;
    }
    if (lib_drive_roms != NULL) {
        paths[i++] = lib_drive_roms;
    }
    if (lib_printer_roms != NULL) {
        paths[i++] = lib_printer_roms;
    }
    
    if (boot_root != NULL) {
        paths[i++] = boot_root;
    }
    if (boot_machine_roms != NULL) {
        paths[i++] = boot_machine_roms;
    }
    if (boot_drive_roms != NULL) {
        paths[i++] = boot_drive_roms;
    }
    if (boot_printer_roms != NULL) {
        paths[i++] = boot_printer_roms;
    }
	
    
    if (home_root != NULL) {
        paths[i++] = home_root;
    }
    if (home_machine_roms != NULL) {
        paths[i++] = home_machine_roms;
    }
    if (home_drive_roms != NULL) {
        paths[i++] = home_drive_roms;
    }
    if (home_printer_roms != NULL) {
        paths[i++] = home_printer_roms;
    }
	
    
    paths[i] = NULL;
    sysfile_path = util_strjoin(paths, ARCHDEP_FINDPATH_SEPARATOR_STRING);
	
    
    
    if (lib_root != NULL) {
        lib_free(lib_root);
    }
    if (lib_machine_roms != NULL) {
        lib_free(lib_machine_roms);
    }
    if (lib_drive_roms != NULL) {
        lib_free(lib_drive_roms);
    }
    if (lib_printer_roms != NULL) {
        lib_free(lib_printer_roms);
    }
    
    if (boot_root != NULL) {
        lib_free(boot_root);
    }
    if (boot_machine_roms != NULL) {
        lib_free(boot_machine_roms);
    }
    if (boot_drive_roms != NULL) {
        lib_free(boot_drive_roms);
    }
    if (boot_printer_roms != NULL) {
        lib_free(boot_printer_roms);
    }
	
    
    if (home_root != NULL) {
        lib_free(home_root);
    }
    if (home_machine_roms != NULL) {
        lib_free(home_machine_roms);
    }
    if (home_drive_roms != NULL) {
        lib_free(home_drive_roms);
    }
    if (home_printer_roms != NULL) {
        lib_free(home_printer_roms);
    }
	
    archdep_startup_log_error("Search path = %s\n", sysfile_path);
    
    return lib_stralloc(sysfile_path);
}


char *archdep_make_backup_filename(const char *fname)
{
    return util_concat(fname, "~", NULL);
}

char *archdep_default_resource_file_name(void)
{
	const char *cfg;
	cfg = archdep_home_path();
    return archdep_join_paths(cfg, ARCHDEP_VICERC_NAME, NULL);
}

char *archdep_default_fliplist_file_name(void)
{
	char *name;
    char *path;

    name = util_concat("fliplist-", machine_get_name(), ".vfl", NULL);
    path = archdep_join_paths(archdep_home_path(), name, NULL);
    lib_free(name);

    return path;
}

int archdep_num_text_lines(void)
{
    char *s;

    s = getenv("LINES");
    if (s == NULL) {
        printf("No LINES!\n");
        return -1;
    }
    return atoi(s);
}

int archdep_num_text_columns(void)
{
    char *s;

    s = getenv("COLUMNS");
    if (s == NULL)
        return -1;
    return atoi(s);
}

int archdep_default_logger(const char *level_string, const char *txt) {
    if (fputs(level_string, stdout) == EOF
        || fprintf(stdout, txt) < 0
        || fputc ('\n', stdout) == EOF)
        return -1;
    return 0;
}

int archdep_path_is_relative(const char *path)
{
	if (path == NULL){
        return 0;
    }
    if (path[0] == '/') {
        return 0;
    }

	return strchr(path, ':') == NULL;
}

int archdep_spawn(const char *name, char **argv,
                  char **pstdout_redir, const char *stderr_redir)
{
#ifndef HAVE_VFORK
    return -1;
#else
    pid_t child_pid;
    int child_status;
    char *stdout_redir = NULL;

    if (pstdout_redir != NULL) {
        if (*pstdout_redir == NULL)
            *pstdout_redir = archdep_tmpnam();
        stdout_redir = *pstdout_redir;
    }

    child_pid = vfork();
    if (child_pid < 0) {
        log_error(LOG_DEFAULT, "vfork() failed: %s.", strerror(errno));
        return -1;
    } else {
        if (child_pid == 0) {
            if (stdout_redir && freopen(stdout_redir, "w", stdout) == NULL) {
                log_error(LOG_DEFAULT, "freopen(\"%s\") failed: %s.",
                          stdout_redir, strerror(errno));
                _exit(-1);
            }
            if (stderr_redir && freopen(stderr_redir, "w", stderr) == NULL) {
                log_error(LOG_DEFAULT, "freopen(\"%s\") failed: %s.",
                          stderr_redir, strerror(errno));
                _exit(-1);
            }
            execvp(name, argv);
            _exit(-1);
        }
    }

    if (waitpid(child_pid, &child_status, 0) != child_pid) {
        log_error(LOG_DEFAULT, "waitpid() failed: %s", strerror(errno));
        return -1;
    }

    if (WIFEXITED(child_status))
        return WEXITSTATUS(child_status);
    else
        return -1;
#endif
}


int archdep_expand_path(char **return_path, const char *orig_name)
{
    
    if (*orig_name == '/') {
        *return_path = lib_stralloc(orig_name);
    } else {
        static char *cwd;

        cwd = ioutil_current_dir();
        *return_path = util_concat(cwd, "/", orig_name, NULL);
        lib_free(cwd);
    }
    return 0;
}

void archdep_startup_log_error(const char *format, ...)
{
    va_list ap;
    FILE *f;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    f = fopen("/dev_hdd0/vice/vice_log.txt", "a");
    if (f != NULL) {
        va_start(ap, format);
        vfprintf(f, format, ap);
        va_end(ap);
        fclose(f);
    }
}

char *archdep_filename_parameter(const char *name)
{
    
    return lib_stralloc(name);
}

char *archdep_quote_parameter(const char *name)
{
    
    return lib_stralloc(name);
}

char *archdep_tmpnam(void)
{
#ifdef GP2X
    static unsigned int tmp_string_counter=0;
    char tmp_string[32];

    sprintf(tmp_string,"vice%d.tmp",tmp_string_counter++);
    return lib_stralloc(tmp_string);
#else
#ifdef HAVE_MKSTEMP
    char *tmpName;
    const char mkstempTemplate[] = "/vice.XXXXXX";
    int fd;
    char* tmp;

    tmpName = (char *)lib_malloc(ioutil_maxpathlen());
    if ((tmp = getenv("TMPDIR")) != NULL ) {
        strncpy(tmpName, tmp, ioutil_maxpathlen());
        tmpName[ioutil_maxpathlen() - sizeof(mkstempTemplate)] = '\0';
    }
    else
        strcpy(tmpName, "/tmp" );
    strcat(tmpName, mkstempTemplate );

    if ((fd = mkstemp(tmpName)) < 0 )
        tmpName[0] = '\0';
    else
        close(fd);

    lib_free(tmpName);
    return lib_stralloc(tmpName);
#else
    return lib_stralloc(tmpnam(NULL));
#endif
#endif
}

FILE *archdep_mkstemp_fd(char **filename, const char *mode)
 {
    static unsigned int tmp_string_counter = 0;
    char *tmp;
    FILE *fd;

    tmp = lib_msprintf("vice%d.tmp", tmp_string_counter++);

    fd = fopen(tmp, mode);

    if (fd == NULL) {
        lib_free(tmp);
        return NULL;
    }

    *filename = tmp;

    return fd;
}

int archdep_file_is_gzip(const char *name)
{
    size_t l = strlen(name);

    if ((l < 4 || strcasecmp(name + l - 3, ".gz"))
        && (l < 3 || strcasecmp(name + l - 2, ".z"))
        && (l < 4 || toupper(name[l - 1]) != 'Z' || name[l - 4] != '.'))
        return 0;
    return 1;
}

int archdep_file_set_gzip(const char *name)
{
    return 0;
}

int archdep_mkdir(const char *pathname, int mode)
{
    return mkdir(pathname, (mode_t)mode);
}

int archdep_stat(const char *file_name, unsigned int *len, unsigned int *isdir)
{
    struct stat statbuf;

    if (stat(file_name, &statbuf) < 0)
        return -1;

    *len = statbuf.st_size;
    *isdir = S_ISDIR(statbuf.st_mode);

    return 0;
}

int archdep_file_is_blockdev(const char *name)
{
    struct stat buf;

    if (stat(name, &buf) != 0)
        return 0;

    if (S_ISBLK(buf.st_mode))
        return 1;

    return 0;
}

int archdep_file_is_chardev(const char *name)
{
    struct stat buf;

    if (stat(name, &buf) != 0)
        return 0;

    if (S_ISCHR(buf.st_mode))
        return 1;

    return 0;
}

void archdep_shutdown(void)
{
	log_message(LOG_DEFAULT, "\nExiting...");
    lib_free(argv0);
    lib_free(boot_path);
#ifdef PS3_DEBUG_CODE
    if (log_file) fclose(log_file);
#endif 
}

signed long kbd_arch_keyname_to_keynum(char *keyname) {
	return (signed long)atoi(keyname);
}

const char *kbd_arch_keynum_to_keyname(signed long keynum) {
	static char keyname[20];

	memset(keyname, 0, 20);
	sprintf(keyname, "%li", keynum);
	return keyname;
}

void kbd_arch_init()
{
	keyboard_clear_keymatrix();
}

int kbd_arch_get_host_mapping(void)
{
    return KBD_MAPPING_US;
} 

int joy_arch_init(void)
{
    return 0;
}

void joystick_close(void)
{
}

void kbd_initialize_numpad_joykeys(int* joykeys)
{
}

int joy_arch_cmdline_options_init(void)
{
    return 0;
}
 
int joy_arch_set_device(int port_idx, int joy_dev)
{
    return 0;
} 

int joy_arch_resources_init(void)
{
    return 0;
}

char *archdep_extra_title_text(void)
{
    return NULL;
} 

char *archdep_default_rtc_file_name(void)
{
    const char *home;
	home = archdep_home_path();
    return util_concat(home, "vice.rtc", NULL);
} 

int archdep_rename(const char *oldpath, const char *newpath)
{
    return rename(oldpath, newpath);
} 


int archdep_rmdir(const char *pathname)
{
    return rmdir(pathname);
} 

int archdep_rtc_get_centisecond(void)
{

#if defined(__QNX__) && !defined(__QNXNTO__)
    struct timespec dtm;
    int status;

    if ((status = clock_gettime(CLOCK_REALTIME, &dtm)) == 0) {
        return dtm.tv_nsec / 10000L;
    }
    return 0;
#endif

}

int archdep_vice_atexit(void (*function)(void))
{
    return atexit(function);
}

void archdep_vice_exit(int excode)
{
    exit(excode);
}

int archdep_register_cbmfont(void)
{
    
   return 0;
}

void archdep_unregister_cbmfont(void)
{
}


char *archdep_join_paths(const char *path, ...)
{
    const char *arg;
    char *result;
    char *endptr;
    size_t result_len;
    size_t len;
    va_list ap;
#if 0
    printf("%s: first argument: '%s'\n", __func__, path);
#endif
    
    if (path == NULL) {
        return NULL;
    }

    
    va_start(ap, path);
    result_len = strlen(path);
    while ((arg = va_arg(ap, const char *)) != NULL) {
        result_len += (strlen(arg) + 1);
    }
    va_end(ap);
#if 0
    
    printf("%s: result length: %lu\n", __func__, (unsigned long)result_len);
#endif
    
    result = lib_calloc(result_len + 1, 1);
    strcpy(result, path);
    endptr = result + (ptrdiff_t)strlen(path);

    
    va_start(ap, path);
    while ((arg = va_arg(ap, const char *)) != NULL) {
#if 0
        printf("%s: adding '%s' to the result.", __func__, arg);
#endif
        len = strlen(arg);
        *endptr++ = ARCHDEP_DIR_SEPARATOR;
        memcpy(endptr, arg, len + 1);
        endptr += (ptrdiff_t)len;
    }

    va_end(ap);
    return result;
}



FILE *archdep_open_default_log_file(void)
{
	 return log_file? log_file: NULL; 
}

const char* archdep_program_name(void)
{
    if (program_name == NULL) {
        char *p;

        p = strrchr(argv0, '/');
        if (p == NULL)
            program_name = lib_stralloc(argv0);
        else
            program_name = lib_stralloc(p + 1);
    }

    return program_name;
}
 
void archdep_program_name_free(void)
{
    if (program_name != NULL) {
        lib_free(program_name);
        program_name = NULL;
    }
}

char *archdep_vice_resource_path(void)
{
    if (vice_resource_dir == NULL) {
		vice_resource_dir = lib_stralloc(APP_RESOURCES);
	}

    return vice_resource_dir;
}



void archdep_vice_resource_path_free(void)
{
    if (vice_resource_dir != NULL) {
        lib_free(vice_resource_dir);
        vice_resource_dir = NULL;
    }
}

static RETSIGTYPE break64(int sig)
{
    log_message(LOG_DEFAULT, "Received signal %d, exiting.", sig);
    exit (-1);
}

void archdep_signals_init(int do_core_dumps)
{
    if (do_core_dumps) {
        signal(SIGPIPE, break64);
    }
}

typedef void (*signal_handler_t)(int);
static signal_handler_t old_pipe_handler;


void archdep_signals_pipe_set(void)
{
    old_pipe_handler = signal(SIGPIPE, SIG_IGN);
}

void archdep_signals_pipe_unset(void)
{
    signal(SIGPIPE, old_pipe_handler);
}
 




char* getwd(char *buffer)
{
	
	if (buffer){
		strcpy(buffer, VICE_DIR);
		return buffer;
	}
	
	return NULL;
}

int chdir(const char *path)
{
	(void)path;
	return 0;
}

int usleep(useconds_t usec)
{
    sceKernelDelayThread(usec);
    return 0;
}
