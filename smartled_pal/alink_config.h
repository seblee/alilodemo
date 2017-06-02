/*************************************************
 ALINK CONFIG
 *************************************************/
#include "common.h"

#pragma once

/* raw data device means device post byte stream to cloud,
 * cloud translate byte stream to json value by lua script
 * for each product model, vendor need to sumbit a lua script
 * doing this job
 *
 * undefine RAW_DATA_DEVICE, sample will post json string to cloud
 */
//#define PASS_THROUGH

#define Method_PostData    "postDeviceData"
#define Method_PostRawData "postDeviceRawData"
#define Method_GetAlinkTime "getAlinkTime"

//TODO: update these product info
#define product_dev_name        "alink_product"
#ifdef PASS_THROUGH
#define product_model         "ALINKTEST_LIVING_LIGHT_SMARTLED_LUA"
#define product_key             "bIjq3G1NcgjSfF9uSeK2"
#define product_secret            "W6tXrtzgQHGZqksvJLMdCPArmkecBAdcr2F5tjuF"
#else
#define product_model         "ALINKTEST_ENTERTAINMENT_ATALK_RTOS_TEST"
#define product_key             "BjwvCnPIT6M8BAoCctGH"
#define product_secret            "aZcAxVPG1AdKyozHL3ZSQow2Pe2u1Xmm2Jctgot8"
#endif
#define product_debug_key       "dpZZEpm9eBfqzK7yVeLq"
#define product_debug_secret    "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm"

//#define PRODUCT_ASR_APP_KEY     "with_service"
//#define PRODUCT_ASR_SECRET      "e898e01b457a44db850e7f96d0da2801"


#define __ALI_ONLINE__

#ifdef __ALI_ONLINE__
	#define PRODUCT_ASR_APP_KEY     "open_plat_test"
	#define PRODUCT_ASR_SECRET      "094c31ff123858a2a2bfe7adf01c1232"
#else //__ALI_DAILY__
	#define PRODUCT_ASR_APP_KEY     "open_plat_test"
	#define PRODUCT_ASR_SECRET      "094c31ffa8c858a2a2bfe7adf01cfc92"
#endif

#define product_dev_sn          "6923450656860"
#define product_dev_version     "1.3"
#define product_dev_type        "LIGHT"
#define product_dev_category    "LIVING"
#define product_dev_manufacturer "ALINKTEST"
#define product_dev_cid         "2D0044000F47333139373038"

/* device info */
#define product_dev_ DEV_CATEGORY "LIVING"
#define DEV_TYPE "LIGHT"
#define DEV_MANUFACTURE "ALINKTEST"

typedef struct _alink_config_t
{
    uint8_t alink_config_data[2048];
} alink_config_t;
