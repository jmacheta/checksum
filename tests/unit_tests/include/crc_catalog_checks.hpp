#ifndef CHECKSUM_TESTS_CRC_CATALOG_CHECKS_HPP
#define CHECKSUM_TESTS_CRC_CATALOG_CHECKS_HPP

// The check value (CRC of the ASCII text "123456789") of every parameter set in checksum/crc_catalog.hpp, as published
// in the CRC catalogue at https://reveng.sourceforge.io/crc-catalogue/.

#include <checksum/crc.hpp>
#include <checksum/crc_catalog.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace crc_test {

struct catalog_check {
  std::string_view name;
  checksum::crc_parameters parameters;
  std::uint64_t check;
};

// In catalog order.
inline constexpr std::array<catalog_check, 112> catalog_checks{
    catalog_check{.name = "crc3_gsm", .parameters = checksum::crc3_gsm, .check = 0x4},
    catalog_check{.name = "crc3_rohc", .parameters = checksum::crc3_rohc, .check = 0x6},
    catalog_check{.name = "crc4_g_704", .parameters = checksum::crc4_g_704, .check = 0x7},
    catalog_check{.name = "crc4_interlaken", .parameters = checksum::crc4_interlaken, .check = 0xB},
    catalog_check{.name = "crc5_epc_c1g2", .parameters = checksum::crc5_epc_c1g2, .check = 0x00},
    catalog_check{.name = "crc5_g_704", .parameters = checksum::crc5_g_704, .check = 0x07},
    catalog_check{.name = "crc5_usb", .parameters = checksum::crc5_usb, .check = 0x19},
    catalog_check{.name = "crc6_cdma2000_a", .parameters = checksum::crc6_cdma2000_a, .check = 0x0D},
    catalog_check{.name = "crc6_cdma2000_b", .parameters = checksum::crc6_cdma2000_b, .check = 0x3B},
    catalog_check{.name = "crc6_darc", .parameters = checksum::crc6_darc, .check = 0x26},
    catalog_check{.name = "crc6_g_704", .parameters = checksum::crc6_g_704, .check = 0x06},
    catalog_check{.name = "crc6_gsm", .parameters = checksum::crc6_gsm, .check = 0x13},
    catalog_check{.name = "crc7_mmc", .parameters = checksum::crc7_mmc, .check = 0x75},
    catalog_check{.name = "crc7_rohc", .parameters = checksum::crc7_rohc, .check = 0x53},
    catalog_check{.name = "crc7_umts", .parameters = checksum::crc7_umts, .check = 0x61},
    catalog_check{.name = "crc8_autosar", .parameters = checksum::crc8_autosar, .check = 0xDF},
    catalog_check{.name = "crc8_bluetooth", .parameters = checksum::crc8_bluetooth, .check = 0x26},
    catalog_check{.name = "crc8_cdma2000", .parameters = checksum::crc8_cdma2000, .check = 0xDA},
    catalog_check{.name = "crc8_darc", .parameters = checksum::crc8_darc, .check = 0x15},
    catalog_check{.name = "crc8_dvb_s2", .parameters = checksum::crc8_dvb_s2, .check = 0xBC},
    catalog_check{.name = "crc8_gsm_a", .parameters = checksum::crc8_gsm_a, .check = 0x37},
    catalog_check{.name = "crc8_gsm_b", .parameters = checksum::crc8_gsm_b, .check = 0x94},
    catalog_check{.name = "crc8_hitag", .parameters = checksum::crc8_hitag, .check = 0xB4},
    catalog_check{.name = "crc8_i_432_1", .parameters = checksum::crc8_i_432_1, .check = 0xA1},
    catalog_check{.name = "crc8_i_code", .parameters = checksum::crc8_i_code, .check = 0x7E},
    catalog_check{.name = "crc8_lte", .parameters = checksum::crc8_lte, .check = 0xEA},
    catalog_check{.name = "crc8_maxim_dow", .parameters = checksum::crc8_maxim_dow, .check = 0xA1},
    catalog_check{.name = "crc8_mifare_mad", .parameters = checksum::crc8_mifare_mad, .check = 0x99},
    catalog_check{.name = "crc8_nrsc_5", .parameters = checksum::crc8_nrsc_5, .check = 0xF7},
    catalog_check{.name = "crc8_opensafety", .parameters = checksum::crc8_opensafety, .check = 0x3E},
    catalog_check{.name = "crc8_rohc", .parameters = checksum::crc8_rohc, .check = 0xD0},
    catalog_check{.name = "crc8_sae_j1850", .parameters = checksum::crc8_sae_j1850, .check = 0x4B},
    catalog_check{.name = "crc8_smbus", .parameters = checksum::crc8_smbus, .check = 0xF4},
    catalog_check{.name = "crc8_tech_3250", .parameters = checksum::crc8_tech_3250, .check = 0x97},
    catalog_check{.name = "crc8_wcdma", .parameters = checksum::crc8_wcdma, .check = 0x25},
    catalog_check{.name = "crc10_atm", .parameters = checksum::crc10_atm, .check = 0x199},
    catalog_check{.name = "crc10_cdma2000", .parameters = checksum::crc10_cdma2000, .check = 0x233},
    catalog_check{.name = "crc10_gsm", .parameters = checksum::crc10_gsm, .check = 0x12A},
    catalog_check{.name = "crc11_flexray", .parameters = checksum::crc11_flexray, .check = 0x5A3},
    catalog_check{.name = "crc11_umts", .parameters = checksum::crc11_umts, .check = 0x061},
    catalog_check{.name = "crc12_cdma2000", .parameters = checksum::crc12_cdma2000, .check = 0xD4D},
    catalog_check{.name = "crc12_dect", .parameters = checksum::crc12_dect, .check = 0xF5B},
    catalog_check{.name = "crc12_gsm", .parameters = checksum::crc12_gsm, .check = 0xB34},
    catalog_check{.name = "crc12_umts", .parameters = checksum::crc12_umts, .check = 0xDAF},
    catalog_check{.name = "crc13_bbc", .parameters = checksum::crc13_bbc, .check = 0x04FA},
    catalog_check{.name = "crc14_darc", .parameters = checksum::crc14_darc, .check = 0x082D},
    catalog_check{.name = "crc14_gsm", .parameters = checksum::crc14_gsm, .check = 0x30AE},
    catalog_check{.name = "crc15_can", .parameters = checksum::crc15_can, .check = 0x059E},
    catalog_check{.name = "crc15_mpt1327", .parameters = checksum::crc15_mpt1327, .check = 0x2566},
    catalog_check{.name = "crc16_arc", .parameters = checksum::crc16_arc, .check = 0xBB3D},
    catalog_check{.name = "crc16_cdma2000", .parameters = checksum::crc16_cdma2000, .check = 0x4C06},
    catalog_check{.name = "crc16_cms", .parameters = checksum::crc16_cms, .check = 0xAEE7},
    catalog_check{.name = "crc16_dds_110", .parameters = checksum::crc16_dds_110, .check = 0x9ECF},
    catalog_check{.name = "crc16_dect_r", .parameters = checksum::crc16_dect_r, .check = 0x007E},
    catalog_check{.name = "crc16_dect_x", .parameters = checksum::crc16_dect_x, .check = 0x007F},
    catalog_check{.name = "crc16_dnp", .parameters = checksum::crc16_dnp, .check = 0xEA82},
    catalog_check{.name = "crc16_en_13757", .parameters = checksum::crc16_en_13757, .check = 0xC2B7},
    catalog_check{.name = "crc16_genibus", .parameters = checksum::crc16_genibus, .check = 0xD64E},
    catalog_check{.name = "crc16_gsm", .parameters = checksum::crc16_gsm, .check = 0xCE3C},
    catalog_check{.name = "crc16_ibm_3740", .parameters = checksum::crc16_ibm_3740, .check = 0x29B1},
    catalog_check{.name = "crc16_ibm_sdlc", .parameters = checksum::crc16_ibm_sdlc, .check = 0x906E},
    catalog_check{.name = "crc16_iso_iec_14443_3_a", .parameters = checksum::crc16_iso_iec_14443_3_a, .check = 0xBF05},
    catalog_check{.name = "crc16_kermit", .parameters = checksum::crc16_kermit, .check = 0x2189},
    catalog_check{.name = "crc16_lj1200", .parameters = checksum::crc16_lj1200, .check = 0xBDF4},
    catalog_check{.name = "crc16_m17", .parameters = checksum::crc16_m17, .check = 0x772B},
    catalog_check{.name = "crc16_maxim_dow", .parameters = checksum::crc16_maxim_dow, .check = 0x44C2},
    catalog_check{.name = "crc16_mcrf4xx", .parameters = checksum::crc16_mcrf4xx, .check = 0x6F91},
    catalog_check{.name = "crc16_modbus", .parameters = checksum::crc16_modbus, .check = 0x4B37},
    catalog_check{.name = "crc16_nrsc_5", .parameters = checksum::crc16_nrsc_5, .check = 0xA066},
    catalog_check{.name = "crc16_opensafety_a", .parameters = checksum::crc16_opensafety_a, .check = 0x5D38},
    catalog_check{.name = "crc16_opensafety_b", .parameters = checksum::crc16_opensafety_b, .check = 0x20FE},
    catalog_check{.name = "crc16_profibus", .parameters = checksum::crc16_profibus, .check = 0xA819},
    catalog_check{.name = "crc16_riello", .parameters = checksum::crc16_riello, .check = 0x63D0},
    catalog_check{.name = "crc16_spi_fujitsu", .parameters = checksum::crc16_spi_fujitsu, .check = 0xE5CC},
    catalog_check{.name = "crc16_t10_dif", .parameters = checksum::crc16_t10_dif, .check = 0xD0DB},
    catalog_check{.name = "crc16_teledisk", .parameters = checksum::crc16_teledisk, .check = 0x0FB3},
    catalog_check{.name = "crc16_tms37157", .parameters = checksum::crc16_tms37157, .check = 0x26B1},
    catalog_check{.name = "crc16_umts", .parameters = checksum::crc16_umts, .check = 0xFEE8},
    catalog_check{.name = "crc16_usb", .parameters = checksum::crc16_usb, .check = 0xB4C8},
    catalog_check{.name = "crc16_xmodem", .parameters = checksum::crc16_xmodem, .check = 0x31C3},
    catalog_check{.name = "crc17_can_fd", .parameters = checksum::crc17_can_fd, .check = 0x04F03},
    catalog_check{.name = "crc21_can_fd", .parameters = checksum::crc21_can_fd, .check = 0x0ED841},
    catalog_check{.name = "crc24_ble", .parameters = checksum::crc24_ble, .check = 0xC25A56},
    catalog_check{.name = "crc24_flexray_a", .parameters = checksum::crc24_flexray_a, .check = 0x7979BD},
    catalog_check{.name = "crc24_flexray_b", .parameters = checksum::crc24_flexray_b, .check = 0x1F23B8},
    catalog_check{.name = "crc24_interlaken", .parameters = checksum::crc24_interlaken, .check = 0xB4F3E6},
    catalog_check{.name = "crc24_lte_a", .parameters = checksum::crc24_lte_a, .check = 0xCDE703},
    catalog_check{.name = "crc24_lte_b", .parameters = checksum::crc24_lte_b, .check = 0x23EF52},
    catalog_check{.name = "crc24_openpgp", .parameters = checksum::crc24_openpgp, .check = 0x21CF02},
    catalog_check{.name = "crc24_os_9", .parameters = checksum::crc24_os_9, .check = 0x200FA5},
    catalog_check{.name = "crc30_cdma", .parameters = checksum::crc30_cdma, .check = 0x04C34ABF},
    catalog_check{.name = "crc31_philips", .parameters = checksum::crc31_philips, .check = 0x0CE9E46C},
    catalog_check{.name = "crc32_aixm", .parameters = checksum::crc32_aixm, .check = 0x3010BF7F},
    catalog_check{.name = "crc32_autosar", .parameters = checksum::crc32_autosar, .check = 0x1697D06A},
    catalog_check{.name = "crc32_base91_d", .parameters = checksum::crc32_base91_d, .check = 0x87315576},
    catalog_check{.name = "crc32_bzip2", .parameters = checksum::crc32_bzip2, .check = 0xFC891918},
    catalog_check{.name = "crc32_cd_rom_edc", .parameters = checksum::crc32_cd_rom_edc, .check = 0x6EC2EDC4},
    catalog_check{.name = "crc32_cksum", .parameters = checksum::crc32_cksum, .check = 0x765E7680},
    catalog_check{.name = "crc32_iscsi", .parameters = checksum::crc32_iscsi, .check = 0xE3069283},
    catalog_check{.name = "crc32_iso_hdlc", .parameters = checksum::crc32_iso_hdlc, .check = 0xCBF43926},
    catalog_check{.name = "crc32_jamcrc", .parameters = checksum::crc32_jamcrc, .check = 0x340BC6D9},
    catalog_check{.name = "crc32_mef", .parameters = checksum::crc32_mef, .check = 0xD2C22F51},
    catalog_check{.name = "crc32_mpeg_2", .parameters = checksum::crc32_mpeg_2, .check = 0x0376E6E7},
    catalog_check{.name = "crc32_xfer", .parameters = checksum::crc32_xfer, .check = 0xBD0BE338},
    catalog_check{.name = "crc40_gsm", .parameters = checksum::crc40_gsm, .check = 0xD4164FC646},
    catalog_check{.name = "crc64_ecma_182", .parameters = checksum::crc64_ecma_182, .check = 0x6C40DF5F0B497347},
    catalog_check{.name = "crc64_go_iso", .parameters = checksum::crc64_go_iso, .check = 0xB90956C775A41001},
    catalog_check{.name = "crc64_ms", .parameters = checksum::crc64_ms, .check = 0x75D4B74F024ECEEA},
    catalog_check{.name = "crc64_nvme", .parameters = checksum::crc64_nvme, .check = 0xAE8B14860A799888},
    catalog_check{.name = "crc64_redis", .parameters = checksum::crc64_redis, .check = 0xE9C6D914C4B8D9CA},
    catalog_check{.name = "crc64_we", .parameters = checksum::crc64_we, .check = 0x62EC59E3F1A4F00A},
    catalog_check{.name = "crc64_xz", .parameters = checksum::crc64_xz, .check = 0x995DC9BBDF1939FA},
};

} // namespace crc_test

#endif // CHECKSUM_TESTS_CRC_CATALOG_CHECKS_HPP
