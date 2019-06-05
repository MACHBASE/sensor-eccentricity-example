#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <machbase_sqlcli.h>

// Please configure your connection info.
#define DB_HOST "127.0.0.1"
#define DB_PORT (5656)

#define DEFAULT_CHECK_COUNT 100
#define ROTATE 1024
#define TOTAL (ROTATE * 10)
#define PI 3.14159265

#define VERIFYERROR(a, conn, stmt, s)     \
    {                               \
        SQLRETURN sRet;             \
        sRet = (a);                 \
        if( sRet == SQL_ERROR )     \
        {                           \
            outerror(sRet, conn, stmt, #a, s);  \
        }                           \
    }

SQLHENV   gEnv;
SQLHDBC   gConn;
SQLHSTMT  gMeasureStmt;
SQLHSTMT  gSensorsStmt;

char*   gMeasureSQL = "SELECT "
                            "eccentric, "
                            "direction "
                        "FROM "
                            "measure_list "
                        "WHERE "
                            "line_id=? "
                        "AND "
                            "ins_time=TO_DATE(?) ";
char*   gSensorsSQL = "SELECT "
                            "encoder_value, "
                            "sensor_value "
                        "FROM "
                            "tag "
                        "WHERE "
                            "line_id=? "
                        "ORDER BY tick_time ";


void outerror(SQLRETURN, SQLHDBC, SQLHSTMT, char*, char*);

int openDBConn(char*, char*, char*, int);
void closeDBConn(void);

int main(int argc, char** argv)
{
    char*   sFileName = NULL;
    int     sCount = -1;

    switch( argc )
    {
    case 5:
        sFileName = argv[4];
    case 4:
        sCount = atoi(argv[3]);
    case 3:
        break;
    default:
        printf("Usage: %s lineid datetime [count] [filename]\n", argv[0]);
        exit(-1);
        break;
    }
    
    if( openDBConn(DB_HOST, "SYS", "MANAGER", DB_PORT) != 0 )
    {
        printf("Open DB connection failed.\n");
        exit(-1);
    }

    retrieve(argv[1], argv[2], sFileName, sCount);
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
                 NULL, NULL,
                 "retrieve connection failed" );
    printf("success!\n");

    return 0;
}

void closeDBConn(void)
{
    VERIFYERROR( SQLDisconnect(gConn), NULL, NULL, "Disconnect failed" );
    VERIFYERROR( SQLFreeConnect(gConn), NULL, NULL, "Free session failed" );
    (void)SQLFreeEnv(gEnv);
}

int retrieve(char* aLineID, char* aTimeStr, char* aOutFile, int aCount)
{
    SQLLEN  sMeasureLen = SQL_NULL_DATA;

    char    sLineID[41];
    char    sTagStr[41];

    SQLLEN  sLineIDLen;
    SQLLEN  sTimeLen;
    SQLLEN  sTagStrLen;

    double  sSensorValue;
    int     sEncoderValue;
    double  sExpectedValue;
    SQLLEN  sSensorLen;
    SQLLEN  sEncoderLen;

    double  sMag;
    double  sAng;
    double  sRad;
    int     sSensorCount;
    int     sLoops = 0;

    FILE*   fp;

    if( aOutFile != NULL )
    {
        fp = fopen(aOutFile, "w");

        if( fp == NULL )
        {
            printf("Error opening [%s](errno=%d)", aOutFile, errno);
            return -1;
        }
    }
    else
    {
        fp = stdout;
    }

    printf("Retrieve from measure_list...");
    VERIFYERROR( SQLAllocStmt(gConn, &gMeasureStmt),
                 gConn, NULL,
                 "measure statement alloc failed");
    VERIFYERROR( SQLPrepare(gMeasureStmt, (SQLCHAR*)gMeasureSQL, SQL_NTS),
                 gConn, gMeasureStmt,
                 "prepare failed" );

    sLineIDLen = strlen(aLineID);
    sTimeLen = strlen(aTimeStr);

    VERIFYERROR( SQLBindParameter(gMeasureStmt,
                                  1,
                                  SQL_PARAM_INPUT,
                                  SQL_C_CHAR,
                                  SQL_VARCHAR,
                                  0,
                                  0,
                                  aLineID,
                                  40,
                                  &sLineIDLen),
                 gConn, gMeasureStmt,
                 "bind parameter failed" );
    VERIFYERROR( SQLBindParameter(gMeasureStmt,
                                  2,
                                  SQL_PARAM_INPUT,
                                  SQL_C_CHAR,
                                  SQL_VARCHAR,
                                  0,
                                  0,
                                  aTimeStr,
                                  40,
                                  &sTimeLen),
                 gConn, gMeasureStmt,
                 "bind parameter failed" );

    VERIFYERROR( SQLExecute(gMeasureStmt), gConn, gMeasureStmt, "execute failed" );
    VERIFYERROR( SQLBindCol(gMeasureStmt, 1, SQL_C_DOUBLE, &sMag, 0, 0),
                 gConn, gMeasureStmt,
                 "bind column failed" );
    VERIFYERROR( SQLBindCol(gMeasureStmt, 2, SQL_C_DOUBLE, &sAng, 0, 0),
                 gConn, gMeasureStmt,
                 "bind column failed" );
    printf("success!\n");

    VERIFYERROR( SQLFetch(gMeasureStmt), gConn, gMeasureStmt, "fetch failed" );

    snprintf(sTagStr, 40, "%s [%s]", aLineID, aTimeStr);
    printf("\tRetrieving...\n", sTagStr);
    sTagStrLen = strlen(sTagStr);
    sRad = sAng * PI / 180;

    VERIFYERROR( SQLAllocStmt(gConn, &gSensorsStmt),
            gConn, NULL,
            "sensors statement alloc failed");
    VERIFYERROR( SQLPrepare(gSensorsStmt, (SQLCHAR*)gSensorsSQL, SQL_NTS),
            gConn, gSensorsStmt,
            "prepare failed" );

    VERIFYERROR( SQLBindParameter(gSensorsStmt,
                1,
                SQL_PARAM_INPUT,
                SQL_C_CHAR,
                SQL_VARCHAR,
                0,
                0,
                sTagStr,
                40,
                &sTagStrLen),
            gConn, gSensorsStmt,
            "bind parameter failed" );

    VERIFYERROR( SQLExecute(gSensorsStmt), gConn, gSensorsStmt, "execute failed" );
    VERIFYERROR( SQLBindCol(gSensorsStmt, 1, SQL_C_LONG,
                &sEncoderValue, 0, NULL),
            gConn, gSensorsStmt,
            "bind column failed" );
    VERIFYERROR( SQLBindCol(gSensorsStmt, 2, SQL_C_DOUBLE,
                &sSensorValue, 0, NULL),
            gConn, gSensorsStmt,
            "bind column failed" );
    sSensorCount = 0;

    while( (SQLFetch(gSensorsStmt) == SQL_SUCCESS) && (aCount != 0) )
    {
        sExpectedValue = sMag * cos((sEncoderValue * 2 * PI / ROTATE) - sRad);
        fprintf(fp, "%g\t%g\t%g\n",
                (sEncoderValue + (sLoops * ROTATE)) / (double)ROTATE,
                sSensorValue,
                sExpectedValue);

        if( sEncoderValue == ROTATE - 1 )
        {
            sLoops++;
        }

        sSensorCount++;
        aCount--;
    }

    printf("\t\"%s\" [%g][%g] => [%d] records retrieved\n", sTagStr, sMag, sAng, sSensorCount);
    VERIFYERROR( SQLFreeStmt(gSensorsStmt, SQL_DROP),
            gConn, NULL,
            "measure statement free failed");

    VERIFYERROR( SQLFreeStmt(gMeasureStmt, SQL_DROP),
                 gConn, NULL,
                 "measure statement free failed");

    if( aOutFile != NULL )
    {
        fclose(fp);
    }
}

void outerror(SQLRETURN aRet, SQLHDBC aConn, SQLHSTMT aStmt, char* aCode, char* aMsg)
{
    SQLINTEGER  sErrNo;
    short       sMsgLen;
    char        sErrMsg[1024];
    SQLCHAR     sErrState[6];

    printf(aMsg);

    if( SQLError(gEnv, aConn, aStmt, sErrState,
                 &sErrNo, sErrMsg, 1024, &sMsgLen) == SQL_SUCCESS )
    {
        printf("\n%s returned;\n\t[ERR-%05d: %s]\n", aCode, sErrNo, sErrMsg);
    }

    exit(-1);
}

