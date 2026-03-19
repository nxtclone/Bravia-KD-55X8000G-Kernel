#include <linux/kernel.h>
#include <linux/hw_random.h>
#include <linux/slab.h>

//-----------------------------------------------------------------------------
// function definition for tztrng
//-----------------------------------------------------------------------------
extern u8 TZ_CTL(u32 u4Cmd, u32 u4PAddr, u32 u4Size);
// Constant definitions
#define TZCTL_TEE_TRNG_HW_RANDOM ((u32)0x6111)
#ifndef DCACHE_LINE_SIZE
#define DCACHE_LINE_SIZE 32lu
#endif
#ifndef DCACHE_LINE_SIZE_MSK
#define DCACHE_LINE_SIZE_MSK (DCACHE_LINE_SIZE - 1)
#endif
#ifndef CACHE_ALIGN
#define CACHE_ALIGN(x) (((x) + DCACHE_LINE_SIZE_MSK) & ~DCACHE_LINE_SIZE_MSK)
#endif

static u8 TZ_TRNG_Hw_Random(void *prKernParam, u32 u4ParamSize)
{
	return TZ_CTL(TZCTL_TEE_TRNG_HW_RANDOM, virt_to_phys(prKernParam),
		      CACHE_ALIGN(u4ParamSize));
}
static int trng_read(void *data, size_t max)
{
	u8 *tmp = NULL;
	u8 *tmpAlign = NULL;
	if (max + DCACHE_LINE_SIZE < max) {
		printk(KERN_ERR "Overflow!!\n");
		return 0;
	}
	tmp = kmalloc(max + DCACHE_LINE_SIZE, GFP_KERNEL);
	if (!tmp) {
		printk(KERN_ERR "Unable to kmalloc\n");
		return 0;
	}
	tmpAlign = (u8*)CACHE_ALIGN((u32)tmp);
	if (!TZ_TRNG_Hw_Random(tmpAlign, max)) {
		printk(KERN_ERR "Get TRNG failed.\n");
		max = 0;
		goto out;
	}
	memcpy(data, tmpAlign, max);
out:
	if (tmp)
		kfree(tmp);
	return max;
}

//-----------------------------------------------------------------------------
