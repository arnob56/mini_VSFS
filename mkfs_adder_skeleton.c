#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>

#define BS 4096u
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint64_t total_blocks;
    uint64_t inode_count;
    uint64_t inode_bitmap_start;
    uint64_t inode_bitmap_blocks;
    uint64_t data_bitmap_start;
    uint64_t data_bitmap_blocks;
    uint64_t inode_table_start;
    uint64_t inode_table_blocks;
    uint64_t data_region_start;
    uint64_t data_region_blocks;
    uint64_t root_inode;
    uint64_t mtime_epoch;
    uint32_t flags;
    uint32_t checksum;
} superblock_t;
#pragma pack(pop)
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");

#pragma pack(push,1)
typedef struct {
    uint16_t mode;
    uint16_t links;
    uint32_t uid;
    uint32_t gid;
    uint64_t size_bytes;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t direct[12];
    uint32_t reserved_0;
    uint32_t reserved_1;
    uint32_t reserved_2;
    uint32_t proj_id;
    uint32_t uid16_gid16;
    uint64_t xattr_ptr;
    uint64_t inode_crc;
} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    uint32_t inode_no;
    uint8_t  type;
    char     name[58];
    uint8_t  checksum;
} dirent64_t;
#pragma pack(pop)
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");

uint32_t CRC32_TAB[256];
void crc32_init(void){
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
}
uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4);
    sb->checksum = s;
    return s;
}
void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE]; memcpy(tmp, ino, INODE_SIZE);
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c;
}
void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0; for (int i = 0; i < 63; i++) x ^= p[i];
    de->checksum = x;
}
static int bit_test(const uint8_t* bmap, uint64_t idx){
    return (bmap[idx>>3] >> (idx & 7u)) & 1u;
}
static void bit_set(uint8_t* bmap, uint64_t idx){
    bmap[idx>>3] |= (uint8_t)(1u << (idx & 7u));
}

int main(int argc, char** argv) {
    crc32_init();
    const char* in_path = NULL;
    const char* out_path = NULL;
    const char* add_path = NULL;

    for (int i=1;i<argc;i++){
        const char* a = argv[i];
        if (strcmp(a,"--input")==0 && i+1<argc) in_path = argv[++i];
        else if (strcmp(a,"--output")==0 && i+1<argc) out_path = argv[++i];
        else if (strcmp(a,"--file")==0 && i+1<argc) add_path = argv[++i];
        else { return 2; }
    }
    if (!in_path || !out_path || !add_path) return 2;

    FILE* fi = fopen(in_path, "rb");
    if (!fi) return 2;
    if (fseek(fi, 0, SEEK_END)!=0){ fclose(fi); return 2; }
    long fsz = ftell(fi);
    if (fsz < 0){ fclose(fi); return 2; }
    if (fseek(fi, 0, SEEK_SET)!=0){ fclose(fi); return 2; }
    uint8_t* img = (uint8_t*)malloc((size_t)fsz);
    if (!img){ fclose(fi); return 2; }
    if (fread(img,1,(size_t)fsz,fi)!=(size_t)fsz){ free(img); fclose(fi); return 2; }
    fclose(fi);
    if ((fsz % BS) != 0){ free(img); return 2; }

    superblock_t sb; memcpy(&sb, img + 0*BS, sizeof(sb));
    if (sb.magic != 0x4D565346u || sb.block_size != BS){ free(img); return 2; }
    superblock_t tmp = sb; tmp.checksum = 0;
    uint32_t want = crc32(&tmp, BS-4);
    if (want != sb.checksum){ free(img); return 2; }
    uint64_t total_blocks = sb.total_blocks;
    if ((uint64_t)fsz != total_blocks * BS){ free(img); return 2; }

    uint8_t* ibmap = img + (size_t)sb.inode_bitmap_start*BS;
    uint8_t* dbmap = img + (size_t)sb.data_bitmap_start*BS;
    uint8_t* itab  = img + (size_t)sb.inode_table_start*BS;

    inode_t root; memcpy(&root, itab + 0*INODE_SIZE, sizeof(root));

    FILE* ff = fopen(add_path, "rb");
    if (!ff){ free(img); return 2; }
    if (fseek(ff,0,SEEK_END)!=0){ fclose(ff); free(img); return 2; }
    long add_sz = ftell(ff);
    if (add_sz < 0){ fclose(ff); free(img); return 2; }
    if (fseek(ff,0,SEEK_SET)!=0){ fclose(ff); free(img); return 2; }

    const char* slash = strrchr(add_path, '/');
    const char* fname = slash ? (slash+1) : add_path;
    size_t fname_len = strlen(fname);
    if (fname_len == 0 || fname_len > 58){ fclose(ff); free(img); return 2; }

    if (root.direct[0] == 0){ fclose(ff); free(img); return 2; }
    uint64_t root_blk_abs = root.direct[0];
    if (root_blk_abs < sb.data_region_start || root_blk_abs >= total_blocks){ fclose(ff); free(img); return 2; }
    uint8_t* dirblk = img + (size_t)root_blk_abs * BS;

    int free_slot = -1;
    for (int i=0;i<(int)(BS/sizeof(dirent64_t));i++){
        dirent64_t de; memcpy(&de, dirblk + i*sizeof(dirent64_t), sizeof(de));
        if (de.inode_no == 0){ if (free_slot<0) free_slot = i; continue; }
        if (strncmp(de.name, fname, 58)==0){ fclose(ff); free(img); return 2; }
    }
    if (free_slot < 0){ fclose(ff); free(img); return 2; }

    uint64_t need_blocks = (add_sz<=0)?0: (( (uint64_t)add_sz + BS - 1) / BS);
    if (need_blocks > DIRECT_MAX){ fclose(ff); free(img); return 2; }

    int new_ino_idx = -1;
    for (uint64_t i=0;i<sb.inode_count;i++){
        if (!bit_test(ibmap, i)){ new_ino_idx = (int)i; break; }
    }
    if (new_ino_idx < 0){ fclose(ff); free(img); return 2; }
    bit_set(ibmap, new_ino_idx);
    uint32_t new_ino_no = (uint32_t)(new_ino_idx + 1);

    uint32_t directs[DIRECT_MAX]={0};
    uint64_t allocated = 0;
    for (uint64_t i=0;i<sb.data_region_blocks && allocated<need_blocks;i++){
        if (!bit_test(dbmap, i)){
            bit_set(dbmap, i);
            directs[allocated++] = (uint32_t)(sb.data_region_start + i);
        }
    }
    if (allocated != need_blocks){ fclose(ff); free(img); return 2; }

    for (uint64_t bi=0; bi<need_blocks; bi++){
        uint8_t* dst = img + (size_t)directs[bi] * BS;
        size_t to_read = (bi+1<need_blocks)? BS : (size_t)((uint64_t)add_sz - bi*BS);
        if (to_read>0){
            if (fread(dst,1,to_read,ff)!=to_read){ fclose(ff); free(img); return 2; }
        }
        if (to_read < BS) memset(dst+to_read, 0, BS-to_read);
    }
    fclose(ff);

    inode_t ino; memset(&ino, 0, sizeof(ino));
    ino.mode = 0100000;
    ino.links = 1;
    ino.uid = 0; ino.gid = 0;
    ino.size_bytes = (uint64_t)add_sz;
    ino.atime = ino.mtime = ino.ctime = (uint64_t)time(NULL);
    for (int i=0;i<DIRECT_MAX;i++) ino.direct[i] = directs[i];
    inode_crc_finalize(&ino);
    memcpy(itab + (size_t)new_ino_idx*INODE_SIZE, &ino, sizeof(ino));

    dirent64_t nde; memset(&nde, 0, sizeof(nde));
    nde.inode_no = new_ino_no;
    nde.type = 1;
    strncpy(nde.name, fname, 57);
    dirent_checksum_finalize(&nde);
    memcpy(dirblk + free_slot*sizeof(dirent64_t), &nde, sizeof(nde));

    root.size_bytes += sizeof(dirent64_t);
    root.links += 1;
    root.mtime = (uint64_t)time(NULL);
    inode_crc_finalize(&root);
    memcpy(itab + 0*INODE_SIZE, &root, sizeof(root));
    
    printf("File '%s' has been successfully added to the file system image '%s'.\n", add_path, out_path);

    FILE* fo = fopen(out_path, "wb");
    if (!fo){ free(img); return 2; }
    if (fwrite(img,1,(size_t)fsz, fo)!=(size_t)fsz){ fclose(fo); free(img); return 2; }
    fclose(fo);
    printf("Updated file system image '%s' has been written successfully.\n", out_path);
    free(img);
    return 0;
}
