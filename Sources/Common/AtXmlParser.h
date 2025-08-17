/*-----------------------------------------------------------------------------
 * AtXmlParser.h
 * Copyright                acontis technologies GmbH, Ravensburg, Germany
 * Response                 Paul Bussmann
 * Description              Basic XML parameter file parsing
 *---------------------------------------------------------------------------*/

#ifndef INC_ATPARAMETER
#define INC_ATPARAMETER

/*-INCLUDE-------------------------------------------------------------------*/
#include "EcType.h"
#include "EcError.h"
#include "EcOsPlatform.h"

/*-COMPILER SETTINGS---------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

/*-DEFINES/MACROS------------------------------------------------------------*/
#define AT_MAX_PATH_LEN     512

/*-TYPEDEFS/ENUMS------------------------------------------------------------*/
typedef enum _ATPARAM_DATA_TYPE
{
    eDataType_None,
    eDataType_BOOL,
    eDataType_CHAR,
    eDataType_DWORD,
    eDataType_STRING,

    /* Borland C++ datatype alignment correction */
    eDataType_BCppDummy   = 0xFFFFFFFF
} ATPARAM_DATA_TYPE;

/*-FUNCTION DECLARATION------------------------------------------------------*/
EC_T_DWORD  atOpenXmlFile(const EC_T_CHAR* szXmlFilePath, EC_T_VOID** ppvHandle);
EC_T_DWORD  atOpenXmlBuffer(const EC_T_CHAR* szBuffer, EC_T_DWORD dwLength, EC_T_VOID** ppvHandle);
EC_T_DWORD  atParamRead(EC_T_VOID* pvHandle, ATPARAM_DATA_TYPE eDataType, const EC_T_CHAR* szPath, EC_T_BYTE* pbyOut, EC_T_DWORD dwOutLen);
EC_T_DWORD  atCloseXmlFile(EC_T_VOID** ppvHandle);

/*-COMPILER SETTINGS---------------------------------------------------------*/
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INC_ATPARAMETER */
