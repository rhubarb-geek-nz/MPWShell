// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import "Document.h"

@interface DocumentRequest : NSObject

@property int packetType, clientId, runspaceId;
@property NSData *data;
@property NSURL *fileURL;
@property Document *document;

@end
