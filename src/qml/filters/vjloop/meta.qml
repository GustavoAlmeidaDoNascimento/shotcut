import QtQuick
import org.shotcut.qml

Metadata {
    type: Metadata.Filter
    objectName: 'vjLoop'
    name: qsTr('Light Layer')
    keywords: qsTr('white flash strobe light dj vj audio music', 'search keywords for the Light Layer video filter') + ' vj loop luz'
    mlt_service: 'vjloop'
    qml: 'ui.qml'
    icon: 'icon.webp'
    allowMultiple: false

    keyframes {
        allowAnimateIn: true
        allowAnimateOut: true
        simpleProperties: ['threshold', 'release', 'intensity', 'opacity']
        parameters: [
            Parameter {
                name: qsTr('Threshold')
                property: 'threshold'
                isCurve: true
                minimum: -60
                maximum: 0
                units: 'dB'
            },
            Parameter {
                name: qsTr('Release')
                property: 'release'
                isCurve: true
                minimum: 0
                maximum: 1
            },
            Parameter {
                name: qsTr('Intensity')
                property: 'intensity'
                isCurve: true
                minimum: 0
                maximum: 1
            },
            Parameter {
                name: qsTr('Opacity')
                property: 'opacity'
                isCurve: true
                minimum: 0
                maximum: 1
            }
        ]
    }
}
