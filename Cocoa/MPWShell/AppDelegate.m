// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import "AppDelegate.h"
#import "Document.h"
#import "DocumentRequest.h"
#import "DoScriptCommand.h"

@interface MissingDeclarations : NSObject
-(void)undo:(id)sender;
-(void)redo:(id)sender;
@end

@interface AppDelegate ()
@end

@implementation AppDelegate
{
    NSPipe *pipeStdin;
    NSPipe *pipeStdout;
    NSPipe *pipeStderr;
    NSTask *taskPowerShell;
    NSCondition *writeCondition;
    NSMutableArray<NSData *> *writeArray;
    NSThread *writeThread;
    int clientCount;
    NSMutableData *dataStdout;
    NSMutableData *dataStderr;
    NSData *dataSignature;
    NSData *dataInitialScript;
    int header[4];
    NSUInteger headerLen;
    NSMutableData *dataMessage;
    NSMutableArray<DocumentRequest *>*pendingRequests;
    NSMutableArray<DoScriptCommand *>*pendingCommands;
}

static int nextClientId(AppDelegate *self)
{
    int result;

    while (true)
    {
        result = ++(self->clientCount);

        if (result)
        {
            int found = 0;

            for (DoScriptCommand *cmd in self->pendingCommands)
            {
                if (cmd.clientId == result)
                {
                    found = 1;
                }
            }

            if (!found)
            {
                for (Document *doc in [[NSDocumentController sharedDocumentController] documents])
                {
                    if (doc.clientId == result)
                    {
                        found = 1;
                        break;
                    }
                }

                if (!found)
                {
                    break;
                }
            }
        }
    }

    return result;
}

static void removeMenuItemsAfter(NSMenu *menu, SEL *sel, NSUInteger selCount)
{
    NSUInteger i = 0;
    NSUInteger accountedFor = 0;
    unsigned long map=0;
    
    while (i < menu.numberOfItems)
    {
        NSMenuItem *item=[menu itemAtIndex:i];
        
        i++;
        
        if (!item.isSeparatorItem)
        {
            NSUInteger j = 0;
            unsigned long mask = 1;
            
            while (j < selCount)
            {
                if ((!(map&mask)) && sel[j] == item.action)
                {
                    map |= mask;
                    
                    accountedFor++;
                    
                    break;
                }
                
                j++;
                
                mask <<= 1;
            }
        }
        
        if (accountedFor == selCount)
        {
            break;
        }
    }

    while (menu.numberOfItems > i)
    {
        [menu removeItemAtIndex:i];
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    SEL selFileMenu[]={
        @selector(newDocument:),
        @selector(openDocument:),
        @selector(saveDocument:),
        @selector(saveDocumentAs:),
        @selector(performClose:)};
    
    SEL selEditMenu[]={
        @selector(undo:),
        @selector(redo:),
        @selector(cut:),
        @selector(copy:),
        @selector(paste:),
        @selector(delete:),
        @selector(selectAll:)};
    
    NSMenu *menubar = [NSApp mainMenu];
    
    removeMenuItemsAfter([menubar.itemArray objectAtIndex:1].submenu,selFileMenu,sizeof(selFileMenu)/sizeof(selFileMenu[0]));

    removeMenuItemsAfter([menubar.itemArray objectAtIndex:2].submenu,selEditMenu,sizeof(selEditMenu)/sizeof(selEditMenu[0]));

    [NSWindow setAllowsAutomaticWindowTabbing:NO];

    [self startPowerShell];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    NSPipe *stdin=NULL;
    [self->writeCondition lock];
    self->taskPowerShell=NULL;
    if (self.serverState)
    {
        [self->writeCondition signal];
    }
    else
    {
        stdin=self->pipeStdin;
        self->pipeStdin=NULL;
    }
    [self->writeCondition unlock];
    if (stdin)
    {
        [[stdin fileHandleForWriting]closeFile];
    }
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app
{
    return YES;
}

static NSString *keyInitialScript = @"InitialScript";
static NSString *keyToolServerLaunchPath = @"ToolServerLaunchPath";
static NSString *keyToolServerArguments = @"ToolServerArguments";
static NSString *keyToolServerEnvironment = @"ToolServerEnvironment";

- (BOOL)startPowerShell
{
    BOOL result=FALSE;
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];

    NSString *appLaunchPath = [userDefaults stringForKey:keyToolServerLaunchPath];
    NSArray<NSString *> *toolServerArguments = [userDefaults stringArrayForKey:keyToolServerArguments];
    NSDictionary *toolServerEnvironment = [userDefaults dictionaryForKey:keyToolServerEnvironment];

    self->pipeStdin = [[NSPipe alloc]init];
    self->pipeStdout = [[NSPipe alloc]init];
    self->pipeStderr = [[NSPipe alloc]init];
    self->taskPowerShell = [[NSTask alloc] init];

    [self->taskPowerShell setLaunchPath:appLaunchPath];

    if (toolServerArguments)
    {
        [self->taskPowerShell setArguments:toolServerArguments];
    }

    if (toolServerEnvironment)
    {
        NSMutableDictionary *mutableDictionary=[[[NSProcessInfo processInfo]environment] mutableCopy];

        [toolServerEnvironment enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull value, BOOL * _Nonnull stop)
        {
            if (value && [value isKindOfClass:[NSString class]])
            {
                NSString *stringValue = value;

                if ([@"PATH" isEqualToString:key])
                {
                    NSString *path = [mutableDictionary objectForKey:@"PATH"];

                    if ([stringValue hasSuffix:@":"])
                    {
                        value = [stringValue stringByAppendingString:path];
                    }
                    else
                    {
                        if ([stringValue hasPrefix:@":"])
                        {
                            value = [path stringByAppendingString:stringValue];
                        }
                    }
                }
                else
                {
                    if (stringValue.length == 0)
                    {
                        value = NULL;
                    }
                }
            }

            if (value)
            {
                mutableDictionary[key] = value;
            }
            else
            {
                [mutableDictionary removeObjectForKey:key];
            }
        }];

        [self->taskPowerShell setEnvironment:mutableDictionary];
    }

    [self->taskPowerShell setStandardInput:self->pipeStdin];
    [self->taskPowerShell setStandardOutput:self->pipeStdout];
    [self->taskPowerShell setStandardError:self->pipeStderr];

    __weak typeof(self) weakSelf = self;

    [[NSNotificationCenter defaultCenter] addObserver:self
           selector:@selector(handleDataAvailableStdout:)
             name:NSFileHandleDataAvailableNotification
           object:self->pipeStdout.fileHandleForReading];

    [[NSNotificationCenter defaultCenter] addObserver:self
           selector:@selector(handleDataAvailableStderr:)
             name:NSFileHandleDataAvailableNotification
           object:self->pipeStderr.fileHandleForReading];

    [self->pipeStdout.fileHandleForReading waitForDataInBackgroundAndNotify];
    [self->pipeStderr.fileHandleForReading waitForDataInBackgroundAndNotify];

    [self->taskPowerShell setTerminationHandler:^(NSTask *completedTask) {
        typeof(self) strongSelf = weakSelf;
        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf terminationHandler:completedTask];
        });
    }];
    
    NSString *message = NULL;

    @try
    {
        [self->taskPowerShell launch];
        result = TRUE;
    }
    @catch (NSException *ex)
    {
        message = ex.reason;
    }
    
    [self->pipeStdin.fileHandleForReading closeFile];
    [self->pipeStdout.fileHandleForWriting closeFile];
    [self->pipeStdout.fileHandleForWriting closeFile];
    
    if (message)
    {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Error launching PowerShell"];
        [alert setInformativeText:message];
        [alert addButtonWithTitle:@"OK"];
        [alert setAlertStyle:NSAlertStyleCritical];
        
        [alert runModal];
    }
    
    return result;
}

- (void)terminationHandler:(NSTask *)task
{
    [self->writeCondition lock];
    self->taskPowerShell = NULL;
    [self->writeCondition signal];
    [self->writeCondition unlock];
}

static NSUInteger nextByteMatch(const UInt8 *data,NSUInteger length, UInt8 value)
{
    NSUInteger i=0;
    
    while (i < length)
    {
        if (data[i] == value)
        {
            return i;
        }
        
        i++;
    }
    
    return NSUIntegerMax;
}

static void startOutput(AppDelegate *self)
{
    self.serverState = 1;
    
    self->writeThread = [[NSThread alloc] initWithTarget:self selector:@selector(handleDataStdin:) object:nil];
    [self->writeThread start];
    
    self.serverState = 2;
    
    [self serverRunning:TRUE];
}

-(void)handleDataAvailableStdout:(NSNotification *)notification
{
    NSFileHandle *fileHandle = [notification object];
    NSData *data=[fileHandle availableData];

    if (data.length > 0)
    {
        const UInt8 *bytes = data.bytes;
        NSUInteger length = data.length;
        NSMutableData *dataWrite = NULL;
        
        while (length)
        {
            switch (self->_serverState)
            {
                case 0:
                    {
                        NSUInteger lfAt = nextByteMatch(bytes, length, 0xA);

                        if (lfAt == NSUIntegerMax)
                        {
                            [self->dataStdout appendBytes:bytes length:length];
                            length = 0;
                        }
                        else
                        {
                            if (lfAt)
                            {
                                [self->dataStdout appendBytes:bytes length:lfAt];
                                bytes += lfAt;
                                length -= lfAt;
                            }
                            
                            if (self->dataStdout.length)
                            {
                                NSUInteger sigLen = self->dataSignature.length;
                                
                                if (self->dataStdout.length >= sigLen)
                                {
                                    const UInt8 *pcb = self->dataStdout.bytes;
                                    
                                    if (memcmp(pcb + self->dataStdout.length - sigLen, self->dataSignature.bytes, sigLen))
                                    {
                                        [self->dataStdout appendBytes:bytes length:1];
                                    }
                                    else
                                    {
                                        self->dataStdout.length = self->dataStdout.length - sigLen;
                                        startOutput(self);
                                    }
                                }
                                else
                                {
                                    [self->dataStdout appendBytes:bytes length:1];
                                }
                                
                                if (self->dataStdout.length)
                                {
                                    if (dataWrite)
                                    {
                                        [dataWrite appendData:self->dataStdout];
                                    }
                                    else
                                    {
                                        dataWrite = self->dataStdout;
                                    }

                                    self->dataStdout = [[NSMutableData alloc] init];
                                }
                            }
                            else
                            {
                                if (dataWrite)
                                {
                                    [dataWrite appendBytes:bytes length:1];
                                }
                                else
                                {
                                    dataWrite = [NSMutableData dataWithBytes:bytes length:1];
                                }
                            }
                            
                            length--;
                            bytes++;
                        }
                    }
                    break;
                case 2:
                    if (self->headerLen < sizeof(self->header))
                    {
                        char *p=(void *)self->header;
                        NSUInteger len = sizeof(self->header) - self->headerLen;
                        if (len > length) len=length;
                        memcpy(p+self->headerLen,bytes,len);
                        bytes += len;
                        length -= len;
                        self->headerLen += len;
                        
                        if (self->headerLen == sizeof(self->header) && self->header[3])
                        {
                            NSUInteger capacity=self->header[3] + 1;
                            self->dataMessage = [NSMutableData dataWithCapacity:capacity];
                        }
                    }
                    else
                    {
                        if (length && self->dataMessage && self->dataMessage.length < self->header[3])
                        {
                            NSUInteger len = self->header[3] - self->dataMessage.length;
                            
                            if (len > length)
                            {
                                len = length;
                            }
                            
                            [self->dataMessage appendBytes:bytes length:len];
                            
                            bytes += len;
                            length -= len;
                        }
                    }
                    
                    if (self->headerLen == sizeof(self->header) && self->header[3] == (self->dataMessage ? self->dataMessage.length : 0))
                    {
                        NSMutableData *messageData = self->dataMessage;
                        self->dataMessage = NULL;
                        self->headerLen = 0;
                        [self receiveMessage:self->header[0] sourceId:self->header[1] destinationId:self->header[2] data:messageData];
                    }

                    break;
                default:
                    length=0;
                    break;
            }
        }
        
        if (dataWrite && dataWrite.length)
        {
            write(1, dataWrite.bytes, dataWrite.length);
        }
        
        [fileHandle waitForDataInBackgroundAndNotify];
    }
    else
    {
        [[NSNotificationCenter defaultCenter]
         removeObserver:self
         name:NSFileHandleDataAvailableNotification
         object:fileHandle];

        [fileHandle closeFile];

        self->pipeStdout = NULL;
        self.serverState = 3;
        
        [self serverRunning:FALSE];
    }
}

-(void)handleDataAvailableStderr:(NSNotification *)notification
{
    NSFileHandle *fileHandle = [notification object];
    NSData *data=[fileHandle availableData];

    if (data.length > 0)
    {
        const UInt8 *bytes=data.bytes;
        NSUInteger length=data.length;
        
        while (length)
        {
            NSUInteger lfAt = nextByteMatch(bytes, length, 0xA);
            
            if (lfAt == NSUIntegerMax)
            {
                [self->dataStderr appendBytes:bytes length:length];
                break;
            }
            else
            {
                if (lfAt)
                {
                    [self->dataStderr appendBytes:bytes length:lfAt];
                    bytes += lfAt;
                    length -= lfAt;
                }
                
                if (self->dataStderr.length)
                {
                    NSString *stringFromData = [[NSString alloc] initWithData:self->dataStderr encoding:NSUTF8StringEncoding];

                    NSLog(@"%@", stringFromData);
                }
                
                length--;
                bytes++;

                self->dataStderr = [[NSMutableData alloc] init];
            }
        }

        [fileHandle waitForDataInBackgroundAndNotify];
    }
    else
    {
        [[NSNotificationCenter defaultCenter]
         removeObserver:self
         name:NSFileHandleDataAvailableNotification
         object:fileHandle];

        [fileHandle closeFile];

        self->pipeStderr = NULL;
    }
}

-(void)handleDataStdin:(NSObject *)something
{
    NSFileHandle *fileHandle = [self->pipeStdin fileHandleForWriting];
    self->pipeStdin = NULL;

    [self->writeCondition lock];
    
    while (self->taskPowerShell)
    {
        NSData *data = NULL;
        
        if (self->writeArray.count)
        {
            data = [self->writeArray firstObject];
            [self->writeArray removeObjectAtIndex:0];
            [self->writeCondition unlock];
            [fileHandle writeData:data];
            [self->writeCondition lock];
        }
        else
        {
            [self->writeCondition wait];
        }
    }
    
    [self->writeCondition unlock];
    
    [fileHandle closeFile];
}

-(void)queueMessage:(int)packetType sourceId:(int)sourceId destinationId:(int)destinationId data:(NSData *)data
{
    int headerInts[4] = {packetType, sourceId, destinationId, data ? (int)data.length : 0};
    NSData *header=[NSData dataWithBytes:headerInts length:sizeof(headerInts)];
    [self->writeCondition lock];
    [self->writeArray addObject:header];
    if (headerInts[3])
    {
        [self->writeArray addObject:data];
    }
    [self->writeCondition signal];
    [self->writeCondition unlock];
}

-(void)receiveMessage:(int)packetType sourceId:(int)sourceId destinationId:(int)destinationId data:(NSMutableData *)data
{
    switch (packetType)
    {
        case 0x41: // pair complete
            for (Document *doc in [[NSDocumentController sharedDocumentController] documents])
            {
                if (doc.state == 1 && destinationId == doc.clientId)
                {
                    doc.state = 2;
                    doc.runspaceId = sourceId;
                }
            }

            for (DoScriptCommand *script in self->pendingCommands)
            {
                if (script.state == 1 && destinationId == script.clientId)
                {
                    script.state = 2;
                    script.runspaceId = sourceId;
                }
            }
            break;

        case 0x40:
        case 0x43:
        case 0x44:
        case 0x42:
        case 0x45:
        case 0x49:
            {
                DoScriptCommand *scriptFound = NULL;

                for (DoScriptCommand *script in self->pendingCommands)
                {
                    if (script.state == 2 && destinationId == script.clientId && sourceId == script.runspaceId)
                    {
                        scriptFound = script;
                        break;
                    }
                }

                if (scriptFound)
                {
                    [scriptFound receiveMessage:packetType sourceId:sourceId destinationId:destinationId data:data];

                    if (scriptFound.state != 2)
                    {
                        [self->pendingCommands removeObject:scriptFound];
                    }
                }
                else
                {
                    if (data && packetType != 0x49 && packetType != 0x42 && packetType != 0x45)
                    {
                        [data appendBytes:"\n" length:1];
                    }

                    for (Document *doc in [[NSDocumentController sharedDocumentController] documents])
                    {
                        if (doc.state == 2 && destinationId == doc.clientId && sourceId == doc.runspaceId)
                        {
                            [doc receiveMessage:packetType sourceId:sourceId destinationId:destinationId data:data];
                        }
                    }
                }
            }
            
            break;

        case 0x47: // new document with content
            {
                NSError *error = nil;
                DocumentRequest *request = [[DocumentRequest alloc]init];
                
                request.packetType = packetType;
                request.clientId = destinationId;
                request.runspaceId = sourceId;
                request.data = data;
                
                [self->pendingRequests addObject:request];
                
                Document *document = [[NSDocumentController sharedDocumentController] openUntitledDocumentAndDisplay:TRUE error:&error];
                
                if (!document)
                {
                    [self->pendingRequests removeObject:request];
                    [self queueMessage:0x52 sourceId:destinationId destinationId:sourceId data:NULL];
                }
            }
            break;

        case 0x48: // open existing file
            {
                NSString *filePath = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                NSURL *fileURL = [NSURL fileURLWithPath:filePath];
                DocumentRequest *request = [[DocumentRequest alloc]init];
                
                request.packetType = packetType;
                request.clientId = destinationId;
                request.runspaceId = sourceId;
                request.fileURL = fileURL;
                
                [self->pendingRequests addObject:request];

                [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:fileURL display:TRUE completionHandler:^(NSDocument * _Nullable document, BOOL documentWasAlreadyOpen, NSError * _Nullable error)
                {
                    if (error || documentWasAlreadyOpen)
                    {
                        [self->pendingRequests removeObject:request];
                        [self queueMessage:0x52 sourceId:destinationId destinationId:sourceId data:NULL];
                    }
                }];
            }
            break;
    }
}

-(void)startPairing:(NSDocument *)document
{
    Document *doc=(Document *)document;

    if (doc.state == 0)
    {
        int packetType;
        DocumentRequest *request = NULL;
        doc.clientId = nextClientId(self);
        
        for (DocumentRequest *req in self->pendingRequests)
        {
            if (doc.fileURL && req.fileURL && [doc.fileURL isEqual:req.fileURL])
            {
                request = req;
                break;
            }
            else
            {
                if (!doc.fileURL && !req.fileURL)
                {
                    request = req;
                    break;
                }
            }
        }
        
        if (request)
        {
            [self->pendingRequests removeObject:request];
            request.document = doc;

            NSData *data=request.data;
            doc.runspaceId = request.runspaceId;
            doc.state = 2;
                        
            if (data)
            {
                [doc receiveMessage:request.packetType sourceId:doc.runspaceId destinationId:doc.clientId data:data];
            }
            
            packetType = 0x55;
        }
        else
        {
            doc.state = 1;
            packetType = 0x51;
        }
            
        [self queueMessage:packetType sourceId:doc.clientId destinationId:doc.runspaceId data:self->dataInitialScript];
    }
}

-(AppDelegate *)init
{
    AppDelegate *result=[super init];

    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];

    self->writeArray = [[NSMutableArray alloc] init];
    self->writeCondition = [[NSCondition alloc] init];
    self->dataStdout = [[NSMutableData alloc] init];
    self->dataStderr = [[NSMutableData alloc] init];
    self->dataSignature = [@"0c63fba6-7c2a-4b72-8de0-b3bd579dedaa" dataUsingEncoding:NSUTF8StringEncoding];
    self->pendingRequests = [[NSMutableArray alloc]init];
    self->pendingCommands = [[NSMutableArray alloc]init];

    NSString *bundlePath = [[NSBundle mainBundle] bundlePath];

    NSDictionary *appDefaults = @{
        keyToolServerLaunchPath:@"/usr/local/bin/pwsh",
        keyInitialScript:[@"if ($PSStyle) { $PSStyle.OutputRendering = 'PlainText' } ; Import-Module '%BUNDLEPATH%/Contents/Resources/Modules/rhubarb-geek-nz.MPWShell/rhubarb-geek-nz.MPWShell.psd1'" stringByReplacingOccurrencesOfString:@"%BUNDLEPATH%" withString:bundlePath],
        keyToolServerArguments:@[
            @"-NoLogo",
            @"-NoProfile",
            @"-NonInteractive",
            @"-Command",
            [@"$ErrorActionPreference = 'Stop' ; Import-Module '%BUNDLEPATH%/Contents/Resources/Modules/rhubarb-geek-nz.ToolServer/rhubarb-geek-nz.ToolServer.psd1' ; Invoke-MPWShell.ToolServer -Protocol 0c63fba6-7c2a-4b72-8de0-b3bd579dedaa" stringByReplacingOccurrencesOfString:@"%BUNDLEPATH%" withString:bundlePath]
        ],
        keyToolServerEnvironment:@{
            @"POWERSHELL_TELEMETRY_OPTOUT": @"true",
            @"POWERSHELL_UPDATECHECK": @"Off"
        }
    };

    [userDefaults registerDefaults:appDefaults];

    NSString *initialScript = [userDefaults stringForKey:keyInitialScript];
    self->dataInitialScript = [initialScript dataUsingEncoding:NSUTF8StringEncoding];

    return result;
}

- (void) serverRunning:(BOOL)isRunning
{
    for (Document *doc in [[NSDocumentController sharedDocumentController] documents])
    {
        [doc serverRunning:isRunning];
    }
}

-(id)startScript:(NSScriptCommand *)script
{
    id result = NULL;
    NSString *scriptArgument = [script directParameter];

    if (scriptArgument && [scriptArgument isKindOfClass:[NSString class]])
    {
        if (scriptArgument.length)
        {
            [script suspendExecution];

            DoScriptCommand *scriptCommand = (DoScriptCommand *)script;

            scriptCommand.state = 1;
            scriptCommand.clientId = nextClientId(self);

            NSData *payload = [scriptArgument dataUsingEncoding:NSUTF8StringEncoding];

            [self->pendingCommands addObject:scriptCommand];

            [self queueMessage:0x51 sourceId:scriptCommand.clientId destinationId:0 data:payload];
        }
        else
        {
            result = @"";
        }
    }
    else
    {
        [script setScriptErrorNumber:NSArgumentsWrongScriptError];
        [script setScriptErrorString:@"The do script command expects a text parameter."];
    }

    return result;
}
@end
