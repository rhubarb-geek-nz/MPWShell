// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import "AppDelegate.h"
#import "DoScriptCommand.h"

@interface DoScriptCommand ()

@end

@implementation DoScriptCommand {
    NSMutableData *dataStdout;
    NSMutableData *dataStderr;
}

- (id)performDefaultImplementation
{
    AppDelegate *appDelegate = [NSApp delegate];

    return [appDelegate startScript:self];
}

- (void) receiveMessage:(int)packetType sourceId:(int)sourceId destinationId:(int)destinationId data:(NSData *)data
{
    AppDelegate *appDelegate = [NSApp delegate];
    NSString *output = NULL;

    switch (packetType)
    {
        case 0x40:
            if (data)
            {
                if (self->dataStdout)
                {
                    [self->dataStdout appendBytes:"\n" length:1];
                    [self->dataStdout appendData:data];
                }
                else
                {
                    self->dataStdout = [NSMutableData dataWithData:data];
                }
            }
            break;

        case 0x43:
        case 0x44:
            if (data)
            {
                if (self->dataStderr)
                {
                    [self->dataStderr appendBytes:"\n" length:1];
                    [self->dataStderr appendData:data];
                }
                else
                {
                    self->dataStderr = [NSMutableData dataWithData:data];
                }
            }
            break;

        case 0x45:
            if (self->dataStderr)
            {
                NSString *error = [[NSString alloc] initWithData:self->dataStderr encoding:NSUTF8StringEncoding];
                [self setScriptErrorString:error];
                [self setScriptErrorNumber:NSInternalScriptError];
            }
            else
            {
                if (self->dataStdout)
                {
                    output = [[NSString alloc] initWithData:self->dataStdout encoding:NSUTF8StringEncoding];
                }
                else
                {
                    output = @"";
                }
            }

            [self resumeExecutionWithResult:output];
            [appDelegate queueMessage:0x50 sourceId:self.clientId destinationId:self.runspaceId data:NULL];
            self.state = 3;
            break;

        case 0x42:
            [self setScriptErrorString:@"Runspace has terminated"];
            [self resumeExecutionWithResult:output];
            self.state=3;
            break;
    }
}
@end
