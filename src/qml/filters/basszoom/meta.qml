import QtQuick
import org.shotcut.qml

Metadata {
    type: Metadata.Filter
    objectName: 'bassZoom'
    name: qsTr('Bass Zoom')
    keywords: qsTr('bass zoom kick pump scale music dj', 'search keywords for the Bass Zoom video filter') + ' grave zoom vj'
    mlt_service: 'basszoom'
    qml: 'ui.qml'
    icon: 'icon.webp'
    allowMultiple: false
}
