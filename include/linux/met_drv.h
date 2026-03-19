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

#ifndef MET_DRV
#define MET_DRV

#include <linux/device.h>

#define MET_TYPE_PMU	1
#define MET_TYPE_BUS	2
#define MET_TYPE_MISC	3

struct metdevice {

	struct list_head list;
	int type;
	const char *name;
	struct module *owner;
	struct kobject *kobj;

	int (*create_subfs) (struct kobject *parent);
	void (*delete_subfs) (void);
	int mode;
	int cpu_related;
	int polling_interval;
	int polling_count_reload;
	int __percpu *polling_count;
	void (*start) (void);
	void (*stop) (void);
	void (*timed_polling) (unsigned long long stamp, int cpu);
	void (*tagged_polling) (unsigned long long stamp, int cpu);
	int (*print_help) (char *buf, int len);
	int (*print_header) (char *buf, int len);
	int (*process_argument) (const char *arg, int len);

	struct list_head exlist;	//for linked list before register
	void *reversed1;
};

int met_register(struct metdevice* met);
int met_deregister(struct metdevice* met);
int met_set_platform(const char *plf_name, int flag);
int met_set_topology(const char *topology_name, int flag);
int met_devlink_add(struct metdevice* met);
int met_devlink_del(struct metdevice* met);
int met_devlink_register_all(void);
int met_devlink_deregister_all(void);

//====================== Tagging API ================================

#define MAX_EVENT_CLASS	31
#define MAX_TAGNAME_LEN	128
#define MET_CLASS_ALL	0x80000000

//IOCTL commands of MET tagging
typedef struct _mtag_cmd_t
{
	unsigned int class_id;
	unsigned int value;
	unsigned int slen;
	char tname[MAX_TAGNAME_LEN];
	void *data;
	unsigned int size;
} mtag_cmd_t;

#define TYPE_START		1
#define TYPE_END		2
#define TYPE_ONESHOT	3
#define TYPE_ENABLE		4
#define TYPE_DISABLE	5
#define TYPE_REC_SET	6
#define TYPE_DUMP		7
#define TYPE_DUMP_SIZE	8
#define TYPE_DUMP_SAVE	9

/* Use 'm' as magic number */
#define MTAG_IOC_MAGIC  'm'
/* Please use a different 8-bit number in your code */
#define MTAG_CMD_START		_IOW(MTAG_IOC_MAGIC, TYPE_START, mtag_cmd_t)
#define MTAG_CMD_END		_IOW(MTAG_IOC_MAGIC, TYPE_END, mtag_cmd_t)
#define MTAG_CMD_ONESHOT	_IOW(MTAG_IOC_MAGIC, TYPE_ONESHOT, mtag_cmd_t)
#define MTAG_CMD_ENABLE		_IOW(MTAG_IOC_MAGIC, TYPE_ENABLE, int)
#define MTAG_CMD_DISABLE	_IOW(MTAG_IOC_MAGIC, TYPE_DISABLE, int)
#define MTAG_CMD_REC_SET	_IOW(MTAG_IOC_MAGIC, TYPE_REC_SET, int)
#define MTAG_CMD_DUMP		_IOW(MTAG_IOC_MAGIC, TYPE_DUMP, mtag_cmd_t)
#define MTAG_CMD_DUMP_SIZE	_IOWR(MTAG_IOC_MAGIC, TYPE_DUMP_SIZE, int)
#define MTAG_CMD_DUMP_SAVE	_IOW(MTAG_IOC_MAGIC, TYPE_DUMP_SAVE, mtag_cmd_t)

//include file
#ifndef MET_USER_EVENT_SUPPORT
#define met_tag_init() (0)

#define met_tag_uninit() (0)

#define met_tag_start(id, name) (0)

#define met_tag_end(id, name) (0)

#define met_tag_oneshot(id, name, value) (0)

#define met_tag_dump(id, name, data, length) (0)

#define met_tag_disable(id) (0)

#define met_tag_enable(id) (0)

#define met_set_dump_buffer(size) (0)

#define met_save_dump_buffer(pathname) (0)

#define met_save_log(pathname) (0)

#define met_record_on() (0)

#define met_record_off() (0)

#else
#include <linux/kernel.h>
int met_tag_init(void);

int met_tag_uninit(void);

int met_tag_start(unsigned int class_id, const char *name);

int met_tag_end(unsigned int class_id, const char *name);

int met_tag_oneshot(unsigned int class_id, const char *name, unsigned int value);

int met_tag_dump(unsigned int class_id, const char *name, void *data, unsigned int length);

int met_tag_disable(unsigned int class_id);

int met_tag_enable(unsigned int class_id);

int met_set_dump_buffer(int size);

int met_save_dump_buffer(const char *pathname);

int met_save_log(const char *pathname);

#define met_record_on()		tracing_on()

#define met_record_off()	tracing_off()

#endif //MET_USER_EVENT_SUPPORT

#endif //MET_DRV
