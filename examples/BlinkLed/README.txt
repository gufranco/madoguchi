BlinkLed: a page on the SD card that turns an LED on and off.

You need an Uno R3 or Uno R4 Minima, an Ethernet shield with a microSD slot,
and an LED wired through a resistor from pin 7 to GND.

Steps:

  a. Pick a free address on your network and put it in BlinkLed.ino.
  b. Put a FAT16 or FAT32 microSD card in the shield and upload the sketch.
  c. Send the page to the card:

       sh upload.sh 192.168.1.177

     Windows users run upload.bat with the same argument; curl.exe is part of
     Windows 10 1803 and newer.

  d. Browse to http://192.168.1.177/ and click the bulb.

The page asks GET /led for {"led":0} or {"led":1} and changes the state with
POST /led and a body of 0 or 1. The bulb is drawn in SVG inside the page, so
the card holds three small files and nothing else.
