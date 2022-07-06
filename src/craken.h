#ifndef __CRAKEN_H__
#define __CRAKEN_H__

// .CRK File define
#define CRK_HEADER_STRING "This is CRAKEN file."
#define CRK_HEADER_LENGTH 0x15

#define CRK_MAX_SECTOR_NUM 30

// track info bit
#define CRK_TRACK_INFO_SECTOR_NUM 0x1F
#define CRK_TRACK_INFO_SINGLE_SIDE 0x20
#define CRK_TRACK_INFO_2DD 0x40
#define CRK_TRACK_INFO_144MB 0x80

#define CRK_TRACK_INFO_LENGTH 0x01	// 1byte
#define CRK_SECTOR_INFO_LENGTH 0x04		// 4byte

// CRK Sector Structure
typedef struct crk_sechd {
    BYTE C;			// Cylinder
    BYTE H;			// Head
    BYTE R;			// 論理Sector
    BYTE N;			// Sector Size
} CRK_SEC_HEAD;

#endif  /* __CRAKEN_H__ */
