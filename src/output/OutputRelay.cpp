/******************************************************************
*
*       Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel (And Serial!) driver
*       Orginal ESPixelStickproject by 2015 Shelby Merrick
*
*       Brought to you by:
*              Bill Porter
*              www.billporter.info
*
*       See Readme for other info and version history
*
*
*This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
<http://www.gnu.org/licenses/>
*
*This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
*To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or
*send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
******************************************************************/

#include "../ESPixelStick.h"
#include <utility>
#include <algorithm>
#include <math.h>

#include "OutputRelay.hpp"
#include "OutputCommon.hpp"

#include "../FileIO.h"

#   define Relay_OUTPUT_ENABLED         true
#   define Relay_OUTPUT_DISABLED        false
#   define Relay_OUTPUT_INVERTED        true
#   define Relay_OUTPUT_NOT_INVERTED    false
#   define Relay_DEFAULT_TRIGGER_LEVEL  128
#   define Relay_DEFAULT_GPIO_ID        ((gpio_num_t)-1)

static const c_OutputRelay::RelayChannel_t RelayChannelDefaultSettings[] =
{
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH},
    {Relay_OUTPUT_DISABLED, Relay_OUTPUT_INVERTED, Relay_DEFAULT_TRIGGER_LEVEL, Relay_DEFAULT_GPIO_ID, LOW, HIGH},
};

//----------------------------------------------------------------------------
c_OutputRelay::c_OutputRelay (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                gpio_num_t outputGpio, 
                                uart_port_t uart,
                                c_OutputMgr::e_OutputType outputType) :
    c_OutputCommon(OutputChannelId, outputGpio, uart, outputType)
{
    // DEBUG_START;
    memcpy((char*)OutputList, (char*)RelayChannelDefaultSettings, sizeof(OutputList));

    // DEBUG_END;
} // c_OutputRelay

//----------------------------------------------------------------------------
c_OutputRelay::~c_OutputRelay ()
{
    // DEBUG_START;

    for (RelayChannel_t currentRelay : OutputList)
    {
        if (currentRelay.Enabled)
        {
            pinMode (currentRelay.GpioId, INPUT);
        }
        currentRelay.Enabled = Relay_OUTPUT_DISABLED;
        currentRelay.GpioId  = Relay_DEFAULT_GPIO_ID;
    }

    // DEBUG_END;
} // ~c_OutputRelay

//----------------------------------------------------------------------------
void c_OutputRelay::Begin ()
{
    // DEBUG_START;
    String DriverName = ""; GetDriverName (DriverName);
    LOG_PORT.println (String (F ("** ")) + DriverName + F(" Initialization for Chan: ") + String (OutputChannelId) + F(" **"));

    SetOutputBufferSize (Num_Channels);

    validate ();

    // DEBUG_END;
}

//----------------------------------------------------------------------------
/*
*   Validate that the current values meet our needs
*
*   needs
*       data set in the class elements
*   returns
*       true - no issues found
*       false - had an issue and had to fix things
*/
bool c_OutputRelay::validate ()
{
    // DEBUG_START;
    bool response = true;

    if ((Num_Channels > OM_RELAY_CHANNEL_LIMIT) || (Num_Channels < 1))
    {
        LOG_PORT.println (String (F ("*** Requested channel count was Not Valid. Setting to ")) + OM_RELAY_CHANNEL_LIMIT + F(" ***"));
        Num_Channels = OM_RELAY_CHANNEL_LIMIT;
        response = false;
    }

    if (Num_Channels < OM_RELAY_CHANNEL_LIMIT)
    {
        LOG_PORT.println (String (F ("*** Requested channel count was Not Valid. Insuficient number of input channels avaialable ***")));

        for (int ChannelIndex = OM_RELAY_CHANNEL_LIMIT - 1; ChannelIndex > Num_Channels; ChannelIndex--)
        {
            LOG_PORT.println (String (String(F ("*** Disabling channel '")) + String(ChannelIndex + 1) + String("' ***")));
            OutputList[ChannelIndex].Enabled = false;
        }

        response = false;
    }

    SetOutputBufferSize (Num_Channels);

    for (RelayChannel_t currentRelay : OutputList)
    {
        if (currentRelay.GpioId == Relay_DEFAULT_GPIO_ID)
        {
            pinMode (currentRelay.GpioId, INPUT);
            currentRelay.Enabled = Relay_OUTPUT_DISABLED;
        }

        else if (currentRelay.Enabled)
        {
            pinMode (currentRelay.GpioId, OUTPUT);
        }

        if (currentRelay.InvertOutput)
        {
            currentRelay.OffValue = HIGH;
            currentRelay.OnValue  = LOW;
        }
        else
        {
            currentRelay.OffValue = LOW;
            currentRelay.OnValue  = HIGH;
        }
    } // for each output channel

    // DEBUG_END;
    return response;

} // validate

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputRelay::SetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;
    uint temp; // Holds enums prior to conversion

    do // once
    {
        FileIO::setFromJSON (UpdateInterval, jsonConfig[OM_RELAY_UPDATE_INTERVAL_NAME]);

        // do we have a channel configuration array?
        if (false == jsonConfig.containsKey (OM_RELAY_CHANNELS_NAME))
        {
            // if not, flag an error and stop processing
            LOG_PORT.println (F ("No Relay Output Channel Settings Found. Using Defaults"));
            break;
        }
        JsonArray JsonChannelList = jsonConfig[OM_RELAY_CHANNELS_NAME];

        for (JsonVariant JsonChannelData : JsonChannelList)
        {
            uint8_t ChannelId = OM_RELAY_CHANNEL_LIMIT;
            FileIO::setFromJSON (ChannelId, JsonChannelData[OM_RELAY_CHANNEL_ID_NAME]);

            // do we have a valid channel configuration ID?
            if (ChannelId >= OM_RELAY_CHANNEL_LIMIT)
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String(F ("No Relay Output Channel Settings Found for Channel '")) + String(ChannelId) + "'");
                continue;
            }

            FileIO::setFromJSON (OutputList[ChannelId].Enabled,           JsonChannelData[OM_RELAY_CHANNEL_ENABLED_NAME]);
            FileIO::setFromJSON (OutputList[ChannelId].InvertOutput,      JsonChannelData[OM_RELAY_CHANNEL_INVERT_NAME]);
            FileIO::setFromJSON (OutputList[ChannelId].OnOffTriggerLevel, JsonChannelData[OM_RELAY_CHANNEL_TRIGGER_NAME]);

            temp = OutputList[ChannelId].GpioId;
            FileIO::setFromJSON (temp, JsonChannelData[OM_RELAY_CHANNEL_GPIO_NAME]);
            OutputList[ChannelId].GpioId = (gpio_num_t)temp;
        }

    } while (false);

    // DEBUG_END;
    return validate ();

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputRelay::GetConfig (ArduinoJson::JsonObject & jsonConfig)
{
    // DEBUG_START;

    jsonConfig[OM_RELAY_UPDATE_INTERVAL_NAME] = UpdateInterval;

    JsonArray JsonChannelList = jsonConfig.createNestedArray (OM_RELAY_CHANNELS_NAME);

    uint8_t ChannelId = 0;
    for (RelayChannel_t currentRelay : OutputList)
    {
        JsonObject JsonChannelData = JsonChannelList.createNestedObject ();

        JsonChannelData[OM_RELAY_CHANNEL_ID_NAME]      = ChannelId++;
        JsonChannelData[OM_RELAY_CHANNEL_ENABLED_NAME] = currentRelay.Enabled;
        JsonChannelData[OM_RELAY_CHANNEL_INVERT_NAME]  = currentRelay.InvertOutput;
        JsonChannelData[OM_RELAY_CHANNEL_TRIGGER_NAME] = currentRelay.OnOffTriggerLevel;
        JsonChannelData[OM_RELAY_CHANNEL_GPIO_NAME]    = currentRelay.GpioId;
    }

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void  c_OutputRelay::GetDriverName (String & sDriverName)
{
    sDriverName = F ("Relay");

} // GetDriverName

//----------------------------------------------------------------------------
void c_OutputRelay::Render ()
{
    // DEBUG_START;

    uint8_t OutputDataIndex = 0;

    for (RelayChannel_t currentRelay : OutputList)
    {
        if (currentRelay.Enabled)
        {
            digitalWrite (currentRelay.GpioId, 
                ((pOutputBuffer[OutputDataIndex++] > currentRelay.OnOffTriggerLevel) ? currentRelay.OnValue : currentRelay.OffValue));
        }
    }

    // DEBUG_END;
} // render
