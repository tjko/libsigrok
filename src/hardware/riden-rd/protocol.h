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
