/* SPDX-License-Identifier: GPL-2.0 OR acontis license matching project */
#include "EcOs.h"      /* must be included before EcLink.h */
#include "EcLink.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

struct Atl2Priv { int fd; EC_T_RECEIVEFRAMECALLBACK pfRxCb; EC_T_VOID* pvCtx; };

static EC_T_LINKMODE EC_FNCALL Atl2_GetMode(EC_T_VOID* inst) { EC_UNREFPARM(inst); return EcLinkMode_POLLING; }
static EC_T_DWORD    EC_FNCALL Atl2_Ioctl(EC_T_VOID* inst, EC_T_DWORD code, EC_T_LINK_IOCTLPARMS* p) { EC_UNREFPARM(inst); EC_UNREFPARM(code); EC_UNREFPARM(p); return EC_E_NOERROR; }

static EC_T_DWORD EC_FNCALL Atl2_Open(EC_T_VOID* pvLinkParms, EC_T_RECEIVEFRAMECALLBACK pfReceive, EC_T_LINK_NOTIFY pfNotify, EC_T_VOID* pvContext, EC_T_VOID** ppvInstance)
{
    EC_UNREFPARM(pvLinkParms); EC_UNREFPARM(pfNotify);
    if (ppvInstance == EC_NULL) return EC_E_INVALIDPARM;
    Atl2Priv* priv = (Atl2Priv*)OsMalloc(sizeof(Atl2Priv));
    if (!priv) return EC_E_NOMEMORY;
    OsMemset(priv, 0, sizeof(*priv));
    priv->pfRxCb = pfReceive; priv->pvCtx = pvContext;
    priv->fd = open("/dev/ec_aqc113_rt0", O_RDWR|O_CLOEXEC);
    if (priv->fd < 0) { SafeOsFree(priv); return EC_E_OPENFAILED; }
    /* Configure rings: prefer power-of-two sizes suitable for 250us cycle */
    struct { unsigned int tx_cnt; unsigned int rx_cnt; } cfg;
    cfg.tx_cnt = 512; cfg.rx_cnt = 1024;
    (void)ioctl(priv->fd, _IOW(0xEA, 10, typeof(cfg)), &cfg);
    *ppvInstance = priv;
    return EC_E_NOERROR;
}

static EC_T_DWORD EC_FNCALL Atl2_Close(EC_T_VOID* inst)
{
    Atl2Priv* priv = (Atl2Priv*)inst;
    if (!priv) return EC_E_INVALIDPARM;
    if (priv->fd >= 0) close(priv->fd);
    SafeOsFree(priv);
    return EC_E_NOERROR;
}

static EC_T_DWORD EC_FNCALL Atl2_SendFrame(EC_T_VOID* inst, EC_T_LINK_FRAMEDESC* pDesc)
{
    Atl2Priv* priv = (Atl2Priv*)inst;
    if (!priv || priv->fd < 0 || !pDesc || !pDesc->pbyFrame) return EC_E_INVALIDPARM;
    struct { unsigned long user_ptr; unsigned int len; } io;
    io.user_ptr = (unsigned long)pDesc->pbyFrame;
    io.len = pDesc->dwSize;
    if (ioctl(priv->fd, _IOW(0xEA, 11, typeof(io)), &io) != 0)
        return EC_E_ERROR;
    return EC_E_NOERROR;
}

static EC_T_DWORD EC_FNCALL Atl2_SendAndFree(EC_T_VOID* inst, EC_T_LINK_FRAMEDESC* pDesc)
{
    EC_T_DWORD dw = Atl2_SendFrame(inst, pDesc);
    if (pDesc && pDesc->pbyFrame) OsFree(pDesc->pbyFrame);
    return dw;
}

static EC_T_DWORD EC_FNCALL Atl2_RecvFrame(EC_T_VOID* inst, EC_T_LINK_FRAMEDESC* pDesc)
{
    Atl2Priv* priv = (Atl2Priv*)inst;
    if (!priv || priv->fd < 0 || !pDesc || !pDesc->pbyFrame) return EC_E_INVALIDPARM;
    struct { unsigned long user_ptr; unsigned int len; } io;
    io.user_ptr = (unsigned long)pDesc->pbyFrame;
    io.len = pDesc->dwSize;
    int rc = ioctl(priv->fd, _IOR(0xEA, 12, typeof(io)), &io);
    if (rc != 0) return EC_E_NOTFOUND;
    pDesc->dwSize = io.len;
    return EC_E_NOERROR;
}

static EC_T_DWORD EC_FNCALL Atl2_AllocSend(EC_T_VOID* inst, EC_T_LINK_FRAMEDESC* pDesc, EC_T_DWORD dwSize)
{
    EC_UNREFPARM(inst);
    if (!pDesc) return EC_E_INVALIDPARM;
    pDesc->dwSize = dwSize ? dwSize : 1518;
    pDesc->pbyFrame = (EC_T_BYTE*)OsMalloc(pDesc->dwSize);
    /* EC-Master uses dwSize as buffer size; actual length is set by sender before send */
    return pDesc->pbyFrame ? EC_E_NOERROR : EC_E_NOMEMORY;
}

static EC_T_VOID  EC_FNCALL Atl2_FreeSend(EC_T_VOID* inst, EC_T_LINK_FRAMEDESC* pDesc)
{
    EC_UNREFPARM(inst);
    if (pDesc && pDesc->pbyFrame) OsFree(pDesc->pbyFrame);
}

static EC_T_VOID  EC_FNCALL Atl2_FreeRecv(EC_T_VOID* inst, EC_T_LINK_FRAMEDESC* pDesc)
{
    EC_UNREFPARM(inst); EC_UNREFPARM(pDesc);
}

static EC_T_DWORD EC_FNCALL Atl2_GetMac(EC_T_VOID* inst, EC_T_BYTE* pMac)
{
    EC_UNREFPARM(inst);
    if (!pMac) return EC_E_INVALIDPARM;
    /* TODO: query mac via ioctl; temporary random */
    pMac[0]=0x02; pMac[1]=0xA0; pMac[2]=0xC1; pMac[3]=0x13; pMac[4]=0x00; pMac[5]=0x01;
    return EC_E_NOERROR;
}

static EC_T_LINKSTATUS EC_FNCALL Atl2_GetStatus(EC_T_VOID* inst)
{ EC_UNREFPARM(inst); return eLinkStatus_OK; }

static EC_T_DWORD EC_FNCALL Atl2_GetSpeed(EC_T_VOID* inst)
{ EC_UNREFPARM(inst); return 10000; }

ATEMLL_API EC_T_DWORD emllRegisterATL2(EC_T_LINK_DRV_DESC* p, EC_T_DWORD sz)
{
    if (!p || sz < sizeof(EC_T_LINK_DRV_DESC)) return EC_E_INVALIDPARM;
    p->dwValidationPattern = LINK_LAYER_DRV_DESC_PATTERN;
    p->dwInterfaceVersion  = LINK_LAYER_DRV_DESC_VERSION;
    p->pfEcLinkOpen            = Atl2_Open;
    p->pfEcLinkClose           = Atl2_Close;
    p->pfEcLinkSendFrame       = Atl2_SendFrame;
    p->pfEcLinkSendAndFreeFrame= Atl2_SendAndFree;
    p->pfEcLinkRecvFrame       = Atl2_RecvFrame;
    p->pfEcLinkAllocSendFrame  = Atl2_AllocSend;
    p->pfEcLinkFreeSendFrame   = Atl2_FreeSend;
    p->pfEcLinkFreeRecvFrame   = Atl2_FreeRecv;
    p->pfEcLinkGetEthernetAddress = Atl2_GetMac;
    p->pfEcLinkGetStatus       = (PF_EcLinkGetStatus)Atl2_GetStatus;
    p->pfEcLinkGetSpeed        = (PF_EcLinkGetSpeed)Atl2_GetSpeed;
    p->pfEcLinkGetMode         = Atl2_GetMode;
    p->pfEcLinkIoctl           = Atl2_Ioctl;
    p->pvLinkInstance          = EC_NULL;
    return EC_E_NOERROR;
}


