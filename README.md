# Plafo
Controller for aquarium lighting Arduino based

![image](https://user-images.githubusercontent.com/72757865/147971744-aab9381f-98f7-4b00-9662-3b5d4668cdb7.png)

The controller is composed by a power supply 24Vdc, 4 LED drives dimmable, a plastic shell 3D printed and a PCB with a Wemos D1 mini, a RTC DS3132 and a DC-DC converter to feed the Wemos and the RTC.

![image](https://user-images.githubusercontent.com/72757865/147972137-0f49f197-e394-4ca2-94e6-e40f3d6ac98c.png)

Every LED driver has a dimmer input that if connected to ground switch off the LED. I used 4 channels PWM of the Wemos to drive the dimmer inputs

![image](https://user-images.githubusercontent.com/72757865/147972356-028ee18b-7543-4831-812e-425efa965028.png)

It manages 4 lamps. It has a webpage as human interface and it has a RTC on board.
In case of RTC not working or router not available the lamps continue to work properly

![image](https://user-images.githubusercontent.com/72757865/147966070-f1a625ea-ebae-466b-9dd4-a6846fe6554c.png)
![image](https://user-images.githubusercontent.com/72757865/147966271-d11f6ccb-12dd-49d9-85e6-d1ef96d5afc7.png)

All material need is available on Amazon or Aliexpress

![image](https://user-images.githubusercontent.com/72757865/147972629-0ed0b4da-59aa-40cf-b696-ce234409129c.png)

In this repository you can also found the 3D model of the plastic shell
