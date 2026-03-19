#ifndef _MTK_MSDC_AES_H_
#define _MTK_MSDC_AES_H_

#ifdef CC_MTD_ENCRYPT_SUPPORT
#include <mtk_msdc.h>

void emmc_aes_init(struct msdc_host *host);
void emmc_gcpu_aes_encrypt(struct msdc_host *host,struct mmc_command	*cmd,struct mmc_data *data);
void emmc_gcpu_aes_decrypt(struct msdc_host *host,struct mmc_command	*cmd,struct mmc_data *data);
void emmc_msdc_aes_encrypt(struct msdc_host *host,struct mmc_command	*cmd);
void emmc_msdc_aes_decrypt(struct msdc_host *host,struct mmc_command	*cmd);
void emmc_aes_close(struct msdc_host *host);
#endif
#endif
