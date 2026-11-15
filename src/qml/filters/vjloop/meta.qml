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
}
