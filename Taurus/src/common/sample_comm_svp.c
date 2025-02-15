/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "hi_common.h"
#include "hi_comm_video.h"
#include "hi_comm_sys.h"
#include "hi_comm_svp.h"
#include "mpi_sys.h"
#include "mpi_vb.h"
#include "sample_comm_svp.h"

static HI_BOOL s_bSampleSvpInit = HI_FALSE;

static HI_S32 SAMPLE_COMM_SVP_SysInit(HI_VOID)
{
    HI_S32 s32Ret = HI_FAILURE;
    VB_CONFIG_S struVbConf;

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    (HI_VOID)memset_s(&struVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));

    struVbConf.u32MaxPoolCnt = 2; /* max pool cnt: 2 */
    struVbConf.astCommPool[1].u64BlkSize = 768 * 576 * 2; /* vb block size: 768 * 576 * 2 */
    struVbConf.astCommPool[1].u32BlkCnt = 1;

    s32Ret = HI_MPI_VB_SetConfig((const VB_CONFIG_S *)&struVbConf);
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x):HI_MPI_VB_SetConf failed!\n", s32Ret);

    s32Ret = HI_MPI_VB_Init();
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x):HI_MPI_VB_Init failed!\n", s32Ret);

    s32Ret = HI_MPI_SYS_Init();
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x):HI_MPI_SYS_Init failed!\n", s32Ret);

    return s32Ret;
}

static HI_S32 SAMPLE_COMM_SVP_SysExit(HI_VOID)
{
    HI_S32 s32Ret = HI_FAILURE;

    s32Ret = HI_MPI_SYS_Exit();
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x):HI_MPI_SYS_Exit failed!\n", s32Ret);

    s32Ret = HI_MPI_VB_Exit();
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x):HI_MPI_VB_Exit failed!\n", s32Ret);

    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_SVP_CheckSysInit(HI_VOID)
{
    if (HI_FALSE == s_bSampleSvpInit) {
        if (SAMPLE_COMM_SVP_SysInit()) {
            SAMPLE_SVP_TRACE(SAMPLE_SVP_ERR_LEVEL_ERROR, "Svp mpi init failed!\n");
            return HI_FAILURE;
        }
        s_bSampleSvpInit = HI_TRUE;
    }

    SAMPLE_SVP_TRACE(SAMPLE_SVP_ERR_LEVEL_DEBUG, "Svp mpi init ok!\n");
    return HI_SUCCESS;
}

HI_VOID SAMPLE_COMM_SVP_CheckSysExit(HI_VOID)
{
    if (s_bSampleSvpInit) {
        SAMPLE_COMM_SVP_SysExit();
        s_bSampleSvpInit = HI_FALSE;
    }

    SAMPLE_SVP_TRACE(SAMPLE_SVP_ERR_LEVEL_DEBUG, "Svp mpi exit ok!\n");
}

HI_U32 SAMPLE_COMM_SVP_Align(HI_U32 u32Size, HI_U16 u16Align)
{
    if (u16Align == 0) {
        printf("Divisor u16Align cannot be 0!\n");
        return HI_FAILURE;
    }
    HI_U32 u32Stride = u32Size + (u16Align - u32Size % u16Align) % u16Align;
    return u32Stride;
}

HI_S32 SAMPLE_COMM_SVP_CreateImage(SVP_IMAGE_S *pstImg, SVP_IMAGE_TYPE_E enType, HI_U32 u32Width, HI_U32 u32Height,
    HI_U32 u32AddrOffset)
{
    HI_U32 u32Size = 0;
    HI_S32 s32Ret = HI_FAILURE;

    SAMPLE_SVP_CHECK_EXPR_RET(pstImg == HI_NULL, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR, "Error(%#x):pstImg is NULL!\n",
        s32Ret);

    pstImg->enType = enType;
    pstImg->u32Width = u32Width;
    pstImg->u32Height = u32Height;
    pstImg->au32Stride[0] = SAMPLE_COMM_SVP_Align(pstImg->u32Width, SAMPLE_SVP_ALIGN_16);

    switch (enType) {
        case SVP_IMAGE_TYPE_U8C1:
        case SVP_IMAGE_TYPE_S8C1: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height + u32AddrOffset;
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);
            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            break;
        }
        case SVP_IMAGE_TYPE_YUV420SP: {
            u32Size =
                pstImg->au32Stride[0] * pstImg->u32Height * 3 / 2 + u32AddrOffset; /* YUV420SP: stride * h * 3 / 2 */
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);
            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            pstImg->au32Stride[1] = pstImg->au32Stride[0];
            pstImg->au64PhyAddr[1] = pstImg->au64PhyAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height;
            pstImg->au64VirAddr[1] = pstImg->au64VirAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height;
            break;
        }
        case SVP_IMAGE_TYPE_YUV422SP: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height * 2 + u32AddrOffset; /* YUV422: stride * h * 2 */
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);
            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            pstImg->au32Stride[1] = pstImg->au32Stride[0];
            pstImg->au64PhyAddr[1] = pstImg->au64PhyAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height;
            pstImg->au64VirAddr[1] = pstImg->au64VirAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height;
            break;
        }
        case SVP_IMAGE_TYPE_YUV420P: {
            pstImg->au32Stride[1] =
                SAMPLE_COMM_SVP_Align(pstImg->u32Width / 2, SAMPLE_SVP_ALIGN_16); /* [1] u_stride, width / 2 */
            pstImg->au32Stride[2] = pstImg->au32Stride[1]; /* [2] v_stride */

            u32Size =
                pstImg->au32Stride[0] * pstImg->u32Height + pstImg->au32Stride[1] * pstImg->u32Height + u32AddrOffset;
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);

            pstImg->au64PhyAddr[0] += u32AddrOffset; /* [0] y */
            pstImg->au64VirAddr[0] += u32AddrOffset; /* [0] y */
            pstImg->au64PhyAddr[1] = pstImg->au64PhyAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height; /* [1] u */
            pstImg->au64VirAddr[1] = pstImg->au64VirAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height; /* [1] u */
            pstImg->au64PhyAddr[2] = pstImg->au64PhyAddr[1] + pstImg->au32Stride[1] * pstImg->u32Height / 2; /* [2] v */
            pstImg->au64VirAddr[2] = pstImg->au64VirAddr[1] + pstImg->au32Stride[1] * pstImg->u32Height / 2; /* [2] v */
            break;
        }
        case SVP_IMAGE_TYPE_YUV422P: {
            pstImg->au32Stride[1] =
                SAMPLE_COMM_SVP_Align(pstImg->u32Width / 2, SAMPLE_SVP_ALIGN_16); /* [1] u_stride, width / 2 */
            pstImg->au32Stride[2] = pstImg->au32Stride[1]; /* [2] v_stride */
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height + pstImg->au32Stride[1] * pstImg->u32Height * 2 +
                u32AddrOffset;
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);

            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            pstImg->au64PhyAddr[1] = pstImg->au64PhyAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height;
            pstImg->au64VirAddr[1] = pstImg->au64VirAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height;
            pstImg->au64PhyAddr[2] = pstImg->au64PhyAddr[1] + pstImg->au32Stride[1] * pstImg->u32Height; /* [2] v */
            pstImg->au64VirAddr[2] = pstImg->au64VirAddr[1] + pstImg->au32Stride[1] * pstImg->u32Height; /* [2] v */
            break;
        }
        case SVP_IMAGE_TYPE_S8C2_PACKAGE: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height * 2 + u32AddrOffset; /* P422: stride * h * 2 */
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);

            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            pstImg->au32Stride[1] = pstImg->au32Stride[0];
            pstImg->au64VirAddr[1] = pstImg->au64VirAddr[0] + 1;
            pstImg->au64PhyAddr[1] = pstImg->au64PhyAddr[0] + 1;
            break;
        }
        case SVP_IMAGE_TYPE_S8C2_PLANAR: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height * 2 + u32AddrOffset; /* S8C2: stride * h * 2 */
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);

            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            pstImg->au32Stride[1] = pstImg->au32Stride[0];
            pstImg->au64VirAddr[1] = pstImg->au64VirAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height;
            pstImg->au64PhyAddr[1] = pstImg->au64PhyAddr[0] + pstImg->au32Stride[0] * pstImg->u32Height;
            break;
        }
        case SVP_IMAGE_TYPE_S16C1:
        case SVP_IMAGE_TYPE_U16C1: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height * sizeof(HI_U16) + u32AddrOffset;
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);
            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            break;
        }
        case SVP_IMAGE_TYPE_U8C3_PACKAGE: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height * 3 + u32AddrOffset; /* U8C3: stride * h * 3 */
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);

            pstImg->au64PhyAddr[0] += u32AddrOffset; /* [0] y_phy_addr */
            pstImg->au64VirAddr[0] += u32AddrOffset; /* [0] y_vir_addr */
            pstImg->au32Stride[1] = pstImg->au32Stride[0]; /* [1] u_stride */
            pstImg->au32Stride[2] = pstImg->au32Stride[0]; /* [2] v_stride */
            pstImg->au64VirAddr[1] = pstImg->au64VirAddr[0] + 1; /* [1] u_vir_addr */
            pstImg->au64VirAddr[2] = pstImg->au64VirAddr[1] + 1; /* [2] v_vir_addr */
            pstImg->au64PhyAddr[1] = pstImg->au64PhyAddr[0] + 1; /* [1] u_phy_addr */
            pstImg->au64PhyAddr[2] = pstImg->au64PhyAddr[1] + 1; /* [2] v_phy_addr */
            break;
        }
        case SVP_IMAGE_TYPE_U8C3_PLANAR: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height * 3 + u32AddrOffset; /* U8C3: stride * h * 3 */
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);

            pstImg->au64PhyAddr[0] += u32AddrOffset; /* [0] y_phy_addr */
            pstImg->au64VirAddr[0] += u32AddrOffset; /* [0] y_vir_addr */
            pstImg->au32Stride[1] = pstImg->au32Stride[0]; /* [1] u_stride */
            pstImg->au32Stride[2] = pstImg->au32Stride[0]; /* [2] v_stride */
            pstImg->au64VirAddr[1] = /* [1] u_vir_addr */
                pstImg->au64VirAddr[0] + (HI_U64)pstImg->au32Stride[0] * (HI_U64)pstImg->u32Height;
            pstImg->au64VirAddr[2] = /* [2] v_vir_addr */
                pstImg->au64VirAddr[1] + (HI_U64)pstImg->au32Stride[1] * (HI_U64)pstImg->u32Height;
            pstImg->au64PhyAddr[1] = /* [1] u_phy_addr */
                pstImg->au64PhyAddr[0] + (HI_U64)pstImg->au32Stride[0] * (HI_U64)pstImg->u32Height;
            pstImg->au64PhyAddr[2] = /* [2] v_phy_addr */
                pstImg->au64PhyAddr[1] + (HI_U64)pstImg->au32Stride[1] * (HI_U64)pstImg->u32Height;
            break;
        }
        case SVP_IMAGE_TYPE_S32C1:
        case SVP_IMAGE_TYPE_U32C1: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height * sizeof(HI_U32) + u32AddrOffset;
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);
            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            break;
        }
        case SVP_IMAGE_TYPE_S64C1:
        case SVP_IMAGE_TYPE_U64C1: {
            u32Size = pstImg->au32Stride[0] * pstImg->u32Height * sizeof(HI_U64) + u32AddrOffset;
            s32Ret =
                HI_MPI_SYS_MmzAlloc(&pstImg->au64PhyAddr[0], (void **)&pstImg->au64VirAddr[0], NULL, HI_NULL, u32Size);
            SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);

            pstImg->au64PhyAddr[0] += u32AddrOffset;
            pstImg->au64VirAddr[0] += u32AddrOffset;
            break;
        }
        default:
            break;
    }

    return HI_SUCCESS;
}

HI_VOID SAMPLE_COMM_SVP_DestroyImage(SVP_IMAGE_S *pstImg, HI_U32 u32AddrOffset)
{
    if (pstImg != NULL) {
        if ((pstImg->au64VirAddr[0] != 0) && (pstImg->au64PhyAddr[0] != 0)) {
            (HI_VOID)HI_MPI_SYS_MmzFree(pstImg->au64PhyAddr[0] - u32AddrOffset,
                (void *)(HI_UINTPTR_T)(pstImg->au64VirAddr[0] - u32AddrOffset));
        }

        (HI_VOID)memset_s(pstImg, sizeof(*pstImg), 0, sizeof(*pstImg));
    }
}

HI_S32 SAMPLE_COMM_SVP_CreateMemInfo(SVP_MEM_INFO_S *pstMemInfo, HI_U32 u32Size, HI_U32 u32AddrOffset)
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_U32 u32SizeTmp;

    SAMPLE_SVP_CHECK_EXPR_RET(pstMemInfo == HI_NULL, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x):pstMemInfo is NULL!\n", s32Ret);

    u32SizeTmp = u32Size + u32AddrOffset;
    pstMemInfo->u32Size = u32Size;
    s32Ret = HI_MPI_SYS_MmzAlloc(&pstMemInfo->u64PhyAddr, (void **)&pstMemInfo->u64VirAddr, NULL, HI_NULL, u32SizeTmp);
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x):HI_MPI_SYS_MmzAlloc failed!\n", s32Ret);

    pstMemInfo->u64PhyAddr += u32AddrOffset;
    pstMemInfo->u64VirAddr += u32AddrOffset;

    return s32Ret;
}

HI_VOID SAMPLE_COMM_SVP_DestroyMemInfo(SVP_MEM_INFO_S *pstMemInfo, HI_U32 u32AddrOffset)
{
    if (pstMemInfo != NULL) {
        if ((pstMemInfo->u64VirAddr != 0) && (pstMemInfo->u64PhyAddr != 0)) {
            (HI_VOID)HI_MPI_SYS_MmzFree(pstMemInfo->u64PhyAddr - u32AddrOffset,
                (HI_VOID *)(HI_UINTPTR_T)(pstMemInfo->u64VirAddr - u32AddrOffset));
        }
        (HI_VOID)memset_s(pstMemInfo, sizeof(*pstMemInfo), 0, sizeof(*pstMemInfo));
    }
}

HI_S32 SAMPLE_COMM_SVP_MallocMem(HI_CHAR *pszMmb, HI_CHAR *pszZone, HI_U64 *pu64PhyAddr, HI_VOID **ppvVirAddr,
    HI_U32 u32Size)
{
    return HI_MPI_SYS_MmzAlloc(pu64PhyAddr, ppvVirAddr, pszMmb, pszZone, u32Size);
}

HI_S32 SAMPLE_COMM_SVP_MallocCached(HI_CHAR *pszMmb, HI_CHAR *pszZone, HI_U64 *pu64PhyAddr, HI_VOID **ppvVirAddr,
    HI_U32 u32Size)
{
    return HI_MPI_SYS_MmzAlloc(pu64PhyAddr, ppvVirAddr, pszMmb, pszZone, u32Size);
}

HI_S32 SAMPLE_COMM_SVP_FlushCache(HI_U64 u64PhyAddr, HI_VOID *pvVirAddr, HI_U32 u32Size)
{
    return HI_MPI_SYS_MmzFlushCache(u64PhyAddr, pvVirAddr, u32Size);
}
