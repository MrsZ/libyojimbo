/*
    Yojimbo Client/Server Network Protocol Library.
    
    Copyright © 2016, The Network Protocol Company, Inc.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

        2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
           in the documentation and/or other materials provided with the distribution.

        3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived 
           from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef YOJIMBO_CLIENT_SERVER_PACKETS_H
#define YOJIMBO_CLIENT_SERVER_PACKETS_H

#include "yojimbo_config.h"
#include "yojimbo_packet.h"
#include "yojimbo_tokens.h"

/** @file */

namespace yojimbo
{
    /**
        Sent from client to server when a client is first requesting a connection. 

        This is the very first packet the server receives from a potential new client.

        Contains a connect token which the server checks to make sure is valid. This is so the server only allows connections from authenticated clients.

        IMPORTANT: All the data required to establish, authenticate and encrypt the connection is encoded inside the connect token data.

        For insecure connects, please refer to InsecureConnectPacket.

        @see ConnectToken
     */

    struct ConnectionRequestPacket : public Packet
    {
        uint64_t connectTokenExpireTimestamp;                                           ///< The timestamp when the connect token expires. Connect tokens are typically short lived (45 seconds only).
        uint8_t connectTokenData[ConnectTokenBytes];                                    ///< Encrypted connect token data generated by matchmaker. See matcher.go
        uint8_t connectTokenNonce[NonceBytes];                                          ///< Nonce required to decrypt the connect token. Basically a sequence number. Increments with each connect token generated by matcher.go.

        ConnectionRequestPacket()
        {
            connectTokenExpireTimestamp = 0;
            memset( connectTokenData, 0, sizeof( connectTokenData ) );
            memset( connectTokenNonce, 0, sizeof( connectTokenNonce ) );
        }

        template <typename Stream> bool Serialize( Stream & stream )
        {
            serialize_uint64( stream, connectTokenExpireTimestamp );
            serialize_bytes( stream, connectTokenData, sizeof( connectTokenData ) );
            serialize_bytes( stream, connectTokenNonce, sizeof( connectTokenNonce ) );
            return true;
        }

        YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
    };

    /**
        Sent from server to client to deny a client connection.

        This is only sent in when the server is full, in response to clients with valid client connect tokens who would otherwise be able to connect.

        This lets clients get quickly notified that a server is full, so they can try the next server in their list rather than waiting for a potentially long timeout period (5-10 seconds).

        All other situations where the client cannot connect (eg. invalid connect token, connect token timed out), will not get any response from the server. They will just be ignored.
     */

    struct ConnectionDeniedPacket : public Packet
    {
        template <typename Stream> bool Serialize( Stream & /*stream*/ ) { return true; }

        YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
    };

    /**
        Sent from server to client in response to valid connection request packets, provided that the server is not full.

        This challenge/response is done so the server can trust that the client is actually at the packet source address it says it is in the connection request packet. 

        The server only completes connection once the client responds with data that the client cannot possibly know, unless it receives this packet sent to it.

        IMPORTANT: Intentially smaller than the connection request packet, to make DDoS amplification attacks impossible.

        @see ChallengeToken
     */

    struct ChallengePacket : public Packet
    {
        uint8_t challengeTokenData[ChallengeTokenBytes];                                ///< Encrypted challenge token data generated be server in response to a connection request.
        uint8_t challengeTokenNonce[NonceBytes];                                        ///< Nonce required to decrypt the challenge token on the server. Effectively a sequence number which is incremented each time a new challenge token is generated by the server.

        ChallengePacket()
        {
            memset( challengeTokenData, 0, sizeof( challengeTokenData ) );
            memset( challengeTokenNonce, 0, sizeof( challengeTokenNonce ) );
        }

        template <typename Stream> bool Serialize( Stream & stream )
        {
            serialize_bytes( stream, challengeTokenData, sizeof( challengeTokenData ) );
            serialize_bytes( stream, challengeTokenNonce, sizeof( challengeTokenNonce ) );
            return true;
        }

        YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
    };

    /**
        Sent from client to server in response to a challenge packet.

        This packet is sent back to the server, so the server knows that the client is really at the address they said they were in the connection packet.

        @see ChallengeToken
     */

    struct ChallengeResponsePacket : public Packet
    {
        uint8_t challengeTokenData[ChallengeTokenBytes];                                ///< Encrypted challenge token data generated by server.
        uint8_t challengeTokenNonce[NonceBytes];                                        ///< Nonce required to decrypt the challenge token on the server.

        ChallengeResponsePacket()
        {
            memset( challengeTokenData, 0, sizeof( challengeTokenData ) );
            memset( challengeTokenNonce, 0, sizeof( challengeTokenNonce ) );
        }

        template <typename Stream> bool Serialize( Stream & stream )
        {
            serialize_bytes( stream, challengeTokenData, sizeof( challengeTokenData ) );
            serialize_bytes( stream, challengeTokenNonce, sizeof( challengeTokenNonce ) );
            return true;
        }

        YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
    };

    /**
        Sent once client/server connection is established, but only if necessary to avoid time out.

        Also used as a payload to transmit the client index down to the client after connection, so the client knows which client slot they were assigned ot.
     */

    struct KeepAlivePacket : public Packet
    {
        int clientIndex;                                                                ///< The index of the client in [0,maxClients-1]. Used to inform the client which client slot they were assigned to once they have connected.
#if !YOJIMBO_SECURE_MODE
        uint64_t clientSalt;                                                            ///< Random number rolled on each call to Client::InsecureConnect. Makes insecure reconnect much more robust, by distinguishing each connect session from previous ones.
#endif // #if !YOJIMBO_SECURE_MODE

        KeepAlivePacket()
        {
            clientIndex = 0;
#if !YOJIMBO_SECURE_MODE
            clientSalt = 0;
#endif // #if !YOJIMBO_SECURE_MODE
        }

        template <typename Stream> bool Serialize( Stream & stream )
        { 
            serialize_int( stream, clientIndex, 0, MaxClients - 1 );
#if !YOJIMBO_SECURE_MODE
            serialize_uint64( stream, clientSalt );
#endif // #if !YOJIMBO_SECURE_MODE
            return true; 
        }

        YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
    };

    /** 
        Sent between client and server after connection is established when either side disconnects cleanly.

        Speeds up clean disconnects, so the other side doesn't have to timeout before realizing it.
     */

    struct DisconnectPacket : public Packet
    {
        template <typename Stream> bool Serialize( Stream & /*stream*/ ) { return true; }

        YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
    };

#if !YOJIMBO_SECURE_MODE

    /**
        Sent from client to server requesting an insecure connect.

        Insecure connects don't have packet encryption, and don't support authentication. Any client that knows the server IP address can connect.

        They are provided for use in development, eg. ease of connecting to development servers running on your LAN.

        Don't use insecure connects in production! Make sure you \#define YOJIMBO_SECURE_MODE 1 in production builds!
     */

    struct InsecureConnectPacket : public Packet
    {
        uint64_t clientId;                                                              ///< The unique client id that identifies this client to the web backend. Pass in a random number if you don't have one yet.
        uint64_t clientSalt;                                                            ///< Random number rolled each time Client::InsecureConnect is called. Used to make client reconnects robust, by ignoring packets sent from previous client connect sessions from the same address.

        InsecureConnectPacket()
        {
            clientId = 0;
            clientSalt = 0;
        }

        template <typename Stream> bool Serialize( Stream & stream )
        {
            serialize_uint64( stream, clientId );
            serialize_uint64( stream, clientSalt );
            return true;
        }

        YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
    };

#endif // #if !YOJIMBO_SECURE_MODE

    /**
        Defines the set of client/server packets used for connection negotiation.
     */

    enum ClientServerPacketTypes
    {
        CLIENT_SERVER_PACKET_CONNECTION_REQUEST,                                        ///< Client requests a connection.
        CLIENT_SERVER_PACKET_CONNECTION_DENIED,                                         ///< Server denies client connection request (server is full).
        CLIENT_SERVER_PACKET_CHALLENGE,                                                 ///< Server responds to client connection request with a challenge.
        CLIENT_SERVER_PACKET_CHALLENGE_RESPONSE,                                        ///< Client response to the server challenge.
        CLIENT_SERVER_PACKET_KEEPALIVE,                                                 ///< Keep-alive packet sent at some low rate (once per-second) to keep the connection alive. Also used to inform the client of their client index (slot #).
        CLIENT_SERVER_PACKET_DISCONNECT,                                                ///< Courtesy packet to indicate that the other side has disconnected. Beats timing out.
#if !YOJIMBO_SECURE_MODE
        CLIENT_SERVER_PACKET_INSECURE_CONNECT,                                          ///< Client requests an insecure connection (dev only!)
#endif // #if !YOJIMBO_SECURE_MODE
        CLIENT_SERVER_PACKET_CONNECTION,                                                ///< Carries messages and per-packet acks once a client/server connection is established if messages are enabled. See ClientServerConfig::enableMessages (on by default).
        CLIENT_SERVER_NUM_PACKETS
    };

    YOJIMBO_PACKET_FACTORY_START( ClientServerPacketFactory, PacketFactory, CLIENT_SERVER_NUM_PACKETS );

        YOJIMBO_DECLARE_PACKET_TYPE( CLIENT_SERVER_PACKET_CONNECTION_REQUEST,       ConnectionRequestPacket );
        YOJIMBO_DECLARE_PACKET_TYPE( CLIENT_SERVER_PACKET_CONNECTION_DENIED,        ConnectionDeniedPacket );
        YOJIMBO_DECLARE_PACKET_TYPE( CLIENT_SERVER_PACKET_CHALLENGE,                ChallengePacket );
        YOJIMBO_DECLARE_PACKET_TYPE( CLIENT_SERVER_PACKET_CHALLENGE_RESPONSE,       ChallengeResponsePacket );
        YOJIMBO_DECLARE_PACKET_TYPE( CLIENT_SERVER_PACKET_KEEPALIVE,                KeepAlivePacket );
        YOJIMBO_DECLARE_PACKET_TYPE( CLIENT_SERVER_PACKET_DISCONNECT,               DisconnectPacket );
#if !YOJIMBO_SECURE_MODE
        YOJIMBO_DECLARE_PACKET_TYPE( CLIENT_SERVER_PACKET_INSECURE_CONNECT,         InsecureConnectPacket );
#endif // #if !YOJIMBO_SECURE_MODE
        YOJIMBO_DECLARE_PACKET_TYPE( CLIENT_SERVER_PACKET_CONNECTION,               ConnectionPacket );

    YOJIMBO_PACKET_FACTORY_FINISH()
}

#endif // #ifndef YOJIMBO_CLIENT_SERVER_PACKETS_H
