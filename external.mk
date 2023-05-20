ILI9341_VERSION = 1.0
ILI9341_SITE = $(BR2_EXTERNAL)/package/ili9341
ILI9341_SITE_METHOD = local
$(eval $(kernel-module))
$(eval $(generic-package))