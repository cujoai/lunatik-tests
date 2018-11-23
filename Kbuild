ccflags-y += -D_KERNEL

obj-$(CONFIG_LUNATIK_DEBUG) += lunatiktest.o
lunatiktest-objs += kernel/linux/driver/driver.o

ifdef CONFIG_DATA_DEBUG
ccflags-y += -DCONFIG_DATA_DEBUG
obj-m += lunatiktest-data.o
lunatiktest-data-objs += kernel/linux/driver/driver.o
endif

ifdef CONFIG_JSON_DEBUG
ccflags-y += -DCONFIG_JSON_DEBUG
obj-m += lunatiktest-json.o
lunatiktest-json-objs += kernel/linux/driver/driver.o
endif

ifdef CONFIG_BASE64_DEBUG
ccflags-y += -DCONFIG_BASE64_DEBUG
obj-m += lunatiktest-base64.o
lunatiktest-base64-objs += kernel/linux/driver/driver.o
endif
