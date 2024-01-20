#include "Presentation.hpp"

#include <QtCore/QFile>
#include <QtCore/QDebug>

#include <zip.h>

// Size of unzip buffor
#define BUFFOR_SIZE 1024

void Presentation::decompressVslidesArchive(QString path){
    zip_t* zipArchive = nullptr;
    zip_file_t* zf;
    zip_stat_t zStat;
    int zipErrorCode = 0;

    QByteArray cPath = path.toUtf8();
    if((zipArchive = zip_open(cPath.data(), ZIP_RDONLY, &zipErrorCode)) == nullptr){
        zip_error_t error;
        zip_error_init_with_code(&error, zipErrorCode);
        QString exceptionString = "Failed to open '" + path + "': "
            + zip_error_strerror(&error);
        zip_error_fini(&error);
        throw PresentationException(exceptionString);
    }

    QDir cwd = QDir::current();

    size_t zEntriesCount = zip_get_num_entries(zipArchive, 0);
    for(size_t i = 0; i < zEntriesCount; ++i){
        if(zip_stat_index(zipArchive, i, 0, &zStat) == -1){
            zip_error_t* error = zip_get_error(zipArchive);
            QString exceptionString = "Failed to decompress '" + path + "': ";
                + zip_error_strerror(error);
            zip_close(zipArchive);
            throw PresentationException(exceptionString);
        }
        QString fileName = QString(zStat.name);
        if(fileName[fileName.length() - 1] == '/'){
            if(!cwd.mkpath(fileName)){
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "'");
            }
        }
        else{
            QFile file(fileName);
            zf = zip_fopen_index(zipArchive, i, 0);
            if(file.open(QIODevice::WriteOnly) == false || zf == nullptr){
                zip_close(zipArchive);
                throw PresentationException("Failed to decompress '" + path + "'");
            }

            char buffor[BUFFOR_SIZE];
            ssize_t sum = 0;
            ssize_t length;
            while(sum < zStat.size){
                if((length = zip_fread(zf, buffor, BUFFOR_SIZE)) == -1){
                    file.close();
                    zip_close(zipArchive);
                    throw PresentationException("Failed to decompress '" + path + "'");
                }
                file.write(buffor, length);
                sum += length;
            }
            file.close();
        }
    }
    zip_close(zipArchive);
}

Presentation::Presentation(QString path) {
    path = QFileInfo(path).absoluteFilePath();
    m_tmpDir.setAutoRemove(false);

    QString oldCwd = QDir::currentPath();
    if(!QDir::setCurrent(m_tmpDir.path())){
        throw PresentationException("Failed to chdir into tmp dir");
    }

    if(!m_tmpDir.isValid()){
        throw PresentationException(m_tmpDir.errorString());
    }

    try{
        decompressVslidesArchive(path);
    }
    catch(PresentationException &e){
        QDir::setCurrent(oldCwd);
        m_tmpDir.remove();
        throw;
    }
}

Presentation::~Presentation() {
    m_tmpDir.remove();
}