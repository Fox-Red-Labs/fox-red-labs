/* The field values both sides use: ref_gen.c puts them into TI's structs on a byte-addressed host, test_wire.c
 * passes them to the packer. Same job as TI's ti_cc140xp_tx sample (app_tx.c). Plain numbers, so the C28x can use them. */
#ifndef SCENARIO_H
#define SCENARIO_H
#define SC_DIO0 0xFFu
#define SC_DIO2 0x54u
#define SC_DIO3 0x12u
#define SC_DIO4 0xFFu
#define SC_DIO5 0xFFu
#define SC_DIO6 0xFFu
#define SC_DIO7 0x00u
#define SC_CLOCK 1u
#define SC_DWELL 0x0123u
#define SC_POLICY 0u
#define SC_RFMODE 0u
#define SC_FE_REF 0xFEFEFEFEUL
#define SC_RF_REF 0xF000000DUL
#define SC_FRAG1_LEN 48u          /* first config fragment: octets 0..47 of the frontend table */
#define SC_FRAG2_LEN 40u          /* last fragment: the final 40 octets */
#define SC_CMD_SLOT 0u
#define SC_PHY_MASK 0x0008u
#define SC_PHY_CFGID 1u
#define SC_STREAM_ID 0u
#define SC_FREQ_KHZ 920600UL
#define SC_PA 1u
#define SC_MODEM 1u
#define SC_PWR_FRACTION 1u
#define SC_PWR_DBM 21
#define SC_PAYLOAD_LEN 76u
#define SC_PAY_BYTE(i) ((unsigned)(((i) * 3u + 1u) & 0xFFu))
#endif
