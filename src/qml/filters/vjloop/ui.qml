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
    property string thresholdParam: 'threshold'
    property string releaseParam: 'release'
    property string intensityParam: 'intensity'
    property string opacityParam: 'opacity'
    property double thresholdDefault: -30
    property double releaseDefault: 45
    property double intensityDefault: 100
    property double opacityDefault: 100

    function setControls() {
        blockUpdate = true;
        thresholdSlider.value = filter.getDouble(thresholdParam);
        releaseSlider.value = filter.getDouble(releaseParam) * 100;
        intensitySlider.value = filter.getDouble(intensityParam) * 100;
        opacitySlider.value = filter.getDouble(opacityParam) * 100;
        blockUpdate = false;
    }

    width: 420
    height: 280
    Component.onCompleted: {
        if (filter.isNew) {
            filter.set(thresholdParam, thresholdDefault);
            filter.set(releaseParam, releaseDefault / 100);
            filter.set(intensityParam, intensityDefault / 100);
            filter.set(opacityParam, opacityDefault / 100);
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
            text: qsTr('Timeline > Add Light Layer reads the audio already on the timeline and adds a video track of white flashes on black. These sliders reshape that flash without reading the audio again. After you change the music, run Add Light Layer again and remove the old light track.')
        }

        Label {
            text: qsTr('Preset')
            Layout.alignment: Qt.AlignRight
        }

        Shotcut.Preset {
            id: preset

            Layout.columnSpan: 2
            parameters: [thresholdParam, releaseParam, intensityParam, opacityParam]
            onPresetSelected: setControls()
        }

        Label {
            text: qsTr('Threshold')
            Layout.alignment: Qt.AlignRight
            Shotcut.HoverTip {
                text: qsTr('Closer to 0 reacts only to the loudest hits. More negative reacts to quieter sound.')
            }
        }

        Shotcut.SliderSpinner {
            id: thresholdSlider

            minimumValue: -60
            maximumValue: 0
            decimals: 1
            suffix: ' dB'
            onValueChanged: {
                if (!blockUpdate)
                    filter.set(thresholdParam, value);
            }
        }

        Shotcut.UndoButton {
            onClicked: thresholdSlider.value = thresholdDefault
        }

        Label {
            text: qsTr('Release')
            Layout.alignment: Qt.AlignRight
            Shotcut.HoverTip {
                text: qsTr('How quickly a flash fades back to black.')
            }
        }

        Shotcut.SliderSpinner {
            id: releaseSlider

            minimumValue: 0
            maximumValue: 100
            decimals: 0
            suffix: ' %'
            onValueChanged: {
                if (!blockUpdate)
                    filter.set(releaseParam, value / 100);
            }
        }

        Shotcut.UndoButton {
            onClicked: releaseSlider.value = releaseDefault
        }

        Label {
            text: qsTr('Intensity')
            Layout.alignment: Qt.AlignRight
            Shotcut.HoverTip {
                text: qsTr('Scales every flash. 100% lets the loud hits go full white.')
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

        Label {
            text: qsTr('Opacity')
            Layout.alignment: Qt.AlignRight
            Shotcut.HoverTip {
                text: qsTr('100% is solid white on black. Lower values make the flashes dimmer.')
            }
        }

        Shotcut.SliderSpinner {
            id: opacitySlider

            minimumValue: 0
            maximumValue: 100
            decimals: 0
            suffix: ' %'
            onValueChanged: {
                if (!blockUpdate)
                    filter.set(opacityParam, value / 100);
            }
        }

        Shotcut.UndoButton {
            onClicked: opacitySlider.value = opacityDefault
        }

        Item {
            Layout.fillHeight: true
            Layout.columnSpan: 3
        }
    }
}
