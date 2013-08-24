/* Looked up in the driver */
#define USB_VENDOR_APPLE	  0x05ac
#define USB_PRODUCT_IPHONE_5	  0x12a8

/* Specifics */
#define IPHETH_USBINTF_CLASS	  0xff
#define IPHETH_USBINTF_SUBCLASS	  0xfd
#define IPHETH_USBINTF_PROTO	  0x01
#define IPHETH_BUF_SIZE		  1516
#define IPHETH_IP_ALIGN		  0x02
#define IPHETH_INTFNUM		  0x02
#define IPHETH_ALT_INTFNUM	  0x01
#define IPHETH_CTRL_ENDP	  0x00
#define IPHETH_CTRL_BUF_SIZE	  0x40

/* Timeouts */
#define IPHETH_TX_TIMEOUT	  (5 * HZ)
#define IPHETH_CTRL_TIMEOUT	  (5 * HZ)

/* Legit Commands */
#define IPHETH_CMD_GET_MACADDR	   0x00
#define IPHETH_CMD_CARRIER_CHECK   0x45

/* U7tr4 b0mb commands */
#define IPHETH_CMD_UNKNOWN_CMD_A   0x01
#define IPHETH_CMD_UNKNOWN_CMD_B   0xff
#define IPHETH_CMD_UNKNOWN_CMD_C   0x46

#define IPHETH_CARRIER_CHECK_TIMEOUT round_jiffies_relative(1 * HZ)
#define IPHETH_CARRIER_ON	   0x04


