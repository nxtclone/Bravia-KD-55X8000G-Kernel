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
#include <linux/met_drv.h>
#include <linux/mutex.h>

static DEFINE_MUTEX(met_dev_mutex);

#define MET_DEV_MAX		10

//Adding met_ext_dev memory need ensure the room of array is correct
struct metdevice *met_ext_dev2[MET_DEV_MAX+1] =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	(struct metdevice *)0xFFFFFFFF  //Used for lock/unlock status and end item
};

int met_ext_dev_add(struct metdevice *metdev)
{
	int i;

	if (try_module_get(metdev->owner) == 0) {
		//Cannot lock MET device owner
		return -1;
	}

	mutex_lock(&met_dev_mutex);

	if (met_ext_dev2[MET_DEV_MAX] != (struct metdevice *)0xFFFFFFFF) {
		//The met_ext_dev has been locked by met driver 
		mutex_unlock(&met_dev_mutex);
		module_put(metdev->owner);
		return -2;
	}

	for (i=0; i<MET_DEV_MAX; i++) {
		if(met_ext_dev2[i]==NULL) break;
	}

	if (i>=MET_DEV_MAX)	{ //No room for new MET device table pointer
		mutex_unlock(&met_dev_mutex);
		module_put(metdev->owner);
		return -3;
	}

	met_ext_dev2[i] = metdev;

	mutex_unlock(&met_dev_mutex);	
	return 0;
}

int met_ext_dev_del(struct metdevice *metdev)
{
	int i;

	mutex_lock(&met_dev_mutex);

	if (met_ext_dev2[MET_DEV_MAX] != (struct metdevice *)0xFFFFFFFF) {
		//The met_ext_dev has been locked by met driver 
		mutex_unlock(&met_dev_mutex);
		return -1;
	}

	for (i=0; i<MET_DEV_MAX; i++) {
		if(met_ext_dev2[i]==metdev) break;
	}

	if (i>=MET_DEV_MAX)	{ //No such device table pointer found
		mutex_unlock(&met_dev_mutex);
		return -2;
	}
	met_ext_dev2[i] = NULL;

	mutex_unlock(&met_dev_mutex);

	module_put(metdev->owner);
	return 0;
}

int met_ext_dev_lock(int flag)
{
	mutex_lock(&met_dev_mutex);
	if (flag)
		met_ext_dev2[MET_DEV_MAX] = (struct metdevice *)0xFFFFFFF0;
	else
		met_ext_dev2[MET_DEV_MAX] = (struct metdevice *)0xFFFFFFFF;
	mutex_unlock(&met_dev_mutex);
	return MET_DEV_MAX;
}


EXPORT_SYMBOL(met_ext_dev2);
EXPORT_SYMBOL(met_ext_dev_add);
EXPORT_SYMBOL(met_ext_dev_del);
EXPORT_SYMBOL(met_ext_dev_lock);

