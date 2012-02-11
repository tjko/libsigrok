/*
 * This file is part of the sigrok project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef LIBSIGROK_HARDWARE_FX2LAFW
#define LIBSIGROK_HARDWARE_FX2LAFW

struct fx2lafw_profile {
	uint16_t vid;
	uint16_t pid;

	char *vendor;
	char *model;
	char *model_version;

	int num_probes;
};

struct fx2lafw_device {
	struct fx2lafw_profile *profile;

	void *session_data;

	struct sr_usb_device_instance *usb;
};

#endif
