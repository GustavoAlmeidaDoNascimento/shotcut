/*
 * Copyright (c) 2026 Meltytech, LLC
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

#include "vjloopfilter.h"

#include <framework/mlt.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

struct PrivateData
{
    std::vector<unsigned char> envelope;
    int envelopeChars;
    int hasPos;
    double level;
    mlt_position lastPos;
    double b0;
    double b1;
    double b2;
    double a1;
    double a2;
    double z1;
    double z2;
    int bassRate;
    double liveLinear;
    char prop[40];
};

double clamp01(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

int base64Value(unsigned char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

void decodeBase64(const char *text, std::vector<unsigned char> &out)
{
    int value = 0;
    int bits = -8;
    for (const char *p = text; *p; ++p) {
        if (*p == '=' || *p == '\n' || *p == '\r')
            continue;
        const int digit = base64Value(static_cast<unsigned char>(*p));
        if (digit < 0)
            continue;
        value = (value << 6) | digit;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<unsigned char>((value >> bits) & 0xff));
            bits -= 8;
        }
    }
}

void ensureEnvelope(PrivateData *data, mlt_properties properties)
{
    const char *text = mlt_properties_get(properties, "envelope");
    const int chars = text ? static_cast<int>(strlen(text)) : 0;
    if (chars == data->envelopeChars)
        return;
    data->envelopeChars = chars;
    data->envelope.clear();
    if (text && chars > 0)
        decodeBase64(text, data->envelope);
}

double amountFromPeak(double linear, double thresholdDb)
{
    if (linear <= 0.00001 || thresholdDb >= 0.0)
        return 0.0;
    const double dB = 20.0 * std::log10(linear);
    if (dB < thresholdDb)
        return 0.0;
    return clamp01((dB - thresholdDb) / std::max(1.0, -thresholdDb));
}

double updateLevel(PrivateData *data,
                   mlt_position position,
                   double fps,
                   double amount,
                   double release,
                   double intensity)
{
    amount = clamp01(amount * intensity);
    // Loud hits go full white. Quieter audio stays dim or black.
    if (amount >= 0.72)
        amount = 1.0;
    const double keep = std::pow(clamp01(release), 30.0 / std::max(1.0, fps));
    if (!data->hasPos || position != data->lastPos + 1)
        data->level = amount;
    else
        data->level = std::max(amount, data->level * keep);
    data->lastPos = position;
    data->hasPos = 1;
    return data->level;
}

void setBassLowpass(PrivateData *data, int frequency)
{
    if (frequency < 1000)
        frequency = 48000;
    if (data->bassRate == frequency)
        return;

    constexpr double kPi = 3.14159265358979323846;
    double cutoff = 120.0;
    const double nyquist = frequency * 0.45;
    if (cutoff > nyquist)
        cutoff = nyquist;
    const double q = 0.70710678118;
    const double w0 = 2.0 * kPi * cutoff / frequency;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double cosw = std::cos(w0);
    const double a0 = 1.0 + alpha;
    data->b0 = ((1.0 - cosw) / 2.0) / a0;
    data->b1 = (1.0 - cosw) / a0;
    data->b2 = data->b0;
    data->a1 = (-2.0 * cosw) / a0;
    data->a2 = (1.0 - alpha) / a0;
    data->z1 = 0.0;
    data->z2 = 0.0;
    data->bassRate = frequency;
}

double bassFromAudio(PrivateData *data, mlt_frame frame, mlt_position position, double fps)
{
    mlt_audio_format format = mlt_audio_s16;
    void *buffer = nullptr;
    int frequency = 48000;
    int channels = 2;
    int samples = mlt_audio_calculate_frame_samples(std::max(1.0, fps), frequency, position);
    if (samples < 1)
        samples = 1600;
    if (mlt_frame_get_audio(frame, &buffer, &format, &frequency, &channels, &samples) != 0)
        return 0.0;
    if (!buffer || samples < 1 || channels < 1 || format != mlt_audio_s16)
        return 0.0;

    if (!data->hasPos || position != data->lastPos + 1) {
        data->z1 = 0.0;
        data->z2 = 0.0;
        data->bassRate = 0;
    }
    setBassLowpass(data, frequency);

    const auto *pcm = static_cast<const int16_t *>(buffer);
    double peak = 0.0;
    for (int i = 0; i < samples; ++i) {
        double mono = 0.0;
        for (int channel = 0; channel < channels; ++channel)
            mono += pcm[i * channels + channel] / 32768.0;
        mono /= channels;
        const double output = data->b0 * mono + data->z1;
        data->z1 = data->b1 * mono - data->a1 * output + data->z2;
        data->z2 = data->b2 * mono - data->a2 * output;
        peak = std::max(peak, std::fabs(output));
    }
    return std::min(1.0, peak);
}

void paintWhite(uint8_t *image, int width, int height, double level)
{
    const int mix = static_cast<int>(std::lround(clamp01(level) * 255.0));
    if (mix <= 0)
        return;
    const int keep = 255 - mix;
    const int count = width * height;
    for (int i = 0; i < count; ++i) {
        uint8_t *pixel = image + i * 4;
        pixel[0] = static_cast<uint8_t>((pixel[0] * keep + 255 * mix + 127) / 255);
        pixel[1] = static_cast<uint8_t>((pixel[1] * keep + 255 * mix + 127) / 255);
        pixel[2] = static_cast<uint8_t>((pixel[2] * keep + 255 * mix + 127) / 255);
    }
}

int createImage(mlt_frame frame,
                uint8_t **image,
                mlt_image_format *format,
                int *width,
                int *height,
                int)
{
    mlt_properties properties = MLT_FRAME_PROPERTIES(frame);
    if (*width < 1)
        *width = mlt_properties_get_int(properties, "meta.media.width");
    if (*height < 1)
        *height = mlt_properties_get_int(properties, "meta.media.height");
    if (*width < 1)
        *width = 1920;
    if (*height < 1)
        *height = 1080;
    *format = mlt_image_rgba;
    const int size = (*width) * (*height) * 4;
    *image = static_cast<uint8_t *>(mlt_pool_alloc(size));
    if (!*image)
        return 1;
    memset(*image, 0, size);
    mlt_frame_set_image(frame, *image, size, mlt_pool_release);
    return 0;
}

int filterGetImage(mlt_frame frame,
                   uint8_t **image,
                   mlt_image_format *format,
                   int *width,
                   int *height,
                   int writable)
{
    auto *filter = static_cast<mlt_filter>(mlt_frame_pop_service(frame));
    auto *data = static_cast<PrivateData *>(filter->child);
    mlt_properties filterProperties = MLT_FILTER_PROPERTIES(filter);
    const mlt_position position = mlt_filter_get_position(filter, frame);
    const double fps = std::max(1.0, mlt_profile_fps(mlt_service_profile(MLT_FILTER_SERVICE(filter))));

    mlt_service_lock(MLT_FILTER_SERVICE(filter));
    ensureEnvelope(data, filterProperties);
    double linear = 0.0;
    if (!data->envelope.empty()) {
        if (position >= 0 && position < static_cast<mlt_position>(data->envelope.size()))
            linear = data->envelope[static_cast<size_t>(position)] / 255.0;
    }
    const bool useLiveAudio = data->envelope.empty();
    mlt_service_unlock(MLT_FILTER_SERVICE(filter));

    if (useLiveAudio) {
        if (mlt_properties_exists(MLT_FRAME_PROPERTIES(frame), data->prop))
            linear = mlt_properties_get_double(MLT_FRAME_PROPERTIES(frame), data->prop);
        else if (mlt_properties_get_double(MLT_FRAME_PROPERTIES(frame), "_speed") == 0.0)
            linear = bassFromAudio(data, frame, position, fps);
        else
            linear = data->liveLinear;
    }

    const int animLength = static_cast<int>(mlt_filter_get_length2(filter, frame));
    const int animPosition = static_cast<int>(position);
    mlt_service_lock(MLT_FILTER_SERVICE(filter));
    const double amount = amountFromPeak(
        linear,
        mlt_properties_anim_get_double(filterProperties, "threshold", animPosition, animLength));
    const double level = updateLevel(
        data,
        position,
        fps,
        amount,
        mlt_properties_anim_get_double(filterProperties, "release", animPosition, animLength),
        mlt_properties_anim_get_double(filterProperties, "intensity", animPosition, animLength));
    const double opacity = clamp01(
        mlt_properties_anim_get_double(filterProperties, "opacity", animPosition, animLength));
    mlt_service_unlock(MLT_FILTER_SERVICE(filter));

    *format = mlt_image_rgba;
    const int error = mlt_frame_get_image(frame, image, format, width, height, 1);
    if (error || !*image || *format != mlt_image_rgba)
        return error;
    paintWhite(*image, *width, *height, level * opacity);
    (void) writable;
    return 0;
}

static int filterGetAudio(mlt_frame frame,
                          void **buffer,
                          mlt_audio_format *format,
                          int *frequency,
                          int *channels,
                          int *samples)
{
    auto *filter = static_cast<mlt_filter>(mlt_frame_pop_audio(frame));
    auto *data = static_cast<PrivateData *>(filter->child);
    const int error = mlt_frame_get_audio(frame, buffer, format, frequency, channels, samples);
    if (error || !buffer || !*buffer || !format || !frequency || !channels || !samples)
        return error;
    if (*samples < 1 || *channels < 1)
        return 0;
    if (*format != mlt_audio_s16 && *format != mlt_audio_f32le && *format != mlt_audio_float)
        return 0;

    const mlt_position position = mlt_filter_get_position(filter, frame);
    if (!data->hasPos || position != data->lastPos + 1) {
        data->z1 = 0.0;
        data->z2 = 0.0;
        data->bassRate = 0;
    }
    setBassLowpass(data, *frequency);

    const auto *ints = static_cast<const int16_t *>(*buffer);
    const auto *floats = static_cast<const float *>(*buffer);
    const bool planar = *format == mlt_audio_float;
    double peak = 0.0;
    for (int i = 0; i < *samples; ++i) {
        double mono = 0.0;
        for (int channel = 0; channel < *channels; ++channel) {
            double sample = 0.0;
            if (*format == mlt_audio_s16)
                sample = ints[i * (*channels) + channel] / 32768.0;
            else if (planar)
                sample = floats[channel * (*samples) + i];
            else
                sample = floats[i * (*channels) + channel];
            mono += sample;
        }
        mono /= *channels;
        const double output = data->b0 * mono + data->z1;
        data->z1 = data->b1 * mono - data->a1 * output + data->z2;
        data->z2 = data->b2 * mono - data->a2 * output;
        peak = std::max(peak, std::fabs(output));
    }
    peak = std::min(1.0, peak);
    data->liveLinear = peak;
    mlt_properties_set_double(MLT_FRAME_PROPERTIES(frame), data->prop, peak);
    return 0;
}

mlt_frame filterProcess(mlt_filter filter, mlt_frame frame)
{
    mlt_frame_push_audio(frame, filter);
    mlt_frame_push_audio(frame, reinterpret_cast<void *>(filterGetAudio));
    if (mlt_frame_is_test_card(frame)) {
        mlt_properties frameProperties = MLT_FRAME_PROPERTIES(frame);
        mlt_profile profile = mlt_service_profile(MLT_FILTER_SERVICE(filter));
        mlt_properties_set_int(frameProperties, "progressive", 1);
        mlt_properties_set_double(frameProperties, "aspect_ratio", mlt_profile_sar(profile));
        mlt_properties_set_int(frameProperties, "meta.media.width", profile->width);
        mlt_properties_set_int(frameProperties, "meta.media.height", profile->height);
        mlt_properties_set_int(frameProperties, "test_image", 0);
        mlt_frame_push_get_image(frame, createImage);
    }
    mlt_frame_push_service(frame, filter);
    mlt_frame_push_get_image(frame, filterGetImage);
    return frame;
}

void filterClose(mlt_filter filter)
{
    auto *data = static_cast<PrivateData *>(filter->child);
    delete data;
    filter->child = nullptr;
    filter->close = nullptr;
    filter->parent.close = nullptr;
    mlt_service_close(&filter->parent);
}

} // namespace

extern "C" mlt_filter filter_vjloop_init(mlt_profile profile,
                                         mlt_service_type type,
                                         const char *id,
                                         const void *arg)
{
    (void) profile;
    (void) type;
    (void) id;
    (void) arg;

    mlt_filter filter = mlt_filter_new();
    auto *data = new PrivateData();
    data->envelopeChars = -1;
    data->hasPos = 0;
    data->level = 0.0;
    data->lastPos = 0;
    data->b0 = data->b1 = data->b2 = data->a1 = data->a2 = 0.0;
    data->z1 = data->z2 = 0.0;
    data->bassRate = 0;
    data->liveLinear = 0.0;
    std::snprintf(data->prop, sizeof(data->prop), "vjloop.%p", static_cast<void *>(filter));

    if (filter && data) {
        mlt_properties properties = MLT_FILTER_PROPERTIES(filter);
        mlt_properties_set_int(properties, "_filter_private", 1);
        mlt_properties_set_double(properties, "threshold", -30.0);
        mlt_properties_set_double(properties, "release", 0.45);
        mlt_properties_set_double(properties, "intensity", 1.0);
        mlt_properties_set_double(properties, "opacity", 1.0);
        filter->close = filterClose;
        filter->process = filterProcess;
        filter->child = data;
    } else {
        delete data;
        if (filter)
            mlt_filter_close(filter);
        filter = nullptr;
    }
    return filter;
}
