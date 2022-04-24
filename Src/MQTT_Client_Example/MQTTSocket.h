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

#ifndef MQTTSocket_h
#define MQTTSocket_h

#include <Client.h>

class Packet {
    private:
        const uint8_t flags;
        const uint8_t type;

    protected:
        Packet(uint8_t f, uint8_t t) : flags(f), type(t) { }

    public:
        virtual ~Packet() { }
        uint8_t getFlags() const { return flags; }
        uint8_t getType() const { return type; }
};

class ConnectAcknowledgement : public Packet {
    private:
        const uint8_t sessionpresent;
        const uint8_t returncode;

    public:
        ConnectAcknowledgement(uint8_t f, uint8_t t, uint8_t s, uint8_t r) : Packet(f, t), sessionpresent(s), returncode(r) { }
        virtual ~ConnectAcknowledgement() { }
        uint8_t getSessionPresent() const { return sessionpresent; }
        uint8_t getReturnCode() const { return returncode; }
        bool isConnectionAccepted() const { return returncode == 0; }
};

class PublishNotification : public Packet {
    private:
        const char* topic;
        const uint16_t packetid;
        const char* payload;

    public:
        PublishNotification(uint8_t f, uint8_t t, char* tc, uint16_t pi, char* pl) : Packet(f, t), topic(tc), packetid(pi), payload(pl) { }
        virtual ~PublishNotification() { delete topic; delete payload; }
        const char* getTopic() const { return topic; }
        uint16_t getPacketId() const { return packetid; }
        bool isDuplicate() const { return getFlags() & 8 > 0; }
        const char* getPayload() const { return payload; }
};

class SubscribeAcknowledgement : public Packet {
    private:
        const uint16_t packetid;
        const uint8_t returncode;

    public:
        SubscribeAcknowledgement(uint8_t f, uint8_t t, uint16_t p, uint8_t r) : Packet(f, t), packetid(p), returncode(r) { }
        virtual ~SubscribeAcknowledgement() { }
        uint16_t getPacketId() const { return packetid; }
        uint8_t getReturnCode() const { return returncode; }
        bool hasPacketId(uint16_t p) const { return p == packetid; }
        bool isSubscriptionAccepted() const { return returncode != 128; }
};

class PublishAcknowledgement : public Packet {
    private:
        const uint16_t packetid;

    public:
        PublishAcknowledgement(uint8_t f, uint8_t t, uint16_t p) : Packet(f, t), packetid(p) { }
        virtual ~PublishAcknowledgement() { }
        uint16_t getPacketId() const { return packetid; }
        bool hasPacketId(uint16_t p) const { return p == packetid; }
};

class PingResponse : public Packet {
    public:
        PingResponse(uint8_t f, uint8_t t) : Packet(f, t) { }
        virtual ~PingResponse() { }
};

class MQTTSocket {
    private:
        Client* client;
        uint16_t packetid;
        uint8_t writebuffer[256];
        size_t writebufferlength;
        bool readerror;
        bool writeerror;
        void writeTypeFlags(uint8_t type, uint8_t flags);
        void writePacketLength(size_t value);
        void writeLengthString(const char* value);
        void writeString(const char* value, size_t len);
        void writeShort(uint16_t value);
        void writeByte(uint8_t value);
        void flush();
        uint8_t readByte();
        uint16_t readShort();
        char* readString(size_t len);
        size_t readPacketLength();

    public:
        MQTTSocket(Client* c) : client(c) { packetid = 0; writebufferlength = 0; writeerror = false; readerror = false; }
        bool connect(const char* host, uint16_t port);
        bool sendConnectRequest(const char clientid[], const char username[], const char password[], uint16_t keepalive);
        bool sendSubscribeRequest(const char topicfilter[], uint8_t qos);
        bool sendPublishRequest(const char* topic, const char* payload, bool retain, bool duplicate);
        bool sendPingRequest();
        bool sendPublishAcknowledgement(uint16_t packetid);
        bool canReadSocket();
        Packet* receive();
        uint16_t getPacketId() const { return packetid; }
        bool isWriteComplete() const { return !writeerror; }
        bool isReadComplete() const { return !readerror; }
        void close();
};

#endif
