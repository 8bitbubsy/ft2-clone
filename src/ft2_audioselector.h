#pragma once

#include <stdint.h>

#define AUDIO_SELECTORS_BOX_WIDTH 247

void setToDefaultAudioOutputDevice(void);
void setToDefaultAudioInputDevice(void);
char *getAudioOutputDeviceFromConfig(void);
char *getAudioInputDeviceFromConfig(void);
bool saveAudioDevicesToConfig(const char *inputString, const char *outputString);
bool testAudioDeviceListsMouseDown(void);
void rescanAudioDevices(void);
void scrollAudInputDevListUp(void);
void scrollAudInputDevListDown(void);
void scrollAudOutputDevListUp(void);
void scrollAudOutputDevListDown(void);
void sbAudOutputSetPos(uint32_t pos);
void sbAudInputSetPos(uint32_t pos);
void freeAudioDeviceLists(void);
void freeAudioDeviceSelectorBuffers(void);
