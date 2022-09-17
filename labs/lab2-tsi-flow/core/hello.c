unsigned int *UART_TX_DATA = (unsigned int *)0x1000U;

void UART_transmit(unsigned char c) {
  *UART_TX_DATA = c;
}

int main() {
  while (1) {
    UART_transmit('H');
  }
}