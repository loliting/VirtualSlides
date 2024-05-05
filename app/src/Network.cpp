#include "Network.hpp"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QChar>

using namespace nlohmann;

static quint32 portCounter = 3000;
static quint32 macAddressCounter = 0;

const QString Network::macAddressOUI = QStringLiteral("12:34:56");

Network::Network(json &netObject) {
    m_id = QString::fromStdString(netObject["id"]);
    
    if(netObject.contains("vmId"))
        m_vmId = QString::fromStdString(netObject["vmId"]);
    else
        m_vmId = nullptr;
    
    if(netObject.contains("wan"))
        m_wan = netObject["wan"];
    else
        m_wan = false;

    m_mcastPort = portCounter++;
}

QString Network::generateNewMacAddress() {
    assert(++macAddressCounter <= 0xFFFFFF);

    return (macAddressOUI + ":%1:%2:%3")
        .arg((macAddressCounter & 0xFF0000) >> 16, 2, 16, (QChar)u'0')
        .arg((macAddressCounter & 0x00FF00) >> 8, 2, 16, (QChar)u'0')
        .arg(macAddressCounter & 0x0000FF, 2, 16, (QChar)u'0');
}