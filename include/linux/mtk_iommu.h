/* SPDX-License-Identifier: LGPL-2.1-only OR BSD-3-Clause */
/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2.1 of the GNU Lesser General Public License
 * ("LGPLv2.1 License") or BSD License.
 *
 * LGPLv2.1 License
 *
 * Copyright(C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html for more
 * details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2019 MediaTek Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/


#ifndef _MTK_IOMMU_H_
#define _MTK_IOMMU_H_
#include <linux/device.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
#include <linux/dma-attrs.h>
#else
#define DEFINE_DMA_ATTRS(dma_attr)	unsigned long dma_attr = 0
#define dma_set_attr(flag, dma_attr)    *(dma_attr) |= (flag)
#endif

/** used for iommu
*/
typedef enum
{
    MTK_IOMMU_DEVICE_GFX = 0,
    MTK_IOMMU_DEVICE_GCPU   = 1,
    MTK_IOMMU_DEVICE_IMGRZ = 2,
    MTK_IOMMU_DEVICE_DMX = 3,
    MTK_IOMMU_DEVICE_DDI = 4,
    MTK_IOMMU_DEVICE_MAX
} MTK_IOMMU_DEVICE_T;

#ifdef CONFIG_MTK_M4U
#define OVL_LARB_ID 5
#if 0
#include <linux/scatterlist.h>
struct m4u_dma_sg_buf {
    struct device *     dev;/*the device attached to*/
    void				*vaddr;/*kernel virtual address*/
    dma_addr_t           iova;/*io virtual address*/
	struct page			**pages;/*physical page frame array*/
	unsigned int			num_pages;/*size of page frame array*/    
	int				offset;/*offset in page*/
	struct sg_table			sg_table;/*sg_table contains iova and physical page*/
	size_t				size;/*iova &user space virtual address size*/
	struct vm_area_struct		*vma;/*internal use*/
    enum dma_data_direction dma_dir;/*dma direction*/
};
extern struct m4u_dma_sg_buf *mtk_m4u_alloc_usr(struct device * dev,unsigned long vaddr,unsigned long size,enum dma_data_direction dir, struct dma_attrs *attrs);
extern void mtk_m4u_free_usr(struct m4u_dma_sg_buf *buf);
#endif
extern struct device * mtk_m4u_getdev(unsigned int larb_id);
#endif
extern void MtkIommuTlbInvRg(struct device * dev,unsigned long iova, size_t size);
#ifdef CONFIG_MTK_IOMMU
extern void MtkIommuSavePgd(void *pgd);
extern dma_addr_t MtkIommuGetPhyPgd(void);
extern void *MtkIommuGetDevAddr(int devIdx);
extern void *MtkIommuMapIova(void * devaddr,dma_addr_t iova,unsigned long size);
extern void MtkIommuUnMapIova(void *vaddr,unsigned long size);
#endif /* CONFIG_MTK_IOMMU */
#endif
