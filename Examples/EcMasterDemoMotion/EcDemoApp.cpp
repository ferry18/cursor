/*-----------------------------------------------------------------------------
 * EcDemoApp.cpp
 * Copyright                acontis technologies GmbH, Weingarten, Germany
 * Response                 Christoph Widmann
 * Description              EC-Master demo application
 *---------------------------------------------------------------------------*/

/*-INCLUDES------------------------------------------------------------------*/

/* EC-Master DAQ Lib */
/* #define INCLUDE_EC_DAQ */

#include "EcDemoApp.h"
#undef DEBUGTRACE
#include "EcFiFo.h"
#include "AtXmlParser.h"
#include "ECMotionControl.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#if (defined INCLUDE_RAS_SERVER)
#include "EcRasServer.h"
#endif

#if (defined INCLUDE_EC_DAQ)
#include "EcDaq.h"
#ifdef _WIN32
#pragma comment(lib, "EcDaq.lib")
#endif
#endif

#include "EcDemoTimingTaskPlatform.h"

/*-DEFINES-------------------------------------------------------------------*/
#define PERF_myAppWorkpd       0
#define PERF_DCM_Logfile       1
#define MAX_JOB_NUM            2

#define MBX_TIMEOUT 5000

#define DCM_ENABLE_LOGFILE
#define MOTION_ENABLE_LOGFILE
#define MOTION_LOG_NUM_MSGS 1000
#undef MOTION_INPOSITION_CHECK
#undef MOTION_ENABLE_TORQUE_LIMIT
#define MOTION_YASK_STAT_TORQUE_LIMIT_ACT  (1 << 14) /* Specific to Yaskawa Sigma V drive */
#define MOTION_YASK_CTRL_TORQUE_LIMIT_EN   (1 << 11 | 1 << 12) /* Specific to Yaskawa Sigma V drive */

/* definitions for motion log file */
#ifdef FILESYS_8_3
#define MOTION_DATALOGGERFILE             (EC_T_CHAR*)"mot"
#else
#define MOTION_DATALOGGERFILE             (EC_T_CHAR*)"motionlog"
#endif
#define MOTION_DATALOGGERFILEEXTENSION    (EC_T_CHAR*)"csv"
#define MOTION_DATALOGGERROLLOVERLIMIT    31000

#define AT_XMLBUF_SIZE 128

// Uncomment to active move sequence test
//#define MOVE_TEST_SEQUENCE
// Uncomment to use ELMO specific functions
//#define ELMO_DRIVE

#ifdef ELMO_DRIVE
#define DRV_OBJ_GAIN_SCHEDULING     0x2E00      /**< Gain scheduling manual index */
#define DRV_OBJ_SMOOTH_FACTOR       0x31D9      /**< Smooth factor index */
#define DRV_OBJ_USER_INT            0x2F00      /**< General purpose user integer array  */
#define DRV_OBJ_USER_FLOAT          0x2F01      /**< General purpose user float array  */
#endif

#ifdef EC_MOTION_SUPPORT_PP_MODE
// Homing on index pulse, negative direction
#define HOMING_MODE                 33
// Homing on index pulse, positive direction
//#define HOMING_MODE                 34
// Speed during search for switch, object 0x6099:1
#define HOMING_SPEED_SEARCH_SWITCH  5000.0
// Speed during search for zero, object 0x6099:2
#define HOMING_SPEED_SEARCH_ZERO    2500.0
// Homing acceleration, object 0x609a:1
#define HOMING_ACCELERATION         10000.0
// Home offset, object 0x607c
#define HOMING_OFFSET               0.0
// Homing "zero" position for FB MC_Home
#define HOMING_POSITION             10.0
#endif

/*-TYPEDEFS------------------------------------------------------------------*/
typedef struct
{
    /* trace file support */
    CAtEmLogging*               poLogging;
    struct _MSG_BUFFER_DESC*    pvLogBufDesc;
} MCLOG_DESC;

/*-LOCAL VARIABLES-----------------------------------------------------------*/
static EC_T_PERF_MEAS_INFO_PARMS S_aPerfMeasInfos[MAX_JOB_NUM] =
{
    {"myAppWorkPd                    ", 0},
    {"Write DCM logfile              ", 0}
};

static MCLOG_DESC  S_McLogDesc[MAX_NUMOF_MASTER_INSTANCES];

/*-FUNCTION DECLARATIONS-----------------------------------------------------*/
static EC_T_VOID  EcMasterJobTask(EC_T_VOID* pvAppContext);
static EC_T_DWORD EcMasterNotifyCallback(EC_T_DWORD dwCode, EC_T_NOTIFYPARMS* pParms);
#if (defined INCLUDE_RAS_SERVER)
static EC_T_DWORD RasNotifyCallback(EC_T_DWORD dwCode, EC_T_NOTIFYPARMS* pParms);
#endif

#ifdef ELMO_DRIVE
MC_T_DWORD SetGainScheduling(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_WORD       wValue              /**< [in] Gain value */
);

MC_T_DWORD SetSmoothFactor(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_DWORD      dwValue             /**< [in] Smooth factor value */
);

MC_T_DWORD SetUserInteger(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_BYTE       bySubIndex,         /**< [in] Subindex 0..24 */
    MC_T_DWORD      dwValue             /**< [in] Smooth factor value */
);

MC_T_DWORD GetUserInteger(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_BYTE       bySubIndex,         /**< [in] Subindex 0..24 */
    MC_T_DWORD* pdwValue            /**< [in] Pointer to buffer */
);

MC_T_DWORD SetUserFloat(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_BYTE       bySubIndex,         /**< [in] Subindex 0..24 */
    MC_T_REAL       fValue              /**< [in] User float value */
);

MC_T_DWORD GetUserFloat(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_BYTE       bySubIndex,         /**< [in] Subindex 0..24 */
    MC_T_REAL* pfValue             /**< [in] Pointer to buffer */
);
#endif

/*-MYAPP---------------------------------------------------------------------*/
/* Demo code: Remove/change this in your application */
/* -MYAPP-DEFINES----------------------------------------------------------- */
/* MotionDemo Command Codes */
#define MC_CMD_CODE_POWER_ON        1
#define MC_CMD_CODE_POWER_OFF       2
#define MC_CMD_CODE_STOP            3
#define MC_CMD_CODE_MOVE_RELATIVE   4
#define MC_CMD_CODE_MOVE_ABSOULTE   5
#define MC_CMD_CODE_READ_POS        6
#define MC_CMD_CODE_MOVE_VELOCITY   7
#define MC_CMD_CODE_RESET           8
#define MC_CMD_CODE_HALT            9
#define MC_CMD_READ_PARAMETER       10
#define MC_CMD_READ_BOOL_PARAMETER  11
#define MC_CMD_WRITE_PARAMETER      12
#define MC_CMD_WRITE_BOOL_PARAMETER 13
#define MC_CMD_HOME                 14

#define MC_CMD_CODE_MOVE_CONT_ABSOLUTE   15
#define MC_CMD_CODE_MOVE_CONT_RELATIVE   16

#define MC_CMD_CODE_GEAR_IN         17
#define MC_CMD_CODE_GEAR_OUT        18

#define MC_CMD_CODE_CAM_TABLE_SELECT 20
#define MC_CMD_CODE_CAM_IN           21
#define MC_CMD_CODE_CAM_OUT          22

/*-PACK SETTINGS-------------------------------------------------------------*/

/* Align/pack structures to 8 byte for proper access to double variables.
 * This is for avoiding misaligned accesses (faults on ARM / PPC) if the
 * object are placed on stack.
 * The cmd structures are manually padded to 8 bytes, so overall structure
 * size is not changed (same as pack 1).
 */
#include EC_PACKED_INCLUDESTART(8)

#if defined  __GNUC__
#  undef EC_PACKED /* GCC has problems with the pack attribute */
#  define EC_PACKED(alignment)  __attribute__(( aligned(alignment) ))
#endif

/* Input command structure from remote application */
typedef struct
{
    EC_T_DWORD      dwCmdCode;
    EC_T_DWORD      dwStationAddress;
    EC_T_DWORD      dwIncPerMM;
    EC_T_DWORD      dwDirection;
    MC_T_REAL       fVel;
    MC_T_REAL       fAcc;
    MC_T_REAL       fDec;
    MC_T_REAL       fJerk;
    MC_T_REAL       fDistance;
    EC_T_DWORD      dwContUpdate;
    EC_T_DWORD      dwParaNumber;       /* Write Parameter: Number */
    EC_T_BOOL       bParaValue;         /* Write Bool Parameter: Value */
    EC_T_DWORD      dwRes4;
    MC_T_REAL       fParaValue;         /* Write Parameter: Value */
    EC_T_DWORD      dwStationAddressMaster; /* for synchronized motion: gearing and camming */
    EC_T_DWORD      dwGearInRatioNumerator; /* MC_GearIn */
    EC_T_DWORD      dwGearInRatioDenoniator; /* MC_GearIn */
    EC_T_BOOL       bCammingPeriodic;   /* MC_CamTableSelect */
    EC_T_DWORD      eCammingStartMode;  /* MC_CamIn:   MC_SM_ABSOLUTE = 0, MC_SM_RELATIVE = 1, MC_SM_RAMP_IN = 2 */
    EC_T_DWORD      dwRes10;
} EC_PACKED(8) MC_T_CMD_INP_PARA_V2;

/* Output command structure to remote application */
typedef struct
{
    MC_T_REAL           fPosition;
    EC_T_WORD           wProfileState;          /* DS 402 or SERCOS drive state   */
    MC_T_WORD           ePLCOpenState;          /* PLCOpen state        */
    MC_T_WORD           wStatusWord;            /* Status Word  0x6041 */
    MC_T_WORD           wControlWord;           /* Control Word 0x6040 */
    EC_T_SDWORD         lActualPosition;        /* Object 0x6064 */
    EC_T_SDWORD         lTargetPosition;        /* Object 0x607A */
} EC_PACKED(8) MC_T_CMD_READ_POS;                            /* Size = 24 bytes */

typedef struct
{
    EC_T_DWORD      dwParaNumber;       /* Read Parameter: Number */
    EC_T_BOOL       bParaValue;         /* Read Bool Parameter: Value */
    MC_T_REAL       fParaValue;         /* Read Parameter: Value */
    MC_T_BOOL       bValid;             /* Value available */
    MC_T_BOOL       bError;             /* Indicates if an error has occurred */
    MC_T_WORD       wErrorID;           /* Error identification */
    MC_T_WORD       wRes;
    EC_T_DWORD      dwRes2;             /* Size = 32 bytes */
} EC_PACKED(8) MC_T_CMD_READ_PARA;

typedef union
{
    MC_T_CMD_READ_POS   oReadPos;
    MC_T_CMD_READ_PARA  oReadPara;
} EC_PACKED(8) MC_T_CMD_OUT_PARA_V2;

/*-PACK SETTINGS-------------------------------------------------------------*/
#include EC_PACKED_INCLUDESTOP

static  CFiFoListDyn<MC_T_CMD_INP_PARA_V2>*    S_pPendMotionCmdFifo = EC_NULL;    /* FIFO containing pending motion commands */

/* Motion Control Function Blocks (MCFB's)
 * This structure contains C++ objects. Don't initialize to zero!
 */
class EC_T_FUNCTION_BLOCKS
{
public:
    MC_T_AXIS_INIT                    AxisInitData;            // MCFB axis initialization data
    MC_T_AXIS_REF                     Axis;                    // MCFB axis reference

    MC_POWER_T                        Power;
    MC_STOP_T                         Stop;
    MC_HALT_T                         Halt;
    MC_RESET_T                        Reset;
    MC_MOVE_RELATIVE_T                MoveRelative;
    MC_MOVE_ABSOLUTE_T                MoveAbsolute;
    MC_MOVE_VELOCITY_T                MoveVelocity;
    MC_READ_ACTUAL_POSITION_T         ReadActualPosition;
    MC_READ_PARAMETER_T               ReadParameter;
    MC_READ_BOOL_PARAMETER_T          ReadBoolParameter;
    MC_WRITE_PARAMETER_T              WriteParameter;
    MC_WRITE_BOOL_PARAMETER_T         WriteBoolParameter;
    MC_READ_MOTION_STATE_T            ReadMotionState;
    MC_READ_AXIS_INFO_T               ReadAxisInfo;
    AMC_CHECK_TARGETPOS_REACHED_T     CheckTargetposReached;
    AMC_HALT_RECOVERY_T               HaltRecovery;
#ifdef EC_MOTION_SUPPORT_PP_MODE
    MC_HOME_T                         Home;
#endif
    MC_READ_DIGITAL_INPUT_T           ReadDigitalInput;
    MC_READ_DIGITAL_OUTPUT_T          ReadDigitalOutput;
    MC_WRITE_DIGITAL_OUTPUT_T         WriteDigitalOutput;
#ifdef EC_MOTION_SUPPORT_MC_CAMMING
    MC_CAMTABLE_SELECT_T              CamTableSelect;
    MC_CAM_IN_T                       CamIn;
    MC_CAM_OUT_T                      CamOut;
#endif
#if defined(EC_MOTION_SUPPORT_MC_GEARING)
    MC_GEAR_IN_T                      GearIn;
    MC_GEAR_OUT_T                     GearOut;
#endif

private:
    EC_T_FUNCTION_BLOCKS& operator=(const EC_T_FUNCTION_BLOCKS&) { return *this; }
};

/* Index for process data variable */
struct EC_T_VARIDX
{
    EC_T_DWORD dwPdoIdx;
    EC_T_DWORD dwVarIdx;
    EC_T_DWORD dwVarSubIdx;
};

enum eMoveState
{
    EMoveSetup,
    EMoving,
    EPause
};

struct EC_T_MOTION_PARAMETER
{
    MC_T_REAL fCommandPos, fCommandVel, fCommandAcc, fCommandJerk;
    MC_T_REAL fFollowingError;
};

struct EC_T_LOGINFO
{
    EC_T_MOTION_PARAMETER curPos;
};

/* data for one axis */
typedef struct
{
    /* read from DemoConfig.xml */
    EC_T_WORD                    wDriveFixAddress;
    EC_T_BYTE                    byDriveModesOfOperation;
    EC_T_BYTE                    bySercosDriveNo;
    EC_T_DWORD                   dwCoeIdxOpMode;
    EC_T_DWORD                   dwCoeSubIdxOpMode;
    EC_T_DWORD                   dwDriveJerk;
    EC_T_DWORD                   dwDriveAcc;
    EC_T_DWORD                   dwDriveDec;
    EC_T_DWORD                   dwDriveVel;
    EC_T_DWORD                   dwDriveDistance;
    EC_T_DWORD                   dwDriveIncPerMM;
    EC_T_DWORD                   dwDriveIncFactor;
    EC_T_DWORD                   dwPosWindow;
    EC_T_DWORD                   dwVelocityGain;        /* CSV-Mode: Velocity Gain */
                                                        /* CSP-Mode: Velocity Gain for Feed Forward Object 0x60B1 */
    MC_T_DWORD                   dwTorqueGain;          /* CSP and CSV-Mode: Torque Gain for Feed Forward Object 0x60B2 */
    EC_T_VARIDX                  IdxStatusword;         /* default for DS402: DRV_OBJ_STATUS_WORD                 0x6041 */
    EC_T_VARIDX                  IdxControlword;        /* default for DS402: DRV_OBJ_CONTROL_WORD                0x6040 */
    EC_T_VARIDX                  IdxActualPos;          /* default for DS402: DRV_OBJ_POSITION_ACTUAL_VALUE       0x6064 */
    EC_T_VARIDX                  IdxTargetPos;          /* default for DS402: DRV_OBJ_TARGET_POSITION             0x607A */
    EC_T_VARIDX                  IdxTargetVel;          /* default for DS402: DRV_OBJ_TARGET_VELOCITY             0x60FF */
    EC_T_VARIDX                  IdxTargetTrq;          /* default for DS402: DRV_OBJ_TARGET_TORQUE               0x6071 */
    EC_T_VARIDX                  IdxActualTrq;          /* default for DS402: DRV_OBJ_ACTUAL_TORQUE               0x6077 */
    EC_T_VARIDX                  IdxVelOffset;          /* default for DS402: DRV_OBJ_VELOCITY_OFFSET             0x60B1 */
    EC_T_VARIDX                  IdxTorOffset;          /* default for DS402: DRV_OBJ_TORQUE_OFFSET               0x60B2 */
    EC_T_VARIDX                  IdxModeOfOperation;    /* default for DS402: DRV_OBJ_MODES_OF_OPERATION          0x6060 */
    EC_T_VARIDX                  IdxModeOfOperationDisplay;    /* default for DS402: DRV_OBJ_MODES_OF_OPERATION_DISPLAY          0x6061 */

    MC_T_AXIS_PROFILE            eDriveProfile;

    /* EtherCAT info */
    EC_T_DWORD                   dwSlaveId;
    EC_T_WORD*                   pwPdStatusWord;         /* Drive status word */
    EC_T_WORD*                   pwPdControlWord;        /* Drive control word */

    EC_T_SDWORD*                 plPdActualPosition;     /* ptr to actual position in process data */
    /* Data type DINT (INTEGER32 -2147483648 to +2147483627) */
    EC_T_SDWORD*                 plPdTargetPosition;     /* ptr to target position: Data type DINT (INTEGER32 -2147483648 to +2147483627)
                                                            In Cyclic Synchronous Position mode, it is always interpreted as an absolute value */

    EC_T_SDWORD*                 plPdTargetVelocity;     /* ptr to target velocity: Data type DINT (INTEGER32 -2147483648 to +2147483627) */

    EC_T_SDWORD*                 plPdVelocityOffset;     /* ptr to velocity offset for feed forward: Data type DINT (INTEGER32 -2147483648 to +2147483627) */
    EC_T_SWORD*                  psPdTorqueOffset;       /* ptr to torque offset for feed forward: Data type INT (INTEGER16 -32768 to +32767) */

    EC_T_SWORD*                  psPdActualTorque;       /* ptr to actual torque: Data type INT (INTEGER16 -32768 to +32767) */
    EC_T_SWORD*                  psPdTargetTorque;       /* ptr to target torque: Data type INT (INTEGER16 -32768 to +32767) */
    EC_T_BYTE*                   pbyPdModeOfOperation;   /* ptr to mode of operation (DS402 0x6060) */
    EC_T_BYTE*                   pbyPdModeOfOperationDisplay;   /* ptr to mode of operation display (DS402 0x6061) */

    EC_T_DWORD*                  pdwDigInputs;
    EC_T_DWORD*                  pdwDigOutputs;

#ifdef EC_MOTION_SUPPORT_PP_MODE
    EC_T_DWORD*                  plPdProfileVelocity;    /* ptr to profile velocity for PP mode and homing mode */
    EC_T_DWORD*                  plPdProfileAcc;         /* ptr to profile acc for PP mode and homing mode */
    EC_T_DWORD*                  plPdProfileDec;         /* ptr to profile dec for PP mode and homing mode */
#endif

    /* MCFB data */
    EC_T_FUNCTION_BLOCKS*        pFb;

    EC_T_BOOL                    bTriggerMoveRel;
    EC_T_BOOL                    bTriggerMoveAbs;
    EC_T_BOOL                    bTriggerMoveVelo;
    EC_T_BOOL                    bTriggerCheckPos;
    EC_T_BOOL                    bTorqueLimitActive;
#ifdef EC_MOTION_SUPPORT_MC_CAMMING
    EC_T_BOOL                    bTriggerCamTableSelect;
    EC_T_BOOL                    bTriggerCamIn;
    EC_T_BOOL                    bTriggerCamOut;
#endif
#ifdef EC_MOTION_SUPPORT_MC_GEARING
    EC_T_BOOL                    bTriggerGearIn;
    EC_T_BOOL                    bTriggerGearOut;
#endif

    /* helpers for autonomous mode */
    EC_T_INT                     eDistanceState;
    eMoveState                   eMotionState;
    EC_T_INT                     nWaitCounter1;
    EC_T_DWORD                   dwMoveStartTime;  /* Track move start time for timeout protection */

    /* Logging */
    EC_T_LOGINFO                 log;
} EC_T_DEMO_AXIS;

#ifdef EC_MOTION_SUPPORT_MC_CAMMING
static MC_T_CAM_REF S_myCamTable;
static MC_T_CAM_ID* S_myCamTableID;
static MC_T_INT     S_aCamPointsExampleInt[][2] = { { 0, 0 },{ 125,707 },{ 250,1000 },{ 375,707 },\
{500,0},{ 625,-707 },{ 750,-1000 },{ 875,-707 },{ 1000,0 } };
/*
static MC_T_REAL    S_aCamPointsExampleReal[][2] = { { 0.0, 0.0 },{ 100.0,100.0 },{ 200.0, 200.0 },{ 300.0, 300.0 },\
{400.0, 400.0},{ 500.0, 500.0 },{ 600.0, 600.0 },{ 700.0, 700.0 },{ 800.0, 800.0 },{ 900.0, 900.0 },{ 1000.0, 1000.0 } }; */
#endif

typedef struct _T_MY_APP_DESC
{
    /* EC-Motion */
    EC_T_VOID* pvCfgFileHandle;
    EC_T_DWORD dwJobTaskCycleCnt;
    EC_T_DWORD dwMotionStartCycle;
    EC_T_DEMO_AXIS aoAxisList[DEMO_MAX_NUM_OF_AXIS];
    EC_T_DWORD dwAxesCnt;

#ifdef MOTION_ENABLE_LOGFILE
    bool       bMotionLogEnabled;
    MC_T_REAL  fCycleTimeS;
#endif
#ifdef EC_SIMULATOR_DS402
    EcSimulatorDemoMotion* pEcSimulatorDemoMotion;
#endif
#if (defined INCLUDE_EC_DAQ)
    EC_T_DAQ_REC hDaq;
#endif
} T_MY_APP_DESC;
static EC_T_DWORD myAppInit(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppPrepare(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppConfigure(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppSetup(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppWorkpd(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppDiagnosis(T_EC_DEMO_APP_CONTEXT* pAppContext);
#if (defined MOVE_TEST_SEQUENCE)
static EC_T_DWORD myAppMoveTest(T_EC_DEMO_APP_CONTEXT* pAppContext);
#endif
static EC_T_DWORD myAppNotify(EC_T_DWORD dwCode, EC_T_NOTIFYPARMS* pParms);

static EC_T_DEMO_AXIS* myAppSearchAxis(T_EC_DEMO_APP_CONTEXT* pAppContext, EC_T_DWORD dwSlaveAddress);

#if (defined MOTION_ENABLE_LOGFILE)
static EC_T_DWORD motionLogEnable(T_EC_DEMO_APP_CONTEXT* pAppContext, EC_T_BYTE* pbyLogBuffer, EC_T_DWORD dwNumMsgs, EC_T_DWORD dwMotionLogBufSize, EC_T_CHAR* szFilenamePrefix);
static EC_T_VOID  motionLogPos(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_VOID motionLogWriteRowHeader(T_EC_DEMO_APP_CONTEXT* pAppContext);
#endif

/*-FUNCTION DEFINITIONS------------------------------------------------------*/

/*
* Reset pending drive errors
*/
static EC_T_VOID ResetDriveErrors(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    for (EC_T_DWORD dwAxesIdx = 0; dwAxesIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxesIdx++)
    {
        EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[dwAxesIdx];

        if (pDemoAxis->pFb->AxisInitData.AxisType == MC_AXIS_TYPE_VIRTUAL) continue;

        pDemoAxis->pFb->ReadAxisInfo.DriveErrorAck = MC_FALSE;

        // DriveErrorAckReq is set to 1 if FB MC_Reset() want's to acknowledge axis errors.
        if (!pDemoAxis->pFb->ReadAxisInfo.DriveErrorAckReq) continue;

        switch (pDemoAxis->eDriveProfile)
        {
        case MC_T_AXIS_PROFILE_SERCOS:
        {
            // S-0-0099: A C1D error is reset if this procedure command is called.
            EC_T_BYTE flags = SOE_BM_ELEMENTFLAG_VALUE;

            EC_T_WORD wCmd = 0x3; // Set & enable cmd
            ecatSoeWrite(pDemoAxis->dwSlaveId, pDemoAxis->bySercosDriveNo, &flags, DRV_IDN_CLEAR_FAULT,
                (EC_T_BYTE*)&wCmd, sizeof(EC_T_WORD), 100);

            wCmd = 0; // Cancel cmd
            ecatSoeWrite(pDemoAxis->dwSlaveId, pDemoAxis->bySercosDriveNo, &flags, DRV_IDN_CLEAR_FAULT,
                (EC_T_BYTE*)&wCmd, sizeof(EC_T_WORD), 100);

            pDemoAxis->pFb->ReadAxisInfo.DriveErrorAck = MC_TRUE;
            break;
        }
        case MC_T_AXIS_PROFILE_DS402:
        {
            // Not implemented
            break;
        }
        default:
            break;
        } // end switch
    }
}

/********************************************************************************/
/** \brief  Motion demo Application.
*
* This is a Motion demo application.
*
* \return  Status value.
*/
EC_T_DWORD EcDemoApp(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_T_DWORD              dwRetVal = EC_E_NOERROR;
    EC_T_DWORD              dwRes = EC_E_NOERROR;

    T_EC_DEMO_APP_PARMS*    pAppParms = &pAppContext->AppParms;
    EC_T_VOID*              pvJobTaskHandle = EC_NULL;

    EC_T_REGISTERRESULTS    oRegisterClientResults;
    OsMemset(&oRegisterClientResults, 0, sizeof(EC_T_REGISTERRESULTS));

    CEcTimer               oAppDuration;

    EC_T_BOOL              bFirstDcmStatus = EC_TRUE;
    CEcTimer               oDcmStatusTimer;

    EC_T_CHAR*              pszCfgBuffer = EC_NULL;
    EC_T_DWORD             dwCfgBufLen = 0;
    EC_T_VOID* pvCfgFileHandle = EC_NULL;

#if (defined INCLUDE_RAS_SERVER)
    EC_T_VOID* pvRasServerHandle = EC_NULL;
#endif

#if (defined INCLUDE_PCAP_RECORDER)
    CPcapRecorder* pPcapRecorder = EC_NULL;
#endif

    /* check link layer parameter */
    if (EC_NULL == pAppParms->apLinkParms[0])
    {
        dwRetVal = EC_E_INVALIDPARM;
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Missing link layer parameter\n"));
        goto Exit;
    }

    /* check if polling mode is selected */
    if (pAppParms->apLinkParms[0]->eLinkMode != EcLinkMode_POLLING)
    {
        dwRetVal = EC_E_INVALIDPARM;
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Link layer in 'interrupt' mode is not supported by %s. Please select 'polling' mode.\n", EC_DEMO_APP_NAME));
        goto Exit;
    }

    pAppContext->pNotificationHandler = EC_NEW(CEmNotification(pAppContext));
    if (EC_NULL == pAppContext->pNotificationHandler)
    {
        dwRetVal = EC_E_NOMEMORY;
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot create notification handler\n"));
        goto Exit;
    }

    pAppContext->pMyAppDesc = (T_MY_APP_DESC*)OsMalloc(sizeof(T_MY_APP_DESC));
    if (EC_NULL == pAppContext->pMyAppDesc)
    {
        dwRetVal = EC_E_NOMEMORY;
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot create myApp descriptor\n"));
        goto Exit;
    }
    OsMemset(pAppContext->pMyAppDesc, 0, sizeof(T_MY_APP_DESC));

#if (defined EC_SIMULATOR_DS402)
    pAppContext->pMyAppDesc->pEcSimulatorDemoMotion = EC_NEW(EcSimulatorDemoMotion());
    if (EC_NULL == pAppContext->pMyAppDesc->pEcSimulatorDemoMotion)
    {
        dwRetVal = EC_E_NOMEMORY;
        goto Exit;
    }
#endif

    if (pAppContext->AppParms.dwPerfMeasLevel > 0)
    {
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "OsMeasGet100kHzFrequency(): %d MHz\n", OsMeasGet100kHzFrequency() / 10));
    }

    /* create command fifo list */
    S_pPendMotionCmdFifo = EC_NEW(CFiFoListDyn<MC_T_CMD_INP_PARA_V2>(32, EC_NULL, (EC_T_CHAR*)"PendMotionCmd"));
    if (S_pPendMotionCmdFifo == EC_NULL)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Can not allocate motion command queue!\n"));
        goto Exit;
    }
    S_pPendMotionCmdFifo->InitInstance(32);

#if (defined STATIC_DEMOCONFIG_XML_DATA)
    pszCfgBuffer = (EC_T_CHAR*)STATIC_DEMOCONFIG_XML_DATA;
    dwCfgBufLen = STATIC_DEMOCONFIG_XML_DATA_SIZE;
#endif
    /* open config file */
    if (pszCfgBuffer != EC_NULL)
    {
        if (atOpenXmlBuffer(pszCfgBuffer, dwCfgBufLen, &pvCfgFileHandle) != EC_E_NOERROR)
        {
            OsPrintf("ERROR: Can't open demo configuration from buffer\n");
            dwRetVal = EC_E_ACCESSDENIED;
            goto Exit;
        }
    }
#if (!defined STATIC_DEMOCONFIG_XML_DATA)
    else if (atOpenXmlFile(pAppContext->AppParms.pszCfgFilePath, &pvCfgFileHandle) != EC_E_NOERROR)
    {
        OsPrintf("ERROR: Can't open demo configuration file %s\n", pAppContext->AppParms.pszCfgFilePath);
        dwRetVal = EC_E_ACCESSDENIED;
        goto Exit;
    }
#endif
    pAppContext->pMyAppDesc->pvCfgFileHandle = pvCfgFileHandle;

    /* Get DCM control setval */
    atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, MOTIONDEMO_CFG_TAG_CTL_SETVAL, (EC_T_BYTE*)&pAppParms->nCtlSetValNsec, sizeof(EC_T_INT));

    /* Enables the Command Mode */
    atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_BOOL, MOTIONDEMO_CFG_TAG_CMD_MODE, (EC_T_BYTE*)&pAppParms->bCmdMode, sizeof(EC_T_BOOL));

    if (pAppParms->bCmdMode)
    {
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "==========================================================\n"));
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Command mode enabled! Motion operation controlled remotely\n"));
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "==========================================================\n"));
    }
#if (defined MOTION_ENABLE_LOGFILE)
    if (pAppContext->AppParms.nVerbose >= 3)
    {
        motionLogEnable(pAppContext, EC_NULL, MOTION_LOG_NUM_MSGS, 0, pAppContext->AppParms.szLogFileprefix);
        pAppContext->pMyAppDesc->fCycleTimeS = pAppContext->AppParms.dwBusCycleTimeUsec / 1000000.0;
    }
#endif

    dwRes = myAppInit(pAppContext);
    if (EC_E_NOERROR != dwRes)
    {
        dwRetVal = dwRes;
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: myAppInit failed: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        goto Exit;
    }

#if (defined INCLUDE_RAS_SERVER)
    /* start RAS server if enabled */
    if (pAppParms->bStartRasServer)
    {
        ECMASTERRAS_T_SRVPARMS oRemoteApiConfig;

        OsMemset(&oRemoteApiConfig, 0, sizeof(ECMASTERRAS_T_SRVPARMS));
        oRemoteApiConfig.dwSignature        = ECMASTERRASSERVER_SIGNATURE;
        oRemoteApiConfig.dwSize             = sizeof(ECMASTERRAS_T_SRVPARMS);
        oRemoteApiConfig.oAddr.dwAddr       = 0;                            /* INADDR_ANY */
        oRemoteApiConfig.wPort              = pAppParms->wRasServerPort;
        oRemoteApiConfig.dwCycleTime        = ECMASTERRAS_CYCLE_TIME;
        oRemoteApiConfig.dwCommunicationTimeout = ECMASTERRAS_MAX_WATCHDOG_TIMEOUT;
        oRemoteApiConfig.oAcceptorThreadCpuAffinityMask = pAppParms->CpuSet;
        oRemoteApiConfig.dwAcceptorThreadPrio           = MAIN_THREAD_PRIO;
        oRemoteApiConfig.dwAcceptorThreadStackSize      = JOBS_THREAD_STACKSIZE;
        oRemoteApiConfig.oClientWorkerThreadCpuAffinityMask = pAppParms->CpuSet;
        oRemoteApiConfig.dwClientWorkerThreadPrio           = MAIN_THREAD_PRIO;
        oRemoteApiConfig.dwClientWorkerThreadStackSize      = JOBS_THREAD_STACKSIZE;
        oRemoteApiConfig.pfnRasNotify    = RasNotifyCallback;                       /* RAS notification callback function */
        oRemoteApiConfig.pvRasNotifyCtxt = pAppContext->pNotificationHandler;       /* RAS notification callback function context */
        oRemoteApiConfig.dwMaxQueuedNotificationCnt = 100;                          /* pre-allocation */
        oRemoteApiConfig.dwMaxParallelMbxTferCnt    = 50;                           /* pre-allocation */
        oRemoteApiConfig.dwCycErrInterval           = 500;                          /* span between to consecutive cyclic notifications of same type */

        if (pAppParms->nVerbose >= 1)
        {
            OsMemcpy(&oRemoteApiConfig.LogParms, &pAppContext->LogParms, sizeof(EC_T_LOG_PARMS));
            oRemoteApiConfig.LogParms.dwLogLevel = EC_LOG_LEVEL_ERROR;
        }
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Start Remote API Server now\n"));
        dwRes = emRasSrvStart(&oRemoteApiConfig, &pvRasServerHandle);
        if (EC_E_NOERROR != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot spawn Remote API Server\n"));
        }
    }
#endif

    /* initialize EtherCAT master */
    {
        EC_T_INIT_MASTER_PARMS oInitParms;

        OsMemset(&oInitParms, 0, sizeof(EC_T_INIT_MASTER_PARMS));
        oInitParms.dwSignature                   = ATECAT_SIGNATURE;
        oInitParms.dwSize                        = sizeof(EC_T_INIT_MASTER_PARMS);
        oInitParms.pOsParms                      = &pAppParms->Os;
        oInitParms.pLinkParms                    = pAppParms->apLinkParms[0];
        oInitParms.pLinkParmsRed                 = pAppParms->apLinkParms[1];
        oInitParms.dwBusCycleTimeUsec            = pAppParms->dwBusCycleTimeUsec;
        oInitParms.dwMaxBusSlaves                = pAppParms->dwMaxBusSlaves;
        oInitParms.dwMaxAcycFramesQueued         = MASTER_CFG_MAX_ACYC_FRAMES_QUEUED;
        oInitParms.dwMaxS2SMbxSize               = 1024;     /* required for Beckhoff AX8xxx drives */
        oInitParms.dwMaxQueuedS2SMbxTfer         = 16;
        if (oInitParms.dwBusCycleTimeUsec >= 1000)
        {
            oInitParms.dwMaxAcycBytesPerCycle    = MASTER_CFG_MAX_ACYC_BYTES_PER_CYC;
            oInitParms.dwMaxAcycFramesPerCycle   = 4;
        }
        else
        {
            oInitParms.dwMaxAcycBytesPerCycle    = 1500;
            oInitParms.dwMaxAcycFramesPerCycle   = 1;
            oInitParms.dwMaxAcycCmdsPerCycle     = 20;
            oInitParms.bNoConsecutiveAcycFrames  = EC_TRUE;
        }
        oInitParms.dwEcatCmdMaxRetries           = MASTER_CFG_MAX_ACYC_CMD_RETRIES;
        if (pAppContext->AppParms.nVerbose >= 1)
        {
            OsMemcpy(&oInitParms.LogParms, G_pEcLogParms, sizeof(EC_T_LOG_PARMS));
            switch (pAppContext->AppParms.nVerbose)
            {
            case 0: oInitParms.LogParms.dwLogLevel = EC_LOG_LEVEL_SILENT;      break;
            case 1: oInitParms.LogParms.dwLogLevel = EC_LOG_LEVEL_WARNING;     break;
            case 2: oInitParms.LogParms.dwLogLevel = EC_LOG_LEVEL_WARNING;     break;
            case 3: oInitParms.LogParms.dwLogLevel = EC_LOG_LEVEL_WARNING;     break;
            case 4: oInitParms.LogParms.dwLogLevel = EC_LOG_LEVEL_INFO;        break;
            case 5: oInitParms.LogParms.dwLogLevel = EC_LOG_LEVEL_VERBOSE;     break;
            default: /* no break */
            case 6: oInitParms.LogParms.dwLogLevel = EC_LOG_LEVEL_VERBOSE_CYC; break;
            }
        }
        if (pAppParms->dwPerfMeasLevel > 0)
        {
            oInitParms.PerfMeasInternalParms.bEnabled = EC_TRUE;

            if (pAppParms->dwPerfMeasLevel > 1)
            {
                oInitParms.PerfMeasInternalParms.HistogramParms.dwBinCount = 202;
            }
        }
        else
        {
            oInitParms.PerfMeasInternalParms.bEnabled = EC_FALSE;
        }

        dwRes = ecatInitMaster(&oInitParms);
        if (dwRes != EC_E_NOERROR)
        {
            dwRetVal = dwRes;
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot initialize EtherCAT-Master: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
        if (0 != OsStrlen(pAppParms->szLicenseKey))
        {
            dwRes = ecatSetLicenseKey(pAppParms->szLicenseKey);
            if (dwRes != EC_E_NOERROR)
            {
                dwRetVal = dwRes;
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot set license key: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
                goto Exit;
            }
        }
    }

    /* initalize performance measurement */
    if (pAppParms->dwPerfMeasLevel > 0)
    {
        EC_T_PERF_MEAS_APP_PARMS oPerfMeasAppParms;
        OsMemset(&oPerfMeasAppParms, 0, sizeof(EC_T_PERF_MEAS_APP_PARMS));
        oPerfMeasAppParms.dwNumMeas = MAX_JOB_NUM;
        oPerfMeasAppParms.aPerfMeasInfos = S_aPerfMeasInfos;
        if (pAppParms->dwPerfMeasLevel > 1)
        {
            oPerfMeasAppParms.HistogramParms.dwBinCount = 202;
        }

        dwRes = ecatPerfMeasAppCreate(&oPerfMeasAppParms, &pAppContext->pvPerfMeas);
        if (dwRes != EC_E_NOERROR)
        {
            dwRetVal = dwRes;
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot initialize app performance measurement: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
        pAppContext->dwPerfMeasLevel = pAppParms->dwPerfMeasLevel;
    }

#if (defined INCLUDE_EC_DAQ)
    {
        /* initalize DAQ library */
        EC_T_DAQ_FP oEcDaqFp;
        EC_T_DAQ_REC_PARMS oEcDaqParms;

        OsMemset(&oEcDaqFp, 0, sizeof(EC_T_DAQ_FP));
        oEcDaqFp.RegisterClient = emRegisterClient;
        oEcDaqFp.UnregisterClient = emUnregisterClient;
        oEcDaqFp.GetCfgSlaveInfo = emGetCfgSlaveInfo;
        oEcDaqFp.GetSlaveInpVarInfoNumOf = emGetSlaveInpVarInfoNumOf;
        oEcDaqFp.GetSlaveInpVarInfo = emGetSlaveInpVarInfoEx;
        oEcDaqFp.GetSlaveOutpVarInfoNumOf = emGetSlaveOutpVarInfoNumOf;
        oEcDaqFp.GetSlaveOutpVarInfo = emGetSlaveOutpVarInfoEx;
        oEcDaqFp.SystemTimeGet = OsSystemTimeGet;
        oEcDaqFp.QueryMsecCount = OsQueryMsecCount;

        OsMemset(&oEcDaqParms, 0, sizeof(EC_T_DAQ_REC_PARMS));
        oEcDaqParms.dwMasterInstanceId = INSTANCE_MASTER_DEFAULT;
        oEcDaqParms.dwBusCycleTimeUsec = pDemoTimingTaskPlatform->nCycleTimeNsec / 1000;
        oEcDaqParms.bCycleCounter = EC_TRUE;
        oEcDaqParms.bElapsedTimeMsec = EC_TRUE;
        oEcDaqParms.bElapsedTimeUsec = EC_FALSE;

        /* thread */
        oEcDaqParms.dwThreadCpuSet = CpuSet;
        oEcDaqParms.dwThreadPrio = LOG_THREAD_PRIO;
        oEcDaqParms.dwThreadStackSize = LOG_THREAD_STACKSIZE;

        /* logging */
        if (1 <= nVerbose)
        {
            OsMemcpy(&oEcDaqParms.LogParms, G_pEcLogParms, sizeof(EC_T_LOG_PARMS));
            oEcDaqParms.LogParms.dwLogLevel = EC_LOG_LEVEL_ERROR;
        }

        /* writer */
        OsStrcpy(oEcDaqParms.szWriter, "MDF");
        OsStrcpy(oEcDaqParms.szName, "EcMasterDemoDaq Example");
        OsStrcpy(oEcDaqParms.szFile, "ecmaster_daq.mf4");

        /* initialize recorder */
        dwRes = ecDaqRecCreate(&pAppContext->hDaq, &oEcDaqFp, &oEcDaqParms);
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqRecCreate: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
    }
#endif

    /* print MAC address */
    {
        ETHERNET_ADDRESS oSrcMacAddress;
        OsMemset(&oSrcMacAddress, 0, sizeof(ETHERNET_ADDRESS));

        dwRes = ecatGetSrcMacAddress(&oSrcMacAddress);
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot get MAC address: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        }
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "EtherCAT network adapter MAC: %02X-%02X-%02X-%02X-%02X-%02X\n",
            oSrcMacAddress.b[0], oSrcMacAddress.b[1], oSrcMacAddress.b[2], oSrcMacAddress.b[3], oSrcMacAddress.b[4], oSrcMacAddress.b[5]));
    }

    /* EtherCAT traffic logging */
#if (defined INCLUDE_PCAP_RECORDER)
    if (pAppParms->bPcapRecorder)
    {
        pPcapRecorder = EC_NEW(CPcapRecorder());
        if (EC_NULL == pPcapRecorder)
        {
            dwRetVal = EC_E_NOMEMORY;
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: %d: Creating PcapRecorder failed: %s (0x%lx)\n", pAppContext->dwInstanceId, ecatGetText(dwRetVal), dwRetVal));
            goto Exit;
        }
        dwRes = pPcapRecorder->InitInstance(pAppContext->dwInstanceId, pAppParms->dwPcapRecorderBufferFrameCnt, pAppParms->szPcapRecorderFileprefix);
        if (dwRes != EC_E_NOERROR)
        {
            dwRetVal = dwRes;
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: %d: Initialize PcapRecorder failed: %s (0x%lx)\n", pAppContext->dwInstanceId, ecatGetText(dwRes), dwRes));
            goto Exit;
        }
    }
#endif /* INCLUDE_PCAP_RECORDER */

    /* create cyclic task to trigger jobs */
    {
        CEcTimer oTimeout(2000);

        pAppContext->bJobTaskRunning = EC_FALSE;
        pAppContext->bJobTaskShutdown = EC_FALSE;
        pvJobTaskHandle = OsCreateThread((EC_T_CHAR*)"EcMasterJobTask", EcMasterJobTask, pAppParms->CpuSet,
            pAppParms->dwJobsThreadPrio, pAppParms->dwJobsThreadStackSize, (EC_T_VOID*)pAppContext);

        /* wait until thread is running */
        while (!oTimeout.IsElapsed() && !pAppContext->bJobTaskRunning)
        {
            OsSleep(10);
        }
        if (!pAppContext->bJobTaskRunning)
        {
            dwRetVal = EC_E_TIMEOUT;
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Timeout starting JobTask\n"));
            goto Exit;
        }
    }

    /* set OEM key if available */
    if (0 != pAppParms->qwOemKey)
    {
        dwRes = ecatSetOemKey(pAppParms->qwOemKey);
        if (dwRes != EC_E_NOERROR)
        {
            dwRetVal = dwRes;
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot set OEM key at master: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
    }
    if (pAppParms->eJunctionRedMode != eJunctionRedundancyMode_Disabled)
    {
        dwRes = ecatIoCtl(EC_IOCTL_SB_SET_JUNCTION_REDUNDANCY_MODE, &pAppParms->eJunctionRedMode, sizeof(EC_T_JUNCTION_REDUNDANCY_MODE), EC_NULL, 0, EC_NULL);
        if (dwRes != EC_E_NOERROR)
        {
            dwRetVal = dwRes;
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot configure junction redundancy mode: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
    }
    /* configure network */
    dwRes = ecatConfigureNetwork(pAppParms->eCnfType, pAppParms->pbyCnfData, pAppParms->dwCnfDataLen);
    if (dwRes != EC_E_NOERROR)
    {
        dwRetVal = dwRes;
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot configure EtherCAT-Master: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        goto Exit;
    }

    dwRes = myAppConfigure(pAppContext);
    if (EC_E_NOERROR != dwRes)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, (EC_T_CHAR*)"myAppConfigure failed: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        dwRetVal = dwRes;
        goto Exit;
    }

    /* register client */
    dwRes = ecatRegisterClient(EcMasterNotifyCallback, pAppContext, &oRegisterClientResults);
    if (dwRes != EC_E_NOERROR)
    {
        dwRetVal = dwRes;
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot register client: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        goto Exit;
    }
    pAppContext->pNotificationHandler->SetClientID(oRegisterClientResults.dwClntId);

    /* configure DC/DCM master is started with ENI */
    if (EC_NULL != pAppParms->pbyCnfData)
    {
#if (defined INCLUDE_DC_SUPPORT)
        /* configure DC */
        {
            EC_T_DC_CONFIGURE oDcConfigure;

            OsMemset(&oDcConfigure, 0, sizeof(EC_T_DC_CONFIGURE));
            oDcConfigure.dwTimeout          = ETHERCAT_DC_TIMEOUT;
            oDcConfigure.dwDevLimit         = ETHERCAT_DC_DEV_LIMIT;
            oDcConfigure.dwSettleTime       = ETHERCAT_DC_SETTLE_TIME;
            oDcConfigure.dwTotalBurstLength = ETHERCAT_DC_ARMW_BURSTCYCLES;
            if (pAppParms->dwBusCycleTimeUsec < 1000)
            {
                /* reduce frames sent within each cycle for cycle time below 1 ms */
                oDcConfigure.dwBurstBulk = ETHERCAT_DC_ARMW_BURSTSPP / 2;
            }
            else
            {
                oDcConfigure.dwBurstBulk = ETHERCAT_DC_ARMW_BURSTSPP;
            }
            /* for short cycle times the cyclic distribution is sufficient. disable acyclic distribution to reduce CPU load. */
            if ((pAppParms->dwBusCycleTimeUsec < 2000) && (eDcmMode_Dcx != pAppParms->eDcmMode))
            {
                oDcConfigure.bAcycDistributionDisabled = EC_TRUE;
            }

            dwRes = ecatDcConfigure(&oDcConfigure);
            if (EC_E_NOERROR != dwRes)
            {
                dwRetVal = dwRes;
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot configure DC! (Result = 0x%x)\n", dwRes));
                goto Exit;
            }
        }
        /* configure DCM */
        if (pAppParms->bDcmLogEnabled && !pAppParms->bDcmConfigure)
        {
            EC_T_BOOL bBusShiftConfiguredByEni = EC_FALSE;
            dwRes = ecatDcmGetBusShiftConfigured(&bBusShiftConfiguredByEni);
            if (dwRes != EC_E_NOERROR)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot check if BusShift is configured  (Result = 0x%x)\n", dwRes));
            }
            if (bBusShiftConfiguredByEni)
            {
                pAppParms->bDcmConfigure = EC_TRUE;
                pAppParms->eDcmMode = eDcmMode_BusShift;
            }
        }
        if (pAppParms->bDcmConfigure)
        {
            EC_T_DWORD dwCycleTimeNsec   = pAppParms->dwBusCycleTimeUsec * 1000; /* cycle time in nsec */
            EC_T_INT   nCtlSetValNsec    = dwCycleTimeNsec * 2 / 3 /* 66% */;    /* distance between cyclic frame send time and DC base on bus */
            EC_T_DWORD dwInSyncLimitNsec = dwCycleTimeNsec / 4 /* 25% */;        /* limit for DCM InSync monitoring */
            if (0 != pAppParms->nCtlSetValNsec)
            {
                nCtlSetValNsec = pAppParms->nCtlSetValNsec;
            }

            EC_T_DCM_CONFIG oDcmConfig;
            OsMemset(&oDcmConfig, 0, sizeof(EC_T_DCM_CONFIG));

            switch (pAppParms->eDcmMode)
            {
            case eDcmMode_Off:
                oDcmConfig.eMode = eDcmMode_Off;
                break;
            case eDcmMode_BusShift:
                oDcmConfig.eMode = eDcmMode_BusShift;
                oDcmConfig.u.BusShift.nCtlSetVal    = nCtlSetValNsec;
                oDcmConfig.u.BusShift.dwInSyncLimit = dwInSyncLimitNsec;
                oDcmConfig.u.BusShift.bLogEnabled = pAppParms->bDcmLogEnabled;
                oDcmConfig.u.BusShift.pGetTimeElapsedSinceCycleStartContext = pAppContext->pTimingTaskContext;
                if (pAppParms->bDcmSyncToCycleStart)
                {
                    oDcmConfig.u.BusShift.pfnGetTimeElapsedSinceCycleStart = CDemoTimingTaskPlatform::GetTimeElapsedSinceCycleStart;
                }
                if (pAppParms->bDcmControlLoopDisabled)
                {
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCM control loop disabled\n"));
                    oDcmConfig.u.BusShift.bCtlOff = EC_TRUE;
                }
                break;
            case eDcmMode_MasterShift:
                oDcmConfig.eMode = eDcmMode_MasterShift;
                oDcmConfig.u.MasterShift.nCtlSetVal    = nCtlSetValNsec;
                oDcmConfig.u.MasterShift.dwInSyncLimit = dwInSyncLimitNsec;
                oDcmConfig.u.MasterShift.bLogEnabled = pAppParms->bDcmLogEnabled;
                oDcmConfig.u.MasterShift.pGetTimeElapsedSinceCycleStartContext = pAppContext->pTimingTaskContext;
                if (pAppParms->bDcmSyncToCycleStart)
                {
                    oDcmConfig.u.MasterShift.pfnGetTimeElapsedSinceCycleStart = CDemoTimingTaskPlatform::GetTimeElapsedSinceCycleStart;
                }
                oDcmConfig.u.MasterShift.pAdjustCycleTimeContext = pAppContext->pTimingTaskContext;
                oDcmConfig.u.MasterShift.pfnAdjustCycleTime = CDemoTimingTaskPlatform::AdjustCycleTime;
                if (pAppParms->bDcmControlLoopDisabled)
                {
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCM control loop disabled\n"));
                    oDcmConfig.u.MasterShift.bCtlOff = EC_TRUE;
                }
                break;
            case eDcmMode_MasterRefClock:
                oDcmConfig.eMode = eDcmMode_MasterRefClock;
                oDcmConfig.u.MasterRefClock.nCtlSetVal  = nCtlSetValNsec;
                oDcmConfig.u.MasterRefClock.dwInSyncLimit = dwInSyncLimitNsec;
                oDcmConfig.u.MasterRefClock.bLogEnabled = pAppParms->bDcmLogEnabled;
                oDcmConfig.u.MasterRefClock.pGetHostTimeContext = pAppContext->pTimingTaskContext;
                oDcmConfig.u.MasterRefClock.pfnGetHostTime = CDemoTimingTaskPlatform::GetHostTime;
                break;
            case eDcmMode_LinkLayerRefClock:
                oDcmConfig.eMode = eDcmMode_LinkLayerRefClock;
                oDcmConfig.u.LinkLayerRefClock.nCtlSetVal = nCtlSetValNsec;
                oDcmConfig.u.LinkLayerRefClock.dwInSyncLimit = dwInSyncLimitNsec;
                oDcmConfig.u.LinkLayerRefClock.bLogEnabled = pAppParms->bDcmLogEnabled;
                break;
            case eDcmMode_Dcx:
                oDcmConfig.eMode = eDcmMode_Dcx;
                /* DCX MasterShift */
                oDcmConfig.u.Dcx.MasterShift.nCtlSetVal = nCtlSetValNsec;
                oDcmConfig.u.Dcx.MasterShift.dwInSyncLimit = dwInSyncLimitNsec;
                oDcmConfig.u.Dcx.MasterShift.bLogEnabled = pAppParms->bDcmLogEnabled;
                oDcmConfig.u.Dcx.MasterShift.pGetTimeElapsedSinceCycleStartContext = pAppContext->pTimingTaskContext;
                if (pAppParms->bDcmSyncToCycleStart)
                {
                    oDcmConfig.u.Dcx.MasterShift.pfnGetTimeElapsedSinceCycleStart = CDemoTimingTaskPlatform::GetTimeElapsedSinceCycleStart;
                }
                oDcmConfig.u.Dcx.MasterShift.pAdjustCycleTimeContext = pAppContext->pTimingTaskContext;
                oDcmConfig.u.Dcx.MasterShift.pfnAdjustCycleTime = CDemoTimingTaskPlatform::AdjustCycleTime;
                /* DCX BusShift */
                oDcmConfig.u.Dcx.nCtlSetVal = nCtlSetValNsec;
                oDcmConfig.u.Dcx.dwInSyncLimit = dwInSyncLimitNsec;
                oDcmConfig.u.Dcx.bLogEnabled = pAppParms->bDcmLogEnabled;
                oDcmConfig.u.Dcx.dwExtClockTimeout = 1000;
                oDcmConfig.u.Dcx.wExtClockFixedAddr = 0; /* 0 only when clock adjustment in external mode configured by EcEngineer */
                if (pAppParms->bDcmControlLoopDisabled)
                {
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCM control loop disabled\n"));

                    oDcmConfig.u.Dcx.MasterShift.bCtlOff = EC_TRUE;
                    oDcmConfig.u.Dcx.bCtlOff = EC_TRUE;
                }
                break;
            default:
                dwRetVal = EC_E_NOTSUPPORTED;
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "DCM mode is not supported!\n"));
                goto Exit;

            }
            dwRes = ecatDcmConfigure(&oDcmConfig, 0);
            switch (dwRes)
            {
            case EC_E_NOERROR:
                break;
            case EC_E_FEATURE_DISABLED:
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot configure DCM mode!\n"));
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Start with -dcmmode off to run the DC demo without DCM, or prepare the ENI file to support the requested DCM mode\n"));
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "In ET9000 for example, select under ""Advanced settings\\Distributed clocks"" ""DC in use"" and ""Slave Mode""\n"));
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "to support BusShift and MasterRefClock modes.\n"));
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Please refer to the class A manual for further information\n"));
                dwRetVal = dwRes;
                goto Exit;
            default:
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot configure DCM mode! %s (Result = 0x%x)\n", ecatGetText(dwRes), dwRes));
                dwRetVal = dwRes;
                goto Exit;
            }
        }
#endif /* INCLUDE_DC_SUPPORT */
    }
#if (defined INCLUDE_SLAVE_STATISTICS)
    /* enable and reset statistics */
    {
        EC_T_DWORD dwPeriodMs = 1000;

        dwRes = ecatIoCtl(EC_IOCTL_SET_SLVSTAT_PERIOD, (EC_T_BYTE*)&dwPeriodMs, sizeof(EC_T_DWORD), EC_NULL, 0, EC_NULL);
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot set slave statistics period: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        }
        dwRes = ecatClearSlaveStatistics(INVALID_SLAVE_ID);
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot reset slave statistics: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        }
    }
#endif

    /* print found slaves */
    if (pAppParms->dwAppLogLevel >= EC_LOG_LEVEL_VERBOSE)
    {
        dwRes = ecatScanBus(ETHERCAT_SCANBUS_TIMEOUT);
        pAppContext->pNotificationHandler->ProcessNotificationJobs();
        switch (dwRes)
        {
        case EC_E_NOERROR:
        case EC_E_BUSCONFIG_MISMATCH:
        case EC_E_LINE_CROSSED:
            PrintSlaveInfos(pAppContext);
            break;
        default:
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot scan bus: %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
            break;
        }
        if (dwRes != EC_E_NOERROR)
        {
            dwRetVal = dwRes;
            goto Exit;
        }
    }

    /* print process variable name and offset for all variables of all slaves */
    if (pAppContext->AppParms.bPrintVars)
    {
        PrintAllSlavesProcVarInfos(pAppContext);
    }

    /* set master to INIT */
    dwRes = ecatSetMasterState(ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_INIT);
    pAppContext->pNotificationHandler->ProcessNotificationJobs();
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot start set master state to INIT: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        dwRetVal = dwRes;
        goto Exit;
    }

    dwRes = myAppPrepare(pAppContext);
    if (EC_E_NOERROR != dwRes)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: myAppPrepare failed: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        dwRetVal = dwRes;
        goto Exit;
    }

#if (defined INCLUDE_EC_DAQ)
    /* apply configuration to recorder */
    dwRes = ecDaqConfigApply(pAppContext->hDaq);
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqConfigApply: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        goto Exit;
    }

    /* start recorder */
    dwRes = ecDaqRecStart(pAppContext->hDaq);
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqRecStart: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        goto Exit;
    }
#endif

    /* set master to PREOP */
    dwRes = ecatSetMasterState(ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_PREOP);
    pAppContext->pNotificationHandler->ProcessNotificationJobs();
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot start set master state to PREOP: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        dwRetVal = dwRes;
        goto Exit;
    }

    /* skip this step if demo started without ENI */
    if (EC_NULL != pAppParms->pbyCnfData)
    {
        dwRes = myAppSetup(pAppContext);
        if (EC_E_NOERROR != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "myAppSetup failed: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            dwRetVal = dwRes;
            goto Exit;
        }

        /* set master to SAFEOP */
        dwRes = ecatSetMasterState(ETHERCAT_DCM_TIMEOUT + ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_SAFEOP);
        pAppContext->pNotificationHandler->ProcessNotificationJobs();
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot start set master state to SAFEOP: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));

            /* most of the time SAFEOP is not reachable due to DCM */
            if ((eDcmMode_Off != pAppParms->eDcmMode) && (eDcmMode_LinkLayerRefClock != pAppParms->eDcmMode))
            {
                EC_T_DWORD dwStatus = 0;
                EC_T_INT   nDiffCur = 0, nDiffAvg = 0, nDiffMax = 0;

                dwRes = ecatDcmGetStatus(&dwStatus, &nDiffCur, &nDiffAvg, &nDiffMax);
                if (dwRes == EC_E_NOERROR)
                {
                    if (dwStatus != EC_E_NOERROR)
                    {
                        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "DCM Status: %s (0x%08X)\n", ecatGetText(dwStatus), dwStatus));
                    }
                }
                else
                {
                    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot get DCM status! %s (0x%08X)\n", ecatGetText(dwRes), dwRes));
                }
            }
            dwRetVal = dwRes;
            goto Exit;
        }

        /* set master to OP */
        dwRes = ecatSetMasterState(ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_OP);
        pAppContext->pNotificationHandler->ProcessNotificationJobs();
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot start set master state to OP: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            dwRetVal = dwRes;
            goto Exit;
        }
    }
    else
    {
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "No ENI file provided. EC-Master started with generated ENI file.\n"));
    }

    if (pAppContext->dwPerfMeasLevel > 0)
    {
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "\nJob times during startup <INIT> to <%s>:\n", ecatStateToStr(ecatGetMasterState())));
        PRINT_PERF_MEAS();
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "\n"));
        /* clear job times of startup phase */
        ecatPerfMeasAppReset(pAppContext->pvPerfMeas, EC_PERF_MEAS_ALL);
        ecatPerfMeasReset(EC_PERF_MEAS_ALL);
    }
    /* run the demo */
    if (pAppParms->dwDemoDuration != 0)
    {
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "%s will stop in %ds...\n", EC_DEMO_APP_NAME, pAppParms->dwDemoDuration / 1000));
        oAppDuration.Start(pAppParms->dwDemoDuration);
    }
    bRun = EC_TRUE;
    {
        CEcTimer oPerfMeasPrintTimer;

        if (pAppParms->bPerfMeasShowCyclic)
        {
            oPerfMeasPrintTimer.Start(2000);
        }

        while (bRun)
        {
            if (oPerfMeasPrintTimer.IsElapsed())
            {
                PRINT_PERF_MEAS();
                oPerfMeasPrintTimer.Restart();
            }

            /* check if demo shall terminate */
            bRun = !(OsTerminateAppRequest() || oAppDuration.IsElapsed());

            myAppDiagnosis(pAppContext);

            if (EC_NULL != pAppParms->pbyCnfData)
            {
                if ((eDcmMode_Off != pAppParms->eDcmMode) && (eDcmMode_LinkLayerRefClock != pAppParms->eDcmMode))
                {
                    EC_T_DWORD dwStatus = 0;
                    EC_T_BOOL  bWriteDiffLog = EC_FALSE;
                    EC_T_INT   nDiffCur = 0, nDiffAvg = 0, nDiffMax = 0;

                    if (!oDcmStatusTimer.IsStarted() || oDcmStatusTimer.IsElapsed())
                    {
                        bWriteDiffLog = EC_TRUE;
                        oDcmStatusTimer.Start(5000);
                    }

                    dwRes = ecatDcmGetStatus(&dwStatus, &nDiffCur, &nDiffAvg, &nDiffMax);
                    if (dwRes == EC_E_NOERROR)
                    {
                        if (bFirstDcmStatus)
                        {
                            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCM during startup (<INIT> to <%s>)\n", ecatStateToStr(ecatGetMasterState())));
                        }
                        if ((dwStatus != EC_E_NOTREADY) && (dwStatus != EC_E_BUSY) && (dwStatus != EC_E_NOERROR))
                        {
                            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCM Status: %s (0x%08X)\n", ecatGetText(dwStatus), dwStatus));
                        }
                        if (bWriteDiffLog && pAppParms->bDcmLogEnabled)
                        {
                            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCM Diff (cur/avg/max) [nsec]: %7d/ %7d/ %7d\n", nDiffCur, nDiffAvg, nDiffMax));
                        }
                    }
                    else
                    {
                        if ((eEcatState_OP == ecatGetMasterState()) || (eEcatState_SAFEOP == ecatGetMasterState()))
                        {
                            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot get DCM status! %s (0x%08X)\n", ecatGetText(dwRes), dwRes));
                        }
                    }
#if (defined INCLUDE_DCX)
                    if (eDcmMode_Dcx == pAppParms->eDcmMode && EC_E_NOERROR == dwRes)
                    {
                        EC_T_INT64 nTimeStampDiff = 0;
                        dwRes = ecatDcxGetStatus(&dwStatus, &nDiffCur, &nDiffAvg, &nDiffMax, &nTimeStampDiff);
                        if (EC_E_NOERROR == dwRes)
                        {
                            if (bFirstDcmStatus)
                            {
                                EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCX during startup (<INIT> to <%s>)\n", ecatStateToStr(ecatGetMasterState())));
                            }
                            if ((dwStatus != EC_E_NOTREADY) && (dwStatus != EC_E_BUSY) && (dwStatus != EC_E_NOERROR))
                            {
                                EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCX Status: %s (0x%08X)\n", ecatGetText(dwStatus), dwStatus));
                            }
                            if (bWriteDiffLog && pAppParms->bDcmLogEnabled)
                            {
                                EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "DCX Diff(ns): Cur=%7d, Avg=%7d, Max=%7d, TimeStamp=%7d\n", nDiffCur, nDiffAvg, nDiffMax, nTimeStampDiff));
                            }
                        }
                        else
                        {
                            if ((eEcatState_OP == ecatGetMasterState()) || (eEcatState_SAFEOP == ecatGetMasterState()))
                            {
                                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot get DCX status! %s (0x%08X)\n", ecatGetText(dwRes), dwRes));
                            }
                        }
                    }
#endif
                    if (bFirstDcmStatus && (EC_E_NOERROR == dwRes))
                    {
                        bFirstDcmStatus = EC_FALSE;
#if EC_VERSION_SINCE(2,4,1,6)
                        ecatDcmResetStatus();
#endif
                    }
                }
            }

            /* Reset pending drive errors */
            ResetDriveErrors(pAppContext);

            /* process notification jobs */
            pAppContext->pNotificationHandler->ProcessNotificationJobs();

            OsSleep(5);
        }
    }

    if (pAppParms->dwAppLogLevel != EC_LOG_LEVEL_SILENT)
    {
        EC_T_DWORD dwCurrentUsage = 0;
        EC_T_DWORD dwMaxUsage = 0;
        dwRes = ecatGetMemoryUsage(&dwCurrentUsage, &dwMaxUsage);
        if (EC_E_NOERROR != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot read memory usage of master: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Memory Usage Master     (cur/max) [bytes]: %u/%u\n", dwCurrentUsage, dwMaxUsage));

#if (defined INCLUDE_RAS_SERVER)
        if (EC_NULL != pvRasServerHandle)
        {
            dwRes = emRasGetMemoryUsage(pvRasServerHandle, &dwCurrentUsage, &dwMaxUsage);
            if (EC_E_NOERROR != dwRes)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot read memory usage of RAS: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
                goto Exit;
            }
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Memory Usage RAS Server (cur/max) [bytes]: %u/%u\n", dwCurrentUsage, dwMaxUsage));
        }
#endif
    }

Exit:
    /* set master state to INIT */
    if (eEcatState_UNKNOWN != ecatGetMasterState())
    {
        if (pAppParms->dwPerfMeasLevel > 0)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "\nJob times before shutdown\n"));
            PRINT_PERF_MEAS();
        }
        if (pAppParms->dwPerfMeasLevel > 1)
        {
            PRINT_HISTOGRAM();
        }

        dwRes = ecatSetMasterState(ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_INIT);
        pAppContext->pNotificationHandler->ProcessNotificationJobs();
        if (EC_E_NOERROR != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot stop EtherCAT-Master: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        }
    }

#if (defined INCLUDE_PCAP_RECORDER)
    SafeDelete(pPcapRecorder);
#endif /* INCLUDE_PCAP_RECORDER */

    /* unregister client */
    if (EC_NULL != pAppContext->pNotificationHandler)
    {
        EC_T_DWORD dwClientId = pAppContext->pNotificationHandler->GetClientID();
        if (INVALID_CLIENT_ID != dwClientId)
        {
            dwRes = ecatUnregisterClient(dwClientId);
            if (EC_E_NOERROR != dwRes)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot unregister client: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            }
            pAppContext->pNotificationHandler->SetClientID(INVALID_CLIENT_ID);
        }
    }

#if (defined INCLUDE_RAS_SERVER)
    /* stop RAS server */
    if (EC_NULL != pvRasServerHandle)
    {
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Stop Remote Api Server\n"));
        dwRes = emRasSrvStop(pvRasServerHandle, 2000);
        if (EC_E_NOERROR != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Remote API Server shutdown failed\n"));
        }
    }
#endif

    /* shutdown JobTask */
    {
        CEcTimer oTimeout(2000);
        pAppContext->bJobTaskShutdown = EC_TRUE;
        while (pAppContext->bJobTaskRunning && !oTimeout.IsElapsed())
        {
            OsSleep(10);
        }
        if (EC_NULL != pvJobTaskHandle)
        {
            OsDeleteThreadHandle(pvJobTaskHandle);
            pvJobTaskHandle = EC_NULL;
        }
    }

#if (defined INCLUDE_EC_DAQ)
    /* stop recorder */
    dwRes = ecDaqRecStop(pAppContext->hDaq);
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqRecStop: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
    }

    /* delete DAQ recorder instance */
    dwRes = ecDaqRecDelete(pAppContext->hDaq);
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqRecDelete: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
    }
#endif

    /* deinitialize master */
    dwRes = ecatDeinitMaster();
    if (EC_E_NOERROR != dwRes)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Cannot de-initialize EtherCAT-Master: %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
    }

    SafeDelete(pAppContext->pNotificationHandler);

    SafeDelete(S_pPendMotionCmdFifo);

    if (EC_NULL != pAppContext->pMyAppDesc)
    {
#if (defined EC_SIMULATOR_DS402)
        SafeDelete(pAppContext->pMyAppDesc->pEcSimulatorDemoMotion);
#endif

        /* free MCFBs */
        for (EC_T_DWORD dwAxesIdx = 0; dwAxesIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxesIdx++)
        {
            EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[dwAxesIdx];
            SafeDelete(pDemoAxis->pFb);
        }
        SafeOsFree(pAppContext->pMyAppDesc);
    }

    /* close parameter file */
    if (pvCfgFileHandle != EC_NULL)
    {
        atCloseXmlFile(&pvCfgFileHandle);
    }

    return dwRetVal;
}

/********************************************************************************/
/** \brief  Trigger jobs to drive master, and update process data.
*
* \return N/A
*/
static EC_T_VOID EcMasterJobTask(EC_T_VOID* pvAppContext)
{
    EC_T_DWORD             dwRes = EC_E_ERROR;
    T_EC_DEMO_APP_CONTEXT* pAppContext       = (T_EC_DEMO_APP_CONTEXT*)pvAppContext;
    T_EC_DEMO_APP_PARMS*   pAppParms = &pAppContext->AppParms;
    EC_T_INT               nOverloadCounter  = 0; /* counter to check if cycle time is to short */
    EC_T_USER_JOB_PARMS    oJobParms;
    OsMemset(&oJobParms, 0, sizeof(EC_T_USER_JOB_PARMS));

    /* demo loop */
    pAppContext->bJobTaskRunning = EC_TRUE;
    do
    {
        /* wait for next cycle (event from scheduler task) */
        OsDbgAssert(pAppContext->pvJobTaskEvent != 0);
        dwRes = OsWaitForEvent(pAppContext->pvJobTaskEvent, 3000);
        if (EC_E_NOERROR != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: OsWaitForEvent(): %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
            OsSleep(500);
        }

        /* start Task (required for enhanced performance measurement) */
        dwRes = ecatExecJob(eUsrJob_StartTask, EC_NULL);
        if (EC_E_NOERROR != dwRes && EC_E_INVALIDSTATE != dwRes && EC_E_LINK_DISCONNECTED != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: ecatExecJob(eUsrJob_StartTask): %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
        }

        /* process all received frames (read new input values) */
        dwRes = ecatExecJob(eUsrJob_ProcessAllRxFrames, &oJobParms);
        if (EC_E_NOERROR != dwRes && EC_E_INVALIDSTATE != dwRes && EC_E_LINK_DISCONNECTED != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: ecatExecJob(eUsrJob_ProcessAllRxFrames): %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
        }

        if (EC_E_NOERROR == dwRes)
        {
            if (!oJobParms.bAllCycFramesProcessed)
            {
                /* it is not reasonable, that more than 5 continuous frames are lost */
                nOverloadCounter += 10;
                if (nOverloadCounter >= 50)
                {
                    if ((pAppContext->dwPerfMeasLevel > 0) && (nOverloadCounter < 60))
                    {
                        PRINT_PERF_MEAS();
                    }
                    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Error: System overload: Cycle time too short or huge jitter!\n"));
                }
                else
                {
                    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "eUsrJob_ProcessAllRxFrames - not all previously sent frames are received/processed (frame loss)!\n"));
                }
            }
            else
            {
                /* everything o.k.? If yes, decrement overload counter */
                if (nOverloadCounter > 0)    nOverloadCounter--;
            }
        }

        /* DCM log */
#ifdef DCM_ENABLE_LOGFILE
        if (pAppParms->bDcmLogEnabled)
        {
            EC_T_CHAR* pszLog = EC_NULL;

            if (pAppContext->dwPerfMeasLevel > 0)
            {
                ecatPerfMeasAppStart(pAppContext->pvPerfMeas, PERF_DCM_Logfile);
            }
            ecatDcmGetLog(&pszLog);
            if ((EC_NULL != pszLog))
            {
                ((CAtEmLogging*)pEcLogContext)->LogDcm(pszLog);
            }
            if (pAppContext->dwPerfMeasLevel > 0)
            {
                ecatPerfMeasAppEnd(pAppContext->pvPerfMeas, PERF_DCM_Logfile);
            }
        }
#endif

        if (pAppContext->dwPerfMeasLevel > 0)
        {
            ecatPerfMeasAppStart(pAppContext->pvPerfMeas, PERF_myAppWorkpd);
        }
        {
            EC_T_STATE eMasterState = ecatGetMasterState();

            if ((eEcatState_SAFEOP == eMasterState) || (eEcatState_OP == eMasterState))
            {
                myAppWorkpd(pAppContext);
#if (defined INCLUDE_EC_DAQ)
                dwRes = ecDaqProcessRt(pAppContext->hDaq);
                if (dwRes != EC_E_NOERROR)
                {
                    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqProcessRt: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
                }
#endif
            }
        }
        if (pAppContext->dwPerfMeasLevel > 0)
        {
            ecatPerfMeasAppEnd(pAppContext->pvPerfMeas, PERF_myAppWorkpd);
        }

        /* write output values of current cycle, by sending all cyclic frames */
        dwRes = ecatExecJob(eUsrJob_SendAllCycFrames, &oJobParms);
        if (EC_E_NOERROR != dwRes && EC_E_INVALIDSTATE != dwRes && EC_E_LINK_DISCONNECTED != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecatExecJob( eUsrJob_SendAllCycFrames,    EC_NULL ): %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
        }

        /* remove this code when using licensed version */
        if (EC_E_EVAL_EXPIRED == dwRes)
        {
            bRun = EC_FALSE; /* set shutdown flag */
        }

        /* execute some administrative jobs. No bus traffic is performed by this function */
        dwRes = ecatExecJob(eUsrJob_MasterTimer, EC_NULL);
        if (EC_E_NOERROR != dwRes && EC_E_INVALIDSTATE != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecatExecJob(eUsrJob_MasterTimer, EC_NULL): %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
        }

        /* send queued acyclic EtherCAT frames */
        dwRes = ecatExecJob(eUsrJob_SendAcycFrames, EC_NULL);
        if (EC_E_NOERROR != dwRes && EC_E_INVALIDSTATE != dwRes && EC_E_LINK_DISCONNECTED != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecatExecJob(eUsrJob_SendAcycFrames, EC_NULL): %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
        }

        pAppContext->pMyAppDesc->dwJobTaskCycleCnt++;

        /* stop Task (required for enhanced performance measurement) */
        dwRes = ecatExecJob(eUsrJob_StopTask, EC_NULL);
        if (EC_E_NOERROR != dwRes && EC_E_INVALIDSTATE != dwRes && EC_E_LINK_DISCONNECTED != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: ecatExecJob(eUsrJob_StopTask): %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
        }
#if !(defined NO_OS)
    } while (!pAppContext->bJobTaskShutdown);

    pAppContext->bJobTaskRunning = EC_FALSE;
#else
    /* in case of NO_OS the job task function is called cyclically within the timer ISR */
    } while (EC_FALSE);
    pAppContext->bJobTaskRunning = !pAppContext->bJobTaskShutdown;
#endif

    return;
}

/********************************************************************************/
/** \brief  Handler for master notifications
*
* \return  Status value.
*/
static EC_T_DWORD EcMasterNotifyCallback(
    EC_T_DWORD         dwCode,  /**< [in]   Notification code */
    EC_T_NOTIFYPARMS*  pParms   /**< [in]   Notification parameters */
)
{
    EC_T_DWORD dwRetVal = EC_E_NOERROR;
    CEmNotification* pNotificationHandler = EC_NULL;

    if ((EC_NULL == pParms) || (EC_NULL == pParms->pCallerData))
    {
        dwRetVal = EC_E_INVALIDPARM;
        goto Exit;
    }

    pNotificationHandler = ((T_EC_DEMO_APP_CONTEXT*)pParms->pCallerData)->pNotificationHandler;

    if ((dwCode >= EC_NOTIFY_APP) && (dwCode <= EC_NOTIFY_APP + EC_NOTIFY_APP_MAX_CODE))
    {
        /* notification for application */
        dwRetVal = myAppNotify(dwCode - EC_NOTIFY_APP, pParms);
    }
    else
    {
        /* default notification handler */
        dwRetVal = pNotificationHandler->ecatNotify(dwCode, pParms);
    }

Exit:
    return dwRetVal;
}

/********************************************************************************/
/** \brief  RAS notification handler
 *
 * \return EC_E_NOERROR or error code
 */
#ifdef INCLUDE_RAS_SERVER
static EC_T_DWORD RasNotifyCallback(
    EC_T_DWORD         dwCode,
    EC_T_NOTIFYPARMS*  pParms
)
{
    EC_T_DWORD dwRetVal = EC_E_NOERROR;
    CEmNotification* pNotificationHandler = EC_NULL;

    if ((EC_NULL == pParms) || (EC_NULL == pParms->pCallerData))
    {
        dwRetVal = EC_E_INVALIDPARM;
        goto Exit;
    }

    pNotificationHandler = (CEmNotification*)pParms->pCallerData;
    dwRetVal = pNotificationHandler->emRasNotify(dwCode, pParms);

Exit:
    return dwRetVal;
}
#endif

/*-MYAPP---------------------------------------------------------------------*/

/***************************************************************************************************/
/**
\brief  Initialize Application

\return EC_E_NOERROR on success, error code otherwise.
*/
static EC_T_DWORD myAppInit(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_UNREFPARM(pAppContext);
    return EC_E_NOERROR;
}

static EC_T_VOID ParseIdx(EC_T_VOID* cfgFileHandle, EC_T_CHAR* paraString, EC_T_DWORD& pdoIdx, EC_T_DWORD& varIdx, EC_T_DWORD& varSubIdx)
{
    EC_T_BYTE buffy[AT_XMLBUF_SIZE];
    if (atParamRead(cfgFileHandle, eDataType_STRING, paraString, buffy, AT_XMLBUF_SIZE) != EC_E_NOERROR) return;
    buffy[AT_XMLBUF_SIZE - 1] = '\0';

    char* idx1Str = OsStrtok(buffy, ":");
    char* idx2Str = OsStrtok(EC_NULL, ":");
    char* idx3Str = OsStrtok(EC_NULL, ":");

    EC_T_DWORD idx1 = 0, idx2 = 0, idx3 = 0;
    if (idx1Str != EC_NULL) idx1 = (EC_T_DWORD)OsStrtoul(idx1Str, EC_NULL, 0);
    if (idx2Str != EC_NULL) idx2 = (EC_T_DWORD)OsStrtoul(idx2Str, EC_NULL, 0);
    if (idx3Str != EC_NULL) idx3 = (EC_T_DWORD)OsStrtoul(idx3Str, EC_NULL, 0);

    varIdx = idx1;
    varSubIdx = idx2;
    pdoIdx = idx3;
}

static EC_T_DWORD myAppConfigure(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_T_DWORD             dwRes = EC_E_NOERROR;
    EC_T_DWORD             dwValue = 0;
    EC_T_CHAR              szParaString[160];

#if (defined EC_SIMULATOR_DS402)
    DS402_T_INIT_PARMS oDS402InitParms;
    OsMemset(&oDS402InitParms, 0, sizeof(DS402_T_INIT_PARMS));
    EC_T_WORD awSlaveAddr[DEMO_MAX_NUM_OF_AXIS];
    OsMemset(awSlaveAddr, 0, DEMO_MAX_NUM_OF_AXIS * sizeof(EC_T_WORD));
#else
    EC_UNREFPARM(pAppContext);
#endif

    pAppContext->pMyAppDesc->dwAxesCnt = 0;
    for (EC_T_DWORD dwAxesIdx = 0; dwAxesIdx < DEMO_MAX_NUM_OF_AXIS; dwAxesIdx++)
    {
        /* read fixed address */
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_ADDR, dwAxesIdx + 1);
        if (atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&dwValue, sizeof(dwValue)) != EC_E_NOERROR
            || !dwValue)
        {
            continue;            /* axis not available */
        }

        EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[pAppContext->pMyAppDesc->dwAxesCnt];
        pAppContext->pMyAppDesc->dwAxesCnt++;
        pDemoAxis->wDriveFixAddress = (EC_T_WORD)dwValue;

#if (defined EC_SIMULATOR_DS402)
        /* store a deduplicated list of station fixed adresses in awSlaveAddr */
        for (EC_T_DWORD dwAxesIdx2 = 0; dwAxesIdx2 < pAppContext->pMyAppDesc->dwAxesCnt; dwAxesIdx2++)
        {
            /* station fixed adress does not exist yet */
            if (0 == awSlaveAddr[dwAxesIdx2])
            {
                oDS402InitParms.dwNumSlaves++;
                awSlaveAddr[dwAxesIdx2] = pDemoAxis->wDriveFixAddress;
                break;
            }

            /* station fixed adress already exists */
            if (pDemoAxis->wDriveFixAddress == awSlaveAddr[dwAxesIdx2])
            {
                break;
            }
        }
#endif
    }

#if (defined EC_SIMULATOR_DS402)
    oDS402InitParms.dwInstanceId = 0x1;
    oDS402InitParms.pSlaveAddr = awSlaveAddr;

    dwRes = pAppContext->pMyAppDesc->pEcSimulatorDemoMotion->InitInstance(G_pEcLogParms, &oDS402InitParms);
    if (EC_E_NOERROR != dwRes)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, (EC_T_CHAR*)"EcSimulatorDemoMotion InitInstance failed: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        goto Exit;
    }

Exit:
    return dwRes;
#else
    return dwRes;
#endif
}

/***************************************************************************************************/
/**
\brief  Initialize Slave Instance.

Find slave parameters.
\return EC_E_NOERROR on success, error code otherwise.
*/
static EC_T_DWORD myAppPrepare(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_T_DWORD          dwRes = EC_E_NOERROR;
    EC_T_DWORD          dwValue = 0;
    EC_T_CHAR           szParaString[160];

    for (EC_T_DWORD dwAxesIdx = 0; dwAxesIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxesIdx++)
    {
        EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[dwAxesIdx];

        /* Drive profile */
        char szBuffy[AT_XMLBUF_SIZE];
        OsMemset(szBuffy, 0, AT_XMLBUF_SIZE);
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_PROFILE, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_STRING, szParaString, (EC_T_BYTE*)szBuffy, sizeof(szBuffy));
        szBuffy[AT_XMLBUF_SIZE - 1] = '\0';

        pDemoAxis->eDriveProfile = MC_T_AXIS_PROFILE_DS402; // Default
        if (OsStrncmp(szBuffy, "DS402", AT_XMLBUF_SIZE) == 0) pDemoAxis->eDriveProfile = MC_T_AXIS_PROFILE_DS402;
        else if (OsStrncmp(szBuffy, "SERCOS", AT_XMLBUF_SIZE) == 0) pDemoAxis->eDriveProfile = MC_T_AXIS_PROFILE_SERCOS;

        /* Modes of operation */
        dwValue = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? (EC_T_DWORD)DRV_MODE_OP_CSP : (EC_T_DWORD)SER_OPMODE_POS_FB1;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_MODE, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&dwValue, sizeof(dwValue));
        pDemoAxis->byDriveModesOfOperation = (EC_T_BYTE)dwValue;

        /* Addressed drive number within SERCOS servo controller */
        dwValue = 0;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_NO, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&dwValue, sizeof(dwValue));
        pDemoAxis->bySercosDriveNo = (EC_T_BYTE)dwValue;

        /* CoE/DS402 operation mode object idx */
        pDemoAxis->dwCoeIdxOpMode = DRV_OBJ_MODES_OF_OPERATION;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_OPMODE, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxStatusword.dwPdoIdx, pDemoAxis->dwCoeIdxOpMode, pDemoAxis->dwCoeSubIdxOpMode);

        /* get values for move: jerk, acc, dec, vel and distance */
        pDemoAxis->dwDriveJerk = DEFAULT_JERK;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_JERK, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwDriveJerk, sizeof(pDemoAxis->dwDriveJerk));

        pDemoAxis->dwDriveAcc = DEFAULT_ACC;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_ACC, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwDriveAcc, sizeof(pDemoAxis->dwDriveAcc));

        pDemoAxis->dwDriveDec = DEFAULT_DEC;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_DEC, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwDriveDec, sizeof(pDemoAxis->dwDriveDec));

        pDemoAxis->dwDriveVel = DEFAULT_MAX_VELOCITY;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_VEL, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwDriveVel, sizeof(pDemoAxis->dwDriveVel));

        pDemoAxis->dwDriveDistance = DEFAULT_DISTANCE;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_DIST, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwDriveDistance, sizeof(pDemoAxis->dwDriveDistance));

        pDemoAxis->dwPosWindow = DEFAULT_POS_WINDOW;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_POS_WIND, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwPosWindow, sizeof(pDemoAxis->dwPosWindow));

        pDemoAxis->dwDriveIncPerMM = DEFAULT_INCPERMM;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_INCPERMM, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwDriveIncPerMM, sizeof(pDemoAxis->dwDriveIncPerMM));

        pDemoAxis->dwDriveIncFactor = 0; // x << 0 -> Not scaled
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_INCFACTOR, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwDriveIncFactor, sizeof(pDemoAxis->dwDriveIncFactor));

        pDemoAxis->dwVelocityGain = DEFAULT_VEL_GAIN;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_VEL_GAIN, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwVelocityGain, sizeof(pDemoAxis->dwVelocityGain));

        pDemoAxis->dwTorqueGain = DEFAULT_TORQUE_GAIN;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_TOR_GAIN, dwAxesIdx + 1);
        atParamRead(pAppContext->pMyAppDesc->pvCfgFileHandle, eDataType_DWORD, szParaString, (EC_T_BYTE*)&pDemoAxis->dwTorqueGain, sizeof(pDemoAxis->dwTorqueGain));

        pDemoAxis->IdxStatusword.dwVarIdx = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? DRV_OBJ_STATUS_WORD : DRV_IDN_STATUS_WORD;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_STATUS, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxStatusword.dwPdoIdx, pDemoAxis->IdxStatusword.dwVarIdx, pDemoAxis->IdxStatusword.dwVarSubIdx);

        pDemoAxis->IdxControlword.dwVarIdx = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? DRV_OBJ_CONTROL_WORD : DRV_IDN_CONTROL_WORD;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_CONTROL, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxControlword.dwPdoIdx, pDemoAxis->IdxControlword.dwVarIdx, pDemoAxis->IdxControlword.dwVarSubIdx);

        pDemoAxis->IdxActualPos.dwVarIdx = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? DRV_OBJ_POSITION_ACTUAL_VALUE : DRV_IDN_POSITION_ACTUAL_VALUE;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_POSACT, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxActualPos.dwPdoIdx, pDemoAxis->IdxActualPos.dwVarIdx, pDemoAxis->IdxActualPos.dwVarSubIdx);

        pDemoAxis->IdxModeOfOperation.dwVarIdx = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? DRV_OBJ_MODES_OF_OPERATION : DRV_IDN_OPMODE;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_MODE, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxModeOfOperation.dwPdoIdx, pDemoAxis->IdxModeOfOperation.dwVarIdx, pDemoAxis->IdxModeOfOperation.dwVarSubIdx);

        pDemoAxis->IdxModeOfOperationDisplay.dwVarIdx = DRV_OBJ_MODES_OF_OPERATION_DISPLAY;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_MODE_DISPLAY, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxModeOfOperationDisplay.dwPdoIdx, pDemoAxis->IdxModeOfOperationDisplay.dwVarIdx, pDemoAxis->IdxModeOfOperationDisplay.dwVarSubIdx);

        /* CSP */
        pDemoAxis->IdxTargetPos.dwVarIdx = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? DRV_OBJ_TARGET_POSITION : DRV_IDN_TARGET_POSITION;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_TARPOS, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxTargetPos.dwPdoIdx, pDemoAxis->IdxTargetPos.dwVarIdx, pDemoAxis->IdxTargetPos.dwVarSubIdx);

        /* Velocity Offset Feed Forward (only DS402) */
        pDemoAxis->IdxVelOffset.dwVarIdx = DRV_OBJ_VELOCITY_OFFSET;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_VELOFF, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxVelOffset.dwPdoIdx, pDemoAxis->IdxVelOffset.dwVarIdx, pDemoAxis->IdxVelOffset.dwVarSubIdx);

        /* Torque Offset Feed Forward (only DS402) */
        pDemoAxis->IdxTorOffset.dwVarIdx = DRV_OBJ_TORQUE_OFFSET;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_TOROFF, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxTorOffset.dwPdoIdx, pDemoAxis->IdxTorOffset.dwVarIdx, pDemoAxis->IdxTorOffset.dwVarSubIdx);

        /* CSV */
        pDemoAxis->IdxTargetVel.dwVarIdx = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? DRV_OBJ_TARGET_VELOCITY : DRV_IDN_TARGET_VELOCITY;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_TARVEL, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxTargetVel.dwPdoIdx, pDemoAxis->IdxTargetVel.dwVarIdx, pDemoAxis->IdxTargetVel.dwVarSubIdx);

        /* CST */
        pDemoAxis->IdxTargetTrq.dwVarIdx = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? DRV_OBJ_TARGET_TORQUE : DRV_IDN_TARGET_TORQUE;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_TARTRQ, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxTargetTrq.dwPdoIdx, pDemoAxis->IdxTargetTrq.dwVarIdx, pDemoAxis->IdxTargetTrq.dwVarSubIdx);

        pDemoAxis->IdxActualTrq.dwVarIdx = (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402) ? DRV_OBJ_ACTUAL_TORQUE : DRV_IDN_ACTUAL_TORQUE;
        EcSnprintf(szParaString, sizeof(szParaString), MOTIONDEMO_CFG_TAG_DRIVE_IDX_ACTTRQ, dwAxesIdx + 1);
        ParseIdx(pAppContext->pMyAppDesc->pvCfgFileHandle, szParaString, pDemoAxis->IdxActualTrq.dwPdoIdx, pDemoAxis->IdxActualTrq.dwVarIdx, pDemoAxis->IdxActualTrq.dwVarSubIdx);

        /*
            * Initialize EC-Motion API
            */

            /* create Motion Control Function Blocks */
        pDemoAxis->pFb = new EC_T_FUNCTION_BLOCKS(); // Allocate on heap, because the demo struct is memset to zero!

        MC_T_AXIS_INIT& axisInitData = pDemoAxis->pFb->AxisInitData;
        axisInitData.AxisType = MC_AXIS_TYPE_REAL_ALL;
        // axisInitData.eAxisType = MC_AXIS_TYPE_VIRTUAL;
        axisInitData.CycleTime = pAppContext->AppParms.dwBusCycleTimeUsec;
        axisInitData.IncPerMM = pDemoAxis->dwDriveIncPerMM;
        axisInitData.IncFactor = pDemoAxis->dwDriveIncFactor;
        axisInitData.LogParms.dwLogLevel = dwEcLogLevel;
        axisInitData.LogParms.pfLogMsg = pLogMsgCallback;
        axisInitData.LogParms.pLogContext = pEcLogContext;
        axisInitData.VelocityGain = pDemoAxis->dwVelocityGain;
        axisInitData.TorqueGain = pDemoAxis->dwTorqueGain;

        // Initialize axis
        pDemoAxis->pFb->Axis.Init(axisInitData);

        // Set mandatory IN/OUT variable "Axis" (reference to the axis).
        MC_T_AXIS_REF* pAxis = &pDemoAxis->pFb->Axis;
        pDemoAxis->pFb->Power.Axis = pAxis;
        pDemoAxis->pFb->Stop.Axis = pAxis;
        pDemoAxis->pFb->Halt.Axis = pAxis;
        pDemoAxis->pFb->Reset.Axis = pAxis;
        pDemoAxis->pFb->MoveRelative.Axis = pAxis;
        pDemoAxis->pFb->MoveAbsolute.Axis = pAxis;
        pDemoAxis->pFb->MoveVelocity.Axis = pAxis;
        pDemoAxis->pFb->ReadActualPosition.Axis = pAxis;
        pDemoAxis->pFb->ReadParameter.Axis = pAxis;
        pDemoAxis->pFb->ReadBoolParameter.Axis = pAxis;
        pDemoAxis->pFb->WriteParameter.Axis = pAxis;
        pDemoAxis->pFb->WriteBoolParameter.Axis = pAxis;
        pDemoAxis->pFb->ReadMotionState.Axis = pAxis;
        pDemoAxis->pFb->ReadAxisInfo.Axis = pAxis;
        pDemoAxis->pFb->CheckTargetposReached.Axis = pAxis;
        pDemoAxis->pFb->HaltRecovery.Axis = pAxis;
#ifdef EC_MOTION_SUPPORT_PP_MODE
        pDemoAxis->pFb->Home.Axis = pAxis;
#endif
        pDemoAxis->pFb->ReadDigitalInput.Axis = pAxis;
        pDemoAxis->pFb->ReadDigitalOutput.Axis = pAxis;
        pDemoAxis->pFb->WriteDigitalOutput.Axis = pAxis;
#ifdef EC_MOTION_SUPPORT_MC_CAMMING
        pDemoAxis->pFb->CamTableSelect.Axis = pAxis;
        pDemoAxis->pFb->CamIn.Axis = pAxis;
        pDemoAxis->pFb->CamOut.Axis = pAxis;
#endif
#if defined(EC_MOTION_SUPPORT_MC_GEARING)
        pDemoAxis->pFb->GearIn.Axis = pAxis;
        pDemoAxis->pFb->GearOut.Axis = pAxis;
#endif

#if (defined INCLUDE_EC_DAQ)
        /* record all variables of a specific slave */
        dwRes = ecDaqConfigAddDataSlave(pAppContext->hDaq, pDemoAxis->wDriveFixAddress);
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqConfigAddDataSlave: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
        /* Record some parameters of function block MoveRelative */
        dwRes = ecDaqConfigRegisterAppVariable(pAppContext->hDaq, "MoveRelative.Distance", DEFTYPE_REAL64, 0,
            64, &(pDemoAxis->pFb->MoveRelative.Distance));
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqConfigAddApplicationVariable: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
        ecDaqConfigAddDataVariable(pAppContext->hDaq, "MoveRelative.Distance");

        dwRes = ecDaqConfigRegisterAppVariable(pAppContext->hDaq, "MoveRelative.Execute", DEFTYPE_BOOLEAN, 0,
            1, &(pDemoAxis->pFb->MoveRelative.Execute));
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqConfigAddApplicationVariable: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
        ecDaqConfigAddDataVariable(pAppContext->hDaq, "MoveRelative.Execute");

        dwRes = ecDaqConfigRegisterAppVariable(pAppContext->hDaq, "MoveRelative.Active", DEFTYPE_BOOLEAN, 0,
            sizeof(pDemoAxis->pFb->MoveRelative.Active) * 8, (EC_T_VOID*)&(pDemoAxis->pFb->MoveRelative.Active));
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqConfigAddApplicationVariable: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
        ecDaqConfigAddDataVariable(pAppContext->hDaq, "MoveRelative.Active");

        dwRes = ecDaqConfigRegisterAppVariable(pAppContext->hDaq, "MoveRelative.Done", DEFTYPE_BOOLEAN, 0,
            sizeof(pDemoAxis->pFb->MoveRelative.Done), (EC_T_VOID*)&(pDemoAxis->pFb->MoveRelative.Done));
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ecDaqConfigAddApplicationVariable: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
        ecDaqConfigAddDataVariable(pAppContext->hDaq, "MoveRelative.Done");
#endif
    }

#if (defined INCLUDE_EC_DAQ)
Exit:
#endif
    return dwRes;
}

/***************************************************************************************************/
/**
\brief  Setup slave parameters (normally done in PREOP state)

  - SDO up- and Downloads
  - Read Object Dictionary

\return EC_E_NOERROR on success, error code otherwise.
*/
static EC_T_DWORD myAppSetup(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_T_DWORD          dwRetVal = EC_E_NOERROR;
    EC_T_DWORD          dwRes    = EC_E_NOERROR;
    EC_T_CFG_SLAVE_INFO oSlaveInfo;

    EC_T_WORD           wNumOfVarsToReadInp          = 0;
    EC_T_WORD           wNumOfVarsToReadOutp         = 0;
    EC_T_WORD           wReadEntries                 = 0;
    EC_T_DWORD          dwProcessVarIdx              = 0;
    EC_T_DWORD          dwAxisIdx = 0;

    EC_T_PROCESS_VAR_INFO_EX* pProcessVarInfo = EC_NULL;
    EC_T_BYTE* pbyPDOut = ecatGetProcessImageOutputPtr();
    EC_T_BYTE* pbyPDIn  = ecatGetProcessImageInputPtr();

    /* read CoE object dictionary from device */
    if (pAppContext->AppParms.bReadOD)
    {
        EC_T_BOOL bStopReading = EC_FALSE;
        dwRes = CoeReadObjectDictionary(pAppContext, &bStopReading, emGetSlaveId(pAppContext->dwInstanceId, pAppContext->AppParms.wReadODSlaveAddr), EC_TRUE, MBX_TIMEOUT);
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: myAppSetup: CoeReadObjectDictionary %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
            dwRetVal = dwRes;
            goto Exit;
        }
    }

    for (dwAxisIdx = 0; dwAxisIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxisIdx++)
    {
        EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[dwAxisIdx];

        /* get slave */
        dwRetVal = ecatGetCfgSlaveInfo(EC_TRUE, pDemoAxis->wDriveFixAddress, &oSlaveInfo);
        if (EC_E_NOERROR != dwRetVal)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: emGetCfgSlaveInfo() (Result = %s 0x%x)", ecatGetText(dwRetVal), dwRetVal));
            goto Exit;
        }

        pDemoAxis->dwSlaveId = oSlaveInfo.dwSlaveId;

        /* Get the number of INPUT process variable information entries of the current slave */
        dwRetVal = ecatGetSlaveInpVarInfoNumOf(EC_TRUE, pDemoAxis->wDriveFixAddress, &wNumOfVarsToReadInp);
        if (wNumOfVarsToReadInp > 0)
        {
            /* Allocate  memory for the INPUT process variable information entries of the current slave */
            pProcessVarInfo = (EC_T_PROCESS_VAR_INFO_EX*)OsMalloc(sizeof(EC_T_PROCESS_VAR_INFO_EX) * wNumOfVarsToReadInp);
            if (pProcessVarInfo == EC_NULL)
            {
                dwRetVal = EC_E_NOMEMORY;
            }
            else
            {
                /* Get all INPUT process variable information entries of the current slave */
                dwRetVal = ecatGetSlaveInpVarInfoEx(EC_TRUE, pDemoAxis->wDriveFixAddress, wNumOfVarsToReadInp, pProcessVarInfo, &wReadEntries);
                if (EC_E_NOERROR != dwRetVal)
                {
                    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: emGetSlaveInpVarInfoEx() (Result = %s 0x%x)", ecatGetText(dwRetVal), dwRetVal));
                }

                /* search input variables */
                for (dwProcessVarIdx = 0; (dwProcessVarIdx < wReadEntries) && (dwRetVal == EC_E_NOERROR); dwProcessVarIdx++)
                {
                    if ((pDemoAxis->IdxStatusword.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxStatusword.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxStatusword.dwVarIdx
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxStatusword.dwVarSubIdx)
                    {
                        pDemoAxis->pwPdStatusWord = (EC_T_WORD*)&(pbyPDIn[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                    else if ((pDemoAxis->IdxActualPos.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxActualPos.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxActualPos.dwVarIdx
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxActualPos.dwVarSubIdx)
                    {
                        pDemoAxis->plPdActualPosition = (EC_T_SDWORD*)&(pbyPDIn[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                    else if ((pDemoAxis->IdxActualTrq.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxActualTrq.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxActualTrq.dwVarIdx
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxActualTrq.dwVarSubIdx)
                    {
                        pDemoAxis->psPdActualTorque = (EC_T_SWORD*)&(pbyPDIn[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                    else if ((pDemoAxis->IdxModeOfOperationDisplay.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxModeOfOperationDisplay.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxModeOfOperationDisplay.dwVarIdx
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxModeOfOperationDisplay.dwVarSubIdx)
                    {
                        pDemoAxis->pbyPdModeOfOperationDisplay = (EC_T_BYTE*)&(pbyPDIn[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                    else if (pProcessVarInfo[dwProcessVarIdx].wIndex == DRV_OBJ_DIGITAL_INPUTS)
                    {
                        pDemoAxis->pdwDigInputs = (EC_T_DWORD*)&(pbyPDIn[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                }

                OsFree(pProcessVarInfo);
                pProcessVarInfo = EC_NULL;
            }
        }

        if (pDemoAxis->pwPdStatusWord == EC_NULL)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Status Word Object=0x%x not found", pDemoAxis->wDriveFixAddress, pDemoAxis->IdxStatusword.dwVarIdx));
            dwRetVal = EC_E_INVALIDPARM;
        }

        if (pDemoAxis->plPdActualPosition == EC_NULL)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Actual Position Object=0x%x not found", pDemoAxis->wDriveFixAddress, pDemoAxis->IdxActualPos.dwVarIdx));
            dwRetVal = EC_E_INVALIDPARM;
        }

#ifdef EC_MOTION_SUPPORT_PP_MODE
        // Only in PP mode, will be used in homing
        if (pDemoAxis->byDriveModesOfOperation == DRV_MODE_OP_PROF_POS)
        {
            if (pDemoAxis->pbyPdModeOfOperationDisplay == EC_NULL)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Operation Mode Display Object=0x%x not found", pDemoAxis->wDriveFixAddress, pDemoAxis->IdxModeOfOperationDisplay.dwVarIdx));
                dwRetVal = EC_E_INVALIDPARM;
            }
        }
#endif
        /* Get the number of OUTPUT process variable information entries of the current slave */
        dwRetVal = ecatGetSlaveOutpVarInfoNumOf(EC_TRUE, pDemoAxis->wDriveFixAddress, &wNumOfVarsToReadOutp);
        if (wNumOfVarsToReadOutp > 0)
        {
            /* Allocate  memory for the OUTPUT process variable information entries of the current slave */
            pProcessVarInfo = (EC_T_PROCESS_VAR_INFO_EX*)OsMalloc(sizeof(EC_T_PROCESS_VAR_INFO_EX) * wNumOfVarsToReadOutp);
            if (pProcessVarInfo == EC_NULL)
            {
                dwRetVal = EC_E_NOMEMORY;
            }
            else
            {
                /* Get all OUTPUT process variable information entries of the current slave */
                dwRetVal = ecatGetSlaveOutpVarInfoEx(EC_TRUE, pDemoAxis->wDriveFixAddress, wNumOfVarsToReadOutp, pProcessVarInfo, &wReadEntries);
                if (EC_E_NOERROR != dwRetVal)
                {
                    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: emGetSlaveInpVarInfoEx() (Result = %s 0x%x)", ecatGetText(dwRetVal), dwRetVal));
                }

                /* search output variables */
                for (dwProcessVarIdx = 0; dwProcessVarIdx < wReadEntries; dwProcessVarIdx++)
                {
                    /* control word */
                    if ((pDemoAxis->IdxControlword.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxControlword.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxControlword.dwVarIdx    /* status word ? */
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxControlword.dwVarSubIdx)   /* status word ? */
                    {

                        pDemoAxis->pwPdControlWord = (EC_T_WORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }

                    /* target position */
                    if ((pDemoAxis->IdxTargetPos.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxTargetPos.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxTargetPos.dwVarIdx
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxTargetPos.dwVarSubIdx)
                    {
                        pDemoAxis->plPdTargetPosition = (EC_T_SDWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }

                    /* target velocity */
                    if ((pDemoAxis->IdxTargetVel.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxTargetVel.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxTargetVel.dwVarIdx
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxTargetVel.dwVarSubIdx)
                    {
                        pDemoAxis->plPdTargetVelocity = (EC_T_SDWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }

                    /* target torque */
                    if ((pDemoAxis->IdxTargetTrq.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxTargetTrq.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxTargetTrq.dwVarIdx
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxTargetTrq.dwVarSubIdx)
                    {
                        pDemoAxis->psPdTargetTorque = (EC_T_SWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }

                    /* velocity offset for feed forward */
                    if ((pDemoAxis->IdxVelOffset.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxVelOffset.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxVelOffset.dwVarIdx)
                    {
                        pDemoAxis->plPdVelocityOffset = (EC_T_SDWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }

                    /* torque offset for feed forward */
                    if ((pDemoAxis->IdxTorOffset.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxTorOffset.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxTorOffset.dwVarIdx)
                    {
                        pDemoAxis->psPdTorqueOffset = (EC_T_SWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                    /* mode of operation */
                    if ((pDemoAxis->IdxModeOfOperation.dwPdoIdx == 0 || pProcessVarInfo[dwProcessVarIdx].wPdoIndex == pDemoAxis->IdxModeOfOperation.dwPdoIdx)
                        && pProcessVarInfo[dwProcessVarIdx].wIndex == pDemoAxis->IdxModeOfOperation.dwVarIdx
                        && pProcessVarInfo[dwProcessVarIdx].wSubIndex == pDemoAxis->IdxModeOfOperation.dwVarSubIdx)
                    {
                        pDemoAxis->pbyPdModeOfOperation = (EC_T_BYTE*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
#ifdef EC_MOTION_SUPPORT_PP_MODE
                    /* profile velocity */
                    if (pProcessVarInfo[dwProcessVarIdx].wIndex == DRV_OBJ_PROFILE_VELOCITY)
                    {
                        pDemoAxis->plPdProfileVelocity = (EC_T_DWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                    /* profile velocity */
                    if (pProcessVarInfo[dwProcessVarIdx].wIndex == DRV_OBJ_TARGET_VELOCITY)
                    {
                        pDemoAxis->plPdTargetVelocity = (EC_T_SDWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                    /* profile acc */
                    if (pProcessVarInfo[dwProcessVarIdx].wIndex == DRV_OBJ_PROFILE_ACC)
                    {
                        pDemoAxis->plPdProfileAcc = (EC_T_DWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                    /* profile dec */
                    if (pProcessVarInfo[dwProcessVarIdx].wIndex == DRV_OBJ_PROFILE_DEC)
                    {
                        pDemoAxis->plPdProfileDec = (EC_T_DWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
#endif
                    if (pProcessVarInfo[dwProcessVarIdx].wIndex == DRV_OBJ_DIGITAL_OUTPUTS)
                    {
                        pDemoAxis->pdwDigOutputs = (EC_T_DWORD*)&(pbyPDOut[pProcessVarInfo[dwProcessVarIdx].nBitOffs / 8]);
                    }
                }

                OsFree(pProcessVarInfo);
                pProcessVarInfo = EC_NULL;
            }
        }

#ifdef EC_MOTION_SUPPORT_PP_MODE
        // Only in PP mode
        if (pDemoAxis->byDriveModesOfOperation == DRV_MODE_OP_PROF_POS)
        {
            if (pDemoAxis->plPdProfileVelocity == EC_NULL)
            {
                pDemoAxis->plPdProfileVelocity = (EC_T_DWORD*)pDemoAxis->plPdTargetVelocity;
            }
            if (pDemoAxis->plPdTargetVelocity == EC_NULL)
            {
                pDemoAxis->plPdTargetVelocity = (EC_T_SDWORD*)pDemoAxis->plPdProfileVelocity;
            }
            if (pDemoAxis->plPdProfileVelocity == EC_NULL)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Profile Velocity Object=0x%x not found", pDemoAxis->wDriveFixAddress, DRV_OBJ_PROFILE_VELOCITY));
                dwRetVal = EC_E_INVALIDPARM;
            }
            if (pDemoAxis->plPdProfileAcc == EC_NULL)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Profile Acceleration Object=0x%x not found", pDemoAxis->wDriveFixAddress, DRV_OBJ_PROFILE_ACC));
                dwRetVal = EC_E_INVALIDPARM;
            }
            if (pDemoAxis->plPdProfileDec == EC_NULL)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Profile Deceleration Object=0x%x not found", pDemoAxis->wDriveFixAddress, DRV_OBJ_PROFILE_DEC));
                dwRetVal = EC_E_INVALIDPARM;
            }
            if (pDemoAxis->pbyPdModeOfOperation == EC_NULL)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Operation Mode Object=0x%x not found", pDemoAxis->wDriveFixAddress, DRV_OBJ_MODES_OF_OPERATION));
                dwRetVal = EC_E_INVALIDPARM;
            }
        }
#endif
        if (pDemoAxis->pwPdControlWord == EC_NULL)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Control Word Object=0x%x not found", pDemoAxis->wDriveFixAddress, pDemoAxis->IdxControlword.dwVarIdx));
            dwRetVal = EC_E_INVALIDPARM;
        }

        EC_T_BYTE op = pDemoAxis->byDriveModesOfOperation;
        if (((pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402 && (op == DRV_MODE_OP_CSP || op == DRV_MODE_OP_INTER_POS))
            || (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_SERCOS && (op >= SER_OPMODE_POS_FB1 && op <= SER_OPMODE_POS_FB1FB2_LAGLESS)))
            && pDemoAxis->plPdTargetPosition == EC_NULL)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Target Position Object=0x%x not found", pDemoAxis->wDriveFixAddress, pDemoAxis->IdxTargetPos.dwVarIdx));
            dwRetVal = EC_E_INVALIDPARM;
        }

        if (((pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402 && op == DRV_MODE_OP_CSV)
            || (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_SERCOS && op == SER_OPMODE_VEL))
            && pDemoAxis->plPdTargetVelocity == EC_NULL)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Target Velocity Object=0x%x not found", pDemoAxis->wDriveFixAddress, pDemoAxis->IdxTargetVel.dwVarIdx));
            dwRetVal = EC_E_INVALIDPARM;
        }

        if (((pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_DS402 && op == DRV_MODE_OP_CST)
            || (pDemoAxis->eDriveProfile == MC_T_AXIS_PROFILE_SERCOS && op == SER_OPMODE_TORQUE))
            && pDemoAxis->psPdTargetTorque == EC_NULL)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid PDO mapping Axis %d: Target Torque Object=0x%x not found", pDemoAxis->wDriveFixAddress, pDemoAxis->IdxTargetTrq.dwVarIdx));
            dwRetVal = EC_E_INVALIDPARM;
        }

        if (dwRetVal == EC_E_NOERROR)
        {
            MC_T_AXIS_INIT_INPUTS inp;
            OsMemset(&inp, 0, sizeof(MC_T_AXIS_INIT_INPUTS));
            inp.pActualTorque = pDemoAxis->psPdActualTorque;
            inp.pActualPosition = pDemoAxis->plPdActualPosition;
            inp.pDigitalInputs = pDemoAxis->pdwDigInputs;

            pDemoAxis->pFb->Axis.InitInputs(inp);

            MC_T_AXIS_INIT_OUTPUTS outp;
            OsMemset(&outp, 0, sizeof(MC_T_AXIS_INIT_OUTPUTS));
            outp.pTargetTorque = pDemoAxis->psPdTargetTorque;
            outp.pTargetVelocity = pDemoAxis->plPdTargetVelocity;
            outp.pTargetPosition = pDemoAxis->plPdTargetPosition;
            outp.pVelocityOffset = pDemoAxis->plPdVelocityOffset;
            outp.pTorqueOffset = pDemoAxis->psPdTorqueOffset;
            outp.pModeOfOperation = pDemoAxis->pbyPdModeOfOperation;
            outp.pModeOfOperationDisplay = pDemoAxis->pbyPdModeOfOperationDisplay;
            outp.pDigitalOutputs = pDemoAxis->pdwDigOutputs;

#ifdef EC_MOTION_SUPPORT_PP_MODE
            outp.pProfileVelocity = pDemoAxis->plPdProfileVelocity;
            outp.pTargetVelocity = pDemoAxis->plPdTargetVelocity;
            outp.pProfileAcc = pDemoAxis->plPdProfileAcc;
            outp.pProfileDec = pDemoAxis->plPdProfileDec;
#endif
            pDemoAxis->pFb->Axis.InitOutputs(outp);

            if (pDemoAxis->pFb->AxisInitData.AxisType == MC_AXIS_TYPE_REAL_ALL)
            {
                /* store information about EtherCAT slave into AXIS ref */
                MC_T_AXIS_INIT_ECAT axisEcInitData;
                OsMemset(&axisEcInitData, 0, sizeof(MC_T_AXIS_INIT_ECAT));
                axisEcInitData.SlaveID = oSlaveInfo.dwSlaveId;
                axisEcInitData.StationAddress = oSlaveInfo.wStationAddress;
                axisEcInitData.Profile = pDemoAxis->eDriveProfile;
                axisEcInitData.SercosDriveNo = pDemoAxis->bySercosDriveNo;
                axisEcInitData.CoeIdxOpMode = (EC_T_WORD)pDemoAxis->dwCoeIdxOpMode;
                axisEcInitData.CoeSubIdxOpMode = (EC_T_WORD)pDemoAxis->dwCoeSubIdxOpMode;
                axisEcInitData.pEcatGetSlaveState = emGetSlaveState;
                axisEcInitData.pStatusWord = pDemoAxis->pwPdStatusWord;
                axisEcInitData.pControlWord = pDemoAxis->pwPdControlWord;

                switch (axisEcInitData.Profile)
                {
                case MC_T_AXIS_PROFILE_NONE: break;
                case MC_T_AXIS_PROFILE_DS402:
                    axisEcInitData.pEcatCoeSdoDownload = emCoeSdoDownload;
                    axisEcInitData.pEcatCoeSdoUpload = emCoeSdoUpload;
                    break;
                case MC_T_AXIS_PROFILE_SERCOS:
                    axisEcInitData.pEcatSoeWrite = emSoeWrite;
                    axisEcInitData.pEcatSoeRead = emSoeRead;
                    break;
                }

                pDemoAxis->pFb->Axis.InitEcat(axisEcInitData);
            }


            /* set modes of operation */
            dwRetVal = pDemoAxis->pFb->Axis.SetModeOfOperation(pDemoAxis->byDriveModesOfOperation);
            if (EC_E_NOERROR != dwRetVal)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Axis %d: Cannot set mode of operation for the drive (Result = %s 0x%x)!", pDemoAxis->wDriveFixAddress, ecatGetText(dwRetVal), dwRetVal));
            }
        }
    }

#ifdef EC_MOTION_SUPPORT_MC_CAMMING
    {
        S_myCamTable.eInterpolType = MC_CAM_INTERPOL_TYPE_CUB;
        S_myCamTable.nNumOfElements = sizeof(S_aCamPointsExampleInt) / 2 / sizeof(MC_T_INT);

        S_myCamTable.eVarType = MC_CAM_VAR_TYPE_INT;
        S_myCamTable.pData = S_aCamPointsExampleInt;
        /* S_myCamTable.eVarType = MC_CAM_VAR_TYPE_REAL;
        S_myCamTable.pData = S_aCamPointsExampleReal; */
    }
#endif

    /* Initialize motion state machines for all axes */
    for (dwAxisIdx = 0; dwAxisIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxisIdx++)
    {
        EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[dwAxisIdx];
        
        /* Initialize motion state machine variables */
        pDemoAxis->eMotionState = EMoveSetup;      /* Start in setup state */
        pDemoAxis->eDistanceState = 0;             /* Start with forward direction */
        pDemoAxis->nWaitCounter1 = 0;              /* Reset wait counter */
        
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, 
            "Axis %u: Initialized motion state machine (EMoveSetup)\n", 
            pDemoAxis->wDriveFixAddress));
    }

    /* no error */
    dwRetVal = EC_E_NOERROR;
Exit:
    return dwRetVal;
}

/*
 * Pop motion command from the cmdqueue and process them if any are pending.
 *
 * The cmdqueue is filled by the EC-Master notification mechanism. The
 * cmd's are send over RAS from the EC-SlaveTestApplication tool.
 */
static EC_T_DEMO_AXIS* MotionQueuePopCmd(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    MC_T_CMD_INP_PARA_V2 oMcInpCmd;
    OsMemset(&oMcInpCmd, 0, sizeof(MC_T_CMD_INP_PARA_V2));

    // get new command from EC-STA
    if (!S_pPendMotionCmdFifo->RemoveNoLock(oMcInpCmd)) return EC_NULL;

    EC_T_DEMO_AXIS* pDemoAxis = myAppSearchAxis(pAppContext, oMcInpCmd.dwStationAddress);
    if (pDemoAxis == EC_NULL) return EC_NULL;

    switch (oMcInpCmd.dwCmdCode)
    {
    case MC_CMD_CODE_POWER_ON:     /* Power-On */
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Power-On received\n"));

        pDemoAxis->dwDriveIncPerMM = oMcInpCmd.dwIncPerMM;

        pDemoAxis->pFb->AxisInitData.IncPerMM = pDemoAxis->dwDriveIncPerMM;
        pDemoAxis->pFb->AxisInitData.CycleTime = pAppContext->AppParms.dwBusCycleTimeUsec;
        pDemoAxis->pFb->Axis.Init(pDemoAxis->pFb->AxisInitData);

        pDemoAxis->pFb->Power.Enable = MC_TRUE;
        pDemoAxis->pFb->Power.EnablePositive = MC_TRUE;
        pDemoAxis->pFb->Power.EnableNegative = MC_TRUE;
        break;

    case MC_CMD_CODE_POWER_OFF:     /* Power-Off */
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Power-Off received\n"));
        pDemoAxis->pFb->Power.Enable = EC_FALSE;
        pDemoAxis->pFb->Power.EnablePositive = EC_FALSE;
        pDemoAxis->pFb->Power.EnableNegative = EC_FALSE;
        break;

    case MC_CMD_CODE_STOP:     /* Stop */
        if (!pDemoAxis->pFb->Stop.Execute)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Stop received\n"));
            pDemoAxis->pFb->Stop.Execute = EC_TRUE;
            pDemoAxis->pFb->Stop.Deceleration = oMcInpCmd.fDec;         /* mm/sec^2 */
            pDemoAxis->pFb->Stop.Jerk = oMcInpCmd.fJerk;        /* mm/sec^3   */
        }
        break;

    case MC_CMD_CODE_HALT:     /* Halt */
        if (!pDemoAxis->pFb->Halt.Execute)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Halt received\n"));
            pDemoAxis->pFb->Halt.Execute = EC_TRUE;
            pDemoAxis->pFb->Halt.Deceleration = oMcInpCmd.fDec;         /* mm/sec^2 */
            pDemoAxis->pFb->Halt.Jerk = oMcInpCmd.fJerk;        /* mm/sec^3   */
        }
        break;

    case MC_CMD_CODE_MOVE_RELATIVE:     /* Move Relative */
        if (!pDemoAxis->pFb->MoveRelative.Execute || pDemoAxis->pFb->MoveRelative.ContinuousUpdate)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Move Relative received and accepted\n"));
            pDemoAxis->pFb->MoveRelative.Distance = oMcInpCmd.fDistance;
            pDemoAxis->pFb->MoveRelative.Acceleration = oMcInpCmd.fAcc;         /* mm/sec^2 */
            pDemoAxis->pFb->MoveRelative.Deceleration = oMcInpCmd.fDec;         /* mm/sec^2 */
            pDemoAxis->pFb->MoveRelative.Velocity = oMcInpCmd.fVel;         /* mm/sec   */
            pDemoAxis->pFb->MoveRelative.Jerk = oMcInpCmd.fJerk;        /* mm/sec^3   */
            pDemoAxis->pFb->MoveRelative.ContinuousUpdate = oMcInpCmd.dwContUpdate;
            if (!pDemoAxis->pFb->MoveRelative.Execute)
                pDemoAxis->bTriggerMoveRel = EC_TRUE;
        }
        else
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Motion Command: Move Relative received, but not accepted due to ongoing move\n"));
        }
        break;

    case MC_CMD_CODE_MOVE_ABSOULTE:     /* Move Absolute */
        if (!pDemoAxis->pFb->MoveAbsolute.Execute || pDemoAxis->pFb->MoveAbsolute.ContinuousUpdate)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Move Absolute received and accepted\n"));
            pDemoAxis->pFb->MoveAbsolute.Position = oMcInpCmd.fDistance;
            pDemoAxis->pFb->MoveAbsolute.Acceleration = oMcInpCmd.fAcc;         /* mm/sec^2 */
            pDemoAxis->pFb->MoveAbsolute.Deceleration = oMcInpCmd.fDec;         /* mm/sec^2 */
            pDemoAxis->pFb->MoveAbsolute.Velocity = oMcInpCmd.fVel;         /* mm/sec   */
            pDemoAxis->pFb->MoveAbsolute.Jerk = oMcInpCmd.fJerk;        /* mm/sec^3   */
            pDemoAxis->pFb->MoveAbsolute.Direction = (MC_T_DIRECTION)oMcInpCmd.dwDirection;
            pDemoAxis->pFb->MoveAbsolute.ContinuousUpdate = oMcInpCmd.dwContUpdate;
            if (!pDemoAxis->pFb->MoveAbsolute.Execute)
                pDemoAxis->bTriggerMoveAbs = EC_TRUE;
        }
        else
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Motion Command: Move Absolute received, but not accepted due to ongoing move\n"));
        }
        break;

    case MC_CMD_CODE_MOVE_VELOCITY:    /* Move Velocity */
        if (!pDemoAxis->pFb->MoveVelocity.Execute || pDemoAxis->pFb->MoveVelocity.ContinuousUpdate)
        {
            pDemoAxis->pFb->MoveVelocity.Execute = EC_FALSE;
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Move Velocity received and accepted\n"));
            pDemoAxis->pFb->MoveVelocity.Acceleration = oMcInpCmd.fAcc;         /* mm/sec^2 */
            pDemoAxis->pFb->MoveVelocity.Deceleration = oMcInpCmd.fDec;         /* mm/sec^2 */
            pDemoAxis->pFb->MoveVelocity.Velocity = oMcInpCmd.fVel;         /* mm/sec   */
            pDemoAxis->pFb->MoveVelocity.Jerk = oMcInpCmd.fJerk;        /* mm/sec^3   */
            pDemoAxis->pFb->MoveVelocity.Direction = (MC_T_DIRECTION)oMcInpCmd.dwDirection;
            pDemoAxis->pFb->MoveVelocity.ContinuousUpdate = oMcInpCmd.dwContUpdate;
            pDemoAxis->bTriggerMoveVelo = EC_TRUE;
        }

        break;

    case MC_CMD_CODE_RESET:            /* Reset */
        if (!pDemoAxis->pFb->Reset.Execute)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Reset received and accepted\n"));
            pDemoAxis->pFb->Reset.Execute = EC_TRUE;
        }
        break;

    case MC_CMD_READ_PARAMETER:       /* Read Parameter */
        pDemoAxis->pFb->ReadParameter.Enable = EC_TRUE;
        pDemoAxis->pFb->ReadParameter.ParameterNumber = oMcInpCmd.dwParaNumber;
        break;

    case MC_CMD_READ_BOOL_PARAMETER:  /* Read Bool Parameter */
        pDemoAxis->pFb->ReadBoolParameter.Enable = EC_TRUE;
        pDemoAxis->pFb->ReadBoolParameter.ParameterNumber = oMcInpCmd.dwParaNumber;
        break;

    case MC_CMD_WRITE_PARAMETER:       /* Write Parameter */
        if (!pDemoAxis->pFb->WriteParameter.Execute)
        {
            pDemoAxis->pFb->WriteParameter.Execute = EC_TRUE;
            pDemoAxis->pFb->WriteParameter.ParameterNumber = oMcInpCmd.dwParaNumber;
            pDemoAxis->pFb->WriteParameter.Value = oMcInpCmd.fParaValue;
        }
        break;

    case MC_CMD_WRITE_BOOL_PARAMETER:   /* Write Bool Parameter */
        if (!pDemoAxis->pFb->WriteBoolParameter.Execute)
        {
            pDemoAxis->pFb->WriteBoolParameter.Execute = EC_TRUE;
            pDemoAxis->pFb->WriteBoolParameter.ParameterNumber = oMcInpCmd.dwParaNumber;
            pDemoAxis->pFb->WriteBoolParameter.Value = oMcInpCmd.bParaValue;
        }
        break;
#ifdef EC_MOTION_SUPPORT_PP_MODE
    case MC_CMD_HOME:   /* Home */
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: Home received\n"));
        if (!pDemoAxis->pFb->Home.Execute)
        {
            // Absolute home position, set after homing done
            pDemoAxis->pFb->Home.Position = HOMING_POSITION;
            // Start homing
            pDemoAxis->pFb->Home.Execute = EC_TRUE;
        }
        break;
#endif
#ifdef EC_MOTION_SUPPORT_MC_GEARING
    case MC_CMD_CODE_GEAR_IN:     /* GearIn */
    {
        EC_T_DEMO_AXIS* pMasterDemoAxis;

        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: GearIn received and accepted\n"));
        pMasterDemoAxis = myAppSearchAxis(pAppContext, oMcInpCmd.dwStationAddressMaster);
        if (pMasterDemoAxis == EC_NULL)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: GearIn: Error Master Axis not found\n"));
            break;
        }

        pDemoAxis->pFb->GearIn.MasterAxis = &pMasterDemoAxis->pFb->Axis;
        pDemoAxis->pFb->GearIn.RatioNumerator = oMcInpCmd.dwGearInRatioNumerator;
        pDemoAxis->pFb->GearIn.RatioDenominator = oMcInpCmd.dwGearInRatioDenoniator;

        if (!pDemoAxis->pFb->GearIn.Execute)
            pDemoAxis->bTriggerGearIn = EC_TRUE;
    }
    break;
    case MC_CMD_CODE_GEAR_OUT:     /* GearOut */
    {
        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: GearOut received and accepted\n"));

        pDemoAxis->pFb->GearOut.Deceleration = oMcInpCmd.fDec;
        pDemoAxis->pFb->GearOut.Jerk = oMcInpCmd.fJerk;        /* mm/sec^3   */

        if (!pDemoAxis->pFb->GearOut.Execute)
            pDemoAxis->bTriggerGearOut = EC_TRUE;
    }
    break;
#endif

#ifdef EC_MOTION_SUPPORT_MC_CAMMING
    case MC_CMD_CODE_CAM_TABLE_SELECT:
        if (!pDemoAxis->pFb->CamTableSelect.Execute)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: CAM Table Select received and accepted\n"));
            EC_T_DEMO_AXIS* pMasterDemoAxis;

            pMasterDemoAxis = myAppSearchAxis(pAppContext, oMcInpCmd.dwStationAddressMaster);
            if (pMasterDemoAxis == EC_NULL)
            {
                EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: CAM Table Select: Error Master Axis not found\n"));
                break;
            }

            pDemoAxis->pFb->CamTableSelect.MasterAxis = &pMasterDemoAxis->pFb->Axis;
            pDemoAxis->pFb->CamTableSelect.CamTable = &S_myCamTable;
            pDemoAxis->pFb->CamTableSelect.Periodic = oMcInpCmd.bCammingPeriodic;

            if (!pDemoAxis->pFb->CamTableSelect.Execute)
                pDemoAxis->bTriggerCamTableSelect = EC_TRUE;
        }
        else
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Motion Command: CAM Table Select received, but not accepted due to ongoing\n"));
        }
        break;
    case MC_CMD_CODE_CAM_IN:
        if (!pDemoAxis->pFb->CamIn.Execute)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: CAM In received and accepted\n"));
            EC_T_DEMO_AXIS* pMasterDemoAxis;

            pMasterDemoAxis = myAppSearchAxis(pAppContext, oMcInpCmd.dwStationAddressMaster);
            if (pMasterDemoAxis == EC_NULL)
            {
                EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: CAM In: Error Master Axis not found\n"));
                break;
            }

            pDemoAxis->pFb->CamIn.MasterAxis = &pMasterDemoAxis->pFb->Axis;
            pDemoAxis->pFb->CamIn.StartMode = (MC_T_START_MODE)oMcInpCmd.eCammingStartMode;
            pDemoAxis->pFb->CamIn.CamTableID = S_myCamTableID;

            if (!pDemoAxis->pFb->CamIn.Execute)
                pDemoAxis->bTriggerCamIn = EC_TRUE;
        }
        else
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Motion Command: CAM In received, but not accepted due to ongoing\n"));
        }
        break;
    case MC_CMD_CODE_CAM_OUT:
        if (!pDemoAxis->pFb->CamOut.Execute)
        {
            EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Motion Command: CAM Out received and accepted\n"));

            pDemoAxis->pFb->CamOut.Deceleration = oMcInpCmd.fDec;
            pDemoAxis->pFb->CamOut.Jerk = oMcInpCmd.fJerk;        /* mm/sec^3   */

            if (!pDemoAxis->pFb->CamOut.Execute)
                pDemoAxis->bTriggerCamOut = EC_TRUE;
        }
        else
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Motion Command: CAM Out received, but not accepted due to ongoing\n"));
        }
        break;
        break;
#endif

    default:
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Invalid motion command code received: Code=%d\n", oMcInpCmd.dwCmdCode));
        break;
    }

    return pDemoAxis;
}

/*
 * Run cyclic part of MCFB's
 */
static EC_T_VOID MotionOnCycle(T_EC_DEMO_APP_CONTEXT* pAppContext, EC_T_DEMO_AXIS* pDemoAxis)
{
    /* power on drive: we have to call this in each cycle */
    pDemoAxis->pFb->Power.OnCycle();
    if (pDemoAxis->pFb->Power.Enable && pDemoAxis->pFb->Power.Error)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_Power Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
            MC_GetErrorText(pDemoAxis->pFb->Power.ErrorID), pDemoAxis->pFb->Power.ErrorID));
    }

    if (pDemoAxis->pFb->Power.Status)
    {
        if (0 == pAppContext->pMyAppDesc->dwMotionStartCycle)
        {
            pAppContext->pMyAppDesc->dwMotionStartCycle = pAppContext->pMyAppDesc->dwJobTaskCycleCnt;
        }
    }

    /* Stop ? */
    pDemoAxis->pFb->Stop.OnCycle();
    if (pDemoAxis->pFb->Stop.Done || pDemoAxis->pFb->Stop.CommandAborted)
    {
        pDemoAxis->pFb->Stop.Execute = EC_FALSE;
    }
    /* Error ? */
    else if (pDemoAxis->pFb->Stop.Error)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_Stop Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
            MC_GetErrorText(pDemoAxis->pFb->Stop.ErrorID), pDemoAxis->pFb->Stop.ErrorID));
    }

    /* Halt ? */
    pDemoAxis->pFb->Halt.OnCycle();
    if (pDemoAxis->pFb->Halt.Done || pDemoAxis->pFb->Halt.CommandAborted)
    {
        pDemoAxis->pFb->Halt.Execute = EC_FALSE;
    }
    /* Error ? */
    else if (pDemoAxis->pFb->Halt.Error)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_Halt Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
            MC_GetErrorText(pDemoAxis->pFb->Halt.ErrorID), pDemoAxis->pFb->Halt.ErrorID));
        pDemoAxis->pFb->Halt.Execute = EC_FALSE;                // Reset error
    }

    /* Reset */
    pDemoAxis->pFb->Reset.OnCycle();
    if (pDemoAxis->pFb->Reset.Done)
    {
        pDemoAxis->pFb->Reset.Execute = EC_FALSE;
    }
    /* Error ? */
    else if (pDemoAxis->pFb->Reset.Error)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_Reset Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
            MC_GetErrorText(pDemoAxis->pFb->Reset.ErrorID), pDemoAxis->pFb->Reset.ErrorID));
        pDemoAxis->pFb->Reset.Execute = EC_FALSE;
    }

    /* Read axis info */
    pDemoAxis->pFb->ReadAxisInfo.Enable = EC_TRUE;
    pDemoAxis->pFb->ReadAxisInfo.OnCycle();

    /* Read Parameter */
    pDemoAxis->pFb->ReadParameter.OnCycle();

    /* Read Bool Parameter */
    pDemoAxis->pFb->ReadBoolParameter.OnCycle();

    /* Write Parameter */
    pDemoAxis->pFb->WriteParameter.OnCycle();
    if (pDemoAxis->pFb->WriteParameter.Execute)
    {
        pDemoAxis->pFb->WriteParameter.Execute = EC_FALSE;
    }

    /* Write Bool Parameter */
    pDemoAxis->pFb->WriteBoolParameter.OnCycle();
    if (pDemoAxis->pFb->WriteBoolParameter.Execute)
    {
        pDemoAxis->pFb->WriteBoolParameter.Execute = EC_FALSE;
    }

    /* read Motion State */
    pDemoAxis->pFb->ReadMotionState.Enable = EC_TRUE;
    pDemoAxis->pFb->ReadMotionState.OnCycle();

    /* read actual position */
    pDemoAxis->pFb->ReadActualPosition.Enable = EC_TRUE;
    pDemoAxis->pFb->ReadActualPosition.OnCycle();

    /* Motion function blocks */
    pDemoAxis->pFb->MoveRelative.OnCycle();

    /* relative move */
#ifndef MOVE_TEST_SEQUENCE
    if (pDemoAxis->pFb->MoveRelative.Execute)
    {
        if (pDemoAxis->pFb->MoveRelative.Error)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_MoveRelative Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->MoveRelative.ErrorID), pDemoAxis->pFb->MoveRelative.ErrorID));
        }

        if (pDemoAxis->pFb->MoveRelative.Done
            || pDemoAxis->pFb->MoveRelative.Error
            || pDemoAxis->pFb->MoveRelative.CommandAborted)      /* move done or aborted ? */
        {
            pDemoAxis->pFb->MoveRelative.Execute = EC_FALSE;
        }
    }
    else if (pDemoAxis->bTriggerMoveRel)
    {
        /* start new movement */
        pDemoAxis->bTriggerMoveRel = EC_FALSE;
        pDemoAxis->pFb->MoveRelative.Execute = EC_TRUE;
        pDemoAxis->bTorqueLimitActive = EC_FALSE;
    }
#endif /* MOVE_TEST_SEQUENCE */

    /* absolute move */
    pDemoAxis->pFb->MoveAbsolute.OnCycle();
    if (pDemoAxis->pFb->MoveAbsolute.Execute)
    {
        if (pDemoAxis->pFb->MoveAbsolute.Error)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_MoveAbsolute Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->MoveAbsolute.ErrorID), pDemoAxis->pFb->MoveAbsolute.ErrorID));
        }

        if (pDemoAxis->pFb->MoveAbsolute.Done
            || pDemoAxis->pFb->MoveAbsolute.Error
            || pDemoAxis->pFb->MoveAbsolute.CommandAborted)     /* move done ? */
        {
            pDemoAxis->pFb->MoveAbsolute.Execute = EC_FALSE;
        }
    }
    else if (pDemoAxis->bTriggerMoveAbs)
    {
        /* start new movement */
        pDemoAxis->bTriggerMoveAbs = EC_FALSE;
        pDemoAxis->pFb->MoveAbsolute.Execute = EC_TRUE;
        pDemoAxis->bTorqueLimitActive = EC_FALSE;
    }

    /* move velocity */
    pDemoAxis->pFb->MoveVelocity.OnCycle();
    if (pDemoAxis->pFb->MoveVelocity.Execute)
    {
        if (pDemoAxis->pFb->MoveVelocity.Error)     /* error ? */
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_MoveVelocity Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->MoveVelocity.ErrorID), pDemoAxis->pFb->MoveVelocity.ErrorID));
            pDemoAxis->pFb->MoveVelocity.Execute = EC_FALSE;
        }
        if (pDemoAxis->pFb->MoveVelocity.CommandAborted) /* This is the regular way to stop this motion */
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: ABORTED MC_MoveVelocity\n"));
            pDemoAxis->pFb->MoveVelocity.Execute = EC_FALSE;
        }
    }
    else if (pDemoAxis->bTriggerMoveVelo)
    {
        /* start new movement */
        pDemoAxis->bTriggerMoveVelo = EC_FALSE;
        pDemoAxis->pFb->MoveVelocity.Execute = EC_TRUE;
        pDemoAxis->bTorqueLimitActive = EC_FALSE;
    }

    /* check target pos reached */
    pDemoAxis->pFb->CheckTargetposReached.InPositionWindow = pDemoAxis->dwPosWindow;
    pDemoAxis->pFb->CheckTargetposReached.OnCycle();
    if (pDemoAxis->bTriggerCheckPos)
    {
        pDemoAxis->bTriggerCheckPos = EC_FALSE;
        pDemoAxis->pFb->CheckTargetposReached.Execute = EC_TRUE;
    }

#if (defined MOTION_ENABLE_TORQUE_LIMIT)
    /* Enable drive's torque limit. The torque limit must be set
    * prior in CoE object 0x2404 and/or 0x2405 (put CoE init command in ENI).
    */
    EC_T_WORD wControlWord = EC_GET_FRM_WORD(pDemoAxis->pwPdControlWord);
    wControlWord |= MOTION_YASK_CTRL_TORQUE_LIMIT_EN;
    EC_SET_FRM_WORD(pDemoAxis->pwPdControlWord, wControlWord);

    // Torque limit active?
    EC_T_WORD wStatusWord = EC_GET_FRM_WORD(pDemoAxis->pwPdStatusWord);
    if (!pDemoAxis->bTorqueLimitActive
        && wStatusWord & MOTION_YASK_STAT_TORQUE_LIMIT_ACT)
    {
        pDemoAxis->bTorqueLimitActive = EC_TRUE;

        /* Sync target position with the actual position. The drive halts immediately
        * if the force is released that hold the drive in the torque limitation.
        */
        pDemoAxis->pFb->HaltRecovery.RecoveryMode = MC_RECOVERY_ABORT_MOVEMENT;
        pDemoAxis->pFb->HaltRecovery.Execute = EC_TRUE;
    }

    pDemoAxis->pFb->HaltRecovery.OnCycle();
    pDemoAxis->pFb->HaltRecovery.Execute = EC_FALSE;

#endif

#ifdef EC_MOTION_SUPPORT_PP_MODE
    // Homing is supported only in PP mode
    // Homing demo
    pDemoAxis->pFb->Home.OnCycle();
    if (pDemoAxis->pFb->Home.Execute)
    {
        if (pDemoAxis->pFb->Home.Error)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_Home Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->Home.ErrorID), pDemoAxis->pFb->Home.ErrorID));
        }

        if (pDemoAxis->pFb->Home.Done
            || pDemoAxis->pFb->Home.Error
            || pDemoAxis->pFb->Home.CommandAborted)     /* move done ? */
        {
            pDemoAxis->pFb->Home.Execute = EC_FALSE;
        }
    }
#endif

#ifdef EC_MOTION_SUPPORT_MC_GEARING
    /* gear in */
    pDemoAxis->pFb->GearIn.OnCycle();
    if (pDemoAxis->pFb->GearIn.Execute)
    {
        if (pDemoAxis->pFb->GearIn.Error)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_GearIn Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->GearIn.ErrorID), pDemoAxis->pFb->GearIn.ErrorID));
        }

        if (pDemoAxis->pFb->GearIn.InGear
            || pDemoAxis->pFb->GearIn.Error
            || pDemoAxis->pFb->GearIn.CommandAborted)     /* gearing in done ? */
        {
            pDemoAxis->pFb->GearIn.Execute = EC_FALSE;
        }
    }
    else if (pDemoAxis->bTriggerGearIn)
    {
        /* execute gearing in */
        pDemoAxis->bTriggerGearIn = EC_FALSE;
        pDemoAxis->pFb->GearIn.Execute = EC_TRUE;
    }

    /* gearing out */
    pDemoAxis->pFb->GearOut.OnCycle();
    if (pDemoAxis->pFb->GearOut.Execute)
    {
        if (pDemoAxis->pFb->GearOut.Error)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: MC_GearOut Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->GearOut.ErrorID), pDemoAxis->pFb->GearOut.ErrorID));
        }

        if (pDemoAxis->pFb->GearOut.Done || pDemoAxis->pFb->GearOut.Error)     /* gear out done ? */
        {
            pDemoAxis->pFb->GearOut.Execute = EC_FALSE;
        }
    }
    else if (pDemoAxis->bTriggerGearOut)
    {
        /* execute gearing out */
        pDemoAxis->bTriggerGearOut = EC_FALSE;
        pDemoAxis->pFb->GearOut.Execute = EC_TRUE;
    }
#endif

#ifdef EC_MOTION_SUPPORT_MC_CAMMING
    pDemoAxis->pFb->CamTableSelect.OnCycle();
    pDemoAxis->pFb->CamIn.OnCycle();
    pDemoAxis->pFb->CamOut.OnCycle();

    /* CAM table select */
    if (pDemoAxis->pFb->CamTableSelect.Execute)
    {
        if (pDemoAxis->pFb->CamTableSelect.Error)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR MC_CamTableSelect Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->CamTableSelect.ErrorID), pDemoAxis->pFb->CamTableSelect.ErrorID));
        }

        if (pDemoAxis->pFb->CamTableSelect.Done || pDemoAxis->pFb->CamTableSelect.Error)     /* done ? */
        {
            S_myCamTableID = pDemoAxis->pFb->CamTableSelect.CamTableIDPtr;       /* save reference to CAM table */
            pDemoAxis->pFb->CamTableSelect.Execute = EC_FALSE;
        }
    }
    else if (pDemoAxis->bTriggerCamTableSelect)
    {
        /* start new movement */
        pDemoAxis->bTriggerCamTableSelect = EC_FALSE;
        pDemoAxis->pFb->CamTableSelect.Execute = EC_TRUE;
    }

    /* CAM In */
    if (pDemoAxis->pFb->CamIn.Execute)
    {
        if (pDemoAxis->pFb->CamIn.Error)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR MC_CamIn Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->CamIn.ErrorID), pDemoAxis->pFb->CamIn.ErrorID));
        }

        if (pDemoAxis->pFb->CamIn.InSync || pDemoAxis->pFb->CamIn.Error || pDemoAxis->pFb->CamIn.CommandAborted)     /* done ? */
        {
            pDemoAxis->pFb->CamIn.Execute = EC_FALSE;
        }
    }
    else if (pDemoAxis->bTriggerCamIn)
    {
        /* start new movement */
        pDemoAxis->bTriggerCamIn = EC_FALSE;
        pDemoAxis->pFb->CamIn.Execute = EC_TRUE;
    }

    /* CAM Out */
    if (pDemoAxis->pFb->CamOut.Execute)
    {
        if (pDemoAxis->pFb->CamOut.Error)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR MC_CamOut Axis %d: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                MC_GetErrorText(pDemoAxis->pFb->CamOut.ErrorID), pDemoAxis->pFb->CamOut.ErrorID));
        }

        if (pDemoAxis->pFb->CamOut.Done || pDemoAxis->pFb->CamOut.Error)     /* done ? */
        {
            pDemoAxis->pFb->CamOut.Execute = EC_FALSE;
        }
    }
    else if (pDemoAxis->bTriggerCamOut)
    {
        /* start new movement */
        pDemoAxis->bTriggerCamOut = EC_FALSE;
        pDemoAxis->pFb->CamOut.Execute = EC_TRUE;
    }
#endif

    // Digital inputs (object 0x60FD) have to be mapped in order to run inputs demo
    // Digital I/O Demo - DISABLED to reduce log clutter
    // Digital outputs (object 0x60FE) have to be mapped in order to run outputs demo
    // if ((pDemoAxis->pdwDigInputs != EC_NULL) || (pDemoAxis->pdwDigOutputs != EC_NULL))
    // {
    //     // Digital I/O Demo - DISABLED
    //     // This entire section has been commented out to eliminate the constant
    //     // digital I/O messages that clutter the motion control logs
    // }
}



/*
* Runs a state machine that moves all configured axes forward and backward.
*
* This code is run if the Motion Demo runs in standalone / unintended mode.
* That means the config's XML-tag MotionDemo/CmdMode is set to false.
* 
* Modified to control ALL configured axes with identical back-and-forth motion.
* Each axis operates independently with the same motion parameters.
*/
#if 0
static void MotionStandaloneModeOnCycle(T_EC_DEMO_APP_CONTEXT* ) { }
#endif

/***************************************************************************************************/
/**
\brief  demo application working process data function.

This function is called in every cycle after the the master stack is started.

*/
static EC_T_DWORD myAppWorkpd(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    // Shared memory for motor increments from external MotorController
    struct IncrementsShm { EC_T_DWORD seq; EC_T_SDWORD horiz_inc; EC_T_SDWORD vert_inc; };
    static int s_inc_shm_fd = -1; static void* s_inc_shm_ptr = EC_NULL; static EC_T_DWORD s_last_seq = 0;
    // Shared memory to publish actual positions for service UI
    struct AxisPosShm { EC_T_DWORD seq; EC_T_SDWORD horiz_pos; EC_T_SDWORD vert_pos; };
    static int s_pos_shm_fd = -1; static AxisPosShm* s_pos_shm_ptr = EC_NULL; static EC_T_DWORD s_pos_seq = 0;
    // Shared memory for absolute setpoints from service UI
    struct AxisSetShm { EC_T_DWORD seq; EC_T_SDWORD horiz_abs; EC_T_SDWORD vert_abs; };
    static int s_set_shm_fd = -1; static AxisSetShm* s_set_shm_ptr = EC_NULL; static EC_T_DWORD s_last_set_seq = 0; static EC_T_DWORD s_last_abs_set_ms = 0; static EC_T_DWORD s_last_gen_ms = 0;
    if (s_inc_shm_fd < 0 || s_inc_shm_ptr == EC_NULL)
    {
        s_inc_shm_fd = shm_open("/motor_increments", O_RDONLY, 0666);
        if (s_inc_shm_fd >= 0)
        {
            s_inc_shm_ptr = mmap(EC_NULL, sizeof(IncrementsShm), PROT_READ, MAP_SHARED, s_inc_shm_fd, 0);
            if (s_inc_shm_ptr == MAP_FAILED) { s_inc_shm_ptr = EC_NULL; close(s_inc_shm_fd); s_inc_shm_fd = -1; }
        }
    }
    if (s_set_shm_fd < 0 || s_set_shm_ptr == EC_NULL)
    {
        s_set_shm_fd = shm_open("/axis_setpoints", O_RDONLY, 0666);
        if (s_set_shm_fd >= 0)
        {
            void* p = mmap(EC_NULL, sizeof(AxisSetShm), PROT_READ, MAP_SHARED, s_set_shm_fd, 0);
            if (p == MAP_FAILED) { s_set_shm_ptr = EC_NULL; close(s_set_shm_fd); s_set_shm_fd = -1; } else { s_set_shm_ptr = (AxisSetShm*)p; }
        }
    }
    if (s_pos_shm_fd < 0)
    {
        s_pos_shm_fd = shm_open("/axis_positions", O_CREAT | O_RDWR, 0666);
        if (s_pos_shm_fd >= 0)
        {
            if (ftruncate(s_pos_shm_fd, (off_t)sizeof(AxisPosShm)) != 0)
            {
                // leave s_pos_shm_ptr as NULL on failure
            }
            void* p = mmap(EC_NULL, sizeof(AxisPosShm), PROT_READ | PROT_WRITE, MAP_SHARED, s_pos_shm_fd, 0);
            if (p == MAP_FAILED) { s_pos_shm_ptr = EC_NULL; } else { s_pos_shm_ptr = (AxisPosShm*)p; }
        }
    }

    // Power enable drives every cycle
    for (EC_T_DWORD i = 0; i < pAppContext->pMyAppDesc->dwAxesCnt; ++i)
    {
        EC_T_DEMO_AXIS* ax = &pAppContext->pMyAppDesc->aoAxisList[i];
        ax->pFb->Power.Enable = MC_TRUE;
        ax->pFb->Power.EnablePositive = MC_TRUE;
        ax->pFb->Power.EnableNegative = MC_TRUE;
    }

    // Home to absolute 0 on startup, once
    static EC_T_BOOL s_homing_done = EC_FALSE;
    static EC_T_DWORD s_home_start_ms = 0;
    static EC_T_DWORD s_home_cmd_mask = 0;         // bit per axis index set when command issued
    // Keep desired and current CSP target position (increments) per axis and initialize after homing
    static EC_T_BOOL s_have_desired[DEMO_MAX_NUM_OF_AXIS] = { EC_FALSE };
    static EC_T_SDWORD s_desired_target[DEMO_MAX_NUM_OF_AXIS] = { 0 };
    static EC_T_BOOL s_have_current[DEMO_MAX_NUM_OF_AXIS] = { EC_FALSE };
    static EC_T_SDWORD s_current_target[DEMO_MAX_NUM_OF_AXIS] = { 0 };
    // Rate limiter and deadband (increments per bus cycle)
    const EC_T_SDWORD MAX_DELTA_PER_CYCLE = 200;   // limit movement per cycle
    const EC_T_SDWORD APPLY_DEADBAND = 100;        // ignore tiny corrections
    EC_T_DWORD expected_mask = 0;
    if (!s_homing_done)
    {
        // Issue MoveAbsolute(0) for all axes as soon as each reports Power.Status
        for (EC_T_DWORD i = 0; i < pAppContext->pMyAppDesc->dwAxesCnt; ++i)
        {
            expected_mask |= (1u << i);
            if ((s_home_cmd_mask & (1u << i)) == 0)
            {
                EC_T_DEMO_AXIS* ax = &pAppContext->pMyAppDesc->aoAxisList[i];
                if (!ax->pFb->Power.Status) continue;
                ax->pFb->MoveAbsolute.Position = 0;
                ax->pFb->MoveAbsolute.Acceleration = (MC_T_REAL)ax->dwDriveAcc;
                ax->pFb->MoveAbsolute.Deceleration = (MC_T_REAL)ax->dwDriveDec;
                ax->pFb->MoveAbsolute.Velocity     = (MC_T_REAL)ax->dwDriveVel;
                ax->pFb->MoveAbsolute.Jerk         = (MC_T_REAL)ax->dwDriveJerk;
                ax->bTriggerMoveAbs = EC_TRUE;
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO,
                    "Homing: Axis %u -> MoveAbsolute(0) acc=%u dec=%u vel=%u\n",
                    ax->wDriveFixAddress, (unsigned)ax->dwDriveAcc, (unsigned)ax->dwDriveDec, (unsigned)ax->dwDriveVel));
                s_home_cmd_mask |= (1u << i);
            }
        }

        // Start homing timer once all commands have been issued
        if (s_home_start_ms == 0 && s_home_cmd_mask == expected_mask)
        {
            s_home_start_ms = OsQueryMsecCount();
        }

        if (s_home_start_ms != 0 && (OsQueryMsecCount() - s_home_start_ms) > 10000)
        {
            // Check within ±100 increments
            EC_T_BOOL all_ok = EC_TRUE;
            for (EC_T_DWORD i = 0; i < pAppContext->pMyAppDesc->dwAxesCnt; ++i)
            {
                EC_T_DEMO_AXIS* ax = &pAppContext->pMyAppDesc->aoAxisList[i];
                if (ax->plPdActualPosition == EC_NULL) continue;
                EC_T_SDWORD act = *ax->plPdActualPosition;
                if (act < -100 || act > 100) all_ok = EC_FALSE;
            }
            if (!all_ok)
            {
                EcLogMsg(EC_LOG_LEVEL_WARNING, (pEcLogContext, EC_LOG_LEVEL_WARNING, "Homing check: position not within ±100 increments on all axes."));
            }
            s_homing_done = EC_TRUE;
            // Initialize desired target positions from actual positions
            for (EC_T_DWORD i = 0; i < pAppContext->pMyAppDesc->dwAxesCnt; ++i)
            {
                EC_T_DEMO_AXIS* ax = &pAppContext->pMyAppDesc->aoAxisList[i];
                if (ax->plPdActualPosition != EC_NULL)
                {
                    s_desired_target[i] = *ax->plPdActualPosition;
                    s_have_desired[i] = EC_TRUE;
                    s_current_target[i] = s_desired_target[i];
                    s_have_current[i] = EC_TRUE;
                        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO,
                        "Init desired target: Axis %u actual=%d\n", ax->wDriveFixAddress, (int)s_desired_target[i]));
                }
            }
        }
    }

    /* get commands from EC-STA */
    if (pAppContext->AppParms.bCmdMode)
    {
        MotionQueuePopCmd(pAppContext);
    }
    else
    {
        // Replace back-and-forth standalone motion with external commands
        // Priority: absolute setpoints (service UI) override incremental deltas (generator/controller)
        // Step 1: read new deltas (if any) and update desired targets per axis
        static EC_T_SDWORD s_last_h = 0;
        static EC_T_SDWORD s_last_v = 0;

        // Absolute setpoints from service UI
        if (s_set_shm_ptr != EC_NULL && s_homing_done)
        {
            AxisSetShm snap = *s_set_shm_ptr; // single read acceptable
            if (snap.seq != s_last_set_seq)
            {
                s_last_set_seq = snap.seq;
                s_last_abs_set_ms = OsQueryMsecCount();
                if ((s_last_set_seq % 10) == 0)
                {
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO,
                        "ServiceSet: seq=%u horiz_abs=%d vert_abs=%d\n",
                        (unsigned)snap.seq, (int)snap.horiz_abs, (int)snap.vert_abs));
                }
                for (EC_T_DWORD i = 0; i < pAppContext->pMyAppDesc->dwAxesCnt; ++i)
                {
                    EC_T_DEMO_AXIS* ax = &pAppContext->pMyAppDesc->aoAxisList[i];
                    if (!ax->pFb->Power.Status) continue;
                    if (ax->wDriveFixAddress == 1002) { s_desired_target[i] = snap.horiz_abs; s_have_desired[i] = EC_TRUE; }
                    else if (ax->wDriveFixAddress == 1001) { s_desired_target[i] = snap.vert_abs;  s_have_desired[i] = EC_TRUE; }
                }
            }
        }

        // Incremental commands from MotorController
        if (s_inc_shm_ptr != EC_NULL && s_homing_done)
        {
            IncrementsShm snap{};
            OsMemcpy(&snap, s_inc_shm_ptr, sizeof(snap));
            if (snap.seq != s_last_seq)
            {
                s_last_seq = snap.seq;
                EC_T_SDWORD delta_h = snap.horiz_inc - s_last_h;
                EC_T_SDWORD delta_v = snap.vert_inc - s_last_v;
                s_last_h = snap.horiz_inc;
                s_last_v = snap.vert_inc;

                // Mark time of last generator activity for dynamic slew parameters
                s_last_gen_ms = OsQueryMsecCount();

                for (EC_T_DWORD i = 0; i < pAppContext->pMyAppDesc->dwAxesCnt; ++i)
                {
                    EC_T_DEMO_AXIS* ax = &pAppContext->pMyAppDesc->aoAxisList[i];
                    if (!ax->pFb->Power.Status) continue;
                    if (!s_have_desired[i])
                    {
                        // initialize if needed
                        s_desired_target[i] = (ax->plPdActualPosition != EC_NULL) ? *ax->plPdActualPosition : 0;
                        s_have_desired[i] = EC_TRUE;
                    }
                    EC_T_SDWORD dist = 0;
                    if (ax->wDriveFixAddress == 1002) dist = delta_h;  // horizontal
                    else if (ax->wDriveFixAddress == 1001) dist = delta_v; // vertical
                    s_desired_target[i] += dist;
                    // throttle logging to every 10th sequence to avoid overload
                    if (dist != 0 && (s_last_seq % 10) == 0)
                    {
                        EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO,
                            "Follow: Axis %u -> desired += %d -> %d seq=%u\n",
                            ax->wDriveFixAddress, (int)dist, (int)s_desired_target[i], (unsigned)s_last_seq));
                    }
                }
            }
        }
    }

    for (EC_T_DWORD dwAxesIdx = 0; dwAxesIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxesIdx++)
    {
        EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[dwAxesIdx];
        MotionOnCycle(pAppContext, pDemoAxis);
    }

    // Apply desired targets to CSP target position after FBL OnCycle has written outputs
    if (s_homing_done)
    {
        EC_T_BOOL bServiceActive = EC_FALSE;
        if (s_last_abs_set_ms != 0)
        {
            EC_T_DWORD nowMs = OsQueryMsecCount();
            if ((nowMs - s_last_abs_set_ms) <= 100) { bServiceActive = EC_TRUE; }
        }
        // Dynamically choose slew parameters:
        EC_T_DWORD nowApply = OsQueryMsecCount();
        EC_T_BOOL bGeneratorActive = (s_last_gen_ms != 0) && ((nowApply - s_last_gen_ms) <= 50); // ~100 Hz window
        const EC_T_SDWORD STOP_IMMEDIATE_THRESH = 50; // inc
        for (EC_T_DWORD i = 0; i < pAppContext->pMyAppDesc->dwAxesCnt; ++i)
        {
            EC_T_DEMO_AXIS* ax = &pAppContext->pMyAppDesc->aoAxisList[i];
            if (!ax->pFb->Power.Status || !s_have_desired[i]) continue;
            if (ax->plPdTargetPosition == EC_NULL) continue;

            if (!s_have_current[i])
            {
                s_current_target[i] = s_desired_target[i];
                s_have_current[i] = EC_TRUE;
            }

            // Select rate limiter params
            EC_T_SDWORD maxDelta = MAX_DELTA_PER_CYCLE;
            EC_T_SDWORD deadband = APPLY_DEADBAND;
            if (bGeneratorActive && !bServiceActive)
            {
                // 5x higher velocity target; smaller deadband for tight following
                maxDelta = MAX_DELTA_PER_CYCLE * 5;
                deadband = (APPLY_DEADBAND / 2);
                if (deadband < 10) deadband = 10;
            }
            

            if (bServiceActive)
            {
                // For service jog: immediate only when stopping (desired ~= actual). Otherwise keep slew limiting.
                EC_T_SDWORD actual = (ax->plPdActualPosition != EC_NULL) ? *ax->plPdActualPosition : s_current_target[i];
                EC_T_SDWORD stopErr = s_desired_target[i] - actual;
                if (stopErr <= STOP_IMMEDIATE_THRESH && stopErr >= -STOP_IMMEDIATE_THRESH)
                {
                    s_current_target[i] = s_desired_target[i];
                }
                else
                {
                    EC_T_SDWORD error = s_desired_target[i] - s_current_target[i];
                    if (error > deadband || error < -deadband)
                    {
                        EC_T_SDWORD step = error;
                        if (step > maxDelta) step = maxDelta;
                        else if (step < -maxDelta) step = -maxDelta;
                        s_current_target[i] += step;
                    }
                }
    }
    else
    {
                EC_T_SDWORD error = s_desired_target[i] - s_current_target[i];
                if (error > deadband || error < -deadband)
                {
                    EC_T_SDWORD step = error;
                    if (step > maxDelta) step = maxDelta;
                    else if (step < -maxDelta) step = -maxDelta;
                    s_current_target[i] += step;
                }
            }
            *ax->plPdTargetPosition = s_current_target[i];
        }
    }

    // Publish actual positions for service UI (after MotionOnCycle and target updates)
    if (s_pos_shm_ptr != EC_NULL)
    {
        EC_T_SDWORD horiz_act = 0, vert_act = 0;
        for (EC_T_DWORD i = 0; i < pAppContext->pMyAppDesc->dwAxesCnt; ++i)
        {
            EC_T_DEMO_AXIS* ax = &pAppContext->pMyAppDesc->aoAxisList[i];
            if (ax->plPdActualPosition == EC_NULL) continue;
            if (ax->wDriveFixAddress == 1002) horiz_act = *ax->plPdActualPosition; // horizontal
            if (ax->wDriveFixAddress == 1001) vert_act  = *ax->plPdActualPosition; // vertical
        }
        s_pos_seq++;
        s_pos_shm_ptr->seq = s_pos_seq;
        s_pos_shm_ptr->horiz_pos = horiz_act;
        s_pos_shm_ptr->vert_pos  = vert_act;
    }

#ifdef MOTION_ENABLE_LOGFILE
    motionLogPos(pAppContext);
#endif

    return EC_E_NOERROR;
}

/***************************************************************************************************/
/**
\brief  demo application doing some diagnostic tasks

This function is called in sometimes from the main demo task
*/
static EC_T_DWORD myAppDiagnosis(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_UNREFPARM(pAppContext);
    return EC_E_NOERROR;
}

#ifdef MOVE_TEST_SEQUENCE
/***************************************************************************************************/
/**
\brief  Does some axis movements

\detail This function is called from the main demo task

*/
static EC_T_DWORD myAppMoveTest(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_UNREFPARM(pAppContext);

    static  EC_T_INT        s_eDistanceState = 0;
    static  EC_T_INT        s_eMotionState = 0;
    static  EC_T_DWORD      s_dwSmooth = 10;
    static  EC_T_WORD       s_wGain = 0xFF00;

#ifdef ELMO_DRIVE
    EC_T_DWORD  dwInt = 0;
    EC_T_DWORD  dwRes = 0;
    EC_T_LREAL  fFloat = 0.0;
#endif

    // We use the axis 1 (index 0) only
    EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[0];

    /* drive available ? */
    if (pDemoAxis->pFb->Power.Status)
    {
        /* we are either in state "move setup", "moving", "pause" */
        switch (s_eMotionState)
        {
        default:
        case 0:         /* move setup */
            pDemoAxis->pFb->MoveRelative.Execute = EC_FALSE;
            pDemoAxis->pFb->MoveRelative.Distance = 20;
            pDemoAxis->pFb->MoveRelative.Velocity = 1000;            /* mm/sec   */
            pDemoAxis->pFb->MoveRelative.Acceleration = 100000;       /* mm/sec^2 */
            pDemoAxis->pFb->MoveRelative.Deceleration = 100000;       /* mm/sec^2 */
            pDemoAxis->pFb->MoveRelative.Jerk = 0;                  /* mm/sec^3 */

            s_eMotionState = 1; /* go to state "moving" */

            break;

        case 1:         /* moving */
#ifdef ELMO_DRIVE
            if (pDemoAxis->pFb->MoveRelative.Distance < 0)
            {
                // Example of using SetSmoothFactor()
                dwRes = SetSmoothFactor(pDemoAxis->wDriveFixAddress, s_dwSmooth);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: SetSmoothFactor() error dwRes=%d\n", dwRes));
                // Example of using SetGainScheduling()
                dwRes = SetGainScheduling(pDemoAxis->wDriveFixAddress, s_wGain);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: SetGainScheduling() error dwRes=%d\n", dwRes));
                // Example of using SetUserInteger():
                // store in slot 1 gain value for moving in negative direction
                dwRes = SetUserInteger(pDemoAxis->wDriveFixAddress, 1, s_wGain);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: SetUserInteger() error dwRes=%d\n", dwRes));
                // Example of using SetUserFloat()
                // store in slot 1 actual position
                dwRes = SetUserFloat(pDemoAxis->wDriveFixAddress, 1, (MC_T_REAL)(*pDemoAxis->plPdActualPosition));
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: SetUserFloat() error dwRes=%d\n", dwRes));

                // Example of using GetUserInteger():
                // recall from slot 2 gain value for moving in positive direction
                dwRes = GetUserInteger(pDemoAxis->wDriveFixAddress, 2, &dwInt);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: GetUserInteger() error dwRes=%d\n", dwRes));
                // Example of using GetUserFloat()
                // recall stored position from slot 2
                dwRes = GetUserFloat(pDemoAxis->wDriveFixAddress, 2, &fFloat);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: GetUserFloat() error dwRes=%d\n", dwRes));
            }
            else
            {
                dwRes = SetSmoothFactor(pDemoAxis->wDriveFixAddress, s_dwSmooth + 20);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: SetSmoothFactor() error dwRes=%d\n", dwRes));
                dwRes = SetGainScheduling(pDemoAxis->wDriveFixAddress, s_wGain ^ 0xffff);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: SetGainScheduling() error dwRes=%d\n", dwRes));
                // store in slot 2 gain value for moving in positive direction
                dwRes = SetUserInteger(pDemoAxis->wDriveFixAddress, 2, s_wGain ^ 0xffff);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: SetUserInteger() error dwRes=%d\n", dwRes));
                // store in slot 2 actual position
                dwRes = SetUserFloat(pDemoAxis->wDriveFixAddress, 2, (MC_T_REAL)(*pDemoAxis->plPdActualPosition));
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: SetUserFloat() error dwRes=%d\n", dwRes));

                // recall from slot 1 gain value for moving in negative direction
                dwRes = GetUserInteger(pDemoAxis->wDriveFixAddress, 1, &dwInt);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: GetUserInteger() dwRes=%d\n", dwRes));
                // recall stored position from slot 1
                dwRes = GetUserFloat(pDemoAxis->wDriveFixAddress, 1, &fFloat);
                if (dwRes != EC_E_NOERROR)
                    EcLogMsg(EC_LOG_LEVEL_INFO, (pEcLogContext, EC_LOG_LEVEL_INFO, "Axis 1: GetUserFloat() dwRes=%d\n", dwRes));
            }
#endif

            // Start axis relative
            pDemoAxis->pFb->MoveRelative.Execute = EC_TRUE;

            s_eMotionState = 2; /* go to state "pause" */

            break;

        case 2:         /* pause */
            if (pDemoAxis->pFb->MoveRelative.Execute)
            {
                if (pDemoAxis->pFb->MoveRelative.Error)
                {
                    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Axis 1: %s Code=%d\n", pDemoAxis->wDriveFixAddress,
                        MC_GetErrorText(pDemoAxis->pFb->MoveRelative.ErrorID), pDemoAxis->pFb->MoveRelative.ErrorID));
                    s_eMotionState = 0;                     /* go to state "pause" */
                }

                if (pDemoAxis->pFb->MoveRelative.Done
                    || pDemoAxis->pFb->MoveRelative.Error
                    || pDemoAxis->pFb->MoveRelative.CommandAborted)      /* move done or aborted ? */
                {
                    pDemoAxis->pFb->MoveRelative.Execute = EC_FALSE;
                }


                if (pDemoAxis->pFb->MoveRelative.Done)
                {
                    pDemoAxis->pFb->MoveRelative.Distance = -pDemoAxis->pFb->MoveRelative.Distance;
                    s_eMotionState = 1; /* go to state "move" */
                }

                if ((pDemoAxis->pFb->MoveRelative.Error)
                    || (pDemoAxis->pFb->MoveRelative.CommandAborted))      /* move done or aborted ? */
                {
                    s_eMotionState = 0;                     /* go to state "pause" */
                }
            }

            break;
        } /* switch(s_eMotionState) */
    }
    else
    {
        pDemoAxis->pFb->MoveRelative.Execute = EC_FALSE;
        s_eMotionState = 0; /* go to state "setup" */
    }

    //OsSleep(1);

    return EC_E_NOERROR;
}
#endif

static void ConvertInParaV2(MC_T_CMD_INP_PARA_V2* p)
{
    MC_SET_DWORD(&p->dwCmdCode, p->dwCmdCode);
    MC_SET_DWORD(&p->dwStationAddress, p->dwStationAddress);
    MC_SET_DWORD(&p->dwIncPerMM, p->dwIncPerMM);
    MC_SET_DWORD(&p->dwDirection, p->dwDirection);
    MC_SET_REAL(&p->fVel, p->fVel);
    MC_SET_REAL(&p->fAcc, p->fAcc);
    MC_SET_REAL(&p->fDec, p->fDec);
    MC_SET_REAL(&p->fJerk, p->fJerk);
    MC_SET_REAL(&p->fDistance, p->fDistance);
    MC_SET_DWORD(&p->dwContUpdate, p->dwContUpdate);
    MC_SET_DWORD(&p->dwParaNumber, p->dwParaNumber);
    MC_SET_REAL(&p->fParaValue, p->fParaValue);
    MC_SET_DWORD((EC_T_DWORD*)&p->bParaValue, p->bParaValue);
}

/********************************************************************************/
/** \brief  Handler for application notifications, see emNotifyApp()
*
* \return EC_E_NOERROR on success, error code otherwise.
*/
static EC_T_DWORD myAppNotify(
    EC_T_DWORD        dwCode, /* [in] Application notification code */
    EC_T_NOTIFYPARMS* pParms  /* [in] Notification parameters */
)
{
    T_EC_DEMO_APP_CONTEXT* pAppContext = (T_EC_DEMO_APP_CONTEXT*)pParms->pCallerData;
    EC_T_DWORD             dwStartCycleCnt = pAppContext->pMyAppDesc->dwJobTaskCycleCnt;      /* for synchronisation with job task */

    MC_T_CMD_INP_PARA_V2   oMcInpParaV2; // Aligned to 8 byte address (for double access)
    MC_T_CMD_OUT_PARA_V2   oMcOutParaV2; // Aligned to 8 byte address (for double access)

    MC_T_CMD_OUT_PARA_V2* pMcOutParaV2 = &oMcOutParaV2;

    OsMemset(&oMcInpParaV2, 0, sizeof(oMcInpParaV2));
    OsMemset(&oMcOutParaV2, 0, sizeof(oMcOutParaV2));

    /* dispatch notification code */
    switch (dwCode)
    {
    case MC_CMD_CODE_POWER_ON:          /* Power-On */
    case MC_CMD_CODE_POWER_OFF:         /* Power-Off */
    case MC_CMD_CODE_STOP:              /* Stop */
    case MC_CMD_CODE_RESET:             /* Reset */
    case MC_CMD_CODE_HALT:              /* Halt */
    case MC_CMD_CODE_MOVE_RELATIVE:     /* Move Relative */
    case MC_CMD_CODE_MOVE_ABSOULTE:     /* Move Absolute */
    case MC_CMD_CODE_MOVE_VELOCITY:     /* Move Velocity */
    case MC_CMD_WRITE_PARAMETER:
    case MC_CMD_WRITE_BOOL_PARAMETER:
#ifdef EC_MOTION_SUPPORT_MC_GEARING
    case MC_CMD_CODE_GEAR_IN:            /* GearIn */
    case MC_CMD_CODE_GEAR_OUT:
#endif
#ifdef EC_MOTION_SUPPORT_MC_CAMMING
    case MC_CMD_CODE_CAM_TABLE_SELECT:
    case MC_CMD_CODE_CAM_IN:
    case MC_CMD_CODE_CAM_OUT:
#endif

        // Homing supported only in PP mode
#ifdef EC_MOTION_SUPPORT_PP_MODE
    case MC_CMD_HOME:                    /* Home */
#endif
    {
        if ((pParms->pbyInBuf != EC_NULL) && (pParms->dwInBufSize == sizeof(MC_T_CMD_INP_PARA_V2)))
        {
            OsMemcpy(&oMcInpParaV2, pParms->pbyInBuf, sizeof(MC_T_CMD_INP_PARA_V2)); // Make an aligned copy
            ConvertInParaV2(&oMcInpParaV2);
        }
        else
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: in myAppNotify: Size of input parameters are not matching: Expected=%d != dwInBufSize=%d\n",
                sizeof(MC_T_CMD_INP_PARA_V2), pParms->dwInBufSize));
            break;
        }
        // Homing supported only in PP mode
#ifdef EC_MOTION_SUPPORT_PP_MODE
        if (dwCode == MC_CMD_HOME)
        {
            EC_T_DEMO_AXIS* pDemoAxis = myAppSearchAxis(pAppContext, oMcInpParaV2.dwStationAddress);

            /* set homing parameters */
            EC_T_DWORD dwRetVal = pDemoAxis->pFb->Axis.SetHomingParameters(HOMING_MODE,
                HOMING_SPEED_SEARCH_SWITCH,
                HOMING_SPEED_SEARCH_ZERO,
                HOMING_ACCELERATION,
                HOMING_OFFSET);
            if (EC_E_NOERROR != dwRetVal)
            {
                EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: Axis 1: Cannot set homing parameters for the drive (Result = %s 0x%x)!\n", ecatGetText(dwRetVal), dwRetVal));
            }
        }
#endif
        S_pPendMotionCmdFifo->Add(oMcInpParaV2);

        break;
    }
    case MC_CMD_CODE_READ_POS:     /* read actual position */
    {
        if ((pParms->pbyInBuf == EC_NULL)
            || (pParms->dwInBufSize < sizeof(MC_T_CMD_INP_PARA_V2))
            || (pParms->pbyOutBuf == EC_NULL))
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: in myAppNotify: Size of parameters are not matching\n"));
            break;
        }

        OsMemcpy(&oMcInpParaV2, pParms->pbyInBuf, sizeof(MC_T_CMD_INP_PARA_V2)); // Make an aligned copy
        // todo ConvertInPara((MC_T_CMD_INP_PARA *) &oMcInpParaV2);

        EC_T_DEMO_AXIS* pDemoAxis = myAppSearchAxis(pAppContext, oMcInpParaV2.dwStationAddress);

        while (pAppContext->pMyAppDesc->dwJobTaskCycleCnt - pAppContext->pMyAppDesc->dwMotionStartCycle < 1); // Wait at least one cycle

        if ((pDemoAxis != EC_NULL) && (pParms->dwOutBufSize == sizeof(MC_T_CMD_READ_POS)))
        {
            MC_SET_REAL(&pMcOutParaV2->oReadPos.fPosition, pDemoAxis->pFb->ReadActualPosition.Position);
            MC_SET_WORD(&pMcOutParaV2->oReadPos.wProfileState, (EC_T_WORD)pDemoAxis->pFb->ReadAxisInfo.DriveState);
            MC_SET_WORD(&pMcOutParaV2->oReadPos.ePLCOpenState, (EC_T_WORD)pDemoAxis->pFb->ReadAxisInfo.PLCOpenState);
            if (pDemoAxis->pwPdStatusWord != EC_NULL) pMcOutParaV2->oReadPos.wStatusWord = EC_GETWORD(pDemoAxis->pwPdStatusWord); // Already little endian
            if (pDemoAxis->pwPdControlWord != EC_NULL) pMcOutParaV2->oReadPos.wControlWord = EC_GETWORD(pDemoAxis->pwPdControlWord); // Already little endian
            MC_SET_SDWORD(&pMcOutParaV2->oReadPos.lActualPosition, (EC_T_INT)(pDemoAxis->pFb->ReadActualPosition.Position * pDemoAxis->dwDriveIncPerMM));
            MC_SET_SDWORD(&pMcOutParaV2->oReadPos.lTargetPosition, (EC_T_INT)(pDemoAxis->pFb->ReadAxisInfo.CommandedPosition * pDemoAxis->dwDriveIncPerMM));
            *pParms->pdwNumOutData = sizeof(MC_T_CMD_READ_POS);
        }
        else
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: in myAppNotify: Size of output parameters are not matching\n"));
        }

        break;
    }
    case MC_CMD_READ_PARAMETER:
    case MC_CMD_READ_BOOL_PARAMETER:
    {
        if ((pParms->pbyInBuf == EC_NULL)
            || (pParms->dwInBufSize != sizeof(MC_T_CMD_INP_PARA_V2))
            || (pParms->pbyOutBuf == EC_NULL)
            || (pParms->dwOutBufSize != sizeof(MC_T_CMD_READ_PARA)))
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: in myAppNotify: Size of parameters are not matching: Expected=%d != dwOutBufSize=%d\n",
                sizeof(MC_T_CMD_READ_PARA), pParms->dwOutBufSize));
            break;
        }

        OsMemcpy(&oMcInpParaV2, pParms->pbyInBuf, sizeof(MC_T_CMD_INP_PARA_V2)); // Make an aligned copy
        ConvertInParaV2(&oMcInpParaV2);

        S_pPendMotionCmdFifo->Add(oMcInpParaV2);

        EC_T_DEMO_AXIS* pDemoAxis = myAppSearchAxis(pAppContext, oMcInpParaV2.dwStationAddress);
        if (pDemoAxis == EC_NULL) break;

        while (pAppContext->pMyAppDesc->dwJobTaskCycleCnt - dwStartCycleCnt < 1); // Wait at least one cycle

        if (dwCode == MC_CMD_READ_PARAMETER)
        {
            MC_SET_DWORD(&pMcOutParaV2->oReadPara.dwParaNumber, (EC_T_DWORD)pDemoAxis->pFb->ReadParameter.ParameterNumber);
            MC_SET_REAL(&pMcOutParaV2->oReadPara.fParaValue, pDemoAxis->pFb->ReadParameter.Value);
            MC_SET_DWORD((EC_T_DWORD*)&pMcOutParaV2->oReadPara.bValid, pDemoAxis->pFb->ReadParameter.Valid);
            MC_SET_DWORD((EC_T_DWORD*)&pMcOutParaV2->oReadPara.bError, pDemoAxis->pFb->ReadParameter.Error);
            MC_SET_WORD(&pMcOutParaV2->oReadPara.wErrorID, pDemoAxis->pFb->ReadParameter.ErrorID);
        }
        else
        {
            MC_SET_DWORD(&pMcOutParaV2->oReadPara.dwParaNumber, (EC_T_DWORD)pDemoAxis->pFb->ReadBoolParameter.ParameterNumber);
            MC_SET_DWORD((EC_T_DWORD*)&pMcOutParaV2->oReadPara.bParaValue, pDemoAxis->pFb->ReadBoolParameter.Value);
            MC_SET_DWORD((EC_T_DWORD*)&pMcOutParaV2->oReadPara.bValid, pDemoAxis->pFb->ReadBoolParameter.Valid);
            MC_SET_DWORD((EC_T_DWORD*)&pMcOutParaV2->oReadPara.bError, pDemoAxis->pFb->ReadBoolParameter.Error);
            MC_SET_WORD(&pMcOutParaV2->oReadPara.wErrorID, pDemoAxis->pFb->ReadBoolParameter.ErrorID);
        }
        *pParms->pdwNumOutData = sizeof(MC_T_CMD_READ_PARA);

        break;
    }
    }

    if (pParms->pbyOutBuf != EC_NULL)
    {
        OsMemcpy(pParms->pbyOutBuf, &oMcOutParaV2, *pParms->pdwNumOutData); // Copy aligned data to unaligned buffer
    }

    return EC_E_NOERROR;
}

/********************************************************************************/
/** \brief  Search axis in list
*
*
* \return  Pointer to axis or EC_NULL if not found
*/
static EC_T_DEMO_AXIS* myAppSearchAxis(T_EC_DEMO_APP_CONTEXT* pAppContext,
    EC_T_DWORD              dwSlaveAddress
)
{
    EC_T_WORD wStationAddress = (EC_T_WORD)(dwSlaveAddress & 0xFFFF);
    EC_T_WORD wAxIdx = (EC_T_WORD)(dwSlaveAddress >> 16);

    int axCnt = 0;
    for (EC_T_DWORD dwAxesIdx = 0; dwAxesIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxesIdx++)
    {
        EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[dwAxesIdx];
        if (pDemoAxis->wDriveFixAddress == wStationAddress) axCnt++;
        if (axCnt == wAxIdx + 1) return pDemoAxis;
    }

    return EC_NULL;
}

#ifdef MOTION_ENABLE_LOGFILE
/********************************************************************************************/
/**
\brief  Initialization motion log file

\return EC_E_NOERROR on success, error code otherwise.
*/
static EC_T_DWORD motionLogEnable
(T_EC_DEMO_APP_CONTEXT* pAppContext,
    EC_T_BYTE* pbyLogBuffer,
    EC_T_DWORD           dwLogNumMsgs,
    EC_T_DWORD           dwMotionLogBufSize,
    EC_T_CHAR* szFilenamePrefix
)
{
    CAtEmLogging* poLogging = (CAtEmLogging*)pAppContext->LogParms.pLogContext;
    MCLOG_DESC* pLogDesc;
    EC_T_DWORD  dwRes = EC_E_NOERROR;
    EC_T_CHAR   szLogFilename[256];

    if ((EC_NULL != szFilenamePrefix) && ('\0' != szFilenamePrefix[0]))
    {
        OsSnprintf(szLogFilename, sizeof(szLogFilename) - 1, "%s_%s", szFilenamePrefix, MOTION_DATALOGGERFILE);
    }
    else
    {
        OsSnprintf(szLogFilename, sizeof(szLogFilename) - 1, "%s", MOTION_DATALOGGERFILE);
    }

    pLogDesc = &S_McLogDesc[pAppContext->dwInstanceId];
    pLogDesc->poLogging = poLogging;
    pLogDesc->pvLogBufDesc = poLogging->AddLogBuffer(
        pAppContext->dwInstanceId,
        MOTION_DATALOGGERROLLOVERLIMIT,
        dwLogNumMsgs,
        EC_FALSE,                           /* do not skip duplicates */
        (EC_T_CHAR*)"Motion",               /* name of the logging (identification) */
        szLogFilename,
        MOTION_DATALOGGERFILEEXTENSION,     /* log file extension */
        EC_FALSE,                           /* print message on console? */
        EC_FALSE);                         /* logging with time stamp? */

    if (pLogDesc->pvLogBufDesc == EC_NULL)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "motionLogEnable: ERROR adding new log buffer\n"));
        dwRes = EC_E_NOMEMORY;
        goto Exit;
    }
    if (pbyLogBuffer != EC_NULL && dwMotionLogBufSize != 0)
    {
        poLogging->SetMsgBuf((MSG_BUFFER_DESC*)pLogDesc->pvLogBufDesc, pbyLogBuffer, dwMotionLogBufSize);
    }

    motionLogWriteRowHeader(pAppContext);

    pAppContext->pMyAppDesc->bMotionLogEnabled = true;

Exit:
    return dwRes;
}

/********************************************************************************/
/** \brief Insert a new message into message buffer
*
* \return N/A
*/
static EC_T_VOID motionLogMsg
(EC_T_DWORD                 dwInstanceID                   /* Master Instance ID */
    , EC_T_CHAR* szFormat, ...)
{
    MCLOG_DESC* pLogDesc;
    EC_T_VALIST vaArgs;


    EC_VASTART(vaArgs, szFormat);
    pLogDesc = &S_McLogDesc[dwInstanceID];
    if (pLogDesc->pvLogBufDesc != EC_NULL)
    {
        pLogDesc->poLogging->InsertNewMsgVa(pLogDesc->pvLogBufDesc, EC_LOG_LEVEL_ERROR, szFormat, vaArgs);
    }

    /* important note: on VxWorks 6.1 PowerPC: re-using vaArgs in the next function call may fail without calling EC_VAEND/EC_VASTART!! */
    EC_VAEND(vaArgs);
    EC_VASTART(vaArgs, szFormat);

    return;
}

/* Calculate actual vel, acc, jerk, lag
*/
static void motionLogCalcMotionDerivatives(EC_T_DEMO_AXIS* pDemoAxis)
{
    MC_T_AXIS_REF* pAxis = &pDemoAxis->pFb->Axis;

    MC_T_REAL fPos = pAxis->lCommandedPosition / (MC_T_REAL)pAxis->dwCalcIncPerMM;
    MC_T_REAL fVel = 0.0, fAcc = 0.0, fJerk = 0.0;

    fVel = pAxis->oMove.lCommandVel / pAxis->fVelToInc;
    if (pAxis->dwDirection == MC_DIR_NEGATIVE) fVel = -fVel;

    fAcc = pAxis->oMove.lCommandAcc / pAxis->fAccToInc;
    fJerk = pAxis->oMove.lCommandJerk / pAxis->fJerkToInc;

    pDemoAxis->log.curPos.fCommandPos = fPos;
    pDemoAxis->log.curPos.fCommandVel = fVel;
    pDemoAxis->log.curPos.fCommandAcc = fAcc;
    pDemoAxis->log.curPos.fCommandJerk = fJerk;
    pDemoAxis->log.curPos.fFollowingError = fPos - (pAxis->lActualPosition / (MC_T_REAL)pAxis->dwCalcIncPerMM);
}

static char* motionLogFormatFp(double val, char* buf, int buflen)
{
    int a = (int)(val * 1000.0 + 0.5);
    OsSnprintf(buf, buflen, "%c%d,%03d",
        (a < 0 ? '-' : ' '),
        abs(a / 1000),
        abs(a % 1000));
    buf[buflen - 1] = '\0';
    return buf;
}

static EC_T_VOID motionLogWriteRowHeader(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
#define LOG_BUFSIZE 200
    EC_T_CHAR szLogBuf[LOG_BUFSIZE];
    EC_T_INT nLen = OsSnprintf(szLogBuf, LOG_BUFSIZE, "Time [ms]; ");

    for (EC_T_DWORD dwAxesIdx = 0; dwAxesIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxesIdx++)
    {
        EC_T_DWORD dwAxesNumber = dwAxesIdx + 1;
        nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen,
            "Ax%d Pos; Ax%d Vel; Ax%d Acc; Ax%d Jer; Ax%d FollowErr; ", dwAxesNumber, dwAxesNumber, dwAxesNumber, dwAxesNumber, dwAxesNumber);
    }

    nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen, "\n");
    szLogBuf[LOG_BUFSIZE - 1] = '\0';
    motionLogMsg(pAppContext->dwInstanceId, szLogBuf);
}

static EC_T_VOID motionLogPos(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    if (!pAppContext->pMyAppDesc->bMotionLogEnabled || (0 == pAppContext->pMyAppDesc->dwMotionStartCycle))
        return;

    EC_T_DWORD motionCycle = pAppContext->pMyAppDesc->dwJobTaskCycleCnt - pAppContext->pMyAppDesc->dwMotionStartCycle;
    EC_T_CHAR szLogBuf[LOG_BUFSIZE];
    EC_T_CHAR szLogFmtBuf[32];
    EC_T_INT nLen = OsSnprintf(szLogBuf, LOG_BUFSIZE, "%d;", motionCycle * pAppContext->AppParms.dwBusCycleTimeUsec / 1000);

    for (EC_T_DWORD dwAxesIdx = 0; dwAxesIdx < pAppContext->pMyAppDesc->dwAxesCnt; dwAxesIdx++)
    {
        EC_T_DEMO_AXIS* pDemoAxis = &pAppContext->pMyAppDesc->aoAxisList[dwAxesIdx];

        if (pDemoAxis->pFb->Power.Status)
        {
            motionLogCalcMotionDerivatives(pDemoAxis);

            /* actual position */
            nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen, "%s;", motionLogFormatFp(pDemoAxis->log.curPos.fCommandPos, szLogFmtBuf, sizeof(szLogFmtBuf)));

            /* commanded velocity */
            nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen, "%s;", motionLogFormatFp(pDemoAxis->log.curPos.fCommandVel, szLogFmtBuf, sizeof(szLogFmtBuf)));

            /* commanded acc */
            nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen, "%s;", motionLogFormatFp(pDemoAxis->log.curPos.fCommandAcc, szLogFmtBuf, sizeof(szLogFmtBuf)));

            /* commanded jerk */
            nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen, "%s;", motionLogFormatFp(pDemoAxis->log.curPos.fCommandJerk, szLogFmtBuf, sizeof(szLogFmtBuf)));

            /* following error */
            nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen, "%s;", motionLogFormatFp(pDemoAxis->log.curPos.fFollowingError, szLogFmtBuf, sizeof(szLogFmtBuf)));
        }
        else
        {
            nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen, ";;;;;");
        }
    }

    nLen = nLen + OsSnprintf(&szLogBuf[nLen], LOG_BUFSIZE - nLen, "\n");
    szLogBuf[LOG_BUFSIZE - 1] = '\0';
    motionLogMsg(pAppContext->dwInstanceId, szLogBuf);
}
#endif

#ifdef ELMO_DRIVE
/***************************************************************************************************/
/**
* \brief    Sets gain scheduling
*
* \detail   An object 0x2E00:00 will be written. May not be called within tEcJobTask.
*
* \param    wStationAddress     EtherCAT station address
* \param    wGainValue          Gain value
*
* \return   EC_E_NOERROR or an error code, i.e. EC_E_SLAVE_NOT_ADDRESSABLE if a slave
*           with station address not found
*/
MC_T_DWORD SetGainScheduling(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_WORD       wValue              /**< [in] Gain value */
)
{
    EC_T_DWORD  dwRetVal = EC_E_NOERROR;
    EC_T_DWORD  dwSlaveId = ecatGetSlaveId(wStationAddress);
    EC_T_WORD   wTmp = wValue;

    if (dwSlaveId != INVALID_SLAVE_ID)
        dwRetVal = ecatCoeSdoDownload(dwSlaveId, DRV_OBJ_GAIN_SCHEDULING, 0,
            (EC_T_BYTE*)&wTmp, sizeof(wTmp), EC_SDO_UP_DOWN_TIMEOUT, 0);
    else
        dwRetVal = EC_E_SLAVE_NOT_ADDRESSABLE;

    return dwRetVal;
}

/***************************************************************************************************/
/**
* \brief    Sets smooth factor
*
* \detail   An object 0x31D9:01 will be written. May not be called within tEcJobTask.
*
* \param    wStationAddress     EtherCAT station address
* \param    wValue              Smooth factor
*
* \return   EC_E_NOERROR or an error code, i.e. EC_E_SLAVE_NOT_ADDRESSABLE if a slave
*           with station address not found
*/
MC_T_DWORD SetSmoothFactor(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_DWORD      dwValue             /**< [in] Smooth factor value */
)
{
    EC_T_DWORD  dwRetVal = EC_E_NOERROR;
    EC_T_DWORD  dwSlaveId = ecatGetSlaveId(wStationAddress);
    EC_T_DWORD  dwTmp = dwValue;

    if (dwSlaveId != INVALID_SLAVE_ID)
        dwRetVal = ecatCoeSdoDownload(dwSlaveId, DRV_OBJ_SMOOTH_FACTOR, 1,
            (EC_T_BYTE*)&dwTmp, sizeof(dwTmp), EC_SDO_UP_DOWN_TIMEOUT, 0);
    else
        dwRetVal = EC_E_SLAVE_NOT_ADDRESSABLE;

    return dwRetVal;
}

/***************************************************************************************************/
/**
* \brief    Sets an integer value in general pupose array
*
* \detail   An object 0x2F00 will be written. May not be called within tEcJobTask.
*
* \param    wStationAddress     EtherCAT station address
* \param    bySubIndex          Subindex 1..24
* \param    dwValue             INT32 value
*
* \return   EC_E_NOERROR or an error code, i.e. EC_E_SLAVE_NOT_ADDRESSABLE if a slave
*           with station address not found
*/
MC_T_DWORD SetUserInteger(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_BYTE       bySubIndex,         /**< [in] Subindex 1..24 */
    MC_T_DWORD      dwValue             /**< [in] Smooth factor value */
)
{
    EC_T_DWORD  dwRetVal = EC_E_NOERROR;
    EC_T_DWORD  dwSlaveId = ecatGetSlaveId(wStationAddress);
    EC_T_DWORD  dwTmp = dwValue;

    if ((bySubIndex < 1) || (bySubIndex > 24))
        return EC_E_INVALIDPARM;

    if (dwSlaveId != INVALID_SLAVE_ID)
        dwRetVal = ecatCoeSdoDownload(dwSlaveId, DRV_OBJ_USER_INT, bySubIndex,
            (EC_T_BYTE*)&dwTmp, sizeof(dwTmp), EC_SDO_UP_DOWN_TIMEOUT, 0);
    else
        dwRetVal = EC_E_SLAVE_NOT_ADDRESSABLE;

    return dwRetVal;
}

/***************************************************************************************************/
/**
* \brief    Gets an integer value from general pupose array
*
* \detail   An object 0x2F00 will be used. This function may not be called within EC main cyclic task.
*
* \param    wStationAddress     EtherCAT station address
* \param    bySubIndex          Subindex 1..24
* \param    pdwValue            Pointer to buffer
*
* \return   EC_E_NOERROR or an error code, i.e. EC_E_SLAVE_NOT_ADDRESSABLE if a slave
*           with station address not found
*/
MC_T_DWORD GetUserInteger(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_BYTE       bySubIndex,         /**< [in] Subindex 1..24 */
    MC_T_DWORD* pdwValue            /**< [in] Pointer to buffer */
)
{
    EC_T_DWORD  dwRetVal = EC_E_NOERROR;
    EC_T_DWORD  dwSlaveId = ecatGetSlaveId(wStationAddress);
    EC_T_DWORD  dwBytesLoaded = 0;
    EC_T_DWORD  dwTmp = 0;

    if (pdwValue == EC_NULL)
        return EC_E_INVALIDPARM;

    if ((bySubIndex < 1) || (bySubIndex > 24))
        return EC_E_INVALIDPARM;

    if (dwSlaveId != INVALID_SLAVE_ID)
        dwRetVal = ecatCoeSdoUpload(dwSlaveId, DRV_OBJ_USER_INT, bySubIndex,
            (EC_T_BYTE*)&dwTmp, sizeof(dwTmp), &dwBytesLoaded, EC_SDO_UP_DOWN_TIMEOUT, 0);
    else
        dwRetVal = EC_E_SLAVE_NOT_ADDRESSABLE;

    *pdwValue = dwTmp;

    return dwRetVal;
}

/***************************************************************************************************/
/**
* \brief    Sets an float value in general pupose array
*
* \detail   An object 0x2F01 will be written. May not be called within tEcJobTask.
*
* \param    wStationAddress     EtherCAT station address
* \param    bySubIndex          Subindex 1..24
* \param    fValue              FLOAT value
*
* \return   EC_E_NOERROR or an error code, i.e. EC_E_SLAVE_NOT_ADDRESSABLE if a slave
*           with station address not found
*/
MC_T_DWORD SetUserFloat(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_BYTE       bySubIndex,         /**< [in] Subindex 1..24 */
    MC_T_REAL       fValue              /**< [in] User float value */
)
{
    EC_T_DWORD  dwRetVal = EC_E_NOERROR;
    EC_T_DWORD  dwSlaveId = ecatGetSlaveId(wStationAddress);
    float       fTmp = (float)fValue;

    if ((bySubIndex < 1) || (bySubIndex > 24))
        return EC_E_INVALIDPARM;

    if (dwSlaveId != INVALID_SLAVE_ID)
        dwRetVal = ecatCoeSdoDownload(dwSlaveId, DRV_OBJ_USER_FLOAT, bySubIndex,
            (EC_T_BYTE*)&fTmp, sizeof(fTmp), EC_SDO_UP_DOWN_TIMEOUT, 0);
    else
        dwRetVal = EC_E_SLAVE_NOT_ADDRESSABLE;

    return dwRetVal;
}

/***************************************************************************************************/
/**
* \brief    Gets an float value from general pupose array
*
* \detail   An object 0x2F01 will be used. This function may not be called within EC main cyclic task.
*
* \param    wStationAddress     EtherCAT station address
* \param    bySubIndex          Subindex 1..24
* \param    pfValue             Pointer to buffer
*
* \return   EC_E_NOERROR or an error code, i.e. EC_E_SLAVE_NOT_ADDRESSABLE if a slave
*           with station address not found
*/
MC_T_DWORD GetUserFloat(
    MC_T_WORD       wStationAddress,    /**< [in] EtherCAT station address */
    MC_T_BYTE       bySubIndex,         /**< [in] Subindex 1..24 */
    MC_T_REAL* pfValue             /**< [in] Pointer to buffer */
)
{
    EC_T_DWORD  dwRetVal = EC_E_NOERROR;
    EC_T_DWORD  dwSlaveId = ecatGetSlaveId(wStationAddress);
    EC_T_DWORD  dwBytesLoaded = 0;
    float       fTmp = 0.0;

    if (pfValue == EC_NULL)
        return EC_E_INVALIDPARM;

    if ((bySubIndex < 1) || (bySubIndex > 24))
        return EC_E_INVALIDPARM;

    if (dwSlaveId != INVALID_SLAVE_ID)
        dwRetVal = ecatCoeSdoUpload(dwSlaveId, DRV_OBJ_USER_FLOAT, bySubIndex,
            (EC_T_BYTE*)&fTmp, sizeof(fTmp), &dwBytesLoaded, EC_SDO_UP_DOWN_TIMEOUT, 0);
    else
        dwRetVal = EC_E_SLAVE_NOT_ADDRESSABLE;

    *pfValue = fTmp;

    return dwRetVal;
}
#endif

EC_T_VOID ShowSyntaxAppUsage(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    const EC_T_CHAR* szAppUsage = "<LinkLayer> [-cfg FileName] [-f ENI-FileName] [-t time] [-b cycle time] [-a affinity] [-v lvl] [-perf [level]] [-log prefix [msg cnt]] [-lic key] [-oem key] [-maxbusslaves cnt] [-readod address] [-printvars]"
#if (defined INCLUDE_RAS_SERVER)
        " [-sp [port]]"
#endif
        " [-dcmmode mode [synctocyclestart]] [-ctloff]"
#if (defined INCLUDE_PCAP_RECORDER)
        " [-rec [prefix [frame cnt]]]"
#endif
        " [-junctionred]\n"
        ;

    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Syntax variant 1:\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "%s [DemoConfigFileName]\n", EC_DEMO_APP_NAME));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   DemoConfigFileName     Use given configuration file (default = %s)\n", DEMO_CFG_DEFAULT_FILENAME));

    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Syntax variant 2:\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "%s %s", EC_DEMO_APP_NAME, szAppUsage));
}

EC_T_VOID ShowSyntaxApp(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -cfg                       Use given file for additional parameters\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     FileName                 file name\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -maxbusslaves              Max number of slaves\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     cnt                      Default = %d\n", MASTER_CFG_ECAT_MAX_BUS_SLAVES));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -flash                     Flash outputs\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     address                  0 = all, >0 = slave station address\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -readod                    Read CoE object dictionary from device\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     address                  0 = MASTER_SLAVE_ID, >0 = slave station address\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -printvars                 Print process variable name and offset for all variables of all slaves\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -dcmmode                   Set DCM mode\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     off                      Off (default)\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     busshift                 BusShift mode (default if configured in ENI)\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     mastershift              MasterShift mode\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     masterrefclock           MasterRefClock mode\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     linklayerrefclock        LinkLayerRefClock mode\n"));
#if (defined INCLUDE_DCX)
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     dcx                      External synchronization mode\n"));
#endif
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "     [synctocyclestart        Sync to cycle start: 0 = disabled (default), 1 = enabled]\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -dcmlog                    Enable DCM logging (default: disabled)\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -ctloff                    Disable DCM control loop for diagnosis (default: enabled)\n"));
#if (defined INCLUDE_PCAP_RECORDER)
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "   -rec                       Record network traffic to pcap file\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "    [prefix                   Pcap file name prefix\n"));
    EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "    [frame cnt]               Frame count for log buffer allocation (default = %d, with %d bytes per message)]\n", PCAP_RECORDER_BUF_FRAME_CNT, ETHERNET_MAX_FRAMEBUF_LEN));
#endif
}

/*-END OF SOURCE FILE--------------------------------------------------------*/
