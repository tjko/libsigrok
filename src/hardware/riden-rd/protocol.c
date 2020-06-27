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

SR_PRIV int riden_rd_read_registers(const struct sr_dev_inst *sdi,
					int address, int nb_registers,
					uint16_t *registers)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	int i, ret;

	if (!sdi || !registers)
		return SR_ERR_ARG;

	devc = sdi->priv;
	modbus = sdi->conn;

	if (!devc || !modbus)
		return SR_ERR;

	g_mutex_lock(&devc->mutex);
	ret = sr_modbus_read_holding_registers(modbus, address,
					       nb_registers, registers);
	g_mutex_unlock(&devc->mutex);

	if (ret == SR_OK) {
		for (i = 0; i < nb_registers; i++) {
			registers[i] = RB16(&registers[i]);
			sr_dbg("read modbus: register=%d, val=%u",
			       address+i, registers[i]);
		}
	}

	return ret;
}

SR_PRIV int riden_rd_write_register(const struct sr_dev_inst *sdi, int address,
				    uint16_t value)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	uint16_t registers[1];
	int ret;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	modbus = sdi->conn;

	if (!devc || !modbus)
		return SR_ERR;

	WB16(registers, value);
	g_mutex_lock(&devc->mutex);
	ret = sr_modbus_write_multiple_registers(modbus, address, 1, registers);
	sr_dbg("write modbus: register=%d, val=%u (%d)", address, value, ret);
	g_mutex_unlock(&devc->mutex);

	return ret;
}

SR_PRIV void riden_rd_channel_send_value(const struct sr_dev_inst *sdi,
					 struct sr_channel *ch, float value,
					 enum sr_mq mq, enum sr_unit unit, int digits)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;

	sr_analog_init(&analog, &encoding, &meaning, &spec, digits);
	analog.meaning->channels = g_slist_append(NULL, ch);
	analog.num_samples = 1;
	analog.data = &value;
	analog.meaning->mq = mq;
	analog.meaning->unit = unit;
	//analog.meaning->mqflags = SR_MQFLAG_DC;

	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	sr_session_send(sdi, &packet);
	g_slist_free(analog.meaning->channels);
}

SR_PRIV int riden_rd_receive_data(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	GSList *l;
	uint16_t regs[4];
	double voltage, current, power, capacity, energy, temp1, temp2;
	int ret, i;

	sr_spew("%s(%d,%d,%p): called", __func__, fd, revents, cb_data);

	(void)fd;
	(void)revents;

	if (!(sdi = cb_data))
		return TRUE;

	devc = sdi->priv;
	if (!devc)
		return TRUE;


	/* read: voltage, current, power */
	if ((ret = riden_rd_read_registers(sdi, REG_VOLTAGE, 4, regs)) != SR_OK)
		goto done;
	voltage = regs[0] * devc->model->voltage[2];
	current = regs[1] * devc->model->current[2];
	power = regs[3] * devc->model->power[2];

	/* read: capacity, energy */
	if ((ret = riden_rd_read_registers(sdi, REG_CAPACITY, 4, regs)) != SR_OK)
		goto done;
	capacity = ((regs[0] << 16) + regs[1]) * 0.001;
	energy = ((regs[2] << 16) + regs[3]) * 0.001;

	/* read: internal temperature */
	if ((ret = riden_rd_read_registers(sdi, REG_TEMP_INTERNAL, 2, regs)) != SR_OK)
		goto done;
	temp1 = (regs[0]  ? -1 : 1) * regs[1];

	/* read: external temperature */
	if ((ret = riden_rd_read_registers(sdi, REG_TEMP_EXTERNAL, 2, regs)) != SR_OK)
		goto done;
	temp2 = (regs[0] ? -1 : 1) * regs[1];

	sr_dbg("V=%.3fV I=%.3fA P=%.3fW C=%.3fAh E=%.3fWh T1=%.1fC T2=%.1fC",
	       voltage,current,power,capacity,energy,temp1,temp2);

	/* send data */
	std_session_send_df_frame_begin(sdi);

	i = 0;
	l = g_slist_nth(sdi->channels, i++);
	riden_rd_channel_send_value(sdi, l->data, voltage, SR_MQ_VOLTAGE,
				    SR_UNIT_VOLT, devc->model->voltage[3]);
	l = g_slist_nth(sdi->channels, i++);
	riden_rd_channel_send_value(sdi, l->data, current, SR_MQ_CURRENT,
				    SR_UNIT_AMPERE, devc->model->current[3]);
	l = g_slist_nth(sdi->channels, i++);
	riden_rd_channel_send_value(sdi, l->data, power, SR_MQ_POWER,
				    SR_UNIT_WATT, devc->model->power[3]);
	/*
	l = g_slist_nth(sdi->channels, i++);
	riden_rd_channel_send_value(sdi, l->data, capacity, SR_MQ_CAPACITY,
				    SR_UNIT_AMPERE_HOUR, 3);
	*/
	l = g_slist_nth(sdi->channels, i++);
	riden_rd_channel_send_value(sdi, l->data, energy, SR_MQ_ENERGY,
				    SR_UNIT_WATT_HOUR, 3);
	l = g_slist_nth(sdi->channels, i++);
	riden_rd_channel_send_value(sdi, l->data, temp1, SR_MQ_TEMPERATURE,
				    SR_UNIT_CELSIUS, 0);
	l = g_slist_nth(sdi->channels, i++);
	riden_rd_channel_send_value(sdi, l->data, temp2, SR_MQ_TEMPERATURE,
				    SR_UNIT_CELSIUS, 0);

	std_session_send_df_frame_end(sdi);
	sr_sw_limits_update_samples_read(&devc->limits, 1);

done:

	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);

	return TRUE;
}
