/**
 *
 * Convert Craken file(CRK) to D88 file(D88/D77/D68).
 *
 *                               2008/11/13 RuRuRu.
 *
 * 制限:
 * Trackは順番に記録されていること(Trackに抜けが有るのはOK)
 * .CRKはSector情報しか持っていないので変なCylinder、Headが書かれていたら
 * Trackとの関連づけが出来ない
 *
 */


#include <iostream>
#include <stdio.h>
#include <windows.h>

#define NAME "Craken file(CRK) to D88 file(D88) converter"
#define VERSION "1.02"

static const size_t MAX_TRACK_NUM = 164;		 // 2DD

// .CRK File define
#define CRK_HEADER_STRING "This is CRAKEN file."

static const size_t CRK_HEADER_LENGTH = 0x15;
static const size_t CRK_MAX_TRACK_BUF = 0x3C000; // (8192byte*30sec)
static const size_t CRK_MAX_SECTOR_NUM = 30;

// Craken Trakinfo define
#define CRK_TRACK_INFO_SECTOR_NUM 0x1F
#define CRK_TRACK_INFO_SINGLE_SIDE 0x20
#define CRK_TRACK_INFO_2DD 0x40
#define CRK_TRACK_INFO_144MB 0x80

#define CRK_TRACK_INFO_LENGTH 0x01	// 1byte
#define CRK_SECTOR_INFO_LENGTH 0x04		// 4byte

enum eERROR {
    ERR_NONE,
    ERR_PARAM,
    ERR_FILE_READ,
    ERR_FILE_WRITE,
    ERR_CRK_HEADER_ERR,
    ERR_CRK_FORMAT_ERR,
    ERR_CRK_TRACK_DEFICIT,
    ERR_CRK_TRACK_DATA_ERR,
    ERR_CRK_MORE_TRACK
};

// D88 Header Structure
typedef struct d88_diskhd {
    char title[16];                  /* 余白は$00で埋める */
    char dmy[10];                    /* $00 */
    char attr;                       /* アトリビュート(WriteProtect=$10) */
    char media;                      /* */
    long next_disk;                  /* 次のdiskにSEEKしてEOFなら終わり */
    long next_track[MAX_TRACK_NUM];  /* unformatは0Lが入る */
} D88_HEAD;

// D88 Sector Structure
typedef struct d88_sechd {
    BYTE C;			// Cylinder
    BYTE H;			// Head
    BYTE R;			// 論理Sector No.
    BYTE N;			// Sector Size
    WORD secn;		// Total Sector Number
    BYTE MFM;       // FM/MFM($40/$00)
    BYTE DD;        // DELETED DATA
    BYTE status;	// DiskImage変換では無視してOK
    BYTE dmy[5];    // $00
    WORD secsize;   // Default:$0100
} D88_SEC_HEAD;

// CRK Sector Structure
typedef struct crk_sechd {
    BYTE C;			// Cylinder
    BYTE H;			// Head
    BYTE R;			// 論理Sector
    BYTE N;			// Sector Size
} CRK_SEC_HEAD;


// Prototype
void usage(void);
void error_print(eERROR, DWORD);
void getTrackInfo(BYTE, BYTE &, BYTE &, BYTE &);
WORD getSectorSize(BYTE);

// main
int main (int argc, char *argv[]) {
    BYTE oTrackInfo = 0;
    DWORD oCount = ERR_NONE;
    int oReturn = 0;
    FILE *pInFile = NULL, *pOutFile = NULL;

    try {
        // parameter check
        if (argc < 2) throw ERR_PARAM;

        // source file open
		pInFile = ::fopen(argv[1], "rb");
        if (pInFile == NULL) throw ERR_FILE_READ;

        // header check
        char aHeader[CRK_HEADER_LENGTH];
		if (::fread(aHeader, 1, CRK_HEADER_LENGTH, pInFile) != CRK_HEADER_LENGTH) {
			throw ERR_FILE_READ;
		}

        aHeader[CRK_HEADER_LENGTH-1] = 0;	// replace 0x1A->0x00
		if (strcmp(aHeader, CRK_HEADER_STRING) != 0) {
			throw ERR_CRK_HEADER_ERR;
		}

        // create dust file name
        char aDustName[MAX_PATH];
        memset(aDustName, 0, MAX_PATH);
        strncpy(aDustName, argv[1], MAX_PATH-1);
        char *pExt = strchr(aDustName, '.');
        strcpy(pExt, ".D88");

        // data write
        BYTE aTrackData[CRK_MAX_TRACK_BUF];
        BYTE oSecNum = 0;
        BYTE oDouble = 0;
        BYTE oDD     = 0;
        D88_HEAD stHead;

        for (oCount=0; oCount<MAX_TRACK_NUM; oCount++) {
            //////////////////////////////////////////////
            // get track info

            // Check for the presence of track
			if (::fread(&oTrackInfo, 1, 1, pInFile) != 1) {
                break;
            }

            getTrackInfo(oTrackInfo, oSecNum, oDouble, oDD);

            //////////////////////////////////////////////
            // read sector info

            // read sector info block
            BYTE aSecInfo[CRK_MAX_SECTOR_NUM][CRK_SECTOR_INFO_LENGTH] = {0};
            DWORD oTrackSize = 0, oCylinder = 0, oHead = 0;
			::fread(aSecInfo, CRK_SECTOR_INFO_LENGTH, CRK_MAX_SECTOR_NUM, pInFile);


            //////////////////////////////////////////////
            // track number check

            // Cylinder*2+Head = トラック番号
            DWORD oTrackNo = aSecInfo[0][0]*2 + aSecInfo[0][1];

            // CRAKENは途中のトラックデーターが収録されなくても良い仕様なので
            // トラック番号がカウンター番号より大きければカウンタ更新
            if (oTrackNo > oCount) {
                oCount = oTrackNo;
            }

            //////////////////////////////////////////////
            // read track data

            // get track size
            // セクタ毎にサイズが異なる可能性があるので1つづつ足していく
            for (int j=0; j< oSecNum; j++) {
                oTrackSize += getSectorSize(aSecInfo[j][3]);
            }

			if (::fread(aTrackData, 1, oTrackSize, pInFile) != oTrackSize) {
				throw ERR_CRK_TRACK_DATA_ERR;
			}

            //////////////////////////////////////////////
            // write track data
            if (oCount == 0) {
                // dust file open
				pOutFile = ::fopen(aDustName, "wb");
                if (pOutFile == NULL) throw ERR_FILE_READ;

                // write header
                memset(&stHead, 0, sizeof(D88_HEAD));

                if (oDD) {
                    stHead.media = 0x10;
                } else {
                    stHead.media = 0x20;
                }
				::fseek(pOutFile, sizeof(D88_HEAD), SEEK_CUR);
            }

            // write track offset
            stHead.next_track[oCount] = ftell(pOutFile);

            // weite sector data
            D88_SEC_HEAD stSecHead;
            for (int i=0; i<oSecNum; i++) {
                // write sector head
                memset(&stSecHead, 0, sizeof(D88_SEC_HEAD));
                stSecHead.C = aSecInfo[i][0];
                stSecHead.H = aSecInfo[i][1];
                stSecHead.R = aSecInfo[i][2];
                stSecHead.N = aSecInfo[i][3];
                stSecHead.secn = oSecNum;
                stSecHead.MFM = oDouble;

                stSecHead.secsize = getSectorSize(aSecInfo[i][3]);
                if (stSecHead.secsize == 0) {
                    throw ERR_CRK_FORMAT_ERR;
                }

                // write header
				if (::fwrite(&stSecHead, 1, sizeof(D88_SEC_HEAD), pOutFile) != sizeof(D88_SEC_HEAD)) {
					throw ERR_FILE_WRITE;
				}

                // write data
				if (::fwrite(aTrackData+(stSecHead.secsize*i), 1, stSecHead.secsize, pOutFile) != stSecHead.secsize) {
					throw ERR_FILE_WRITE;
				}
            }
        }

        // write file end
        stHead.next_disk = ftell(pOutFile);

		::fseek(pOutFile, 0, SEEK_SET);
		::fwrite(&stHead, 1, sizeof(D88_HEAD), pOutFile);

    } catch (eERROR error) {
        oReturn = error;
		if (error != ERR_NONE) {
			printf(NAME " Ver." VERSION "\n");
			if (error != ERR_PARAM) {
				printf("file : %s - ", argv[1]);
			}
		}
        error_print(error, oCount);
    }

	if (pInFile  != NULL) ::fclose(pInFile);
	if (pOutFile != NULL) ::fclose(pOutFile);

    return (oReturn);
}

// Print usage
void usage(void)
{
    printf("usage :: CRK2D88 [.crk file]\n");
}

// Print error
inline void error_print(eERROR error, DWORD count)
{
	switch (error) {
	  case ERR_PARAM:
		usage();
		printf("paramater error.\n");
		break;
	  case ERR_FILE_READ:
		printf("file open error.\n");
		break;
	  case ERR_FILE_WRITE:
		printf("write error\n;");
		break;
	  case ERR_CRK_HEADER_ERR:
		printf(".CRK file header error.\n");
		break;
	  case ERR_CRK_FORMAT_ERR:
		printf(".CRK file sector size error.\n");
		printf("track no:%lu\n", count);
		break;
	  case ERR_CRK_TRACK_DEFICIT:
		printf(".CRK file track is deficit.\n");
		printf("track no:%lu\n", count);
		break;
	  case ERR_CRK_TRACK_DATA_ERR:
		printf(".CRK track data size is not enough.\n");
		printf("track no:%lu\n", count);
		break;
	  case ERR_CRK_MORE_TRACK:
		printf(".CRK file have more tracks than 153 tracks.\n");
		break;
	  default:
		printf("Unknwon error.\n");
		break;
	}
}

// Track情報取得
inline void getTrackInfo(BYTE p_oTrackInfo, BYTE &p_oSecNum, BYTE &p_oDouble, BYTE &p_oDD)
{
    // get track sector number
    p_oSecNum = CRK_TRACK_INFO_SECTOR_NUM & p_oTrackInfo;

    // get track double-sided
    p_oDouble = (CRK_TRACK_INFO_SINGLE_SIDE & p_oTrackInfo)?0x40:0;

    // get 2HD/2DD
    p_oDD     = (CRK_TRACK_INFO_2DD & p_oTrackInfo);
}

// CRK -> D88 sec size convert
// @return code /error(0)
inline WORD getSectorSize(BYTE p_oSec)
{
    switch(p_oSec) {
      case 0:
        return 128;
      case 1:
        return 256;
      case 2:
        return 512;
      case 3:
        return 1024;
      case 4:
        return 2048;
      case 5:
        return 4096;
      case 6:
        return 8192;
      default:
        return 0;
    }
}
