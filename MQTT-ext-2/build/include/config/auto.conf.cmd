deps_config := \
	/home/jan/esp/esp-idf/components/app_trace/Kconfig \
	/home/jan/esp/esp-idf/components/aws_iot/Kconfig \
	/home/jan/esp/esp-idf/components/bt/Kconfig \
	/home/jan/esp/esp-idf/components/esp32/Kconfig \
	/home/jan/CloudStation/eclipse/workspace_cdt/MQTT-ext-2/components/espmqtt/Kconfig \
	/home/jan/esp/esp-idf/components/ethernet/Kconfig \
	/home/jan/esp/esp-idf/components/fatfs/Kconfig \
	/home/jan/esp/esp-idf/components/freertos/Kconfig \
	/home/jan/esp/esp-idf/components/log/Kconfig \
	/home/jan/esp/esp-idf/components/lwip/Kconfig \
	/home/jan/esp/esp-idf/components/mbedtls/Kconfig \
	/home/jan/esp/esp-idf/components/openssl/Kconfig \
	/home/jan/esp/esp-idf/components/spi_flash/Kconfig \
	/home/jan/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/jan/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/jan/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/jan/CloudStation/eclipse/workspace_cdt/MQTT-ext-2/main/Kconfig.projbuild \
	/home/jan/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
