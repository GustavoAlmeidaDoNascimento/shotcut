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

Shotcut.KeyframableFilter {
    property string thresholdParam: 'threshold'
    property string releaseParam: 'release'
    property string intensityParam: 'intensity'
    property string opacityParam: 'opacity'
    property double thresholdDefault: -30
    property double releaseDefault: 45
    property double intensityDefault: 100
    property double opacityDefault: 100

    function sliderToProperty(slider) {
        return slider === thresholdSlider ? slider.value : slider.value / 100;
    }

    function setControls() {
        var position = getPosition();
        blockUpdate = true;
        thresholdSlider.value = filter.getDouble(thresholdParam, position);
        thresholdKeyframesButton.checked = filter.animateIn <= 0 && filter.animateOut <= 0 && filter.keyframeCount(thresholdParam) > 0;
        releaseSlider.value = filter.getDouble(releaseParam, position) * 100;
        releaseKeyframesButton.checked = filter.animateIn <= 0 && filter.animateOut <= 0 && filter.keyframeCount(releaseParam) > 0;
        intensitySlider.value = filter.getDouble(intensityParam, position) * 100;
        intensityKeyframesButton.checked = filter.animateIn <= 0 && filter.animateOut <= 0 && filter.keyframeCount(intensityParam) > 0;
        opacitySlider.value = filter.getDouble(opacityParam, position) * 100;
        opacityKeyframesButton.checked = filter.animateIn <= 0 && filter.animateOut <= 0 && filter.keyframeCount(opacityParam) > 0;
        blockUpdate = false;
        enableControls(isSimpleKeyframesActive());
    }

    function enableControls(enabled) {
        thresholdSlider.enabled = releaseSlider.enabled = intensitySlider.enabled = opacitySlider.enabled = enabled;
    }

    function updateSimpleKeyframes() {
        setControls();
        updateFilter(thresholdParam, sliderToProperty(thresholdSlider), thresholdKeyframesButton, null);
        updateFilter(releaseParam, sliderToProperty(releaseSlider), releaseKeyframesButton, null);
        updateFilter(intensityParam, sliderToProperty(intensitySlider), intensityKeyframesButton, null);
        updateFilter(opacityParam, sliderToProperty(opacitySlider), opacityKeyframesButton, null);
    }

    keyframableParameters: [thresholdParam, releaseParam, intensityParam, opacityParam]
    startValues: [thresholdDefault, releaseDefault / 100, intensityDefault / 100, opacityDefault / 100]
    middleValues: [thresholdDefault, releaseDefault / 100, intensityDefault / 100, opacityDefault / 100]
    endValues: [thresholdDefault, releaseDefault / 100, intensityDefault / 100, opacityDefault / 100]
    width: 480
    height: 320
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

    GridLayout {
        anchors.fill: parent
        anchors.margins: 8
        columns: 4

        Label {
            Layout.columnSpan: 4
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr('The white flash is drawn on top of this video. Between hits the picture stays as it is.')
        }

        Label {
            text: qsTr('Preset')
            Layout.alignment: Qt.AlignRight
        }

        Shotcut.Preset {
            id: preset

            Layout.columnSpan: 3
            parameters: [thresholdParam, releaseParam, intensityParam, opacityParam]
            onBeforePresetLoaded: resetSimpleKeyframes()
            onPresetSelected: {
                setControls();
                initializeSimpleKeyframes();
            }
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
            onValueChanged: updateFilter(thresholdParam, sliderToProperty(thresholdSlider), thresholdKeyframesButton, getPosition())
        }

        Shotcut.UndoButton {
            onClicked: thresholdSlider.value = thresholdDefault
        }

        Shotcut.KeyframesButton {
            id: thresholdKeyframesButton

            onToggled: {
                enableControls(true);
                toggleKeyframes(checked, thresholdParam, sliderToProperty(thresholdSlider));
            }
        }

        Label {
            text: qsTr('Release')
            Layout.alignment: Qt.AlignRight
            Shotcut.HoverTip {
                text: qsTr('How quickly a flash fades so the picture shows through again.')
            }
        }

        Shotcut.SliderSpinner {
            id: releaseSlider

            minimumValue: 0
            maximumValue: 100
            decimals: 0
            suffix: ' %'
            onValueChanged: updateFilter(releaseParam, sliderToProperty(releaseSlider), releaseKeyframesButton, getPosition())
        }

        Shotcut.UndoButton {
            onClicked: releaseSlider.value = releaseDefault
        }

        Shotcut.KeyframesButton {
            id: releaseKeyframesButton

            onToggled: {
                enableControls(true);
                toggleKeyframes(checked, releaseParam, sliderToProperty(releaseSlider));
            }
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
            onValueChanged: updateFilter(intensityParam, sliderToProperty(intensitySlider), intensityKeyframesButton, getPosition())
        }

        Shotcut.UndoButton {
            onClicked: intensitySlider.value = intensityDefault
        }

        Shotcut.KeyframesButton {
            id: intensityKeyframesButton

            onToggled: {
                enableControls(true);
                toggleKeyframes(checked, intensityParam, sliderToProperty(intensitySlider));
            }
        }

        Label {
            text: qsTr('Opacity')
            Layout.alignment: Qt.AlignRight
            Shotcut.HoverTip {
                text: qsTr('100% lets a loud hit cover the picture in white. Lower values keep more of the video visible.')
            }
        }

        Shotcut.SliderSpinner {
            id: opacitySlider

            minimumValue: 0
            maximumValue: 100
            decimals: 0
            suffix: ' %'
            onValueChanged: updateFilter(opacityParam, sliderToProperty(opacitySlider), opacityKeyframesButton, getPosition())
        }

        Shotcut.UndoButton {
            onClicked: opacitySlider.value = opacityDefault
        }

        Shotcut.KeyframesButton {
            id: opacityKeyframesButton

            onToggled: {
                enableControls(true);
                toggleKeyframes(checked, opacityParam, sliderToProperty(opacitySlider));
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.columnSpan: 4
        }
    }

    Connections {
        function onChanged() {
            setControls();
        }

        function onInChanged() {
            updateSimpleKeyframes();
        }

        function onOutChanged() {
            updateSimpleKeyframes();
        }

        function onAnimateInChanged() {
            updateSimpleKeyframes();
        }

        function onAnimateOutChanged() {
            updateSimpleKeyframes();
        }

        function onPropertyChanged(name) {
            setControls();
        }

        target: filter
    }

    Connections {
        function onPositionChanged() {
            setControls();
        }

        target: producer
    }
}
