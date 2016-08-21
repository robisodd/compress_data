'use strict';  // jshint ignore:line
var zip = require('./zip.js');

// ------------------------------------------------------------------------------------------------------------------------------------------------ //
//  Constants
// ------------------------------
const COLORTYPE_GREYSCALE   = 0;  // bitdepth: 1, 2, 4, 8, 16  Each pixel is a greyscale sample
const COLORTYPE_COLOR       = 2;  // bitdepth:          8, 16  Each pixel is an R,G,B triple
const COLORTYPE_INDEXED     = 3;  // bitdepth: 1, 2, 4, 8      Each pixel is a palette index; a PLTE chunk shall appear.
const COLORTYPE_GREY_ALPHA  = 4;  // bitdepth:          8, 16  Each pixel is a greyscale sample followed by an alpha sample.
const COLORTYPE_COLOR_ALPHA = 6;  // bitdepth:          8, 16  Each pixel is an R,G,B triple followed by an alpha sample.

const COMPRESSION_DEFLATE   = 0;

const FILTERING_ADAPTIVE    = 0;

const INTERLACE_NONE        = 0;
const INTERLACE_ADAM7       = 1;



// ------------------------------------------------------------------------------------------------------------------------------------------------ //
// Generate CRC Table and Calculate CRC
// Adapted from: https://www.w3.org/TR/PNG/#D-CRCAppendix
// ------------------------------
var crc_table = [];
for (var n = 0; n < 256; n++) {
  var c = n;
  for (var k = 0; k < 8; k++)
    if (c & 1)
      c = 0xedb88320 ^ (c >>> 1);
    else
      c = c >>> 1;
  crc_table[n] = c;
}

function crc(buf) {
  var c = 0xffffffff;
  for (var n = 0; n < buf.length; n++)
    c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >>> 8);
  return c ^ 0xffffffff;
}


function dword(dword) {
  return [(dword >>> 24) & 0xFF,
          (dword >>> 16) & 0xFF,
          (dword >>>  8) & 0xFF,
          (dword       ) & 0xFF];
}


function create_chunk(type, data) {
  return Array.prototype.concat(dword(data.length), type, data, dword(crc(type.concat(data))));
}


function create_header(width, height, bit_depth) {
  return Array.prototype.concat(dword(width),
                   dword(height),
                   [bit_depth,
                    COLORTYPE_GREYSCALE,
                    COMPRESSION_DEFLATE,
                    FILTERING_ADAPTIVE,
                    INTERLACE_NONE]
                  );
}



var w, h;
function parse_data(data) {
  if(w < 2000) {  // 2000 bytes @ 8 pixels / bit = 16000px wide image.  Almost too big for Pebble.
    console.log("Size already small enough: (" + w + "w x " + h + "h)");
    return [0].concat(data);  // 0 = No Filter
  }
  
  while (w>=2000) {
    h *= 2;
    w = Math.ceil(w / 2);
  }
  console.log("New dimensions: (" + w + "w x " + h + "h)");
  
  var arr = [];
  for (var y = 0; y < h; ++y) {
    arr.push(0);  // 0 = No Filter.  Needs to be at the beginning of every row
    for (var x = 0; x < w; x++) {
      if (y*w+x >= data.length)  // If we've reached the end of the data
        arr.push(0);  // pad image's bottom row to the end of the width
      else
        arr.push(data[y*w+x]);
    }
  }
  return arr;
}


// Spec from: https://www.w3.org/TR/PNG/
// data should be an array of bytes
function generate(data) {
  w = data.length; h = 1;
  var compressed_array = new zip.deflate(parse_data(data)).compress();
  var compressed_data = Array.prototype.slice.call(compressed_array);  // Convert TypedArray to standard JS array
  //console.log("Size before / after compression: " + data.length + " / " + compressed_data.length);
  
  var SIGNATURE = [137, 80, 78, 71, 13, 10, 26, 10];           // PNG Signature
  var IHDR_data = create_header(w*8, h, 1);        // PNG Header
  var IHDR = create_chunk([73, 72, 68, 82], IHDR_data);        // PNG Header
  var IDAT = create_chunk([73, 68, 65, 84], compressed_data);  // PNG Compressed Data
  var IEND = create_chunk([73, 69, 78, 68], []);               // PNG End
  return Array.prototype.concat(SIGNATURE, IHDR, IDAT, IEND);  // Return PNG in a byte array
}




exports.generate = generate;
