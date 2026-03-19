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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtk_thermal.h>

struct met_api_tbl {
	int (*met_tag_start) (unsigned int class_id, const char *name);
	int (*met_tag_end) (unsigned int class_id, const char *name);
	int (*met_tag_oneshot) (unsigned int class_id, const char *name, unsigned int value);
	int (*met_tag_dump) (unsigned int class_id, const char *name, void *data, unsigned int length);
	int (*met_tag_disable) (unsigned int class_id);
	int (*met_tag_enable) (unsigned int class_id);
	int (*met_set_dump_buffer) (int size);
	int (*met_save_dump_buffer) (const char *pathname);
	int (*met_save_log) (const char *pathname);
};

struct met_api_tbl met_ext_api;
EXPORT_SYMBOL(met_ext_api);

int met_tag_start(unsigned int class_id, const char *name)
{
	if (met_ext_api.met_tag_start) {
		return met_ext_api.met_tag_start(class_id, name);
	}
	return 0;
}

int met_tag_end(unsigned int class_id, const char *name)
{
	if (met_ext_api.met_tag_end) {
		return met_ext_api.met_tag_end(class_id, name);
	}
	return 0;
}

int met_tag_oneshot(unsigned int class_id, const char *name, unsigned int value)
{
	//trace_printk("8181888\n");
	if (met_ext_api.met_tag_oneshot) {
		return met_ext_api.met_tag_oneshot(class_id, name, value);
	}
	//else {
	//	trace_printk("7171777\n");
	//}
	return 0;
}

int met_tag_dump(unsigned int class_id, const char *name, void *data, unsigned int length)
{
	if (met_ext_api.met_tag_dump) {
		return met_ext_api.met_tag_dump(class_id, name, data, length);
	}
	return 0;
}

int met_tag_disable(unsigned int class_id)
{
	if (met_ext_api.met_tag_disable) {
		return met_ext_api.met_tag_disable(class_id);
	}
	return 0;
}

int met_tag_enable(unsigned int class_id)
{
	if (met_ext_api.met_tag_enable) {
		return met_ext_api.met_tag_enable(class_id);
	}
	return 0;
}

int met_set_dump_buffer(int size)
{
	if (met_ext_api.met_set_dump_buffer) {
		return met_ext_api.met_set_dump_buffer(size);
	}
	return 0;
}

int met_save_dump_buffer(const char *pathname)
{
	if (met_ext_api.met_save_dump_buffer) {
		return met_ext_api.met_save_dump_buffer(pathname);
	}
	return 0;
}

int met_save_log(const char *pathname)
{
	if (met_ext_api.met_save_log) {
		return met_ext_api.met_save_log(pathname);
	}
	return 0;
}

struct met_thermal_api_tbl met_thermal_ext_api;
EXPORT_SYMBOL(met_thermal_ext_api);

int mtk_thermal_get_temp(MTK_THERMAL_SENSOR_ID id)
{
	if (met_thermal_ext_api.mtk_thermal_get_temp) {
		return met_thermal_ext_api.mtk_thermal_get_temp(id);
	}
	return -127000;
}

EXPORT_SYMBOL(met_tag_start);
EXPORT_SYMBOL(met_tag_end);
EXPORT_SYMBOL(met_tag_oneshot);
EXPORT_SYMBOL(met_tag_dump);
EXPORT_SYMBOL(met_tag_disable);
EXPORT_SYMBOL(met_tag_enable);
EXPORT_SYMBOL(met_set_dump_buffer);
EXPORT_SYMBOL(met_save_dump_buffer);
EXPORT_SYMBOL(met_save_log);
EXPORT_SYMBOL(mtk_thermal_get_temp);


