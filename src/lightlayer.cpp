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

#include "lightlayer.h"

#include "Logger.h"

#include <MltChain.h>
#include <MltFilter.h>
#include <MltPlaylist.h>
#include <MltProducer.h>
#include <MltProfile.h>
#include <MltTractor.h>
#include <QCoreApplication>
#include <QProgressDialog>
#include <QThread>

#include <atomic>
#include <cmath>
#include <memory>
#include <thread>
#include <utility>

namespace {

void disableVideo(Mlt::Producer &producer, int depth)
{
    if (!producer.is_valid() || depth > 64)
        return;
    const QString service = QString::fromUtf8(producer.get("mlt_service"));
    if (service.startsWith(QStringLiteral("avformat")) || service == QStringLiteral("timewarp"))
        producer.set("video_index", -1);

    if (producer.type() == mlt_service_tractor_type) {
        Mlt::Tractor tractor(producer);
        const int count = tractor.count();
        for (int i = 0; i < count; ++i) {
            std::unique_ptr<Mlt::Producer> track(tractor.track(i));
            if (track && track->is_valid())
                disableVideo(*track, depth + 1);
        }
    } else if (producer.type() == mlt_service_playlist_type) {
        Mlt::Playlist playlist(producer);
        for (int i = 0; i < playlist.count(); ++i) {
            if (playlist.is_blank(i))
                continue;
            std::unique_ptr<Mlt::Producer> clip(playlist.get_clip(i));
            if (clip && clip->is_valid())
                disableVideo(*clip, depth + 1);
        }
    } else if (producer.type() == mlt_service_chain_type) {
        Mlt::Chain chain(producer);
        Mlt::Producer source = chain.get_source();
        if (source.is_valid() && source.get_producer() != producer.get_producer())
            disableVideo(source, depth + 1);
    } else {
        Mlt::Producer &parent = producer.parent();
        if (parent.is_valid() && parent.get_producer() != producer.get_producer())
            disableVideo(parent, depth + 1);
    }
}

void configureProfile(Mlt::Profile &profile,
                      int width,
                      int height,
                      int sampleAspectNum,
                      int sampleAspectDen,
                      int displayAspectNum,
                      int displayAspectDen,
                      int progressive,
                      int colorspace,
                      int frameRateNum,
                      int frameRateDen)
{
    profile.set_explicit(1);
    profile.set_width(width);
    profile.set_height(height);
    profile.set_sample_aspect(sampleAspectNum, sampleAspectDen);
    profile.set_display_aspect(displayAspectNum, displayAspectDen);
    profile.set_progressive(progressive);
    profile.set_colorspace(colorspace);
    profile.set_frame_rate(frameRateNum, frameRateDen);
}

} // namespace

namespace LightLayer {

AnalyzeStatus analyze(const QString &xml,
                      int width,
                      int height,
                      int sampleAspectNum,
                      int sampleAspectDen,
                      int displayAspectNum,
                      int displayAspectDen,
                      int progressive,
                      int colorspace,
                      int frameRateNum,
                      int frameRateDen,
                      int length,
                      QByteArray &envelope,
                      QWidget *parent)
{
    if (xml.isEmpty() || length < 1)
        return AnalyzeStatus::Failed;

    QProgressDialog dialog(QCoreApplication::translate("TimelineDock",
                                                       "Reading the timeline audio…"),
                           QCoreApplication::translate("TimelineDock", "Cancel"),
                           0,
                           length,
                           parent);
    dialog.setWindowTitle(QCoreApplication::translate("TimelineDock", "Light layer"));
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumDuration(0);
    dialog.setValue(0);

    std::atomic<int> progress{0};
    std::atomic<bool> cancel{false};
    std::atomic<bool> finished{false};
    AnalyzeStatus status = AnalyzeStatus::Failed;
    QByteArray levels;
    levels.reserve(length);

    std::thread worker([&]() {
        Mlt::Profile profile;
        configureProfile(profile,
                         width,
                         height,
                         sampleAspectNum,
                         sampleAspectDen,
                         displayAspectNum,
                         displayAspectDen,
                         progressive,
                         colorspace,
                         frameRateNum,
                         frameRateDen);
        Mlt::Producer producer(profile, "xml-string", xml.toUtf8().constData());
        if (!producer.is_valid()) {
            status = AnalyzeStatus::Failed;
            finished = true;
            return;
        }
        disableVideo(producer, 0);
        Mlt::Filter channels(profile, "audiochannels");
        Mlt::Filter converter(profile, "audioconvert");
        Mlt::Filter audioLevel(profile, "audiolevel");
        if (channels.is_valid())
            producer.attach(channels);
        if (converter.is_valid())
            producer.attach(converter);
        if (audioLevel.is_valid())
            producer.attach(audioLevel);
        producer.seek(0);

        const double fps = profile.fps() > 0.0 ? profile.fps() : 30.0;
        double b0 = 0.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double z1 = 0.0;
        double z2 = 0.0;
        int bassRate = 0;
        unsigned char peakByte = 0;
        for (int i = 0; i < length; ++i) {
            if (cancel.load()) {
                status = AnalyzeStatus::Canceled;
                finished = true;
                return;
            }
            std::unique_ptr<Mlt::Frame> frame(producer.get_frame());
            double peak = 0.0;
            if (frame && frame->is_valid() && !frame->get_int("test_audio")) {
                mlt_audio_format format = mlt_audio_s16;
                int frequency = 48000;
                int channelCount = 2;
                int samples = mlt_audio_calculate_frame_samples(fps, frequency, i);
                const int16_t *pcm = static_cast<const int16_t *>(
                    frame->get_audio(format, frequency, channelCount, samples));
                if (pcm && samples > 0 && channelCount > 0 && frequency >= 1000) {
                    if (bassRate != frequency) {
                        constexpr double kPi = 3.14159265358979323846;
                        const double q = 0.70710678118;
                        const double w0 = 2.0 * kPi * 120.0 / frequency;
                        const double alpha = std::sin(w0) / (2.0 * q);
                        const double cosw = std::cos(w0);
                        const double a0 = 1.0 + alpha;
                        b0 = ((1.0 - cosw) / 2.0) / a0;
                        b1 = (1.0 - cosw) / a0;
                        b2 = b0;
                        a1 = (-2.0 * cosw) / a0;
                        a2 = (1.0 - alpha) / a0;
                        z1 = 0.0;
                        z2 = 0.0;
                        bassRate = frequency;
                    }
                    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex) {
                        double mono = 0.0;
                        for (int channel = 0; channel < channelCount; ++channel)
                            mono += pcm[sampleIndex * channelCount + channel] / 32768.0;
                        mono /= channelCount;
                        const double output = b0 * mono + z1;
                        z1 = b1 * mono - a1 * output + z2;
                        z2 = b2 * mono - a2 * output;
                        peak = std::max(peak, std::fabs(output));
                    }
                    if (peak > 1.0)
                        peak = 1.0;
                }
            }
            if (peak < 0.0)
                peak = 0.0;
            if (peak > 1.0)
                peak = 1.0;
            const auto byte = static_cast<unsigned char>(std::lround(peak * 255.0));
            levels.append(static_cast<char>(byte));
            if (byte > peakByte)
                peakByte = byte;
            if ((i & 15) == 0)
                progress.store(i + 1);
        }
        progress.store(length);
        status = peakByte > 0 ? AnalyzeStatus::Ok : AnalyzeStatus::NoAudio;
        finished = true;
    });

    while (!finished.load()) {
        dialog.setValue(progress.load());
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (dialog.wasCanceled())
            cancel.store(true);
        QThread::msleep(40);
    }
    worker.join();
    dialog.setValue(length);

    if (status == AnalyzeStatus::Ok)
        envelope = std::move(levels);
    else
        envelope.clear();
    LOG_DEBUG() << "light layer analysis" << static_cast<int>(status) << envelope.size();
    return status;
}

} // namespace LightLayer
