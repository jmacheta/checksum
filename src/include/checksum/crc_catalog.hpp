#ifndef CHECKSUM_CRC_CATALOG_HPP
#define CHECKSUM_CRC_CATALOG_HPP

#include <checksum/crc.hpp>

/**
 * @addtogroup checksum
 * @{
 *   @addtogroup checksum_crc
 *   @{
 */

namespace checksum {

/// CRC-3/GSM: polynomial x^3 + x + 1.
inline constexpr crc_parameters crc3_gsm{
    .polynomial = *crc_polynomial::create(0x3, 3),
    .initial_value = 0x0,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x7,
};

/// CRC-3/ROHC: polynomial x^3 + x + 1.
inline constexpr crc_parameters crc3_rohc{
    .polynomial = *crc_polynomial::create(0x3, 3),
    .initial_value = 0x7,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0,
};

/// CRC-4/G-704: polynomial x^4 + x + 1. Also known as CRC-4/ITU.
inline constexpr crc_parameters crc4_g_704{
    .polynomial = *crc_polynomial::create(0x3, 4),
    .initial_value = 0x0,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0,
};

/// CRC-4/INTERLAKEN: polynomial x^4 + x + 1.
inline constexpr crc_parameters crc4_interlaken{
    .polynomial = *crc_polynomial::create(0x3, 4),
    .initial_value = 0xF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xF,
};

/// CRC-5/EPC-C1G2: polynomial x^5 + x^3 + 1. Also known as CRC-5/EPC.
inline constexpr crc_parameters crc5_epc_c1g2{
    .polynomial = *crc_polynomial::create(0x09, 5),
    .initial_value = 0x09,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-5/G-704: polynomial x^5 + x^4 + x^2 + 1. Also known as CRC-5/ITU.
inline constexpr crc_parameters crc5_g_704{
    .polynomial = *crc_polynomial::create(0x15, 5),
    .initial_value = 0x00,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-5/USB: polynomial x^5 + x^2 + 1.
inline constexpr crc_parameters crc5_usb{
    .polynomial = *crc_polynomial::create(0x05, 5),
    .initial_value = 0x1F,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x1F,
};

/// CRC-6/CDMA2000-A: polynomial x^6 + x^5 + x^2 + x + 1.
inline constexpr crc_parameters crc6_cdma2000_a{
    .polynomial = *crc_polynomial::create(0x27, 6),
    .initial_value = 0x3F,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-6/CDMA2000-B: polynomial x^6 + x^2 + x + 1.
inline constexpr crc_parameters crc6_cdma2000_b{
    .polynomial = *crc_polynomial::create(0x07, 6),
    .initial_value = 0x3F,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-6/DARC: polynomial x^6 + x^4 + x^3 + 1.
inline constexpr crc_parameters crc6_darc{
    .polynomial = *crc_polynomial::create(0x19, 6),
    .initial_value = 0x00,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-6/G-704: polynomial x^6 + x + 1. Also known as CRC-6/ITU.
inline constexpr crc_parameters crc6_g_704{
    .polynomial = *crc_polynomial::create(0x03, 6),
    .initial_value = 0x00,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-6/GSM: polynomial x^6 + x^5 + x^3 + x^2 + x + 1.
inline constexpr crc_parameters crc6_gsm{
    .polynomial = *crc_polynomial::create(0x2F, 6),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x3F,
};

/// CRC-7/MMC: polynomial x^7 + x^3 + 1. Also known as CRC-7.
inline constexpr crc_parameters crc7_mmc{
    .polynomial = *crc_polynomial::create(0x09, 7),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-7/ROHC: polynomial x^7 + x^6 + x^3 + x^2 + x + 1.
inline constexpr crc_parameters crc7_rohc{
    .polynomial = *crc_polynomial::create(0x4F, 7),
    .initial_value = 0x7F,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-7/UMTS: polynomial x^7 + x^6 + x^2 + 1.
inline constexpr crc_parameters crc7_umts{
    .polynomial = *crc_polynomial::create(0x45, 7),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/AUTOSAR: polynomial x^8 + x^5 + x^3 + x^2 + x + 1.
inline constexpr crc_parameters crc8_autosar{
    .polynomial = *crc_polynomial::create(0x2F, 8),
    .initial_value = 0xFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFF,
};

/// CRC-8/BLUETOOTH: polynomial x^8 + x^7 + x^5 + x^2 + x + 1.
inline constexpr crc_parameters crc8_bluetooth{
    .polynomial = *crc_polynomial::create(0xA7, 8),
    .initial_value = 0x00,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-8/CDMA2000: polynomial x^8 + x^7 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc8_cdma2000{
    .polynomial = *crc_polynomial::create(0x9B, 8),
    .initial_value = 0xFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/DARC: polynomial x^8 + x^5 + x^4 + x^3 + 1.
inline constexpr crc_parameters crc8_darc{
    .polynomial = *crc_polynomial::create(0x39, 8),
    .initial_value = 0x00,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-8/DVB-S2: polynomial x^8 + x^7 + x^6 + x^4 + x^2 + 1.
inline constexpr crc_parameters crc8_dvb_s2{
    .polynomial = *crc_polynomial::create(0xD5, 8),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/GSM-A: polynomial x^8 + x^4 + x^3 + x^2 + 1.
inline constexpr crc_parameters crc8_gsm_a{
    .polynomial = *crc_polynomial::create(0x1D, 8),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/GSM-B: polynomial x^8 + x^6 + x^3 + 1.
inline constexpr crc_parameters crc8_gsm_b{
    .polynomial = *crc_polynomial::create(0x49, 8),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFF,
};

/// CRC-8/HITAG: polynomial x^8 + x^4 + x^3 + x^2 + 1.
inline constexpr crc_parameters crc8_hitag{
    .polynomial = *crc_polynomial::create(0x1D, 8),
    .initial_value = 0xFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/I-432-1: polynomial x^8 + x^2 + x + 1. Also known as CRC-8/ITU.
inline constexpr crc_parameters crc8_i_432_1{
    .polynomial = *crc_polynomial::create(0x07, 8),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x55,
};

/// CRC-8/I-CODE: polynomial x^8 + x^4 + x^3 + x^2 + 1.
inline constexpr crc_parameters crc8_i_code{
    .polynomial = *crc_polynomial::create(0x1D, 8),
    .initial_value = 0xFD,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/LTE: polynomial x^8 + x^7 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc8_lte{
    .polynomial = *crc_polynomial::create(0x9B, 8),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/MAXIM-DOW: polynomial x^8 + x^5 + x^4 + 1. Also known as CRC-8/MAXIM, DOW-CRC.
inline constexpr crc_parameters crc8_maxim_dow{
    .polynomial = *crc_polynomial::create(0x31, 8),
    .initial_value = 0x00,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-8/MIFARE-MAD: polynomial x^8 + x^4 + x^3 + x^2 + 1.
inline constexpr crc_parameters crc8_mifare_mad{
    .polynomial = *crc_polynomial::create(0x1D, 8),
    .initial_value = 0xC7,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/NRSC-5: polynomial x^8 + x^5 + x^4 + 1.
inline constexpr crc_parameters crc8_nrsc_5{
    .polynomial = *crc_polynomial::create(0x31, 8),
    .initial_value = 0xFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/OPENSAFETY: polynomial x^8 + x^5 + x^3 + x^2 + x + 1.
inline constexpr crc_parameters crc8_opensafety{
    .polynomial = *crc_polynomial::create(0x2F, 8),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/ROHC: polynomial x^8 + x^2 + x + 1.
inline constexpr crc_parameters crc8_rohc{
    .polynomial = *crc_polynomial::create(0x07, 8),
    .initial_value = 0xFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-8/SAE-J1850: polynomial x^8 + x^4 + x^3 + x^2 + 1.
inline constexpr crc_parameters crc8_sae_j1850{
    .polynomial = *crc_polynomial::create(0x1D, 8),
    .initial_value = 0xFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFF,
};

/// CRC-8/SMBUS: polynomial x^8 + x^2 + x + 1. Also known as CRC-8.
inline constexpr crc_parameters crc8_smbus{
    .polynomial = *crc_polynomial::create(0x07, 8),
    .initial_value = 0x00,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00,
};

/// CRC-8/TECH-3250: polynomial x^8 + x^4 + x^3 + x^2 + 1. Also known as CRC-8/AES, CRC-8/EBU.
inline constexpr crc_parameters crc8_tech_3250{
    .polynomial = *crc_polynomial::create(0x1D, 8),
    .initial_value = 0xFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-8/WCDMA: polynomial x^8 + x^7 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc8_wcdma{
    .polynomial = *crc_polynomial::create(0x9B, 8),
    .initial_value = 0x00,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00,
};

/// CRC-10/ATM: polynomial x^10 + x^9 + x^5 + x^4 + x + 1. Also known as CRC-10, CRC-10/I-610.
inline constexpr crc_parameters crc10_atm{
    .polynomial = *crc_polynomial::create(0x233, 10),
    .initial_value = 0x000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000,
};

/// CRC-10/CDMA2000: polynomial x^10 + x^9 + x^8 + x^7 + x^6 + x^4 + x^3 + 1.
inline constexpr crc_parameters crc10_cdma2000{
    .polynomial = *crc_polynomial::create(0x3D9, 10),
    .initial_value = 0x3FF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000,
};

/// CRC-10/GSM: polynomial x^10 + x^8 + x^6 + x^5 + x^4 + x^2 + 1.
inline constexpr crc_parameters crc10_gsm{
    .polynomial = *crc_polynomial::create(0x175, 10),
    .initial_value = 0x000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x3FF,
};

/// CRC-11/FLEXRAY: polynomial x^11 + x^9 + x^8 + x^7 + x^2 + 1. Also known as CRC-11.
inline constexpr crc_parameters crc11_flexray{
    .polynomial = *crc_polynomial::create(0x385, 11),
    .initial_value = 0x01A,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000,
};

/// CRC-11/UMTS: polynomial x^11 + x^9 + x^8 + x^2 + x + 1.
inline constexpr crc_parameters crc11_umts{
    .polynomial = *crc_polynomial::create(0x307, 11),
    .initial_value = 0x000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000,
};

/// CRC-12/CDMA2000: polynomial x^12 + x^11 + x^10 + x^9 + x^8 + x^4 + x + 1.
inline constexpr crc_parameters crc12_cdma2000{
    .polynomial = *crc_polynomial::create(0xF13, 12),
    .initial_value = 0xFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000,
};

/// CRC-12/DECT: polynomial x^12 + x^11 + x^3 + x^2 + x + 1. Also known as X-CRC-12.
inline constexpr crc_parameters crc12_dect{
    .polynomial = *crc_polynomial::create(0x80F, 12),
    .initial_value = 0x000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000,
};

/// CRC-12/GSM: polynomial x^12 + x^11 + x^10 + x^8 + x^5 + x^4 + 1.
inline constexpr crc_parameters crc12_gsm{
    .polynomial = *crc_polynomial::create(0xD31, 12),
    .initial_value = 0x000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFF,
};

/// CRC-12/UMTS: polynomial x^12 + x^11 + x^3 + x^2 + x + 1. Also known as CRC-12/3GPP.
inline constexpr crc_parameters crc12_umts{
    .polynomial = *crc_polynomial::create(0x80F, 12),
    .initial_value = 0x000,
    .reflect_input = false,
    .reflect_output = true,
    .final_xor = 0x000,
};

/// CRC-13/BBC: polynomial x^13 + x^12 + x^11 + x^10 + x^7 + x^6 + x^5 + x^4 + x^2 + 1.
inline constexpr crc_parameters crc13_bbc{
    .polynomial = *crc_polynomial::create(0x1CF5, 13),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-14/DARC: polynomial x^14 + x^11 + x^2 + 1.
inline constexpr crc_parameters crc14_darc{
    .polynomial = *crc_polynomial::create(0x0805, 14),
    .initial_value = 0x0000,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-14/GSM: polynomial x^14 + x^13 + x^5 + x^3 + x^2 + 1.
inline constexpr crc_parameters crc14_gsm{
    .polynomial = *crc_polynomial::create(0x202D, 14),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x3FFF,
};

/// CRC-15/CAN: polynomial x^15 + x^14 + x^10 + x^8 + x^7 + x^4 + x^3 + 1. Also known as CRC-15.
inline constexpr crc_parameters crc15_can{
    .polynomial = *crc_polynomial::create(0x4599, 15),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-15/MPT1327: polynomial x^15 + x^14 + x^13 + x^11 + x^4 + x^2 + 1.
inline constexpr crc_parameters crc15_mpt1327{
    .polynomial = *crc_polynomial::create(0x6815, 15),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0001,
};

/// CRC-16/ARC: polynomial x^16 + x^15 + x^2 + 1. Also known as ARC, CRC-16, CRC-16/LHA, CRC-IBM.
inline constexpr crc_parameters crc16_arc{
    .polynomial = *crc_polynomial::create(0x8005, 16),
    .initial_value = 0x0000,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-16/CDMA2000: polynomial x^16 + x^15 + x^14 + x^11 + x^6 + x^5 + x^2 + x + 1.
inline constexpr crc_parameters crc16_cdma2000{
    .polynomial = *crc_polynomial::create(0xC867, 16),
    .initial_value = 0xFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/CMS: polynomial x^16 + x^15 + x^2 + 1.
inline constexpr crc_parameters crc16_cms{
    .polynomial = *crc_polynomial::create(0x8005, 16),
    .initial_value = 0xFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/DDS-110: polynomial x^16 + x^15 + x^2 + 1.
inline constexpr crc_parameters crc16_dds_110{
    .polynomial = *crc_polynomial::create(0x8005, 16),
    .initial_value = 0x800D,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/DECT-R: polynomial x^16 + x^10 + x^8 + x^7 + x^3 + 1. Also known as R-CRC-16.
inline constexpr crc_parameters crc16_dect_r{
    .polynomial = *crc_polynomial::create(0x0589, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0001,
};

/// CRC-16/DECT-X: polynomial x^16 + x^10 + x^8 + x^7 + x^3 + 1. Also known as X-CRC-16.
inline constexpr crc_parameters crc16_dect_x{
    .polynomial = *crc_polynomial::create(0x0589, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/DNP: polynomial x^16 + x^13 + x^12 + x^11 + x^10 + x^8 + x^6 + x^5 + x^2 + 1.
inline constexpr crc_parameters crc16_dnp{
    .polynomial = *crc_polynomial::create(0x3D65, 16),
    .initial_value = 0x0000,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFF,
};

/// CRC-16/EN-13757: polynomial x^16 + x^13 + x^12 + x^11 + x^10 + x^8 + x^6 + x^5 + x^2 + 1.
inline constexpr crc_parameters crc16_en_13757{
    .polynomial = *crc_polynomial::create(0x3D65, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFF,
};

/// CRC-16/GENIBUS: polynomial x^16 + x^12 + x^5 + 1. Also known as CRC-16/DARC, CRC-16/EPC, CRC-16/EPC-C1G2, CRC-16/I-CODE.
inline constexpr crc_parameters crc16_genibus{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0xFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFF,
};

/// CRC-16/GSM: polynomial x^16 + x^12 + x^5 + 1.
inline constexpr crc_parameters crc16_gsm{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFF,
};

/// CRC-16/IBM-3740: polynomial x^16 + x^12 + x^5 + 1. Also known as CRC-16/AUTOSAR, CRC-16/CCITT-FALSE.
inline constexpr crc_parameters crc16_ibm_3740{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0xFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/IBM-SDLC: polynomial x^16 + x^12 + x^5 + 1. Also known as CRC-16/ISO-HDLC, CRC-16/ISO-IEC-14443-3-B, CRC-16/X-25, CRC-B, X-25.
inline constexpr crc_parameters crc16_ibm_sdlc{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0xFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFF,
};

/// CRC-16/ISO-IEC-14443-3-A: polynomial x^16 + x^12 + x^5 + 1. Also known as CRC-A.
inline constexpr crc_parameters crc16_iso_iec_14443_3_a{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0xC6C6,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-16/KERMIT: polynomial x^16 + x^12 + x^5 + 1. Also known as CRC-16/BLUETOOTH, CRC-16/CCITT, CRC-16/CCITT-TRUE, CRC-16/V-41-LSB, CRC-CCITT,
/// KERMIT.
inline constexpr crc_parameters crc16_kermit{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0x0000,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-16/LJ1200: polynomial x^16 + x^14 + x^13 + x^11 + x^10 + x^9 + x^8 + x^6 + x^5 + x + 1.
inline constexpr crc_parameters crc16_lj1200{
    .polynomial = *crc_polynomial::create(0x6F63, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/M17: polynomial x^16 + x^14 + x^12 + x^11 + x^8 + x^5 + x^4 + x^2 + 1.
inline constexpr crc_parameters crc16_m17{
    .polynomial = *crc_polynomial::create(0x5935, 16),
    .initial_value = 0xFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/MAXIM-DOW: polynomial x^16 + x^15 + x^2 + 1. Also known as CRC-16/MAXIM.
inline constexpr crc_parameters crc16_maxim_dow{
    .polynomial = *crc_polynomial::create(0x8005, 16),
    .initial_value = 0x0000,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFF,
};

/// CRC-16/MCRF4XX: polynomial x^16 + x^12 + x^5 + 1.
inline constexpr crc_parameters crc16_mcrf4xx{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0xFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-16/MODBUS: polynomial x^16 + x^15 + x^2 + 1. Also known as MODBUS.
inline constexpr crc_parameters crc16_modbus{
    .polynomial = *crc_polynomial::create(0x8005, 16),
    .initial_value = 0xFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-16/NRSC-5: polynomial x^16 + x^11 + x^3 + x + 1.
inline constexpr crc_parameters crc16_nrsc_5{
    .polynomial = *crc_polynomial::create(0x080B, 16),
    .initial_value = 0xFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-16/OPENSAFETY-A: polynomial x^16 + x^14 + x^12 + x^11 + x^8 + x^5 + x^4 + x^2 + 1.
inline constexpr crc_parameters crc16_opensafety_a{
    .polynomial = *crc_polynomial::create(0x5935, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/OPENSAFETY-B: polynomial x^16 + x^14 + x^13 + x^12 + x^10 + x^8 + x^6 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc16_opensafety_b{
    .polynomial = *crc_polynomial::create(0x755B, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/PROFIBUS: polynomial x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^6 + x^3 + x^2 + x + 1. Also known as CRC-16/IEC-61158-2.
inline constexpr crc_parameters crc16_profibus{
    .polynomial = *crc_polynomial::create(0x1DCF, 16),
    .initial_value = 0xFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFF,
};

/// CRC-16/RIELLO: polynomial x^16 + x^12 + x^5 + 1.
inline constexpr crc_parameters crc16_riello{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0xB2AA,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-16/SPI-FUJITSU: polynomial x^16 + x^12 + x^5 + 1. Also known as CRC-16/AUG-CCITT.
inline constexpr crc_parameters crc16_spi_fujitsu{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0x1D0F,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/T10-DIF: polynomial x^16 + x^15 + x^11 + x^9 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1.
inline constexpr crc_parameters crc16_t10_dif{
    .polynomial = *crc_polynomial::create(0x8BB7, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/TELEDISK: polynomial x^16 + x^15 + x^13 + x^7 + x^4 + x^2 + x + 1.
inline constexpr crc_parameters crc16_teledisk{
    .polynomial = *crc_polynomial::create(0xA097, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/TMS37157: polynomial x^16 + x^12 + x^5 + 1.
inline constexpr crc_parameters crc16_tms37157{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0x89EC,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000,
};

/// CRC-16/UMTS: polynomial x^16 + x^15 + x^2 + 1. Also known as CRC-16/BUYPASS, CRC-16/VERIFONE.
inline constexpr crc_parameters crc16_umts{
    .polynomial = *crc_polynomial::create(0x8005, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-16/USB: polynomial x^16 + x^15 + x^2 + 1.
inline constexpr crc_parameters crc16_usb{
    .polynomial = *crc_polynomial::create(0x8005, 16),
    .initial_value = 0xFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFF,
};

/// CRC-16/XMODEM: polynomial x^16 + x^12 + x^5 + 1. Also known as CRC-16/ACORN, CRC-16/LTE, CRC-16/V-41-MSB, XMODEM, ZMODEM.
inline constexpr crc_parameters crc16_xmodem{
    .polynomial = *crc_polynomial::create(0x1021, 16),
    .initial_value = 0x0000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000,
};

/// CRC-17/CAN-FD: polynomial x^17 + x^16 + x^14 + x^13 + x^11 + x^6 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc17_can_fd{
    .polynomial = *crc_polynomial::create(0x1685B, 17),
    .initial_value = 0x00000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00000,
};

/// CRC-21/CAN-FD: polynomial x^21 + x^20 + x^13 + x^11 + x^7 + x^4 + x^3 + 1.
inline constexpr crc_parameters crc21_can_fd{
    .polynomial = *crc_polynomial::create(0x102899, 21),
    .initial_value = 0x000000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000000,
};

/// CRC-24/BLE: polynomial x^24 + x^10 + x^9 + x^6 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc24_ble{
    .polynomial = *crc_polynomial::create(0x00065B, 24),
    .initial_value = 0x555555,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x000000,
};

/// CRC-24/FLEXRAY-A: polynomial x^24 + x^22 + x^20 + x^19 + x^18 + x^16 + x^14 + x^13 + x^11 + x^10 + x^8 + x^7 + x^6 + x^3 + x + 1.
inline constexpr crc_parameters crc24_flexray_a{
    .polynomial = *crc_polynomial::create(0x5D6DCB, 24),
    .initial_value = 0xFEDCBA,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000000,
};

/// CRC-24/FLEXRAY-B: polynomial x^24 + x^22 + x^20 + x^19 + x^18 + x^16 + x^14 + x^13 + x^11 + x^10 + x^8 + x^7 + x^6 + x^3 + x + 1.
inline constexpr crc_parameters crc24_flexray_b{
    .polynomial = *crc_polynomial::create(0x5D6DCB, 24),
    .initial_value = 0xABCDEF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000000,
};

/// CRC-24/INTERLAKEN: polynomial x^24 + x^21 + x^20 + x^17 + x^15 + x^11 + x^9 + x^8 + x^6 + x^5 + x + 1.
inline constexpr crc_parameters crc24_interlaken{
    .polynomial = *crc_polynomial::create(0x328B63, 24),
    .initial_value = 0xFFFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFFFF,
};

/// CRC-24/LTE-A: polynomial x^24 + x^23 + x^18 + x^17 + x^14 + x^11 + x^10 + x^7 + x^6 + x^5 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc24_lte_a{
    .polynomial = *crc_polynomial::create(0x864CFB, 24),
    .initial_value = 0x000000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000000,
};

/// CRC-24/LTE-B: polynomial x^24 + x^23 + x^6 + x^5 + x + 1.
inline constexpr crc_parameters crc24_lte_b{
    .polynomial = *crc_polynomial::create(0x800063, 24),
    .initial_value = 0x000000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000000,
};

/// CRC-24/OPENPGP: polynomial x^24 + x^23 + x^18 + x^17 + x^14 + x^11 + x^10 + x^7 + x^6 + x^5 + x^4 + x^3 + x + 1. Also known as CRC-24.
inline constexpr crc_parameters crc24_openpgp{
    .polynomial = *crc_polynomial::create(0x864CFB, 24),
    .initial_value = 0xB704CE,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x000000,
};

/// CRC-24/OS-9: polynomial x^24 + x^23 + x^6 + x^5 + x + 1.
inline constexpr crc_parameters crc24_os_9{
    .polynomial = *crc_polynomial::create(0x800063, 24),
    .initial_value = 0xFFFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFFFF,
};

/// CRC-30/CDMA: polynomial x^30 + x^29 + x^21 + x^20 + x^15 + x^13 + x^12 + x^11 + x^8 + x^7 + x^6 + x^2 + x + 1.
inline constexpr crc_parameters crc30_cdma{
    .polynomial = *crc_polynomial::create(0x2030B9C7, 30),
    .initial_value = 0x3FFFFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x3FFFFFFF,
};

/// CRC-31/PHILIPS: polynomial x^31 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1.
inline constexpr crc_parameters crc31_philips{
    .polynomial = *crc_polynomial::create(0x04C11DB7, 31),
    .initial_value = 0x7FFFFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x7FFFFFFF,
};

/// CRC-32/AIXM: polynomial x^32 + x^31 + x^24 + x^22 + x^16 + x^14 + x^8 + x^7 + x^5 + x^3 + x + 1. Also known as CRC-32Q.
inline constexpr crc_parameters crc32_aixm{
    .polynomial = *crc_polynomial::create(0x814141AB, 32),
    .initial_value = 0x00000000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00000000,
};

/// CRC-32/AUTOSAR: polynomial x^32 + x^31 + x^30 + x^29 + x^28 + x^26 + x^23 + x^21 + x^19 + x^18 + x^15 + x^14 + x^13 + x^12 + x^11 + x^9 + x^8 +
/// x^4 + x + 1.
inline constexpr crc_parameters crc32_autosar{
    .polynomial = *crc_polynomial::create(0xF4ACFB13, 32),
    .initial_value = 0xFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFFFFFF,
};

/// CRC-32/BASE91-D: polynomial x^32 + x^31 + x^29 + x^27 + x^21 + x^20 + x^17 + x^16 + x^15 + x^12 + x^11 + x^5 + x^3 + x + 1. Also known as CRC-32D.
inline constexpr crc_parameters crc32_base91_d{
    .polynomial = *crc_polynomial::create(0xA833982B, 32),
    .initial_value = 0xFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFFFFFF,
};

/// CRC-32/BZIP2: polynomial x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1. Also known as CRC-32/AAL5,
/// CRC-32/DECT-B, B-CRC-32.
inline constexpr crc_parameters crc32_bzip2{
    .polynomial = *crc_polynomial::create(0x04C11DB7, 32),
    .initial_value = 0xFFFFFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFFFFFF,
};

/// CRC-32/CD-ROM-EDC: polynomial x^32 + x^31 + x^16 + x^15 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc32_cd_rom_edc{
    .polynomial = *crc_polynomial::create(0x8001801B, 32),
    .initial_value = 0x00000000,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00000000,
};

/// CRC-32/CKSUM: polynomial x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1. Also known as CKSUM,
/// CRC-32/POSIX.
inline constexpr crc_parameters crc32_cksum{
    .polynomial = *crc_polynomial::create(0x04C11DB7, 32),
    .initial_value = 0x00000000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFFFFFF,
};

/// CRC-32/ISCSI: polynomial x^32 + x^28 + x^27 + x^26 + x^25 + x^23 + x^22 + x^20 + x^19 + x^18 + x^14 + x^13 + x^11 + x^10 + x^9 + x^8 + x^6 + 1.
/// Also known as CRC-32/BASE91-C, CRC-32/CASTAGNOLI, CRC-32/INTERLAKEN, CRC-32C, CRC-32/NVME.
inline constexpr crc_parameters crc32_iscsi{
    .polynomial = *crc_polynomial::create(0x1EDC6F41, 32),
    .initial_value = 0xFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFFFFFF,
};

/// CRC-32/ISO-HDLC: polynomial x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1. Also known as CRC-32,
/// CRC-32/ADCCP, CRC-32/V-42, CRC-32/XZ, PKZIP.
inline constexpr crc_parameters crc32_iso_hdlc{
    .polynomial = *crc_polynomial::create(0x04C11DB7, 32),
    .initial_value = 0xFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFFFFFF,
};

/// CRC-32/JAMCRC: polynomial x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1. Also known as JAMCRC.
inline constexpr crc_parameters crc32_jamcrc{
    .polynomial = *crc_polynomial::create(0x04C11DB7, 32),
    .initial_value = 0xFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00000000,
};

/// CRC-32/MEF: polynomial x^32 + x^30 + x^29 + x^28 + x^26 + x^20 + x^19 + x^17 + x^16 + x^15 + x^11 + x^10 + x^7 + x^6 + x^4 + x^2 + x + 1.
inline constexpr crc_parameters crc32_mef{
    .polynomial = *crc_polynomial::create(0x741B8CD7, 32),
    .initial_value = 0xFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x00000000,
};

/// CRC-32/MPEG-2: polynomial x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1.
inline constexpr crc_parameters crc32_mpeg_2{
    .polynomial = *crc_polynomial::create(0x04C11DB7, 32),
    .initial_value = 0xFFFFFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00000000,
};

/// CRC-32/XFER: polynomial x^32 + x^7 + x^5 + x^3 + x^2 + x + 1. Also known as XFER.
inline constexpr crc_parameters crc32_xfer{
    .polynomial = *crc_polynomial::create(0x000000AF, 32),
    .initial_value = 0x00000000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x00000000,
};

/// CRC-40/GSM: polynomial x^40 + x^26 + x^23 + x^17 + x^3 + 1.
inline constexpr crc_parameters crc40_gsm{
    .polynomial = *crc_polynomial::create(0x0004820009, 40),
    .initial_value = 0x0000000000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFFFFFFFF,
};

/// CRC-64/ECMA-182: polynomial x^64 + x^62 + x^57 + x^55 + x^54 + x^53 + x^52 + x^47 + x^46 + x^45 + x^40 + x^39 + x^38 + x^37 + x^35 + x^33 + x^32 +
/// x^31 + x^29 + x^27 + x^24 + x^23 + x^22 + x^21 + x^19 + x^17 + x^13 + x^12 + x^10 + x^9 + x^7 + x^4 + x + 1. Also known as CRC-64.
inline constexpr crc_parameters crc64_ecma_182{
    .polynomial = *crc_polynomial::create(0x42F0E1EBA9EA3693, 64),
    .initial_value = 0x0000000000000000,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0x0000000000000000,
};

/// CRC-64/GO-ISO: polynomial x^64 + x^4 + x^3 + x + 1.
inline constexpr crc_parameters crc64_go_iso{
    .polynomial = *crc_polynomial::create(0x000000000000001B, 64),
    .initial_value = 0xFFFFFFFFFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFFFFFFFFFFFFFF,
};

/// CRC-64/MS: polynomial x^64 + x^61 + x^58 + x^56 + x^55 + x^52 + x^51 + x^50 + x^47 + x^42 + x^39 + x^38 + x^35 + x^33 + x^32 + x^31 + x^29 + x^26
/// + x^25 + x^22 + x^17 + x^14 + x^13 + x^9 + x^8 + x^6 + x^3 + 1.
inline constexpr crc_parameters crc64_ms{
    .polynomial = *crc_polynomial::create(0x259C84CBA6426349, 64),
    .initial_value = 0xFFFFFFFFFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000000000000000,
};

/// CRC-64/NVME: polynomial x^64 + x^63 + x^61 + x^59 + x^58 + x^56 + x^55 + x^52 + x^49 + x^48 + x^47 + x^46 + x^44 + x^41 + x^37 + x^36 + x^34 +
/// x^32 + x^31 + x^28 + x^26 + x^23 + x^22 + x^19 + x^16 + x^13 + x^12 + x^10 + x^9 + x^6 + x^4 + x^3 + 1.
inline constexpr crc_parameters crc64_nvme{
    .polynomial = *crc_polynomial::create(0xAD93D23594C93659, 64),
    .initial_value = 0xFFFFFFFFFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFFFFFFFFFFFFFF,
};

/// CRC-64/REDIS: polynomial x^64 + x^63 + x^61 + x^59 + x^58 + x^56 + x^55 + x^52 + x^49 + x^48 + x^47 + x^46 + x^44 + x^41 + x^37 + x^36 + x^34 +
/// x^32 + x^31 + x^28 + x^26 + x^23 + x^22 + x^19 + x^16 + x^13 + x^12 + x^10 + x^8 + x^7 + x^5 + x^3 + 1.
inline constexpr crc_parameters crc64_redis{
    .polynomial = *crc_polynomial::create(0xAD93D23594C935A9, 64),
    .initial_value = 0x0000000000000000,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0x0000000000000000,
};

/// CRC-64/WE: polynomial x^64 + x^62 + x^57 + x^55 + x^54 + x^53 + x^52 + x^47 + x^46 + x^45 + x^40 + x^39 + x^38 + x^37 + x^35 + x^33 + x^32 + x^31
/// + x^29 + x^27 + x^24 + x^23 + x^22 + x^21 + x^19 + x^17 + x^13 + x^12 + x^10 + x^9 + x^7 + x^4 + x + 1.
inline constexpr crc_parameters crc64_we{
    .polynomial = *crc_polynomial::create(0x42F0E1EBA9EA3693, 64),
    .initial_value = 0xFFFFFFFFFFFFFFFF,
    .reflect_input = false,
    .reflect_output = false,
    .final_xor = 0xFFFFFFFFFFFFFFFF,
};

/// CRC-64/XZ: polynomial x^64 + x^62 + x^57 + x^55 + x^54 + x^53 + x^52 + x^47 + x^46 + x^45 + x^40 + x^39 + x^38 + x^37 + x^35 + x^33 + x^32 + x^31
/// + x^29 + x^27 + x^24 + x^23 + x^22 + x^21 + x^19 + x^17 + x^13 + x^12 + x^10 + x^9 + x^7 + x^4 + x + 1. Also known as CRC-64/GO-ECMA.
inline constexpr crc_parameters crc64_xz{
    .polynomial = *crc_polynomial::create(0x42F0E1EBA9EA3693, 64),
    .initial_value = 0xFFFFFFFFFFFFFFFF,
    .reflect_input = true,
    .reflect_output = true,
    .final_xor = 0xFFFFFFFFFFFFFFFF,
};

/// Alias of crc32_iso_hdlc: the same object.
inline constexpr crc_parameters const &crc32 = crc32_iso_hdlc;

/// Alias of crc32_iscsi: the same object.
inline constexpr crc_parameters const &crc32c = crc32_iscsi;

/// Alias of crc16_ibm_3740: the same object.
inline constexpr crc_parameters const &crc16_ccitt_false = crc16_ibm_3740;

/// Alias of crc16_ibm_sdlc: the same object.
inline constexpr crc_parameters const &crc16_x25 = crc16_ibm_sdlc;

} // namespace checksum

///@}
///@}

#endif // CHECKSUM_CRC_CATALOG_HPP
