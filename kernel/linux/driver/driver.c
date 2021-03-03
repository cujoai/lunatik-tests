#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

#define DEVICE_NAME "luadrv"
#define CLASS_NAME "lua"
MODULE_LICENSE("GPL");

#define print(msg) pr_warn("[lua] %s - %s\n", __func__, msg);

struct luastate_handle {
	struct mutex mtx;
	lua_State *L;
	luaL_Buffer lua_buf;
};

static dev_t major;
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
	if (alloc_chrdev_region(&major, 0, 1, DEVICE_NAME)) {
		print("major number failed");
		return -ECANCELED;
	}
	luaclass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(luaclass)) {
		unregister_chrdev_region(major, 1);
		print("class failed");
		return PTR_ERR(luaclass);
	}
	luadev = device_create(luaclass, NULL, major, NULL, "%s",
	                       DEVICE_NAME);
	if (IS_ERR(luadev)) {
		class_destroy(luaclass);
		unregister_chrdev_region(major, 1);
		print("device failed");
		return PTR_ERR(luaclass);
	}
	cdev_init(&luacdev, &fops);
	if (cdev_add(&luacdev, major, 1) == -1) {
		device_destroy(luaclass, major);
		class_destroy(luaclass);
		unregister_chrdev_region(major, 1);
		print("device registering failed");
		return -1;
	}
	return 0;
}

static void __exit luadrv_exit(void) 
{
	cdev_del(&luacdev);
	device_destroy(luaclass, major);
	class_destroy(luaclass);
	unregister_chrdev_region(major, 1);
}

static int dev_open(struct inode *i, struct file *f)
{
	struct luastate_handle *handle;

	print("open callback");
	if ((handle = kmalloc(sizeof(struct luastate_handle), GFP_KERNEL)) == NULL) {
		print("could not allocate lua state handle");
		return -ENOMEM;
	}

	mutex_init(&handle->mtx);

	print("creating new lua state");
	if ((handle->L = luaL_newstate()) == NULL) {
		print("could not allocate lua state");
		return -ENOMEM;
	}
	luaL_openlibs(handle->L);

	/* load function will be called in the close cb */
	if (lua_getglobal(handle->L, "load") != LUA_TFUNCTION) {
		print("load function not found");
		lua_close(handle->L);
		handle->L = NULL;
		return -ECANCELED;
	}
	luaL_buffinit(handle->L, &handle->lua_buf);

	f->private_data = handle;
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
	int ret_ignore = 0;
	char *script = NULL;
	struct luastate_handle *handle = f->private_data;

	print("write callback");
	if (handle == NULL) {
		print("invalid lua state handle");
		return -ESTALE;
	}

	mutex_lock(&handle->mtx);
	script = kmalloc(len + 1, GFP_KERNEL);
	if (script == NULL) {
		print("no memory");
		ret = -ENOMEM;
		goto end;
	}
	if ((ret_ignore = copy_from_user(script, buf, len)) < 0) {
		print("copy from user failed");
		ret = -ECANCELED;
		goto end;
	}
	script[len] = '\0';
	luaL_addstring(&handle->lua_buf, script);

end:
	kfree(script);
	mutex_unlock(&handle->mtx);
	return ret ? ret : len;
}

static int dev_release(struct inode *i, struct file *f)
{
	int ret = 0;
	struct luastate_handle *handle = f->private_data;

	print("release callback");
	if (handle == NULL) {
		print("invalid lua state handle");
		return -ESTALE;
	}

	mutex_lock(&handle->mtx);
	luaL_pushresult(&handle->lua_buf);
	if (lua_pcall(handle->L, 1, 1, 0)) {
		print("load error:");
		print(lua_tostring(handle->L, -1));
		ret = -ECANCELED;
		goto end;
	}
	if (lua_pcall(handle->L, 0, 1, 0)) {
		print("execution error:");
		print(lua_tostring(handle->L, -1));
		ret = -ECANCELED;
		goto end;
	}
	print("test done, final return:");
	print(lua_tostring(handle->L, -1));

end:
	lua_close(handle->L);
	handle->L = NULL;
	mutex_unlock(&handle->mtx);

	kfree(handle);
	f->private_data = NULL;
	return ret;
}

module_init(luadrv_init);
module_exit(luadrv_exit);
