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

#ifndef LIBSIGROK_HARDWARE_RIDEN_RD_PROTOCOL_H
#define LIBSIGROK_HARDWARE_RIDEN_RD_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "riden-rd"

enum riden_modbus_registers {
	REG_MODEL                   = 0,
	REG_SERIAL                  = 1,
	REG_FIRMWARE                = 3,
	REG_TEMP_INTERNAL           = 4, /* two registers */
	REG_TEMP_INTERNAL_F         = 6, /* two registers */
	REG_VOLTAGE_TARGET          = 8,
	REG_CURRENT_LIMIT           = 9,
	REG_VOLTAGE                 = 10,
	REG_CURRENT                 = 11,
/*	REG_ENERGY                  = 12, */
	REG_POWER                   = 13,
	REG_INPUT_VOLTAGE           = 14,
	REG_PROTECTION_STATUS       = 16, /* OVP/OCP status */
	REG_REGULATION_STATUS       = 17, /* CV/CC */
	REG_ENABLE                  = 18,
	REG_BATTERY_MODE            = 32,
	REG_BATTERY_VOLTAGE         = 33,
	REG_TEMP_EXTERNAL           = 34, /* two registers */
	REG_TEMP_EXTERNAL_F         = 36, /* two registers */
	REG_CAPACITY                = 38, /* two registers */
	REG_ENERGY                  = 40, /* two registers */
	REG_DATE_YEAR               = 48,
	REG_DATE_MONTH              = 49,
	REG_DATE_DAY                = 50,
	REG_TIME_HOUR               = 51,
	REG_TIME_MIN                = 52,
	REG_TIME_SEC                = 53,
	REG_BACKLIGHT               = 72,
	REG_OVP_THRESHOLD           = 82,
	REG_OCP_THRESHOLD           = 83,
};

struct riden_model {
	uint16_t model;
	const char *name;

	/* Min, max, programming resolution, spec digits, encoding digits. */
	double voltage[5];
	double current[5];
	double power[5];
	double ovp[5];
	double ocp[5];
};

struct dev_context {
	const struct riden_model *model;

	struct sr_sw_limits limits;
	GMutex mutex;
};

SR_PRIV int riden_rd_read_registers(const struct sr_dev_inst *sdi, int address,
				    int nb_registers, uint16_t *registers);
SR_PRIV int riden_rd_write_register(const struct sr_dev_inst *sdi, int address,
				    uint16_t value);
SR_PRIV void riden_rd_channel_send_value(const struct sr_dev_inst *sdi,
					 struct sr_channel *ch, float value,
					 enum sr_mq mq, enum sr_unit unit, int digits);
SR_PRIV int riden_rd_receive_data(int fd, int revents, void *cb_data);

#endif
