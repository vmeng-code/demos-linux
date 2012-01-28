//#define DEBUG
#include <linux/module.h> // for MODULE_* stuff
#include <linux/bootmem.h> // for the bootmem function

//define DO_DEBUG
#include "kernel_helper.h" // our own helper

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mark Veltzer");
MODULE_DESCRIPTION("This is a driver that demos the alloc_bootmem function");

/*
 * Why would you need bootmem ?
 * - you may want a large contiguous buffer to work with some kind of device.
 * after the kernel has loaded and has started to use the systems memory you are no longer
 * guaranteed that such a large contiguous buffer could be obtained. In addition you HAVE NO
 * API to obtain such a buffer even if you were guaranteed that it does exist. kmalloc and other
 * friends have 4MB or some such limit (depending on platform).
 * - if you write a small driver like this, link it to the physical kernel, the the function
 *   alloca_bootmem will be found and you will get the memory assigned.
 * 
 * How would you use the buffer ?
 * Add yet another function that returns the pointer to the buffer in some way and export it.
 * Other modules, even dynamic ones, could call upon that function and get the pointer and
 * do it whatever they want. If you need it for large contiguous DMA then you could call _pa()
 * on it and get it's physical address and so on.
 *
 * TODO:
 * - is there a dealloc_bootmem function and if so why am I not showing how to use it here ?
 */

// our own functions
static int __init mod_init(void) {
	void* p;
	PR_DEBUG("start");
	// this will NOT work if your module is dynamic (you must be linked into the kernel)
	//alloc_bootmem(10000000);
	p=alloc_bootmem(PAGE_SIZE);
	if (IS_ERR(p)) {
		PR_ERROR("unable to allocate bootmem");
		return(PTR_ERR(p));
	}
	PR_DEBUG("ok");
	return(0);
}

static void __exit mod_exit(void) {
	PR_DEBUG("start");
}

// declaration of init/cleanup functions of this module
module_init(mod_init);
module_exit(mod_exit);
