// ------------------------------------------------------------------------------------------------------------------------------------------------ //
//  Includes
// ------------------------------
//var MessageQueue = require('message-queue-pebble');  // For switching to Common-JS-Style
//var Zlib = require('zlibjs');  // <-- I need to make this a pebble package
var png_compressor = require('./png.js');
var MessageQueue   = require('./MessageQueue.js');
var lots_of_text   = require('./lots_of_text.js');

// ------------------------------------------------------------------------------------------------------------------------------------------------ //
//  Pebble Functions
// ------------------------------
Pebble.addEventListener("ready", function(e) {
  console.log("PebbleKit JS Has Started!");
  MessageQueue.sendAppMessage({"MESSAGE":"JavaScript Connected!"}, null, null);  // let watch know JS is ready
});



const COMMAND_SEND_TEXT = 10;
const COMMAND_BLOW_UP_MARS = 11;

Pebble.addEventListener("appmessage", function(e) {
  if (typeof e.payload.MESSAGE !== 'undefined') {
    console.log("Received message: " + e.payload.MESSAGE);
  }
  
  // Received Command from Pebble
  if (typeof e.payload.COMMAND !== 'undefined') {
    if(e.payload.COMMAND === COMMAND_SEND_TEXT) {
      console.log("Command = SEND TEXT!");
      send_some_text_to_pebble();
    } else if(e.payload.COMMAND === COMMAND_BLOW_UP_MARS) {
      console.log("Command = BLOW UP MARS!");
    } else {
      console.log("Received Unknown command from Pebble C: " + e.payload.COMMAND);
    }
  }
});


// ------------------------------------------------------------------------------------------------------------------------------------------------ //
//  Data Functions
// ------------------------------

// function create_text() {
//   var str = "hello bob!" + "\n";
//   var second = "how are you??" + "\n";
//   str = str+str+second+str+second+second+str+str+str+second+str+str+str+str + "\0";
//   var data = [];
//   for (var i = 0; i < str.length; i++)
//     data.push(str.charCodeAt(i));
//   return data;
// }

// function create_data() {
//   var DATA_LENGTH = 8000;
//   var data = [];
//   for (var i = 0; i < DATA_LENGTH; i++)
//     data.push((i % 26) + 65);
//   return data;
// }


function send_some_text_to_pebble() {
  // Step 1: Generate the text
  // var data = create_data();            // Generate data locally
  // var data = create_text();            // Generate data locally
  var data = lots_of_text.create_data();  // Generate a lot of text from external file
  // Step 2: Compress the data into a PNG
  var png = png_compressor.generate(data);
  console.log("Size before / after compression: " + data.length + " / " + png.length);
  
  // Step 3: Send the data to watch as a PNG
  send_data_to_pebble_in_pieces(png, 
                                function(){console.log("Successfully sent PNG to pebble");},
                                function(){console.log("Failed sending PNG to pebble");}
                               );
}



function send_data_to_pebble_in_pieces(data, success_callback, error_callback) {
  var PIECE_MAX_SIZE = 1000;
  var bytes = 0;
  var send_piece = function() {
    if(bytes >= data.length) {success_callback(); return;}
    var piece_size = data.length - bytes > PIECE_MAX_SIZE ? PIECE_MAX_SIZE : data.length - bytes;
    var piece = data.slice(bytes, bytes + piece_size);
    // Send piece and if successful, call this function again to send next piece
    MessageQueue.sendAppMessage({"JIGSAW_PIECE_INDEX":bytes, "JIGSAW_PIECE":piece}, send_piece, error_callback);
    console.log("Sending " + bytes + "/" + data.length);
    bytes += piece_size;
  };
  // Send size (which tell's C to start new), then send first piece
  MessageQueue.sendAppMessage({"JIGSAW_INIT": data.length}, send_piece, error_callback);
}
