#ifndef PROTOCOL_H
#define PROTOCOL_H

#define BIT(x) (1 << x)

// Bit at start and end of frame
#define FLAG 0x7E

// Address field (A)

#define ADDR_TX 0x03
#define ADDR_RX 0x01

#define CTRL_SET 0x03
#define CTRL_UA 0X07
#define CTRL_RR0 0xAA
#define CTRL_RR1 0xAB
#define CTRL_REJ0 0x54
#define CTRL_REJ1 0x55
#define CTRL_DISC 0x0B

#define CTRL_INFO0 0x00
#define CTRL_INFO1 0x01

#define BUF_SIZE 5
unsigned char txSetBuffer[BUF_SIZE] = {FLAG, ADDR_TX, CTRL_SET, ADDR_TX ^ CTRL_SET, FLAG};

typedef enum e_state_header {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} t_state_header;

#endif