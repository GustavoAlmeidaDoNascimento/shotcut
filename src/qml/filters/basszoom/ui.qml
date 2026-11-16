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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Shotcut.Controls as Shotcut

Item {
    property bool blockUpdate: true
    property string sensitivityParam: 'sensitivity'
    property string intensityParam: 'intensity'
    property double sensitivityDefault: 65
    property double intensityDefault: 100

    function setControls() {
        blockUpdate = true;
        sensitivitySlider.value = filter.getDouble(sensitivityParam) * 100;
        var storedIntensity = filter.get(intensityParam);
        intensitySlider.value = storedIntensity === '' ? intensityDefault : filter.getDouble(intensityParam) * 100;
        blockUpdate = false;
    }

    width: 400
    height: 230
    Component.onCompleted: {
        if (filter.isNew) {
            filter.set(sensitivityParam, sensitivityDefault / 100);
            filter.set(intensityParam, intensityDefault / 100);
            filter.savePreset(preset.parameters);
        }
        setControls();
    }

    Connections {
        function onChanged() {
            setControls();
        }

        function onPropertyChanged() {
            setControls();
        }

        target: filter
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 8
        columns: 3

        Label {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr('Add this filter to Timeline > Output. It measures the bass of the whole timeline and zooms the picture in while that bass is loud. Sensitivity sets how far the same bass pushes the zoom.')
        }

        Label {
            text: qsTr('Preset')
            Layout.alignment: Qt.AlignRight
        }

        Shotcut.Preset {
            id: preset

            Layout.columnSpan: 2
            parameters: [sensitivityParam, intensityParam]
            onPresetSelected: setControls()
        }

        Label {
            text: qsTr('Sensitivity')
            Layout.alignment: Qt.AlignRight
            Shotcut.HoverTip {
                text: qsTr('0 stays still. Higher values zoom farther on the same bass.')
            }
        }

        Shotcut.SliderSpinner {
            id: sensitivitySlider

            minimumValue: 0
            maximumValue: 100
            decimals: 0
            suffix: ' %'
            onValueChanged: {
                if (!blockUpdate)
                    filter.set(sensitivityParam, value / 100);
            }
        }

        Shotcut.UndoButton {
            onClicked: sensitivitySlider.value = sensitivityDefault
        }

        Label {
            text: qsTr('Intensity')
            Layout.alignment: Qt.AlignRight
            Shotcut.HoverTip {
                text: qsTr('100% keeps the full zoom. Lower values make the same bass zoom less.')
            }
        }

        Shotcut.SliderSpinner {
            id: intensitySlider

            minimumValue: 0
            maximumValue: 100
            decimals: 0
            suffix: ' %'
            onValueChanged: {
                if (!blockUpdate)
                    filter.set(intensityParam, value / 100);
            }
        }

        Shotcut.UndoButton {
            onClicked: intensitySlider.value = intensityDefault
        }

        Item {
            Layout.fillHeight: true
            Layout.columnSpan: 3
        }
    }
}
