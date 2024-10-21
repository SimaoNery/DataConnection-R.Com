#ifndef PROTOCOL_H
#define PROTOCOL_H

#define BIT(x) (1 << x)

// Bit at start and end of frame
#define FLAG 0x7E

//Byte Stuffing
#define ESCAPE 0x7D 
#define ESCAPE_OFFSET 0x20

// Address field (A)
#define ADDR_SEND 0x03
#define ADDR_RCV 0x01

// Control field (C)
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

unsigned char SET_Command[BUF_SIZE] = {FLAG, ADDR_SEND, CTRL_SET, ADDR_SEND ^ CTRL_SET, FLAG};
unsigned char UA_Rx_Response[BUF_SIZE] = {FLAG, ADDR_SEND, CTRL_UA, ADDR_SEND ^ CTRL_UA, FLAG};
unsigned char UA_Tx_Response[BUF_SIZE] = {FLAG, ADDR_RCV, CTRL_UA, ADDR_RCV ^ CTRL_UA, FLAG};
unsigned char DISC_Rx_Command[BUF_SIZE] = {FLAG, ADDR_RCV, CTRL_DISC, ADDR_RCV ^ CTRL_DISC, FLAG};
unsigned char DISC_Tx_Command[BUF_SIZE] = {FLAG, ADDR_SEND, CTRL_DISC, ADDR_SEND ^ CTRL_DISC, FLAG};

typedef enum e_frame_type {
    SET,
    UA_Rx,
    UA_Tx,
    DISC_Rx,
    DISC_Tx
} t_frame_type;

unsigned char* frame_buffers[] = {
    SET_Command,
    UA_Rx_Response,
    UA_Tx_Response,
    DISC_Rx_Command,
    DISC_Tx_Command
};

typedef enum e_state {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    DATA_RCV,
    STOP
} t_state;


#endif