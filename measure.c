#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#include <machbase_sqlcli.h>

// Please configure your connection info.
#define DB_HOST "127.0.0.1"
#define DB_PORT (5656)

#define DEFAULT_CHECK_COUNT 100
#define ROTATE 1024
#define TOTAL (ROTATE * 10)
#define PI 3.14159265

#define VERIFYERROR(a, s)           \
    {                               \
        SQLRETURN sRet;             \
        sRet = (a);                 \
        if( sRet == SQL_ERROR )     \
        {                           \
            outerror(sRet, #a, s);  \
        }                           \
    }

SQLHENV   gEnv;
SQLHDBC   gConn;
SQLHSTMT  gMeasureStmt;
SQLHSTMT  gSensorsStmt;

void outerror(SQLRETURN, char*, char*);

int openDBConn(char*, char*, char*, int);
int prepareAppend(void);
int appendMeasure(char*, char*, time_t, double*, time_t*, int, double, double, double, double);
void closeDBConn(void);

int measure(double*, time_t*, int, double*, double*, double*, double*);

int main(int argc, char** argv)
{
    int i;

    double x, y;
    double sRealMag, sRealAng;
    double sResMag, sResAng;

    struct timeval tm;
    time_t sec;

    double sig[TOTAL];
    time_t ticks[TOTAL];

    static char* sDefaultLineID = "MACH01-BASE01";
    static char* sDefaultTire   = "IOT-TS-205/70-ZR-18";

    char*   sLineID;
    char*   sTireModel;
    int     sRepeats;

    sRepeats = 1;
    sLineID = sDefaultLineID;
    sTireModel = sDefaultTire;

    switch(argc)
    {
    case 4:
        sTireModel  = argv[3];
    case 3:
        sLineID     = argv[2];
    case 2:
        sRepeats    = atoi(argv[1]);
    case 1:
        break;
    default:
        printf("Too many arguments\n");
        printf("Usage: %s [repeats] [line id] [tire model]\n");
        exit(-1);
        break;
    }

    if( sRepeats < 1 )
    {
        sRepeats = 1;
    }

    srand(getpid());

    if( openDBConn(DB_HOST, "SYS", "MANAGER", DB_PORT) != 0 )
    {
        printf("Open DB connection failed.\n");
        exit(-1);
    }
    if( prepareAppend() != 0 )
    {
        printf("Preparing for APPEND failed.\n");
        exit(-1);
    }

    for(i = 0; i < sRepeats; i++)
    {
        if( gettimeofday(&tm, NULL) == 0 )
        {
            sec = tm.tv_sec;
        }
        else
        {
            printf("Get time failed.\n");
            break;
        }
        if( measure(sig, ticks, TOTAL, &sRealMag, &sRealAng, &sResMag, &sResAng) != 0 )
        {
            printf("Measure failed.\n");
            break;
        }
        if( appendMeasure(sLineID, sTireModel, sec, sig, ticks, TOTAL, sRealMag, sRealAng, sResMag, sResAng) != 0 )
        {
            printf("Append failed.\n");
            break;
        }
    }

    closeDBConn();

    return 0;
}

int openDBConn(char* aHost, char* aUser, char* aPWD, int aPort)
{
    char sConnStr[128];

    if( SQLAllocEnv(&gEnv) == SQL_ERROR )
    {
        printf("Cannot allocate SQLENV");
        return -1;
    }
    if( SQLAllocConnect(gEnv, &gConn) == SQL_ERROR )
    {
        printf("Cannot allocate SQLConn" );
        return -1;
    }

    snprintf(sConnStr, 128, "DSN=%s;UID=%s;PWD=%s;CONNTYPE=1;PORT_NO=%d",
             aHost, aUser, "*****", aPort);
    printf("Connect using \"%s\"...", sConnStr);
    snprintf(sConnStr, 128, "DSN=%s;UID=%s;PWD=%s;CONNTYPE=1;PORT_NO=%d",
             aHost, aUser, aPWD, aPort);

    VERIFYERROR( SQLDriverConnect(gConn, NULL, sConnStr, SQL_NTS,
                                  NULL, 0, NULL, SQL_DRIVER_NOPROMPT),
                 "connection failed" );
    printf("success!\n");
    return 0;
}

int prepareAppend(void)
{
    printf("Preparing for append...");
    VERIFYERROR( SQLAllocStmt(gConn, &gMeasureStmt), "Measure statement alloc failed");
    VERIFYERROR( SQLAllocStmt(gConn, &gSensorsStmt), "Sensor  statement alloc failed");

    if( SQLAppendOpen(gMeasureStmt, "measure_list", DEFAULT_CHECK_COUNT) == SQL_ERROR )
    {
        printf("appendOpen measure list failed\n");
        return -1;
    }
    if( SQLAppendOpen(gSensorsStmt, "tag", DEFAULT_CHECK_COUNT) == SQL_ERROR )
    {
        printf("appendOpen tag data failed\n");
        return -1;
    }
    printf("success!\n");

    return 0;
}

void closeDBConn(void)
{
    VERIFYERROR( SQLFreeStmt(gSensorsStmt, SQL_DROP), "Free stmt failed" );
    VERIFYERROR( SQLFreeStmt(gMeasureStmt, SQL_DROP), "Free stmt failed" );
    VERIFYERROR( SQLDisconnect(gConn), "Disconnect failed" );
    VERIFYERROR( SQLFreeConnect(gConn), "Free session failed" );
    VERIFYERROR( SQLFreeEnv(gEnv), "Free env failed" );
}

int appendMeasure(char*     aLineID,
                  char*     aModel,
                  time_t    aSec,
                  double*   aSig,
                  time_t*   aTicks,
                  int       aCount,
                  double    aRealMag,
                  double    aRealAng,
                  double    aResMag,
                  double    aResAng)
{
    int     i;
    time_t  sNSec;

    struct tm   sLocalTime;
    char        sTagStr[40];

    SQL_APPEND_PARAM    sMesParam[7];
    SQL_APPEND_PARAM    sSenParam[4];

    localtime_r(&aSec, &sLocalTime);
    sNSec = aSec * 1000000000;

    snprintf(sTagStr, sizeof(sTagStr),
             "%s [%04d-%02d-%02d %02d:%02d:%02d]",
             aLineID,
             sLocalTime.tm_year + 1900,
             sLocalTime.tm_mon + 1,
             sLocalTime.tm_mday,
             sLocalTime.tm_hour,
             sLocalTime.tm_min,
             sLocalTime.tm_sec);

    printf("Append [%s][%g][%g]...", sTagStr, aRealMag, aRealAng);

    sSenParam[0].mVarchar.mLength   = strlen(sTagStr);
    sSenParam[0].mVarchar.mData     = sTagStr;

    for(i = 0; i < TOTAL; i++)
    {
        sSenParam[1].mDateTime.mTime    = aTicks[i];
        sSenParam[2].mDouble            = aSig[i];
        sSenParam[3].mInteger           = i % ROTATE;

        VERIFYERROR( SQLAppendDataV2(gSensorsStmt, sSenParam), "failed" );
    }

    VERIFYERROR( SQLAppendFlush(gSensorsStmt), "failed" );

    sMesParam[0].mVarchar.mLength   = strlen(aLineID);
    sMesParam[0].mVarchar.mData     = aLineID;
    sMesParam[1].mVarchar.mLength   = strlen(aModel);
    sMesParam[1].mVarchar.mData     = aModel;

    sMesParam[2].mDateTime.mTime    = sNSec;
    sMesParam[3].mDouble            = aRealMag;
    sMesParam[4].mDouble            = aRealAng;
    sMesParam[5].mDouble            = aResMag;
    sMesParam[6].mDouble            = aResAng;

    VERIFYERROR( SQLAppendDataV2(gMeasureStmt, sMesParam), "failed" );
    VERIFYERROR( SQLAppendFlush(gMeasureStmt), "failed" );
    printf("success!\n");

    return 0;
}

int measure(double* aSig, time_t* aTicks, int aCount, double* aRealMag, double* aRealAng, double* aResMag, double* aResAng)
{
    double noiselevel = 2.;

    int i;
    double sRealMag;
    double sRealAng;
    struct timeval tm;
    time_t sec;

    double sXPos;
    double sYPos;
    double sResMag;
    double sResAng;

    sRealMag = 3. + (double)(rand() % 30) / 10.;
    sRealAng = (double)(rand() % 1024);

    if( gettimeofday(&tm, NULL) == 0 )
    {
        sec = tm.tv_sec;
    }
    else
    {
        printf("Get time failed.\n");
        return -1;
    }

    sec *= 1000000000;
    for(i = 0; i < aCount; i++)
    {
        aTicks[i]   = sec + (i * (time_t)2000000000) / aCount;
        aSig[i]     = sRealMag * cos(((i % ROTATE) - sRealAng) * PI / ROTATE * 2) +
                      ((noiselevel * ((rand() % 5001) - 2500)) / 2500.);
    }

    *aRealMag = sRealMag;
    *aRealAng = sRealAng / 1024 * 360;

    sXPos = 0;
    sYPos = 0;

    for(i = 0; i < aCount; i++)
    {
        sXPos += aSig[i] * cos(i * 2 * PI / ROTATE);
        sYPos += aSig[i] * sin(i * 2 * PI / ROTATE);
    }

    sXPos   = sXPos * 2 / aCount;
    sYPos   = sYPos * 2 / aCount;
    sResMag = sqrt(sXPos * sXPos + sYPos * sYPos);
    sResAng = atan2(sYPos, sXPos) * 180 / PI;
    if( sResAng < 0. ) sResAng += 360.;

    *aResMag = sResMag;
    *aResAng = sResAng;

    sleep(2);

    return 0;
}

void outerror(SQLRETURN aRet, char* aCode, char* aMsg)
{
    SQLINTEGER  sErrNo;
    short       sMsgLen;
    char        sErrMsg[1024];
    SQLCHAR     sErrState[6];

    printf(aMsg);

    if( SQLError(gEnv, gConn, NULL, sErrState,
                 &sErrNo, sErrMsg, 1024, &sMsgLen) == SQL_SUCCESS )
    {
        printf("\n%s returned;\n\t[ERR-%05d: %s]\n", aCode, sErrNo, sErrMsg);
    }

    exit(-1);
}

