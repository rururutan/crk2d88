#include <iostream>
#include <stdio.h>
#include <windows.h>

// Craken file(CRK) info

// Prototype
void usage(char *);

#define NAME "Craken file(CRK) info"
#define VERSION "1.00"

// .CRK File define
#define CRK_HEADER_STRING "This is CRAKEN file."
#define CRK_HEADER_LENGTH 0x15

#define CRK_TRACK_SECTOR_LENGTH 0x01

#define CRK_TRACK_INFO_SECTOR_NUM 0x1F
#define CRK_TRACK_INFO_SINGLE_SIDE 0x20
#define CRK_TRACK_INFO_2DD 0x40
#define CRK_TRACK_INFO_144MB 0x80

#define CRK_SECTOR_INFO_LENGTH 0x78 //    4*30
#define CRK_TRACK_DATA 0x2000       // 1024* 8


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

int main (int argc, char *argv[]) {
  char oTrackInfo = 0;
  int oCount = ERR_NONE;
  int oReturn = 0;
  FILE *pInFile = NULL, *pOutFile = NULL;

  try {

    if (argc < 2) throw ERR_PARAM;

    // source file open
    pInFile = fopen(argv[1], "rb");
    if (pInFile == NULL) throw ERR_FILE_READ;

    // header check
    char aHeader[CRK_HEADER_LENGTH];
    if (fread(aHeader, 1, CRK_HEADER_LENGTH, pInFile) != CRK_HEADER_LENGTH)
      throw ERR_FILE_READ;

    aHeader[CRK_HEADER_LENGTH-1] = 0;
    if (strcmp(aHeader, CRK_HEADER_STRING) != 0)
      throw ERR_CRK_HEADER_ERR;

    // data check
    int oSecNum = 0;
    for (oCount=0; oCount<164; oCount++) {
      //////////////////////////////////////////////
      // Check for the presence of track
      if (fread(&oTrackInfo, 1, 1, pInFile) != 1)
        break;

      printf("Track No.%03d ", oCount);

      // Check track sector size
      oSecNum = CRK_TRACK_INFO_SECTOR_NUM & oTrackInfo;
      printf("Sec:%02d ", oSecNum);

      // Check track double-sided
      printf("Dbl:%d ", (CRK_TRACK_INFO_SINGLE_SIDE & oTrackInfo)>>4);

      // Check 2HD/2DD
      printf("2DD:%d\n", CRK_TRACK_INFO_2DD & oTrackInfo);

      //////////////////////////////////////////////
      // Skip track info
      char aSecInfo[30][4] = {0};
      int oSecSize = 0, oCylinder = 0, oHead = 0, oTrackSize = 0;
      fread(aSecInfo, 4, 30, pInFile);
      for (int j=0; j< oSecNum; j++) {
        oCylinder = aSecInfo[j][0];
        oHead     = aSecInfo[j][1];
        switch(aSecInfo[j][3]) {
        case 0:
          oSecSize =  128; break;
        case 1:
          oSecSize =  256; break;
        case 2:
          oSecSize =  512; break;
        case 3:
          oSecSize = 1024; break;
        case 4:
          oSecSize = 2048; break;
        case 5:
          oSecSize = 4096; break;
        }

        // Track number check
        int oTrackNo = oCylinder*2 + oHead;
        if (oTrackNo > oCount) {
          oCount = oTrackNo;
        }

        printf(" Sector No.%02d C:%2d H:%d S:%2d Secsize:%4d\n",
               j, aSecInfo[j][0], aSecInfo[j][1], aSecInfo[j][2], oSecSize);
      }

      //////////////////////////////////////////////
      // Skip track data
      fseek(pInFile, oSecNum*oSecSize, 1);
    }
  } catch (eERROR error) {
    usage(argv[0]);
    oReturn = error;

    switch (error) {
    case ERR_PARAM:
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
      printf("track no:%d trackinfo:%d\n", oCount, oTrackInfo);
      printf("this program support only 8sector double-sided format.\n");
      break;
    case ERR_CRK_TRACK_DATA_ERR:
      printf(".CRK track data size is not enough.\n");
      printf("track no:%d\n", oCount);
      break;
    case ERR_CRK_MORE_TRACK:
      printf(".CRK file have more tracks than 153 tracks.\n");
      break;
    default:
      break;
    }
  }

  if (pInFile != NULL) fclose(pInFile);

  return (oReturn);
}

// Print usage
void usage(char* p_pFilename) {
  printf(NAME " Ver." VERSION "\n");
  printf("usage :: %s [.crk file]\n", p_pFilename);
}
