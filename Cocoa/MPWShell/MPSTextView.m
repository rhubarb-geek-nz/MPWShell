// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import <Carbon/Carbon.h>
#import "MPSTextView.h"

@interface MPSTextView ()

@end

@implementation MPSTextView
{

}

-(void)keyDown:(NSEvent *)event
{
    switch (event.keyCode)
    {
        case kVK_ANSI_KeypadEnter:
            [self.window keyDown:event];
            return;
        case kVK_Return:
            if (event.modifierFlags & NSEventModifierFlagCommand)
            {
                [self.window keyDown:event];
                return;
            }
            break;
    }

    [super keyDown:event];
}

@end
