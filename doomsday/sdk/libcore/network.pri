publicHeaders(root, \
    include/de/Address \
    include/de/Beacon \
    include/de/BlockPacket \
    include/de/IdentifiedPacket \
    include/de/Message \
    include/de/ListenSocket \
    include/de/Packet \
    include/de/Protocol \
    include/de/RecordPacket \
    include/de/Socket \
    include/de/Transmitter \
)

publicHeaders(net, \
    include/de/net/address.h \
    include/de/net/beacon.h \
    include/de/net/blockpacket.h \
    include/de/net/identifiedpacket.h \
    include/de/net/message.h \
    include/de/net/listensocket.h \
    include/de/net/packet.h \
    include/de/net/protocol.h \
    include/de/net/recordpacket.h \
    include/de/net/socket.h \
    include/de/net/transmitter.h \
)

SOURCES += \
    src/net/address.cpp \
    src/net/beacon.cpp \
    src/net/blockpacket.cpp \
    src/net/identifiedpacket.cpp \
    src/net/message.cpp \
    src/net/listensocket.cpp \
    src/net/packet.cpp \
    src/net/protocol.cpp \
    src/net/recordpacket.cpp \
    src/net/socket.cpp \
    src/net/transmitter.cpp
