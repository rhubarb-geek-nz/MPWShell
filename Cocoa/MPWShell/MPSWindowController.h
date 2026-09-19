// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

#import <Cocoa/Cocoa.h>

@interface MPSWindowController : NSWindowController<NSTextViewDelegate>

@property (nonatomic, weak) NSTextView *textView;

@end
