/*
 * drivers/net/phy/motorcomm.c
 *
 * Driver for Motorcomm PHYs
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/phy.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/of.h>

#define YT8512H_EXT_REG_ADDR_OFFSET_REG	0x1E
#define YT8512H_EXT_REG_DATA_REG	0x1F
#define YT8512H_EXT_LED1_CFG	0x40C3
#define YT8512H_EXT_LED0_CFG	0x40C0

#define YT8521SH_EXT_REG_ADDR_OFFSET_REG	0x1E
#define YT8521SH_EXT_REG_DATA_REG	0x1F
#define YT8521SH_EXT_LED2_CFG	0xA00E
#define YT8521SH_EXT_LED1_CFG	0xA00D
#define YT8521SH_EXT_COMMON_RGMII_CFG1			0xA003
#define YT8521SH_EXT_MDIO_CFG_AND_RGMII_OOB_MON	0xA005
#define YT8521SH_EXT_REG_TX_OFFSET			0
#define YT8521SH_EXT_REG_RX_OFFSET			10
#define YT8521SH_EXT_REG_EN_PHYADDR0_OFFSET		6

static unsigned long phy_tx_delay = 0;
static unsigned long phy_rx_delay = 0;

MODULE_DESCRIPTION("Motorcomm PHY driver");
MODULE_AUTHOR("www.tronlong.com");
MODULE_LICENSE("GPL");

static int yt8512h_config_init(struct phy_device *phydev)
{
	int value;

	/* Reset phy */
	phy_write(phydev, MII_BMCR, BMCR_RESET);
	while (BMCR_RESET & phy_read(phydev, MII_BMCR))
		msleep(30);

	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, (value & ~BMCR_PDOWN));
	msleep(50);

	/* Config LED1 to ON when phy link up */
	phy_write(phydev, YT8512H_EXT_REG_ADDR_OFFSET_REG, YT8512H_EXT_LED1_CFG);
	msleep(50);
	phy_write(phydev, YT8512H_EXT_REG_DATA_REG, 0x0030);
	msleep(50);

	/* Config LED0 to BLINK when phy link up and tx rx active */
	phy_write(phydev, YT8512H_EXT_REG_ADDR_OFFSET_REG, YT8512H_EXT_LED0_CFG);
	msleep(50);
	phy_write(phydev, YT8512H_EXT_REG_DATA_REG, 0x0330);

	return 0;
}

static int yt8521sh_config_init(struct phy_device *phydev)
{
	struct device_node *of_node = phydev->mdio.dev.of_node;
	int value;

	/* Reset phy */
	phy_write(phydev, MII_BMCR, BMCR_RESET);
	while (BMCR_RESET & phy_read(phydev, MII_BMCR))
		msleep(30);

	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, (value & ~BMCR_PDOWN));
	msleep(50);

	/* Config LED2 to ON when phy link up */
	phy_write(phydev, YT8521SH_EXT_REG_ADDR_OFFSET_REG, YT8521SH_EXT_LED2_CFG);
	msleep(50);
	phy_write(phydev, YT8521SH_EXT_REG_DATA_REG, 0x0070);

	/* Config LED1 to BLINK when phy link up and tx rx active */
	phy_write(phydev, YT8521SH_EXT_REG_ADDR_OFFSET_REG, YT8521SH_EXT_LED1_CFG);
	msleep(50);
	phy_write(phydev, YT8521SH_EXT_REG_DATA_REG, 0x0670);

	/* Get phy tx/rx delay vlaues from devicetree */
	if(!of_property_read_u32(of_node, "phy-tx-delay", &value))
		phy_tx_delay = value;

	if(!of_property_read_u32(of_node, "phy-rx-delay", &value))
		phy_rx_delay = value;

	/* Read rgmii cfg1 */
	phy_write(phydev, YT8521SH_EXT_REG_ADDR_OFFSET_REG, YT8521SH_EXT_COMMON_RGMII_CFG1);
	msleep(50);
	value = phy_read(phydev, YT8521SH_EXT_REG_DATA_REG);
	msleep(50);

	value &= ~(0xf << YT8521SH_EXT_REG_TX_OFFSET);
	value |= ((phy_tx_delay & 0xf) << YT8521SH_EXT_REG_TX_OFFSET);

	value &= ~(0xf << YT8521SH_EXT_REG_RX_OFFSET);
	value |= ((phy_rx_delay & 0xf) << YT8521SH_EXT_REG_RX_OFFSET);

	/* Config phy tx/rx delay */
	phy_write(phydev, YT8521SH_EXT_REG_ADDR_OFFSET_REG, YT8521SH_EXT_COMMON_RGMII_CFG1);
	msleep(50);
	phy_write(phydev, YT8521SH_EXT_REG_DATA_REG, value);

	if(of_property_read_bool(of_node, "phy-address-broadcast-off")) {
		/*
		 * Disable total response phy address 0.
		 * Only respond to MDIO command whose PHYAD filed equals to PHY address strapping.
		 */

		/* Read MDIO_Cfg_And_RGMII_OOB_Mon reg*/
		phy_write(phydev, YT8521SH_EXT_REG_ADDR_OFFSET_REG, YT8521SH_EXT_MDIO_CFG_AND_RGMII_OOB_MON);
		msleep(50);
		value = phy_read(phydev, YT8521SH_EXT_REG_DATA_REG);
		msleep(50);

		value &= ~(0x1 << YT8521SH_EXT_REG_EN_PHYADDR0_OFFSET);

		phy_write(phydev, YT8521SH_EXT_REG_DATA_REG, value);
	}

	return 0;
}

static struct phy_driver motorcomm_driver[] = {
	{
		PHY_ID_MATCH_EXACT(0x00000128),
		.name		= "YT8512H Ethernet",
		.features	= PHY_BASIC_FEATURES,
		.suspend        = &genphy_suspend,
		.resume         = &genphy_resume,
		.flags		= PHY_IS_INTERNAL,
		.config_init	= &yt8512h_config_init,
		.config_aneg    = &genphy_config_aneg,
		.read_status    = &genphy_read_status,
	}, {
		PHY_ID_MATCH_EXACT(0x0000011a),
		.name		= "YT8521SH Gigabit Ethernet",
		.features	= PHY_GBIT_FEATURES,
		.suspend 	= &genphy_suspend,
		.resume 	= &genphy_resume,
		.flags		= PHY_IS_INTERNAL,
		.config_init	= &yt8521sh_config_init,
		.config_aneg	= &genphy_config_aneg,
		.read_status	= &genphy_read_status,
	}
};

module_phy_driver(motorcomm_driver);

static struct mdio_device_id __maybe_unused motorcomm_tbl[] = {
	{ PHY_ID_MATCH_VENDOR(0x00000128) },
	{ PHY_ID_MATCH_VENDOR(0x0000011a) },
	{ }
};

MODULE_DEVICE_TABLE(mdio, motorcomm_tbl);
