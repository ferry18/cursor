/*-----------------------------------------------------------------------------
 * EcDemoApp.h
 * Copyright                acontis technologies GmbH, Ravensburg, Germany
 * Response                 Holger Oelhaf
 * Description              Application specific settings for EC-Master demo
 *---------------------------------------------------------------------------*/

#ifndef INC_ECDEMOAPP_H
#define INC_ECDEMOAPP_H 1

/*-LOGGING-------------------------------------------------------------------*/
#ifndef pEcLogParms
#define pEcLogParms (&(pAppContext->LogParms))
#endif

#define INCLUDE_EC_MASTER
#define INCLUDE_EC_MOTION_CONTROL

/*-INCLUDES------------------------------------------------------------------*/
#include "EcMaster.h"
#include "EcDemoPlatform.h"
#include "EcDemoParms.h"
#include "EcLogging.h"
#include "EcNotification.h"
#include "EcSdoServices.h"
#include "EcSelectLinkLayer.h"
#include "EcSlaveInfo.h"
#include "AtXmlParser.h"
#include "EcDemoTimingTaskPlatform.h"

#ifdef VXWORKS
#include "wvLib.h"
#endif

#if (defined EC_SIMULATOR_DS402)
#include "EcSimulatorDs402.h"
#endif

/*-DEFINES-------------------------------------------------------------------*/
#define EC_DEMO_APP_NAME (EC_T_CHAR*)"EcMasterDemoMotion"
#ifndef MOTION_DEMO
#define MOTION_DEMO
#endif

/* the RAS server is necessary to support the EC-Engineer or other remote applications */
#if (!defined INCLUDE_RAS_SERVER) && (defined EC_SOCKET_SUPPORTED)
#define INCLUDE_RAS_SERVER
#endif

#if (defined INCLUDE_RAS_SERVER)
#include "EcRasServer.h"
#define ECMASTERRAS_MAX_WATCHDOG_TIMEOUT    10000
#define ECMASTERRAS_CYCLE_TIME              2
#endif

#define MAX_LINKLAYER 5

/******************************/
/* EC-Master DC configuration */
/******************************/
#define ETHERCAT_DC_TIMEOUT             12000   /* DC initialization timeout in ms */
#define ETHERCAT_DC_ARMW_BURSTCYCLES    10000   /* DC burst cycles (static drift compensation) */
#define ETHERCAT_DC_ARMW_BURSTSPP       12      /* DC burst bulk (static drift compensation) */
#define ETHERCAT_DC_DEV_LIMIT           13      /* DC deviation limit (highest bit tolerate by the broadcast read) */
#define ETHERCAT_DC_SETTLE_TIME         1500    /* DC settle time in ms */
#define ETHERCAT_DCM_TIMEOUT            30000   /* DC initialization timeout in ms */

/***************************/
/* EC-Motion configuration */
/***************************/
#define DEMOCONFIG_XML_NAME             "DemoConfig.xml"

/* Default values */
/* -------------- */
#define DEFAULT_INCPERMM                100000  /* [inc] increments per mm */

/* Move parameters */
#define DEFAULT_DISTANCE                500     /* [mm]     distance                */
#define DEFAULT_MAX_VELOCITY            1000    /* [mm/s]   maximum velocity        */
#define DEFAULT_ACC                     2000    /* [mm/s^2] maximum acceleration    */
#define DEFAULT_DEC                     3000    /* [mm/s^2] maximum deceleration increments/cycle^2 */
#define DEFAULT_JERK                    6000    /* [mm/s^3] jerk */
#define DEFAULT_POS_WINDOW              10
#define DEFAULT_VEL_GAIN                1
#define DEFAULT_TORQUE_GAIN             1

#define COMMAND_LINE_BUFFER_LENGTH      512
#define LINK_LAYER_PARAMETERS_LENGTH    256

#if (defined EC_VERSION_TIRTOS) || (defined EC_VERSION_RIN32M3) ||\
    (defined EC_VERSION_XILINX_STANDALONE) || (defined EC_VERSION_RZT1) ||\
    (defined EC_VERSION_UCOS)
extern EC_T_BYTE  DemoConfig_xml_data[];
extern EC_T_DWORD DemoConfig_xml_data_size;

#define STATIC_DEMOCONFIG_XML_DATA      DemoConfig_xml_data
#define STATIC_DEMOCONFIG_XML_DATA_SIZE DemoConfig_xml_data_size
#endif

/* tag names for DemoConfig.xml file */
#define DEMO_CFG_TAG_ENI_FILENAME                   (EC_T_CHAR*)"Config\\Common\\ENIFileName"
#define DEMO_CFG_TAG_LOG_FILEPREFIX                 (EC_T_CHAR*)"Config\\Common\\LogFilePrefix"
#define DEMO_CFG_TAG_LINK_LAYER                     (EC_T_CHAR*)"Config\\Common\\LinkLayer"
#define DEMO_CFG_TAG_LINK_LAYER2                    (EC_T_CHAR*)"Config\\Common\\LinkLayer2"
#define DEMO_CFG_TAG_DURATION                       (EC_T_CHAR*)"Config\\Common\\DemoDuration"
#define DEMO_CFG_TAG_CPU_AFFINITY                   (EC_T_CHAR*)"Config\\Common\\CpuAffinity"
#define DEMO_CFG_TAG_VERBOSITY_LVL                  (EC_T_CHAR*)"Config\\Common\\VerbosityLevel"
#define DEMO_CFG_TAG_PERF_MEASURE                   (EC_T_CHAR*)"Config\\Common\\PerfMeasurement"
#define DEMO_CFG_TAG_PCAP_RECORDER                  (EC_T_CHAR*)"Config\\Common\\PcapRecorder"
#define DEMO_CFG_TAG_RAS_ENABLED                    (EC_T_CHAR*)"Config\\Common\\RASEnabled"
#define DEMO_CFG_TAG_RAS_PORT                       (EC_T_CHAR*)"Config\\Common\\RASPort"
#define DEMO_CFG_TAG_AUXCLK                         (EC_T_CHAR*)"Config\\Common\\AuxClk"
#define DEMO_CFG_TAG_BUSCYCLETIME                   (EC_T_CHAR*)"Config\\Common\\BusCycleTime"

#define MOTIONDEMO_CFG_TAG_NO_BUS_SHIFT             (EC_T_CHAR*)"Config\\MotionDemo\\NoDCMBusShift"             /* Disable DCM controller */
#define MOTIONDEMO_CFG_TAG_CTL_SETVAL               (EC_T_CHAR*)"Config\\MotionDemo\\DCMCtlSetVal"              /* DCM controller set value in nanosec */
#define MOTIONDEMO_CFG_TAG_CMD_MODE                 (EC_T_CHAR*)"Config\\MotionDemo\\CmdMode"                   /* Command Mode */

#define MOTIONDEMO_CFG_TAG_DRIVE_ADDR               (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\Address"          /* EtherCAT Address */
#define MOTIONDEMO_CFG_TAG_DRIVE_MODE               (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\OperationMode"    /* Mode of operation */
#define MOTIONDEMO_CFG_TAG_DRIVE_NO                 (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\SercosDriveNo"    /* Adressed drive number within SERCOS servo controller */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_OPMODE         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\CoeIdxOpMode"     /* Index of CoE object 'Operation Mode'. Default 0x6060 */
#define MOTIONDEMO_CFG_TAG_DRIVE_PROFILE            (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\DriveProfile"     /* "VIRTUAL" or "DS402" or "SERCOS" */
#define MOTIONDEMO_CFG_TAG_DRIVE_INCPERMM           (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IncPerMM"         /* Increments per mm */
#define MOTIONDEMO_CFG_TAG_DRIVE_INCFACTOR          (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IncFactor"        /* Increments are scaled with this value */
#define MOTIONDEMO_CFG_TAG_DRIVE_ACC                (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\Acc"              /* Move acc */
#define MOTIONDEMO_CFG_TAG_DRIVE_DEC                (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\Dec"              /* Move dec */
#define MOTIONDEMO_CFG_TAG_DRIVE_VEL                (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\Vel"              /* Move vel */
#define MOTIONDEMO_CFG_TAG_DRIVE_JERK               (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\Jerk"             /* Move jerk */
#define MOTIONDEMO_CFG_TAG_DRIVE_DIST               (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\Distance"         /* Move distance */
#define MOTIONDEMO_CFG_TAG_DRIVE_POS_WIND           (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\PositionWindow"   /* Position Window */
#define MOTIONDEMO_CFG_TAG_DRIVE_VEL_GAIN           (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\VelocityGain"     /* Velocity gain */
#define MOTIONDEMO_CFG_TAG_DRIVE_TOR_GAIN           (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\TorqueGain"       /* Torque gain */

#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_STATUS         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxStatus"        /* Object Index Statusword */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_CONTROL        (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxControl"       /* Object Index Controlword */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_POSACT         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxPosact"        /* Object Index Position actual */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_TARPOS         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxTargetpos"     /* Object Index Target Position */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_TARVEL         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxTargetvel"     /* Object Index Target Velocity */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_ACTTRQ         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxActualtrq"     /* Object Index Actual Torque */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_TARTRQ         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxTargettrq"     /* Object Index Target Torque */

#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_VELOFF         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxVelOffset"     /* Object Index Velocity Offset */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_TOROFF         (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxTorOffset"     /* Object Index Torque Offset */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_MODE           (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxMode"          /* Object Index Mode of Operation */
#define MOTIONDEMO_CFG_TAG_DRIVE_IDX_MODE_DISPLAY   (EC_T_CHAR*)"Config\\MotionDemo\\Drive%d\\IdxModeDisplay"   /* Object Index Mode of Operation Dissplay */

/*-FUNCTION DECLARATIONS-----------------------------------------------------*/
EC_T_VOID  ShowSyntaxAppUsage(T_EC_DEMO_APP_CONTEXT* pAppContext);
EC_T_VOID  ShowSyntaxApp(T_EC_DEMO_APP_CONTEXT* pAppContext);
EC_T_VOID  ShowSyntaxLinkLayer(EC_T_VOID);
EC_T_DWORD EcDemoApp(T_EC_DEMO_APP_CONTEXT* pAppContext);

#define PRINT_PERF_MEAS() ((EC_NULL != pEcLogContext)?((CAtEmLogging*)pEcLogContext)->PrintPerfMeas(pAppContext->dwInstanceId, 0, pEcLogContext) : 0)
#define PRINT_HISTOGRAM() ((EC_NULL != pEcLogContext)?((CAtEmLogging*)pEcLogContext)->PrintHistogramAsCsv(pAppContext->dwInstanceId, pAppContext->pvPerfMeas) : 0)

#endif /* INC_ECDEMOAPP_H */

/*-END OF SOURCE FILE--------------------------------------------------------*/
