#ifdef CC_MTD_ENCRYPT_SUPPORT
#include <mtk_msdc_aes.h>
#include <mtk_msdc_dbg.h>
#include <crypto/mtk_crypto.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/soc/mediatek/mt53xx_linux.h>

extern mtk_partition mtk_msdc_partition;
extern char partition_access;
extern BOOL TZ_CTL(UINT32 u4Cmd, UINT32 u4PAddr, UINT32 u4Size);
static int AES_flag = 0;

static int msdc_partition_encrypted(struct msdc_host *host, struct mmc_command	*cmd)
{
	int i = 0;

	BUG_ON(!host);

	u32 addr_blk = mmc_card_blockaddr(host->mmc->card) ? (cmd->arg) : (cmd->arg >> 9);

	if (host->hw->host_function != MSDC_EMMC) {
		ERR_MSG("decrypt on emmc only!\n");
		return 0;
	}

	if (partition_access == 3) {
		N_MSG(CMD, "rpmb skip enc\n");
		return 0;
	}

	for (i = 1; i <= mtk_msdc_partition.nparts; i++) {
		if ((mtk_msdc_partition.parts[i].attr & ATTR_ENCRYPTION) &&
			(addr_blk >= mtk_msdc_partition.parts[i].start_sect) &&
			(addr_blk < (mtk_msdc_partition.parts[i].start_sect +
			mtk_msdc_partition.parts[i].nr_sects))) {
			N_MSG(CMD, " the buf(0x%08x) is encrypted!\n", addr_blk);
			return 1;
		}
	}

	return 0;
}

#if defined(CONFIG_EMMC_AES_SUPPORT)
/* More AES mode can refer the msdc_test.c in loader. CBC/ECB/CTR */
/* set MSDC AES key */
static void msdc_aes_config_init(struct msdc_host *host)
{
    u32 __iomem *base = host->base;
	N_MSG(CMD, "# MSDC AES config init #\n");
    /* enable ,select setting0
	 * use 480MHZ as default
	 */
    sdr_set_field(EMMC52_AES_EN, AES_CLK_DIV_SEL, 0);
    sdr_set_field(EMMC52_AES_EN, AES_SWITCH_VALID0, 1);

    /* set data unit 512bytes */
    sdr_write32(EMMC52_AES_CFG_GP0, 0x02000000);

	/* close AES bypass for ES2. */
	if (IS_IC_MT5893_ES2())
		sdr_set_field(EMMC52_AES_SWST, AES_BYPASS_EN, 1);
}

/* Enable MSDC AES feature */
static void msdc_aes_open(struct msdc_host *host)
{
	if (IS_IC_MT5893_ES1()) {
		#ifdef CC_TRUSTZONE_SUPPORT
		u32 __iomem *base = host->base;
		TZ_CTL(TZCTL_MSDC_AES_ENABLE, 0, 1);
		AES_flag = 1;
		N_MSG(CMD, "# MSDC AES OPEN #\n");
		#endif
	} else if (IS_IC_MT5893_ES2()) {
		u32 __iomem *base = host->base;
		sdr_set_field(EMMC52_AES_SWST, AES_BYPASS_EN, 0);
		AES_flag = 1;
		N_MSG(CMD,  "# MSDC AES OPEN #\n");
	}
}

/* Disable MSDC AES feature */
static void msdc_aes_close(struct msdc_host *host)
{
	if (IS_IC_MT5893_ES1()) {
		#ifdef CC_TRUSTZONE_SUPPORT
		u32 __iomem *base = host->base;
		TZ_CTL(TZCTL_MSDC_AES_DISABLE, 0, 0);
		AES_flag = 0;
		N_MSG(CMD, "# MSDC AES Close #\n");
		#endif
	} else if (IS_IC_MT5893_ES2()) {
		u32 __iomem *base = host->base;
		sdr_set_field(EMMC52_AES_SWST, AES_BYPASS_EN, 1);
		AES_flag = 0;
		N_MSG(CMD,  "# MSDC AES Close #\n");
	}
}

/* Switch to decryption */
static int msdc_aes_dec_switch(struct msdc_host *host)
{
	int retry = 30000; /* 30 seconds */
    u32 __iomem *base = host->base;
	/* start dec switch */
	sdr_set_field(EMMC52_AES_SWST, AES_DEC, 1);

	/* polling wait */
	while (sdr_read32(EMMC52_AES_SWST) & AES_DEC) {
		retry--;
		if (retry == 0) {
			N_MSG(CMD, "ERROR! MSDC AES Switch DEC FAIL @ line %d \n",__LINE__);
			return -1;
		}
		mdelay(1);
	}

	return 0;
}

/* Switch to encryption */
static int msdc_aes_enc_switch(struct msdc_host *host)
{

	int retry = 30000; /* 30 seconds */
    u32 __iomem *base = host->base;
	/* start dec switch */
	sdr_set_field(EMMC52_AES_SWST, AES_ENC, 1);

	/* polling wait */
	while (sdr_read32(EMMC52_AES_SWST) & AES_ENC) {
		retry--;
		if (retry == 0) {
			N_MSG(CMD, "ERROR! MSDC AES Switch ENC FAIL @ line %d \n",__LINE__);
			return -1;
		}
		mdelay(1);
	}

	return 0;
}

#else
static u8 _pu1MsdcBuf_AES[AES_BUFF_LEN + AES_ADDR_ALIGN], *_pu1MsdcBuf_Aes_Align;

static void msdc_mtd_aes_init(struct msdc_host *host)
{
	u32 tmp;

	if (NAND_AES_INIT())
		N_MSG(CMD, "%s: MTD aes init ok\n", __func__);
	else
		N_MSG(CMD, "%s: MTD aes init failed\n", __func__);

	tmp = (unsigned long)&_pu1MsdcBuf_AES[0] & (AES_ADDR_ALIGN - 1);
	if (tmp != 0x0)
		_pu1MsdcBuf_Aes_Align = (UINT8 *)(((unsigned long)&_pu1MsdcBuf_AES[0] &
				(~((unsigned long)(AES_ADDR_ALIGN - 1)))) + AES_ADDR_ALIGN);
	else
		_pu1MsdcBuf_Aes_Align = &_pu1MsdcBuf_AES[0];

	N_MSG(CMD, "%s: _pu1MsdcBuf_Aes_Align is 0x%p, _pu1MsdcBuf_AES is 0x%p\n",
			__func__, _pu1MsdcBuf_Aes_Align, &_pu1MsdcBuf_AES[0]);

	return;
}

static int msdc_gcpu_encryption_sg(struct mmc_data *data, struct msdc_host *host)
{
	u32 len_used = 0, len_total = 0, len = 0, i = 0;
	unsigned long buff = 0;
	struct scatterlist *sg;

	BUG_ON(!host);

	if (host->hw->host_function != MSDC_EMMC) {
		ERR_MSG("encrypt on emmc only!\n");
		return -1;
	}

	if (!data) {
		ERR_MSG("encrypt data is invalid\n");
		return -1;
	}

	BUG_ON(data->sg_len > 1);
	for_each_sg(data->sg, sg, data->sg_len, i) {
		len = sg->length;
		buff = (unsigned long)sg_virt(sg);
		len_total = len;

		do {
			len_used = (len_total > AES_BUFF_LEN) ? AES_BUFF_LEN : len_total;
			if ((len_used & (AES_LEN_ALIGN - 1)) != 0x0) {
				ERR_MSG("the buffer(0x%08lx) to be encrypted is not align to %d bytes!\n",
						buff, AES_LEN_ALIGN);
				return -1;
			}
			memcpy((void *)_pu1MsdcBuf_Aes_Align, (void *)buff, len_used);
			// TODO: 64bit to 32bit!!!!!!!!!!!!!!
			if (!NAND_AES_Encryption(virt_to_phys((void *)_pu1MsdcBuf_Aes_Align),
				virt_to_phys((void *)_pu1MsdcBuf_Aes_Align), len_used)) {
				ERR_MSG("Encryption to buffer(addr:0x%p size:0x%08X) failed!\n",
						_pu1MsdcBuf_Aes_Align, len_used);
				return -1;
			}
			memcpy((void *)buff, (void *)_pu1MsdcBuf_Aes_Align, len_used);

			len_total -= len_used;
			buff += len_used;
		} while (len_total > 0);
	}

	return 0;
}

static int msdc_gcpu_decryption_sg(struct mmc_data *data, struct msdc_host *host)
{
	u32 len_used = 0, len_total = 0, len = 0, i = 0;
	unsigned long buff = 0;
	struct scatterlist *sg;

	BUG_ON(!host);

	if (host->hw->host_function != MSDC_EMMC) {
		ERR_MSG("decrypt on emmc only!\n");
		return -1;
	}

	if (!data) {
		ERR_MSG("decrypt data is invalid\n");
		return -1;
	}
	BUG_ON(data->sg_len > 1);
	for_each_sg(data->sg, sg, data->sg_len, i) {
		unsigned int offset = 0;

		len = sg->length;
		buff = (unsigned long)sg_virt(sg);
		len_total = len;
		do {
			len_used = (len_total > AES_BUFF_LEN) ? AES_BUFF_LEN : len_total;
			if ((len_used & (AES_LEN_ALIGN - 1)) != 0x0) {
				ERR_MSG("the buffer(0x%08lx) to be decrypted is not align to %d bytes!\n",
					buff, AES_LEN_ALIGN);
				return -1;
			}

			memcpy((void *)_pu1MsdcBuf_Aes_Align, (void *)buff, len_used);
			// TODO: 64bit to 32bit!!!!!!!!!!!!!!
			if (!NAND_AES_Decryption(virt_to_phys((void *)_pu1MsdcBuf_Aes_Align),
				virt_to_phys((void *)_pu1MsdcBuf_Aes_Align), len_used)) {
				ERR_MSG("Decryption to buffer(addr:0x%p size:0x%08X) failed!\n",
					_pu1MsdcBuf_Aes_Align, len_used);
				return -1;
			}
			memcpy((void *)buff, (void *)_pu1MsdcBuf_Aes_Align, len_used);

			len_total -= len_used;
			buff += len_used;
		} while (len_total > 0);
	}

	return 0;
}
#endif

/*
 *  extern interface
 */
void emmc_aes_init(struct msdc_host *host)
{
#if defined(CONFIG_EMMC_AES_SUPPORT)
    msdc_aes_config_init(host);
#else
    msdc_mtd_aes_init(host);
#endif
}

void emmc_gcpu_aes_encrypt(struct msdc_host *host, struct mmc_command *cmd,struct mmc_data *data)
{
#if defined(CONFIG_EMMC_AES_SUPPORT)
#else
    if (((cmd->opcode == MMC_WRITE_BLOCK) ||
        (cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) &&
        (host->hw->host_function == MSDC_EMMC))
    {
        if (msdc_partition_encrypted(host, cmd)) {
            if (msdc_gcpu_encryption_sg(data, host)) {
                ERR_MSG("[1]encryption before write process failed!\n");
                BUG_ON(1);
            }
        }
    }
#endif
}

void emmc_gcpu_aes_decrypt(struct msdc_host *host, struct mmc_command *cmd, struct mmc_data *data)
{
#if defined(CONFIG_EMMC_AES_SUPPORT)
#else
    if (((cmd->opcode == MMC_READ_SINGLE_BLOCK) ||
        (cmd->opcode == MMC_READ_MULTIPLE_BLOCK)) &&
        (host->hw->host_function == MSDC_EMMC))
    {
        if (msdc_partition_encrypted(host, cmd)) {
            if (msdc_gcpu_decryption_sg(data, host)) {
                ERR_MSG("[1]decryption after read process failed!\n");
                BUG_ON(1);
            }
        }
    }
#endif
}

void emmc_msdc_aes_encrypt(struct msdc_host *host, struct mmc_command	*cmd)
{
#if defined(CONFIG_EMMC_AES_SUPPORT)
	if (((cmd->opcode == MMC_WRITE_BLOCK) ||
        (cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) &&
        (host->hw->host_function == MSDC_EMMC))
    {
        if (msdc_partition_encrypted(host, cmd)) {
			N_MSG(CMD, "[1]aes encryption!\n");
			msdc_aes_open(host);
			msdc_aes_enc_switch(host);
        }
    }
#endif
}

void emmc_msdc_aes_decrypt(struct msdc_host *host, struct mmc_command *cmd)
{
#if defined(CONFIG_EMMC_AES_SUPPORT)
    if(((cmd->opcode == MMC_READ_SINGLE_BLOCK) ||
        (cmd->opcode == MMC_READ_MULTIPLE_BLOCK)) &&
        (host->hw->host_function == MSDC_EMMC))
    {
        if (msdc_partition_encrypted(host, cmd)) {
			N_MSG(CMD, "[1]aes decryption!\n");
			msdc_aes_open(host);
			msdc_aes_dec_switch(host);
        }
    }
#endif
}

void emmc_aes_close(struct msdc_host *host)
{
#if defined(CONFIG_EMMC_AES_SUPPORT)
	if (AES_flag)
		msdc_aes_close(host);
#endif
}
#endif
