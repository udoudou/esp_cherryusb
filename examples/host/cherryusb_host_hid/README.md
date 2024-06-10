# CherryUSB Host HID Example

Receive and print data reported by HID devices.

(See the [README.md](../../README.md) file in the upper level 'examples' directory for more information about examples.)

## How to use example

Follow detailed instructions provided specifically for this example. 

Select the instructions depending on Espressif chip installed on your development board:

- [ESP32-S2 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)
- [ESP32-S3 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html)

Building an example is the same as building any other project:

- Run `idf.py set-target TARGET` to select the correct chip target to build before opening the project configuration menu. By default the target is `esp32`. For all options see `idf.py set-target --help`
- Run `idf.py menuconfig` to open the project configuration menu. Most examples have a project-specific "Example Configuration" section here (for example, to set the WiFi SSID & password to use).
- `idf.py build` to build the example.
- Follow the printed instructions to flash, or run `idf.py -p PORT flash`.

## Example output

When running, the example will print the following output:

```
I [I/USB] cherryusb, version: 0x010300
[I/USB] ========== dwc2 hcd params ==========
[I/USB] CID:00000000
[I/USB] GSNPSID:4f54400a
[I/USB] GHWCFG1:00000000
[I/USB] GHWCFG2:224dd930
[I/USB] GHWCFG3:00c804b5
[I/USB] GHWCFG4:d3f0a030
[I/USB] dwc2 has 8 channels and dfifo depth(32-bit words) is 200
(342) host_main: usb host init done
I (382) main_task: Returned from app_main()
[I/usbh_hub] New full-speed device on Bus 0, Hub 1, Port 1 connected
[I/usbh_core] New device found,idVendor:258a,idProduct:0049,bcdDevice:0200
[I/usbh_core] The device has 1 bNumConfigurations
[I/usbh_core] The device has 2 interfaces
[I/usbh_core] Enumeration success, start loading class driver
[I/usbh_core] Loading hid class driver
[I/usbh_hid] Ep=81 Attr=03 Mps=8 Interval=01 Mult=00
[I/usbh_hid] Register HID Class:/dev/input0
[I/usbh_core] Loading hid class driver
I (6892)[I/usbh_hid] Ep=82 Attr=03 Mps=16 Interval=01 Mult=00
[I/usbh_hid] Register HID Class:/dev/input1
 host_main: intf 0, SubClass 1, Protocol 1
I (6902) host_main: intf 1, SubClass 0, Protocol 0
abcd
```

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-idf/issues)

We will get back to you as soon as possible.
