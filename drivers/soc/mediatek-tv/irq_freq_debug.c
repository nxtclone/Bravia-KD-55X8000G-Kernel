/*
 * linux/arch/arm/mach-mt53xx/irq_freq_debug.c
 *
 * debug irq frequency
 *
 * Copyright (c) 2010-2012 MediaTek Inc.
 * $Author$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 */
#define pr_fmt(fmt) "irq_freq: " fmt
#include <linux/tracepoint.h>
#include <linux/interrupt.h>
#include <linux/irqnr.h>
#include <trace/events/irq.h>
#include <linux/percpu.h>
#include <linux/kernel_stat.h>


struct irq_freq_info
{
    ktime_t time;
    u16     times;
};
static unsigned long time_thresh_in_us=0;//time threshold in us.
static u16 times_thresh=0;//times threshold to check irq interval
static int ori_nr_irqs;

static void _irq_handler_probe(void *data, int irq, struct irqaction *action)
{
    struct irq_freq_info *irq_info;
    ktime_t k_tm;
    unsigned long irq_delta_us;
    
    if(action  && (action->flags & IRQF_PERCPU))//do not check ppi
    {
        return;
    }
    if(irq>=ori_nr_irqs)
    {
        WARN_ONCE(irq>=ori_nr_irqs,"nr_irqs is enlarged from %d to %d,can not trace\n",ori_nr_irqs,nr_irqs);
        return;
    }
    irq_info=this_cpu_ptr(data);
    if(irq_info[irq].times==times_thresh)
    {
        k_tm=ktime_get();
        if(ktime_to_ns(irq_info[irq].time)>0)
        {
            irq_delta_us=ktime_to_us(ktime_sub(k_tm,irq_info[irq].time));
            if(irq_delta_us<time_thresh_in_us)
            {
                pr_warn("virq %d at cpu %d reach %d times in %ld us,handler %pS count=%d\n",irq,
                raw_smp_processor_id(),times_thresh,irq_delta_us,action->handler,kstat_irqs_cpu(irq,raw_smp_processor_id()));
            }
        }
        irq_info[irq].time=k_tm;
        irq_info[irq].times=0;  
    }
    irq_info[irq].times++;
}
static int _enable_trace_irq_handler_entry(void)
{
    struct irq_freq_info *irq_info;
    ori_nr_irqs=nr_irqs;
    irq_info=__alloc_percpu(sizeof(struct irq_freq_info)*nr_irqs,__alignof__(struct irq_freq_info));
    if(!irq_info)return -ENOMEM;
    return register_trace_irq_handler_entry(_irq_handler_probe,irq_info);
}
static __init int irq_debug_init(void)
{
    int ret=0;
    if(time_thresh_in_us && times_thresh)
    {
        ret=_enable_trace_irq_handler_entry();
        if(ret)
        {
            pr_err("%s fail.ret=%d\n",__func__,ret);
        }
        else
        {
            pr_info("successful to enable trace_irq_handler_entry\n");
            pr_info("irqfreq=%d@%ldus\n", times_thresh,time_thresh_in_us);   
        }
        return ret;
    }
    return 0;
}
core_initcall(irq_debug_init);

static int __init setup_irq_freq(char *str)
{
    char *token;
    unsigned long data;
    u8 index=0;
    if (*str++ != '=' || !*str)
        return -EINVAL;
    while (true) {
        token = strsep(&str, ",");

        if (!token)
            break;

        if (*token) {
            if(kstrtoul(token,0,&data))
               return -EINVAL;
            if(index==0)
            {
                times_thresh=(u16)data;
            }
            else if(index==1)
            {
                time_thresh_in_us=data*USEC_PER_MSEC;//input is ms
            }
            else
            {
                break;
            }
            index++;
        }
        if (str)
            *(str - 1) = ',';
    }
    return 1;
}
/*to check the time cost in certain counts of interrupts(by times_thresh),
if the time <time_thresh_in_us,it will print log
The format is times_threshold,time_threshold_in_ms
*/
__setup("irqfreq", setup_irq_freq);


