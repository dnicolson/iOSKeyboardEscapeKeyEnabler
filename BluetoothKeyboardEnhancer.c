// clang -framework IOKit -framework Carbon BluetoothKeyboardEnhancer.c -o BluetoothKeyboardEnhancer
// ./BluetoothKeyboardEnhancer

#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDValue.h>

void TriggerEscKey()
{
    CGEventRef event = CGEventCreateKeyboardEvent(NULL, kVK_Escape, true);
    CGEventPost(kCGSessionEventTap, event);
    CFRelease(event);
}

void TriggerForceQuit()
{
    system("osascript -l JavaScript -e \"Application('System Events').processes['Finder'].menuBars[0].menus['Apple'].menuItems['Force Quit…'].click()\" 1>/dev/null");
}

void TriggerForceQuitFrontmostApp()
{
    pid_t pid;
    ProcessSerialNumber psn;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    GetFrontProcess(&psn);
    GetProcessPID(&psn, &pid);
#pragma clang diagnostic pop

    killpg(getpgid(pid), SIGTERM);
}

void TriggerEmojiPicker()
{
    CGEventRef keyboard_down_event = CGEventCreateKeyboardEvent(NULL, kVK_Space, true);
    CGEventRef keyboard_up_event = CGEventCreateKeyboardEvent(NULL, kVK_Space, false);

    CGEventSetFlags(keyboard_down_event, kCGEventFlagMaskCommand | kCGEventFlagMaskControl);
    CGEventSetFlags(keyboard_up_event, 0);

    CGEventPost(kCGHIDEventTap, keyboard_down_event);
    CGEventPost(kCGHIDEventTap, keyboard_up_event);

    CFRelease(keyboard_down_event);
    CFRelease(keyboard_up_event);
}

CFMutableDictionaryRef CreateMatchingDictionary(UInt32 usage_page, UInt32 usage)
{
    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFNumberRef page_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage_page);
    CFDictionarySetValue(dictionary, CFSTR(kIOHIDDeviceUsagePageKey), page_number);
    CFRelease(page_number);

    CFNumberRef usage_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
    CFDictionarySetValue(dictionary, CFSTR(kIOHIDDeviceUsageKey), usage_number);
    CFRelease(usage_number);

    return dictionary;
}

void HIDKeyboardCallback(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
    IOHIDElementRef elem = IOHIDValueGetElement(value);
    uint32_t usage_page = IOHIDElementGetUsagePage(elem);
    uint32_t usage = IOHIDElementGetUsage(elem);
    long pressed = IOHIDValueGetIntegerValue(value);
    static int shift_down, ctrl_down, option_down, command_down;

    if (usage_page == kHIDPage_KeyboardOrKeypad) {
        switch (usage) {
            case kHIDUsage_KeyboardLeftShift:
            case kHIDUsage_KeyboardRightShift:
                shift_down = pressed;
                break;
            case kHIDUsage_KeyboardLeftControl:
                ctrl_down = pressed;
                break;
            case kHIDUsage_KeyboardLeftAlt:
            case kHIDUsage_KeyboardRightAlt:
                option_down = pressed;
                break;
            case kHIDUsage_KeyboardLeftGUI:
            case kHIDUsage_KeyboardRightGUI:
                command_down = pressed;
                break;
        }

        if (ctrl_down && command_down && usage == -1 && pressed == 0x10101010101 /*1103823438081*/) {
            TriggerEmojiPicker();
        }
    }

    if (usage_page == kHIDPage_Consumer && usage == kHIDUsage_Csmr_ACHome && pressed == 1) {
        if (option_down && command_down) {
            if (shift_down) {
                TriggerForceQuitFrontmostApp();
            } else {
                TriggerForceQuit();
            }
        } else {
            TriggerEscKey();
        }
    }
}

bool CheckAccessibility()
{
#ifdef MAC_OS_X_VERSION_10_9
    const void* keys[] = { (void*)kAXTrustedCheckOptionPrompt };
    const void* vals[] = { (void*)kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(NULL, keys, vals, 1, NULL, NULL);
    return AXIsProcessTrustedWithOptions(options);
#else
    return AXAPIEnabled();
#endif
}

int main()
{
    IOHIDManagerRef hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    CFMutableDictionaryRef matching_dictionary = CreateMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
    IOHIDManagerSetDeviceMatching(hid_manager, matching_dictionary);
    IOHIDManagerRegisterInputValueCallback(hid_manager, HIDKeyboardCallback, NULL);
    IOHIDManagerScheduleWithRunLoop(hid_manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(hid_manager, kIOHIDOptionsTypeNone);
    if (CheckAccessibility()) {
        CFRunLoopRun();
    }
}
