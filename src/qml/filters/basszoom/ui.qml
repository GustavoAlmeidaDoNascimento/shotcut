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
    property double sensitivityDefault: 65

    function setControls() {
        blockUpdate = true;
        sensitivitySlider.value = filter.getDouble(sensitivityParam) * 100;
        blockUpdate = false;
    }

    width: 400
    height: 180
    Component.onCompleted: {
        if (filter.isNew) {
            filter.set(sensitivityParam, sensitivityDefault / 100);
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
            parameters: [sensitivityParam]
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

        Item {
            Layout.fillHeight: true
            Layout.columnSpan: 3
        }
    }
}
