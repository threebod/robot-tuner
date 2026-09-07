#include "protocol/Crc16.h"

quint16 crc16CcittFalse(QByteArrayView bytes) {
    quint16 crc = 0xFFFF;
    for (const char byte : bytes) {
        crc = static_cast<quint16>(crc ^
                                   (static_cast<quint16>(static_cast<quint8>(byte))
                                    << 8));
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000) != 0) {
                crc = static_cast<quint16>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<quint16>(crc << 1);
            }
        }
    }
    return crc;
}
