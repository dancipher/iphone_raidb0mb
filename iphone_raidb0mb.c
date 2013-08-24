/*
 * iphone_raidb0mb.c - iPhone 5 USB Tethering Jailbreak DKMS
 * jailbreak 'em through USB.
 *
 * 0.1
 */

#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>

#include "iphone_raidb0mb.h"

static struct
usb_device_id raidb0mb_table[] = {
  { USB_DEVICE_AND_INTERFACE_INFO(USB_VENDOR_APPLE, USB_PRODUCT_IPHONE_5,
    IPHETH_USBINTF_CLASS, IPHETH_USBINTF_SUBCLASS, IPHETH_USBINTF_PROTO) },
  { }
};

MODULE_DEVICE_TABLE(usb, raidb0mb_table);

typedef struct _raidb0mb_bullet {
  unsigned int req;
  unsigned int req_type;
  unsigned int val;
  unsigned int index;
} raidb0mb_bullet;

typedef struct _raidb0mb_device {
  struct usb_device * udev;
  struct usb_interface * intf;
  struct net_device * net;
  struct sk_buff * tx_skb;
  struct urb * tx_urb;
  struct urb * rx_urb;
  unsigned char * tx_buf;
  unsigned char * rx_buf;
  unsigned char * ctrl_buf;
  u8 bulk_in;
  u8 bulk_out;
  struct delayed_work carrier_work;
} raidb0mb_device;

static int raidb0mb_rx_submit(raidb0mb_device *dev, gfp_t mem_flags);

static int
raidb0mb_alloc_urbs (raidb0mb_device * iphone)
{
  struct urb * tx_urb = NULL;
  struct urb * rx_urb = NULL;
  u8 * tx_buf = NULL;
  u8 * rx_buf = NULL;

  tx_urb = usb_alloc_urb(0, GFP_KERNEL);

  if (tx_urb == NULL) {
    printk(KERN_INFO "Broken. Something, somewhere is horribly broken.\n");
  }

  rx_urb = usb_alloc_urb(0, GFP_KERNEL);
  
  if (rx_urb == NULL) {
    goto free_tx_urb;
  }

  tx_buf = usb_alloc_coherent(iphone->udev, IPHETH_BUF_SIZE,
            GFP_KERNEL, &tx_urb->transfer_dma);
  if (tx_buf == NULL)
    goto free_rx_urb;

  rx_buf = usb_alloc_coherent(iphone->udev, IPHETH_BUF_SIZE,
            GFP_KERNEL, &rx_urb->transfer_dma);
  if (rx_buf == NULL)
    goto free_tx_buf;


  iphone->tx_urb = tx_urb;
  iphone->rx_urb = rx_urb;
  iphone->tx_buf = tx_buf;
  iphone->rx_buf = rx_buf;
  
  return 0;

free_tx_buf:
  usb_free_coherent(iphone->udev, IPHETH_BUF_SIZE, tx_buf,
        tx_urb->transfer_dma);
free_rx_urb:
  usb_free_urb(rx_urb);
free_tx_urb:
  usb_free_urb(tx_urb);
}

static void
raidb0mb_free_urbs (raidb0mb_device * iphone)
{
  usb_free_coherent(iphone->udev, IPHETH_BUF_SIZE, iphone->rx_buf,
        iphone->rx_urb->transfer_dma);
  usb_free_coherent(iphone->udev, IPHETH_BUF_SIZE, iphone->tx_buf,
        iphone->tx_urb->transfer_dma);
  usb_free_urb(iphone->rx_urb);
  usb_free_urb(iphone->tx_urb);
}

static void
raidb0mb_kill_urbs(raidb0mb_device * dev)
{
  usb_kill_urb(dev->tx_urb);
  usb_kill_urb(dev->rx_urb);
}

static void
raidb0mb_rcvbulk_callback(struct urb * urb)
{
  raidb0mb_device *dev;
  struct sk_buff *skb;
  int status;
  char *buf;
  int len;

  dev = urb->context;
  if (dev == NULL) {
    return;
  }

  status = urb->status;
  
  switch (status) {
    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
      return;
    case 0:
      break;
    default:
      dev_err(&dev->intf->dev, "%s: urb status: %d\n",
	__func__, status);
      return;
  }

  if (urb->actual_length <= IPHETH_IP_ALIGN) {
    dev->net->stats.rx_length_errors++;
    return;
  }

  len = urb->actual_length - IPHETH_IP_ALIGN;
  buf = urb->transfer_buffer + IPHETH_IP_ALIGN;

  skb = dev_alloc_skb(len);
  
  if (!skb) {
    dev_err(&dev->intf->dev, "%s: dev_alloc_skb: -ENOMEM\n",  __func__);
    dev->net->stats.rx_dropped++;
    return;
  }

  memcpy(skb_put(skb, len), buf, len);
  skb->dev = dev->net;
  skb->protocol = eth_type_trans(skb, dev->net);

  dev->net->stats.rx_packets++;
  dev->net->stats.rx_bytes += len;

  netif_rx(skb);
  raidb0mb_rx_submit(dev, GFP_ATOMIC);
}

static void
raidb0mb_sndbulk_callback(struct urb *urb)
{
  raidb0mb_device *dev;
  int status;

  status = urb->status;

  dev = urb->context;
  if (dev == NULL) {
    return;
  }

  if (status != 0 &&
      status != -ENOENT &&
      status != -ECONNRESET &&
      status != -ESHUTDOWN)
    dev_err(&dev->intf->dev, "%s: urb status: %d\n",
    __func__, status);

  dev_kfree_skb_irq(dev->tx_skb);
  netif_wake_queue(dev->net);
}

static int
raidb0mb_carrier_set (raidb0mb_device * dev)
{
  struct usb_device * udev;
  int retval;

  udev = dev->udev;

  retval = usb_control_msg(udev,
      usb_rcvctrlpipe(udev, IPHETH_CTRL_ENDP),
      IPHETH_CMD_CARRIER_CHECK, /* request */
      0xc0, /* request type */
      0x00, /* value */
      0x02, /* index */
      dev->ctrl_buf, IPHETH_CTRL_BUF_SIZE,
      IPHETH_CTRL_TIMEOUT);
  if (retval < 0) {
    dev_err(&dev->intf->dev, "%s: usb_control_msg: %d\n",
      __func__, retval);
    return retval;
  }

  if (dev->ctrl_buf[0] == IPHETH_CARRIER_ON) {
    netif_carrier_on(dev->net);
  } else {
    netif_carrier_off(dev->net);
  }

  return 0;
}

static void
raidb0mb_carrier_check_work (struct work_struct * work)
{
  raidb0mb_device *dev = container_of(work, raidb0mb_device, carrier_work.work);

  raidb0mb_carrier_set(dev);
  schedule_delayed_work(&dev->carrier_work, IPHETH_CARRIER_CHECK_TIMEOUT);
}

static int
raidb0mb_send_command(raidb0mb_device * dev, raidb0mb_bullet * bullet)
{
  struct usb_device *udev;
  struct net_device *net;
  int retval;
  unsigned int req, req_type, value, index;

  udev = dev->udev;
  net = dev->net;

  req = bullet->req;
  req_type = bullet->req_type;
  value = bullet->val;
  index = bullet->index;
  
  retval = usb_control_msg(udev,
	usb_rcvctrlpipe(udev, IPHETH_CTRL_ENDP),
	req, req_type, value, index, dev->ctrl_buf,
	IPHETH_CTRL_BUF_SIZE, IPHETH_CTRL_TIMEOUT);

  if (retval < 0) {
    /*dev_err(&dev->intf->dev, "%s: usb_control_msg: %d\n",
      __func__, retval);
      .. do nothing */
  } else if (retval < ETH_ALEN) {
    dev_err(&dev->intf->dev,
      "%s: usb_control_msg: short packet: %s [%d bytes]\n",
      __func__, (char *)dev->ctrl_buf, retval);
    retval = -EINVAL;
  } else {
    dev_info(&dev->intf->dev,
      "%s: usb: packet; %s [%d bytes]\n",
      __func__, (char *)dev->ctrl_buf, retval);
    retval = 0;
  }

  return retval;
}

static int
raidb0mb_get_macaddr(raidb0mb_device * dev)
{
  struct usb_device *udev;
  struct net_device *net;
  int retval;

  udev = dev->udev;
  net = dev->net;

  retval = usb_control_msg(udev,
         usb_rcvctrlpipe(udev, IPHETH_CTRL_ENDP),
         IPHETH_CMD_GET_MACADDR, /* request */
         0xc0, /* request type */
         0x00, /* value */
         0x02, /* index */
         dev->ctrl_buf,
         IPHETH_CTRL_BUF_SIZE,
         IPHETH_CTRL_TIMEOUT);
  if (retval < 0) {
    dev_err(&dev->intf->dev, "%s: usb_control_msg: %d\n",
      __func__, retval);
  } else if (retval < ETH_ALEN) {
    dev_err(&dev->intf->dev,
      "%s: usb_control_msg: short packet: %d bytes\n",
      __func__, retval);
    retval = -EINVAL;
  } else {
    printk(KERN_INFO "Recieved buffer of length %d. Contents: %s (hex)\n", sizeof(dev->ctrl_buf), (char *)dev->ctrl_buf); 
    memcpy(net->dev_addr, dev->ctrl_buf, ETH_ALEN);
    retval = 0;
  }

  return retval;
}

static int
raidb0mb_rx_submit(raidb0mb_device * dev, gfp_t mem_flags)
{
  struct usb_device *udev;
  int retval;

  udev = dev->udev;

  usb_fill_bulk_urb(dev->rx_urb, udev,
        usb_rcvbulkpipe(udev, dev->bulk_in),
        dev->rx_buf, IPHETH_BUF_SIZE,
        raidb0mb_rcvbulk_callback,
        dev);
  dev->rx_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

  retval = usb_submit_urb(dev->rx_urb, mem_flags);
  if (retval) {
    dev_err(&dev->intf->dev, "%s: usb_submit_urb: %d\n",
      __func__, retval);
  }

  return retval;
}

static int
raidb0mb_open(struct net_device * net)
{
  raidb0mb_device * dev;
  struct usb_device * udev;
  int retval;

  dev = netdev_priv(net);
  udev = dev->udev;
  retval = 0;

  usb_set_interface(udev, IPHETH_INTFNUM, IPHETH_ALT_INTFNUM);

  retval = raidb0mb_carrier_set(dev);
  if (retval) {
    return retval;
  }

  retval = raidb0mb_rx_submit(dev, GFP_KERNEL);
  
  if (retval) {
    return retval;
  }

  schedule_delayed_work(&dev->carrier_work, IPHETH_CARRIER_CHECK_TIMEOUT);
  netif_start_queue(net);
  
  return retval;
}

static int
raidb0mb_close(struct net_device * net)
{
  raidb0mb_device *dev;

  dev = netdev_priv(net);
  cancel_delayed_work_sync(&dev->carrier_work);
  netif_stop_queue(net);
  
  return 0;
}

static int
raidb0mb_tx (struct sk_buff * skb, struct net_device * net)
{
  raidb0mb_device *dev;
  struct usb_device *udev;
  int retval;
  
  dev = netdev_priv(net);
  udev = dev->udev;

  /* Paranoid */
  if (skb->len > IPHETH_BUF_SIZE) {
    WARN(1, "%s: skb too large: %d bytes\n", __func__, skb->len);
    dev->net->stats.tx_dropped++;
    dev_kfree_skb_irq(skb);
    return NETDEV_TX_OK;
  }

  memcpy(dev->tx_buf, skb->data, skb->len);
  if (skb->len < IPHETH_BUF_SIZE) {
    memset(dev->tx_buf + skb->len, 0, IPHETH_BUF_SIZE - skb->len);
  }

  usb_fill_bulk_urb(dev->tx_urb, udev,
        usb_sndbulkpipe(udev, dev->bulk_out),
        dev->tx_buf, IPHETH_BUF_SIZE,
        raidb0mb_sndbulk_callback,
        dev);
  dev->tx_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

  retval = usb_submit_urb(dev->tx_urb, GFP_ATOMIC);
  if (retval) {
    dev_err(&dev->intf->dev, "%s: usb_submit_urb: %d\n",
      __func__, retval);
    dev->net->stats.tx_errors++;
    dev_kfree_skb_irq(skb);
  } else {
    dev->tx_skb = skb;

    dev->net->stats.tx_packets++;
    dev->net->stats.tx_bytes += skb->len;
    netif_stop_queue(net);
  }

  return NETDEV_TX_OK;
}

static void
raidb0mb_tx_timeout(struct net_device *net)
{
  raidb0mb_device *dev = netdev_priv(net);

  dev_err(&dev->intf->dev, "%s: TX timeout\n", __func__);
  dev->net->stats.tx_errors++;
  usb_unlink_urb(dev->tx_urb);
}

static u32 raidb0mb_ethtool_op_get_link(struct net_device *net)
{
  raidb0mb_device *dev = netdev_priv(net);
  return netif_carrier_ok(dev->net);
}

static const struct ethtool_ops ops = {
  .get_link = raidb0mb_ethtool_op_get_link
};

static const struct net_device_ops raidb0mb_netdev_ops = {
  .ndo_open = raidb0mb_open,
  .ndo_stop = raidb0mb_close,
  .ndo_start_xmit = raidb0mb_tx,
  .ndo_tx_timeout = raidb0mb_tx_timeout,
};

static int raidb0mb_probe(struct usb_interface *intf,
      const struct usb_device_id *id)
{
  raidb0mb_device *dev;
  struct usb_device * udev;
  struct usb_host_interface * hintf;
  struct usb_endpoint_descriptor * endp;
  struct net_device * netdev;
  raidb0mb_bullet * bull;
  int i, retval, t;

  udev = interface_to_usbdev(intf);
  netdev = alloc_etherdev(sizeof(raidb0mb_device));
  if (!netdev) {
    return -ENOMEM;
  }

  netdev->netdev_ops = &raidb0mb_netdev_ops;
  netdev->watchdog_timeo = IPHETH_TX_TIMEOUT;
  strcpy(netdev->name, "eth%d");

  dev = netdev_priv(netdev);
  dev->udev = udev;
  dev->net = netdev;
  dev->intf = intf;

  /* Set up endpoints */
  hintf = usb_altnum_to_altsetting(intf, IPHETH_ALT_INTFNUM);
  if (hintf == NULL) {
    retval = -ENODEV;
    dev_err(&intf->dev, "Unable to find alternate settings interface\n");
    goto err_endpoints;
  }

  for (i=0;i<hintf->desc.bNumEndpoints;i++) {
    endp = &hintf->endpoint[i].desc;
    if (usb_endpoint_is_bulk_in(endp)) {
      dev->bulk_in = endp->bEndpointAddress;
    } else if (usb_endpoint_is_bulk_out(endp)) {
      dev->bulk_out = endp->bEndpointAddress;
    }
  }

  if (!(dev->bulk_in && dev->bulk_out)) {
    retval = -ENODEV;
    dev_err(&intf->dev, "Unable to find endpoints\n");
    goto err_endpoints;
  }

  dev->ctrl_buf = kmalloc(IPHETH_CTRL_BUF_SIZE, GFP_KERNEL);

  if (dev->ctrl_buf == NULL) {
    retval = -ENOMEM;
    goto err_alloc_ctrl_buf;
  }

  retval = raidb0mb_get_macaddr(dev);

  if (retval) {
    goto err_get_macaddr;
  }

  bull = kmalloc(sizeof(raidb0mb_bullet), GFP_KERNEL);
  for (t=0;t<=255;t++)
  {
    bull->req = t;
    dev_info(&intf->dev, "Round %d\n", t);
    bull->req_type = 0xc0;
    bull->val = 0x00;
    bull->index = 0x02;

    retval = raidb0mb_send_command(dev, bull);
    
  }

  INIT_DELAYED_WORK(&dev->carrier_work, raidb0mb_carrier_check_work);

  retval = raidb0mb_alloc_urbs(dev);
  
  if (retval) {
    dev_err(&intf->dev, "error allocating urbs: %d\n", retval);
    goto err_alloc_urbs;
  }

  /*
  usb_set_intfdata(intf, dev);

  SET_NETDEV_DEV(netdev, &intf->dev);
  SET_ETHTOOL_OPS(netdev, &ops);

  retval = register_netdev(netdev);
  if (retval) {
    dev_err(&intf->dev, "error registering netdev: %d\n", retval);
    retval = -EIO;
    goto err_register_netdev;
  }
  */

  dev_info(&intf->dev, "Apple iPhone USB Ethernet device attached\n");

  return 0;

err_register_netdev:
  raidb0mb_free_urbs(dev);
err_alloc_urbs:
err_get_macaddr:
err_alloc_ctrl_buf:
  kfree(dev->ctrl_buf);
err_endpoints:
  free_netdev(netdev);
  return retval;
}

static void
raidb0mb_disconnect (struct usb_interface * intf)
{
  raidb0mb_device *dev;

  dev = usb_get_intfdata(intf);
  if (dev != NULL) {
    unregister_netdev(dev->net);
    raidb0mb_kill_urbs(dev);
    raidb0mb_free_urbs(dev);
    kfree(dev->ctrl_buf);
    free_netdev(dev->net);
  }
  usb_set_intfdata(intf, NULL);
  dev_info(&intf->dev, "iPhone 5 disconnected.\n");
}

static struct
usb_driver raidb0mb_driver = {
  .name = "iphone_raidb0mb",
  .probe = raidb0mb_probe,
  .disconnect = raidb0mb_disconnect,
  .id_table = raidb0mb_table,
  .disable_hub_initiated_lpm = 1,
};

module_usb_driver(raidb0mb_driver);

MODULE_AUTHOR("zulla@zulla.org");
MODULE_DESCRIPTION("foo bar");
MODULE_LICENSE("Dual BSD/GPL");

