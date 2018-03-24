# wink-relay-manager
Manages most features of a Wink Relay natively and sends events to MQTT
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
screen_timeout=20
proximity_threshold=5000
hide_status_bar=true
relay_upper_flags=1
relay_lower_flags=2
```

Relay upper and lower flags indicate the preferred functionality per relay/button

| Flag | Bit Value | Description |
| --- | --- | --- |
| RELAY_FLAG_NONE | 0000 | Does nothing when you press a button |
| RELAY_FLAG_TOGGLE | 0001 | Toggles the relay when button is pressed |
| RELAY_FLAG_SEND_CLICK | 0010 |Sends the click event to MQTT |
| RELAY_FLAG_SEND_HELD | 0100 | Sends the held event to MQTT |
| RELAY_FLAG_SEND_RELEASE |1000 |Sends the release event to MQTT |

Flags need to be ORed to combine functionality
```
relay_xxxxx_flags=1 // Just toggle the relay on button click
relay_xxxxx_flags=2 // Just send click events
relay_xxxxx_flags=3 // Toggle and send click events (0001 | 0010) = 0011 => 3
```

MQTT Topics
--------
##### Sensor events will be posted to:
```
<MQTTPrefix>/sensors/temperature
<MQTTPrefix>/sensors/humidity
<MQTTPrefix>/relays/0/state
<MQTTPrefix>/relays/1/state
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

where:
<relay> is: 0 or 1
MQTT payloads:
1 or case insensive "on" for enabling
or 0 or case insensitive "off" for disabling
```
