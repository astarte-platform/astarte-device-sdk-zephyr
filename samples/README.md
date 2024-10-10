<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

## Samples organization

The sample app in this folder contains code to send all data types supported by astarte.
The sample folder also contains:
- Board specific overlays and configurations. Contained in the `common/boards` folder.
- Common source code for generic Ethernet/Wifi connectivity, TLS settings plus some utilities for
  generation of standard datasets.

Some configuration and cmake utilites are stored in a shared `common` folder that does not contain
any valid zephyr application. This folder also contains common Astarte interfaces shared by the
samples and the tests. These interfaces have been designed to be generic in order for the
`astarte_app` sample to demonstrate as much functionality as possible.
The interfaces are defined in JSON filed sontained in the `common/interfaces` folder.
In addition to the JSON version of the interfaces, an auto-generated version of the same interfaces
is contained in the `generated_interfaces` header/source files. Those files have been generated
running the `west generate-interfaces` command and should not be modified manually.

### Sample behaviour

The sample contains three threads:
- A master application thread that will handle Ethernet/Wifi reconnection.
- A secondary thread that will manage the Astarte device and handle reception of data from Astarte.
- The third thread that sends data to astarte of the configured types.

The sample will connect to Astarte and remain connected for the time it takes to send the data.
Intervals between send of different types of data can be configured.
Additionally if no data type transmission is enabled the device will be connected for a specified
timeout to allow reception of test messages.
After the operational time of the device has concluded, the device will disconnect and the sample
will terminate.

Take a look a the [Kconfig](astarte_app/Kconfig) file or use menuconfig to test out different transmission
types and timeouts.

## Samples configuration

### Configuration for demonstration non-TLS capable Astarte

This option assumes you are using this example with an Astarte instance similar to the
one explained in the
[Astarte in 5 minutes](https://docs.astarte-platform.org/astarte/latest/010-astarte_in_5_minutes.html)
tutorial.

The following entries should be modified in the `proj.conf` file.
```conf
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP=y
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT=y
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_DEVICE_ID="<DEVICE_ID>"
CONFIG_CREDENTIAL_SECRET="<CREDENTIAL_SECRET>"
```
Where `<DEVICE_ID>` is the device ID of the device you would like to use in the sample, `<HOSTNAME>`
is the hostname for your Astarte instance, `<REALM_NAME>` is the name of your testing realm and
`<CREDENTIAL_SECRET>` is the credential secret obtained through the manual registration.

### Configuration for fully TLS capable Astarte

This option assumes you are using a fully deployed Astarte instance with valid certificates from
an official certificate authority. All the samples assume the root CA certificate for the MQTT
broker to be the same as the root CA certificate for all HTTPs APIs.

```conf
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=2
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_DEVICE_ID="<DEVICE_ID>"
CONFIG_CREDENTIAL_SECRET="<CREDENTIAL_SECRET>"
```
Where `<DEVICE_ID>` is the device ID of the device you would like to use in the sample, `<HOSTNAME>`
is the hostname for your Astarte instance, `<REALM_NAME>` is the name of your testing realm and
`<CREDENTIAL_SECRET>` is the credential secret obtained through the manual registration.

In addition, the file `ca_certificates.h` should be modified, placing in the `ca_certificate_root`
array a valid CA certificate in the PEM format.

### Use native_sim with net-tools

The net-setup.sh script can setup an ethernet interface to the host. This net-setup.sh script will need to be run as a root user.

```
./net-setup.sh --config nat.conf
```
