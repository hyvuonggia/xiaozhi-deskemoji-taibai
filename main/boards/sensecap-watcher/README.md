# Build Commands

## One-command Build

```bash
python scripts/release.py sensecap-watcher
```

## Manual Build Configuration

```bash
idf.py set-target esp32s3
```

**Configure**

```bash
idf.py menuconfig
```

Select board

```
Xiaozhi Assistant -> Board Type -> SenseCAP Watcher
```

The following additional options for Watcher must be selected in menuconfig.

```
CONFIG_BOARD_TYPE_SENSECAP_WATCHER=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v1/32m.csv"
CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH=y
CONFIG_ESPTOOLPY_FLASH_MODE_AUTO_DETECT=n
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
```

## Build and flash

```bash
idf.py -DBOARD_NAME=sensecap-watcher build flash
```

Note: If the device shipped with SenseCAP firmware (not the Xiaozhi version), take special care with flash partition addresses to avoid accidentally erasing device-specific information (such as the EUI). Otherwise, even after restoring the SenseCAP firmware, the device may not connect to the SenseCraft server correctly. Before flashing, record the necessary device information so recovery is possible!

Use the following command to back up the factory information

```bash
# firstly backup the factory information partition which contains the credentials for connecting the SenseCraft server
esptool.py --chip esp32s3 --baud 2000000 --before default_reset --after hard_reset --no-stub read_flash 0x9000 204800 nvsfactory.bin

```