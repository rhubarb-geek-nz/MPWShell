// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import <Carbon/Carbon.h>
#import "MPSWindow.h"
#import "MPSWindowController.h"
#import "Document.h"

@interface MPSWindow ()

@end

@implementation MPSWindow
{
    NSMutableArray<NSString *> *messageQueue;
}

static void mainWindowExecute(MPSWindow *self)
{
    MPSWindowController *wc= self.windowController;
    NSTextView *textView = wc.textView;
    Document *document=wc.document;
    
    NSRange range = textView.selectedRange;
    NSString *content=textView.string;
    
    if (!range.length)
    {
        range = [content lineRangeForRange:range];
        
        if (range.length)
        {
            if ([content characterAtIndex:range.location+range.length-1] == 0xA)
            {
                range=NSMakeRange(range.location, range.length-1);
            }
        }
    }
        
    if (range.length)
    {
        NSString *script=[content substringWithRange:range];
        
        if (script)
        {
            NSData* data = [script dataUsingEncoding:NSUTF8StringEncoding];
            
            [document queueMessage:0x50 data:data];
        }
    }
            
    NSFont *font = document.font;
    NSTextStorage *storage = document.textStorage;
    NSDictionary *attributes = @{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: [NSColor textColor]
    };
    NSAttributedString *styledString = [[NSAttributedString alloc] initWithString:@"\n" attributes:attributes];
    NSRange newRange = NSMakeRange(range.location + range.length, 0);
    [storage beginEditing];
    [storage replaceCharactersInRange:newRange withAttributedString:styledString];
    [storage endEditing];
    
    newRange = NSMakeRange(range.location + range.length + 1, 0);
    textView.selectedRange = newRange;
    [textView scrollRangeToVisible:newRange];
    
    [document updateChangeCount:NSChangeDone];
    document.textCursor=newRange.location+newRange.length;
}

-(void)keyDown:(NSEvent *)event
{
    switch (event.keyCode)
    {
        case kVK_ANSI_KeypadEnter:
            mainWindowExecute(self);
            return;
        case kVK_Return:
            if (event.modifierFlags & NSEventModifierFlagCommand)
            {
                mainWindowExecute(self);
                return;
            }
            break;
    }

    [super keyDown:event];
}

static void showAlertSheet(MPSWindow *self, NSString *message)
{
    NSAlert *alert = [[NSAlert alloc] init];
    
    [alert setIcon:[[NSApplication sharedApplication] applicationIconImage]];
    [alert setMessageText:@"MPW Shell"];
    [alert setInformativeText:message];
    [alert setAlertStyle:NSAlertStyleInformational];
    [alert addButtonWithTitle:@"OK"];

    [alert beginSheetModalForWindow:self completionHandler:^(NSModalResponse returnCode)
    {
        if (returnCode == NSAlertFirstButtonReturn)
        {
            if (self->messageQueue.count)
            {
                NSString *message=[self->messageQueue objectAtIndex:0];
                [self->messageQueue removeObjectAtIndex:0];
                showAlertSheet(self, message);
            }
            else
            {
                self->messageQueue = NULL;
            }
        }
    }];
}

- (void) showAlert:(NSString *)message
{
    if (self->messageQueue)
    {
        [self->messageQueue addObject:message];
    }
    else
    {
        self->messageQueue=[[NSMutableArray alloc]init];
        showAlertSheet(self, message);
    }
}

@end
