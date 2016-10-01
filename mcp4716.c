/*
 * mcp4716.c - Support for Microchip MCP4716
 *
 * Copyright (C) 2016 Krzysztof Chojnowski <krzysiek@embedev.pl>
 *
 * Based on mcp4725 by Peter Meerwald <pmeerw@pmeerw.net>
 *
 * This file is subject to the terms and conditions of version 2 of
 * the GNU General Public License.  See the file COPYING in the main
 * directory of this archive for more details.
 *
 * driver for the Microchip I2C 10-bit digital-to-analog converter (DAC)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define MCP4716_DRV_NAME "mcp4716"

struct mcp4716_data {
	struct i2c_client *client;
	u16 dac_value;
};

static const struct iio_chan_spec mcp4716_channel = {
	.type		= IIO_VOLTAGE,
	.indexed	= 1,
	.output		= 1,
	.channel	= 0,
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
};

static int mcp4716_set_value(struct iio_dev *indio_dev, int val)
{
	struct mcp4716_data *data = iio_priv(indio_dev);
	u8 outbuf[2];
	int ret;

	if (val >= (1 << 10) || val < 0)
		return -EINVAL;

        val <<= 2;
	outbuf[0] = (val >> 8) & 0xf;
	outbuf[1] = val & 0xfc;

	ret = i2c_master_send(data->client, outbuf, 2);
	if (ret < 0)
		return ret;
	else if (ret != 2)
		return -EIO;
	else
		return 0;
}

static int mcp4716_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct mcp4716_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = data->dac_value;
		return IIO_VAL_INT;
	}
	return -EINVAL;
}

static int mcp4716_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val, int val2, long mask)
{
	struct mcp4716_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = mcp4716_set_value(indio_dev, val);
		data->dac_value = val;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct iio_info mcp4716_info = {
	.read_raw = mcp4716_read_raw,
	.write_raw = mcp4716_write_raw,
	.driver_module = THIS_MODULE,
};

static int mcp4716_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct mcp4716_data *data;
	struct iio_dev *indio_dev;
	u8 inbuf[3];
	int err;


	indio_dev = iio_device_alloc(sizeof(*data));
	if (indio_dev == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);
	data->client = client;

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = id->name;
	indio_dev->info = &mcp4716_info;
	indio_dev->channels = &mcp4716_channel;
	indio_dev->num_channels = 1;
	indio_dev->modes = INDIO_DIRECT_MODE;

	/* read current DAC value */
	err = i2c_master_recv(client, inbuf, 3);
	if (err < 0) {
		dev_err(&client->dev, "failed to read DAC value");
		goto exit_free_device;
	}
	data->dac_value = (inbuf[1] << 2) | (inbuf[2] >> 6);

	err = iio_device_register(indio_dev);
	if (err)
		goto exit_free_device;

	dev_info(&client->dev, "MCP4716 DAC registered\n");

	return 0;

exit_free_device:
	iio_device_free(indio_dev);
exit:
	return err;
}

static int mcp4716_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);

	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

static const struct i2c_device_id mcp4716_id[] = {
	{ "mcp4716", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mcp4716_id);

static struct i2c_driver mcp4716_driver = {
	.driver = {
		.name	= MCP4716_DRV_NAME,
	},
	.probe		= mcp4716_probe,
	.remove		= mcp4716_remove,
	.id_table	= mcp4716_id,
};
module_i2c_driver(mcp4716_driver);

MODULE_AUTHOR("Krzysztof Chojnowski <krzysiek@embedev.pl>");
MODULE_DESCRIPTION("MCP4716 10-bit DAC");
MODULE_LICENSE("GPL");
