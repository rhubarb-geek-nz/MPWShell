// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import <Carbon/Carbon.h>
#import "MPSWindowController.h"
#import "Document.h"
#import "AppDelegate.h"

@interface MPSWindowController ()

@end

@implementation MPSWindowController
{
}

static NSView *findViewWithIdentifier(NSView *view, NSString *identifier)
{
    if ([identifier isEqualToString:view.identifier])
    {
        return view;
    }

    for (NSView * subview in [view subviews])
    {
        NSView *found = findViewWithIdentifier(subview, identifier);
        
        if (found)
        {
            return found;
        }
    }
    
    return NULL;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    AppDelegate *appDelegate = [NSApp delegate];
    Document *document = self.document;
    NSTextStorage *storage = document.textStorage;
    NSTextView *textView = (NSTextView *)findViewWithIdentifier(self.window.contentView, @"MPSTextView");
    
    self.textView = textView;
    
    if (@available(macOS 15.0, *))
    {
        [textView setWritingToolsBehavior:NSWritingToolsBehaviorNone];
        [textView setAutomaticQuoteSubstitutionEnabled:FALSE];
        [textView setAutomaticDashSubstitutionEnabled:FALSE];
        [textView setAutomaticTextReplacementEnabled:FALSE];
        [textView setAutomaticSpellingCorrectionEnabled:FALSE];
    }

    [self.window setRestorable:FALSE];
    
    if (textView)
    {
        NSFont *font = document.font;
        textView.delegate = self;
        [textView setFont:font];
        textView.typingAttributes = @{
            NSFontAttributeName: font,
            NSForegroundColorAttributeName: [NSColor textColor]
        };

        if (storage)
        {
            [textView.layoutManager replaceTextStorage:storage];
            textView.selectedRange = NSMakeRange(0,0);
        }
        
        if (appDelegate.serverState != 2)
        {
            [textView setEditable:FALSE];
        }
    }
    
    [appDelegate startPairing:document];
}

- (void)textDidChange:(NSNotification *)notification
{
    Document *document = self.document;
    [document updateChangeCount:NSChangeDone];
    NSRange range = self.textView.selectedRange;
    document.textCursor = range.location + range.length;
}

- (void)cancel:(id)sender
{
    Document *document = self.document;
    [document queueMessage:0x54 data:NULL];
}

@end
