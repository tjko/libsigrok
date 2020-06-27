/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2020 Timo Kokkonen <tjko@iki.fi>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "protocol.h"

#define DEFAULT_SERIALCOMM "115200/8n1"
#define DEFAULT_MODBUSADDR 1

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
	SR_CONF_MODBUSADDR,
};

static const uint32_t drvopts[] = {
	SR_CONF_POWER_SUPPLY,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_VOLTAGE | SR_CONF_GET,
	SR_CONF_VOLTAGE_TARGET | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_CURRENT | SR_CONF_GET,
	SR_CONF_CURRENT_LIMIT | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_ENABLED | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_REGULATION | SR_CONF_GET,
	SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED | SR_CONF_GET,
	SR_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE | SR_CONF_GET,
	SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_OVER_CURRENT_PROTECTION_ENABLED | SR_CONF_GET,
	SR_CONF_OVER_CURRENT_PROTECTION_ACTIVE | SR_CONF_GET,
	SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD | SR_CONF_GET | SR_CONF_SET,
};

static const struct riden_model supported_models[] = {
	{ 6006, "RD6006",
	  { 0,  60.0, 0.010, 2, 2 }, /* voltage */
	  { 0,   6.0, 0.001, 3, 3 }, /* current */
	  { 0, 380.0, 0.010, 2, 2 }, /* power */
	  { 0,  62.0, 0.010, 2, 2 }, /* OVP */
	  { 0,   6.2, 0.001, 3, 3 }, /* OCP */
	},
	{ 6012, "RD6012",
	  { 0,  60.0, 0.010, 2, 2 }, /* voltage */
	  { 0,   6.0, 0.001, 3, 3 }, /* current */
	  { 0, 720.0, 0.010, 2, 2 }, /* power */
	  { 0,  62.0, 0.010, 2, 2 }, /* OVP */
	  { 0,  12.4, 0.001, 3, 3 }, /* OCP */
	},
};

static struct sr_dev_driver riden_rd_driver_info;

static struct sr_dev_inst *probe_device(struct sr_modbus_dev_inst *modbus)
{
	const struct riden_model *model = NULL;
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_channel_group *cg;
	struct sr_channel *ch;
	unsigned int i;
	int ret;
	uint16_t id, registers[4];
	char *serial,*fw;

	sr_spew("%s(%p)", __func__, modbus);

	/* query device model information */
	ret = sr_modbus_read_holding_registers(modbus, 0, 4, registers);
	if (ret != SR_OK)
		return NULL;
	id = RB16(&registers[REG_MODEL])/10;
	serial = g_strdup_printf("%08d", RB32(&registers[REG_SERIAL]));
	fw = g_strdup_printf("%1.2f", RB16(&registers[REG_FIRMWARE]) / 100.0);

	for (i = 0; i < ARRAY_SIZE(supported_models); i++)
		if (id == supported_models[i].model) {
			model = &supported_models[i];
			break;
		}
	if (model == NULL) {
		sr_err("Unknown model: %d", id);
		return NULL;
	}

	sr_info("Model: %s (%u)", model->name, RB16(&registers[0]));
	sr_info("Firmware version: %s", fw);
	sr_info("Serial number: %s", serial);


	/* setup device instance and context */
	sdi = g_malloc0(sizeof(*sdi));
	devc = g_malloc0(sizeof(*devc));

	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("Riden");
	sdi->model = g_strdup(model->name);
	sdi->version = fw;
	sdi->serial_num = serial;
	sdi->conn = modbus;
	sdi->driver = &riden_rd_driver_info;
	sdi->inst_type = SR_INST_MODBUS;
	sdi->priv = devc;

	sr_sw_limits_init(&devc->limits);
	g_mutex_init(&devc->mutex);
	devc->model = model;

	/* setup channels */
	cg = g_malloc0(sizeof(*cg));
	cg->name = g_strdup("1");
	sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);

	ch = sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "V");
	cg->channels = g_slist_append(cg->channels, ch);
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "I");
	cg->channels = g_slist_append(cg->channels, ch);
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "P");
	cg->channels = g_slist_append(cg->channels, ch);
	/*
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "C");
	cg->channels = g_slist_append(cg->channels, ch);
	*/
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "E");
	cg->channels = g_slist_append(cg->channels, ch);
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "T1");
	cg->channels = g_slist_append(cg->channels, ch);
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "T2");
	cg->channels = g_slist_append(cg->channels, ch);

	return sdi;
}

static int config_compare(gconstpointer a, gconstpointer b)
{
	return ((struct sr_config*)a)->key != ((struct sr_config*)b)->key;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *opts, *devices;
	struct sr_config *def_serial, *def_modbus;

	sr_spew("%s(%p,%p)", __func__, di, options);

	def_serial = sr_config_new(SR_CONF_SERIALCOMM,
				   g_variant_new_string(DEFAULT_SERIALCOMM));
	def_modbus = sr_config_new(SR_CONF_MODBUSADDR,
				   g_variant_new_uint64(DEFAULT_MODBUSADDR));
	opts = options;

	/* apply default serialcomm and modbus address if those were
	   not specified */
	if (!g_slist_find_custom(options, def_serial, config_compare)) {
		sr_info("Using default serialcomm settings: %s",
			DEFAULT_SERIALCOMM);
		opts = g_slist_prepend(opts, def_serial);
	}
	if (!g_slist_find_custom(options, def_modbus, config_compare)) {
		sr_info("Using default modbus address: %d",
			DEFAULT_MODBUSADDR);
		opts = g_slist_prepend(opts, def_modbus);
	}

	devices = sr_modbus_scan(di->context, opts, probe_device);

	/* remove and free the default config objects */
	while (opts != options)
		opts = g_slist_delete_link(opts, opts);
	sr_config_free(def_serial);
	sr_config_free(def_modbus);

	return devices;
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_modbus_dev_inst *modbus;

	modbus = sdi->conn;

	if (!sdi || !modbus)
		return SR_ERR_ARG;

	return sr_modbus_open(modbus);
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct sr_modbus_dev_inst *modbus;

	modbus = sdi->conn;

	if (!sdi || !modbus)
		return SR_ERR_ARG;

	return sr_modbus_close(modbus);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	uint16_t regs[4];
	int ret;

	sr_spew("%s(%u,%p,%p,%p)", __func__, key, data, sdi, cg);

	if (!sdi || !data)
		return SR_ERR_ARG;

	devc = sdi->priv;
	(void)cg;
	ret = SR_OK;

	switch (key) {
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		ret = sr_sw_limits_config_get(&devc->limits, key, data);
		break;
	case SR_CONF_ENABLED:
		if ((ret = riden_rd_read_registers(sdi, REG_ENABLE,
						   1, regs)) == SR_OK)
			*data = g_variant_new_boolean(regs[0]);
		break;
	case SR_CONF_REGULATION:
		if ((ret = riden_rd_read_registers(sdi, REG_REGULATION_STATUS,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_string(regs[0] ? "CC" : "CV");
		break;
	case SR_CONF_VOLTAGE:
		if ((ret = riden_rd_read_registers(sdi, REG_VOLTAGE,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_double((double)regs[0] * devc->model->voltage[2]);
		break;
	case SR_CONF_VOLTAGE_TARGET:
		if ((ret = riden_rd_read_registers(sdi, REG_VOLTAGE_TARGET,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_double((double)regs[0] * devc->model->voltage[2]);
		break;
	case SR_CONF_CURRENT:
		if ((ret = riden_rd_read_registers(sdi, REG_CURRENT,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_double((double)regs[0] * devc->model->current[2]);
		break;
	case SR_CONF_CURRENT_LIMIT:
		if ((ret = riden_rd_read_registers(sdi, REG_CURRENT_LIMIT,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_double((double)regs[0] * devc->model->current[2]);
		break;
	case SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED:
		*data = g_variant_new_boolean(TRUE);
		break;
	case SR_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE:
		if ((ret = riden_rd_read_registers(sdi, REG_PROTECTION_STATUS,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_boolean(regs[0] & 0x01);
		break;
	case SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		if ((ret = riden_rd_read_registers(sdi, REG_OVP_THRESHOLD,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_double((double)regs[0] * devc->model->ovp[2]);
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_ENABLED:
		*data = g_variant_new_boolean(TRUE);
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_ACTIVE:
		if ((ret = riden_rd_read_registers(sdi, REG_PROTECTION_STATUS,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_boolean(regs[0] & 0x02);
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		if ((ret = riden_rd_read_registers(sdi, REG_OCP_THRESHOLD,
						   1, regs)) != SR_OK)
			break;
		*data = g_variant_new_double((double)regs[0] * devc->model->ocp[2]);
		break;

	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const struct sr_key_info *kinfo;
	int ret;

	kinfo = sr_key_info_get(SR_KEY_CONFIG, key);
	sr_spew("%s(%u [%s],%p,%p,%p)", __func__, key,
		(kinfo ? kinfo->name : "unknown"), data, sdi, cg);

	if (!data || !sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	ret = SR_OK;

	switch (key) {
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		ret = sr_sw_limits_config_set(&devc->limits, key, data);
		break;
	case SR_CONF_ENABLED:
		ret = riden_rd_write_register(sdi, REG_ENABLE,
					      g_variant_get_boolean(data));
		break;
	case SR_CONF_VOLTAGE_TARGET:
		ret = riden_rd_write_register(sdi, REG_VOLTAGE_TARGET,
			g_variant_get_double(data) / devc->model->voltage[2]);
		break;
	case SR_CONF_CURRENT_LIMIT:
		ret = riden_rd_write_register(sdi, REG_CURRENT_LIMIT,
			g_variant_get_double(data) / devc->model->current[2]);
		break;
	case SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		ret = riden_rd_write_register(sdi, REG_OVP_THRESHOLD,
			g_variant_get_double(data) / devc->model->ovp[2]);
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		ret = riden_rd_write_register(sdi, REG_OCP_THRESHOLD,
			g_variant_get_double(data) / devc->model->ocp[2]);
		break;
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	const struct dev_context *devc;
	const struct sr_key_info *kinfo;

	kinfo = sr_key_info_get(SR_KEY_CONFIG, key);
	sr_spew("%s(%u [%s],%p,%p,%p)", __func__,
		key, (kinfo ? kinfo->name : "unknown"),
		data, sdi, cg);

	if (!data)
		return SR_ERR_ARG;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	}

	devc = (sdi ? sdi->priv : NULL);
	if (!devc)
		return SR_ERR_ARG;

	switch (key) {
	case SR_CONF_VOLTAGE_TARGET:
		*data = std_gvar_min_max_step_array(devc->model->voltage);
		break;
	case SR_CONF_CURRENT_LIMIT:
		*data = std_gvar_min_max_step_array(devc->model->current);
		break;
	case SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		*data = std_gvar_min_max_step_array(devc->model->ovp);
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		*data = std_gvar_min_max_step_array(devc->model->ocp);
		break;
	default:
		sr_info("%s: unsupported key: %d (%s)", __func__,
			key, (kinfo ? kinfo->name : "unknown"));
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	int ret;

	sr_spew("%s(%p): called", __func__, sdi);

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	modbus = sdi->conn;

	ret = sr_modbus_source_add(sdi->session, modbus, G_IO_IN,
				   10, riden_rd_receive_data, (void *)sdi);
	if (ret == SR_OK) {
		sr_sw_limits_acquisition_start(&devc->limits);
		std_session_send_df_header(sdi);
	}

	return ret;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct sr_modbus_dev_inst *modbus;

	sr_spew("%s(%p): called", __func__, sdi);

	if (!sdi)
		return SR_ERR_ARG;

	modbus = sdi->conn;

	sr_modbus_source_remove(sdi->session, modbus);
	std_session_send_df_end(sdi);

	return SR_OK;
}

static void dev_clear_callback(void *priv)
{
	struct dev_context *devc;

	sr_spew("%s(%p): called", __func__, priv);

	if (!priv)
		return;

	devc = priv;
	g_mutex_clear(&devc->mutex);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	sr_spew("%s(%p): called", __func__, di);

	return std_dev_clear_with_callback(di, dev_clear_callback);
}

static struct sr_dev_driver riden_rd_driver_info = {
	.name = "riden-rd",
	.longname = "Riden RD60xx series",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(riden_rd_driver_info);
