#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>
#include <stdlib.h>

// Bit at start and end of frame
#define FLAG 0x7E

// Byte Stuffing
#define ESCAPE 0x7D
#define ESCAPE_OFFSET 0x20

#define BUF_SIZE 5

typedef enum {
    CTRL_SET = 0x03,
    CTRL_UA = 0X07,
    
    CTRL_RR0 = 0xAA,
    CTRL_RR1 = 0xAB,
    
    CTRL_REJ0 = 0x54,
    CTRL_REJ1 = 0x55,

    CTRL_DISC = 0x0B,

    CTRL_INFO0 = 0x00,
    CTRL_INFO1 = 0x80
}   t_frame_ctrl;

typedef enum
{
    ADDR_SEND = 0x03,
    ADDR_RCV = 0x01
}   t_frame_addr;

typedef struct
{
    t_frame_addr    a;
    t_frame_ctrl    c;
    uint8_t         bcc1;

    uint8_t         *data;
    size_t          dataSize;
    
    uint8_t         bcc2;

    size_t          bytesToStuff;
}   t_frame;

typedef enum e_state
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    DATA_RCV,
    STOP
}   t_state;

t_frame newSUFrame(t_frame_addr addr, t_frame_ctrl ctrl);

#define SET_Command newSUFrame(ADDR_SEND, CTRL_SET)
#define UA_Rx_Response newSUFrame(ADDR_SEND, CTRL_UA)
#define UA_Tx_Response newSUFrame(ADDR_RCV, CTRL_UA)
#define DISC_Rx_Command newSUFrame(ADDR_RCV, CTRL_DISC)
#define DISC_Tx_Command newSUFrame(ADDR_SEND, CTRL_DISC)
#define RR0_Command newSUFrame(ADDR_SEND, CTRL_RR0)
#define RR1_Command newSUFrame(ADDR_SEND, CTRL_RR1)
#define REJ0_Command newSUFrame(ADDR_SEND, CTRL_REJ0)
#define REJ1_Command newSUFrame(ADDR_SEND, CTRL_REJ1)

#endif