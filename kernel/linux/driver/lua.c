#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#define _KERNEL
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

#define DEVICE_NAME "luadrv"
#define CLASS_NAME "lua"

#define print(msg) pr_warn("[lua] %s - %s\n", __func__, msg);

static DEFINE_MUTEX(mtx);

static int major;
static lua_State *L;
static luaL_Buffer lua_buf;
static struct device *luadev;
static struct class *luaclass;
static struct cdev luacdev;

static int dev_open(struct inode*, struct file*);
static int dev_release(struct inode*, struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t*);

static struct file_operations fops =
{
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release
};

static int __init luadrv_init(void)
{
	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0) {
		print("major number failed");
		return -ECANCELED;
	}
	luaclass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(luaclass)) {
		unregister_chrdev(major, DEVICE_NAME);
		print("class failed");
		return PTR_ERR(luaclass);
	}
	luadev = device_create(luaclass, NULL, MKDEV(major, 1), NULL, "%s",
	                       DEVICE_NAME);
	if (IS_ERR(luadev)) {
		class_destroy(luaclass);
		cdev_del(&luacdev);
		unregister_chrdev(major, DEVICE_NAME);
		print("device failed");
		return PTR_ERR(luaclass);
	}
	return 0;
}

static void __exit luadrv_exit(void) 
{
	return;
}

static int dev_open(struct inode *i, struct file *f)
{
	print("open callback");
	L = luaL_newstate();
	if (L == NULL) {
		print("no memory");
		return -ENOMEM;
	}
	luaL_openlibs(L);
	/* load function will be called in the close cb */
	if (lua_getglobal(L, "load") != LUA_TFUNCTION) {
		print("load function not found");
		lua_close(L);
		L = NULL;
		return -ECANCELED;
	}
	luaL_buffinit(L, &lua_buf);
	return 0;
}

static ssize_t dev_read(struct file *f, char *buf, size_t len, loff_t *off)
{
	return 0;
}

static ssize_t dev_write(struct file *f, const char *buf, size_t len,
                         loff_t* off)
{
	int ret = 0;
	char *script = NULL;

	print("write callback");
	mutex_lock(&mtx);
	script = kmalloc(len, GFP_KERNEL);
	if (script == NULL) {
		print("no memory");
		ret = -ENOMEM;
		goto end;
	}
	if (copy_from_user(script, buf, len) < 0) {
		print("copy from user failed");
		ret = -ECANCELED;
		goto end;
	}
	script[len-1] = '\0';
	luaL_addstring(&lua_buf, script);

end:
	kfree(script);
	mutex_unlock(&mtx);
	return ret ? ret : len;
}

static int dev_release(struct inode *i, struct file *f)
{
	int ret = 0;

	print("release callback");
	mutex_lock(&mtx);
	luaL_pushresult(&lua_buf);
	if (lua_pcall(L, 1, 1, 0)) {
		print("load error:");
		print(lua_tostring(L, -1));
		ret = -ECANCELED;
		goto end;
	}
	if (lua_pcall(L, 0, 1, 0)) {
		print("execution error:");
		print(lua_tostring(L, -1));
		ret = -ECANCELED;
		goto end;
	}
	print("test done, final return:");
	print(lua_tostring(L, -1));

end:
	lua_close(L);
	L = NULL;
	mutex_unlock(&mtx);
	return ret;
}

module_init(luadrv_init);
module_exit(luadrv_exit);
