/*
 * $Id: system_info_web.c,v 1.2 2009/10/12 15:11:31 john_f Exp $
 *
 * Web server to get info on CPU, memory and disk statistics
 *
 * Copyright (C) 2008  British Broadcasting Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <shttpd.h>

typedef struct {
    double user;
    double nice;
    double sys;
    double idle;
    double iowait;
    double irq;
    double sirq;
    double steal;
} CpuInfo;

static CpuInfo cpu = {0};

#define CPU_POLL_INTERVAL 2     // in seconds

static void *cpu_monitor(void *arg)
{
    FILE *fp_stat = (FILE *)arg;

    long long last_user = 0, last_nice = 0, last_sys = 0, last_idle = 0, last_iowait = 0, last_irq = 0, last_sirq = 0, last_steal = 0;

    while (1)
    {
        rewind(fp_stat);

        char buf[16*1024];      // should be enough for 16 CPUs worth of stats

        if (fread(buf, 1, sizeof(buf), fp_stat) < 1) {
            printf("Could not read /proc/stat\n");
            sleep(CPU_POLL_INTERVAL);
            continue;
        }

        // The linux kernel provides these number as unsigned long long,
        // see linux/fs/proc/proc_misc.c for details.
        unsigned long long user, nice, sys, idle, iowait, irq, sirq, steal;
        if (sscanf(buf, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
                        &user, &nice, &sys, &idle, &iowait, &irq, &sirq, &steal) != 8) {
            printf("Could not parse 'cpu' line in /proc/stat\n");
            sleep(CPU_POLL_INTERVAL);
            continue;
        }

        unsigned long long diff_user = (user - last_user);
        unsigned long long diff_nice = (nice - last_nice);
        unsigned long long diff_sys = (sys - last_sys);
        unsigned long long diff_idle = (idle - last_idle);
        unsigned long long diff_iowait = (iowait - last_iowait);
        unsigned long long diff_irq = (irq - last_irq);
        unsigned long long diff_sirq = (sirq - last_sirq);
        unsigned long long diff_steal = (steal - last_steal);

        unsigned long long total_diff = diff_user + diff_nice + diff_sys + diff_idle + diff_iowait + diff_irq + diff_sirq + diff_steal;

        double tf = total_diff;

        // debugs
        //printf("%llu %llu %llu %llu %llu %llu %llu %llu\n", last_user, last_nice, last_sys, last_idle, last_iowait, last_irq, last_sirq, last_steal);
        //printf("%llu %llu %llu %llu %llu %llu %llu %llu, total_diff=%llu\n", user, nice, sys, idle, iowait, irq, sirq, steal, total_diff);
        //printf("%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", diff_user/tf*100.0, diff_nice/tf*100.0, diff_sys/tf*100.0, diff_idle/tf*100.0, diff_iowait/tf*100.0, diff_irq/tf*100.0, diff_sirq/tf*100.0, diff_steal/tf*100.0);

        cpu.user = diff_user/tf*100.0;
        cpu.nice = diff_nice/tf*100.0;
        cpu.sys = diff_sys/tf*100.0;
        cpu.idle = diff_idle/tf*100.0;
        cpu.iowait = diff_iowait/tf*100.0;
        cpu.irq = diff_irq/tf*100.0;
        cpu.sirq = diff_sirq/tf*100.0;
        cpu.steal = diff_steal/tf*100.0;

        last_user = user;
        last_nice = nice;
        last_sys = sys;
        last_idle = idle;
        last_iowait = iowait;
        last_irq = irq;
        last_sirq = sirq;
        last_steal = steal;

        sleep(CPU_POLL_INTERVAL);
    }
    return NULL;
}

// Use /etc/mtab to determine which filesystems are real disk filesystems and
// to ignore network filesystems or special mounts like /proc.
static void disk_filesystem_stats(struct shttpd_arg* arg)
{
    FILE *fp;
    char buf[1024];
    int first_time = 1;

    // Use text/plain instead of application/json for ease of debugging in a browser
    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
    shttpd_printf(arg, "Content-type: text/plain\r\n\r\n");

    // /etc/mtab is maintained by the kernel and lists all mounted filesystems
    if ((fp = fopen("/etc/mtab", "r")) == NULL) {
        shttpd_printf(arg, "fopen failed on \"/etc/mtab\"\n");
        goto finish;
    }

    shttpd_printf(arg, "{\"state\":\"info\",\"monitorData\":{\n");

    // disk space statistics
    shttpd_printf(arg, "\"disk_space\": {\n");
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *p, *q;
        struct statvfs fs;

        if (buf[0] != '/')      // skip mounts which don't start with '/'
            continue;
        p = strchr(buf, ' ');   // find the end of first word
        if (p == NULL)
            continue;           // skip lines without a space

        p++;
        q = strchr(p, ' ');     // find the end of second word
        if (q == NULL)
            continue;           // skip lines without a second space
        *q = '\0';              // terminate string after e.g. "/dev/sda1"

        if (statvfs(p, &fs) < 0) {
            shttpd_printf(arg, "statvfs(\"%s\") failed\n", p);
            goto finish;
        }

        // Make sure we put a comma at the end correctly
        if (first_time)
            first_time = 0;
        else
            shttpd_printf(arg, ",\n");

        // print the filesystem total and free space in bytes
        shttpd_printf(arg, "  \"%s\":        { \"total\": %llu, \"free\": %llu }",
                            p, (uint64_t)fs.f_bsize * fs.f_blocks, (uint64_t)fs.f_bsize * fs.f_bfree);
    }
    shttpd_printf(arg, "\n},\n");

    // CPU statistics
    shttpd_printf(arg, "\"cpu_usage\": {\n");
    shttpd_printf(arg, "  \"user\": %.2f,\n", cpu.user);
    shttpd_printf(arg, "  \"sys\": %.2f,\n", cpu.sys);
    shttpd_printf(arg, "  \"idle\": %.2f,\n", cpu.idle);
    shttpd_printf(arg, "  \"iowait\": %.2f\n", cpu.iowait);
    shttpd_printf(arg, "},\n");

    // memory statistics
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        shttpd_printf(arg, "sysinfo failed\n");
        goto finish;
    }
    shttpd_printf(arg, "\"memory\": {\n");
    shttpd_printf(arg, "  \"total\": %llu,", (unsigned long long)info.totalram * info.mem_unit);
    shttpd_printf(arg, " \"free\": %llu\n", (unsigned long long)info.freeram * info.mem_unit);
    shttpd_printf(arg, "}\n");

    // close opening braces
    shttpd_printf(arg, "}}\n");

finish:
    arg->flags |= SHTTPD_END_OF_OUTPUT;

    if (fp)
        fclose(fp);
}

static void availability(struct shttpd_arg* arg)
{
    // VERY SIMPLE function returns {"availability":true}
    // Just indicates to the interface that its http request to me succeeded
    
    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");

    // Use text/plain instead of application/json for ease of debugging in a browser
    shttpd_printf(arg, "Content-type: text/plain\r\n\r\n");

    
    shttpd_printf(arg,"{\"availability\":true}\n"); // end whole thing
    arg->flags |= SHTTPD_END_OF_OUTPUT;
}

int main(int argc, char *argv[])
{
    FILE *fp;
    if ((fp = fopen("/proc/stat", "r")) == NULL) {
        printf("Could not open /proc/stat\n");
        return 1;
    }

    // Start CPU monitoring thread
    pthread_t thread_id;
    int err;
    if ((err = pthread_create(&thread_id, NULL, cpu_monitor, (void*)fp)) != 0) {
        fprintf(stderr, "Failed to create monitor thread: %s\n", strerror(err));
        return 1;
    }

    // Setup the shttpd object
    struct shttpd_ctx *ctx;
    ctx = shttpd_init(NULL,"document_root","/dev/null",NULL);

    shttpd_listen(ctx, 7001, 0);

    // register handlers for various pages
    shttpd_register_uri(ctx, "/", &disk_filesystem_stats, NULL);
    shttpd_register_uri(ctx, "/availability", &availability, NULL);

    printf("Web server ready...\n");

    // loop forever serving requests
    while (1)
    {
        shttpd_poll(ctx, 1000);
    }
    shttpd_fini(ctx);       // just for completeness but not reached

    return 0;
}
