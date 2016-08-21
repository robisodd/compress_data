# compress_data
Compress large amounts of data on phone before sending from phone to pebble

Works by compressing the data via DEFLATE(RFC1951) and sends to Pebble as a PNG image.  
Pebble uncompresses it using the built-in: `gbitmap_create_from_png_data`  


Normally the data is sent as a PNG of height 1 pixel, but since Pebble can't have an image larger than 16383 pixels wide, for particularly large amounts of data the javascript will double the height until the width fits.  It sends it as a 1bit monochrome PNG so the image width (in pixels) will be 8 times the number of bytes

Compression amount depends on the data being sent.  For the current Lorum Ipsum, the details are:  

    Originally: 19050 bytes of text
    Reduced to:  1583 bytes to xmit via PNG
    9528px (1191 bytes) wide
      16px (  16 bytes) high
    w * h =
    152448 pixels = 19056 bytes

19056 bytes is close to 19050, but a few left over since it doesn't fit perfectly in the rectangle of an image.  

Of course, for the Pebble to be able to decompress it, it needs 1583 bytes free to download the data plus 19050 bytes to hold the uncompressed data... plus a few bytes for working space.
