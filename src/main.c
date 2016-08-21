#include <pebble.h>

bool js_ready = false;
char *message_text;  // Display textbox on screen
char text_buffer[1000] = "";
uint8_t command = 0;

static Window *window;
static Layer        *root_layer;
static GBitmap      *image = NULL;

#define COMMAND_SEND_DATA 10



// ------------------------------------------------------------------------ //
//  Jigsaw Functions and Variables
// ------------------------------------------------------------------------ //
static uint8_t *jigsaw = NULL;
static uint32_t jigsaw_size = 0;


static void jigsaw_finished() {
  if (image) {
    gbitmap_destroy(image);
    image = NULL;
  }
  
  image = gbitmap_create_from_png_data(jigsaw, jigsaw_size);
  free(jigsaw); jigsaw = NULL;

  GRect gbsize = gbitmap_get_bounds(image);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Bitmap GRect = (%d, %d, %d, %d)", gbsize.origin.x, gbsize.origin.y, gbsize.size.w, gbsize.size.h);
  message_text = "";  // Remove message after data is downloaded
  layer_mark_dirty(root_layer);
}


static void init_jigsaw(uint32_t total_size) {
  if (jigsaw)
    free(jigsaw);
  if ((jigsaw = malloc(jigsaw_size = total_size)))
    APP_LOG(APP_LOG_LEVEL_DEBUG, "New data starting: size = %d bytes", (int)jigsaw_size);
  else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to allocate %d bytes", (int)jigsaw_size);
    jigsaw_size = 0;
  }
}


static void jigsaw_add_piece(uint8_t *piece, uint32_t piece_index, uint16_t piece_length) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Received Piece: index=%d/%d piece size=%d", (int)piece_index, (int)jigsaw_size, (int)piece_length);
  if (jigsaw) {
    memcpy(jigsaw + piece_index, piece, piece_length);

    // Display percentage downloaded (Add length cause it's already done downloading that part)
    snprintf(message_text, sizeof(message_text), "%d/%d bytes (%d%%)", (int)(piece_index+piece_length), (int)jigsaw_size, (int)((piece_index+piece_length)*100/jigsaw_size));
    layer_mark_dirty(root_layer);

    if(piece_index + piece_length >= jigsaw_size)
      jigsaw_finished();
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Can't add piece: Jigsaw not allocated! Discarding piece.");
  }
}


// ------------------------------------------------------------------------ //
//  AppMessage Functions
// ------------------------------------------------------------------------ //
static void send_command(uint8_t command) {
  printf("Sending Command: %d", command);
  if (!js_ready) {printf("Cannot send command: Javascript not ready"); return;}
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter)) {printf("Cannot send command: Error Preparing Outbox"); return;}
  if (dict_write_uint8(iter, MESSAGE_KEY_COMMAND, command)) {printf("Cannot send command: Failed to write uint8"); return;}
  dict_write_end(iter);
  app_message_outbox_send();
}


static void appmessage_in_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *jigsaw_init_tuple, *message_tuple, *jigsaw_piece_tuple, *jigsaw_index_tuple;
  js_ready = true;
  
  // If there's a message, display it on the screen
  if ((message_tuple = dict_find(iter, MESSAGE_KEY_MESSAGE))) {
    snprintf(text_buffer, sizeof(text_buffer), "%s", message_tuple->value->cstring);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received Message: %s", text_buffer);
    message_text = text_buffer;
    layer_mark_dirty(root_layer);
  }

  // If phone is starting to send new blob of data
  if ((jigsaw_init_tuple = dict_find(iter, MESSAGE_KEY_JIGSAW_INIT))) {
    init_jigsaw(jigsaw_init_tuple->value->uint32);
  }

  // Get jigsaw piece
  if ((jigsaw_piece_tuple = dict_find(iter, MESSAGE_KEY_JIGSAW_PIECE))) {
    if ((jigsaw_index_tuple = dict_find(iter, MESSAGE_KEY_JIGSAW_PIECE_INDEX))) {
      uint32_t piece_index = jigsaw_index_tuple->value->uint32;
      uint16_t piece_length = jigsaw_piece_tuple->length;
      uint8_t *piece = &jigsaw_piece_tuple->value->uint8;
      jigsaw_add_piece(piece, piece_index, piece_length);
    }
  }
}



static char *translate_appmessageresult(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}


static void appmessage_out_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  js_ready = false;
  APP_LOG(APP_LOG_LEVEL_ERROR, "App Message Failed: %s", translate_appmessageresult(reason));
}


static void appmessage_in_dropped_handler(AppMessageResult reason, void *context) {
  js_ready = false;
  APP_LOG(APP_LOG_LEVEL_ERROR, "App Message Failed: %s", translate_appmessageresult(reason));
}


static void app_message_init() {
  // Register message handlers
  app_message_register_inbox_received(appmessage_in_received_handler); 
  app_message_register_inbox_dropped(appmessage_in_dropped_handler); 
  app_message_register_outbox_failed(appmessage_out_failed_handler);
  //app_message_open(640, 640);  // <-- ought to be enough for anybody
  app_message_open(app_message_inbox_size_maximum(), APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
  // Size = 1 + 7 * N + size0 + size1 + .... + sizeN
  //app_message_open(1 + 7 * 2 + sizeof(int32_t) + PIECE_MAX_SIZE, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);

}





// ------------------------------------------------------------------------ //
//  Drawing Functions
// ------------------------------------------------------------------------ //
// Fill screen with color.  Note: For Aplite, color should be either 0 or 255. Vertical stripes will appear otherwise.
static void fill_screen(GContext *ctx, GColor color) {
  uint32_t *framebuffer = (uint32_t*)*(uintptr_t*)ctx;
  #if defined(PBL_PLATFORM_APLITE)
    memset(framebuffer, color.argb, 20 * 168);
    graphics_release_frame_buffer(ctx, graphics_capture_frame_buffer(ctx));  // Needed on Aplite to force screen to draw
  #elif defined(PBL_PLATFORM_BASALT)
    memset(framebuffer, color.argb, 144 * 168);
  #elif defined(PBL_PLATFORM_CHALK)
    memset(framebuffer, color.argb, 76 + 25792); // First pixel on PTR doesn't start til framebuffer + 76
  #endif
}

static void display_message_text(Layer *layer, GContext *ctx) {
  if (!message_text[0]) return;
  GColor background_color = GColorWhite;
  GColor text_color = GColorBlack;
  GColor border_color = GColorBlack;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GSize margin = GSize(3, 2);
  GRect layer_frame = layer_get_frame(layer);
  GSize text_size = GSize(100, 30);
  GRect rect = GRect((layer_frame.size.w - text_size.w) / 2 - margin.w,
                     (layer_frame.size.h - text_size.h) / 2 - margin.h,
                     text_size.w + margin.w + margin.w,
                     text_size.h + margin.h + margin.h);
  
  graphics_context_set_text_color(ctx, text_color);
  graphics_context_set_stroke_color(ctx, border_color);
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_rect(ctx, rect, 0, GCornerNone);  // fill background
  graphics_draw_rect(ctx, rect);                  // border
  
  rect = GRect(0, 0, text_size.w, text_size.h);
  text_size = graphics_text_layout_get_content_size(message_text, font, rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  rect = GRect((layer_frame.size.w - text_size.w) / 2,
               (layer_frame.size.h - text_size.h) / 2,
               text_size.w,
               text_size.h);

  rect.origin.y -= 3;  // needed cause of pebble dumbness
  graphics_draw_text(ctx, message_text, font, rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

}


static void graphics_layer_update(Layer *layer, GContext *ctx) {
  GRect layer_frame = layer_get_frame(layer);
  fill_screen(ctx, GColorWhite);
  
  if(image) {
    uint8_t          *data = gbitmap_get_data(image);
    snprintf(text_buffer, sizeof(text_buffer), "%s", data);
    graphics_context_set_text_color(ctx, GColorBlack);
    GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    graphics_draw_text(ctx, text_buffer, font, layer_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else {
    display_message_text(layer, ctx);
  }
}















// ------------------------------------------------------------------------ //
//  Button Functions
// ------------------------------------------------------------------------ //
static void sl_click_handler(ClickRecognizerRef recognizer, void *context) {
  send_command(COMMAND_SEND_DATA);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, sl_click_handler);
}

// ------------------------------------------------------------------------ //
//  Main Functions
// ------------------------------------------------------------------------ //
static void window_load(Window *window) {
  root_layer = window_get_root_layer(window);
  layer_set_update_proc(root_layer, graphics_layer_update);
  message_text = "Waiting for Javascript...";
}

static void window_unload(Window *window) {

}

static void init(void) {
  app_message_init();
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
