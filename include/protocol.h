#ifndef PROTOCOL_H
#define PROTOCOL_H

#define BIT(x)      (1 << x)

// Bit at start and end of frame
#define FLAG        0x7E

// Address field (A)

// Address in commands sent by transmitter
// and responses to them by the receiver 
#define CMD_TRANSM  0x03
// Address in commands sent by receiver
// and responses to them by the transmitter
#define CMD_RECEIV  0x01

#define CTRL_SET    0x03
#define CTRL_UA     0X07
#define CTRL_RR0    0xAA
#define CTRL_RR1    0xAB
#define CTRL_REJ0   0x54
#define CTRL_REJ1   0x55
#define CTRL_DISC   0x0B

#define CTRL_INFO0  0x00
#define CTRL_INFO1  0x01

#endif