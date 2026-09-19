// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import <Cocoa/Cocoa.h>

@interface Document : NSDocument

@property int state;
@property int clientId;
@property int runspaceId;
@property NSFont *font;
@property NSTextStorage *textStorage;
@property NSUInteger textCursor;

- (void) receiveMessage:(int)packetType sourceId:(int)sourceId destinationId:(int)destinationId data:(NSData *)data;
- (void) queueMessage:(int)packetType data:(NSData *)data;
- (void) serverRunning:(BOOL)isRunning;

@end

