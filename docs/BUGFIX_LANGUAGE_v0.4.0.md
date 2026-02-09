# Language Settings Bug Fix - v0.4.0

## Date

February 4, 2026

## Issues Fixed

### 1. Language Combobox Scrollbar Issue

**Problem**: The language combobox dropdown didn't show a scrollbar, making it impossible to see languages outside the visible area (9 languages total: zh-CN, zh-TW, en, ja, ko, de, fr, es, ru).

**Root Cause**: The combobox dropdown height was set to 120 pixels, which wasn't sufficient to display all 9 languages. Additionally, the CBS_DISABLENOSCROLL style wasn't set.

**Fix**:

- Increased dropdown height from 120 to 200 pixels
- Added `CBS_DISABLENOSCROLL` and `WS_VSCROLL` styles to ensure scrollbar is always visible
- Modified `src/support/settings_window.cpp` line ~1630

### 2. Language Selection Not Taking Effect

**Problem**: When user selected a language from the combobox (e.g., Chinese), the UI would update temporarily but the change wouldn't persist. After closing and reopening the settings window, the language would revert to the previous selection.

**Root Cause**: The language change handler (`IDC_LANGUAGE_COMBO` case in `WM_COMMAND`) was updating the UI and calling `localizationManager_->setLanguage()`, but it wasn't saving the configuration to disk. The change would only be saved if the user clicked "Apply" or "OK" button.

**Fix**:

- Added immediate config save when language changes
- Modified `src/support/settings_window.cpp` line ~900-920
- Now calls `configManager_->setLanguage()` and `configManager_->save()` immediately when language selection changes
- This ensures the language preference is persisted immediately

### 3. Duplicate Header Declaration

**Problem**: Compilation error due to duplicate `HFONT uiFont_;` declaration in header file.

**Fix**: Removed duplicate declaration in `include/support/settings_window.h`

### 4. Lambda Function Calling Convention Issue (x86 build)

**Problem**: x86 build failed because lambda functions don't have the correct calling convention (`__stdcall`) required by Windows API callbacks like `EnumChildWindows`.

**Fix**:

- Replaced lambda functions with proper callback function `EnumChildProc`
- Added `CALLBACK` calling convention to ensure compatibility with Windows API
- Modified `applyUIFont()` method in `src/support/settings_window.cpp`

## Files Modified

1. `src/support/settings_window.cpp`
    - Line ~1630: Updated language combobox creation with better styles and height
    - Line ~900-920: Added immediate config save in language change handler
    - Line ~2735: Fixed `applyUIFont()` to use proper callback instead of lambda

2. `include/support/settings_window.h`
    - Line 118-119: Removed duplicate `uiFont_` declaration

## Build Information

**Version**: 0.4.0
**Build Date**: February 4, 2026, 19:32

### Checksums

- **x64**: MD5 = 289f82c34bbaf7656e5a8f634b7cb13c (1.9MB)
- **x86**: MD5 = ec728a4b2c6873150ac987e5bf669ea8 (2.1MB)

## Testing Notes

To test the fixes:

1. **Language Combobox Scrollbar**:
    - Open Settings window
    - Click on Language tab
    - Click the language dropdown
    - Verify that all 9 languages are visible with scrollbar

2. **Language Persistence**:
    - Open Settings window
    - Select a different language (e.g., 简体中文)
    - Verify UI updates immediately
    - Close Settings window
    - Reopen Settings window
    - Verify the selected language is still active
    - Check `config.json` to confirm language is saved

3. **UI Font Application**:
    - Verify all controls in Settings window use consistent system font
    - Check all tabs and nested controls

## Deployment

Files have been copied to `/Volumes/Test/ClawDeskMCP/` for Windows testing:

- `ClawDeskMCP-x64.exe` (1.9MB)
- `ClawDeskMCP-x86.exe` (2.1MB)

## Related Issues

This fix addresses user feedback from the previous deployment where:

1. Language list had no scrollbar
2. Language selection didn't persist after closing settings
3. Style modifications were made by Codex (already reviewed and compiled)
