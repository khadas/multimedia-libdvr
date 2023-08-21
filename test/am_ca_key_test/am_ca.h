#ifndef _AM_CA_H_
#define _AM_CA_H_

#ifdef CA_TYPES

/**Descrambling algorithm.*/
enum ca_sc2_algo_type {
    CA_ALGO_AES_ECB_CLR_END,    /**< AES ECB clear end.*/
    CA_ALGO_AES_ECB_CLR_FRONT,  /**< AES ECB clear head.*/
    CA_ALGO_AES_CBC_CLR_END,    /**< AES CBC clear end.*/
    CA_ALGO_AES_CBC_IDSA,       /**< IDSA.*/
    CA_ALGO_CSA2,               /**< DVB-CSA2.*/
    CA_ALGO_DES_SCTE41,         /**< DES SCTE41.*/
    CA_ALGO_DES_SCTE52,         /**< DES SCTE52.*/
    CA_ALGO_TDES_ECB_CLR_END,   /**< TDES ECB clear end.*/
    CA_ALGO_CPCM_LSA_MDI_CBC,   /**< CPCM LSA MDI CBC.*/
    CA_ALGO_CPCM_LSA_MDD_CBC,   /**< CPCM LSA MDD CBC.*/
    CA_ALGO_CSA3,               /**< DVB-CSA3*/
    CA_ALGO_ASA,                /**< ASA.*/
    CA_ALGO_ASA_LIGHT,          /**< ASA light.*/
    CA_ALGO_S17_ECB_CLR_END,    /**< S17 ECB clear end.*/
    CA_ALGO_S17_ECB_CTS,        /**< S17 ECB CTS.*/
    CA_ALGO_UNKNOWN
};

/**Descrambler hardware module type for T5W.*/
enum ca_sc2_dsc_type {
    CA_DSC_COMMON_TYPE, /**< TSN module.*/
    CA_DSC_TSD_TYPE,    /**< TSD module..*/
    CA_DSC_TSE_TYPE     /**< TSE module.*/
};

/**Key's parity type.*/
enum ca_sc2_key_type {
    CA_KEY_EVEN_TYPE,    /**< Even key.*/
    CA_KEY_EVEN_IV_TYPE, /**< IV data for even key.*/
    CA_KEY_ODD_TYPE,     /**< Odd key.*/
    CA_KEY_ODD_IV_TYPE,  /**< IV data for odd key.*/
    CA_KEY_00_TYPE,      /**< Key for packets' scrambling control flags == 00.*/
    CA_KEY_00_IV_TYPE    /**< IV data for packets' scrambling control flags == 00.*/
};

#endif /*CA_TYPES*/

/**
 * Open the CA(descrambler) device.
 * \param devno The CA device number.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_open(int devno);

/**
 * Allocate a descrambler channel from the device.
 * \param devno The CA device number.
 * \param pid The descrambled elementary stream's PID of this channel.
 * \param algo The descrambling algorithm.
 * This parameter is defined as "enum ca_sc2_algo_type".
 * \param dsc_type The descrambler hardware module type for T5W.
 * This parameter is defined as "enum ca_sc2_dsc_type".
 * This parameter is not used on T5D.
 * \return The allocated descrambler channel's index.
 * \retval -1 On error.
 */
int ca_alloc_chan(int devno, unsigned int pid, int algo, int dsc_type);

/**
 * Free an unused descrambler channel.
 * \param devno the CA device number.
 * \param chan_index The descrambler channel's index to be freed.
 * The index is allocated by the function ca_alloc_chan.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_free_chan(int devno, int chan_index);

/**
 * Set the key to the descrambler channel.
 * \param devno The CA device number.
 * \param chan_index The descrambler channel's index to be set.
 * The index is allocated by the function ca_alloc_chan.
 * \param parity The key's parity.
 * This parameter is defined as "enum ca_sc2_key_type".
 * \param key_handle The key's handle.
 * The key is allocated and set by the CAS/CI+ TA.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_set_key(int devno, int chan_index, int parity, int key_handle);

/**
 * Set the key to the descrambler channel.
 * \param devno The CA device number.
 * \param chan_index The descrambler channel's index to be set.
 * The index is allocated by the function ca_alloc_chan.
 * \param parity The key's parity.
 * \param key_len The key's length.
 * \param key The key's content.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_set_cw_key(int devno, int chan_index, int parity, int key_len, char *key);

/**
 * Close the CA device.
 * \param devno The CA device number.
 * \retval 0 On success.
 * \retval -1 On error.
 */
int ca_close(int devno);

#endif
