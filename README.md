# wink-relay-manager
Manages most features of a Wink Relay natively and sends events to MQTT
![Wink Relay](https://raw.githubusercontent.com/jimpastos/wink-relay-manager/master/wink-relay.jpg)

This project is based on the work done on [wink-relay-handler](https://github.com/mjg59/wink-relay-handler/)

Building
--------
Download the Android NDK and install <br />
Edit build.sh and set ANDROID_NDK path <br />
Run ./build.sh

Installing
----------

You'll need adb access to a rooted Wink Relay. Disable the existing Wink control software by running


```
pm disable http://com.quirky.android.wink.projectone
```

as root. Remount /system read-write:

```
mount -o rw,remount /system
```

Delete /system/bin/edisonwink:

```
rm /system/bin/edisonwink
```

adb push libs/armeabi-v7a/wink_manager to /sdcard and then copy it over edisonwink and fix permissions:

```
cp /sdcard/wink_manager /system/bin/edisonwink
chmod 755 /system/bin/edisonwink
```

Reboot after setting up wink_manager.ini in /sdcard
```
reboot
```

Configuration
--
You'll need to create an ini file on /sdcard/wink_manager.ini
```
mqtt_clientid=Relay
mqtt_topic_prefix=Relay
mqtt_address=tcp://<host>:<port>
relay_upper_flags=1
relay_lower_flags=2
```
For MQTT credentials, specify the below optional fields
```
mqtt_username=<user>
mqtt_password=<password>
```
To set an initial state of a relay on startup, specify the following fields.
```
initial_relay_upper_state=1
initial_relay_lower_state=0
```
To send additional triggers and states
```
send_proximity_trigger=true
send_screen_state=true
```
To change default behaviors
```
hide_status_bar=true
screen_timeout=20
proximity_threshold=5000
temperature_threshold=100
humidity_threshold=100
```
If an initial state is not specified, the current state will be preserved

Boolean config values can be either 1, yes, true or 0, no, false (case insensitive)

Relay upper and lower flags indicate the preferred functionality per relay/button

| Flag | Bit Value | Description |
| --- | --- | --- |
| RELAY_FLAG_NONE | 000000 | Does nothing when you press a button |
| RELAY_FLAG_TOGGLE | 000001 | Toggles the relay when button is pressed |
| RELAY_FLAG_SEND_CLICK | 000010 |Sends the click event to MQTT |
| RELAY_FLAG_SEND_HELD | 000100 | Sends the held event to MQTT |
| RELAY_FLAG_SEND_RELEASE |001000 |Sends the release event to MQTT |
| RELAY_FLAG_GRAB_TOUCH_INPUT | 010000 | Toggle blocking screen touches when double clicked |
| RELAY_FLAG_TOGGLE_OPPOSITE | 100000 | Toggles the opposite relay when button is pressed when combined with RELAY_FLAG_TOGGLE |

Flags need to be ORed to combine functionality
```
relay_xxxxx_flags=1 // Just toggle the relay on button click
relay_xxxxx_flags=2 // Just send click events
relay_xxxxx_flags=3 // Toggle and send click events (0001 | 0010) = 0011 => 3
relay_xxxxx_flags=35 // Toggle opposite and send click events (000001 | 000010 | 100000) = 100011 => 35
```

MQTT Topics
--------
##### Sensor events will be posted to:
```
<MQTTPrefix>/sensors/temperature
<MQTTPrefix>/sensors/humidity
<MQTTPrefix>/relays/0/state
<MQTTPrefix>/relays/1/state
<MQTTPrefix>/screen/state // if enabled will be ON or OFF
<MQTTPrefix>/proximity/trigger // if enabled will be the sensor value that triggered it
```
#####  Button events are posted to different topics
```
<MQTTPrefix>/buttons/<button>/<action>/<clicks>

where:
<button> is: 0 or 1
<action> is: click, held or released
<clicks> is: the number of clicks detected
```
#####  Control topics
```
<MQTTPrefix>/relays/<relay>
<MQTTPrefix>/screen/
<MQTTPrefix>/command/exit // exits the process (any payload)
<MQTTPrefix>/command/reboot // reboots the device (any payload)

where:
<relay> is: 0 or 1
MQTT payloads:
1 or case insensive "on" for enabling
or 0 or case insensitive "off" for disabling
```
#####  Logging and Debug
Logs default to using logcat

Debug logging can be enabled by adding the following config line
```
debug=true
```
To change logging from logcat to a file add a "log_file" line in the config. E.g:
```
log_file=/data/local/tmp/wink_manager.log
```
