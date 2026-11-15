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

#include "basszoomfilter.h"

#include <framework/mlt.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kBassHz = 120.0;
constexpr double kMaxZoom = 0.40;
constexpr double kRelease = 0.78;
constexpr double kAttack = 0.42;

struct Biquad
{
    double b0;
    double b1;
    double b2;
    double a1;
    double a2;
    double z1;
    double z2;
    int frequency;
};

struct PrivateData
{
    Biquad bass;
    mlt_filter affine;
    int hasPos;
    double level;
    mlt_position lastPos;
    char prop[40];
};

double clamp01(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

void setLowpass(Biquad *biquad, int frequency)
{
    if (frequency < 1000)
        frequency = 48000;
    if (biquad->frequency == frequency)
        return;

    double cutoff = kBassHz;
    const double nyquist = frequency * 0.45;
    if (cutoff > nyquist)
        cutoff = nyquist;

    const double q = 0.70710678118;
    const double w0 = 2.0 * kPi * cutoff / frequency;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double cosw = std::cos(w0);
    const double a0 = 1.0 + alpha;
    biquad->b0 = ((1.0 - cosw) / 2.0) / a0;
    biquad->b1 = (1.0 - cosw) / a0;
    biquad->b2 = biquad->b0;
    biquad->a1 = (-2.0 * cosw) / a0;
    biquad->a2 = (1.0 - alpha) / a0;
    biquad->z1 = 0.0;
    biquad->z2 = 0.0;
    biquad->frequency = frequency;
}

double filterSample(Biquad *biquad, double sample)
{
    const double output = biquad->b0 * sample + biquad->z1;
    biquad->z1 = biquad->b1 * sample - biquad->a1 * output + biquad->z2;
    biquad->z2 = biquad->b2 * sample - biquad->a2 * output;
    return output;
}

double sampleAt(const void *samples,
                mlt_audio_format format,
                int index,
                int channel,
                int channels,
                int sampleCount)
{
    switch (format) {
    case mlt_audio_s16:
        return static_cast<const int16_t *>(samples)[index * channels + channel] / 32768.0;
    case mlt_audio_s32le:
        return static_cast<const int32_t *>(samples)[index * channels + channel] / 2147483648.0;
    case mlt_audio_s32:
        return static_cast<const int32_t *>(samples)[channel * sampleCount + index] / 2147483648.0;
    case mlt_audio_f32le:
        return static_cast<const float *>(samples)[index * channels + channel];
    case mlt_audio_float:
        return static_cast<const float *>(samples)[channel * sampleCount + index];
    default:
        return 0.0;
    }
}

double measureBass(Biquad *biquad,
                   const void *samples,
                   mlt_audio_format format,
                   int sampleCount,
                   int channels,
                   double *signal)
{
    *signal = 0.0;
    if (!samples || sampleCount < 1 || channels < 1)
        return 0.0;
    if (format != mlt_audio_s16 && format != mlt_audio_s32 && format != mlt_audio_s32le
        && format != mlt_audio_float && format != mlt_audio_f32le)
        return 0.0;

    double peak = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        double mono = 0.0;
        for (int channel = 0; channel < channels; ++channel) {
            const double sample = sampleAt(samples, format, i, channel, channels, sampleCount);
            *signal = std::max(*signal, std::fabs(sample));
            mono += sample;
        }
        mono /= channels;
        peak = std::max(peak, std::fabs(filterSample(biquad, mono)));
    }
    return std::min(1.0, peak);
}

double smoothZoom(PrivateData *data, mlt_position position, double fps, double target)
{
    const mlt_position gap = data->hasPos ? position - data->lastPos : 0;
    const bool continuous = data->hasPos && gap > 0 && gap <= 5;
    if (!continuous) {
        data->level = target;
    } else {
        const double keep = std::pow(kRelease, 30.0 / std::max(1.0, fps));
        for (mlt_position step = 0; step < gap; ++step) {
            if (target > data->level)
                data->level += (target - data->level) * kAttack;
            else
                data->level = std::max(target, data->level * keep);
        }
    }
    data->lastPos = position;
    data->hasPos = 1;
    return data->level;
}

void applyZoom(PrivateData *data, mlt_frame frame, double zoom)
{
    if (!data->affine || zoom <= 0.004)
        return;
    const double scale = 1.0 / (1.0 + zoom);
    mlt_properties properties = MLT_FILTER_PROPERTIES(data->affine);
    mlt_properties_set_double(properties, "transition.scale_x", scale);
    mlt_properties_set_double(properties, "transition.scale_y", scale);
    mlt_properties_set_double(properties, "transition.ox", 0);
    mlt_properties_set_double(properties, "transition.oy", 0);
    if (!mlt_properties_get(MLT_FRAME_PROPERTIES(frame), "consumer.rescale"))
        mlt_properties_set(MLT_FRAME_PROPERTIES(frame), "consumer.rescale", "bilinear");
    mlt_filter_process(data->affine, frame);
}

} // namespace

static int filterGetAudio(mlt_frame frame,
                          void **buffer,
                          mlt_audio_format *format,
                          int *frequency,
                          int *channels,
                          int *samples)
{
    auto *filter = static_cast<mlt_filter>(mlt_frame_pop_audio(frame));
    auto *data = static_cast<PrivateData *>(filter->child);
    const mlt_position position = mlt_frame_get_position(frame);
    const double fps = std::max(1.0, mlt_profile_fps(mlt_service_profile(MLT_FILTER_SERVICE(filter))));

    const int error = mlt_frame_get_audio(frame, buffer, format, frequency, channels, samples);
    if (error || !format || !frequency || !channels || !samples)
        return error;

    const double sensitivity = clamp01(
        mlt_properties_get_double(MLT_FILTER_PROPERTIES(filter), "sensitivity"));

    mlt_service_lock(MLT_FILTER_SERVICE(filter));
    const bool redraw = data->hasPos && position == data->lastPos;
    const bool jumped = !data->hasPos || position < data->lastPos || position > data->lastPos + 5;
    double signal = 0.0;
    double zoom = data->level;
    if (!(buffer == nullptr || *buffer == nullptr) || !redraw) {
        if (jumped) {
            data->bass.z1 = 0.0;
            data->bass.z2 = 0.0;
        }
        setLowpass(&data->bass, *frequency);
        const double bass = measureBass(&data->bass,
                                        *buffer,
                                        *format,
                                        *samples,
                                        *channels,
                                        &signal);
        double target = 0.0;
        if (signal < 1e-7 && redraw)
            target = data->level;
        else
            target = std::min(kMaxZoom, bass * sensitivity * 1.5);
        zoom = smoothZoom(data, position, fps, target);
    }
    mlt_service_unlock(MLT_FILTER_SERVICE(filter));

    mlt_properties_set_double(MLT_FRAME_PROPERTIES(frame), data->prop, zoom);
    return 0;
}

static int filterGetImage(mlt_frame frame,
                          uint8_t **image,
                          mlt_image_format *format,
                          int *width,
                          int *height,
                          int writable)
{
    auto *filter = static_cast<mlt_filter>(mlt_frame_pop_service(frame));
    auto *data = static_cast<PrivateData *>(filter->child);
    mlt_properties frameProperties = MLT_FRAME_PROPERTIES(frame);

    if (!mlt_properties_exists(frameProperties, data->prop)) {
        mlt_audio_format audioFormat = mlt_audio_float;
        void *buffer = nullptr;
        int rate = 48000;
        int channelCount = 2;
        int sampleCount = 1600;
        mlt_frame_get_audio(frame, &buffer, &audioFormat, &rate, &channelCount, &sampleCount);
    }

    const double zoom = mlt_properties_get_double(frameProperties, data->prop);
    mlt_service_lock(MLT_FILTER_SERVICE(filter));
    applyZoom(data, frame, zoom);
    const int error = mlt_frame_get_image(frame, image, format, width, height, writable);
    mlt_service_unlock(MLT_FILTER_SERVICE(filter));
    return error;
}

static mlt_frame filterProcess(mlt_filter filter, mlt_frame frame)
{
    mlt_frame_push_audio(frame, filter);
    mlt_frame_push_audio(frame, reinterpret_cast<void *>(filterGetAudio));
    mlt_frame_push_service(frame, filter);
    mlt_frame_push_get_image(frame, filterGetImage);
    return frame;
}

static void filterClose(mlt_filter filter)
{
    auto *data = static_cast<PrivateData *>(filter->child);
    if (data) {
        mlt_filter_close(data->affine);
        delete data;
    }
    filter->child = nullptr;
    filter->close = nullptr;
    filter->parent.close = nullptr;
    mlt_service_close(&filter->parent);
}

extern "C" mlt_filter filter_basszoom_init(mlt_profile profile,
                                           mlt_service_type type,
                                           const char *id,
                                           const void *arg)
{
    (void) type;
    (void) id;
    (void) arg;

    mlt_filter filter = mlt_filter_new();
    auto *data = new PrivateData();
    data->bass = {};
    data->affine = mlt_factory_filter(profile, "affine", "colour:0x00000000");
    data->hasPos = 0;
    data->level = 0.0;
    data->lastPos = 0;
    std::snprintf(data->prop, sizeof(data->prop), "basszoom.%p", static_cast<void *>(filter));

    if (filter && data->affine) {
        mlt_properties properties = MLT_FILTER_PROPERTIES(filter);
        mlt_properties_set_int(properties, "_filter_private", 1);
        mlt_properties_set_double(properties, "sensitivity", 0.65);
        filter->close = filterClose;
        filter->process = filterProcess;
        filter->child = data;
    } else {
        if (data->affine)
            mlt_filter_close(data->affine);
        delete data;
        if (filter)
            mlt_filter_close(filter);
        filter = nullptr;
    }
    return filter;
}
