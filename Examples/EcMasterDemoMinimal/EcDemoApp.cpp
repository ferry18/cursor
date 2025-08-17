/*-----------------------------------------------------------------------------
 * EcDemoApp.cpp
 * Copyright                acontis technologies GmbH, Ravensburg, Germany
 * Response                 Cyril Eyssautier
 * Description              EC-Master demo application
 *---------------------------------------------------------------------------*/

/*-INCLUDES------------------------------------------------------------------*/
#include "EcDemoApp.h"

/*-FUNCTION DECLARATIONS-----------------------------------------------------*/
static EC_T_VOID  EcMasterJobTask(EC_T_VOID* pvAppContext);

/*-MYAPP---------------------------------------------------------------------*/
static EC_T_DWORD myAppInit(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppPrepare(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppSetup(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppWorkpd(T_EC_DEMO_APP_CONTEXT* pAppContext);
static EC_T_DWORD myAppDiagnosis(T_EC_DEMO_APP_CONTEXT* pAppContext);

/********************************************************************************/
/* \brief Minimal printf based logging
 * Return: N/A
 */
EC_T_LOG_PARMS  G_aLogParms[1];
EC_T_LOG_PARMS* G_pEcLogParms = G_aLogParms;
EC_T_DWORD EcDemoLogMsg(struct _EC_T_LOG_CONTEXT* pContext, EC_T_DWORD dwLogMsgSeverity, const EC_T_CHAR* szFormat, ...)
{
    EC_T_VALIST vaArgs; EC_UNREFPARM(pContext); EC_UNREFPARM(dwLogMsgSeverity);
    EC_VASTART(vaArgs, szFormat);
    OsVprintf(szFormat, vaArgs);
    EC_VAEND(vaArgs);
    return EC_E_NOERROR;
}

/********************************************************************************/
/** \brief EC-Master demo application.
*
* This is an EC-Master demo application.
*
* \return  Status value.
*/
EC_T_DWORD EcDemoApp(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_T_DWORD           dwRetVal        = EC_E_NOERROR;
    EC_T_DWORD           dwRes           = EC_E_NOERROR;
    T_EC_DEMO_APP_PARMS* pAppParms       = &pAppContext->AppParms;
    EC_T_DWORD           dwDemoCountDown = pAppParms->dwDemoDuration;
    EC_T_VOID*           pvJobTaskHandle = EC_NULL;

    /* initialize application */
    dwRes = myAppInit(pAppContext);

    /* initialize EtherCAT master */
    {
        EC_T_INIT_MASTER_PARMS oInitParms;

        OsMemset(&oInitParms, 0, sizeof(EC_T_INIT_MASTER_PARMS));
        oInitParms.dwSignature           = ATECAT_SIGNATURE;
        oInitParms.dwSize                = sizeof(EC_T_INIT_MASTER_PARMS);
        oInitParms.pOsParms              = &pAppParms->Os;
        oInitParms.pLinkParms            = pAppParms->apLinkParms[0];
        oInitParms.pLinkParmsRed         = pAppParms->apLinkParms[1];
        oInitParms.dwBusCycleTimeUsec    = pAppParms->dwBusCycleTimeUsec;
        oInitParms.dwMaxAcycFramesQueued = MASTER_CFG_MAX_ACYC_FRAMES_QUEUED;
        OsMemcpy(&oInitParms.LogParms, &pAppContext->LogParms, sizeof(EC_T_LOG_PARMS));
        oInitParms.LogParms.dwLogLevel = pAppParms->dwMasterLogLevel;

        dwRes = ecatInitMaster(&oInitParms);
        if (dwRes != EC_E_NOERROR)
        {
            dwRetVal = dwRes;
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot initialize EtherCAT-Master: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            goto Exit;
        }
    }
    /* create cyclic task to trigger jobs */
    pAppContext->bJobTaskRunning = EC_FALSE;
    pAppContext->bJobTaskShutdown = EC_FALSE;
    pvJobTaskHandle = OsCreateThread((EC_T_CHAR*)"EcMasterJobTask", EcMasterJobTask, pAppParms->CpuSet,  pAppParms->dwJobsThreadPrio, pAppParms->dwJobsThreadStackSize, (EC_T_VOID*)pAppContext);
    
    while (!pAppContext->bJobTaskRunning)
    {        
        OsSleep(10);
    }
    /* configure network */
    dwRes = ecatConfigureNetwork(pAppParms->eCnfType, pAppParms->pbyCnfData, pAppParms->dwCnfDataLen);
    if (dwRes != EC_E_NOERROR)
    {
        dwRetVal = dwRes;
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot configure EtherCAT-Master: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        goto Exit;
    }
    /* set master to INIT */
    dwRes = ecatSetMasterState(ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_INIT);
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot start set master state to INIT: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        dwRetVal = dwRes;
        goto Exit;
    }
    /* prepare application */
    dwRes = myAppPrepare(pAppContext);

    /* set master to PREOP */
    dwRes = ecatSetMasterState(ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_PREOP);
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot start set master state to PREOP: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        dwRetVal = dwRes;
        goto Exit;
    }
    /* setup application */
    dwRes = myAppSetup(pAppContext);

    if (EC_NULL != pAppParms->pbyCnfData)
    {
        /* set master to OP */
        dwRes = ecatSetMasterState(ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_OP);
        if (dwRes != EC_E_NOERROR)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot start set master state to OP: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
            dwRetVal = dwRes;
            goto Exit;
        }
    }
    while (bRun)
    {
        if (0 != pAppParms->dwDemoDuration)
        {
            dwDemoCountDown--;
            if (0 == dwDemoCountDown)
            {
                bRun = EC_FALSE;
            }
        }
        OsSleep(1);
        if (0 == (dwDemoCountDown % 100))
        {
            dwRes = myAppDiagnosis(pAppContext);
        }
    }

    /* set master to INIT */
    dwRes = ecatSetMasterState(ETHERCAT_STATE_CHANGE_TIMEOUT, eEcatState_INIT);
    if (dwRes != EC_E_NOERROR)
    {
        EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "Cannot start set master state to INIT: %s (0x%lx))\n", ecatGetText(dwRes), dwRes));
        dwRetVal = dwRes;
        goto Exit;
    }
Exit:
    /* shutdown JobTask */
    pAppContext->bJobTaskShutdown = EC_TRUE;
    OsDeleteThreadHandle(pvJobTaskHandle);

    /* deinitialize master */
    dwRes = ecatDeinitMaster();

    return dwRetVal;
}

/********************************************************************************/
/** \brief  Trigger EC-Master jobs
*
* \return N/A
*/
static EC_T_VOID EcMasterJobTask(EC_T_VOID* pvAppContext)
{
    EC_T_DWORD dwRes = EC_E_NOERROR;
    T_EC_DEMO_APP_CONTEXT* pAppContext = (T_EC_DEMO_APP_CONTEXT*)pvAppContext;
    EC_T_STATE eMasterState = eEcatState_UNKNOWN;

    OsWaitForEvent(pAppContext->pvJobTaskEvent, EC_WAITINFINITE);
    pAppContext->bJobTaskRunning = EC_TRUE;

    while (!pAppContext->bJobTaskShutdown)
    {
        /* wait for next cycle (event from scheduler task) */
        dwRes = OsWaitForEvent(pAppContext->pvJobTaskEvent, 3000);
        if (EC_E_NOERROR != dwRes)
        {
            EcLogMsg(EC_LOG_LEVEL_ERROR, (pEcLogContext, EC_LOG_LEVEL_ERROR, "ERROR: OsWaitForEvent(): %s (0x%lx)\n", ecatGetText(dwRes), dwRes));
            OsSleep(500);
        }

        /* process all received frames (read new input values) */
        dwRes = ecatExecJob(eUsrJob_ProcessAllRxFrames, EC_NULL);
        EC_UNREFPARM(dwRes);

        /* process data */
        eMasterState = ecatGetMasterState();
        if ((eEcatState_SAFEOP == eMasterState) || (eEcatState_OP == eMasterState))
        {
            dwRes = myAppWorkpd(pAppContext);
            EC_UNREFPARM(dwRes);
        }
        /* write output values of current cycle, by sending all cyclic frames */
        dwRes = ecatExecJob(eUsrJob_SendAllCycFrames, EC_NULL);
        EC_UNREFPARM(dwRes);

        /* execute some administrative jobs. No bus traffic is performed by this function */
        dwRes = ecatExecJob(eUsrJob_MasterTimer, EC_NULL);
        EC_UNREFPARM(dwRes);

        /* send queued acyclic EtherCAT frames */
        dwRes = ecatExecJob(eUsrJob_SendAcycFrames, EC_NULL);
        EC_UNREFPARM(dwRes);
    }
}

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

/***************************************************************************************************/
/**
\brief  Initialize Slave Instance.

Find slave parameters.
\return EC_E_NOERROR on success, error code otherwise.
*/
static EC_T_DWORD myAppPrepare(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_UNREFPARM(pAppContext);
    return EC_E_NOERROR;
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
    EC_UNREFPARM(pAppContext);
    return EC_E_NOERROR;
}

/***************************************************************************************************/
/**
\brief  demo application working process data function.

  This function is called in every cycle after the the master stack is started.

*/
static EC_T_DWORD myAppWorkpd(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_UNREFPARM(pAppContext);
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

EC_T_VOID ShowSyntaxAppUsage(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    const EC_T_CHAR* szAppUsage = "<LinkLayer> [-f ENI-FileName] [-t time] [-b cycle time]";
    EcLogMsg(EC_LOG_LEVEL_ANY, (pEcLogContext, EC_LOG_LEVEL_ANY, "%s V%s for %s %s\n", EC_DEMO_APP_NAME, EC_FILEVERSIONSTR, ATECAT_PLATFORMSTR, EC_COPYRIGHT));
    EcLogMsg(EC_LOG_LEVEL_ANY, (pEcLogContext, EC_LOG_LEVEL_ANY, "Syntax:\n"));
    EcLogMsg(EC_LOG_LEVEL_ANY, (pEcLogContext, EC_LOG_LEVEL_ANY, "%s %s", EC_DEMO_APP_NAME, szAppUsage));
}
EC_T_VOID ShowSyntaxApp(T_EC_DEMO_APP_CONTEXT* pAppContext)
{
    EC_UNREFPARM(pAppContext);
}

/*-END OF SOURCE FILE--------------------------------------------------------*/