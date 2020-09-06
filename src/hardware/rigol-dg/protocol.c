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
#include "scpi.h"
#include "protocol.h"


SR_PRIV const char* waveform_type_to_string(enum waveform_type type)
{
	switch (type) {
	case WF_DC:
		return "DC";
	case WF_SINE:
		return "Sine";
	case WF_SQUARE:
		return "Square";
	case WF_RAMP:
		return "Ramp";
	case WF_PULSE:
		return "Pulse";
	case WF_NOISE:
		return "Noise";
	case WF_ARB:
		return "Arb";
		
	}
	return "Unknown";
}

SR_PRIV const struct waveform_spec* get_waveform_spec(const struct channel_spec *ch, enum waveform_type wf)
{
	const struct waveform_spec *spec;
	unsigned int i;

	spec = NULL;
	for (i = 0; i < ch->num_waveforms; i++) {
		if (ch->waveforms[i].waveform == wf) {
			spec = &ch->waveforms[i];
			break;
		}
	}
	
	return spec;
}

SR_PRIV int rigol_dg_get_channel_state(const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct sr_channel *ch;
	struct channel_status *ch_status;
	const char *command;
	GVariant *data;
	const gchar *response, *s;
	gchar **params;
	enum waveform_type wf;
	double freq, ampl, offset, phase;
	int ret;

	devc = sdi->priv;
	data = NULL;
	params = NULL;
	ret = SR_ERR_NA;

	if (!sdi || !cg)
		return SR_ERR_BUG;

	ch = cg->channels->data;
	ch_status = &devc->ch_status[ch->index];
	sr_info("cg=%s, ch=%s, ch->index=%d", cg->name, ch->name, ch->index);

	command = sr_scpi_cmd_get(devc->cmdset, PSG_CMD_GET_SOURCE);
	if (command && *command) {
		ret = sr_scpi_cmd_resp(sdi, devc->device->cmdset,
				       PSG_CMD_SELECT_CHANNEL, cg->name,
				       &data, G_VARIANT_TYPE_STRING,
				       PSG_CMD_GET_SOURCE, cg->name);
		if (ret != SR_OK)
			goto done;
		response = g_variant_get_string(data, NULL);
		sr_spew("Channel state: '%s'", response);
		params = g_strsplit(response, ",", 0);
		if (!params[0]) 
			goto done;
		
		/* First parameter is the waveform type */
		if (!(s = params[0]))
			goto done;
		if (s[0] == '"') s++;
		sr_info("signal type: %s", s);
		if (!strncmp(s, "SIN", 3)) { wf = WF_SINE; }
		else if (!strncmp(s, "SQU", 3)) { wf = WF_SQUARE; }
		else if (!strncmp(s, "RAMP", 4)) { wf = WF_RAMP; }
		else if (!strncmp(s, "PULSE", 5)) { wf = WF_PULSE; }
		else if (!strncmp(s, "NOISE", 5)) { wf = WF_NOISE; }
		else if (!strncmp(s, "USER", 4)) { wf = WF_ARB; }
		else if (!strncmp(s, "DC", 2)) { wf = WF_DC; }
		else goto done;
		sr_info("waveform: %s (%d)", waveform_type_to_string(wf), wf);
		ch_status->wf = wf;
		ch_status->wf_spec = get_waveform_spec(&devc->device->channels[ch->index], wf);

		/* Second parameter if the frequency (or "DEF" if not applicable) */
		if (!(s = params[1]))
			goto done;
		freq = g_ascii_strtod(s, NULL);
		sr_info("Frequency: %s (%f)", s, freq);
		ch_status->freq = freq;

		/* Third parameter if the amplitude (or "DEF" if not applicable) */
		if (!(s = params[2]))
			goto done;
		ampl = g_ascii_strtod(s, NULL);
		sr_info("Amplitude: %s (%f)", s, ampl);
		ch_status->ampl = ampl;

		/* Fourth parameter if the offset (or "DEF" if not applicable) */
		if (!(s = params[3]))
			goto done;
		offset = g_ascii_strtod(s, NULL);
		sr_info("Offset: %s (%f)", s, offset);
		ch_status->offset = offset;

		/* Fifth parameter if the phase (or "DEF" if not applicable) */
		if (!(s = params[4]))
			goto done;
		phase = g_ascii_strtod(s, NULL);
		sr_info("Phase: %s (%f)", s, phase);
		ch_status->phase = phase;
		
		ret = SR_OK;
	}

done:
	g_strfreev(params);
	return ret;
}

SR_PRIV int rigol_dg_receive_data(int fd, int revents, void *cb_data)
{
	const struct sr_dev_inst *sdi;
	struct dev_context *devc;

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (revents == G_IO_IN) {
		/* TODO */
	}

	return TRUE;
}
