/*
 * resid-fp.cc - reSID-fp interface code.
 *
 * Written by
 *  Teemu Rantanen <tvr@cs.hut.fi>
 *  Dag Lem <resid@nimrod.no>
 *  Andreas Boose <viceteam@t-online.de>
 *  Antti S. Lankila <alankila@bel.fi>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

extern "C" {

#include <string.h>

#include "sid/sid.h"
#include "lib.h"
#include "log.h"
#include "resid-fp.h"
#include "resources.h"
#include "sid-snapshot.h"
#include "types.h"

} // extern "C"

#include "resid-fp/sid.h"

extern "C" {

struct sound_s
{
    int factor;

    SIDFP *sid;
};

typedef struct sound_s sound_t;

static short *buf = NULL;
static int blen = 0;

static short *getbuf(int len)
{
    if ((buf == NULL) || (blen < len)) {
        if (buf) {
            lib_free(buf);
        }
        blen = len;
        buf = (short *)lib_calloc(len, 1);
    }
    return buf;
}

static sound_t *residfp_open(uint8_t *sidstate)
{
    sound_t *psid;
    int i;

    psid = new sound_t;
    psid->sid = new SIDFP;

    for (i = 0x00; i <= 0x18; i++) {
        psid->sid->write(i, sidstate[i]);
    }

    return psid;
}

static int residfp_init(sound_t *psid, int speed, int cycles_per_sec, int factor)
{
    sampling_method method;
    char model_text[100];
    char method_text[100];
    float passband;
    int filters_enabled, model, sampling, passband_percentage;

    if (resources_get_int("SidFilters", &filters_enabled) < 0) {
        return 0;
    }

    if (resources_get_int("SidModel", &model) < 0) {
        return 0;
    }

    if (resources_get_int("SidResidSampling", &sampling) < 0) {
        return 0;
    }

    if (model == 1 || model == 2) {
        if (resources_get_int("SidResid8580Passband", &passband_percentage) < 0) {
            return 0;
        }
    } else {
        if (resources_get_int("SidResidPassband", &passband_percentage) < 0) {
            return 0;
        }
    }

    passband = (float)speed * passband_percentage / 200.f;

    psid->factor = factor;

    psid->sid->input(0);

    switch (model) {
      default:
      case 0:
        psid->sid->set_chip_model(MOS6581FP);
        psid->sid->set_voice_nonlinearity(0.96f);
        psid->sid->get_filter().set_distortion_properties(0.5f, 3.3e6f);
        psid->sid->get_filter().set_type3_properties(1141069.9277645703f, 276016753.85303545f, 1.0066634233403395f, 16402.86712485317f);
        strcpy(model_text, "MOS6581");
        break;
      case 1:
        psid->sid->set_chip_model(MOS8580FP);
        psid->sid->set_voice_nonlinearity(1.0f);
        psid->sid->get_filter().set_distortion_properties(0.5f, 0.f);
        psid->sid->get_filter().set_type4_properties(5.7f, 20.0f);
        strcpy(model_text, "MOS8580");
        break;
      case 2:
        psid->sid->set_chip_model(MOS8580FP);
        psid->sid->set_voice_nonlinearity(1.0f);
        psid->sid->get_filter().set_distortion_properties(0.5f, 0.f);
        psid->sid->get_filter().set_type4_properties(5.7f, 20.0f);
        psid->sid->input(-32768);
        strcpy(model_text, "MOS8580 + digi boost");
        break;
    }

    psid->sid->enable_filter(filters_enabled ? true : false);

    switch (sampling) {
      default:
      case 0:
      case 1:
        method = SAMPLE_INTERPOLATE;
        strcpy(method_text, "interpolation");
        break;
      case 2:
      case 3:
        method = SAMPLE_RESAMPLE_INTERPOLATE;
        sprintf(method_text, "resampling, pass to %dHz", (int)passband);
        break;
    }

    if (!psid->sid->set_sampling_parameters((float)cycles_per_sec, method,
                                            (float)speed, passband)) {
        log_warning(LOG_DEFAULT,
                    "ReSID-FP: Out of spec, increase sampling rate or decrease maximum speed");
        return 0;
    }

    log_message(LOG_DEFAULT, "ReSID-FP: %s, filter %s, sampling rate %dHz - %s",
                model_text,
                filters_enabled ? "on" : "off",
                speed, method_text);

    return 1;
}

static void residfp_close(sound_t *psid)
{
    delete psid->sid;
    delete psid;

    if (buf) {
        lib_free(buf);
        buf = NULL;
        blen = 0;
    }
}

static uint8_t residfp_read(sound_t *psid, uint16_t addr)
{
    return psid->sid->read(addr & 0xffu);
}

static void residfp_store(sound_t *psid, uint16_t addr, uint8_t byte)
{
    psid->sid->write(addr & 0xffu, byte);
}

static void residfp_reset(sound_t *psid, CLOCK cpu_clk)
{
    psid->sid->reset();
}

static int residfp_calculate_samples(sound_t *psid, short *pbuf, int nr,
                                     int interleave, int *delta_t)
{
    short *tmp_buf;
    int retval;

    if (psid->factor == 1000) {
        return psid->sid->clock(*delta_t, pbuf, nr, interleave);
    }

    tmp_buf = getbuf(2 * nr * psid->factor / 1000);
    retval = psid->sid->clock(*delta_t, tmp_buf, nr * psid->factor / 1000, interleave) * 1000 / psid->factor;
    memcpy(pbuf, tmp_buf, 2 * nr);
    return retval;
}

static void residfp_prevent_clk_overflow(sound_t *psid, CLOCK sub)
{
}

static char *residfp_dump_state(sound_t *psid)
{
    return lib_stralloc("");
}

static void residfp_state_read(sound_t *psid, sid_snapshot_state_t *sid_state)
{
    SIDFP::State state;
    unsigned int i;

    if (psid) {
        state = psid->sid->read_state();
    }

    for (i = 0; i < 0x20; i++) {
        sid_state->sid_register[i] = (uint8_t)state.sid_register[i];
    }

    sid_state->bus_value = (uint8_t)state.bus_value;
    sid_state->bus_value_ttl = (uint32_t)state.bus_value_ttl;
    for (i = 0; i < 3; i++) {
        sid_state->accumulator[i] = (uint32_t)state.accumulator[i];
        sid_state->shift_register[i] = (uint32_t)state.shift_register[i];
        sid_state->rate_counter[i] = (uint16_t)state.rate_counter[i];
        sid_state->rate_counter_period[i] = (uint16_t)state.rate_counter_period[i];
        sid_state->exponential_counter[i] = (uint16_t)state.exponential_counter[i];
        sid_state->exponential_counter_period[i] = (uint16_t)state.exponential_counter_period[i];
        sid_state->envelope_counter[i] = (uint8_t)state.envelope_counter[i];
        sid_state->envelope_state[i] = (uint8_t)state.envelope_state[i];
        sid_state->hold_zero[i] = (uint8_t)state.hold_zero[i];
        sid_state->envelope_pipeline[i] = 0;
        sid_state->shift_pipeline[i] = 0;
        sid_state->shift_register_reset[i] = 0;
        sid_state->floating_output_ttl[i] = 0;
        sid_state->pulse_output[i] = 0;
    }
    sid_state->write_pipeline = 0;
    sid_state->write_address = 0;
    sid_state->voice_mask = 0x07;
}

static void residfp_state_write(sound_t *psid, sid_snapshot_state_t *sid_state)
{
    SIDFP::State state;
    unsigned int i;

    for (i = 0; i < 0x20; i++) {
        state.sid_register[i] = (char)sid_state->sid_register[i];
    }

    state.bus_value = (reg8)sid_state->bus_value;
    state.bus_value_ttl = (cycle_count)sid_state->bus_value_ttl;
    for (i = 0; i < 3; i++) {
        state.accumulator[i] = (reg24)sid_state->accumulator[i];
        state.shift_register[i] = (reg24)sid_state->shift_register[i];
        state.rate_counter[i] = (reg16)sid_state->rate_counter[i];
        if (sid_state->rate_counter_period[i]) {
            state.rate_counter_period[i] = (reg16)sid_state->rate_counter_period[i];
        }
        state.exponential_counter[i] = (reg16)sid_state->exponential_counter[i];
        if (sid_state->exponential_counter_period[i]) {
            state.exponential_counter_period[i] = (reg16)sid_state->exponential_counter_period[i];
        }
        state.envelope_counter[i] = (reg8)sid_state->envelope_counter[i];
        state.envelope_state[i] = (EnvelopeGeneratorFP::State)sid_state->envelope_state[i];
        state.hold_zero[i] = (sid_state->hold_zero[i] != 0);
    }

    if (psid) {
        psid->sid->write_state((const SIDFP::State)state);
    }
}

sid_engine_t residfp_hooks =
{
    residfp_open,
    residfp_init,
    residfp_close,
    residfp_read,
    residfp_store,
    residfp_reset,
    residfp_calculate_samples,
    residfp_prevent_clk_overflow,
    residfp_dump_state,
    residfp_state_read,
    residfp_state_write
};

} // extern "C"
