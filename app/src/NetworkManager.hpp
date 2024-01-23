#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>

#include "Network.hpp"

class NetworkManager : QObject
{
    Q_OBJECT
public:
    NetworkManager(QString netsXmlPath);
private:
    QMap<QString, Network*> m_networks;
};

#endif // NETWORKMANAGER_HPP