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

#ifndef LIGHTLAYER_H
#define LIGHTLAYER_H

#include <QByteArray>
#include <QString>

class QWidget;

namespace LightLayer {

enum class AnalyzeStatus { Ok, Canceled, NoAudio, Failed };

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
                      QWidget *parent);

} // namespace LightLayer

#endif
