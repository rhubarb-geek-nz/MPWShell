// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property int serverState;

-(BOOL)startPowerShell;
-(void)terminationHandler:(NSTask *)task;
-(void)handleDataAvailableStdout:(NSNotification *)notification;
-(void)handleDataAvailableStderr:(NSNotification *)notification;
-(void)handleDataStdin:(NSObject *)something;
-(void)queueMessage:(int)packetType sourceId:(int)sourceId destinationId:(int)destinationId data:(NSData *)data;
-(void)receiveMessage:(int)packetType sourceId:(int)sourceId destinationId:(int)destinationId data:(NSMutableData *)data;
-(id)startScript:(NSScriptCommand *)script;
-(AppDelegate *)init;

@end
