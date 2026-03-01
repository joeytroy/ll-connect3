/*
 * Lian Li SL Infinity Fan Control Driver (Fan Only)
 * 
 * This driver provides fan speed control for Lian Li SL Infinity fans.
 * RGB control is handled by OpenRGB to avoid conflicts.
 * 
 * Exposes:
 *   /proc/Lian_li_SL_INFINITY/Port_X/fan_speed      (write 0–100, read current setting)
 *   /proc/Lian_li_SL_INFINITY/Port_X/fan_rpm        (read actual RPM from hub hardware)
 *   /proc/Lian_li_SL_INFINITY/Port_X/fan_connected  (read 0/1 - is fan configured)
 *   /proc/Lian_li_SL_INFINITY/Port_X/fan_config     (write 0/1 - configure fan presence)
 *
 * Author: AI + Joey
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define VENDOR_ID  0x0CF2
#define PRODUCT_ID 0xA102

struct sli_port {
	int index;  /* 0..3 */
	struct sli_hub *hub;
	u8 fan_speed;  /* Current fan speed (0-100) */
	u16 fan_rpm;   /* Last read actual RPM from hub */
	bool fan_connected;  /* Is a fan connected to this port? (user configured) */
};

struct sli_hub {
	struct hid_device *hdev;
	struct proc_dir_entry *procdir;
	struct sli_port ports[4];  /* 4 ports */
};

static struct sli_hub *g_hub = NULL;
static bool g_log_enabled;

module_param_named(log_enabled, g_log_enabled, bool, 0644);
MODULE_PARM_DESC(log_enabled, "Enable informational logging for SLI driver");

#define SLI_LOG(fmt, ...)                           \
	do {                                            \
		if (g_log_enabled)                          \
			pr_info("SLI: " fmt, ##__VA_ARGS__);    \
	} while (0)

/* Send HID command for fan control */
static int sli_send_segment(struct hid_device *hdev, const u8 *buf, size_t len)
{
	int rc;
	rc = hid_hw_raw_request(hdev, 0xE0, (u8 *)buf, len,
							HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	if (rc < 0)
		pr_err("SLI: HID command failed: %d\n", rc);
	return rc;
}

/*
 * Query actual RPM for all 4 ports via USB GET_REPORT (Input Report 0xe0).
 * The hub returns 65 bytes:  e0 [P1_hi P1_lo] [P2_hi P2_lo] [P3_hi P3_lo] [P4_hi P4_lo] ...
 * RPM values are big-endian 16-bit unsigned.
 *
 * We bypass hid_hw_raw_request for Input reports because the Linux HID
 * subsystem returns -EAGAIN when the interrupt endpoint is active.
 * Instead we issue the USB control transfer directly.
 */
static int sli_query_rpm(struct sli_hub *hub)
{
	struct usb_interface *intf = to_usb_interface(hub->hdev->dev.parent);
	struct usb_device *udev = interface_to_usbdev(intf);
	int ifnum = intf->cur_altsetting->desc.bInterfaceNumber;
	u8 *buf;
	int rc;

	buf = kmalloc(65, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* USB HID GET_REPORT: wValue = (ReportType << 8) | ReportID
	 *   ReportType 1 = Input, ReportID = 0xe0  →  wValue = 0x01e0 */
	rc = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			     0x01,	/* HID_REQ_GET_REPORT */
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			     0x01e0,	/* Input Report, ID 0xe0 */
			     ifnum, buf, 65,
			     1000);	/* 1 second timeout */

	if (rc < 9) {
		if (rc < 0)
			pr_err("SLI: RPM query failed: %d\n", rc);
		else
			pr_err("SLI: RPM query short read: %d bytes\n", rc);
		kfree(buf);
		return rc < 0 ? rc : -EIO;
	}

	hub->ports[0].fan_rpm = (buf[1] << 8) | buf[2];
	hub->ports[1].fan_rpm = (buf[3] << 8) | buf[4];
	hub->ports[2].fan_rpm = (buf[5] << 8) | buf[6];
	hub->ports[3].fan_rpm = (buf[7] << 8) | buf[8];

	SLI_LOG("RPM: P1=%u P2=%u P3=%u P4=%u\n",
		hub->ports[0].fan_rpm, hub->ports[1].fan_rpm,
		hub->ports[2].fan_rpm, hub->ports[3].fan_rpm);

	kfree(buf);
	return 0;
}

/* Send commit command to apply buffered fan speed changes.
 * The hub firmware buffers individual port speeds and applies them
 * atomically when it receives the 0x50 commit. Without this, speed
 * changes may not fully take effect (observed in Windows USB captures). */
static int sli_commit(struct hid_device *hdev)
{
	u8 cmd[7] = {0xe0, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00};

	return sli_send_segment(hdev, cmd, sizeof(cmd));
}

/* Set fan speed for a specific port (0-100%) */
static int sli_set_fan_speed(struct sli_port *p, u8 speed_percent)
{
	u8 cmd[7];
	int port_num = p->index + 1;
	int rc;

	if (speed_percent > 100)
		speed_percent = 100;

	/* Build command: e0 <port_cmd> 00 <duty> 00 00 00 */
	cmd[0] = 0xe0;
	cmd[1] = 0x1f + port_num;  /* 0x20, 0x21, 0x22, 0x23 for ports 1-4 */
	cmd[2] = 0x00;
	cmd[3] = speed_percent;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;

	rc = sli_send_segment(p->hub->hdev, cmd, sizeof(cmd));

	if (rc < 0) {
		pr_err("SLI: Failed to set port %d speed: error %d\n", port_num, rc);
		return rc;
	}

	p->fan_speed = speed_percent;
	SLI_LOG("Port %d set to %d%%\n", port_num, speed_percent);

	/* Commit so the hub firmware applies the new speed immediately */
	rc = sli_commit(p->hub->hdev);
	if (rc < 0)
		pr_err("SLI: Commit after port %d speed change failed: %d\n",
		       port_num, rc);

	return 0;
}

/* Read handler for fan speed */
static ssize_t sli_read_fan_speed(struct file *file, char __user *ubuf,
								  size_t count, loff_t *ppos)
{
	struct sli_port *p = pde_data(file_inode(file));
	char buf[16];
	int len;

	if (*ppos > 0)
		return 0;

	len = snprintf(buf, sizeof(buf), "%d\n", p->fan_speed);
	if (len > count)
		len = count;

	if (copy_to_user(ubuf, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

/* Write handler for fan speed */
static ssize_t sli_write_fan_speed(struct file *file, const char __user *ubuf,
									size_t count, loff_t *ppos)
{
	struct sli_port *p = pde_data(file_inode(file));
	char buf[16];
	int speed_percent;
	int rc;

	if (count >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;
	buf[count] = '\0';

	if (kstrtoint(buf, 10, &speed_percent) < 0)
		return -EINVAL;

	if (speed_percent < 0 || speed_percent > 100)
		return -EINVAL;

	rc = sli_set_fan_speed(p, speed_percent);
	if (rc < 0)
		return rc;

	return count;
}

static const struct proc_ops sli_fan_speed_ops = {
	.proc_read = sli_read_fan_speed,
	.proc_write = sli_write_fan_speed,
};

/* Read handler for fan connection status */
static ssize_t sli_read_fan_connected(struct file *file, char __user *ubuf,
									  size_t count, loff_t *ppos)
{
	struct sli_port *p = pde_data(file_inode(file));
	char buf[16];
	int len;

	if (*ppos > 0)
		return 0;

	len = snprintf(buf, sizeof(buf), "%d\n", p->fan_connected ? 1 : 0);
	if (len > count)
		len = count;

	if (copy_to_user(ubuf, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static const struct proc_ops sli_fan_connected_ops = {
	.proc_read = sli_read_fan_connected,
};

/* Read handler for actual fan RPM (queries hub hardware) */
static ssize_t sli_read_fan_rpm(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct sli_port *p = pde_data(file_inode(file));
	char buf[16];
	int len;

	if (*ppos > 0)
		return 0;

	sli_query_rpm(p->hub);

	len = snprintf(buf, sizeof(buf), "%u\n", p->fan_rpm);
	if (len > count)
		len = count;

	if (copy_to_user(ubuf, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static const struct proc_ops sli_fan_rpm_ops = {
	.proc_read = sli_read_fan_rpm,
};

/* Write handler for fan configuration */
static ssize_t sli_write_fan_config(struct file *file, const char __user *ubuf,
									size_t count, loff_t *ppos)
{
	struct sli_port *p = pde_data(file_inode(file));
	char buf[16];
	int connected;

	if (count >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;
	buf[count] = '\0';

	if (kstrtoint(buf, 10, &connected) < 0)
		return -EINVAL;

	/* Set fan configuration */
	p->fan_connected = (connected != 0);

	SLI_LOG("Port %d fan configuration set to %s\n",
		p->index + 1, p->fan_connected ? "connected" : "disconnected");

	return count;
}

/* Read handler for fan configuration */
static ssize_t sli_read_fan_config(struct file *file, char __user *ubuf,
								   size_t count, loff_t *ppos)
{
	struct sli_port *p = pde_data(file_inode(file));
	char buf[16];
	int len;

	if (*ppos > 0)
		return 0;

	len = snprintf(buf, sizeof(buf), "%d\n", p->fan_connected ? 1 : 0);
	if (len > count)
		len = count;

	if (copy_to_user(ubuf, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static const struct proc_ops sli_fan_config_ops = {
	.proc_read = sli_read_fan_config,
	.proc_write = sli_write_fan_config,
};

/* Read handler for logging flag */
static ssize_t sli_read_logging_enabled(struct file *file, char __user *ubuf,
										size_t count, loff_t *ppos)
{
	char buf[16];
	int len;

	if (*ppos > 0)
		return 0;

	len = snprintf(buf, sizeof(buf), "%d\n", g_log_enabled ? 1 : 0);
	if (len > count)
		len = count;

	if (copy_to_user(ubuf, buf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

/* Write handler for logging flag */
static ssize_t sli_write_logging_enabled(struct file *file, const char __user *ubuf,
										 size_t count, loff_t *ppos)
{
	char buf[16];
	int enabled;

	if (count >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;
	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enabled) < 0)
		return -EINVAL;

	g_log_enabled = (enabled != 0);

	return count;
}

static const struct proc_ops sli_logging_enabled_ops = {
	.proc_read = sli_read_logging_enabled,
	.proc_write = sli_write_logging_enabled,
};

/* Probe function */
static int sli_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct sli_hub *hub;
	int rc;
	int i;

	SLI_LOG("Probing device\n");

	rc = hid_parse(hdev);
	if (rc) {
		pr_err("SLI: hid_parse failed: %d\n", rc);
		return rc;
	}

	rc = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (rc) {
		pr_err("SLI: hid_hw_start failed: %d\n", rc);
		return rc;
	}

	rc = hid_hw_open(hdev);
	if (rc) {
		pr_err("SLI: hid_hw_open failed: %d\n", rc);
		hid_hw_stop(hdev);
		return rc;
	}

	hub = kzalloc(sizeof(*hub), GFP_KERNEL);
	if (!hub) {
		hid_hw_close(hdev);
		hid_hw_stop(hdev);
		return -ENOMEM;
	}

	hub->hdev = hdev;
	hid_set_drvdata(hdev, hub);

	/* Initialize ports */
	for (i = 0; i < 4; i++) {
		hub->ports[i].index = i;
		hub->ports[i].hub = hub;
		hub->ports[i].fan_speed = 0;
		hub->ports[i].fan_rpm = 0;
		hub->ports[i].fan_connected = true;  /* Default to connected */
	}

	/* Create proc directory */
	hub->procdir = proc_mkdir("Lian_li_SL_INFINITY", NULL);
	if (!hub->procdir) {
		pr_err("SLI: Failed to create proc directory\n");
		kfree(hub);
		hid_hw_close(hdev);
		hid_hw_stop(hdev);
		return -ENOMEM;
	}

	/* Global logging control */
	proc_create("logging_enabled", 0666, hub->procdir, &sli_logging_enabled_ops);

	/* Create proc files for each port */
	for (i = 0; i < 4; i++) {
		char port_name[16];
		struct proc_dir_entry *port_dir;
		struct sli_port *p = &hub->ports[i];

		snprintf(port_name, sizeof(port_name), "Port_%d", i + 1);
		port_dir = proc_mkdir(port_name, hub->procdir);
		if (!port_dir) {
			pr_err("SLI: Failed to create port %d directory\n", i + 1);
			continue;
		}

		/* Fan speed control */
		proc_create_data("fan_speed", 0666, port_dir, &sli_fan_speed_ops, p);
		
		/* Actual RPM from hub hardware (read-only) */
		proc_create_data("fan_rpm", 0444, port_dir, &sli_fan_rpm_ops, p);

		/* Fan connection status (read-only) */
		proc_create_data("fan_connected", 0444, port_dir, &sli_fan_connected_ops, p);
		
		/* Fan configuration (read/write) */
		proc_create_data("fan_config", 0666, port_dir, &sli_fan_config_ops, p);
	}

	g_hub = hub;
	SLI_LOG("HID device initialized\n");

	return 0;
}

/* Remove function */
static void sli_remove(struct hid_device *hdev)
{
	struct sli_hub *hub = hid_get_drvdata(hdev);

	SLI_LOG("Removing device\n");

	if (hub) {
		if (hub->procdir) {
			proc_remove(hub->procdir);
		}
		g_hub = NULL;
		kfree(hub);
	}

	hid_hw_close(hdev);
	hid_hw_stop(hdev);
	
	SLI_LOG("HID device removed\n");
}

static const struct hid_device_id sli_devices[] = {
	{ HID_USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ }
};
MODULE_DEVICE_TABLE(hid, sli_devices);

static struct hid_driver sli_driver = {
	.name = "Lian_Li_SL_INFINITY",
	.id_table = sli_devices,
	.probe = sli_probe,
	.remove = sli_remove,
};

module_hid_driver(sli_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AI + Joey");
MODULE_DESCRIPTION("Lian Li SL Infinity Fan Control Driver (Fan Only)");
MODULE_VERSION("1.0");
