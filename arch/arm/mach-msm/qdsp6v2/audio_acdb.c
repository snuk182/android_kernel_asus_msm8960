/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/msm_ion.h>
#include <linux/mm.h>
#include <mach/qdsp6v2/audio_acdb.h>
#include <linux/slab.h>

#define MAX_NETWORKS		15
#define MAX_HW_DELAY_ENTRIES	25

struct sidetone_atomic_cal {
	atomic_t	enable;
	atomic_t	gain;
};


struct acdb_data {
	struct mutex		acdb_mutex;

	/* ANC Cal */
	struct acdb_atomic_cal_block	anc_cal;

	/* AudProc Cal */
	atomic_t			asm_topology;
	atomic_t			adm_topology[MAX_AUDPROC_TYPES];
	struct acdb_atomic_cal_block	audproc_cal[MAX_AUDPROC_TYPES];
	struct acdb_atomic_cal_block	audstrm_cal[MAX_AUDPROC_TYPES];
	struct acdb_atomic_cal_block	audvol_cal[MAX_AUDPROC_TYPES];

	/* VocProc Cal */
	atomic_t			voice_rx_topology;
	atomic_t			voice_tx_topology;
	struct acdb_atomic_cal_block	vocproc_cal[MAX_NETWORKS];
	struct acdb_atomic_cal_block	vocstrm_cal[MAX_NETWORKS];
	struct acdb_atomic_cal_block	vocvol_cal[MAX_NETWORKS];
	/* size of cal block tables above*/
	atomic_t			vocproc_cal_size;
	atomic_t			vocstrm_cal_size;
	atomic_t			vocvol_cal_size;
	/* Total size of cal data for all networks */
	atomic_t			vocproc_total_cal_size;
	atomic_t			vocstrm_total_cal_size;
	atomic_t			vocvol_total_cal_size;

	/* AFE cal */
	struct acdb_atomic_cal_block	afe_cal[MAX_AUDPROC_TYPES];

	/* Sidetone Cal */
	struct sidetone_atomic_cal	sidetone_cal;

	/* Allocation information */
	struct ion_client		*ion_client;
	struct ion_handle		*ion_handle;
	atomic_t			map_handle;
	atomic64_t			paddr;
	atomic64_t			kvaddr;
	atomic64_t			mem_len;
	/* Av sync delay info */
	struct hw_delay hw_delay_rx;
	struct hw_delay hw_delay_tx;
};

static struct acdb_data		acdb_data;
static atomic_t usage_count;

uint32_t get_voice_rx_topology(void)
{
	return atomic_read(&acdb_data.voice_rx_topology);
}

void store_voice_rx_topology(uint32_t topology)
{
	atomic_set(&acdb_data.voice_rx_topology, topology);
}

uint32_t get_voice_tx_topology(void)
{
	return atomic_read(&acdb_data.voice_tx_topology);
}

void store_voice_tx_topology(uint32_t topology)
{
	atomic_set(&acdb_data.voice_tx_topology, topology);
}

uint32_t get_adm_rx_topology(void)
{
	return atomic_read(&acdb_data.adm_topology[RX_CAL]);
}

void store_adm_rx_topology(uint32_t topology)
{
	atomic_set(&acdb_data.adm_topology[RX_CAL], topology);
}

uint32_t get_adm_tx_topology(void)
{
	return atomic_read(&acdb_data.adm_topology[TX_CAL]);
}

void store_adm_tx_topology(uint32_t topology)
{
	atomic_set(&acdb_data.adm_topology[TX_CAL], topology);
}

uint32_t get_asm_topology(void)
{
	return atomic_read(&acdb_data.asm_topology);
}

void store_asm_topology(uint32_t topology)
{
	atomic_set(&acdb_data.asm_topology, topology);
}

void get_all_voice_cal(struct acdb_cal_block *cal_block)
{
	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.vocproc_cal[0].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.vocproc_cal[0].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.vocproc_total_cal_size) +
		atomic_read(&acdb_data.vocstrm_total_cal_size) +
		atomic_read(&acdb_data.vocvol_total_cal_size);
}

void get_all_cvp_cal(struct acdb_cal_block *cal_block)
{
	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.vocproc_cal[0].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.vocproc_cal[0].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.vocproc_total_cal_size) +
		atomic_read(&acdb_data.vocvol_total_cal_size);
}

void get_all_vocproc_cal(struct acdb_cal_block *cal_block)
{
	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.vocproc_cal[0].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.vocproc_cal[0].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.vocproc_total_cal_size);
}

void get_all_vocstrm_cal(struct acdb_cal_block *cal_block)
{
	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.vocstrm_cal[0].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.vocstrm_cal[0].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.vocstrm_total_cal_size);
}

void get_all_vocvol_cal(struct acdb_cal_block *cal_block)
{
	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.vocvol_cal[0].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.vocvol_cal[0].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.vocvol_total_cal_size);
}

int get_hw_delay(int32_t path, struct hw_delay_entry *entry)
{
	int i, result = 0;
	struct hw_delay *delay = NULL;
	struct hw_delay_entry *info = NULL;
	pr_debug("%s,\n", __func__);

	if (entry == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		result = -EINVAL;
		goto ret;
	}
	if ((path >= MAX_AUDPROC_TYPES) || (path < 0)) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
		       __func__, path);
		result = -EINVAL;
		goto ret;
	}
	mutex_lock(&acdb_data.acdb_mutex);
	if (path == RX_CAL)
		delay = &acdb_data.hw_delay_rx;
	else if (path == TX_CAL)
		delay = &acdb_data.hw_delay_tx;
	else
		pr_err("ACDB=> %s Invalid path: %d\n",__func__,path);

	if ((delay == NULL) || ((delay != NULL) && delay->num_entries == 0)) {
		pr_debug("ACDB=> %s Invalid delay/ delay entries\n", __func__);
		result = -EINVAL;
		goto done;
	}

	info = (struct hw_delay_entry *)(delay->delay_info);
	if (info == NULL) {
		pr_err("ACDB=> %s Delay entries info is NULL\n", __func__);
		result = -EINVAL;
		goto done;
	}
	for (i = 0; i < delay->num_entries; i++) {
		if (info[i].sample_rate == entry->sample_rate) {
			entry->delay_usec = info[i].delay_usec;
			break;
		}
	}
	if (i == delay->num_entries) {
		pr_err("ACDB=> %s: Unable to find delay for sample rate %d\n",
		       __func__, entry->sample_rate);
		result = -EINVAL;
	}

done:
	mutex_unlock(&acdb_data.acdb_mutex);
ret:
	if (entry)
		pr_debug("ACDB=>%s: Path=%d samplerate=%u usec=%u status %d\n",
			__func__, path, entry->sample_rate,
			entry->delay_usec, result);
	else
		pr_debug("ACDB=> %s: Path=%d entry=%p status %d\n",
			__func__, path, entry, result);
	return result;
}

int store_hw_delay(int32_t path, void *arg)
{
	int result = 0;
	struct hw_delay delay;
	struct hw_delay *delay_dest = NULL;
	pr_debug("%s,\n", __func__);

	if ((path >= MAX_AUDPROC_TYPES) || (path < 0) || (arg == NULL)) {
		pr_err("ACDB=> Bad path/ pointer sent to %s, path: %d\n",
		      __func__, path);
		result = -EINVAL;
		goto done;
	}
	result = copy_from_user((void *)&delay, (void *)arg,
				sizeof(struct hw_delay));
	if (result) {
		pr_err("ACDB=> %s failed to copy hw delay: result=%d path=%d\n",
		       __func__, result, path);
		result = -EFAULT;
		goto done;
	}
	if ((delay.num_entries <= 0) ||
		(delay.num_entries > MAX_HW_DELAY_ENTRIES)) {
		pr_debug("ACDB=> %s incorrect no of hw delay entries: %d\n",
		       __func__, delay.num_entries);
		result = -EINVAL;
		goto done;
	}
	if ((path >= MAX_AUDPROC_TYPES) || (path < 0)) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
		__func__, path);
		result = -EINVAL;
		goto done;
	}

	pr_debug("ACDB=> %s : Path = %d num_entries = %d\n",
		 __func__, path, delay.num_entries);

	mutex_lock(&acdb_data.acdb_mutex);
	if (path == RX_CAL)
		delay_dest = &acdb_data.hw_delay_rx;
	else if (path == TX_CAL)
		delay_dest = &acdb_data.hw_delay_tx;

	delay_dest->num_entries = delay.num_entries;

	result = copy_from_user(delay_dest->delay_info,
				delay.delay_info,
				(sizeof(struct hw_delay_entry)*
				delay.num_entries));
	if (result) {
		pr_err("ACDB=> %s failed to copy hw delay info res=%d path=%d",
		       __func__, result, path);
		result = -EFAULT;
	}
	mutex_unlock(&acdb_data.acdb_mutex);
done:
	return result;
}

void get_anc_cal(struct acdb_cal_block *cal_block)
{
	pr_debug("%s\n", __func__);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}

	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.anc_cal.cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.anc_cal.cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.anc_cal.cal_size);
done:
	return;
}

void store_anc_cal(struct cal_block *cal_block)
{
	pr_debug("%s,\n", __func__);

	if (cal_block->cal_offset > atomic64_read(&acdb_data.mem_len)) {
		pr_err("%s: offset %d is > mem_len %ld\n",
			__func__, cal_block->cal_offset,
			(long)atomic64_read(&acdb_data.mem_len));
		goto done;
	}

	atomic_set(&acdb_data.anc_cal.cal_kvaddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.kvaddr));
	atomic_set(&acdb_data.anc_cal.cal_paddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.paddr));
	atomic_set(&acdb_data.anc_cal.cal_size,
		cal_block->cal_size);
done:
	return;
}

void store_afe_cal(int32_t path, struct cal_block *cal_block)
{
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block->cal_offset > atomic64_read(&acdb_data.mem_len)) {
		pr_err("%s: offset %d is > mem_len %ld\n",
			__func__, cal_block->cal_offset,
			(long)atomic64_read(&acdb_data.mem_len));
		goto done;
	}
	if ((path >= MAX_AUDPROC_TYPES) || (path < 0)) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		goto done;
	}

	atomic_set(&acdb_data.afe_cal[path].cal_kvaddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.kvaddr));
	atomic_set(&acdb_data.afe_cal[path].cal_paddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.paddr));
	atomic_set(&acdb_data.afe_cal[path].cal_size,
		cal_block->cal_size);
done:
	return;
}

void get_afe_cal(int32_t path, struct acdb_cal_block *cal_block)
{
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}
	if ((path >= MAX_AUDPROC_TYPES) || (path < 0)) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		goto done;
	}

	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.afe_cal[path].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.afe_cal[path].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.afe_cal[path].cal_size);
done:
	return;
}

void store_audproc_cal(int32_t path, struct cal_block *cal_block)
{
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block->cal_offset > atomic64_read(&acdb_data.mem_len)) {
		pr_err("%s: offset %d is > mem_len %ld\n",
			__func__, cal_block->cal_offset,
			(long)atomic64_read(&acdb_data.mem_len));
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		goto done;
	}

	atomic_set(&acdb_data.audproc_cal[path].cal_kvaddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.kvaddr));
	atomic_set(&acdb_data.audproc_cal[path].cal_paddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.paddr));
	atomic_set(&acdb_data.audproc_cal[path].cal_size,
		cal_block->cal_size);
done:
	return;
}

void get_audproc_cal(int32_t path, struct acdb_cal_block *cal_block)
{
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		goto done;
	}

	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.audproc_cal[path].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.audproc_cal[path].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.audproc_cal[path].cal_size);
done:
	return;
}

void store_audstrm_cal(int32_t path, struct cal_block *cal_block)
{
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block->cal_offset > atomic64_read(&acdb_data.mem_len)) {
		pr_err("%s: offset %d is > mem_len %ld\n",
			__func__, cal_block->cal_offset,
			(long)atomic64_read(&acdb_data.mem_len));
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		goto done;
	}

	atomic_set(&acdb_data.audstrm_cal[path].cal_kvaddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.kvaddr));
	atomic_set(&acdb_data.audstrm_cal[path].cal_paddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.paddr));
	atomic_set(&acdb_data.audstrm_cal[path].cal_size,
		cal_block->cal_size);
done:
	return;
}

void get_audstrm_cal(int32_t path, struct acdb_cal_block *cal_block)
{
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		goto done;
	}

	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.audstrm_cal[path].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.audstrm_cal[path].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.audstrm_cal[path].cal_size);
done:
	return;
}

void store_audvol_cal(int32_t path, struct cal_block *cal_block)
{
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block->cal_offset > atomic64_read(&acdb_data.mem_len)) {
		pr_err("%s: offset %d is > mem_len %ld\n",
			__func__, cal_block->cal_offset,
			(long)atomic64_read(&acdb_data.mem_len));
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		goto done;
	}

	atomic_set(&acdb_data.audvol_cal[path].cal_kvaddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.kvaddr));
	atomic_set(&acdb_data.audvol_cal[path].cal_paddr,
		cal_block->cal_offset + atomic64_read(&acdb_data.paddr));
	atomic_set(&acdb_data.audvol_cal[path].cal_size,
		cal_block->cal_size);
done:
	return;
}

void get_audvol_cal(int32_t path, struct acdb_cal_block *cal_block)
{
	pr_debug("%s, path = %d\n", __func__, path);

	if (cal_block == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}
	if (path >= MAX_AUDPROC_TYPES || path < 0) {
		pr_err("ACDB=> Bad path sent to %s, path: %d\n",
			__func__, path);
		goto done;
	}

	cal_block->cal_kvaddr =
		atomic_read(&acdb_data.audvol_cal[path].cal_kvaddr);
	cal_block->cal_paddr =
		atomic_read(&acdb_data.audvol_cal[path].cal_paddr);
	cal_block->cal_size =
		atomic_read(&acdb_data.audvol_cal[path].cal_size);
done:
	return;
}


void store_vocproc_cal(int32_t len, struct cal_block *cal_blocks)
{
	int i;
	pr_debug("%s\n", __func__);

	if (len > MAX_NETWORKS) {
		pr_err("%s: Calibration sent for %d networks, only %d are "
			"supported!\n", __func__, len, MAX_NETWORKS);
		goto done;
	}

	atomic_set(&acdb_data.vocproc_total_cal_size, 0);
	for (i = 0; i < len; i++) {
		if (cal_blocks[i].cal_offset >
					atomic64_read(&acdb_data.mem_len)) {
			pr_err("%s: offset %d is > mem_len %ld\n",
				__func__, cal_blocks[i].cal_offset,
				(long)atomic64_read(&acdb_data.mem_len));
			atomic_set(&acdb_data.vocproc_cal[i].cal_size, 0);
		} else {
			atomic_add(cal_blocks[i].cal_size,
				&acdb_data.vocproc_total_cal_size);
			atomic_set(&acdb_data.vocproc_cal[i].cal_size,
				cal_blocks[i].cal_size);
			atomic_set(&acdb_data.vocproc_cal[i].cal_paddr,
				cal_blocks[i].cal_offset +
				atomic64_read(&acdb_data.paddr));
			atomic_set(&acdb_data.vocproc_cal[i].cal_kvaddr,
				cal_blocks[i].cal_offset +
				atomic64_read(&acdb_data.kvaddr));
		}
	}
	atomic_set(&acdb_data.vocproc_cal_size, len);
done:
	return;
}

void get_vocproc_cal(struct acdb_cal_data *cal_data)
{
	pr_debug("%s\n", __func__);

	if (cal_data == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}

	cal_data->num_cal_blocks = atomic_read(&acdb_data.vocproc_cal_size);
	cal_data->cal_blocks = &acdb_data.vocproc_cal[0];
done:
	return;
}

void store_vocstrm_cal(int32_t len, struct cal_block *cal_blocks)
{
	int i;
	pr_debug("%s\n", __func__);

	if (len > MAX_NETWORKS) {
		pr_err("%s: Calibration sent for %d networks, only %d are "
			"supported!\n", __func__, len, MAX_NETWORKS);
		goto done;
	}

	atomic_set(&acdb_data.vocstrm_total_cal_size, 0);
	for (i = 0; i < len; i++) {
		if (cal_blocks[i].cal_offset >
					atomic64_read(&acdb_data.mem_len)) {
			pr_err("%s: offset %d is > mem_len %ld\n",
				__func__, cal_blocks[i].cal_offset,
				(long)atomic64_read(&acdb_data.mem_len));
			atomic_set(&acdb_data.vocstrm_cal[i].cal_size, 0);
		} else {
			atomic_add(cal_blocks[i].cal_size,
				&acdb_data.vocstrm_total_cal_size);
			atomic_set(&acdb_data.vocstrm_cal[i].cal_size,
				cal_blocks[i].cal_size);
			atomic_set(&acdb_data.vocstrm_cal[i].cal_paddr,
				cal_blocks[i].cal_offset +
				atomic64_read(&acdb_data.paddr));
			atomic_set(&acdb_data.vocstrm_cal[i].cal_kvaddr,
				cal_blocks[i].cal_offset +
				atomic64_read(&acdb_data.kvaddr));
		}
	}
	atomic_set(&acdb_data.vocstrm_cal_size, len);
done:
	return;
}

void get_vocstrm_cal(struct acdb_cal_data *cal_data)
{
	pr_debug("%s\n", __func__);

	if (cal_data == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}

	cal_data->num_cal_blocks = atomic_read(&acdb_data.vocstrm_cal_size);
	cal_data->cal_blocks = &acdb_data.vocstrm_cal[0];
done:
	return;
}

void store_vocvol_cal(int32_t len, struct cal_block *cal_blocks)
{
	int i;
	pr_debug("%s\n", __func__);

	if (len > MAX_NETWORKS) {
		pr_err("%s: Calibration sent for %d networks, only %d are "
			"supported!\n", __func__, len, MAX_NETWORKS);
		goto done;
	}

	atomic_set(&acdb_data.vocvol_total_cal_size, 0);
	for (i = 0; i < len; i++) {
		if (cal_blocks[i].cal_offset >
					atomic64_read(&acdb_data.mem_len)) {
			pr_err("%s: offset %d is > mem_len %ld\n",
				__func__, cal_blocks[i].cal_offset,
				(long)atomic64_read(&acdb_data.mem_len));
			atomic_set(&acdb_data.vocvol_cal[i].cal_size, 0);
		} else {
			atomic_add(cal_blocks[i].cal_size,
				&acdb_data.vocvol_total_cal_size);
			atomic_set(&acdb_data.vocvol_cal[i].cal_size,
				cal_blocks[i].cal_size);
			atomic_set(&acdb_data.vocvol_cal[i].cal_paddr,
				cal_blocks[i].cal_offset +
				atomic64_read(&acdb_data.paddr));
			atomic_set(&acdb_data.vocvol_cal[i].cal_kvaddr,
				cal_blocks[i].cal_offset +
				atomic64_read(&acdb_data.kvaddr));
		}
	}
	atomic_set(&acdb_data.vocvol_cal_size, len);
done:
	return;
}

void get_vocvol_cal(struct acdb_cal_data *cal_data)
{
	pr_debug("%s\n", __func__);

	if (cal_data == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}

	cal_data->num_cal_blocks = atomic_read(&acdb_data.vocvol_cal_size);
	cal_data->cal_blocks = &acdb_data.vocvol_cal[0];
done:
	return;
}

void store_sidetone_cal(struct sidetone_cal *cal_data)
{
	pr_debug("%s\n", __func__);

	atomic_set(&acdb_data.sidetone_cal.enable, cal_data->enable);
	atomic_set(&acdb_data.sidetone_cal.gain, cal_data->gain);
}


void get_sidetone_cal(struct sidetone_cal *cal_data)
{
	pr_debug("%s\n", __func__);

	if (cal_data == NULL) {
		pr_err("ACDB=> NULL pointer sent to %s\n", __func__);
		goto done;
	}

	cal_data->enable = atomic_read(&acdb_data.sidetone_cal.enable);
	cal_data->gain = atomic_read(&acdb_data.sidetone_cal.gain);
done:
	return;
}

static int acdb_open(struct inode *inode, struct file *f)
{
	s32 result = 0;
	pr_debug("%s\n", __func__);

	if (atomic64_read(&acdb_data.mem_len)) {
		pr_debug("%s: ACDB opened but memory allocated, "
			"using existing allocation!\n",
			__func__);
	}

	atomic_inc(&usage_count);

	return result;

}

static void allocate_hw_delay_entries(void)
{

	/* Allocate memory for hw delay entries */

	acdb_data.hw_delay_rx.num_entries = 0;
	acdb_data.hw_delay_tx.num_entries = 0;
	acdb_data.hw_delay_rx.delay_info =
				kmalloc(sizeof(struct hw_delay_entry)*
					MAX_HW_DELAY_ENTRIES,
					GFP_KERNEL);
	if (acdb_data.hw_delay_rx.delay_info == NULL) {
		pr_err("%s : Failed to allocate av sync delay entries rx\n",
			__func__);
	}
	acdb_data.hw_delay_tx.delay_info =
				kmalloc(sizeof(struct hw_delay_entry)*
					MAX_HW_DELAY_ENTRIES,
					GFP_KERNEL);
	if (acdb_data.hw_delay_tx.delay_info == NULL) {
		pr_err("%s : Failed to allocate av sync delay entries tx\n",
			__func__);
	}

	return;
}

static int deregister_memory(void)
{
	mutex_lock(&acdb_data.acdb_mutex);
	kfree(acdb_data.hw_delay_tx.delay_info);
	kfree(acdb_data.hw_delay_rx.delay_info);
	mutex_unlock(&acdb_data.acdb_mutex);

	if (atomic64_read(&acdb_data.mem_len)) {
		mutex_lock(&acdb_data.acdb_mutex);
		atomic_set(&acdb_data.vocstrm_total_cal_size, 0);
		atomic_set(&acdb_data.vocproc_total_cal_size, 0);
		atomic_set(&acdb_data.vocvol_total_cal_size, 0);
		atomic64_set(&acdb_data.mem_len, 0);
		ion_unmap_kernel(acdb_data.ion_client, acdb_data.ion_handle);
		ion_free(acdb_data.ion_client, acdb_data.ion_handle);
		ion_client_destroy(acdb_data.ion_client);
		mutex_unlock(&acdb_data.acdb_mutex);
	}
	return 0;
}

static int register_memory(void)
{
	int			result;
	unsigned long		paddr;
	void                    *kvptr;
	unsigned long		kvaddr;
	unsigned long		mem_len;

	mutex_lock(&acdb_data.acdb_mutex);
	allocate_hw_delay_entries();
	acdb_data.ion_client =
		msm_ion_client_create(UINT_MAX, "audio_acdb_client");
	if (IS_ERR_OR_NULL(acdb_data.ion_client)) {
		pr_err("%s: Could not register ION client!!!\n", __func__);
		result = PTR_ERR(acdb_data.ion_client);
		goto err;
	}

	acdb_data.ion_handle = ion_import_dma_buf(acdb_data.ion_client,
		atomic_read(&acdb_data.map_handle));
	if (IS_ERR_OR_NULL(acdb_data.ion_handle)) {
		pr_err("%s: Could not import map handle!!!\n", __func__);
		result = PTR_ERR(acdb_data.ion_handle);
		goto err_ion_client;
	}

	result = ion_phys(acdb_data.ion_client, acdb_data.ion_handle,
				&paddr, (size_t *)&mem_len);
	if (result != 0) {
		pr_err("%s: Could not get phys addr!!!\n", __func__);
		goto err_ion_handle;
	}

	kvptr = ion_map_kernel(acdb_data.ion_client,
		acdb_data.ion_handle);
	if (IS_ERR_OR_NULL(kvptr)) {
		pr_err("%s: Could not get kernel virt addr!!!\n", __func__);
		result = PTR_ERR(kvptr);
		goto err_ion_handle;
	}
	kvaddr = (unsigned long)kvptr;
	atomic64_set(&acdb_data.paddr, paddr);
	atomic64_set(&acdb_data.kvaddr, kvaddr);
	atomic64_set(&acdb_data.mem_len, mem_len);
	mutex_unlock(&acdb_data.acdb_mutex);

	pr_debug("%s: done! paddr = 0x%lx, "
		"kvaddr = 0x%lx, len = x%lx\n",
		 __func__,
		(long)atomic64_read(&acdb_data.paddr),
		(long)atomic64_read(&acdb_data.kvaddr),
		(long)atomic64_read(&acdb_data.mem_len));

	return result;
err_ion_handle:
	ion_free(acdb_data.ion_client, acdb_data.ion_handle);
err_ion_client:
	ion_client_destroy(acdb_data.ion_client);
err:
	atomic64_set(&acdb_data.mem_len, 0);
	mutex_unlock(&acdb_data.acdb_mutex);
	return result;
}

int gRingtone_state;     //Bruno++
int gSKYPE_state = 0;    //Bruno++
int gGarmin_state =0;    //ASUS Tim++
int galarm_state =0;         //ASUS Tim++
int gVR_state = 0;      //Bruno++
int gHAC_mode;     //ASUS Tim++
int gdevice_change =0;  //ASUS Tim++
int g_flag_csvoice_fe_connected = 0;
extern void SetLimitCurrentInPhoneCall(bool phoneOn);   //Bruno++
static long acdb_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	int32_t			result = 0;
	int32_t			size;
	int32_t			map_fd;
	uint32_t		topology;
	struct cal_block	data[MAX_NETWORKS];
	pr_debug("%s\n", __func__);

	switch (cmd) {
	case AUDIO_REGISTER_PMEM:
		pr_debug("AUDIO_REGISTER_PMEM\n");
		if (atomic_read(&acdb_data.mem_len)) {
			deregister_memory();
			pr_debug("Remove the existing memory\n");
		}

		if (copy_from_user(&map_fd, (void *)arg, sizeof(map_fd))) {
			pr_err("%s: fail to copy memory handle!\n", __func__);
			result = -EFAULT;
		} else {
			atomic_set(&acdb_data.map_handle, map_fd);
			result = register_memory();
		}
		goto done;

	case AUDIO_DEREGISTER_PMEM:
		pr_debug("AUDIO_DEREGISTER_PMEM\n");
		deregister_memory();
		goto done;
	case AUDIO_SET_VOICE_RX_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_voice_rx_topology(topology);
		goto done;
	case AUDIO_SET_VOICE_TX_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_voice_tx_topology(topology);
		goto done;
	case AUDIO_SET_ADM_RX_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_adm_rx_topology(topology);
		goto done;
	case AUDIO_SET_ADM_TX_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_adm_tx_topology(topology);
		goto done;
	case AUDIO_SET_ASM_TOPOLOGY:
		if (copy_from_user(&topology, (void *)arg,
				sizeof(topology))) {
			pr_err("%s: fail to copy topology!\n", __func__);
			result = -EFAULT;
		}
		store_asm_topology(topology);
		goto done;
	case AUDIO_SET_HW_DELAY_RX:
		result = store_hw_delay(RX_CAL, (void *)arg);
		goto done;
	case AUDIO_SET_HW_DELAY_TX:
		result = store_hw_delay(TX_CAL, (void *)arg);
		goto done;

    //Bruno++
    case AUDIO_SET_RINGTONE_STATE:      
        if (copy_from_user(&gRingtone_state, (void *)arg,
                sizeof(gRingtone_state))) {
            pr_err("%s: fail to copy gRingtone_state!\n", __func__);
            result = -EFAULT;
        }
        goto done;
    case AUDIO_GET_RINGTONE_STATE:
        if (copy_to_user((void *)arg, &gRingtone_state,
                sizeof(gRingtone_state))) {
            pr_err("%s: fail to copy gRingtone_state!!\n", __func__);
            result = -EFAULT;
        }
        goto done;
    case AUDIO_SET_INCALL_STATE:      
        if (copy_from_user(&g_flag_csvoice_fe_connected, (void *)arg,
                sizeof(g_flag_csvoice_fe_connected))) {
            pr_err("%s: fail to copy g_flag_csvoice_fe_connected!\n", __func__);
            result = -EFAULT;
            } else {
            SetLimitCurrentInPhoneCall( (bool)(g_flag_csvoice_fe_connected ? 1:0) ); 
        }
        goto done;
    case AUDIO_SET_SKYPE_STATE:      
        if (copy_from_user(&gSKYPE_state, (void *)arg,
                sizeof(gSKYPE_state))) {
            pr_err("%s: fail to copy gSKYPE_state!\n", __func__);
            result = -EFAULT;
        }
        goto done;
    case AUDIO_GET_SKYPE_STATE:
        if (copy_to_user((void *)arg, &gSKYPE_state,
                sizeof(gSKYPE_state))) {
            pr_err("%s: fail to copy gSKYPE_state!!\n", __func__);
            result = -EFAULT;
        }
        goto done;
    case AUDIO_SET_VR_STATE:      
        if (copy_from_user(&gVR_state, (void *)arg,
                sizeof(gVR_state))) {
            pr_err("%s: fail to copy gVR_state!\n", __func__);
            result = -EFAULT;
        }
        goto done;
    case AUDIO_GET_VR_STATE:
        if (copy_to_user((void *)arg, &gVR_state,
                sizeof(gVR_state))) {
            pr_err("%s: fail to copy gVR_state!!\n", __func__);
            result = -EFAULT;
        }
        goto done;
        //Bruno++
//ASUS Tim++
    case AUDIO_SET_HAC_mode:      
        if (copy_from_user(&gHAC_mode, (void *)arg,
                sizeof(gHAC_mode))) {
            pr_err("%s: fail to copy gHAC_mode!\n", __func__);
            result = -EFAULT;
        }
        goto done;
    case AUDIO_GET_HAC_mode:
        if (copy_to_user((void *)arg, &gHAC_mode,
                sizeof(gHAC_mode))) {
            pr_err("%s: fail to copy gHAC_mode!!\n", __func__);
            result = -EFAULT;
        }
        goto done;
	//ASUS Tim++
    case AUDIO_SET_Garmin_STATE:      
         if (copy_from_user(&gGarmin_state, (void *)arg,
                 sizeof(gGarmin_state))) {
             pr_err("%s: fail to copy gSKYPE_state!\n", __func__);
             result = -EFAULT;
         }
         goto done;
    case AUDIO_GET_Garmin_STATE:
        if (copy_to_user((void *)arg, &gGarmin_state,
                 sizeof(gGarmin_state))) {
             pr_err("%s: fail to copy gSKYPE_state!!\n", __func__);
             result = -EFAULT;
         }
         goto done; 

    case AUDIO_SET_Alarm_STATE:      
         if (copy_from_user(&galarm_state, (void *)arg,
                 sizeof(galarm_state))) {
             pr_err("%s: fail to copy gSKYPE_state!\n", __func__);
             result = -EFAULT;
         }
         goto done;
    case AUDIO_GET_Alarm_STATE:
        if (copy_to_user((void *)arg, &galarm_state,
                 sizeof(galarm_state))) {
             pr_err("%s: fail to copy gSKYPE_state!!\n", __func__);
             result = -EFAULT;
         }
         goto done; 
		 
	case AUDIO_SET_change_device:      
         if (copy_from_user(&gdevice_change, (void *)arg,
                 sizeof(gdevice_change))) {
             pr_err("%s: fail to copy gSKYPE_state!\n", __func__);
             result = -EFAULT;
         }
         goto done;
    case AUDIO_GET_change_device:
        if (copy_to_user((void *)arg, &gdevice_change,
                 sizeof(gdevice_change))) {
             pr_err("%s: fail to copy gSKYPE_state!!\n", __func__);
             result = -EFAULT;
         }
         goto done; 
	
//ASUS Tim--
	}    
    //Bruno++

	if (copy_from_user(&size, (void *) arg, sizeof(size))) {

		result = -EFAULT;
		goto done;
	}

	if ((size <= 0) || (size > sizeof(data))) {
		pr_err("%s: Invalid size sent to driver: %d\n",
			__func__, size);
		result = -EFAULT;
		goto done;
	}

	if (copy_from_user(data, (void *)(arg + sizeof(size)), size)) {

		pr_err("%s: fail to copy table size %d\n", __func__, size);
		result = -EFAULT;
		goto done;
	}

	if (data == NULL) {
		pr_err("%s: NULL pointer sent to driver!\n", __func__);
		result = -EFAULT;
		goto done;
	}

	switch (cmd) {
	case AUDIO_SET_AUDPROC_TX_CAL:
		if (size > sizeof(struct cal_block))
			pr_err("%s: More Audproc Cal then expected, "
				"size received: %d\n", __func__, size);
		store_audproc_cal(TX_CAL, data);
		break;
	case AUDIO_SET_AUDPROC_RX_CAL:
		if (size > sizeof(struct cal_block))
			pr_err("%s: More Audproc Cal then expected, "
				"size received: %d\n", __func__, size);
		store_audproc_cal(RX_CAL, data);
		break;
	case AUDIO_SET_AUDPROC_TX_STREAM_CAL:
		if (size > sizeof(struct cal_block))
			pr_err("%s: More Audproc Cal then expected, "
				"size received: %d\n", __func__, size);
		store_audstrm_cal(TX_CAL, data);
		break;
	case AUDIO_SET_AUDPROC_RX_STREAM_CAL:
		if (size > sizeof(struct cal_block))
			pr_err("%s: More Audproc Cal then expected, "
				"size received: %d\n", __func__, size);
		store_audstrm_cal(RX_CAL, data);
		break;
	case AUDIO_SET_AUDPROC_TX_VOL_CAL:
		if (size > sizeof(struct cal_block))
			pr_err("%s: More Audproc Cal then expected, "
				"size received: %d\n", __func__, size);
		store_audvol_cal(TX_CAL, data);
		break;
	case AUDIO_SET_AUDPROC_RX_VOL_CAL:
		if (size > sizeof(struct cal_block))
			pr_err("%s: More Audproc Cal then expected, "
				"size received: %d\n", __func__, size);
		store_audvol_cal(RX_CAL, data);
		break;
	case AUDIO_SET_AFE_TX_CAL:
		if (size > sizeof(struct cal_block))
			pr_err("%s: More AFE Cal then expected, "
				"size received: %d\n", __func__, size);
		store_afe_cal(TX_CAL, data);
		break;
	case AUDIO_SET_AFE_RX_CAL:
		if (size > sizeof(struct cal_block))
			pr_err("%s: More AFE Cal then expected, "
				"size received: %d\n", __func__, size);
		store_afe_cal(RX_CAL, data);
		break;
	case AUDIO_SET_VOCPROC_CAL:
		store_vocproc_cal(size / sizeof(struct cal_block), data);
		break;
	case AUDIO_SET_VOCPROC_STREAM_CAL:
		store_vocstrm_cal(size / sizeof(struct cal_block), data);
		break;
	case AUDIO_SET_VOCPROC_VOL_CAL:
		store_vocvol_cal(size / sizeof(struct cal_block), data);
		break;
	case AUDIO_SET_SIDETONE_CAL:
		if (size > sizeof(struct sidetone_cal))
			pr_err("%s: More sidetone cal then expected, "
				"size received: %d\n", __func__, size);
		store_sidetone_cal((struct sidetone_cal *)data);
		break;
	case AUDIO_SET_ANC_CAL:
		store_anc_cal(data);
		break;
	default:
		pr_err("ACDB=> ACDB ioctl not found!\n");
	}

done:
	return result;
}

static int acdb_mmap(struct file *file, struct vm_area_struct *vma)
{
	int result = 0;
	uint32_t size = vma->vm_end - vma->vm_start;

	pr_debug("%s\n", __func__);

	if (atomic64_read(&acdb_data.mem_len)) {
		if (size <= atomic64_read(&acdb_data.mem_len)) {
			vma->vm_page_prot = pgprot_noncached(
						vma->vm_page_prot);
			result = remap_pfn_range(vma,
				vma->vm_start,
				atomic64_read(&acdb_data.paddr) >> PAGE_SHIFT,
				size,
				vma->vm_page_prot);
		} else {
			pr_err("%s: Not enough memory!\n", __func__);
			result = -ENOMEM;
		}
	} else {
		pr_err("%s: memory is not allocated, yet!\n", __func__);
		result = -ENODEV;
	}

	return result;
}

static int acdb_release(struct inode *inode, struct file *f)
{
	s32 result = 0;

	atomic_dec(&usage_count);
	atomic_read(&usage_count);

	pr_debug("%s: ref count %d!\n", __func__,
		atomic_read(&usage_count));

	if (atomic_read(&usage_count) >= 1)
		result = -EBUSY;
	else
		result = deregister_memory();

	return result;
}

static const struct file_operations acdb_fops = {
	.owner = THIS_MODULE,
	.open = acdb_open,
	.release = acdb_release,
	.unlocked_ioctl = acdb_ioctl,
	.mmap = acdb_mmap,
};

struct miscdevice acdb_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_acdb",
	.fops	= &acdb_fops,
};

static int __init acdb_init(void)
{
	memset(&acdb_data, 0, sizeof(acdb_data));
	mutex_init(&acdb_data.acdb_mutex);
	atomic_set(&usage_count, 0);
	return misc_register(&acdb_misc);
}

static void __exit acdb_exit(void)
{
}

module_init(acdb_init);
module_exit(acdb_exit);

MODULE_DESCRIPTION("MSM 8x60 Audio ACDB driver");
MODULE_LICENSE("GPL v2");
