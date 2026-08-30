// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <mbcs.h>

#define MPWTOOLS_CLIENT_USERTYPE struct MPWTOOLS_CLIENT_OPAQUE

#include <mpwtools.h>

void MPWToolServerInit(struct MPWToolServer*server, MPWTOOLS_CLIENT_USERTYPE*userData)
{
	server->writeQueue = NULL;
	server->writeMessage = NULL;
	server->userData = userData;
	server->readHeaderLength = 0;
	server->writeHeaderLength = sizeof(server->writeHeader);
	server->prefixLength = 0;
	server->state = 0;
}

static const int prefixLength = 36;

static int isPrefixAtEndWithNewline(const char* data, int len)
{
	if (data[0] != 0xA) return 0;

	if (data[-1] == 0xD)
	{
		data--;
		len--;
	}

	if (len < prefixLength)
	{
		return 0;
	}

	return memcmp(data - prefixLength, "0c63fba6-7c2a-4b72-8de0-b3bd579dedaa", prefixLength) ? 0 : 1;
}

int MPWToolServerRead(struct MPWToolServer*server)
{
	switch (server->state)
	{
	case 0:
		{
			int i = sizeof(server->prefix) - server->prefixLength - 1;

			if (i > 0)
			{
				i = read(server->fdRead, server->prefix + server->prefixLength, i);

				if (i > 0)
				{
					server->prefixLength += i;

					server->prefix[server->prefixLength] = 0;
				}
			}

			while (server->prefixLength)
			{
				int writeLength = 0;
				const char* lastLF = NULL;
				const char* p = server->prefix;
				
				while (p)
				{
					const char* q = strchr(p, 0xA);

					if (q)
					{
						lastLF = q;
						p = q + 1;
					}
					else
					{
						break;
					}
				}

				if (lastLF)
				{
					int m = (int)(lastLF - server->prefix);

					if (m >= prefixLength)
					{
						if (isPrefixAtEndWithNewline(lastLF, m))
						{
							server->prefixLength -= prefixLength + 1;
							if (lastLF[-1] == 0x0D)
							{
								server->prefixLength--;
							}
							writeLength = server->prefixLength;
							server->state = 1;
						}
						else
						{
							writeLength = m + 1;
						}
					}
				}
				else
				{
					writeLength = server->prefixLength - prefixLength - 2;
				}

				if ((writeLength > 0)||(server->state))
				{
					struct MPWToolServerMessage* message = MPWToolServerMessageNew(0x46, 0, 0, writeLength);

					if (writeLength)
					{
						memcpy(message->data, server->prefix, writeLength);
					}

					server->prefixLength -= writeLength;

					if (server->prefixLength)
					{
						memmove(server->prefix, server->prefix + writeLength, server->prefixLength);
					}

					server->prefix[server->prefixLength] = 0;

					MPWToolServerOnRead(server, message);
					MPWToolServerMessageRelease(message);
				}
				else
				{
					break;
				}
			}

			if (i == 0)
			{
				if (server->prefixLength)
				{
					struct MPWToolServerMessage* message = MPWToolServerMessageNew(0x46, 0, 0, server->prefixLength);
					memcpy(message->data, server->prefix, server->prefixLength);
					message->data[server->prefixLength] = 0;
					server->prefixLength = 0;
					server->prefix[server->prefixLength] = 0;
					MPWToolServerOnRead(server, message);
					MPWToolServerMessageRelease(message);
				}
			}

			return i;
		}
		break;

	default:
		{
			int i = sizeof(server->readHeader) - server->readHeaderLength;

			if (i > 0)
			{
				char* p = (void*)server->readHeader;

				i = read(server->fdRead, p + server->readHeaderLength, i);

				if (i < 1) 
				{
					return i;
				}

				server->readHeaderLength += i;

				if (server->readHeaderLength == sizeof(server->readHeader))
				{
					int* hdr = server->readHeader;
					server->readMessage = MPWToolServerMessageNew(hdr[0], hdr[2], hdr[1], hdr[3]);
				}
			}
			else
			{
				struct MPWToolServerMessage* msg = server->readMessage;

				i = msg->dataLen - msg->dataOffset;

				if (i > 0)
				{
					i = read(server->fdRead, msg->data + msg->dataOffset, i);

					if (i < 1) return i;

					msg->dataOffset += i;
				}

				if (msg->dataLen == msg->dataOffset)
				{
					server->readMessage = NULL;
					server->readHeaderLength = 0;

					switch (msg->packetType)
					{
						case 0x40:
						case 0x42:
						case 0x43:
						case 0x44:
						case 0x45:
							msg->data[msg->dataLen++]='\n';
							msg->data[msg->dataLen]=0;
							break;
					}

					MPWToolServerOnRead(server, msg);
					MPWToolServerMessageRelease(msg);
				}
			}

			return 1;
		}

		break;
	}

	return 0;
}

int MPWToolServerWrite(struct MPWToolServer* server)
{
	int i = 0;

	if (!(server->writeMessage))
	{
		struct MPWToolServerMessage* p = server->writeQueue;

		if (!p)
		{
			return 0;
		}

		p->queue = NULL;
		server->writeQueue= p->next;
		server->writeMessage = p;
		p->dataOffset = 0;
		server->writeHeader[0] = p->packetType;
		server->writeHeader[1] = p->clientId;
		server->writeHeader[2] = p->runspaceId;
		server->writeHeader[3] = p->dataLen;
		server->writeHeaderLength = 0;
	}

	i = sizeof(server->writeHeader) - server->writeHeaderLength;

	if (i > 0)
	{
		char* p = (void *)server->writeHeader;
		i = write(server->fdWrite, p + server->writeHeaderLength, i);
		if (i < 0) return -1;
		server->writeHeaderLength += i;
	}
	else
	{
		struct MPWToolServerMessage* p = server->writeMessage;

		i = p->dataLen - p->dataOffset;
		i = write(server->fdWrite, p->data + p->dataOffset, i);

		if (i < 0) return -1;
		p->dataOffset += i;

		if (p->dataOffset == p->dataLen)
		{
			server->writeMessage = NULL;
			MPWToolServerMessageRelease(p);
		}
	}

	return 1;
}

void MPWToolServerAdd(struct MPWToolServer* server, struct MPWToolServerMessage* message)
{
	message->usage++;
	message->queue = &(server->writeQueue);
	message->next = NULL;

	if (server->writeQueue)
	{
		struct MPWToolServerMessage* p = server->writeQueue;
		while (p->next) p = p->next;
		p->next = message;
	}
	else
	{
		server->writeQueue = message;
	}
}

struct MPWToolServerMessage* MPWToolServerMessageNew(int packetType, int clientId, int runspaceId, int dataLen)
{
	struct MPWToolServerMessage* msg = malloc(sizeof(*msg) + dataLen + 4);

	if (msg)
	{
		msg->usage = 1;
		msg->packetType = packetType;
		msg->clientId = clientId;
		msg->runspaceId = runspaceId;
		msg->dataLen = dataLen;
		msg->dataOffset = 0;
		msg->next = NULL;
		msg->queue = NULL;
		msg->data[dataLen] = 0;
	}

	return msg;
}

void MPWToolServerMessageRelease(struct MPWToolServerMessage*msg)
{
	if (!--(msg->usage))
	{
		if (msg->queue)
		{
			if (msg->queue[0] == msg)
			{
				msg->queue[0] = msg->next;
			}
			else
			{
				struct MPWToolServerMessage* p = msg->queue[0];
				while (p->next != msg) p = p->next;
				p->next = msg->next;
			}
		}

		free(msg);
	}
}

int MPWToolServerCanWrite(struct MPWToolServer* server)
{
	return server->writeMessage || server->writeQueue;
}

struct MPWToolServerMessage* MPWToolServerMessageNewWcs(int packetType, int clientId, int runtimeId, const wchar_t *data, int dataLen)
{
	int actLen=0;
	int len=dataLen;
	const wchar_t *wp=data;
	struct MPWToolServerMessage* msg;
	unsigned char *dest;

	while (len--)
	{
		unsigned char buf[4];
		actLen+=mbcsFromChar(*wp++,buf);
	}

	msg=MPWToolServerMessageNew(packetType, clientId, runtimeId, actLen);

	wp=data;
	len=dataLen;
	dest=msg->data;

	while (len--)
	{
		dest += mbcsFromChar(*wp++,dest);
	}

	return msg;
}
