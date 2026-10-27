FileUpload: send files to the SD card with HTTP PUT and get them back with GET.

You need an Uno R3 or Uno R4 Minima and an Ethernet shield with a microSD slot.

Steps:

  a. Pick a free address on your network and put it in FileUpload.ino.
  b. Put a FAT16 or FAT32 microSD card in the shield and upload the sketch.
  c. Send the sample site in html/ to the card:

       sh upload.sh 192.168.1.177

     or send a single file:

       curl -T index.htm http://192.168.1.177/upload/index.htm

     Windows users run upload.bat with the same argument.

  d. Browse to http://192.168.1.177/.

The SD library stores 8.3 names only: at most eight characters, a dot, and at
most three more. A path such as /upload/docs/a.htm lands in the docs folder if
that folder is already on the card. A path with a . or .. segment is answered
with 400 Bad Request.
