/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef __ION_FB_HEAP_H__
#define __ION_FB_HEAP_H__

struct ion_fb_buffer_info {
	struct mutex lock;/*mutex lock on fb buffer*/
	int module_id;
	unsigned int security;
	unsigned int coherent;
	void *VA;
	unsigned int MVA;
	//unsigned int FIXED_MVA;
	unsigned long iova_start;
	unsigned long iova_end;
	ion_phys_addr_t priv_phys;
//	struct ion_mm_buf_debug_info dbg_info;
//	struct ion_mm_sf_buf_info sf_buf_info;
};
struct ion_heap *ion_fb_heap_create(struct ion_platform_heap *);
void ion_fb_heap_destroy(struct ion_heap *);
//from ion_system_heap_iommu.c

typedef int M4U_MODULE_ID_ENUM;
extern int m4u_alloc_mva_sg(M4U_MODULE_ID_ENUM eModuleID, struct sg_table *sg_table, const unsigned int BufSize, int security, int cache_coherent, unsigned int *pRetMVABuf) ;

extern int m4u_dealloc_mva_sg(M4U_MODULE_ID_ENUM eModuleID, struct sg_table *sg_table, const unsigned int BufSize, const unsigned int MVA);
extern int ion_fb_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer, ion_phys_addr_t *addr, size_t *len);
#define ION_CARVEOUT_ALLOCATE_FAIL  -1
extern int ion_drv_create_heap(struct ion_platform_heap *heap_data);
#endif
