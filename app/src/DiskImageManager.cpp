#include "DiskImageManager.hpp"

#include <QtCore/QFile>
#include <QtCore/QDebug>

#include "third-party/nlohmann/json.hpp"

#include "Application.hpp"

using namespace nlohmann;

bool DiskImageManager::m_initializated = false;
QMap<QString, DiskImage*> DiskImageManager::m_diskImages = QMap<QString, DiskImage*>();


void DiskImageManager::Initializate(){
    assert(m_initializated == false);

    QString disksJsonPath = Application::applicationDirPath() + "/config.json";
    QFile disksJsonFile(disksJsonPath);
    
    if(disksJsonFile.open(QIODevice::ReadOnly) == false){
        QString exceptionStr = "Failed to open \"" + disksJsonPath + "\": ";
        exceptionStr += disksJsonFile.errorString();
        throw DiskImageManagerException(exceptionStr);
    }

    QByteArray ba = disksJsonFile.readAll();
    json disksJson;
    
    try{
        disksJson = json::parse(ba.data());
    }
    catch(json::exception &e){
        QString exceptionStr = "Failed to parse \"" + disksJsonPath + "\": ";
        exceptionStr += e.what();
        throw DiskImageManagerException(exceptionStr);       
    }
    

    for(auto image : disksJson["images"]){
        DiskImage* diskImage = new DiskImage();
        try{
            diskImage->name = QString::fromStdString(image["image-name"]);
            diskImage->path = QString::fromStdString(image["path"]);
        }
        catch(json::exception &e){
            QString exceptionStr = "Failed to parse \"" + disksJsonPath + "\": ";
            exceptionStr += e.what();
            throw DiskImageManagerException(exceptionStr);   
        }

        QFileInfo diskFileInfo(Application::applicationDirPath() + "/" + diskImage->path);
        if(diskFileInfo.exists() == false){
            QString exceptionStr = "Failed to parse \"" + disksJsonPath + "\": ";
            exceptionStr += "Image \"" + diskImage->path + "\" does not exist.";
            throw DiskImageManagerException(exceptionStr);  
        }
        diskImage->path = diskFileInfo.absoluteFilePath();

        if(image["init-sys"].is_string()){
            diskImage->initSysPath = QString::fromStdString(image["init-sys"]);
        }
        else{
            diskImage->initSysPath = "/bin/sh";
        }

        if(m_diskImages.contains(diskImage->name)){
            QString exceptionStr = "Failed to parse \"" + disksJsonPath + "\": ";
            exceptionStr += "Image with name \"" + diskImage->name + "\" already exist.";
            throw DiskImageManagerException(exceptionStr);   
        }
        m_diskImages[diskImage->name] = diskImage;
    }
    
    disksJsonFile.close();
    
    m_initializated = true;
}

DiskImage* DiskImageManager::getDiskImage(QString name){
    assert(m_initializated == true);
    return m_diskImages.value(name, nullptr);
}

void DiskImageManager::CleanUp(){
    for(auto diskImage : m_diskImages){
        delete diskImage;
    }
    m_initializated = false;
}