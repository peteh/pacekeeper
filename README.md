# Homebrain PaceKeeper for WalkingPads in Home Assistant

PaceKeeper is an ESP32 bridge that connects via Bluetooth to your WalkingPad and exposes the device via MQTT to Home Assistant.

![Home Assistant PaceKeeper Device](doc/ha_device.png)

## Supported Hardware

* PitPat-T01 Treadmill - Superun BA06-B1 [[AliExpress](https://s.click.aliexpress.com/e/_c3V1ssrv)]

## Required Tools

* ESP32 - I'm using a Wemos S3 Mini but any ESP32 with bluetooth should do [[AliExpress](https://de.aliexpress.com/item/1005006646247867.html)] [[Amazon](https://amzn.to/44VolhQ)]
* VS Code with Platform IO

## Setup

### Find the Bluetooth Address of the Device

Get an app like nRFConnect - this app allows to show bluetooth connections on your phone.

* Turn the device on with the power switch
* Either use the app to initialize the device or follow the steps in the section "Cloud Free Usage"
* Open nRFConnect on your phone
* The device should show up as "PitPat-T01"
* Write down the Bluetooth address (it should look like AA:BB:CC:11:22)

### Preparation of Homeassistant for MQTT

* Add the MQTT integration and follow the setup steps <https://www.home-assistant.io/integrations/mqtt>

### Project Compilation

* Setup VSCode with PlatformIO (<https://docs.platformio.org/en/latest/integration/ide/vscode.html#installation>)
* Clone this repo and open it in VSCode
* rename `config.h.sample` to `config.h`
* Open the file and set the configuration values for MQTT and the address from the previous step
* Connect the ESP32 with a USB cable (you might have to hold RST and BOOT while plugging it in)
* Compile and flash the project via PlatformIO -> Upload and Monitor
* If everything goes well you should see a bunch of log messages and a new device called `PaceKeeper` should show up in your Home Assistant

## Cloud Free Usage - Start without WiFi, App and Cloud Account

 You'll get the remote with it, it'll have +, - and play/pause buttons. However, when you turn it on, it just reacts with a long annoying sound to any button press. When you turn it on with the power button, it will also take a while before showing the display info, first turning on all the segments on the display.

That's where you strike!

You turn it on, and quickly press (+); you will be greeted with a short sound. Then you press: -, -, -, +, +, then wait 20s, turn it off and on again - should now display something else and you can start using it.

Sequence:

* Turn on with `power` switch on the device
* 3x press `-` on the remote
* 1x press `+` on the remote
* 1x press `+` on the remote for 3s

Each good input will be confirmed by a short happy sound. Each wrong input will be confirmed by a long annoying sound.

Source: <https://www.reddit.com/r/treadmills/comments/1jtuwix/heres_how_you_unlock_superun_treadmills_without/>

## Acknowledgements

I built this with the help of a lot of other people that put effort into reverse engineering the Bluetooth protocol.

### Web Bluetooth App

Python web interface to control the treadmill via Bluetooth.

GitHub project: <https://github.com/azmke/pitpat-treadmill-control>

## Web Bluetooth APP in JS

A web Bluetooth app written in JS. Fully supports the B1 as well.

GitHub project: <https://github.com/KeiranY/PitPat-WebBT/>

## Zwift Integration by qdomyos

There is some work in a B1 subbranch.

Source File: <https://github.com/cagnulein/qdomyos-zwift/blob/master/src/devices/deeruntreadmill/deerruntreadmill.cpp>

## Further Notes

Deerrun and Superun seem to use the same OEM hardware. So it's likely that those devices might work too.
