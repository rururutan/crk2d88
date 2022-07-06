#include <iostream>
#include <stdio.h>
#include <windows.h>

// Convert Craken file(CRK) to BKDSK file(HDM).

// 制限:
// MS-DOSフォーマット(2HD/8sector)専用
// 0-153Trackまで順番に記録されていること(Trackに抜けが有るのはOK)
// 153以降のTrackは無視(.HDMのformat的に保存出来ない)
// .CRKはSector情報しか持っていないので変なCylinder、Headが書かれていたら
// Trackとの関連づけが出来ない

#define NAME "Craken file(CRK) to BKDSK file(HDM) converter"
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

// Prototype
void usage(char *);
bool dummyWrite(FILE*, int);

// main
int main (int argc, char *argv[]) {
  char oTrackInfo = 0;
  int oCount = ERR_NONE;
  int oReturn = 0;
  FILE *pInFile = NULL, *pOutFile = NULL;

  try {

    // parameter check
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

    // create dust file name
    char aDustName[MAX_PATH];
    memset(aDustName, 0, MAX_PATH);
    strncpy(aDustName, argv[1], MAX_PATH-1);
    char *pExt = strchr(aDustName, '.');
    strcpy(pExt, ".hdm");

    // data write
    char aTrackData[CRK_TRACK_DATA];
    int oSecNum = 0;
    for (oCount=0; oCount<154; oCount++) {
      //////////////////////////////////////////////
      // Check track info

      // Check for the presence of track
      if (fread(&oTrackInfo, 1, 1, pInFile) != 1)
        throw ERR_CRK_TRACK_DEFICIT;

      // Check track double-sided
      if ((CRK_TRACK_INFO_SINGLE_SIDE & oTrackInfo) == 0x01)
        throw ERR_CRK_FORMAT_ERR;

      // Check track sector size (8sector?)
      oSecNum = CRK_TRACK_INFO_SECTOR_NUM & oTrackInfo;
      if (oSecNum != 0x08)
        throw ERR_CRK_FORMAT_ERR;

      // Check 2HD/2DD
      if ((CRK_TRACK_INFO_2DD & oTrackInfo) == 0x01)
        throw ERR_CRK_FORMAT_ERR;

      //////////////////////////////////////////////
      // Check sector info
      char aSecInfo[30][4] = {0};
      int oSecSize = 0, oCylinder = 0, oHead = 0;
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
        // 足りないトラックはダミーを挿入
        int oTrackNo = oCylinder*2 + oHead;
        if (oTrackNo> oCount) {
          for (int i=oTrackNo-oCount; i>0; i--) {
            printf("write dummy track no.%d", oCount);
            if (dummyWrite(pOutFile, oSecNum*oSecSize) != true)
              throw ERR_FILE_WRITE;
            oCount++;
          }
        }
      }

      //////////////////////////////////////////////
      // Read track data
      if (fread(aTrackData, 1, oSecNum*oSecSize, pInFile) != CRK_TRACK_DATA)
        throw ERR_CRK_TRACK_DATA_ERR;

      //////////////////////////////////////////////
      // Write track data
      if (oCount == 0) {
        // dust file open
        pOutFile = fopen(aDustName, "wb");
        if (pOutFile == NULL) throw ERR_FILE_READ;
      }
      if (fwrite(aTrackData, 1, oSecNum*oSecSize, pOutFile) != CRK_TRACK_DATA)
        throw ERR_FILE_WRITE;
    }
    // more track ?
    if (fread(&oTrackInfo, 1, 1, pInFile) == 1)
      throw ERR_CRK_MORE_TRACK;

  } catch (eERROR error) {
    oReturn = error;

    if (error != ERR_NONE) {
      printf(NAME " Ver." VERSION "\n");
      if (error != ERR_PARAM)
        printf("file : %s - ", argv[1]);
    }

    switch (error) {
    case ERR_PARAM:
      usage(argv[0]);
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
    case ERR_CRK_TRACK_DEFICIT:
      printf(".CRK file track is deficit.\n");
      printf("track no:%d\n", oCount);
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
  if (pOutFile != NULL) fclose(pOutFile);

  return (oReturn);
}

// Print usage
void usage(char* p_pFilename) {
  printf("usage :: %s [.crk file]\n", p_pFilename);
}

// 0xE5でダミートラックを書き込む(NECのFORMAT.EXE互換)
bool dummyWrite(FILE* p_pOutFile, int p_DataSize) {
  char *pTrackData = new char[p_DataSize];
  memset(pTrackData, 0xE5, p_DataSize);
  int ret = fwrite(pTrackData, 1, p_DataSize, p_pOutFile);
  delete pTrackData;

  if (ret != p_DataSize) return (false);
  else                   return (true);
}
