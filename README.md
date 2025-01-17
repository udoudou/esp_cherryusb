# CherryUSB Component and Examples for ESP

[![Component Registry](https://components.espressif.com/components/udoudou/esp_cherryusb/badge.svg)](https://components.espressif.com/components/udoudou/esp_cherryusb) [![Build Status](https://github.com/udoudou/esp_cherryusb/actions/workflows/build_examples.yml/badge.svg)](https://github.com/udoudou/esp_cherryusb/actions/workflows/build_examples.yml)

This is the component and examples repository for the [CherryUSB](https://github.com/cherry-embedded/CherryUSB), which is a tiny and portable USB Stack (device & host) for embedded system with USB IP.

This repository is wrapped as an ESP-IDF component and finally published to [Component Registry](https://components.espressif.com/).

You can build the examples in this repository with **ESP-IDF v4.4.1 or later** directly. Or using the ESP-IDF component manager.

## How to add this component from esp-registry to your project

Just add ``idf_component.yml`` to your main component with the following content::

```yaml
## IDF Component Manager Manifest File
dependencies:
  udoudou/esp_cherryusb: "*"
```

Or simply run:

```
idf.py add-dependency "udoudou/esp_cherryusb"
```

During the build process, the ESP-IDF build system will automatically download and install this component.

## How to download examples from esp-registry

Please use the component manager command `create-project-from-example` to create the project from example template

```
idf.py create-project-from-example "udoudou/esp_cherryusb=*:cherryusb_device_cdc"
```

## How to pull the repository directly as a component

Please create a `components` folder in the project root directory and pull the repository in the components directory.

```
git clone --recursive https://github.com/udoudou/esp_cherryusb.git
```

## How to build the examples

Please refer to the [examples/README.md](./examples/README.md)

## API Documentation

1. Library introduction can be found on [README](https://github.com/cherry-embedded/CherryUSB/blob/master/README.md) from the upstream CherryUSB.
2. Full API code documentations and step by step guides can be found in [CherryUSB Docs Website](https://cherryusb.readthedocs.io/).