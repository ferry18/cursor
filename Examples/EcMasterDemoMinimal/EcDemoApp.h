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
#define EXCLUDE_EC_LOGGING

#define INCLUDE_EC_MASTER

/*-INCLUDES------------------------------------------------------------------*/
#include "EcMaster.h"
#include "EcDemoPlatform.h"
#include "EcDemoParms.h"
#include "EcSelectLinkLayer.h"
#include "EcDemoTimingTaskPlatform.h"

/*-DEFINES-------------------------------------------------------------------*/
#define EC_DEMO_APP_NAME (EC_T_CHAR*)"EcMasterDemoMinimal"

/*-LOGGING-------------------------------------------------------------------*/
extern struct _EC_T_LOG_PARMS G_aLogParms[];
extern EC_T_LOG_PARMS* G_pEcLogParms;
EC_T_DWORD EcDemoLogMsg(struct _EC_T_LOG_CONTEXT* pContext, EC_T_DWORD dwLogMsgSeverity, const EC_T_CHAR* szFormat, ...);

/*-FUNCTION DECLARATIONS-----------------------------------------------------*/
EC_T_VOID  ShowSyntaxAppUsage(T_EC_DEMO_APP_CONTEXT* pAppContext);
EC_T_VOID  ShowSyntaxApp(T_EC_DEMO_APP_CONTEXT* pAppContext);
EC_T_VOID  ShowSyntaxLinkLayer(EC_T_VOID);
EC_T_DWORD EcDemoApp(T_EC_DEMO_APP_CONTEXT* pAppContext);

#endif /* INC_ECDEMOAPP_H */

/*-END OF SOURCE FILE--------------------------------------------------------*/
