/*
 * de/repriv - move ownership of a path/tree into a specified range
 * 
 * Copyright (c) 2022, Mason Loring Bliss. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mason Loring Bliss.
 * 4. The name of the copyright holder may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* On BSD, use getprogname(3). On GNU/Linux, instead use
 * program_invocation_short_name(3). Shame this isn't standardized. */
#if __linux__
#define _GNU_SOURCE
#define MYNAME program_invocation_short_name
#else
#define MYNAME getprogname()
#endif

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FOOLISHLYLONGPATHMAX 65535
#define FOOTCANNONCOUNT 19
#define FOOTCANNONMAX 13

char footcannons[FOOTCANNONCOUNT][FOOTCANNONMAX] = {
    "/",
    "/bin",
    "/boot",
    "/dev",
    "/etc",
    "/home",
    "/lib",
    "/lib64",
    "/proc",
    "/root",
    "/sbin",
    "/sys",
    "/tmp",
    "/usr",
    "/var",
    "/usr/bin",
    "/usr/lib",
    "/usr/libexec",
    "/usr/sbin"
};

int depriv;
char *pathname;
uid_t uidbase;
gid_t gidbase;

void usage(void);
void processpath(size_t);

int main(int argc, char *argv[])
{
    int i;
    size_t length;

    /* Behaviour depends on name. */
    if (strcmp("depriv", MYNAME) == 0) {
        depriv = 1;
    } else if (strcmp("repriv", MYNAME) == 0) {
        depriv = 0;
    } else {
        fprintf(stderr,
                "Error: program must be named one of 'depriv' or 'repriv'\n");
        exit(1);
    }

    /* Two added arguments should be path and UID. Three should be path,
     * UID, and GID. If we aren't given a GID, use the UID value for GID. */
    if (argc < 3 || argc > 4) {
        usage();
    }

    /* Seed pathname. Length includes trailing null. */
    length = strlen(argv[1]) + 1;
    if (length > FOOLISHLYLONGPATHMAX) {
        fprintf(stderr, "Error: path too long: %s.\n", argv[1]);
        exit(1);
    }
    pathname = malloc(FOOLISHLYLONGPATHMAX);
    if (pathname == NULL) {
        fprintf(stderr, "Error: we couldn't allocate %u bytes\n",
                FOOLISHLYLONGPATHMAX);
        exit(1);
    }
    memset(pathname, 0, FOOLISHLYLONGPATHMAX);
    strncpy(pathname, argv[1], length);

    /* Try to avoid some of the more obvious footcannons. */
    for (i = 0; i < FOOTCANNONCOUNT; ++i) {
        if (strcmp(footcannons[i], pathname) == 0) {
            fprintf(stderr, "Error: unwilling to operate on %s.\n", pathname);
            exit(1);
        }
    }

    /* New UID */
    if (sscanf(argv[2], "%u", &uidbase) != 1) {
        fprintf(stderr, "Error: %s is not a number\n", argv[2]);
        exit(1);
    }

    /* New GID if given */
    if (argc == 4) {
        if (sscanf(argv[3], "%u", &gidbase) != 1) {
            fprintf(stderr, "Error: %s is not a number\n", argv[3]);
            exit(1);
        }
    } else {
        /* Same as UID if not */
        gidbase = uidbase;
    }

    /* Recursively modify descendants. */
    processpath(length);

    exit(0);
}

void usage(void)
{
    printf("Usage: %s [directory] [uid [gid]]\n", MYNAME);
    exit(1);
}

void processpath(size_t length)
{
    struct stat sb;
    uid_t newuid = -1;
    gid_t newgid = -1;
    DIR *directory;
    struct dirent *entry;
    size_t newlength;

    /* if we can't stat, there's nothing else to do for this */
    if (lstat(pathname, &sb) == -1) {
        fprintf(stderr, "Error: could not lstat(2) %s: %s\n", pathname,
                strerror(errno));
        return;
    }

    /* set ownership for all file types */
    if (depriv) {
        if (sb.st_uid < uidbase) {
            newuid = sb.st_uid + uidbase;
        }
        if (sb.st_gid < gidbase) {
            newgid = sb.st_gid + gidbase;
        }
    } else {
        /* repriv */
        newuid = sb.st_uid - uidbase;
        newgid = sb.st_gid - gidbase;
    }
    if (newuid != -1 || newgid != -1) {
        if (lchown(pathname, newuid, newgid) == -1) {
            fprintf(stderr, "Error: could not lchown(2) %s: %s\n", pathname,
                    strerror(errno));
        }
    }

    /* set mode if not a symlink */
    if ((sb.st_mode & S_IFMT) != S_IFLNK) {
        if (chmod(pathname, sb.st_mode) == -1) {
            fprintf(stderr, "Error: could not chmod(2) %s: %s\n", pathname,
                    strerror(errno));
        }
    }

    /* process entries if a directory */
    if ((sb.st_mode & S_IFMT) == S_IFDIR) {
        directory = opendir(pathname);
        if (length < FOOLISHLYLONGPATHMAX) {
            /* replace trailing null with directory separator */
            *(pathname + length - 1) = '/';
            *(pathname + length) = '\0';
        } else {
            fprintf(stderr, "Error: path too long below: %s\n",
                    pathname);
            return;
        }
        while ((entry = readdir(directory)) != NULL) {
            if (strcmp(".", entry->d_name) == 0
                    || strcmp("..", entry->d_name) == 0) {
                continue;
            }
            /* Path too long? */
            newlength = strlen(entry->d_name) + 1;
            /* remember, length's terminating null is now a / */
            if ((length + newlength) > FOOLISHLYLONGPATHMAX) {
                fprintf(stderr, "Error: path too long: %s%s\n",
                        pathname, entry->d_name);
            } else {
                strncpy(pathname + length, entry->d_name, newlength);
                processpath(length + newlength);
                /* trim back to where we were before this call */
                *(pathname + length) = '\0';
            }
        }
        closedir(directory);
    }
}
