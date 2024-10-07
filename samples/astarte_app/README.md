<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Astarte app sample

This sample shows how transmission and reception of astarte supported types can be performed
using the provided APIs.

The trasmission is divided in four steps that can be enabled indipendently to test out the
device and server behaviour. Kconfig values contol the compilation of these steps, here
are presented in the order in which they are executed:
- [registration](#Registration): Calls the sdk api to register a device given a pairing jwt token.
- [datastream individual send](#Individuals): Transmit individual data to astarte.
- [datastream object send](#Objects): Transmit object data to astarte.
- [properties set/unset individual](#Properties): set and unset individual properties.

Each one of those steps has a corresponding configuration menu that lets you enable
and disable it's compilation. It is also possible to configure a timeout in second to
wait for before sending each type of data.
If no transmission is enabled it is possible to configure a timeout to to wait for
before disconnecting the device and stopping the sample. This allows the device
to log messages received from astarte.
Run menuconfig to try out different values.

## Required configuration

Refer to the
[generic samples readme](https://github.com/astarte-platform/astarte-device-sdk-zephyr/tree/master/samples/README.md)
to configure correctly the device.

### Configuration of secrets in ignored files

Some of the data configured in the `prj.conf` files could be private.
Data like wifi passwords or even the astarte credential secret could be configured in a `private.conf` file.
You can have a `private.conf` file in the [astarte_app](https://github.com/astarte-platform/astarte-device-sdk-zephyr/tree/master/samples/astarte_app).
This file is specified in the `.gitignore` and won't be committed.

To configure the wifi in a file that is ignored by git you can add the following to `samples/astarte_app/private.conf`
```conf
# WiFi credentials
CONFIG_WIFI_SSID=""
CONFIG_WIFI_PASSWORD=""
```

## Sample configurable steps

### Registration

This step can be enabled through the `Registration` menu and shows how to register a new device to
a remote Astarte instance using the pairing APIs provided in this library.
It furthermore shows how connection is established and how callbacks can be used to verify
the connection status.

### Individuals

This step shows how transmission of individual datastreams can be performed using the provided APIs.

#### Manually send server datastream data

By leveraging [`astartectl`](https://github.com/astarte-platform/astartectl) we can send test
data to the device.
We recomend storing the JWT in a shell variable to access it with ease.
```bash
TOKEN=<appengine_token>
```

The syntax to use the send-data subcommand is the following:
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerDatastream \
    /boolean_endpoint true
```

Where:
- `<REALM>` is your realm name
- `<DEVICE_ID>` is the device ID to send the data to
- `<API_URL>` is the Astarte api endpoint

Another option, to see a simple "hello world" message displayed on the serial port connected to the
device is:
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerDatastream \
    /stringarray_endpoint '["hello", "world!"]'
```

### Objects

This step shows how transmission of aggregate datastreams can be performed using the provided APIs.

#### Manually send test data

By leveraging [`astartectl`](https://github.com/astarte-platform/astartectl) we can send test
data to the device.
We recomend storing the JWT in a shell variable to access it with ease.
```bash
TOKEN=<appengine_token>
```

The syntax to use the send-data subcommand is the following:
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerAggregate \
    /sensor11 \
    '{"double_endpoint": 459.432, "integer_endpoint": 32, "boolean_endpoint": true, \
    "longinteger_endpoint": 45993543534, "string_endpoint": "some value", \
    "binaryblob_endpoint": "aGVsbG8gd29ybGQ=", "doublearray_endpoint": [23.45, 543.12, 33.1, 0.1], \
    "booleanarray_endpoint": [true, false], "stringarray_endpoint": ["hello", "world"], \
    "binaryblobarray_endpoint": ["aGVsbG8gd29ybGQ=", "aGVsbG8gd29ybGQ="]}'
```

Where:
- `<REALM>` is your realm name
- `<DEVICE_ID>` is the device ID to send the data to
- `<API_URL>` is the Astarte api endpoint

### Properties

This step shows how to set and unset properties.

#### Manually send test data

By leveraging [`astartectl`](https://github.com/astarte-platform/astartectl) we can send test
data to the device.
We recomend storing the JWT in a shell variable to access it with ease.
```bash
TOKEN=<appengine_token>
```

The syntax to use the send-data subcommand is the following:
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerProperty \
    /sensor11/integer_endpoint <VALUE>
```

Where:
- `<REALM>` is your realm name
- `<DEVICE_ID>` is the device ID to send the data to
- `<API_URL>` is the Astarte api endpoint
- `<VALUE>` is the new value for the property

The device is configured to react to four different property values in different ways.
All other values will be ignored.
- `40` The device sets some device owned properties
- `41` The device sets some device owned properties
- `42` The device unsets the device owned properties set with `40`
- `43` The device unsets the device owned properties set with `41`
