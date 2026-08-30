// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

struct MPWToolServer
{
	int fdRead, fdWrite;
	struct MPWToolServerMessage* writeQueue;
	struct MPWToolServerMessage* writeMessage;
	struct MPWToolServerMessage* readMessage;
	MPWTOOLS_CLIENT_USERTYPE* userData;
	int readHeader[4], writeHeader[4], state, readHeaderLength, writeHeaderLength, prefixLength;
	char prefix[128];
};

struct MPWToolServerMessage
{
	struct MPWToolServerMessage** queue;
	struct MPWToolServerMessage* next;
	int packetType, clientId, runspaceId, dataLen, usage, dataOffset;
	unsigned char data[1];
};

void MPWToolServerInit(struct MPWToolServer*, MPWTOOLS_CLIENT_USERTYPE*);
int MPWToolServerRead(struct MPWToolServer*);
int MPWToolServerWrite(struct MPWToolServer*);
int MPWToolServerCanWrite(struct MPWToolServer*);
void MPWToolServerAdd(struct MPWToolServer*, struct MPWToolServerMessage*);
void MPWToolServerOnRead(struct MPWToolServer*, struct MPWToolServerMessage*);
struct MPWToolServerMessage* MPWToolServerMessageNew(int packetType, int clientId, int runtimeId, int dataLen);
struct MPWToolServerMessage* MPWToolServerMessageNewWcs(int packetType, int clientId, int runtimeId, const wchar_t *data, int dataLen);
void MPWToolServerMessageRelease(struct MPWToolServerMessage*);
