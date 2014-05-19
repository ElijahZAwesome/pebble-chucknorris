#include <pebble.h>

static Window *window;
static TextLayer *time_layer;
static TextLayer *quote_layer;
static TextLayer *border_quote_layer_1;
static TextLayer *border_quote_layer_2;
static InverterLayer *inverter_layer;

static BitmapLayer  *chuck_image_layer;

static int          image_nb;
static GBitmap      *image;

static uint32_t IMAGE_RESOURCES[5] = {
  RESOURCE_ID_IMAGE_CHUCK_1,
  RESOURCE_ID_IMAGE_CHUCK_2,
  RESOURCE_ID_IMAGE_CHUCK_3,
  RESOURCE_ID_IMAGE_CHUCK_4,
  RESOURCE_ID_IMAGE_CHUCK_5,
};

static AppTimer  *timer;

static PropertyAnimation *prop_animation;
static int out;

// Need to be static because they're used by the system later.
static char time_text[] = "00:00";
static char tmp_time_text[] = "00:00";

#define KEY_QUOTE    0

char *translate_error(AppMessageResult result) {
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

static void animation_started(Animation *animation, void *data) {
  // text_layer_set_text(text_layer, "Started.");
}

static void animation_stopped(Animation *animation, bool finished, void *data) {
  property_animation_destroy(prop_animation);
  if(out){
    memcpy(time_text, tmp_time_text, 6);
    text_layer_set_text(time_layer, time_text);
    out = 0;
    Layer *layer = text_layer_get_layer(time_layer);
    GRect to_rect = GRect(15, -3, 144, 45);
    prop_animation = property_animation_create_layer_frame(layer, NULL, &to_rect);
    animation_set_duration((Animation*) prop_animation, 400);
    animation_set_curve((Animation*) prop_animation, AnimationCurveEaseIn);
    animation_set_handlers((Animation*) prop_animation, (AnimationHandlers) {
      .started = (AnimationStartedHandler) animation_started,
      .stopped = (AnimationStoppedHandler) animation_stopped,
    }, NULL /* callback data */);
    animation_schedule((Animation*) prop_animation);
  }
}

static void update_timer(void *self) {
  timer = NULL;
  image_nb = (image_nb + 1) % 5;
  gbitmap_destroy(image);
  image = gbitmap_create_with_resource(IMAGE_RESOURCES[image_nb]);
  bitmap_layer_set_bitmap(chuck_image_layer, image);
  layer_mark_dirty(bitmap_layer_get_layer(chuck_image_layer));
  if(image_nb > 0){
    timer = app_timer_register(300 /* ms */, (AppTimerCallback) update_timer, NULL);
  }
  if(image_nb == 4){
    Layer *layer = text_layer_get_layer(time_layer);

    GRect to_rect = GRect(144, -3, 144, 45);
    out = 1;

    prop_animation = property_animation_create_layer_frame(layer, NULL, &to_rect);
    animation_set_duration((Animation*) prop_animation, 200);
    animation_set_curve((Animation*) prop_animation, AnimationCurveEaseOut);
    animation_set_handlers((Animation*) prop_animation, (AnimationHandlers) {
      .started = (AnimationStartedHandler) animation_started,
      .stopped = (AnimationStoppedHandler) animation_stopped,
    }, NULL /* callback data */);
    animation_schedule((Animation*) prop_animation);
  }
}

void getQuote(){
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter,  KEY_QUOTE, 1);
  dict_write_end(iter);
  app_message_outbox_send();
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  

  char *time_format;

  if (!tick_time) {
    time_t now = time(NULL);
    tick_time = localtime(&now);
  }

  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  strftime(tmp_time_text, sizeof(tmp_time_text), time_format, tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(tmp_time_text, &tmp_time_text[1], sizeof(tmp_time_text) - 1);
  }

  update_timer(NULL);

  if(tick_time->tm_min % 15 == 0){
    getQuote();
  }
}

void accel_tap_handler(AccelAxisType axis, int32_t direction){
  getQuote();
}

static void cb_in_received_handler(DictionaryIterator *iter, void *context) {
  static char quote_text[500] = "";

  // Get the quote
  Tuple *quote_tuple = dict_find(iter, KEY_QUOTE);
  if (quote_tuple) {
    uint8_t* values = &quote_tuple->value->uint8;
    memcpy(&quote_text[values[0]*100],values + 2,quote_tuple->length-2);
    if(values[0] == values[1])
      text_layer_set_text(quote_layer, quote_text);
  }
}

static void cb_out_fail_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "cb_out_fail_handler %s", translate_error(reason));
}

static void app_message_init() {
  // Register message handlers
  app_message_register_inbox_received(cb_in_received_handler);
  app_message_register_outbox_failed(cb_out_fail_handler);
  // Init buffers
  app_message_open(APP_MESSAGE_INBOX_SIZE_MINIMUM, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  chuck_image_layer = bitmap_layer_create(GRect(-10, 3, 61, 40));
  bitmap_layer_set_alignment(chuck_image_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(chuck_image_layer));

  image_nb = 0;
  image = gbitmap_create_with_resource(IMAGE_RESOURCES[image_nb]);
  bitmap_layer_set_bitmap(chuck_image_layer, image);


  time_layer = text_layer_create(GRect(15, -3, 144, 45));
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  text_layer_set_background_color(time_layer, GColorClear);
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(time_layer));

  border_quote_layer_1 = text_layer_create(GRect(0, 50, bounds.size.w, bounds.size.h - 50));
  text_layer_set_background_color(border_quote_layer_1, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(border_quote_layer_1));

  border_quote_layer_2 = text_layer_create(GRect(2, 52, bounds.size.w - 4, bounds.size.h - 50 - 4));
  layer_add_child(window_layer, text_layer_get_layer(border_quote_layer_2));
  
  quote_layer = text_layer_create(GRect(4, 52, bounds.size.w - 8, bounds.size.h - 50 - 4));
  text_layer_set_font(quote_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(quote_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(quote_layer));

  inverter_layer = inverter_layer_create(bounds);
  layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(time_layer);
  text_layer_destroy(quote_layer);
  text_layer_destroy(border_quote_layer_1);
  text_layer_destroy(border_quote_layer_2);
  inverter_layer_destroy(inverter_layer);
  bitmap_layer_destroy(chuck_image_layer);

  gbitmap_destroy(image);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  app_message_init();

  accel_tap_service_subscribe(accel_tap_handler);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  handle_minute_tick(NULL, MINUTE_UNIT);
}

static void deinit(void) {
  window_destroy(window);

  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
