/*
@Date  : 8/11/2021
@Author: Tan Dung, Tran
@Brief : MQTT client library
 */
#include "MQTTClient.h"

#define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG    //Macros are usually in all capital letters.
   #define debug(...)          Serial.print(__VA_ARGS__)     //debug print
   #define debugx(...)         Serial.println(__VA_ARGS__)   //debug print with new line
#else
   #define debug(...)     //now defines a blank line
   #define debugx(...)   //now defines a blank line
   #define logxFunc(...)
#endif

#if 0
  #define logFunc(...) debugx(__func__)
#else
  #define logFunc(...)
#endif

MQTTClient* MQTTClient::current = NULL;//pointer for MQTTclient

MQTTClient::MQTTClient(CooperativeMultitasking* _tasks, Client* _client, const char* _host, uint16_t _port, const char* _clientid, const char* _username, const char* _password, uint16_t _keepalive) {
  tasks = _tasks;
  client = _client;
  host = strdup(_host);// make sure same format
  port = _port;
  clientid = strdup(_clientid);// make sure same format
  username = strdupOrNull(_username);// can be null
  password = strdupOrNull(_password);// can be null
  keepalive = _keepalive;
  isconnected = false;
  head = NULL;//head of double linked list
  tail = NULL;//tail of double linked list
}

MQTTClient::~MQTTClient() {
  free(host);
  free(clientid);
  free(username);
  free(password);
  host = NULL;
  clientid = NULL;
  username = NULL;
  password = NULL;
  //seeking all the payloads and free them
  while (head) {
    PublishPacket* next = head->next;
    free(head->payload);
    delete head;
    head = next;
  }
  //
  tail = NULL;
  //free the MQTT client pointer
  if (current == this) current = NULL;
}

bool MQTTClient::connect() {
  if (!current) {
    if (client->connect(host, port)) {
      if (sendConnectPacket()) {
        current = this;
        // in this case, asynchronous is used to avoid delaying the program
        // a connectpacket will has been then a ackpacket will be expected to run the next step receiveConnectAcknowledgementPacket()
        // that's why if we call the publish() method immediatelly, it will be canceled b/c it's waiting for connecting
        // (or we need to delay the program at least 100ms to run publish())
        auto task1 = tasks->ifThen([] () -> bool { return current ? current->available() >= 4 : true; },
                                   [] () -> void { if (current) current->receiveConnectAcknowledgementPacket(); });
        auto task2 = tasks->after(10000, [] () -> void { if (current) current->stop(); });
        tasks->onlyOneOf(task1, task2);
        //
        return true;
      }
      //
      Serial.println("cannot send connect packet");
      //
      stop();
    } else {
      Serial.print("cannot connect to ");
      Serial.print(host);
      Serial.print(":");
      Serial.println(port);
    }
  } else {
    Serial.println("another mqtt client is connected");
  }
  //
  return false;
}

bool MQTTClient::connected() {
  return isconnected;
}

bool MQTTClient::publishAcknowledged() {
  return !head;
}

bool MQTTClient::publish(bool retain, const char* topicname, const char* payload) {
  logFunc();
  PublishPacket* packet = new PublishPacket(); // std::nothrow is default
  current = this;
  //
  if (packet) {
    packet->retain = retain;
    packet->topicname = topicname;
    packet->payload = strdup(payload);
    enqueuePublishPacket(packet);
    auto taskPub = tasks->ifThen( [] () -> bool {return current->isACKconnected;},
                                  [] () -> void { if (current) current->transmitPublishPacketsAfter(0); });//waiting for ack connect done
    current->isACKconnected = false;
    //
    /* transmitPublishPacketsAfter(0); */
    return true;
  }
  //
  Serial.println("cannot enqueue publish packet");
  //
  return false;
}

void MQTTClient::disconnect() {
  if (isconnected) sendDisconnectPacket();
  //
  stop();
}

void MQTTClient::enqueuePublishPacket(PublishPacket* packet) {
  logFunc();
  uint16_t packetid = 1; // 2.3.1 non-zero 16-bit packetid
  //check if there is another packet
  if (head) {
    PublishPacket* packet = head;
    //
    while (packet) {
      if (packet->packetid > packetid) packetid = packet->packetid;
      //
      packet = packet->next;
    }
    //
    packetid++; // biggest packetid plus 1
  }
  //
  packet->packetid = packetid;
  packet->trycount = 0;
  packet->next = head;
  head = packet;
  //
  if (!tail) tail = head;
  debug("Number of packet: ");
  debugx(packet->packetid);
}

void MQTTClient::transmitPublishPacketsAfter(unsigned long duration) {
  logFunc();
  tasks->after(duration, [] () -> void { if (current) current->transmitPublishPackets(); });
}

void MQTTClient::transmitPublishPackets() {
  logFunc();
  if (isconnected && current == this && head) {
    //if the packet has been delivered well, receive the ACK and recheck after 100ms
    if (available() >= 4) {
      debugx("Case1: ");
      receivePublishAcknowledgementPacket();
      transmitPublishPacketsAfter(100);
      //
      return;
    }
    //if the try times >= n times, so give up to deliver the packet
    if (head->trycount >= TRY_TIME) {
      Serial.println("discarding packet");
      //
      removePublishPacket(head->packetid);
      transmitPublishPacketsAfter(100);//???
      //
      return;
    }
    //if the packet has been delivered fail or hasn't delivered, try again after 10s
    if (sendHeadPublishPacket()) {
      debugx("Case3: ");
      head->trycount++;
      rotatePublishPackets();
      transmitPublishPacketsAfter(INTERVAL_TO_RETRY);
      //
      return;
    }
    //
    Serial.println("cannot send publish packet");
    //
    stop();
  }
}

void MQTTClient::removePublishPacket(uint16_t packetid) {
  logFunc();
  PublishPacket* last = NULL;
  PublishPacket* packet = head;
  //
  while (packet) {
    if (packet->packetid == packetid) {
      if (last) last->next = packet->next;
      //
      if (packet == head) head = packet->next;
      //
      if (packet == tail) tail = last;
      //
      free(packet->payload);
      delete packet;
      //
      return;
    }
    //
    last = packet;
    packet = packet->next;
  }
}

void MQTTClient::rotatePublishPackets() {
  logFunc();
  if (!head || head == tail) return; // less than 2 packets
  //
  tail->next = head;
  tail = head;
  head = head->next;
  tail->next = NULL;
}

bool MQTTClient::sendConnectPacket() {
  /* msg format:
  control field: comand type (4 bits) + control flag (4 bits): 1 
  remaining length: 1
  length: 2
  MQTT (name of protocol): 4
  protocol level: 1
  connect flag: 1
  keep alive: 2
  client id
  */
  logFunc();
  
  int packetlength = 2 + 4 + 1 + 1 + 2 + 2 + strlen(clientid);
  //
  if (username != NULL) packetlength += (2 + strlen(username));
  //
  if (password != NULL) packetlength += (2 + strlen(password));
  //
  uint8_t connectflags = 2; // clean session
  //
  if (username != NULL) connectflags |= 128;
  //
  if (password != NULL) connectflags |= 64;
  //
  // Type, Flags, Packet Length
  writeTypeFlags(1, 0); // connect, 0
  writePacketLength(packetlength);
  //
  // Header
  writeLengthString("MQTT"); // protocol name
  writeByte(4); // protocol level
  writeByte(connectflags);
  writeShort(keepalive);
  //
  // Payload
  writeLengthString(clientid);
  //
  if (username != NULL) writeLengthString(username);
  //
  if (password != NULL) writeLengthString(password);
  //
  flush();
  debugx("Send connect ok!");
  //
  return !getWriteError();
}

/* 
This is the main task of program
The program will handle the feedback from Server here and decide what to do next
 */

// void MQTTClient::receiveConnectAcknowledgementPacket() {
void MQTTClient::receiveConnectAcknowledgementPacket() {
  logFunc();

  uint8_t typeflags = readByte();
  uint8_t packetlength = readByte();
  uint8_t sessionpresent = readByte();
  uint8_t returncode = readByte();
  //
  if (typeflags == (2 << 4) && packetlength == 2) {
    switch (returncode) {
      case 0: Serial.println("connection accepted"); isconnected = true; isACKconnected = true; /* transmitPublishPacketsAfter(0); */ return;
      case 1: Serial.println("unacceptable protocol version"); break;
      case 2: Serial.println("identifier rejected"); break;
      case 3: Serial.println("server unavailable"); break;
      case 4: Serial.println("bad user name or password"); break;
      case 5: Serial.println("not authorized"); break;
      default: Serial.println(returncode); break;
    }
  } else {
    Serial.println("not a connect acknowledgement");
  }
  //
  stop();
}

bool MQTTClient::sendHeadPublishPacket() {
  logFunc();

  int packetlength = 2 + strlen(head->topicname) + 2 + strlen(head->payload);
  uint8_t flags = 2; // QoS 1
  //
  if (head->trycount > 0) flags |= 8; // duplicate
  //
  if (head->retain) flags |= 1; // check retain flag
  //
  // Type, Flags, Packet Length
  writeTypeFlags(3, flags); // publish, flags
  writePacketLength(packetlength);
  //
  // Header
  writeLengthString(head->topicname);
  writeShort(head->packetid);
  //
  // Payload
  writeString(head->payload, strlen(head->payload));
  //
  flush();
  //
  return !getWriteError();
}

void MQTTClient::receivePublishAcknowledgementPacket() {
  logFunc();

  uint8_t typeflags = readByte();
  uint8_t packetlength = readByte();
  uint16_t packetid = readShort();
  //
  if (typeflags == (4 << 4) && packetlength == 2) {
    removePublishPacket(packetid);
    //
    Serial.println("publish acknowledged");
    //
    return;
  }
  //
  Serial.println("not a publish acknowledgement");
  disconnect();
}

void MQTTClient::sendDisconnectPacket() {
  logFunc();

  // Type, Flags, Packet Length
  writeTypeFlags(14, 0); // disconnect, 0
  writePacketLength(0);
  //
  flush();
}

void MQTTClient::writeTypeFlags(uint8_t type, uint8_t flags) {
  writeByte(type << 4 | flags);
}

void MQTTClient::writePacketLength(int value) {
  while (true) {
    int digit = value & 127;
    value >>= 7;
    //
    if (value > 0) {
      writeByte(digit | 128);
    } else {
      writeByte(digit);
      //
      break;
    }
  }
}

void MQTTClient::writeLengthString(const char* value) {
  size_t len = strlen(value);
  //
  if (len > 65535) return;
  //
  writeShort(len);
  writeString(value, len);
}

void MQTTClient::writeString(const char* value, size_t len) {
  client->write((uint8_t*) value, len);
}

void MQTTClient::writeShort(uint16_t value) {
  writeByte(value >> 8);
  writeByte(value & 255);
}

void MQTTClient::writeByte(uint8_t value) {
  client->write(value);
}

void MQTTClient::flush() {
  client->flush();
}

int MQTTClient::getWriteError() {
  return client->getWriteError();
}

int MQTTClient::available() {
  return client->available();
}

void MQTTClient::stop() {
  if (client->connected()) {
    client->stop();
    client->clearWriteError();
  }
  //
  isconnected = false;
  current = NULL;
}

uint8_t MQTTClient::readByte() {
  return client->read();
}

uint16_t MQTTClient::readShort() {
  uint16_t value = client->read();
  value <<= 8;
  value += client->read();
  //
  return value;
}

char* MQTTClient::strdupOrNull(const char* string) {
  if (string == NULL) return NULL;
  //
  return strdup(string);
}

//Subcribe here
// SubscribePacket* MQTTClient::SubcribeReceiveHandle() {
//     uint8_t firstbyte = readByte();//doc byte dau tien
//     uint8_t flags = firstbyte & 15;//0b1111
//     uint8_t type = firstbyte >> 4;//
//     size_t length = readPacketLength();
//     //check the control header (cmd type + ctrl flag)
//     switch (type) {
//         //handle sub here
//         case 9: //Subcribe ack (S-C)
//         {
//             if (length == 3) {
//                 uint16_t packetid = readShort();
//                 uint8_t returncode = readByte();
//                 //
//                 if (isReadComplete()) return new SubscribeAcknowledgement(flags, type, packetid, returncode);
//             }
//             //
//             break;
//         }
//     }
//     //
//     while (length > 0) {
//         readByte();
//         length--;
//     }
//     //
//     return nullptr;
// }

// bool MQTTClient::sendSubscribeRequest(const char topicfilter[], uint8_t qos) {
//     packetid++;
//     //
//     if (packetid == 0) packetid = 1;
//     //
//     size_t packetlength = 2 + 2 + strlen(topicfilter) + 1;
//     writeTypeFlags(8, 2);
//     writePacketLength(packetlength);
//     writeShort(packetid);
//     writeLengthString(topicfilter);
//     writeByte(qos);
//     flush();
//     //
//     return isWriteComplete();
// }

// size_t MQTTClient::readPacketLength() {
//     size_t len = 0;
//     size_t multiplier = 1;
//     //
//     while (true) {
//         uint8_t digit = readByte();
//         len += (digit & 127) * multiplier;
//         multiplier <<= 7;
//         //
//         if ((digit & 128) == 0) break;
//     }
//     //
//     return len;
// }

// //this is polling
// //read all msg from server
// char* MQTTClient::readString(size_t len) {
//     char* str = new char[len + 1];
//     //
//     if (str) {
//         for (size_t i = 0; i < len; i++) {
//             str[i] = (char) readByte();
//         }
//         //
//         str[len] = (char) 0;
//     }
//     //
//     return str;
// }

/*
 *
 */

MQTTTopic::MQTTTopic(MQTTClient* _client, const char* _topicname) {
  client = _client;
  topicname = strdup(_topicname);
}

MQTTTopic::~MQTTTopic() {
  free(topicname);
  topicname = NULL;
}

bool MQTTTopic::publish(const char* payload, bool retain) {
  return client->publish(retain, topicname, payload);
}