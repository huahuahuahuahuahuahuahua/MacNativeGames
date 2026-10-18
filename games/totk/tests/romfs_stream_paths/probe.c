/* Exercise the real RomFS open/size/read code against a generated, game-free
   fixture. In particular a stream must read beyond its first 8 KiB. */
#include "runtime.h"
#include "fs_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

_Noreturn void runtime_fail(const char *message) { fprintf(stderr,"FAIL: %s\n",message);exit(1); }
void runtime_boot_progress(const char *stage,uint64_t done,uint64_t total) { (void)stage;(void)done;(void)total; }
void dm_guest_write_prepare(void *address,size_t size) { (void)address;(void)size; }
uint64_t runtime_now_ns(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec; }
int runtime_output_res(unsigned *w,unsigned *h) { (void)w;(void)h;return 0; }
uint64_t runtime_output_res_extra_bytes(void) { return 0; }
void *runtime_fs_host_override(const char *name) { (void)name;return NULL; }
static void check(int ok,const char *message) { if(!ok)runtime_fail(message); }
static unsigned char expected(unsigned offset) { return (unsigned char)((offset*37u+offset/251u+17u)&255u); }

int main(void) {
    typedef unsigned (*MountRom)(const char *,void *,uint64_t);
    MountRom mount=(MountRom)runtime_fs_override("_ZN2nn2fs8MountRomEPKcPvm");
    check(mount&&mount("content",NULL,0)==0,"mount content fixture");
    const char *paths[]={"content:/Sound/Resource/Stream/Fixture.bwav",
                        "content://Sound/Resource/Stream/Fixture.bwav",
                        "content:////Sound/Resource/Stream/Fixture.bwav"};
    unsigned char data[8192];
    for(unsigned p=0;p<sizeof(paths)/sizeof(paths[0]);p++) {
        uint64_t handle=0,read=0;int64_t size=0;int type=-1;
        check(runtime_romfs_owns(paths[p]),"the mounted content path belongs to RomFS");
        check(runtime_romfs_open(&handle,paths[p],1)==0,"open normal and repeated-root stream paths");
        check(runtime_romfs_size(&size,handle)==0&&size==40000,"stream file size");
        for(unsigned block=0;block<4;block++) {
            memset(data,0,sizeof(data));
            check(runtime_romfs_read(&read,handle,block*sizeof(data),data,sizeof(data))==0&&read==sizeof(data),"read successive stream blocks");
            for(unsigned i=0;i<sizeof(data);i++)check(data[i]==expected(block*sizeof(data)+i),"stream data matches each original block");
        }
        check(runtime_romfs_read(&read,handle,size,data,1)==0&&read==0,"EOF is a successful empty read");
        check(runtime_romfs_entry_type(&type,paths[p])==0&&type==1,"entry lookup uses the same root handling");
        runtime_romfs_close(handle);
    }
    const char *invalid[]={"other://Sound/Resource/Stream/Fixture.bwav",
        "content://../Sound/Resource/Stream/Fixture.bwav",
        "content://Sound/../Resource/Stream/Fixture.bwav",
        "content://./Sound/Resource/Stream/Fixture.bwav",
        "content://Sound\\Resource/Stream/Fixture.bwav",
        "content://Sound//Resource/Stream/Fixture.bwav",
        "content://", "content:Sound/Resource/Stream/Fixture.bwav"};
    uint64_t handle=0;
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)check(runtime_romfs_open(&handle,invalid[i],1)!=0,"invalid paths remain rejected");
    check(runtime_romfs_open(&handle,paths[1],2)==0x6402u,"RomFS remains read-only");
    puts("{\"status\":\"pass\",\"valid_root_forms\":3,\"invalid_paths_rejected\":8,\"stream_blocks_per_form\":4,\"read_only\":true,\"game_data\":false}");
    return 0;
}
