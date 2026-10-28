/*
 * POSIX calls newlib on the Vita does not provide, as stubs that fail the way an unsupported call
 * does. glib references them from code paths the port never takes - spawning processes, child
 * watches, Unix signals, the user database - but the references still have to link.
 */
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


pid_t waitpid(pid_t pid, int *status, int options) { (void)pid; (void)status; (void)options; errno = ECHILD; return -1; }

int execv(const char *path, char *const argv[]) { (void)path; (void)argv; errno = ENOSYS; return -1; }

int dup2(int oldfd, int newfd) { (void)oldfd; (void)newfd; errno = ENOSYS; return -1; }

pid_t getppid(void) { return 1; }

int creat(const char *path, mode_t mode) { (void)path; (void)mode; errno = ENOSYS; return -1; }

int sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{
	(void)sig; (void)act;
	if (old) {
		/* Report "default action" so callers saving and restoring a handler have something sane. */
		old->sa_handler = SIG_DFL;
	}
	return 0;
}

struct passwd *getpwuid(uid_t uid) { (void)uid; errno = ENOENT; return NULL; }

int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t len, struct passwd **result)
{
	(void)name; (void)pwd; (void)buf; (void)len;
	*result = NULL;
	return 0;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t len, struct passwd **result)
{
	(void)uid; (void)pwd; (void)buf; (void)len;
	*result = NULL;
	return 0;
}

int nftw(const char *path, int (*fn)(const char *, const struct stat *, int, struct FTW *), int fds, int flags)
{
	(void)path; (void)fn; (void)fds; (void)flags;
	errno = ENOSYS;
	return -1;
}

/* Stdio stream locking: newlib declares it, the Vita's libc does not have it. The one user, Pango's
 * line reader, is not shared between threads. */
void flockfile(FILE *file) { (void)file; }

void funlockfile(FILE *file) { (void)file; }
