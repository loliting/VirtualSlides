#include "Config.hpp"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDebug>
#include <qprocessordetection.h>

#include "third-party/nlohmann/json.hpp"

#include "Application.hpp"

using namespace nlohmann;

bool Config::m_initializated = false;
QMap<QString, DiskImage*> Config::m_diskImages = QMap<QString, DiskImage*>();

size_t Config::m_guestMemSize = 256;
size_t Config::m_guestProcCount = 2;
QString Config::m_guestKernelPath = nullptr;

#ifdef Q_PROCESSOR_X86_64
bool Config::m_kvmEnabled = true;
#else
bool Config::m_kvmEnabled = false;
#endif

void Config::Initializate() {
    assert(m_initializated == false);

    QString configJsonPath = Application::applicationDirPath() + "/Config.json";
    QFile configJsonFile(configJsonPath);
    
    if(configJsonFile.open(QIODevice::ReadOnly) == false){
        QString exceptionStr = "Failed to open \"" + configJsonPath + "\": ";
        exceptionStr += configJsonFile.errorString();
        throw ConfigException(exceptionStr);
    }

    QByteArray ba = configJsonFile.readAll();
    json configJson;
    
    try{
        configJson = json::parse(ba.data());
    }
    catch(json::exception &e){
        QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
        exceptionStr += e.what();
        throw ConfigException(exceptionStr);       
    }
    

    for(auto image : configJson["images"]){
        DiskImage* diskImage = new DiskImage();
        try{
            diskImage->name = QString::fromStdString(image["imageName"]);
            diskImage->path = QString::fromStdString(image["path"]);
        }
        catch(json::exception &e){
            QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
            exceptionStr += e.what();
            delete diskImage;
            throw ConfigException(exceptionStr);   
        }

        diskImage->name = diskImage->name.simplified().toLower().replace(' ', "-");

        QFileInfo diskFileInfo(Application::applicationDirPath() + "/" + diskImage->path);
        if(diskFileInfo.exists() == false){
            QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
            exceptionStr += "Image \"" + diskImage->path + "\" does not exist.";
            delete diskImage;
            throw ConfigException(exceptionStr);  
        }
        diskImage->path = diskFileInfo.absoluteFilePath();

        if(image["initSys"].is_string()){
            diskImage->initSysPath = QString::fromStdString(image["initSys"]);
        }
        else{
            diskImage->initSysPath = "/bin/sh";
        }

        if(m_diskImages.contains(diskImage->name)){
            QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
            exceptionStr += "Image with name \"" + diskImage->name + "\" already exist.";
            delete diskImage;
            throw ConfigException(exceptionStr);   
        }
        m_diskImages[diskImage->name] = diskImage;
    }
    
    if(configJson.contains("guestMemory")){
        json guestMem = configJson["guestMemory"];
        
        if(!guestMem.is_number_unsigned()){
            QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
            exceptionStr += "Field \"guestMemory\" exists, but it's of a wrong type";
            throw ConfigException(exceptionStr);   
        }
        m_guestMemSize = guestMem;
    } 

    if(configJson.contains("guestProcCount")){
        json procCount = configJson["guestProcCount"];
        
        if(!procCount.is_number_unsigned() || procCount < 1){
            QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
            exceptionStr += "Field \"guestProcCount\" exists, but it's of a wrong type, or lesser than 1";
            throw ConfigException(exceptionStr);   
        }
        m_guestProcCount = procCount;
    } 

    if(configJson.contains("kernelPath")){
        json kernelPath = configJson["kernelPath"];
        
        if(!kernelPath.is_string()){
            QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
            exceptionStr += "Field \"kernelPath\" exists, but it's of a wrong type";
            throw ConfigException(exceptionStr);   
        }

        QFileInfo fInfo(QApplication::applicationDirPath() + "/" + QString::fromStdString(kernelPath));
        if(!fInfo.exists() || !fInfo.isFile()) {
            QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
            exceptionStr += "Specified \"kernelPath\" either does not exists, or is not a file";
            throw ConfigException(exceptionStr);   
        }

        m_guestKernelPath = fInfo.absoluteFilePath();
    }
    else {
        QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
        exceptionStr += "Field \"kernelPath\" does not exists";
        throw ConfigException(exceptionStr);
    }

    if(configJson.contains("kvmEnabled")){
        json kvmEnabled = configJson["kvmEnabled"];
        
        if(!kvmEnabled.is_boolean()){
            QString exceptionStr = "Failed to parse \"" + configJsonPath + "\": ";
            exceptionStr += "Field \"kvmEnabled\" exists, but it's of a wrong type";
            throw ConfigException(exceptionStr);   
        }
#ifndef Q_PROCESSOR_X86_64
        if(kvmEnabled){
            qWarning() << "[Config]: KVM is not supported on this platform, ignoring.";
        }
        m_kvmEnabled = false;
#else
        m_kvmEnabled = kvmEnabled;
#endif
    } 

    m_initializated = true;
}

DiskImage* Config::getDiskImage(QString name) {
    assert(m_initializated == true);
    
    return m_diskImages.value(name.simplified().toLower().replace(' ', "-"), nullptr);
}

size_t Config::getGuestMemSize() {
    assert(m_initializated == true);
    return m_guestMemSize;
}

size_t Config::getGuestProcCount() {
    assert(m_initializated == true);
    return m_guestProcCount;
}

QString Config::getGuestKernelPath() {
    assert(m_initializated == true);
    return m_guestKernelPath;
}

bool Config::getKvmEnabled() {
    assert(m_initializated == true);
    return m_kvmEnabled;
}

void Config::CleanUp() {
    for(auto diskImage : m_diskImages){
        delete diskImage;
    }
    m_initializated = false;
}