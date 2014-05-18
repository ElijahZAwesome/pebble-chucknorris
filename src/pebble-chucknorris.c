#include <pebble.h>

static Window *window;
static TextLayer *time_layer;
static TextLayer *quote_layer;
static InverterLayer *inverter_layer;

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

void getQuote(){
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter,  KEY_QUOTE, 1);
  dict_write_end(iter);
  app_message_outbox_send();
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";

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

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(time_layer, time_text);

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

  time_layer = text_layer_create(GRect(0, 0, bounds.size.w, 45));
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(time_layer));

  quote_layer = text_layer_create(GRect(10, 55, bounds.size.w - 20, bounds.size.h - 45));
  text_layer_set_font(quote_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(quote_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(quote_layer));

  inverter_layer = inverter_layer_create(bounds);
  layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(time_layer);
  text_layer_destroy(quote_layer);
  inverter_layer_destroy(inverter_layer);
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
