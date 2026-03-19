/*
 * drivers/staging/android/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#define CC_ION_TO_IOMMU
#ifdef CC_ION_TO_IOMMU
#include <linux/platform_device.h>
#endif
#include <linux/mtk_iommu.h>
#include "ion.h"
#include "ion_priv.h"


#ifdef CC_ION_TO_IOMMU
#define IONMSG(string, args...) pr_err("[ION]"string, ##args)

#define M4UMSG(string, args...) pr_err("[M4U] "string, ##args)
#define M4UERR(string, args...) do {\
            pr_err("[M4U] error:"string, ##args);  \
} while (0)

typedef int M4U_MODULE_ID_ENUM;

typedef struct {
	int eModuleID;
	unsigned int security;
	unsigned int coherent;
	void *pVA;
	unsigned int MVA;
	//ion_mm_buf_debug_info_t dbg_info;
	//ion_mm_sf_buf_info_t sf_buf_info;
	//ion_mm_buf_destroy_callback_t *destroy_fn;
	struct sg_table *table;
} ion_mm_buffer_info;

typedef struct {
	struct list_head link;
	unsigned int bufAddr;
	unsigned int mvaStart;
	unsigned int size;
	M4U_MODULE_ID_ENUM eModuleId;
	unsigned int flags;
	int security;
	int cache_coherent;
	unsigned int mapped_kernel_va_for_debug;
} mva_info_t;

/* we use this for trace the mva<-->sg_table relation ship */
struct mva_sglist {
        struct list_head list;
        unsigned int mva;
        struct sg_table *table;
};

LIST_HEAD(pseudo_sglist);
/* this is the mutex lock to protect mva_sglist->list*/
static DEFINE_MUTEX(pseudo_list_mutex);

#endif

static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_RECLAIM;
static gfp_t low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN);
#ifdef CONFIG_MACH_MT5863
#define CC_KEEP_ION_HEAP_PAGE_ORDER_TO_0
#endif
#ifdef CC_KEEP_ION_HEAP_PAGE_ORDER_TO_0
static const unsigned int orders[] = {0};
#define NUM_ORDERS 1
#else
static const unsigned int orders[] = {4, 2, 0};
#define NUM_ORDERS 3
#endif
static const int num_orders = ARRAY_SIZE(orders);
static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static inline unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool *uncached_pools[NUM_ORDERS];
	struct ion_page_pool *cached_pools[NUM_ORDERS];
};

/**
 * The page from page-pool are all zeroed before. We need do cache
 * clean for cached buffer. The uncached buffer are always non-cached
 * since it's allocated. So no need for non-cached pages.
 */
static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order)
{
	bool cached = ion_buffer_cached(buffer);
	struct ion_page_pool *pool;
	struct page *page;

	if (!cached)
		pool = heap->uncached_pools[order_to_index(order)];
	else
		pool = heap->cached_pools[order_to_index(order)];

	page = ion_page_pool_alloc(pool);
	if (!page)
		return NULL;

	if (cached)
		ion_pages_sync_for_device(g_ion_device->dev.this_device, page, PAGE_SIZE << order,
					  DMA_BIDIRECTIONAL);
	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page)
{
	struct ion_page_pool *pool;
	unsigned int order = compound_order(page);
	bool cached = ion_buffer_cached(buffer);

	/* go to system */
	if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) {
		__free_pages(page, order);
		return;
	}

	if (!cached)
		pool = heap->uncached_pools[order_to_index(order)];
	else
		pool = heap->cached_pools[order_to_index(order)];

	ion_page_pool_free(pool, page);
}


static struct page *alloc_largest_available(struct ion_system_heap *heap,
					    struct ion_buffer *buffer,
					    unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i]);
		if (!page)
			continue;

		return page;
	}

	return NULL;
}

static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
#ifdef CC_ION_TO_IOMMU
	ion_mm_buffer_info *pBufferInfo = NULL;
#endif

	if (align > PAGE_SIZE)
		return -EINVAL;

	if (size / PAGE_SIZE > totalram_pages / 2)
		return -ENOMEM;

	INIT_LIST_HEAD(&pages);
	while (size_remaining > 0) {
		page = alloc_largest_available(sys_heap, buffer, size_remaining,
						max_order);
		if (!page)
			goto free_pages;
		list_add_tail(&page->lru, &pages);
		size_remaining -= PAGE_SIZE << compound_order(page);
		max_order = compound_order(page);
		i++;
	}
	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto free_pages;

	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_table;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, PAGE_SIZE << compound_order(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}
    buffer->sg_table=table;
    
#ifdef CC_ION_TO_IOMMU
	/* create MM buffer info for it */
	pBufferInfo = kzalloc(sizeof(ion_mm_buffer_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pBufferInfo)) {
		IONMSG("[ion_mm_heap_allocate]: Error. Allocate ion_buffer failed.\n");
		goto free_table;
	}

	pBufferInfo->MVA = 0;
	pBufferInfo->table = table;

	buffer->priv_virt = pBufferInfo;
#else
	buffer->priv_virt = table;
#endif
	return 0;

free_table:
	kfree(table);
free_pages:
	list_for_each_entry_safe(page, tmp_page, &pages, lru)
		free_buffer_page(sys_heap, buffer, page);
	return -ENOMEM;
}

#ifdef CC_ION_TO_IOMMU
int m4u_dealloc_mva_sg(M4U_MODULE_ID_ENUM eModuleID,
		       struct sg_table *sg_table,
		       const unsigned int BufSize, const unsigned int MVA);

void ion_mm_heap_free_bufferInfo(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	ion_mm_buffer_info *pBufferInfo = (ion_mm_buffer_info *) buffer->priv_virt;

	buffer->priv_virt = NULL;

	if (pBufferInfo) {
#if 0
		if ((pBufferInfo->destroy_fn) && (pBufferInfo->MVA))
			pBufferInfo->destroy_fn(buffer, pBufferInfo->MVA);
#endif

		if ((pBufferInfo->MVA))
			m4u_dealloc_mva_sg(pBufferInfo->eModuleID, table, buffer->size, pBufferInfo->MVA);

		kfree(pBufferInfo);
	}
}
#endif

static void ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_system_heap *sys_heap = container_of(buffer->heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	int i;

	/* zero the buffer before goto page pool */
	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE))
		ion_heap_buffer_zero(buffer);

#ifdef CC_ION_TO_IOMMU
	ion_mm_heap_free_bufferInfo(buffer);
#endif

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg));
	sg_free_table(table);
	kfree(table);
}

static int ion_system_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
					int nr_to_scan)
{
	struct ion_page_pool *uncached_pool;
	struct ion_page_pool *cached_pool;
	struct ion_system_heap *sys_heap;
	int nr_total = 0;
	int i, nr_freed;
	int only_scan = 0;

	sys_heap = container_of(heap, struct ion_system_heap, heap);

	if (!nr_to_scan)
		only_scan = 1;

	for (i = 0; i < num_orders; i++) {
		uncached_pool = sys_heap->uncached_pools[i];
		cached_pool = sys_heap->cached_pools[i];

		if (only_scan) {
			nr_total += ion_page_pool_shrink(uncached_pool,
							 gfp_mask,
							 nr_to_scan);

			nr_total += ion_page_pool_shrink(cached_pool,
							 gfp_mask,
							 nr_to_scan);
		} else {
			nr_freed = ion_page_pool_shrink(uncached_pool,
							gfp_mask,
							nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
			nr_freed = ion_page_pool_shrink(cached_pool,
							gfp_mask,
							nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
		}
	}
	return nr_total;
}

#ifdef CC_ION_TO_IOMMU
struct sg_table *m4u_find_sgtable(unsigned int mva)
{
	struct mva_sglist *entry;

	mutex_lock(&pseudo_list_mutex);

	list_for_each_entry(entry, &pseudo_sglist, list) {
		if (entry->mva == mva) {
			mutex_unlock(&pseudo_list_mutex);
			return entry->table;
		}
	}

	mutex_unlock(&pseudo_list_mutex);
	return NULL;
}

struct sg_table *m4u_del_sgtable(unsigned int mva)
{
	struct mva_sglist *entry, *tmp;
	struct sg_table *table;

	mutex_lock(&pseudo_list_mutex);
	list_for_each_entry_safe(entry, tmp, &pseudo_sglist, list) {
		if (entry->mva == mva) {
			list_del(&entry->list);
			mutex_unlock(&pseudo_list_mutex);
			table = entry->table;
			kfree(entry);
			return table;
		}
	}
	mutex_unlock(&pseudo_list_mutex);

	return NULL;
}

struct sg_table *m4u_add_sgtable(struct mva_sglist *mva_sg)
{
	struct sg_table *table;

	table = m4u_find_sgtable(mva_sg->mva);
	if (table)
		return table;

	table = mva_sg->table;
	mutex_lock(&pseudo_list_mutex);
	list_add(&mva_sg->list, &pseudo_sglist);
	mutex_unlock(&pseudo_list_mutex);

	return table;
}

/* since the device have been attached, then we get from the dma_ops->map_sg is arm_iommu_map_sg */
static int __m4u_alloc_mva(mva_info_t *pmvainfo, struct sg_table *sg_table)
{
	int ret;
	struct mva_sglist *mva_sg;
	struct sg_table *table = NULL;
#ifdef CONFIG_MTK_M4U    
    struct device *dev=mtk_m4u_getdev(OVL_LARB_ID);//for ovl
#else    
	struct device *dev = MtkIommuGetDevAddr(MTK_IOMMU_DEVICE_GFX);
#endif
    const struct dma_map_ops *dma_ops = get_dma_ops(dev);
    unsigned long size=0;

    bool is_iomem=false;
	DEFINE_DMA_ATTRS(dma_attr);

	if (!dev) {
		pr_err("[ION]the gfx device is null\n");
		return -EINVAL;
	}
	if (!pmvainfo && !sg_table) {
		M4UMSG("pMvaInfo and sg_table are all NULL\n");
		return -EINVAL;
	}

	if (!sg_table) {
		M4UMSG("sg_table are all NULL\n");
		return -EINVAL;
	}

	/* this is for ion mm heap and ion fb heap usage. */
	if (sg_table) {
		struct scatterlist *s = sg_table->sgl, *ng;
		int i;

		table = kzalloc(sizeof(*table), GFP_KERNEL);
		sg_alloc_table(table, sg_table->nents, GFP_KERNEL);
		ng = table->sgl;

		for (i = 0; i < sg_table->nents; i++) {
            size+=s->length;
            if(!pfn_valid(page_to_pfn(sg_page(s))))//iomem
            {
                is_iomem=true;
            }
			sg_set_page(ng, sg_page(s), s->length, 0);
			s = sg_next(s);
			ng = sg_next(ng);
		}
	}

#if 0 // not for ION case, for user-space case
	if (!table) {
		table = pseudo_get_sg(pmvainfo->bufAddr, pmvainfo->size);
		if (!table) {
			M4UMSG("pseudo_get_sg failed\n");
			goto err;
		}
	}
#endif
    if(is_iomem){
        dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC,&dma_attr); 
    }
    #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
        ret = dma_ops->map_sg(dev, table->sgl, table->nents, 0, &dma_attr);    
    #else
        ret = dma_ops->map_sg(dev, table->sgl, table->nents, 0, dma_attr);
    #endif

	if (!ret) {
		M4UMSG("map sg for dma failed\n");
		goto err;
	}

	pmvainfo->mvaStart = sg_dma_address(table->sgl);
	if (pmvainfo->mvaStart == DMA_ERROR_CODE) {
		M4UMSG("map_sg for dma failed, dma addr is DMA_ERROR_CODE\n");
		goto err;
	}

	mva_sg = kzalloc(sizeof(*mva_sg), GFP_KERNEL);
	mva_sg->table = table;
	mva_sg->mva = pmvainfo->mvaStart;

	m4u_add_sgtable(mva_sg);
#ifdef CONFIG_MTK_M4U  
	MtkIommuTlbInvRg(dev,pmvainfo->mvaStart,size);
#endif  
	return 0;
err:
	if (!sg_table && table) {
		kfree(table->sgl);
		kfree(table);
	}

	pmvainfo->mvaStart = 0;
	return -EINVAL;
}

mva_info_t *m4u_alloc_garbage_list(unsigned int mvaStart,
				   unsigned int bufSize,
				   M4U_MODULE_ID_ENUM eModuleID,
				   unsigned int va,
				   unsigned int flags, int security, int cache_coherent)
{
	mva_info_t *pList = NULL;

	pList = kmalloc(sizeof(mva_info_t), GFP_KERNEL);
	if (pList == NULL) {
		M4UERR("m4u_alloc_garbage_list(), pList=0x%p\n", pList);
		return NULL;
	}

	pList->mvaStart = mvaStart;
	pList->size = bufSize;
	pList->eModuleId = eModuleID;
	pList->bufAddr = va;
	pList->flags = flags;
	pList->security = security;
	pList->cache_coherent = cache_coherent;
	return pList;
}

int m4u_alloc_mva_sg(M4U_MODULE_ID_ENUM eModuleID,
		     struct sg_table *sg_table,
		     const unsigned int BufSize,
		     int security, int cache_coherent, unsigned int *pRetMVABuf)
{
	mva_info_t *pMvaInfo = NULL;
	int ret;

	pMvaInfo = m4u_alloc_garbage_list(0, BufSize,
				eModuleID, 0, 0, security, cache_coherent);
	ret = __m4u_alloc_mva(pMvaInfo, sg_table);

	if (ret == 0)
		*pRetMVABuf = pMvaInfo->mvaStart;
	else
		*pRetMVABuf = 0;

	kfree(pMvaInfo);

	return ret;

}

/* the caller should make sure the mva offset have been eliminated. */
int __m4u_dealloc_mva(M4U_MODULE_ID_ENUM eModuleID,
		      const unsigned int BufAddr,
		      const unsigned int BufSize, const unsigned int MVA, struct sg_table *sg_table)
{
	struct sg_table *table = NULL;
#ifdef CONFIG_MTK_M4U    
    struct device *dev=mtk_m4u_getdev(OVL_LARB_ID);//for ovl
#else    
	struct device *dev = MtkIommuGetDevAddr(MTK_IOMMU_DEVICE_GFX);
#endif    

    bool is_iomem=false;
	DEFINE_DMA_ATTRS(dma_attr);

	const struct dma_map_ops *dma_ops = get_dma_ops(dev);

	if(!dev) {
		pr_err("[ION] gfx device is null when free iova\n");
		return -EINVAL;
	}
	if (!sg_table) {
		panic("not support sg_table==NULL in __m4u_dealloc_mva\n");
	}

	if (sg_table) {
		table = m4u_del_sgtable(MVA);
		if (sg_page(table->sgl) != sg_page(sg_table->sgl)) {
			M4UMSG("%s, %d, error, sg have not been added\n", __func__, __LINE__);
			return -EINVAL;
		}
	}

#if 0 // from user space
	if (!table)
		table = m4u_del_sgtable(MVA);
#endif
	 if(!pfn_valid(page_to_pfn(sg_page(sg_table->sgl))))
	{
		is_iomem = true;
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC,&dma_attr);
	}
	if (table)
	{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		dma_ops->unmap_sg(dev, table->sgl, table->nents, 0, &dma_attr);
#else
		dma_ops->unmap_sg(dev, table->sgl, table->nents, 0, dma_attr);
#endif
	}

#if 0
	if (0 != BufAddr) {
		/* from user space */
		if (BufAddr < PAGE_OFFSET) {
			struct vm_area_struct *vma = NULL;

			down_read(&current->mm->mmap_sem);
			vma = find_vma(current->mm, BufAddr);
			if (vma == NULL) {
				M4UMSG("cannot find vma: module=%s, va=0x%x, size=0x%x\n",
				       m4u_get_module_name(eModuleID), BufAddr, BufSize);
				up_read(&current->mm->mmap_sem);
				goto out;
			}
			if ((vma->vm_flags) & VM_PFNMAP) {
				up_read(&current->mm->mmap_sem);
				goto out;
			}
			up_read(&current->mm->mmap_sem);
			if (table)
				m4u_put_sgtable_pages(table);
		}
	}

out:
#endif
	if (table) {
		sg_free_table(table);
		kfree(table);
	}
	return 0;

}

int m4u_dealloc_mva_sg(M4U_MODULE_ID_ENUM eModuleID,
		       struct sg_table *sg_table,
		       const unsigned int BufSize, const unsigned int MVA)
{
	return __m4u_dealloc_mva(eModuleID, 0, BufSize, MVA, sg_table);
}

int ion_mm_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
		ion_phys_addr_t *addr, size_t *len) {
	ion_mm_buffer_info *pBufferInfo = (ion_mm_buffer_info *) buffer->priv_virt;

	if (!pBufferInfo) {
		IONMSG("[ion_mm_heap_phys]: Error. Invalid buffer.\n");
		return -EFAULT; /* Invalid buffer */
	}

	/* Allocate MVA */
	if (pBufferInfo->MVA == 0) {
		int ret = m4u_alloc_mva_sg(pBufferInfo->eModuleID, buffer->sg_table,
				buffer->size, pBufferInfo->security, pBufferInfo->coherent,
				&pBufferInfo->MVA);

		if (ret < 0) {
			pBufferInfo->MVA = 0;
			IONMSG("[ion_mm_heap_phys]: Error. Allocate MVA failed.\n");
			return -EFAULT;
		}
	}
	*(unsigned int *) addr = pBufferInfo->MVA; /* MVA address */
	*len = buffer->size;

	return 0;
}
#endif
static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
//	.map_dma = ion_system_heap_map_dma,
//	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.shrink = ion_system_heap_shrink,
#if 0//def CC_ION_TO_IOMMU
	.phys = ion_mm_heap_phys,
#endif
};

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;
	struct ion_page_pool *pool;

	for (i = 0; i < num_orders; i++) {
		pool = sys_heap->uncached_pools[i];

		seq_printf(s, "%d order %u highmem pages uncached %lu total\n",
			   pool->high_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages uncached %lu total\n",
			   pool->low_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->low_count);
	}

	for (i = 0; i < num_orders; i++) {
		pool = sys_heap->cached_pools[i];

		seq_printf(s, "%d order %u highmem pages cached %lu total\n",
			   pool->high_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages cached %lu total\n",
			   pool->low_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->low_count);
	}
	return 0;
}
static void ion_system_heap_destroy_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < num_orders; i++)
		if (pools[i])
			ion_page_pool_destroy(pools[i]);
}
static int ion_system_heap_create_pools(struct ion_page_pool **pools,
					bool cached)
{
	int i;
	gfp_t gfp_flags = low_order_gfp_flags;

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;

		if (orders[i] > 4)
			gfp_flags = high_order_gfp_flags;

		pool = ion_page_pool_create(gfp_flags, orders[i], cached);
		if (!pool)
			goto err_create_pool;
		pools[i] = pool;
	}
	return 0;

err_create_pool:
	ion_system_heap_destroy_pools(pools);
	return -ENOMEM;
}

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	if (ion_system_heap_create_pools(heap->uncached_pools, false))
		goto free_heap;

	if (ion_system_heap_create_pools(heap->cached_pools, true))
		goto destroy_uncached_pools;

	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;

destroy_uncached_pools:
	ion_system_heap_destroy_pools(heap->uncached_pools);

free_heap:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}


void ion_system_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	for (i = 0; i < num_orders; i++)
		ion_page_pool_destroy(sys_heap->uncached_pools[i]);
		ion_page_pool_destroy(sys_heap->cached_pools[i]);
	kfree(sys_heap);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	int order = get_order(len);
	struct page *page;
	struct sg_table *table;
	unsigned long i;
	int ret;

	if (align > (PAGE_SIZE << order))
		return -EINVAL;

	page = alloc_pages(low_order_gfp_flags | __GFP_NOWARN, order);
	if (!page)
		return -ENOMEM;

	split_page(page, order);

	len = PAGE_ALIGN(len);
	for (i = len >> PAGE_SHIFT; i < (1 << order); i++)
		__free_page(page + i);

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto free_pages;
	}

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_table;

	sg_set_page(table->sgl, page, len, 0);

	buffer->sg_table = table;

	ion_pages_sync_for_device(g_ion_device->dev.this_device, page, len, DMA_BIDIRECTIONAL);

	return 0;

free_table:
	kfree(table);
free_pages:
	for (i = 0; i < len >> PAGE_SHIFT; i++)
		__free_page(page + i);

	return ret;
}

static void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	unsigned long pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;
	unsigned long i;

	for (i = 0; i < pages; i++)
		__free_page(page + i);
	sg_free_table(table);
	kfree(table);
}


static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	//.phys = ion_system_contig_heap_phys,
	//.map_dma = ion_system_contig_heap_map_dma,
	//.unmap_dma = ion_system_contig_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}
