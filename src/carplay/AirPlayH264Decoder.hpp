#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

class AirPlayH264Decoder {
public:
    AirPlayH264Decoder();
    ~AirPlayH264Decoder();

    bool preload();
    bool configure(const QByteArray &avcC);
    QImage decode(const QByteArray &annexB);
    void flush();
    QString lastError() const;

private:
    struct Impl;
    Impl *m = nullptr;
};
