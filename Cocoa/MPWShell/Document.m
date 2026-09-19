// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import "Document.h"
#import "MPSWindow.h"
#import "MPSWindowController.h"
#import "AppDelegate.h"

@interface Document ()

@end

@implementation Document {
}

- (instancetype)init
{
    self = [super init];

    if (self)
    {
        CGFloat fontSize= [NSFont systemFontSize];
        self.font = [NSFont userFixedPitchFontOfSize:fontSize];
        self.textStorage = [[NSTextStorage alloc] init];
    }

    return self;
}

+ (BOOL)autosavesInPlace
{
    return NO;
}

- (void)makeWindowControllers
{
    MPSWindowController *customWC = [[MPSWindowController alloc] initWithWindowNibName:@"Document"];
    [self addWindowController:customWC];
}

- (NSData *)dataOfType:(NSString *)typeName error:(NSError **)outError
{
    NSTextStorage *storage = self.textStorage;
    NSString *value=storage.string;
    NSData *result = [value dataUsingEncoding:NSUTF8StringEncoding];
    return result;
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError
{
    NSTextStorage *storage = self.textStorage;
    NSRange range = NSMakeRange(0,storage.length);
    NSString *value = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    NSDictionary *attributes = @{
        NSFontAttributeName: self.font,
        NSForegroundColorAttributeName: [NSColor textColor]
    };
    NSAttributedString *styledString = [[NSAttributedString alloc] initWithString:value attributes:attributes];
    [storage beginEditing];
    [storage replaceCharactersInRange:range withAttributedString:styledString];
    [storage endEditing];
    [self updateChangeCount:NSChangeCleared];
    self.textCursor = 0;
    return YES;
}

- (void) close
{
    AppDelegate *appDelegate = [NSApp delegate];
    int closeMessage = 0;
    
    switch (self.state)
    {
        case 1:
            closeMessage = 0x53;
            break;
        case 2:
            closeMessage = 0x52;
            break;
    }
    
    self.state = 3;
    
    if (closeMessage)
    {
        [appDelegate queueMessage:closeMessage sourceId:self.clientId destinationId:self.runspaceId data:NULL];
    }
        
    [super close];    
}

- (void)receiveMessage:(int)packetType sourceId:(int)sourceId destinationId:(int)destinationId data:(NSData *)data
{
    NSString *value = NULL;
    
    switch (packetType)
    {
        case 0x40:
        case 0x43:
        case 0x44:
        case 0x47:
        case 0x49:
            if (data)
            {
                value = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            }
            else
            {
                value = @"\n";
            }
            
            if (packetType == 0x49) // show in alert
            {
                for (NSWindowController *c in [self windowControllers])
                {
                    MPSWindow *mainWindow = (MPSWindow *)c.window;
                    [mainWindow showAlert:value];
                    break;
                }
            }
            else
            {
                NSTextStorage *storage = self.textStorage;
                NSRange range = NSMakeRange(self.textCursor,0);
                NSDictionary *attributes = @{
                    NSFontAttributeName: self.font,
                    NSForegroundColorAttributeName: [NSColor textColor]
                };
                NSAttributedString *styledString = [[NSAttributedString alloc] initWithString:value attributes:attributes];
                [storage beginEditing];
                [storage replaceCharactersInRange:range withAttributedString:styledString];
                [storage endEditing];
                [self updateChangeCount:NSChangeDone];

                if (packetType != 0x47)
                {
                    self.textCursor += value.length;
                }
                
                for (NSWindowController *c in [self windowControllers])
                {
                    MPSWindowController *wc = (MPSWindowController *)c;
                    NSTextView *textView = wc.textView;
                    
                    if (packetType == 0x47)
                    {
                        textView.selectedRange = NSMakeRange(0,0);
                    }
                    else
                    {
                        NSRange range = textView.selectedRange;
                        
                        if (self.textCursor >= range.location && self.textCursor <= (range.location+range.length))
                        {
                            [textView scrollRangeToVisible:range];
                        }
                    }
                }
            }
            break;
        case 0x42:
            if (self->_state < 3)
            {
                self->_state = 3;

                for (NSWindowController *c in [self windowControllers])
                {
                    MPSWindowController *wc = (MPSWindowController *)c;
                    NSTextView *textView = wc.textView;
                    [textView setEditable:FALSE];
                }

                dispatch_async(dispatch_get_main_queue(), ^{
                    [self canCloseDocumentWithDelegate:self shouldCloseSelector:@selector(document:shouldClose:contextInfo:) contextInfo:NULL];
                });
            }

            break;
    }
}

-(void)document:(NSDocument *)doc shouldClose:(BOOL)shouldClose contextInfo:(void *)contextInfo
{
    if (shouldClose)
    {
        [doc close];
    }
}

- (void) queueMessage:(int)packetType data:(NSData *)data
{
    AppDelegate *appDelegate = [NSApp delegate];

    [appDelegate queueMessage:packetType sourceId:self.clientId destinationId:self.runspaceId data:data];
}

- (void) serverRunning:(BOOL)isRunning;
{
    for (NSWindowController *c in [self windowControllers])
    {
        MPSWindowController *wc = (MPSWindowController *)c;
        NSTextView *textView = wc.textView;
        
        [textView setEditable:isRunning];
    }
}

@end
