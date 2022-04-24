/*
 * Copyright (C) 2021 Andreas Motzek andreas-motzek@t-online.de
 *
 * This file is part of the MQTT Client package.
 *
 * You can use, redistribute and/or modify this file under the terms
 * of the Creative Commons Attribution 4.0 International Public License.
 * See https://creativecommons.org/licenses/by/4.0/ for details.
 *
 * This file is distributed in the hope that it will be useful, but
 * without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.
 */

#include "MQTTSocket.h"
#include "Arduino.h"

bool MQTTSocket::connect(const char* host, uint16_t port) {
    return client->connect(host, port);
}

bool MQTTSocket::sendConnectRequest(const char clientid[], const char username[], const char password[], uint16_t keepalive) {
    size_t packetlength = 2 + 4 + 1 + 1 + 2 + 2 + strlen(clientid);
    //
    if (username) packetlength += (2 + strlen(username));
    //
    if (password) packetlength += (2 + strlen(password));
    //
    uint8_t connectflags = 2; // clean session
    //
    if (username) connectflags |= 128;
    //
    if (password) connectflags |= 64;
    //
    writeTypeFlags(1, 0);
    writePacketLength(packetlength);
    writeLengthString("MQTT"); // protocol name
    writeByte(4); // protocol level
    writeByte(connectflags);
    writeShort(keepalive);
    writeLengthString(clientid);
    //
    if (username) writeLengthString(username);
    //
    if (password) writeLengthString(password);
    //
    flush();
    //
    return isWriteComplete();
}

bool MQTTSocket::sendSubscribeRequest(const char topicfilter[], uint8_t qos) {
    packetid++;
    //
    if (packetid == 0) packetid = 1;
    //
    size_t packetlength = 2 + 2 + strlen(topicfilter) + 1;
    writeTypeFlags(8, 2);
    writePacketLength(packetlength);
    writeShort(packetid);
    writeLengthString(topicfilter);
    writeByte(qos);
    flush();
    //
    return isWriteComplete();
}

bool MQTTSocket::sendPublishRequest(const char* topic, const char* payload, bool retain, bool duplicate) {
    uint8_t flags = 2; // QoS 1
    //
    if (retain) flags |= 1;
    //
    if (duplicate) {
        flags |= 8;
    } else {
        packetid++;
        //
        if (packetid == 0) packetid = 1;
    }
    //
    size_t packetlength = 2 + strlen(topic) + 2 + strlen(payload);
    writeTypeFlags(3, flags);
    writePacketLength(packetlength);
    writeLengthString(topic);
    writeShort(packetid);
    writeString(payload, strlen(payload));
    flush();
    //
    return isWriteComplete();
}

bool MQTTSocket::sendPingRequest() {
    writeTypeFlags(12, 0);
    writePacketLength(0);
    flush();
    //
    return isWriteComplete();
}

bool MQTTSocket::sendPublishAcknowledgement(uint16_t packetid) {
    writeTypeFlags(4, 0);
    writePacketLength(2);
    writeShort(packetid);
    flush();
    //
    return isWriteComplete();
}

void MQTTSocket::writeTypeFlags(uint8_t type, uint8_t flags) {
    writeByte(type << 4 | flags);
}

void MQTTSocket::writePacketLength(size_t len) {
    while (true) {
        uint8_t digit = len & 127;
        len >>= 7;
        //
        if (len > 0) {
            writeByte(digit | 128);
        } else {
            writeByte(digit);
            //
            break;
        }
    }
}

void MQTTSocket::writeLengthString(const char* value) {
    size_t len = strlen(value);
    //
    if (len > 65535) return;
    //
    writeShort(len);
    writeString(value, len);
}
//Utils
void MQTTSocket::writeString(const char* value, size_t len) {
    for (size_t i = 0; i < len; i++, value++) {
        writeByte((uint8_t) *value);
    }
}

void MQTTSocket::writeShort(uint16_t value) {
    writeByte(value >> 8);
    writeByte(value & 255);
}

void MQTTSocket::writeByte(uint8_t value) {
    if (writeerror) return;
    //
    if (writebufferlength < sizeof writebuffer) {
        writebuffer[writebufferlength] = value;
        writebufferlength++;
    }
}

void MQTTSocket::flush() {
    if (writeerror) return;
    //
    if (writebufferlength < sizeof writebuffer) {
        client->clearWriteError();
        client->write(writebuffer, writebufferlength);
        client->flush();
        writeerror = (client->getWriteError() != 0);
    } else {
        writeerror = true;
    }
    //
    writebufferlength = 0;
}

bool MQTTSocket::canReadSocket() {
    return client->available() >= 2;
}

Packet* MQTTSocket::receive() {
    uint8_t firstbyte = readByte();//doc byte dau tien
    uint8_t flags = firstbyte & 15;//0b1111
    uint8_t type = firstbyte >> 4;//
    size_t length = readPacketLength();
    //check the control header (cmd type + ctrl flag)
    switch (type) {
        //handle pub here
        case 2: //connect ack (S-C)
        {
            if (length == 2) {
                uint8_t sessionpresent = readByte();
                uint8_t returncode = readByte();
                //
                if (isReadComplete()) return new ConnectAcknowledgement(flags, type, sessionpresent, returncode);
            }
            //
            break;
        }
        case 3: //Publish msg (C-S or S-C)
        {
            size_t topiclength = readShort();
            char* topic = readString(topiclength);
            uint16_t packetid = 0;
            char* payload = nullptr;
            //
            if ((flags & 6) > 0) {
                packetid = readShort();
                payload = readString(length - topiclength - 4);
            } else {
                payload = readString(length - topiclength - 2);
            }
            //
            if (isReadComplete()) {
                return new PublishNotification(flags, type, topic, packetid, payload);
            } else {
                delete topic;
                delete payload;
            }
            //
            break;
        }
        case 4: //Publish ack (C-S or S-C)
        {
            if (length == 2) {
                uint16_t packetid = readShort();
                //
                if (isReadComplete()) return new PublishAcknowledgement(flags, type, packetid);
            }
            //
            break;
        }
        //handle sub here
        case 9: //Subcribe ack (S-C)
        {
            if (length == 3) {
                uint16_t packetid = readShort();
                uint8_t returncode = readByte();
                //
                if (isReadComplete()) return new SubscribeAcknowledgement(flags, type, packetid, returncode);
            }
            //
            break;
        }
        case 13: //ping response (S-C)
        {
            if (length == 0 && isReadComplete()) return new PingResponse(flags, type);
            //
            break;
        }
    }
    //
    while (length > 0) {
        readByte();
        length--;
    }
    //
    return nullptr;
}

void MQTTSocket::close() {
    client->stop();
    writeerror = false;
    readerror = false;
    packetid = 0;
}

uint8_t MQTTSocket::readByte() {
    if (readerror) return 0;
    //
    int i = client->read();
    //
    if (i < 0) {
        readerror = true;
        //
        return 0;
    }
    //
    return i & 255;
}

uint16_t MQTTSocket::readShort() {
    uint16_t value = readByte();
    value <<= 8;
    value += readByte();
    //
    return value;
}

char* MQTTSocket::readString(size_t len) {
    char* str = new char[len + 1];
    //
    if (str) {
        for (size_t i = 0; i < len; i++) {
            str[i] = (char) readByte();
        }
        //
        str[len] = (char) 0;
    }
    //
    return str;
}

size_t MQTTSocket::readPacketLength() {
    size_t len = 0;
    size_t multiplier = 1;
    //
    while (true) {
        uint8_t digit = readByte();
        len += (digit & 127) * multiplier;
        multiplier <<= 7;
        //
        if ((digit & 128) == 0) break;
    }
    //
    return len;
}
