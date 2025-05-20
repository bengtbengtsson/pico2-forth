#include "forth_core.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

int main(void) {
  stdio_usb_init();
  while (!stdio_usb_connected())
    sleep_ms(50);
  return forth_main_loop();
}
