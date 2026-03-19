/*
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include "../ion.h"
#include "../ion_priv.h"
#include "mtktv/ion_fb_heap.h"
#include "mtktv/ion_sec_heap.h"
#include "mtktv/mtk_ion.h"

#ifdef CONFIG_MTK_IOMMU
#define CC_ION_TO_IOMMU
#endif

int num_heaps;
struct ion_heap **mtkionheaps;
struct ion_device *g_ion_device;
EXPORT_SYMBOL(g_ion_device);

#ifdef LIMIT_GPU_TO_ChannelX
extern int mt53xx_get_reserve_memory(unsigned int *start, unsigned int *size, unsigned int *ionsize);
#endif

#ifndef ION_CARVEOUT_MEM_BASE
#define ION_CARVEOUT_MEM_BASE 0
#endif

#ifndef ION_CARVEOUT_MEM_SIZE
#define ION_CARVEOUT_MEM_SIZE 0
#endif

extern int ion_mm_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
		ion_phys_addr_t *addr, size_t *len);
long mtktv_ion_ioctl(struct ion_client *client, unsigned int cmd,
		      unsigned long arg)
{
	struct ion_phys_data phys;
	struct ion_handle *handle;
	int ret = 0;

	//printk("mtktv_ion_ioctl: 0x%x, 0x%lx, pid:%d\n", cmd, arg, current->pid);
	if (cmd == 0x80) {
#ifdef CC_ION_TO_IOMMU
		//printk("mtktv_ion_ioctl\n");
		if (copy_from_user(&phys, (void __user *)arg, sizeof(phys)))
			return -EFAULT;
		handle = ion_handle_get_by_id(client, (int)phys.handle);
		//printk("heap type %d\n", handle->buffer->heap->type);

		switch((int)handle->buffer->heap->type)
		{
			case ION_HEAP_TYPE_FB:
				ret = ion_fb_heap_phys(NULL, handle->buffer, (ion_phys_addr_t *)&(phys.addr), (size_t *)&(phys.len));
				//printk("fb heap ioctl\n");
				break;
			default:
				ret = ion_mm_heap_phys(NULL, handle->buffer, (ion_phys_addr_t *)&(phys.addr), (size_t *)&(phys.len));
				//printk("mm heap ioctl\n");
				break;
		}

		if (!IS_ERR(handle))
			ion_handle_put(handle);

		if (ret < 0)
		    return ret;

		if (copy_to_user((void __user *)arg, &phys, sizeof(phys)))
			return -EFAULT;
#else
		printk("ION to IOMMU is off\n");
#endif
	}

	return 0;
}
extern int ion_drv_create_FB_heap(ion_phys_addr_t fb_base, size_t fb_size);
extern unsigned long mt53xx_fb_start;
extern unsigned long mt53xx_fb_size;
struct ion_heap *ion_mtk_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
	case ION_HEAP_TYPE_FB:
		heap = ion_fb_heap_create(heap_data);
		break;
    case ION_HEAP_TYPE_MULTIMEDIA_SEC:
		heap = ion_sec_heap_create(heap_data);
		break;

	default:
		heap = ion_heap_create(heap_data);
	}
	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %lu size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	return heap;

}
int ion_drv_create_heap(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap;

	heap = ion_mtk_heap_create(heap_data);

	if (IS_ERR_OR_NULL(heap)) {
		printk("%s: %d heap is err or null.\n", __func__, heap_data->id);
		return PTR_ERR(heap);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	ion_device_add_heap(g_ion_device, heap);

	printk("%s: create heap: %s\n", __func__, heap->name);
	return 0;
}

int mtktv_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int err;
	int i;
	unsigned int addr =0;
	unsigned int ionsize =0;

	num_heaps = pdata->nr;

	mtkionheaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);

	printk(KERN_ALERT "ion_device_create  num_heaps = %d \n", num_heaps);
#ifdef LIMIT_GPU_TO_ChannelX
	if(0 == mt53xx_get_reserve_memory(&addr, &size, &ionsize))
	{
	   printk("ion using memory addr=0x%x, ionsize=0x%x\n", addr, ionsize);
	}
#endif

	g_ion_device = ion_device_create(&mtktv_ion_ioctl);
	if (IS_ERR_OR_NULL(g_ion_device)) {
		kfree(mtkionheaps);
		return PTR_ERR(g_ion_device);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
		if(heap_data->type == ION_HEAP_TYPE_CARVEOUT)
		{
		  heap_data->base = addr;
		  heap_data->size = ionsize;
		     if(ionsize == 0) 
		 	{
		 	  heap_data->base =0;
			  continue;
		     	}
		 
		}
		
		mtkionheaps[i] = ion_mtk_heap_create(heap_data);
		if (IS_ERR_OR_NULL(mtkionheaps[i])) {
			err = PTR_ERR(mtkionheaps[i]);
			goto err;
		}
		ion_device_add_heap(g_ion_device, mtkionheaps[i]);
	}
	ion_drv_create_FB_heap(mt53xx_fb_start, mt53xx_fb_size);
	platform_set_drvdata(pdev, g_ion_device);
	g_ion_device->dev.this_device->archdata.dma_ops = NULL;
	arch_setup_dma_ops(g_ion_device->dev.this_device, 0, 0, NULL, false);
	return 0;
err:
	for (i = 0; i < num_heaps; i++) {
		if (mtkionheaps[i])
			ion_heap_destroy(mtkionheaps[i]);
	}
	kfree(mtkionheaps);
	return err;
}

int mtktv_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(mtkionheaps[i]);
	kfree(mtkionheaps);
	return 0;
}


struct ion_device *get_ion_device(void)
{
    return g_ion_device;
}

EXPORT_SYMBOL(get_ion_device);


static struct platform_driver ion_driver = {
	.probe = mtktv_ion_probe,
	.remove = mtktv_ion_remove,
	.driver = { .name = "ion-mtktv" }
};

static struct ion_platform_heap gsHeap[] =
{
	{
		.type = ION_HEAP_TYPE_SYSTEM_CONTIG,
		.name = "system_contig",
		.id   = 2,
	},
	{
		.type = ION_HEAP_TYPE_SYSTEM,
		.name = "system",
		.id   = ION_HEAP_TYPE_SYSTEM,
	},
#if 0
	{
		.type = ION_HEAP_TYPE_CARVEOUT,
		.name = "carveout",
		.id   = 1,
		.base = ION_CARVEOUT_MEM_BASE,
		.size = ION_CARVEOUT_MEM_SIZE,
	},
#endif
#if 0
	{
			.type = ION_HEAP_TYPE_MULTIMEDIA_SEC,
			.id = ION_HEAP_TYPE_MULTIMEDIA_SEC,
			.name = "ion_sec_heap",
			.base = 0,
			.size = 0,
			.align = 0,
			.priv = NULL,
	},
#endif

};


static struct ion_platform_data gsGenericConfig =
{
	.nr = ARRAY_SIZE(gsHeap),
	.heaps = gsHeap,
};

static struct platform_device gsIonDevice = 
{
    .name = "ion-mtktv",
	.id = 0,
	.num_resources = 0,
	.dev = {
	    .platform_data = &gsGenericConfig,
	}
};

static int __init ion_init(void)
{
	int ret;

	ret = platform_driver_register(&ion_driver);
	
		if (!ret) {
			ret = platform_device_register(&gsIonDevice);
			if (ret)
				platform_driver_unregister(&ion_driver);
		}
		return ret;
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
	platform_device_unregister(&gsIonDevice);
}

module_init(ion_init);
module_exit(ion_exit);

