/*
 * Lian Li SL Infinity Fan Control Driver (Fan Only)
 * 
 * This driver provides fan speed control for Lian Li SL Infinity fans.
 * RGB control is handled by OpenRGB to avoid conflicts.
 * 
 * Exposes:
 *   /proc/Lian_li_SL_INFINITY/Port_X/fan_speed      (write 0–100, read current setting)
 *   /proc/Lian_li_SL_INFINITY/Port_X/fan_connected  (read 0/1 - is fan configured)
 *   /proc/Lian_li_SL_INFINITY/Port_X/fan_config     (write 0/1 - configure fan presence)
 *
 * Author: AI + Joey
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hid.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define VENDOR_ID  0x0CF2
#define PRODUCT_ID 0xA102

struct sli_port {
	int index;  /* 0..3 */
	struct sli_hub *hub;
	u8 fan_speed;  /* Current fan speed (0-100) */
	bool fan_connected;  /* Is a fan connected to this port? (user configured) */
};

struct sli_hub {
	struct hid_device *hdev;
	struct proc_dir_entry *procdir;
	struct sli_port ports[4];  /* 4 ports */
};

static struct sli_hub *g_hub = NULL;

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
	
	/* hid_hw_raw_request returns number of bytes transferred on success (7), not 0 */
	if (rc >= 0) {
		p->fan_speed = speed_percent;
		pr_info("SLI: Port %d set to %d%%\n", port_num, speed_percent);
		return 0;  /* Return 0 for success */
	} else {
		pr_err("SLI: Failed to set port %d speed: error %d\n", port_num, rc);
		return rc;  /* Return negative error code */
	}
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

	pr_info("SLI: Port %d fan configuration set to %s\n", 
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

/* Probe function */
static int sli_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct sli_hub *hub;
	int rc;
	int i;

	pr_info("SLI: Probing device\n");

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
		
		/* Fan connection status (read-only) */
		proc_create_data("fan_connected", 0444, port_dir, &sli_fan_connected_ops, p);
		
		/* Fan configuration (read/write) */
		proc_create_data("fan_config", 0666, port_dir, &sli_fan_config_ops, p);
	}

	g_hub = hub;
	pr_info("SLI: HID device initialized\n");

	return 0;
}

/* Remove function */
static void sli_remove(struct hid_device *hdev)
{
	struct sli_hub *hub = hid_get_drvdata(hdev);

	pr_info("SLI: Removing device\n");

	if (hub) {
		if (hub->procdir) {
			proc_remove(hub->procdir);
		}
		g_hub = NULL;
		kfree(hub);
	}

	hid_hw_close(hdev);
	hid_hw_stop(hdev);
	
	pr_info("SLI: HID device removed\n");
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
