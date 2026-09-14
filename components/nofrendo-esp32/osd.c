/* vim: set tabstop=3 expandtab:
**
** This file is in the public domain.
**
** osd.c
**
** $Id: osd.c,v 1.2 2001/04/27 14:37:11 neil Exp $
**
*/

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
       
#include <noftypes.h>
#include <nofconfig.h>
#include <log.h>
#include <osd.h>
#include <nofrendo.h>

#include <stdio.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <version.h>

/* This is os-specific part of main() */
int osd_main(int argc, char *argv[])
{
    config.filename = "/sdcard/Game/nofrendo.cfg";
    return main_loop(argv[0], system_autodetect);
}

/* File system interface */
void osd_fullname(char *fullname, const char *shortname)
{
   strncpy(fullname, shortname, PATH_MAX);
}

/* This gives filenames for storage of saves */
char *osd_newextension(char *string, char *ext)
{
   char *dot;

   /* 如果字符串为空，直接返回 */
   if (!string || !ext)
      return string;

   /* 找到最后一个 '.' */
   dot = strrchr(string, '.');

   /* 如果有 '.'，从那里截断 */
   if (dot)
      *dot = '\0';

   /* 追加 '.' 和新扩展名 */
   strcat(string, ".");
   strcat(string, ext);

   return string;
}

/* This gives filenames for storage of PCX snapshots */
int osd_makesnapname(char *filename, int len)
{
   return -1;
}

char *osd_getromdata(const char *filename) {
    // 从 SD 卡读取 ROM 到 PSRAM
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    
    char *romdata = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (romdata == NULL) {
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(romdata, 1, size, f);
    fclose(f);
    
    if (read != size) {
        heap_caps_free(romdata);
        return NULL;
    }
    
    return romdata;
}