/*
 * $Id: tapeops.cpp,v 1.1 2008/07/08 16:26:43 philipn Exp $
 *
 * Provides tape I/O functions
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if ! defined(_LARGEFILE_SOURCE) && ! defined(_FILE_OFFSET_BITS)
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/mtio.h>

#include "logF.h"
#include "tapeops.h"

//
// Code based on practices found in cpio-2.6/src/mt.c
//
// Other useful tape programs are:
//  mtst (mt_st package), tapeinfo (mtx package)
//

#define TAPEBLOCK 512*512           // i.e. 256kB blocksize

static int verbose = 0;

bool rewind_tape(const char *device)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        logTF("rewind_tape: Failed to open device %s: %s\n", device, strerror(errno));
        return false;
    }

    // Perform a rewind operation
    struct mtop control;
    control.mt_op = MTREW;
    control.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        logTF("MTIOCTOP mt_op=MTREW failed: %s\n", strerror(errno));
        close(fd);
        return false;
    }

    logTF("tape rewound\n");
    close(fd);
    return true;
}

bool eject_tape(const char *device)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        logTF("eject_tape: Failed to open device %s: %s\n", device, strerror(errno));
        return false;
    }

    // Perform an eject operation
    struct mtop control;
    control.mt_op = MTOFFL;
    control.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        logTF("MTIOCTOP mt_op=MTOFFL failed: %s\n", strerror(errno));
        close(fd);
        return false;
    }

    logTF("tape ejected\n");
    close(fd);
    return true;
}

// Set block size and compression parameters
// Should be called after tape load since settings are reset after tape load
bool set_tape_params(const char *device)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        logTF("eject_tape: Failed to open device %s: %s\n", device, strerror(errno));
        return false;
    }

    // Perform the setblk operation
    // Equivalent to e.g.
    //   mt -f /dev/nst0 setblk 262144
    struct mtop control;
    control.mt_op = MTSETBLK;
    control.mt_count = TAPEBLOCK;   // block size
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        logTF("MTIOCTOP mt_op=MTSETBLK failed: %s\n", strerror(errno));
        close(fd);
        return false;
    }

    // Turn off drive compression
    control.mt_op = MTCOMPRESSION;
    control.mt_count = 0;           // turn off compression
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        logTF("MTIOCTOP mt_op=MTSETBLK failed: %s\n", strerror(errno));
        close(fd);
        return false;
    }

    logTF("tape blocksize and compression set correctly\n");
    close(fd);
    return true;
}

// File mark seeking. Specify forwards or backwards and number of marks.
bool tape_filemark_seek(const char *device, bool forwards, int num_marks)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        logTF("eject_tape: Failed to open device %s: %s\n", device, strerror(errno));
        return false;
    }

    struct mtop control;

    control.mt_op = MTBSFM;             // Backward space count file marks
    if (forwards)
        control.mt_op = MTFSFM;         // Forward space count file marks
    control.mt_count = num_marks;

    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        logTF("MTIOCTOP mt_op=%s failed: %s\n", forwards ? "MTFSFM" : "MTBSFM", strerror(errno));
        close(fd);
        return false;
    }

    logTF("tape filemark seek %s %d marks\n", forwards ? "MTFSFM" : "MTBSFM", num_marks);
    close(fd);
    return true;
}

bool tape_contains_tar_file(const char *device, string *p)
{
    rewind_tape(device);

    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        logTF("tape_contains_tar_file: Failed to open device %s: %s\n", device, strerror(errno));
        return false;
    }

    // POSIX tar header is 512 bytes
    char buf[TAPEBLOCK];
    ssize_t nread = read(fd, buf, sizeof(buf));
    close(fd);
    if (nread != TAPEBLOCK) {
        logTF("tape_contains_tar_file: read %d instead of TAPEBLOCK\n", (int)nread);
        rewind_tape(device);
        return false;
    }

    // tar magic at offset 257 should be "ustar" followed by space or NUL
    char *pmagic = buf + 257;
    char magic1[6] = {'u','s','t','a','r','\0'};
    char magic2[6] = {'u','s','t','a','r',' '};
    if (memcmp(pmagic, magic1, 6) == 0 || memcmp(pmagic, magic2, 6) == 0) {
        logTF("tape_contains_tar_file: found tar header, first file=%s\n", buf);
        // Does it look like a D3 archive tape, starting with a txt file?

        char *ext;
        if ((ext = strcasestr(buf, ".txt")) != NULL) {
            *ext = '\0';
            // delete trailing 00
            if (strlen(buf) >= 2 && strncmp(ext-2, "00", 2) == 0)
                *(ext-2) = '\0';
            logTF("Found index file, barcode=%s\n", buf);
            *p = buf;
            rewind_tape(device);
            return true;
        }
        else {
            logTF("No index file found, some other kind of tar archive\n");
            rewind_tape(device);
            return true;
        }
    }

    logTF("tape is empty of a tar file\n");
    rewind_tape(device);
    return false;
}

// returns whether tape drive state is understood, including drive failure
TapeDetailedState probe_tape_status(const char *device)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        switch (errno) {
            case EBUSY:
                // E.g. when another process has opened the same device
                // domo: busy when ejecting tape
                // logTF("* Drive busy (ejecting or writing)\n");
                return Busyeject;
                break;
            default:
                // Incorrect device, permission errors expected here
                perror(device);
                return Failed;
                break;
        }
    }

    // Perform a NOP to prepare for getting status
    struct mtop control;
    control.mt_op = MTNOP;
    control.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        // Errors which can happen in order of usefulness of information
        switch (errno) {
            case ENOMEDIUM:
                // domo: no tape
                // logTF("* No tape\n");
                close(fd); return Notape;
                break;
            case EIO:
                // Two cases observed:
                // domo: busy after tape inserted but not ready (still loading)
                // neffie: after drive no longer responds (erase + eject)
                // logTF("* Drive busy (loading) (or possibly drive error)\n");
                close(fd); return Busyload;
                break;
            default:
                perror(device);
                close(fd); return Failed;
                break;
        }
    }

    // Get status
    struct mtget status;
    if (ioctl(fd, MTIOCGET, &status) == -1) {
        perror(device);
        close(fd); return Failed;
    }

    if (verbose) {
        printf ("drive type = %s\n", (status.mt_type == MT_ISSCSI2) ? "SCSI-2 tape" : "not known");
        printf ("drive status = %d 0x%8x\n", (int) status.mt_dsreg, (int) status.mt_dsreg);
    
        printf ("sense key error = %d\n", (int) status.mt_erreg);
        printf ("residue count = %d\n", (int) status.mt_resid);
    
        printf ("file number = %d\n", (int) status.mt_fileno);
        printf ("block number = %d\n", (int) status.mt_blkno);
    }

    if (verbose) {
        if (GMT_EOF(status.mt_gstat))
           printf(" EOF");

        // beginning of tape (after tape load or tape rewind on neffie & domo)
        if (GMT_BOT(status.mt_gstat))   // beginning of tape
         printf(" BOT");

        if (GMT_EOT(status.mt_gstat))
         printf(" EOT");
        if (GMT_SM(status.mt_gstat))
         printf(" SM");
        if (GMT_EOD(status.mt_gstat))
         printf(" EOD");
    }

    // write protect tab set
    if (GMT_WR_PROT(status.mt_gstat)) {
        if (verbose) printf(" WR_PROT\n");
        close(fd); return Writeprotect;
    }

    if (GMT_ONLINE(status.mt_gstat)) {  // tape loaded
        // Experiment on domo showed that a fresh tape load gives a momentary
        // ONLINE state but with fileno and blkno set to -1
        if (status.mt_fileno == -1 && status.mt_blkno == -1) {
            logTF(" (momentary ?) fresh tape loading\n");
            close(fd); return Busyloadmom;
        }
        
        if (verbose) printf(" ONLINE\n");
        close(fd); return Online;
    }

    // If we get here, tape is not ONLINE, NOTAPE or BUSY
    // so we don't know what the state is
    close(fd); return Failed;
}

// After loading tape
//
// drive type = SCSI-2 tape
// drive status = 1140850688 0x44000000
// sense key error = 0
// residue count = 0
// file number = 0
// block number = 0
//  BOT ONLINE IM_REP_EN

// tape4 (erasure state not known)
// tar tvf /dev/nst0
// open("/dev/nst0", O_RDONLY|O_LARGEFILE) = 3
// read(3, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"..., 10240) = 10240
//
// file number = 0
// block number = 1
//  ONLINE IM_REP_EN
//
// tar:
// open("/dev/nst0", O_RDONLY|O_LARGEFILE) = 3
// read(3, "", 10240)                      = 0
//
// file number = 1
// block number = 0
//  EOF ONLINE IM_REP_EN
//
// After load blank fresh tape
//
// * Drive busy (or possibly drive error)
// * No tape
// * No tape
// drive status = 0 0x       0
// sense key error = 1
// residue count = 0
// file number = -1
// block number = -1
//  ONLINE IM_REP_EN
// * Drive busy (or possibly drive error)
// ...
// drive type = SCSI-2 tape
// drive status = 1140850688 0x44000000
// sense key error = 0
// residue count = 0
// file number = 0
// block number = 0
//  BOT ONLINE IM_REP_EN
//
// tar:
// open("/dev/nst0", O_RDONLY|O_LARGEFILE) = 3
// read(3, 0x807d000, 10240)               = -1 EIO (Input/output error)
//
// sense key error = 0
// residue count = 0
// file number = 0
// block number = -1
//  ONLINE IM_REP_EN
//
// insert blank fresh tape with write protect set
// (same as before, including file number = -1 data)
// drive type = SCSI-2 tape
// drive status = 1140850688 0x44000000
// sense key error = 0
// residue count = 0
// file number = 0
// block number = 0
//  BOT WR_PROT ONLINE IM_REP_EN
//
// insert quick-erased tape
// same as true fresh tape except "file number = -1" data not there
