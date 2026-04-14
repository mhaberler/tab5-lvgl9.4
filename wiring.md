SK6812 x  = GPIO 20

https://shop.m5stack.com/products/rgb-unit

Relay x 2 = 23,27

https://shop.m5stack.com/products/2-channel-spst-relay-unit?srsltid=AfmBOorUuhrt1bviDAu5uk6ArR3vd2aZPgXzRARLv2GpDpBpKgZdmcFY

grove connector i2c: SDA=32,SCL=33

| Pin Type       | GPIO Numbers        | Why treat them as "Input Only"?                                                                      |
| -------------- | ------------------- | ---------------------------------------------------------------------------------------------------- |
| Strapping Pins | GPIO 45, 46, 47     | These pins control the boot mode (e.g., voltage regulators, JTAG enable). If you use them as outputs to drive a heavy load, you might accidentally pull the pin high/low during a reboot and "brick" the boot process until the load is removed. |
| JTAG Pins      | GPIO 24, 25, 26, 27 | If you are debugging, these are tied up. Using them as outputs will break your ability to debug the chip. |
| Crystal Pins   | LP_GPIO 0, 1        | These are often used for the 32kHz external crystal. If a crystal is populated, these are physically occupied and cannot be used for I/O. |