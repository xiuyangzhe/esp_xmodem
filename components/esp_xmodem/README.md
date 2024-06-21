### Xmodem/Ymodem protocol for ESP8266_RTOS_SDK and esp-idf

#### Feature

* Support Xmodem, Xmodem-1k and Ymodem protocol
* Support transmit the data over UART
* Offer the sender and receiver example

#### Introduce

The detailed Xmodem/Ymodem protocol can refer to `http://pauillac.inria.fr/~doligez/zmodem/ymodem.txt`

#### How to build

* Get the SDK for ESP8266 (Support release/v3.3, v3.3 and master branch)

    * `git clone -b master https://github.com/espressif/ESP8266_RTOS_SDK.git`

    * Prepare the environment for ESP8266_RTOS_SDK, the details please refer to `https://github.com/espressif/ESP8266_RTOS_SDK`

    * Get the Xmodem repository and enter examples to build example demo. The more details please refer to example README file

* Get the SDK for ESP32 (Support release/v4.0, release/4.1, release/v4.2 and master branch)

    * `git clone -b master https://github.com/espressif/esp-idf.git`

    * Prepare the environment for esp-idf, the details please refer to `https://github.com/espressif/esp-idf`

    * Get the Xmodem repository and enter examples to build example demo. The more details please refer to example README file

#### How to test

* In Linux, it is recommended to use rz/sz command to play a role of receiver and sender. Use following command to install rz/sz command.
```
sudo apt-get install lrzsz
```

* rz command can receive data/file and sz command can send data/file. Both of them support Xmodem, Ymodem and Zmodem. In this project example, user can use following command to send data/file to serial.
```
sz --xmodem/--ymodem (file name or pure data) >/dev/ttyUSB0 </dev/ttyUSB0
```
```
rz --ymodem >/dev/ttyUSB0 </dev/ttyUSB0
```
/dev/ttyUSB0 is depended on your tty serial number.

* The more details about rz/sz command, please refer to `--help`
