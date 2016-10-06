//******************************************************************
//
// Copyright 2014 Intel Mobile Communications GmbH All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "Arduino.h"
#include "logger.h"
#include "string.h"
#include "oc_logger.h"
#include "oc_console_logger.h"

static oc_log_ctx_t *logCtx = 0;

static oc_log_level LEVEL_XTABLE[] = {OC_LOG_DEBUG, OC_LOG_INFO, OC_LOG_WARNING, OC_LOG_ERROR, OC_LOG_FATAL};

static const uint16_t LINE_BUFFER_SIZE = (16 * 2) + 16 + 1;  // Show 16 bytes, 2 chars/byte, spaces between bytes, null termination

// Convert LogLevel to platform-specific severity level.  Store in PROGMEM on Arduino
#ifdef __ANDROID__
    static android_LogPriority LEVEL[] = {ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_FATAL};
#elif defined __linux__
    static const char * LEVEL[] __attribute__ ((unused)) = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
#elif defined ARDUINO
    #include <stdarg.h>

    PROGMEM const char level0[] = "DEBUG";
    PROGMEM const char level1[] = "INFO";
    PROGMEM const char level2[] = "WARNING";
    PROGMEM const char level3[] = "ERROR";
    PROGMEM const char level4[] = "FATAL";

    PROGMEM const char * const LEVEL[]  = {level0, level1, level2, level3, level4};

    static void OCLogString(LogLevel level, PROGMEM const char * tag, PROGMEM const char * logStr);
#ifdef ARDUINO_ARCH_AVR
    //Mega2560 and other 8-bit AVR microcontrollers
    #define GET_PROGMEM_BUFFER(buffer, addr) { strcpy_P(buffer, (char*)pgm_read_word(addr));}
#elif defined ARDUINO_ARCH_SAM
    //Arduino Due and other 32-bit ARM micro-controllers
    #define GET_PROGMEM_BUFFER(buffer, addr) { strcpy_P(buffer, (char*)pgm_read_dword(addr));}
#else
    #define GET_PROGMEM_BUFFER(buffer, addr) { buffer[0] = '\0';}
#endif
#endif // __ANDROID__


#ifndef ARDUINO

void OCLogConfig(oc_log_ctx_t *ctx) {
    logCtx = ctx;
}

void OCLogInit() {

}

void OCLogShutdown() {
#ifdef __linux__
    if (logCtx && logCtx->destroy)
    {
        logCtx->destroy(logCtx);
    }
#endif
}

/**
 * Output a variable argument list log string with the specified priority level.
 * Only defined for Linux and Android
 *
 * @param level  - DEBUG, INFO, WARNING, ERROR, FATAL
 * @param tag    - Module name
 * @param format - variadic log string
 */
void OCLogv(LogLevel level, const char * tag, const char * format, ...) {
    if (!format || !tag) {
        return;
    }
    char buffer[MAX_LOG_V_BUFFER_SIZE];
    memset(buffer, 0, sizeof buffer);
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer - 1, format, args);
    va_end(args);
    OCLog(level, tag, buffer);
}

/**
 * Output a log string with the specified priority level.
 * Only defined for Linux and Android
 *
 * @param level  - DEBUG, INFO, WARNING, ERROR, FATAL
 * @param tag    - Module name
 * @param logStr - log string
 */
void OCLog(LogLevel level, const char * tag, const char * logStr) {
    if (!logStr || !tag) {
        return;
    }

    #ifdef __ANDROID__
        __android_log_write(LEVEL[level], tag, logStr);
    #elif defined __linux__
        if (logCtx && logCtx->write_level)
        {
            logCtx->write_level(logCtx, LEVEL_XTABLE[level], logStr);

        }
        else
        {
            printf("%s: %s: %s\n", LEVEL[level], tag, logStr);
        }
    #endif
}

/**
 * Output the contents of the specified buffer (in hex) with the specified priority level.
 *
 * @param level      - DEBUG, INFO, WARNING, ERROR, FATAL
 * @param tag        - Module name
 * @param buffer     - pointer to buffer of bytes
 * @param bufferSize - max number of byte in buffer
 */
void OCLogBuffer(LogLevel level, const char * tag, const uint8_t * buffer, uint16_t bufferSize) {
    if (!buffer || !tag || (bufferSize == 0)) {
        return;
    }

    char lineBuffer[LINE_BUFFER_SIZE];
    memset(lineBuffer, 0, sizeof lineBuffer);
    int lineIndex = 0;
    int i;
    for (i = 0; i < bufferSize; i++) {
        // Format the buffer data into a line
        snprintf(&lineBuffer[lineIndex*3], sizeof(lineBuffer)-lineIndex*3, "%02X ", buffer[i]);
        lineIndex++;
        // Output 16 values per line
        if (((i+1)%16) == 0) {
            OCLog(level, tag, lineBuffer);
            memset(lineBuffer, 0, sizeof lineBuffer);
            lineIndex = 0;
        }
    }
    // Output last values in the line, if any
    if (bufferSize % 16) {
        OCLog(level, tag, lineBuffer);
    }
}

#else
    /**
     * Initialize the serial logger for Arduino
     * Only defined for Arduino
     */
    void OCLogInit() {
        Serial1.begin(9600);
	//Serial1.println("OCLogInit");
    }

    /**
     * Output a log string with the specified priority level.
     * Only defined for Arduino.  Only uses PROGMEM strings
     * for the tag parameter
     *
     * @param level  - DEBUG, INFO, WARNING, ERROR, FATAL
     * @param tag    - Module name
     * @param logStr - log string
     */
    void OCLogString(LogLevel level, PROGMEM const char * tag, const char * logStr) {
        if (!logStr || !tag) {
          return;
        }

        char buffer[LINE_BUFFER_SIZE];

        GET_PROGMEM_BUFFER(buffer, &(LEVEL[level]));
        Serial1.print(buffer);

        char c;
        Serial1.print(F(": "));
        while ((c = pgm_read_byte(tag))) {
          Serial1.write(c);
          tag++;
        }
        Serial1.print(F(": "));

        Serial1.println(logStr);
    }

    /**
     * Output the contents of the specified buffer (in hex) with the specified priority level.
     *
     * @param level      - DEBUG, INFO, WARNING, ERROR, FATAL
     * @param tag        - Module name
     * @param buffer     - pointer to buffer of bytes
     * @param bufferSize - max number of byte in buffer
     */
    void OCLogBuffer(LogLevel level, PROGMEM const char * tag, const uint8_t * buffer, uint16_t bufferSize) {
        if (!buffer || !tag || (bufferSize == 0)) {
            return;
        }

        char lineBuffer[LINE_BUFFER_SIZE] = {0};
        uint8_t lineIndex = 0;
        for (uint8_t i = 0; i < bufferSize; i++) {
            // Format the buffer data into a line
            snprintf(&lineBuffer[lineIndex*3], sizeof(lineBuffer)-lineIndex*3, "%02X ", buffer[i]);
            lineIndex++;
            // Output 16 values per line
            if (((i+1)%16) == 0) {
                OCLogString(level, tag, lineBuffer);
                memset(lineBuffer, 0, sizeof lineBuffer);
                lineIndex = 0;
            }
        }
        // Output last values in the line, if any
        if (bufferSize % 16) {
            OCLogString(level, tag, lineBuffer);
        }
    }

    void OCSimpleLog(const char * logStr){
	Serial1.println(logStr);
    }

    /**
     * Output a log string with the specified priority level.
     * Only defined for Arduino.  Uses PROGMEM strings
     *
     * @param level  - DEBUG, INFO, WARNING, ERROR, FATAL
     * @param tag    - Module name
     * @param logStr - log string
     */
    void OCLog(LogLevel level, PROGMEM const char * tag, PROGMEM const char * logStr) {
        if (!logStr || !tag) {
          return;
        }

        char buffer[LINE_BUFFER_SIZE];

        GET_PROGMEM_BUFFER(buffer, &(LEVEL[level]));
        Serial1.print(buffer);

        char c;
        Serial1.print(F(": "));
        while ((c = pgm_read_byte(tag))) {
          Serial1.write(c);
          tag++;
        }
        Serial1.print(F(": "));

        while ((c = pgm_read_byte(logStr))) {
          Serial1.write(c);
          logStr++;
        }
        Serial1.println();
    }

    /**
     * Output a variable argument list log string with the specified priority level.
     * Only defined for Arduino as depicted below.
     *
     * @param level  - DEBUG, INFO, WARNING, ERROR, FATAL
     * @param tag    - Module name
     * @param format - variadic log string
     */
    void OCLogv(LogLevel level, PROGMEM const char * tag, const char * format, ...)
    {
        char buffer[LINE_BUFFER_SIZE];
        va_list ap;
        va_start(ap, format);

        GET_PROGMEM_BUFFER(buffer, &(LEVEL[level]));
        Serial1.print(buffer);

        char c;
        Serial1.print(F(": "));

        while ((c = pgm_read_byte(tag))) {
            Serial1.write(c);
            tag++;
        }
        Serial1.print(F(": "));

        vsnprintf(buffer, sizeof(buffer), format, ap);
        for(char *p = &buffer[0]; *p; p++) // emulate cooked mode for newlines
        {
            if(*p == '\n')
            {
                Serial1.write('\r');
            }
            Serial1.write(*p);
        }
        Serial1.println();
        va_end(ap);
    }
    /**
     * Output a variable argument list log string with the specified priority level.
     * Only defined for Arduino as depicted below.
     *
     * @param level  - DEBUG, INFO, WARNING, ERROR, FATAL
     * @param tag    - Module name
     * @param format - variadic log string
     */
    void OCLogv(LogLevel level, PROGMEM const char * tag, const __FlashStringHelper *format, ...)
    {
        char buffer[LINE_BUFFER_SIZE];
        va_list ap;
        va_start(ap, format);

        GET_PROGMEM_BUFFER(buffer, &(LEVEL[level]));
        Serial1.print(buffer);

        char c;
        Serial1.print(F(": "));

        while ((c = pgm_read_byte(tag))) {
          Serial1.write(c);
          tag++;
        }
        Serial1.print(F(": "));

        #ifdef __AVR__
            vsnprintf_P(buffer, sizeof(buffer), (const char *)format, ap); // progmem for AVR
        #else
            vsnprintf(buffer, sizeof(buffer), (const char *)format, ap); // for the rest of the world
        #endif
        for(char *p = &buffer[0]; *p; p++) // emulate cooked mode for newlines
        {
            if(*p == '\n')
            {
                Serial1.write('\r');
            }
            Serial1.write(*p);
        }
        Serial1.println();
        va_end(ap);
    }


#endif

