#ifndef _H_FAT_
#define _H_FAT_

#define MSDOS_SUPER_MAGIC       0x4d44          /* MD */
#if 0
/* FAT bootsector format, also used by other disk-based derivatives */
struct boot_sector {
    uint8_t bsJump[3];
    char bsOemName[8];
    uint16_t bsBytesPerSec;
    uint8_t bsSecPerClust;
    uint16_t bsResSectors;
    uint8_t bsFATs;
    uint16_t bsRootDirEnts;
    uint16_t bsSectors;
    uint8_t bsMedia;
    uint16_t bsFATsecs;
    uint16_t bsSecPerTrack;
    uint16_t bsHeads;
    uint32_t bsHiddenSecs;
    uint32_t bsHugeSectors;

    union {
        struct {
            uint8_t DriveNumber;
            uint8_t Reserved1;
            uint8_t BootSignature;
            uint32_t VolumeID;
            char VolumeLabel[11];
            char FileSysType[8];
            uint8_t Code[442];
        } __attribute__ ((packed)) bs16;
        struct {
            uint32_t FATSz32; 
            uint16_t ExtFlags;
            uint16_t FSVer;
            uint32_t RootClus;
            uint16_t FSInfo;
            uint16_t BkBootSec;
            uint8_t Reserved0[12];
            uint8_t DriveNumber;
            uint8_t Reserved1;
            uint8_t BootSignature;
            uint32_t VolumeID;
            char VolumeLabel[11];
            char FileSysType[8];
            uint8_t Code[414];
        } __attribute__ ((packed)) bs32;
    } __attribute__ ((packed));
    
    uint32_t NextSector;        /* Pointer to the first unused sector */
    uint16_t MaxTransfer;       /* Max sectors per transfer */
    uint16_t bsSignature;
} __attribute__ ((packed));

#define bsHead      bsJump
#define bsHeadLen   offsetof(struct boot_sector, bsOemName)
#define bsCode      bs32.Code   /* The common safe choice */
#define bsCodeLen   (offsetof(struct boot_sector, bsSignature) - \
                     offsetof(struct boot_sector, bsCode))
#endif
#endif
