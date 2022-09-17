#include "hello.h"

uint32_t *UART_TXDATA = 0x1000U;

void UART_transmit(uint8_t char) {
  *UART_TXDATA = char;
}

int main() {
  while (1) {
    UART_transmit('H');
  }
}