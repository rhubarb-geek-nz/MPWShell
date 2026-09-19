// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import <Cocoa/Cocoa.h>

@interface DoScriptCommand : NSScriptCommand
@property int state;
@property int clientId;
@property int runspaceId;
- (void) receiveMessage:(int)packetType sourceId:(int)sourceId destinationId:(int)destinationId data:(NSData *)data;
@end
